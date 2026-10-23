/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 *
 * Prepared model execution. A run follows tt_model_run step by step and
 * performs the same floating-point operations for every output:
 *  - spectra come from spectrum plans, shared by all users of equal spectra;
 *  - each rate is a list of terms in ascending wavelength order. A term whose
 *    cross section and yield are both constant carries their product, as the
 *    generic rate forms it, and terms whose product is zero are omitted:
 *    adding a zero product cannot change a sum that starts at +0, provided
 *    the flux is finite, which every run checks;
 *  - Delta-Eddington transfer runs in blocks of wavelengths (radiation_block.c);
 *  - the actinic flux and irradiance are formed from the scaled fields.
 * A run returns 0, leaving the decision to the generic run, for any input
 * outside these assumptions: invalid values, temperatures above the spectrum
 * plans' limit, or nonfinite radiation. */
#include "model_plan.h"
#include "spectrum_plan.h"
#include "diagnostics_internal.h"
#include "reaper_internal.h"
#include "oxygen_internal.h"
#include "turbotuv_reaper.h"
#include "reaper_writer.h"
#include <float.h>
#include <fenv.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Retain the reference's physical constants, as rates.c does. */
static const double hc = 6.626068e-34 * 2.99792458e8;

typedef enum { TERM_CONSTANT, TERM_CROSS, TERM_YIELD, TERM_BOTH } term_kind;
typedef struct {
    size_t w;
    term_kind kind;
    double value;                /* the product, or the constant factor */
    double energy;               /* heating: the wavelength's energy factor */
    const double *cross, *yield; /* level rows of the varying factors */
} term;
typedef struct {
    size_t count;
    term *terms;
    double scale, threshold; /* threshold: heating energy wavelength */
    int heating, dose;
    size_t output;
} integral;
/* A spectrum plan with its evaluation rows for one level set. */
typedef struct {
    tt_spectrum_plan *plan;
    size_t levels, varying, *position; /* position[w]: row of w, or SIZE_MAX */
    double *rows;
} evaluation;

struct tt_model_plan {
    size_t evaluation_count;
    evaluation *evaluations;
    size_t *reaction_cross, *reaction_yield, *radiator_cross; /* evaluation indices */
    size_t integral_count;
    integral *integrals;
    double *oxygen_cross, *oxygen_work, *scratch, *block, maximum_temperature;
    unsigned char *gas_initialized;
    int needs_temperature, needs_air, oxygen_reactions;
    tt_reaper_kernels kernels;
    unsigned capabilities;
    unsigned char identity[32];
    int has_identity, auto_checked;
    const double **row_views;
    double *reaper_block;
    int reaper_prepared;
    size_t radiator_count;
    unsigned char *reaper_roles;
};

void tt_model_plan_destroy(tt_model_plan *p) {
    if (!p)
        return;
    for (size_t i = 0; i < p->evaluation_count; ++i) {
        tt_spectrum_plan_destroy(p->evaluations[i].plan);
        free(p->evaluations[i].position);
        free(p->evaluations[i].rows);
    }
    for (size_t i = 0; i < p->integral_count; ++i)
        free(p->integrals[i].terms);
    free(p->evaluations);
    free(p->reaction_cross);
    free(p->reaction_yield);
    free(p->radiator_cross);
    free(p->integrals);
    free(p->oxygen_cross);
    free(p->oxygen_work);
    free(p->scratch);
    free(p->block);
    free(p->gas_initialized);
    free(p->reaper_roles);
    free(p->row_views);
    free(p->reaper_block);
    free(p);
}

/* Index of an evaluation of an equal spectrum on the same levels, adding one. */
static tt_status evaluation_for(tt_model_plan *p, const tt_spectrum *s, size_t levels,
                                size_t capacity, size_t *index, size_t *scratch) {
    tt_spectrum_plan *plan = NULL;
    tt_status status = tt_spectrum_plan_create(s, &plan);
    if (status)
        return status;
    for (size_t i = 0; i < p->evaluation_count; ++i)
        if (p->evaluations[i].levels == levels &&
            tt_spectrum_plan_equal(p->evaluations[i].plan, plan)) {
            tt_spectrum_plan_destroy(plan);
            *index = i;
            return TT_OK;
        }
    if (p->evaluation_count == capacity) {
        tt_spectrum_plan_destroy(plan);
        return TT_INVALID;
    }
    evaluation *e = p->evaluations + p->evaluation_count;
    const size_t *varying;
    size_t nw = s->wavelength.cells;
    e->plan = plan;
    e->levels = levels;
    e->varying = tt_spectrum_plan_varying(plan, &varying);
    e->position = malloc(nw * sizeof(size_t));
    e->rows = malloc((e->varying ? e->varying : 1) * levels * sizeof(double));
    if (!e->position || !e->rows) {
        tt_spectrum_plan_destroy(plan);
        free(e->position);
        free(e->rows);
        return TT_MEMORY;
    }
    for (size_t w = 0; w < nw; ++w)
        e->position[w] = SIZE_MAX;
    for (size_t k = 0; k < e->varying; ++k)
        e->position[varying[k]] = k;
    size_t need = tt_spectrum_plan_scratch(plan, levels);
    if (need > *scratch)
        *scratch = need;
    double limit = tt_spectrum_plan_maximum_temperature(plan);
    if (limit < p->maximum_temperature)
        p->maximum_temperature = limit;
    *index = p->evaluation_count++;
    return TT_OK;
}

/* Terms of one rate. Photolysis and heating follow tt_photolysis_rate and
 * tt_heating_rate; the oxygen reaction's cross section varies in its bands. */
static tt_status reaction_terms(const tt_model *m, tt_model_plan *p, size_t r, integral *out) {
    const tt_model_reaction *reaction = m->reactions + r;
    const evaluation *cs = p->evaluations + p->reaction_cross[r],
                     *qy = p->evaluations + p->reaction_yield[r];
    const double *cs_value = tt_spectrum_plan_constant(cs->plan),
                 *qy_value = tt_spectrum_plan_constant(qy->plan);
    const double *mid = m->grids[m->wavelength].value.midpoints;
    size_t nw = m->shape.wavelengths, nz = m->shape.levels;
    size_t la = m->bands.lyman_alpha, sr = m->bands.schumann_runge;
    out->terms = malloc(nw * sizeof(term));
    if (!out->terms)
        return TT_MEMORY;
    for (size_t w = 0; w < nw; ++w) {
        term t = {w, TERM_CONSTANT, 0, 0, NULL, NULL};
        if (out->heating)
            t.energy = fmax(0, hc * 1.e9 * (out->threshold - mid[w]) / (out->threshold * mid[w]));
        if (cs->position[w] != SIZE_MAX)
            t.cross = cs->rows + cs->position[w] * nz;
        if (qy->position[w] != SIZE_MAX)
            t.yield = qy->rows + qy->position[w] * nz;
        if (reaction->oxygen &&
            ((la != SIZE_MAX && w == la) || (sr != SIZE_MAX && w >= sr && w < sr + 17)))
            t.cross = p->oxygen_cross + w * nz;
        if (t.cross && t.yield)
            t.kind = TERM_BOTH;
        else if (t.cross) {
            t.kind = TERM_CROSS;
            t.value = qy_value[w];
        } else if (t.yield) {
            t.kind = TERM_YIELD;
            t.value = cs_value[w];
        } else {
            t.value =
                out->heating ? t.energy * (cs_value[w] * qy_value[w]) : cs_value[w] * qy_value[w];
            if (t.value == 0)
                continue;
        }
        out->terms[out->count++] = t;
    }
    return TT_OK;
}

size_t tt_model_plan_generic_spectra(const tt_model_plan *p) {
    size_t n = 0;
    for (size_t i = 0; p && i < p->evaluation_count; ++i)
        n += (size_t)tt_spectrum_plan_generic(p->evaluations[i].plan);
    return n;
}
tt_status tt_model_plan_create(const tt_model *m, tt_model_plan **out) {
    if (!m || !out)
        return TT_INVALID;
    tt_model_plan *p = calloc(1, sizeof *p);
    if (!p)
        return TT_MEMORY;
    size_t nz = m->shape.levels, nl = nz - 1, nw = m->shape.wavelengths;
    size_t nr = m->shape.photolysis, capacity = 2 * nr + m->radiator_count, scratch = 0;
    p->maximum_temperature = INFINITY;
    p->evaluations = calloc(capacity ? capacity : 1, sizeof *p->evaluations);
    p->reaction_cross = calloc(nr ? nr : 1, sizeof(size_t));
    p->reaction_yield = calloc(nr ? nr : 1, sizeof(size_t));
    p->radiator_cross = calloc(m->radiator_count ? m->radiator_count : 1, sizeof(size_t));
    p->gas_initialized = calloc(m->radiator_count ? m->radiator_count : 1, 1);
    p->integral_count = nr + m->shape.heating + m->shape.dose;
    p->integrals = calloc(p->integral_count ? p->integral_count : 1, sizeof *p->integrals);
    for (size_t r = 0; r < nr; ++r)
        p->oxygen_reactions |= m->reactions[r].oxygen;
    if (p->oxygen_reactions)
        p->oxygen_cross = calloc(nw * nz, sizeof(double));
    tt_status status = p->evaluations && p->reaction_cross && p->reaction_yield &&
                               p->radiator_cross && p->gas_initialized && p->integrals &&
                               (p->oxygen_cross || !p->oxygen_reactions)
                           ? TT_OK
                           : TT_MEMORY;
    for (size_t i = 0; i < m->radiator_count && !status; ++i)
        if (m->radiators[i].gas) {
            status = evaluation_for(p, m->spectra[m->radiators[i].spectrum].spectrum, nl, capacity,
                                    p->radiator_cross + i, &scratch);
            p->needs_temperature = 1;
        }
    /* The generic irradiance rejects nonpositive wavelengths on every run. */
    for (size_t w = 0; w < nw && !status && m->solar != SIZE_MAX; ++w)
        if (!(m->grids[m->wavelength].value.midpoints[w] > 0))
            status = TT_INVALID;
    for (size_t r = 0; r < nr && !status; ++r) {
        status = evaluation_for(p, m->reactions[r].cross_section, nz, capacity,
                                p->reaction_cross + r, &scratch);
        if (!status)
            status = evaluation_for(p, m->reactions[r].yield, nz, capacity, p->reaction_yield + r,
                                    &scratch);
        p->needs_temperature = p->needs_air = 1;
    }
    for (size_t r = 0; r < nr && !status; ++r) {
        integral *photolysis = p->integrals + r;
        photolysis->scale = m->reactions[r].scale;
        photolysis->output = r * nz;
        status = reaction_terms(m, p, r, photolysis);
        if (!status && m->reactions[r].heating_index != SIZE_MAX) {
            integral *heating = p->integrals + nr + m->reactions[r].heating_index;
            heating->heating = 1;
            heating->threshold = m->reactions[r].energy_wavelength;
            heating->scale = m->reactions[r].scale;
            heating->output = (nr + m->reactions[r].heating_index) * nz;
            status = reaction_terms(m, p, r, heating);
        }
    }
    for (size_t d = 0; d < m->shape.dose && !status; ++d) {
        integral *dose = p->integrals + nr + m->shape.heating + d;
        dose->dose = 1;
        dose->output = (nr + m->shape.heating + d) * nz;
        dose->terms = malloc(nw * sizeof(term));
        if (!dose->terms)
            status = TT_MEMORY;
        for (size_t w = 0; w < nw && !status; ++w)
            if (m->doses[d].weights[w] != 0)
                dose->terms[dose->count++] =
                    (term){w, TERM_CONSTANT, m->doses[d].weights[w], 0, NULL, NULL};
    }
    if (!status && (m->bands.lyman_alpha != SIZE_MAX || m->bands.schumann_runge != SIZE_MAX)) {
        if (nz > SIZE_MAX / 20 / sizeof(double))
            status = TT_MEMORY;
        else
            p->oxygen_work = malloc(20 * nz * sizeof(double));
        if (!p->oxygen_work)
            status = TT_MEMORY;
    }
    if (!status) {
        p->scratch = malloc((scratch ? scratch : 1) * sizeof(double));
        if (!m->host_solver && m->streams == 1)
            p->block = malloc(tt_block_delta_eddington_work(nl) * sizeof(double));
        if (!p->scratch || (!m->host_solver && m->streams == 1 && !p->block))
            status = TT_MEMORY;
    }
    if (!status) {
        p->row_views = calloc(p->evaluation_count + 1, sizeof(double *));
        if (!p->row_views)
            status = TT_MEMORY;
        else {
            for (size_t i = 0; i < p->evaluation_count; ++i)
                p->row_views[i] = p->evaluations[i].rows;
            p->row_views[p->evaluation_count] = p->oxygen_cross;
        }
    }
    if (status) {
        tt_model_plan_destroy(p);
        return status;
    }
    *out = p;
    return TT_OK;
}

/* Rates as sums over their terms, z-tiled so the partial sums stay in
 * registers; the sum over wavelengths keeps the generic order at each level. */
enum { TILE = 16 };
static void integrate_tile(const integral *g, const double *flux, size_t nz, size_t z0,
                           size_t width, double *out) {
    double acc[TILE] = {0};
    for (size_t t = 0; t < g->count; ++t) {
        const term *x = g->terms + t;
        const double *f = flux + x->w * nz + z0;
        const double *c = x->cross ? x->cross + z0 : NULL, *y = x->yield ? x->yield + z0 : NULL;
        double v = x->value, e = x->energy;
        if (!g->heating)
            switch (x->kind) {
            case TERM_CONSTANT:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * v;
                break;
            case TERM_CROSS:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (c[z] * v);
                break;
            case TERM_YIELD:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (v * y[z]);
                break;
            case TERM_BOTH:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (c[z] * y[z]);
                break;
            }
        else
            switch (x->kind) {
            case TERM_CONSTANT:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * v;
                break;
            case TERM_CROSS:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (e * (c[z] * v));
                break;
            case TERM_YIELD:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (e * (v * y[z]));
                break;
            case TERM_BOTH:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (e * (c[z] * y[z]));
                break;
            }
    }
    if (g->dose)
        memcpy(out + z0, acc, width * sizeof(double));
    else
        for (size_t z = 0; z < width; ++z)
            out[z0 + z] = acc[z] * g->scale;
}
static void integrate(const integral *g, const double *flux, size_t nz, double *out) {
    size_t z0 = 0;
    for (; z0 + TILE <= nz; z0 += TILE)
        integrate_tile(g, flux, nz, z0, TILE, out);
    if (z0 < nz)
        integrate_tile(g, flux, nz, z0, nz - z0, out);
}

#include "reaper_rates.inc"

static int finite_range(size_t n, const double *v, double lo, double hi) {
    int ok = 1;
    for (size_t i = 0; i < n; ++i)
        ok &= v[i] >= lo && v[i] <= hi;
    return ok;
}
static int all_finite(size_t n, const double *v) {
    return finite_range(n, v, -1.7976931348623157e308, 1.7976931348623157e308);
}

void tt_model_plan_radiators_changed(tt_model_plan *p) {
    if (p && p->reaper_roles)
        memset(p->reaper_roles, 0, p->radiator_count);
}

/* All radiator views are immutable copies. Only optical depth changes during
 * gas/O2 evaluation. A successful host replacement invalidates this cache. */
static const unsigned char *radiator_roles(tt_model_plan *p, const tt_model *m, size_t count) {
    if (!p->reaper_roles || fegetround() != FE_TONEAREST)
        return NULL;
    for (size_t j = 0; j < m->radiator_count; ++j) {
        if (p->reaper_roles[j])
            continue;
        const tt_radiator *r = m->radiator_views + j;
        int valid = 1, zero = 1, absorb = 1, scatter = 1;
        for (size_t k = 0; k < count; ++k) {
            double tau = r->optical_depth[k], a = r->single_scattering_albedo[k],
                   g = r->asymmetry[k];
            valid &= (tau >= 0) & (tau <= DBL_MAX) & (a >= 0) & (a <= 1) & (fabs(g) <= 1);
            zero &= tau == 0;
            absorb &= a == 0;
            scatter &= (a == 1) & (g == 0);
        }
        if (!valid)
            return NULL;
        int dynamic = m->radiators[j].gas || j == m->oxygen_radiator;
        p->reaper_roles[j] = (unsigned char)((zero && !dynamic ? 1
                                              : absorb         ? 2
                                              : scatter        ? 3
                                                               : 4) |
                                             (dynamic ? 8 : 0));
    }
    return p->reaper_roles;
}

static int run_plan(tt_model_plan *p, tt_model *m, double zenith, double scale, int reaper) {
    tt_status (*evaluate)(const tt_spectrum_plan *, size_t, const double *, const double *,
                          double *, double *) =
        reaper ? tt_spectrum_reaper_evaluate : tt_spectrum_plan_evaluate;
    const tt_grid *height = &m->grids[m->height].value,
                  *wavelength = &m->grids[m->wavelength].value;
    const tt_profile *temperature = &m->profiles[m->temperature].value,
                     *air = &m->profiles[m->air].value;
    size_t nz = m->shape.levels, nw = m->shape.wavelengths, nl = nz - 1, n = nz * nw,
           layer = nl * nw;
    if (!isfinite(scale) || !isfinite(zenith))
        return 0;
    /* The spectrum plans require valid, bounded temperatures and densities. */
    if (p->needs_temperature &&
        (!finite_range(nz, temperature->edges, 1.e-300, p->maximum_temperature) ||
         !finite_range(nl, temperature->midpoints, 1.e-300, p->maximum_temperature)))
        return 0;
    if (p->needs_air && !finite_range(nz, air->edges, 0, 1.7976931348623157e308))
        return 0;
    TT_TIME_START(geometry_time);
    if (tt_spherical_geometry(nz, height->edges, zenith, m->nid, m->path))
        return 0;
    int oxygen = m->bands.lyman_alpha != SIZE_MAX || m->bands.schumann_runge != SIZE_MAX;
    if (oxygen) {
        if (tt_air_mass(nz, air->extended_columns, m->nid, m->path, m->vertical, m->slant_air))
            return 0;
        if (m->scaled_oxygen)
            for (size_t i = 0; i < nz; ++i)
                m->slant_oxygen[i] = m->oxygen_scale * m->slant_air[i];
        else if (tt_air_mass(nz, m->profiles[m->oxygen].value.extended_columns, m->nid, m->path,
                             m->oxygen_vertical, m->slant_oxygen))
            return 0;
    }
    TT_TIME_END(&m->diagnostics, TT_STAGE_GEOMETRY, geometry_time);
    TT_TIME_START(gas_time);
    /* Gas radiators: optical depth = column * cross section, as tt_gas_optics. */
    for (size_t i = 0; i < m->radiator_count; ++i) {
        tt_model_radiator_entry *r = m->radiators + i;
        if (!r->gas)
            continue;
        const evaluation *e = p->evaluations + p->radiator_cross[i];
        const double *columns = m->profiles[r->profile].value.columns;
        const double *constant = tt_spectrum_plan_constant(e->plan);
        if (!finite_range(nl, columns, 0, 1.7976931348623157e308) ||
            evaluate(e->plan, nl, temperature->midpoints, air->midpoints, p->scratch, e->rows))
            return 0;
        for (size_t w = 0; w < nw; ++w) {
            const double *cross = e->position[w] == SIZE_MAX ? NULL : e->rows + e->position[w] * nl;
            double *od = r->data + w * nl;
            if (cross) {
                if (!finite_range(nl, cross, 0, 1.7976931348623157e308))
                    return 0;
                for (size_t l = 0; l < nl; ++l)
                    od[l] = columns[l] * cross[l];
            } else {
                double c = constant[w];
                if (!(c >= 0 && c <= 1.7976931348623157e308))
                    return 0;
                for (size_t l = 0; l < nl; ++l)
                    od[l] = columns[l] * c;
            }
        }
        if (!p->gas_initialized[i]) {
            for (size_t j = 0; j < layer; ++j) {
                r->data[layer + j] = r->view.is_air ? 1 : 0;
                r->data[2 * layer + j] = 0;
            }
            p->gas_initialized[i] = 1;
        }
    }
    TT_TIME_END(&m->diagnostics, TT_STAGE_GAS, gas_time);
    TT_TIME_START(oxygen_depth_time);
    int shared_oxygen = reaper && TT_REAPER_OXYGEN;
    if (oxygen && m->oxygen_radiator != SIZE_MAX) {
        tt_status status =
            shared_oxygen
                ? tt_oxygen_optics_work(
                      &m->bands, nw, nz, temperature->edges, m->vertical, m->slant_air,
                      m->slant_oxygen, m->radiators[m->oxygen_radiator].data,
                      p->oxygen_reactions ? p->oxygen_cross : NULL, p->oxygen_work)
                : tt_oxygen_optics(&m->bands, nw, nz, temperature->edges, m->vertical, m->slant_air,
                                   m->slant_oxygen, m->radiators[m->oxygen_radiator].data, NULL);
        if (status)
            return 0;
    }
    TT_TIME_END(&m->diagnostics, TT_STAGE_OXYGEN_DEPTH, oxygen_depth_time);
    TT_TIME_START(transfer_time);
    /* Radiation, multiplied by the scale as the generic run does. */
    tt_radiation *radiation = &m->result.radiation;
    if (m->host_solver) {
        for (size_t i = 0; i < 6 * n; ++i)
            m->radiation_storage[i] = m->host_radiation[i] * scale;
    } else if (m->streams == 1) {
        tt_status status =
            reaper && p->reaper_block
                ? tt_reaper_delta_eddington_profiled(
                      m->radiator_count, m->radiator_views, nl, nw, m->nid, m->path, zenith,
                      m->profiles[m->surface].value.midpoints, scale, p->reaper_block, radiation,
                      &m->diagnostics, radiator_roles(p, m, layer))
                : tt_block_delta_eddington_profiled(m->radiator_count, m->radiator_views, nl, nw,
                                                    m->nid, m->path, zenith,
                                                    m->profiles[m->surface].value.midpoints, scale,
                                                    p->block, radiation, &m->diagnostics);
        if (status)
            return 0;
    } else {
        if (tt_mix_radiators(m->radiator_count, m->radiator_views, nl, nw, m->streams,
                             m->optical_storage, m->optical_storage + layer,
                             m->optical_storage + 2 * layer) ||
            tt_discrete_ordinates(&m->atmosphere, m->streams, zenith,
                                  m->profiles[m->surface].value.midpoints, radiation))
            return 0;
        for (size_t i = 0; i < 6 * n; ++i)
            m->radiation_storage[i] = m->radiation_storage[i] * scale;
    }
    TT_TIME_END(&m->diagnostics, TT_STAGE_TRANSFER, transfer_time);
    TT_TIME_START(flux_time);
    if (m->solar != SIZE_MAX) {
        const double *solar = m->profiles[m->solar].value.midpoints;
        double *actinic = m->work, *irradiance = m->work + n;
        int flux_ok = 1;
        for (size_t w = 0; w < nw; ++w) {
            double s = solar[w], denominator = wavelength->midpoints[w] * 1.e-13;
            for (size_t z = 0; z < nz; ++z) {
                size_t i = w * nz + z;
                double f =
                    (radiation->direct_flux[i] + radiation->up_flux[i] + radiation->down_flux[i]) *
                    s;
                actinic[i] = f < 0 ? 0 : f;
                double e = (radiation->direct_irradiance[i] + radiation->up_irradiance[i] +
                            radiation->down_irradiance[i]) *
                           s * hc / denominator;
                irradiance[i] = e < 0 ? 0 : e;
                if (reaper && TT_REAPER_FLUX_CHECK)
                    flux_ok &= isfinite(actinic[i]) && isfinite(irradiance[i]);
            }
        }
        /* Omitted zero terms need finite fluxes. */
        if (reaper && TT_REAPER_FLUX_CHECK ? !flux_ok : !all_finite(2 * n, m->work))
            return 0;
    }
    TT_TIME_END(&m->diagnostics, TT_STAGE_FLUX, flux_time);
    /* Spectra of the reactions, then the rates. */
    if (m->shape.photolysis) {
        for (size_t i = 0; i < p->evaluation_count; ++i) {
            evaluation *e = p->evaluations + i;
            if (e->levels == nz) {
                TT_TIME_START(spectrum_time);
                if (evaluate(e->plan, nz, temperature->edges, air->edges, p->scratch, e->rows))
                    return 0;
                TT_TIME_END(&m->diagnostics,
                            TT_STAGE_SPECTRA_DIRECT + tt_spectrum_plan_category(e->plan),
                            spectrum_time);
            }
        }
        TT_TIME_START(oxygen_cross_time);
        if (oxygen && p->oxygen_reactions && !(shared_oxygen && m->oxygen_radiator != SIZE_MAX)) {
            tt_status status =
                shared_oxygen
                    ? tt_oxygen_optics_work(&m->bands, nw, nz, temperature->edges, m->vertical,
                                            m->slant_air, m->slant_oxygen, NULL, p->oxygen_cross,
                                            p->oxygen_work)
                    : tt_oxygen_optics(&m->bands, nw, nz, temperature->edges, m->vertical,
                                       m->slant_air, m->slant_oxygen, NULL, p->oxygen_cross);
            if (status)
                return 0;
        }
        TT_TIME_END(&m->diagnostics, TT_STAGE_OXYGEN_CROSS, oxygen_cross_time);
    }
    unsigned generated = 0;
    if (reaper && p->capabilities) {
        tt_reaper_view view = {nz,          nw,           p->evaluation_count + 1, m->work,
                               m->work + n, p->row_views, m->rate_storage};
        tt_reaper_kernel functions[] = {p->kernels.photolysis_kernel, p->kernels.heating_kernel,
                                        p->kernels.dose_kernel};
        for (unsigned k = 0; k < 3; ++k)
            if (functions[k]) {
                TT_TIME_START(generated_time);
                int accepted = functions[k](&view);
                TT_TIME_END(&m->diagnostics, TT_STAGE_PHOTOLYSIS + k, generated_time);
                if (accepted) {
                    generated |= 1u << k;
                    ++m->diagnostics.generated_calls;
                } else
                    ++m->diagnostics.generated_declines;
            }
    }
    for (size_t i = 0; i < p->integral_count; ++i) {
        const integral *g = p->integrals + i;
        unsigned capability = g->dose ? 4u : g->heating ? 2u : 1u;
        if (generated & capability)
            continue;
        TT_TIME_START(rate_time);
        if (reaper)
            reaper_integrate(g, g->dose ? m->work + n : m->work, nz, m->rate_storage + g->output);
        else
            integrate(g, g->dose ? m->work + n : m->work, nz, m->rate_storage + g->output);
        TT_TIME_END(&m->diagnostics,
                    g->dose      ? TT_STAGE_DOSE
                    : g->heating ? TT_STAGE_HEATING
                                 : TT_STAGE_PHOTOLYSIS,
                    rate_time);
    }
    return 1;
}

int tt_model_plan_run(tt_model_plan *p, tt_model *m, double zenith, double scale) {
    return run_plan(p, m, zenith, scale, 0);
}
int tt_model_reaper_run(tt_model_plan *p, tt_model *m, double zenith, double scale) {
    return run_plan(p, m, zenith, scale, 1);
}

/* Resolve prepared pointers into exported slot IDs and offsets by equality,
 * never pointer ordering/subtraction between unrelated allocations. */
static void write_row(const tt_model_plan *p, size_t nw, size_t nz, const double *row,
                      tt_reaper_writer *w) {
    if (!row) {
        tt_rw_text(w, "null");
        return;
    }
    for (size_t e = 0; e < p->evaluation_count; ++e)
        for (size_t k = 0; k < p->evaluations[e].varying; ++k)
            if (row == p->evaluations[e].rows + k * p->evaluations[e].levels) {
                tt_rw_text(w, "[");
                tt_rw_size(w, e);
                tt_rw_text(w, ",");
                tt_rw_size(w, k * p->evaluations[e].levels);
                tt_rw_text(w, "]");
                return;
            }
    for (size_t k = 0; p->oxygen_cross && k < nw; ++k)
        if (row == p->oxygen_cross + k * nz) {
            tt_rw_text(w, "[");
            tt_rw_size(w, p->evaluation_count);
            tt_rw_text(w, ",");
            tt_rw_size(w, k * nz);
            tt_rw_text(w, "]");
            return;
        }
    w->failed = 1;
}

tt_status tt_model_export_reaper(tt_model *m, FILE *file, unsigned char digest[32]) {
    if (!m || !m->plan || (!file && !digest))
        return TT_INVALID;
    if (sizeof(double) != 8 || DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024)
        return TT_NOT_FOUND;
    double one = 1;
    uint64_t bits;
    memcpy(&bits, &one, 8);
    if (bits != UINT64_C(0x3ff0000000000000))
        return TT_NOT_FOUND;
    tt_model_plan *p = m->plan;
    tt_reaper_writer w;
    tt_rw_init(&w, file);
    size_t nz = m->shape.levels, nw = m->shape.wavelengths;
    tt_rw_text(&w, "{\"schema\":");
    tt_rw_size(&w, TT_REAPER_SCHEMA);
    tt_rw_text(&w, ",\"abi\":");
    tt_rw_size(&w, TT_REAPER_ABI);
    tt_rw_text(&w, ",\"implementation\":");
    tt_rw_size(&w, TT_REAPER_IMPLEMENTATION);
    tt_rw_text(&w, ",\"layout\":\"wavelength-level-v1\",\"levels\":");
    tt_rw_size(&w, nz);
    tt_rw_text(&w, ",\"wavelengths\":");
    tt_rw_size(&w, nw);
    tt_rw_text(&w, ",\"outputs\":[");
    tt_rw_size(&w, m->shape.photolysis);
    tt_rw_text(&w, ",");
    tt_rw_size(&w, m->shape.heating);
    tt_rw_text(&w, ",");
    tt_rw_size(&w, m->shape.dose);
    tt_rw_text(&w, "],\"wavelength_edges\":");
    tt_rw_array(&w, m->grids[m->wavelength].value.edges, nw + 1);
    tt_rw_text(&w, ",\"wavelength_midpoints\":");
    tt_rw_array(&w, m->grids[m->wavelength].value.midpoints, nw);
    tt_rw_text(&w, ",\"slots\":[");
    for (size_t i = 0; i < p->evaluation_count; ++i) {
        if (i)
            tt_rw_text(&w, ",");
        tt_rw_text(&w, "{\"levels\":");
        tt_rw_size(&w, p->evaluations[i].levels);
        tt_rw_text(&w, ",\"spectrum\":");
        tt_spectrum_plan_write(p->evaluations[i].plan, &w);
        tt_rw_text(&w, "}");
    }
    tt_rw_text(&w, "],\"oxygen_slot\":");
    tt_rw_size(&w, p->evaluation_count);
    tt_rw_text(&w, ",\"rates\":[");
    for (size_t i = 0; i < p->integral_count; ++i) {
        const integral *g = p->integrals + i;
        unsigned family = g->dose ? 2 : g->heating ? 1 : 0;
        size_t index = g->output / nz - (g->dose      ? m->shape.photolysis + m->shape.heating
                                         : g->heating ? m->shape.photolysis
                                                      : 0);
        if (i)
            tt_rw_text(&w, ",");
        tt_rw_text(&w, "{\"family\":");
        tt_rw_size(&w, family);
        tt_rw_text(&w, ",\"name\":");
        tt_rw_string(&w, tt_model_rate_name(m, (tt_rate_kind)family, index));
        tt_rw_text(&w, ",\"output\":");
        tt_rw_size(&w, g->output);
        tt_rw_text(&w, ",\"scale\":");
        tt_rw_double(&w, g->scale);
        tt_rw_text(&w, ",\"terms\":[");
        for (size_t j = 0; j < g->count; ++j) {
            const term *t = g->terms + j;
            if (j)
                tt_rw_text(&w, ",");
            tt_rw_text(&w, "[");
            tt_rw_size(&w, t->w);
            tt_rw_text(&w, ",");
            tt_rw_size(&w, (size_t)t->kind);
            tt_rw_text(&w, ",");
            tt_rw_double(&w, t->value);
            tt_rw_text(&w, ",");
            tt_rw_double(&w, t->energy);
            tt_rw_text(&w, ",");
            write_row(p, nw, nz, t->cross, &w);
            tt_rw_text(&w, ",");
            write_row(p, nw, nz, t->yield, &w);
            tt_rw_text(&w, "]");
        }
        tt_rw_text(&w, "]}");
    }
    tt_rw_text(&w, "]}\n");
    unsigned char computed[32];
    tt_sha256_final(&w.sha, computed);
    if (w.failed)
        return TT_INVALID;
    memcpy(p->identity, computed, 32);
    p->has_identity = 1;
    if (digest)
        memcpy(digest, computed, 32);
    return TT_OK;
}

tt_status tt_model_reaper_identity(tt_model *m, unsigned char digest[32]) {
    if (!m || !m->plan || !digest)
        return TT_INVALID;
    if (!m->plan->has_identity)
        return tt_model_export_reaper(m, NULL, digest);
    memcpy(digest, m->plan->identity, 32);
    return TT_OK;
}

tt_status tt_model_attach_reaper(tt_model *m, const tt_reaper_kernels *k, tt_error *error) {
    if (error)
        memset(error, 0, sizeof *error);
    const char *reason = NULL;
    if (!m || !m->plan)
        reason = "REAPER attachment requires a prepared model";
    else if (!k) {
        memset(&m->plan->kernels, 0, sizeof m->plan->kernels);
        m->plan->capabilities = 0;
        m->plan->auto_checked = 1;
        return TT_OK;
    } else if (k->abi != TT_REAPER_ABI || k->schema != TT_REAPER_SCHEMA ||
               k->implementation != TT_REAPER_IMPLEMENTATION)
        reason = "REAPER ABI/schema/implementation mismatch";
    else if ((k->numerics != TT_REAPER_ORDERED && k->numerics != TT_REAPER_ROUNDOFF) ||
             (k->accumulators != 1 && k->accumulators != 2 && k->accumulators != 4 &&
              k->accumulators != 8) ||
             (k->numerics == TT_REAPER_ORDERED && k->accumulators != 1) ||
             (k->term_budget != 32 && k->term_budget != 64 && k->term_budget != 128 &&
              k->term_budget != 256) ||
             k->level_tile > 32)
        reason = "REAPER numerical mode or layout mismatch";
    else if (k->levels != m->shape.levels || k->wavelengths != m->shape.wavelengths ||
             k->slots != m->plan->evaluation_count + 1 || k->photolysis != m->shape.photolysis ||
             k->heating != m->shape.heating || k->dose != m->shape.dose)
        reason = "REAPER dimensions/slots mismatch";
    else {
        unsigned char digest[32];
        if (tt_model_reaper_identity(m, digest) || memcmp(k->model_sha256, digest, 32))
            reason = "REAPER model fingerprint mismatch";
    }
    if (reason) {
        if (error) {
            error->status = TT_INVALID;
            snprintf(error->message, sizeof error->message, "%s", reason);
        }
        return TT_INVALID;
    }
    m->plan->kernels = *k;
    m->plan->capabilities = (k->photolysis_kernel ? 1u : 0u) | (k->heating_kernel ? 2u : 0u) |
                            (k->dose_kernel ? 4u : 0u);
    m->plan->auto_checked = 1;
    return TT_OK;
}
unsigned tt_model_reaper_capabilities(const tt_model *m) {
    return m && m->plan ? m->plan->capabilities : 0;
}
extern const tt_reaper_kernels *const *tt_reaper_builtin_tables(size_t *count);
void tt_model_reaper_autoselect(tt_model *m) {
    if (!m || !m->plan)
        return;
    if (!m->plan->reaper_prepared) {
        m->plan->reaper_prepared = 1;
        m->plan->radiator_count = m->radiator_count;
        if (TT_REAPER_ROLES)
            m->plan->reaper_roles = calloc(m->radiator_count ? m->radiator_count : 1, 1);
        if (!m->host_solver && m->streams == 1) {
            size_t count =
                tt_reaper_delta_eddington_work(m->shape.levels - 1, m->shape.wavelengths);
            if (count && count <= SIZE_MAX / sizeof(double))
                m->plan->reaper_block = malloc(count * sizeof(double));
        }
    }
    if (m->plan->auto_checked)
        return;
    m->plan->auto_checked = 1;
    size_t count = 0;
    const tt_reaper_kernels *const *tables = tt_reaper_builtin_tables(&count);
    for (size_t i = 0; i < count; ++i)
        if (tt_model_attach_reaper(m, tables[i], NULL) == TT_OK)
            return;
}

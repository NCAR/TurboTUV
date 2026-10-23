/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TT_REAPER_MATH_H
#define TT_REAPER_MATH_H
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Experimental, bounded binary64 exponential. The ordinary paths retain
 * libm. See docs/REAPER_MATH.md for the independent error bound. */
static inline int tt_reaper_binary64(void) {
#if FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && DBL_MIN_EXP == -1021
    if (sizeof(double) != sizeof(uint64_t))
        return 0;
    double one = 1.0;
    uint64_t bits = 0;
    memcpy(&bits, &one, sizeof bits);
    return bits == UINT64_C(0x3ff0000000000000);
#else
    return 0;
#endif
}

/* In-place use is supported. All exceptional and out-of-domain operands use
 * libm, including arguments whose result could be subnormal. */
static inline void tt_reaper_exponential_array(size_t count, const double *arg, double *out,
                                               int base10) {
    if (!tt_reaper_binary64() || fegetround() != FE_TONEAREST) {
        for (size_t i = 0; i < count; ++i)
            out[i] = base10 ? pow(10, arg[i]) : exp(arg[i]);
        return;
    }
    static const double powers[64] = {
        0x1.0000000000000p+0, 0x1.02c9a3e778061p+0, 0x1.059b0d3158574p+0, 0x1.0874518759bc8p+0,
        0x1.0b5586cf9890fp+0, 0x1.0e3ec32d3d1a2p+0, 0x1.11301d0125b51p+0, 0x1.1429aaea92de0p+0,
        0x1.172b83c7d517bp+0, 0x1.1a35beb6fcb75p+0, 0x1.1d4873168b9aap+0, 0x1.2063b88628cd6p+0,
        0x1.2387a6e756238p+0, 0x1.26b4565e27cddp+0, 0x1.29e9df51fdee1p+0, 0x1.2d285a6e4030bp+0,
        0x1.306fe0a31b715p+0, 0x1.33c08b26416ffp+0, 0x1.371a7373aa9cbp+0, 0x1.3a7db34e59ff7p+0,
        0x1.3dea64c123422p+0, 0x1.4160a21f72e2ap+0, 0x1.44e086061892dp+0, 0x1.486a2b5c13cd0p+0,
        0x1.4bfdad5362a27p+0, 0x1.4f9b2769d2ca7p+0, 0x1.5342b569d4f82p+0, 0x1.56f4736b527dap+0,
        0x1.5ab07dd485429p+0, 0x1.5e76f15ad2148p+0, 0x1.6247eb03a5585p+0, 0x1.6623882552225p+0,
        0x1.6a09e667f3bcdp+0, 0x1.6dfb23c651a2fp+0, 0x1.71f75e8ec5f74p+0, 0x1.75feb564267c9p+0,
        0x1.7a11473eb0187p+0, 0x1.7e2f336cf4e62p+0, 0x1.82589994cce13p+0, 0x1.868d99b4492edp+0,
        0x1.8ace5422aa0dbp+0, 0x1.8f1ae99157736p+0, 0x1.93737b0cdc5e5p+0, 0x1.97d829fde4e50p+0,
        0x1.9c49182a3f090p+0, 0x1.a0c667b5de565p+0, 0x1.a5503b23e255dp+0, 0x1.a9e6b5579fdbfp+0,
        0x1.ae89f995ad3adp+0, 0x1.b33a2b84f15fbp+0, 0x1.b7f76f2fb5e47p+0, 0x1.bcc1e904bc1d2p+0,
        0x1.c199bdd85529cp+0, 0x1.c67f12e57d14bp+0, 0x1.cb720dcef9069p+0, 0x1.d072d4a07897cp+0,
        0x1.d5818dcfba487p+0, 0x1.da9e603db3285p+0, 0x1.dfc97337b9b5fp+0, 0x1.e502ee78b3ff6p+0,
        0x1.ea4afa2a490dap+0, 0x1.efa1bee615a27p+0, 0x1.f50765b6e4540p+0, 0x1.fa7c1819e90d8p+0,
    };
    double limit = base10 ? 260 : 600;
    int valid = 1;
    for (size_t i = 0; i < count; ++i)
        valid &= (arg[i] >= -limit) & (arg[i] <= limit);
    if (!valid) {
        for (size_t i = 0; i < count; ++i)
            out[i] = base10 ? pow(10, arg[i]) : exp(arg[i]);
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        double x = arg[i];
        double q = x * (base10 ? 0x1.a934f0979a371p+7 : 0x1.71547652b82fep+6);
        int k = (int)(q + copysign(0.5, q));
        double r;
        if (base10) {
            double high = x * 0x1.26bb1bbb00000p+1;
            double low = fma(x, 0x1.26bb1bbb00000p+1, -high);
            low = fma(x, 0x1.554560b752b6bp-33, low);
            r = fma(-(double)k, 0x1.a39ef35793c76p-39,
                    (high - (double)k * 0x1.62e42fee00000p-7) + low);
        } else
            r = fma(-(double)k, 0x1.a39ef35793c76p-39, x - (double)k * 0x1.62e42fee00000p-7);
        double p = 0x1.1111111111111p-7;
        p = fma(p, r, 0x1.5555555555555p-5);
        p = fma(p, r, 0x1.5555555555555p-3);
        p = fma(p, r, 0x1p-1);
        p = fma(p, r, 0x1p+0);
        p = fma(p, r, 0x1p+0);
        uint64_t index = (uint64_t)(k + 65536);
        p = p * powers[index & 63];
        uint64_t bits = ((index >> 6) - 1) << 52;
        double scale;
        memcpy(&scale, &bits, sizeof bits);
        out[i] = scale * p;
    }
}

static inline void tt_reaper_exp_array(size_t count, const double *arg, double *out) {
    tt_reaper_exponential_array(count, arg, out, 0);
}
static inline void tt_reaper_pow10_array(size_t count, const double *arg, double *out) {
    tt_reaper_exponential_array(count, arg, out, 1);
}
#endif

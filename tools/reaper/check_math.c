/* Independent development oracle; MPFR is not a library/build dependency. */
#include "reaper_math.h"
#include <mpfr.h>
#include <stdio.h>
#include <stdlib.h>

static mpfr_t mx, my, ln2, boundary;
static double maximum, maximum_libm, worst;
static size_t checked;
static int base10;
static uint64_t rng = UINT64_C(0x1a166c5103f71a1);
static uint64_t random_bits(void) {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    return rng ^= rng << 17;
}
static void check(size_t n, double *x) {
    double y[64];
    tt_reaper_exponential_array(n, x, y, base10);
    for (size_t i = 0; i < n; ++i) {
        mpfr_set_d(mx, x[i], MPFR_RNDN);
        if (base10)
            mpfr_exp10(my, mx, MPFR_RNDN);
        else
            mpfr_exp(my, mx, MPFR_RNDN);
        double expected = mpfr_get_d(my, MPFR_RNDN);
        double error = fabs(y[i] - expected) / expected / DBL_EPSILON;
        double library = fabs(y[i] - (base10 ? pow(10, x[i]) : exp(x[i]))) / expected / DBL_EPSILON;
        if (error > maximum) {
            maximum = error;
            worst = x[i];
        }
        if (library > maximum_libm)
            maximum_libm = library;
        if (!isfinite(error) || error > 8) {
            fprintf(stderr, "exp(%a): %.17g eps\n", x[i], error);
            exit(1);
        }
        ++checked;
    }
}
static void neighbors(double x) {
    double a[17];
    a[8] = x;
    for (int j = 7; j >= 0; --j)
        a[j] = nextafter(a[j + 1], -INFINITY);
    for (int j = 9; j < 17; ++j)
        a[j] = nextafter(a[j - 1], INFINITY);
    check(17, a);
}
int main(int argc, char **argv) {
    size_t random_count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 100000;
    mpfr_inits2(200, mx, my, ln2, boundary, (mpfr_ptr)0);
    base10 = argc > 2 ? atoi(argv[2]) : 0;
    double limit = base10 ? 260 : 600;
    mpfr_const_log2(ln2, MPFR_RNDN);
    if (base10) {
        mpfr_set_ui(mx, 10, MPFR_RNDN);
        mpfr_log(mx, mx, MPFR_RNDN);
        mpfr_div(ln2, ln2, mx, MPFR_RNDN);
    }
    mpfr_div_ui(ln2, ln2, 64, MPFR_RNDN);
    /* Every range-reduction switch and every integer multiple of ln(2). */
    for (int k = -55400; k <= 55400; ++k) {
        for (int half = 0; half < 2; ++half) {
            mpfr_mul_d(boundary, ln2, k + 0.5 * half, MPFR_RNDN);
            double x = mpfr_get_d(boundary, MPFR_RNDN);
            if (fabs(x) <= limit)
                neighbors(x);
        }
    }
    neighbors(-limit);
    neighbors(limit);
    neighbors(0);
    double a[64];
    for (size_t i = 0; i < random_count; i += 64) {
        for (size_t j = 0; j < 64; ++j)
            a[j] = -limit + 2 * limit * (double)(random_bits() >> 11) * 0x1p-53;
        check(64, a);
    }
    /* Dense reduction residuals, independent of the two model inputs. */
    for (size_t i = 0; i < 65536; i += 64) {
        for (size_t j = 0; j < 64; ++j)
            a[j] = -0.00542 + 0.01084 * (double)(i + j) / 65535;
        check(64, a);
    }
    printf("{\"checked\":%zu,\"max_eps_mpfr\":%.17g,\"max_eps_libm\":%.17g,\"worst\":\"%a\"}\n",
           checked, maximum, maximum_libm, worst);
    mpfr_clears(mx, my, ln2, boundary, (mpfr_ptr)0);
    return 0;
}

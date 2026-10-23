#include "reaper_math.h"
#include <stdio.h>
#include <stdlib.h>

static void require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "REAPER math: %s\n", message);
        exit(1);
    }
}
static int same(double a, double b) {
    return (isnan(a) && isnan(b)) || !memcmp(&a, &b, sizeof a);
}
int main(void) {
    const double unusual[] = {0,       -0.0,     INFINITY,  -INFINITY,  NAN, DBL_MAX, -DBL_MAX,
                              DBL_MIN, -DBL_MIN, 0x1p-1074, -0x1p-1074, 709, -745,    308,
                              -324,    600,      -600,      260,        -260};
    double a[129], b[129], c[129];
    int initial = fegetround();
    require(!fesetround(FE_TONEAREST), "nearest rounding unavailable");
    for (int base = 0; base < 2; ++base) {
        double limit = base ? 260 : 600;
        for (size_t n = 0; n <= 129; ++n) {
            for (size_t i = 0; i < n; ++i)
                a[i] = c[i] = -limit + 2 * limit * (double)i / 129;
            tt_reaper_exponential_array(n, a, b, base);
            tt_reaper_exponential_array(n, c, c, base);
            for (size_t i = 0; i < n; ++i) {
                double ref = base ? pow(10, a[i]) : exp(a[i]);
                require(same(b[i], c[i]), "in-place result differs");
                require(fabs(b[i] - ref) <= 8 * DBL_EPSILON * ref, "ordinary value");
            }
        }
        for (size_t j = 0; j < sizeof unusual / sizeof *unusual; ++j) {
            double x = unusual[j];
            a[0] = c[0] = x;
            tt_reaper_exponential_array(1, a, b, base);
            tt_reaper_exponential_array(1, c, c, base);
            require(same(b[0], c[0]), "exceptional in-place result");
            double ref = base ? pow(10, x) : exp(x);
            if (!((x >= -limit) & (x <= limit)))
                require(same(b[0], ref), "exceptional fallback");
            else
                require(fabs(b[0] - ref) <= 8 * DBL_EPSILON * ref, "domain boundary");
        }
        /* Test both adjacent inside values as single fast-path operands. */
        for (int sign = -1; sign <= 1; sign += 2) {
            double x = sign * limit;
            for (int j = 0; j < 32; ++j) {
                tt_reaper_exponential_array(1, &x, b, base);
                double ref = base ? pow(10, x) : exp(x);
                require(fabs(b[0] - ref) <= 8 * DBL_EPSILON * ref, "inside boundary");
                x = nextafter(x, 0);
            }
        }
        const int modes[] = {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
        for (size_t m = 0; m < sizeof modes / sizeof *modes; ++m) {
            if (fesetround(modes[m]))
                continue;
            for (size_t i = 0; i < 129; ++i)
                a[i] = c[i] = -limit + 2 * limit * (double)i / 129;
            tt_reaper_exponential_array(129, a, b, base);
            tt_reaper_exponential_array(129, c, c, base);
            for (size_t i = 0; i < 129; ++i) {
                double ref = base ? pow(10, a[i]) : exp(a[i]);
                require(same(b[i], c[i]) && same(b[i], ref), "rounding mode fallback");
            }
        }
        require(!fesetround(FE_TONEAREST), "restore nearest");
    }
    require(!fesetround(initial), "restore caller rounding");
    puts("REAPER bounded math: tails, in-place, exceptions, boundaries and rounding modes pass");
    return 0;
}

/* Independent development oracle for the emitted exp2 table.
 * MPFR is not a numerical-library or ordinary-build dependency. */
#include <float.h>
#include <math.h>
#include <mpfr.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    unsigned long n = argc == 2 ? strtoul(argv[1], NULL, 10) : 0;
    if (!n || n > 65536 || (n & (n - 1)) || DBL_MANT_DIG != 53 || FLT_RADIX != 2)
        return 2;
    mpfr_t x, y;
    mpfr_init2(x, 200);
    /* Destination precision 53 avoids double rounding through a wider result. */
    mpfr_init2(y, 53);
    for (unsigned long i = 0; i < n; ++i) {
        double actual;
        if (scanf("%la", &actual) != 1)
            return 3;
        mpfr_set_ui(x, i, MPFR_RNDN);
        mpfr_div_ui(x, x, n, MPFR_RNDN); /* i/n is exact: n is a power of two. */
        mpfr_exp2(y, x, MPFR_RNDN);
        double expected = mpfr_get_d(y, MPFR_RNDN);
        if (actual != expected) {
            fprintf(stderr, "table[%lu]: %a != %a\n", i, actual, expected);
            return 1;
        }
    }
    char extra;
    if (scanf(" %c", &extra) != EOF)
        return 4;
    mpfr_clears(x, y, (mpfr_ptr)0);
    printf("{\"checked\":%lu,\"destination_precision\":53,\"exact\":true}\n", n);
    return 0;
}

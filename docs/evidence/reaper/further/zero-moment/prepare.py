from pathlib import Path
s=Path('src/radiation_reaper.c').read_text().replace('#include <math.h>','#include <math.h>\n#include <fenv.h>')
s=s.replace('''    for (size_t q = 0; q < n * BLOCK; ++q) {
        double gu''','''    int nearest=fegetround()==FE_TONEAREST;
    for(size_t row=0;row<n;++row) {
        int zero=nearest && (TT_ZERO_MOMENT & 1);
        for(size_t c=0;c<BLOCK && zero;++c) zero &= gp[row*BLOCK+c]==0;
        if(zero) continue;
        for(size_t q=row*BLOCK;q<(row+1)*BLOCK;++q) {
        double gu''')
s=s.replace('''        taup[q] = (1.0 - omu * f) * tau;
    }
    return TT_OK;''','''        taup[q] = (1.0 - omu * f) * tau;
        }
    }
    return TT_OK;''')
s=s.replace('''    double tauc[BLOCK];''','''    int nearest=fegetround()==FE_TONEAREST;
    double tauc[BLOCK];''')
s=s.replace('''        int valid_root = 1;''','''        int valid_root = 1;
        int zero=nearest && (TT_ZERO_MOMENT & 2);
        for(size_t c=0;c<BLOCK && zero;++c) zero &= gp[c]==0;''')
s=s.replace('''            double gi = copysign(fmin(fabs(gp[c]), 1.0 - 1.e-7), gp[c]);
            omi[c] = fmin(omp[c], 1.0 - 1.e-7);
            gam1[c] = (7.0 - omi[c] * (4.0 + 3.0 * gi)) / 4.0;
            gam2[c] = -(1.0 - omi[c] * (4.0 - 3.0 * gi)) / 4.0;
            gam3[c] = (2.0 - 3.0 * gi * mu) / 4.0;
            gam4[c] = 1.0 - gam3[c];''','''            omi[c] = fmin(omp[c], 1.0 - 1.e-7);
            if(zero) {
                gam1[c]=(7.0-omi[c]*4.0)/4.0;
                gam2[c]=-(1.0-omi[c]*4.0)/4.0;
                gam3[c]=.5;gam4[c]=.5;
            } else {
                double gi = copysign(fmin(fabs(gp[c]), 1.0 - 1.e-7), gp[c]);
                gam1[c] = (7.0 - omi[c] * (4.0 + 3.0 * gi)) / 4.0;
                gam2[c] = -(1.0 - omi[c] * (4.0 - 3.0 * gi)) / 4.0;
                gam3[c] = (2.0 - 3.0 * gi * mu) / 4.0;
                gam4[c] = 1.0 - gam3[c];
            }''')
Path('build/reaper/zero-moment-prototype/radiation_reaper.c').write_text(s)
s=Path('build/reaper/accurate-transfer-sweep.py').read_text().replace('accurate-transfer','zero-moment').replace('accuracy-prototype','zero-moment-prototype').replace('TT_TRANSFER_EXP','TT_ZERO_MOMENT')
Path('build/reaper/zero-moment-sweep.py').write_text(s)
Path('docs/evidence/reaper/further/zero-moment/commands.py').write_text(s)

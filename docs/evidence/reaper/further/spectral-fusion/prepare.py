from pathlib import Path
root=Path('build/reaper/spectral-fusion-prototype')
s=Path('src/reaper_math.h').read_text()
a=s.index('    static const double powers[64]'); b=s.index('    double limit',a)
table=s[a:b];s=s[:a]+s[b:]
a=s.index('    for (size_t i = 0; i < count; ++i) {\n        double x = arg[i];',s.index('#pragma GCC unroll 4'))
b=s.index('\n}\n\nstatic inline void tt_reaper_exp_array',a)
body=s[a:b]
core=body.removeprefix('    for (size_t i = 0; i < count; ++i) {\n        double x = arg[i];\n').removesuffix('\n    }').replace('out[i] = scale * p;', 'return scale * p;')
s=s[:a]+'    for (size_t i = 0; i < count; ++i)\n        out[i] = tt_reaper_exp_core(arg[i], base10);'+s[b:]
pos=s.index('/* In-place use is supported.')
s=s[:pos]+'static inline double tt_reaper_exp_core(double x, int base10) {\n'+table+core+'\n}\n\n'+s[pos:]
hints='''#if TT_REAPER_MATH_HINTS && defined(__clang__) && __clang_major__ >= 4
#pragma clang loop vectorize_width(2) interleave_count(2)
#elif TT_REAPER_MATH_HINTS && defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC unroll 4
#endif
'''
more='''
static inline void tt_reaper_affine_exp(size_t count, const double *x, double slope,
                                       double intercept, double prefactor, int base10, double *out) {
    double limit=base10 ? 260 : 600;
    int valid=tt_reaper_binary64() && fegetround()==FE_TONEAREST;
    for(size_t i=0;i<count;++i) {
        double a=intercept+slope*x[i];
        valid &= (a>=-limit) & (a<=limit);
    }
    if(!valid) {
        for(size_t i=0;i<count;++i) {
            double a=intercept+slope*x[i];
            out[i]=prefactor*(base10 ? pow(10,a) : exp(a));
        }
        return;
    }
HINTS
    for(size_t i=0;i<count;++i)
        out[i]=prefactor*tt_reaper_exp_core(intercept+slope*x[i],base10);
}
static inline void tt_reaper_cl2_exp(size_t count, const double *x1,const double *x2,
                                     const double *prefactor, double c0,double c1,double *out) {
    int valid=tt_reaper_binary64() && fegetround()==FE_TONEAREST;
    for(size_t i=0;i<count;++i) {
        double a=x1[i]*c0,b=x2[i]*c1;
        valid &= (a>=-600) & (a<=600) & (b>=-600) & (b<=600);
    }
    if(!valid) {
        for(size_t i=0;i<count;++i)
            out[i]=prefactor[i]*(27.3*exp(x1[i]*c0)+.932*exp(x2[i]*c1));
        return;
    }
HINTS
    for(size_t i=0;i<count;++i)
        out[i]=prefactor[i]*(27.3*tt_reaper_exp_core(x1[i]*c0,0)+.932*tt_reaper_exp_core(x2[i]*c1,0));
}
'''.replace('HINTS',hints)
pos=s.rindex('#endif');s=s[:pos]+more+s[pos:]
(root/'reaper_math.h').write_text(s)
s=Path('src/spectrum_plan.c').read_text()
for name,base10 in [('pow10',1),('exp',0)]:
 old=f'''                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[1] * z[i];
                tt_reaper_{name}_array(nz, out, out);
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[0] * out[i];'''
 new=f'''#if TT_SPECTRAL_FUSION & 1
                tt_reaper_affine_exp(nz,z,c[1],0,c[0],{base10},out);
#else
{old}
#endif'''
 assert old in s;s=s.replace(old,new)
old='''                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[0] + z[i] * c[1];
                tt_reaper_exp_array(nz, out, out);'''
s=s.replace(old,'''#if TT_SPECTRAL_FUSION & 1
                tt_reaper_affine_exp(nz,z,c[1],c[0],1,0,out);
#else
'''+old+'\n#endif')
old='''                double *ex1 = z + 3 * nz, *ex2 = ex1 + nz;'''
s=s.replace(old,'''#if TT_SPECTRAL_FUSION & 2
                tt_reaper_cl2_exp(nz,z,z+nz,z+2*nz,c[0],c[1],out);
#else
'''+old)
old='''                    out[i] = z[2 * nz + i] * (27.3 * ex1[i] + .932 * ex2[i]);
                break;'''
s=s.replace(old,'''                    out[i] = z[2 * nz + i] * (27.3 * ex1[i] + .932 * ex2[i]);
#endif
                break;''')
old='''        case TT_CS_HCFC:
            for (size_t i = 0; i < nz; ++i) {
                double sigma = 0;'''
s=s.replace(old,'''        case TT_CS_HCFC:
#if TT_SPECTRAL_FUSION & 4
            if (reaper) {
                for(size_t i=0;i<nz;++i) out[i]=0;
                for(size_t j=0;j<t[0].rows;++j)
                    for(size_t i=0;i<nz;++i)
                        out[i]=out[i]+z[j*nz+i]*c[j];
                if(TT_REAPER_SPECTRUM_EXP) tt_reaper_exp_array(nz,out,out);
                else for(size_t i=0;i<nz;++i) out[i]=exp(out[i]);
                break;
            }
#endif
            for (size_t i = 0; i < nz; ++i) {
                double sigma = 0;''')
(root/'spectrum_plan.c').write_text(s)
s=Path('build/reaper/spectral-scatter-sweep.py').read_text().replace('spectral-scatter','spectral-fusion').replace('scatter-prototype','spectral-fusion-prototype').replace('for mode in [1,2]:','for mode in [1,2,4,7]:').replace('TT_SCATTER','TT_SPECTRAL_FUSION')
Path('build/reaper/spectral-fusion-sweep.py').write_text(s)
Path('docs/evidence/reaper/further/spectral-fusion/commands.py').write_text(s)
import gzip
Path('docs/evidence/reaper/further/spectral-fusion/reaper_math.h.gz').write_bytes(gzip.compress((root/'reaper_math.h').read_bytes(),mtime=0))

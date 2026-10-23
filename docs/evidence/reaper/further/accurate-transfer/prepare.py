from decimal import Decimal, localcontext
from pathlib import Path
root=Path('build/reaper/accuracy-prototype')
with localcontext() as c:
 c.prec=110
 ln2=Decimal(2).ln(); vals=[(ln2*Decimal(i)/64).exp() for i in range(64)]
 hi=[float(v) for v in vals]; lo=[float(v-Decimal.from_float(h)) for v,h in zip(vals,hi)]
 third=float(ln2/64-Decimal.from_float(float.fromhex('0x1.62e42fee00000p-7'))-Decimal.from_float(float.fromhex('0x1.a39ef35793c76p-39')))
head='''/* Experimental compensated transfer exponential; not validated for production. */
#ifndef TT_TRANSFER_MATH_H
#define TT_TRANSFER_MATH_H
#include "reaper_math.h"
static inline void transfer_exp(size_t count, const double *arg, double *out) {
    int valid = tt_reaper_binary64() && fegetround() == FE_TONEAREST;
    for (size_t i=0; i<count; ++i) valid &= (arg[i]>=-600) & (arg[i]<=600);
    if (!valid) {
        for (size_t i=0; i<count; ++i) out[i]=exp(arg[i]);
        return;
    }
'''
for name,values in [('hi',hi),('lo',lo)]:
 head+='    static const double '+name+'[64] = {\n'
 for i in range(0,64,4):head+='        '+', '.join(x.hex() for x in values[i:i+4])+',\n'
 head+='    };\n'
head+='''#if defined(__clang__)
#pragma clang loop vectorize_width(2) interleave_count(2)
#elif defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC unroll 4
#endif
    for (size_t i=0; i<count; ++i) {
        double x=arg[i], q=x*0x1.71547652b82fep+6;
        int k=(int)(q+copysign(.5,q));
        double rh=x-(double)k*0x1.62e42fee00000p-7;
        double r=fma(-(double)k,0x1.a39ef35793c76p-39,rh);
        double dr=fma(-(double)k,0x1.a39ef35793c76p-39,rh-r);
        dr=fma(-(double)k,THIRD,dr);
        double p=0x1.a01a01a01a01ap-13;
        p=fma(p,r,0x1.6c16c16c16c17p-10);
        p=fma(p,r,0x1.1111111111111p-7);
        p=fma(p,r,0x1.5555555555555p-5);
        p=fma(p,r,0x1.5555555555555p-3);
        p=fma(p,r,0x1p-1);
        double poly=(r*r)*p;
        uint64_t index=(uint64_t)(k+65536);
        double t=hi[index&63], tl=lo[index&63];
        double tail_hi=fma(t,r,tl);
        double tail_err=fma(t,r,-tail_hi)+tl;
        double tail=fma(t,poly,tail_hi);
        tail_err+=fma(t,poly,tail_hi-tail);
        tail_err+=(t+tail)*dr+tl*(r+poly);
        double y=t+tail;
        y+=((t-y)+tail)+tail_err;
        uint64_t bits=((index>>6)-1)<<52;
        double scale; memcpy(&scale,&bits,sizeof bits);
        out[i]=scale*y;
    }
}
#endif
'''.replace('THIRD',third.hex())
(root/'transfer_math.h').write_text(head)
s=Path('src/radiation_reaper.c').read_text().replace('#include "reaper_internal.h"','#include "reaper_internal.h"\n#include "transfer_math.h"')
s=s.replace('''    for (size_t l = 0; l <= n; ++l)
        for (size_t c = 0; c < BLOCK; ++c)
            b_ex[l * BLOCK + c] = exp(-b_slant[l * BLOCK + c]);''','''#if TT_TRANSFER_EXP & 1
    for(size_t i=0;i<(n+1)*BLOCK;++i) b_ex[i]=-b_slant[i];
    transfer_exp((n+1)*BLOCK,b_ex,b_ex);
#else
    for (size_t l = 0; l <= n; ++l)
        for (size_t c = 0; c < BLOCK; ++c)
            b_ex[l * BLOCK + c] = exp(-b_slant[l * BLOCK + c]);
#endif''')
s=s.replace('''        for (size_t c = 0; c < BLOCK; ++c)
            expon[c] = exp(arg[c]);''','''#if TT_TRANSFER_EXP & 2
        transfer_exp(BLOCK,arg,expon);
#else
        for (size_t c = 0; c < BLOCK; ++c)
            expon[c] = exp(arg[c]);
#endif''')
assert s.count('TT_TRANSFER_EXP')==2
(root/'radiation_reaper.c').write_text(s)
s=Path('tools/reaper/check_math.c').read_text().replace('#include "reaper_math.h"','#include "transfer_math.h"').replace('tt_reaper_exponential_array(n, x, y, base10);','transfer_exp(n, x, y);')
(root/'check_transfer_math.c').write_text(s)
print('third constant',third.hex())

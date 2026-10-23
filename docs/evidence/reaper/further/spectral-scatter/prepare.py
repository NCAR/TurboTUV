from pathlib import Path
s=Path('src/spectrum_plan.c').read_text()
start=s.index('static void direct_cached('); end=s.index('\n/* ---- Quantum yields',start)
old=s[start:end].replace('static void direct_cached(', 'static void direct_cached_original(')
new=r'''
/* Experimental: classify once and scatter by indices or contiguous runs. */
static void direct_cached(const tt_spectrum_plan *p, size_t nz, const double *temperature,
                          double *scratch, double *rows) {
    if (nz > 0x1p51 || nz > SIZE_MAX/3) {
        direct_cached_original(p,nz,temperature,scratch,rows);
        return;
    }
    double *packed=scratch,*work=packed+nz,*values=work+cs_scratch(p,nz);
    size_t active=0;
#if TT_SCATTER == 1
    int have_low=0,have_high=0;
    for(size_t z=0;z<nz;++z) {
        have_low |= temperature[z]<=p->clamp_low;
        have_high |= temperature[z]>=p->clamp_high;
    }
    size_t low=0,high=(size_t)have_low;
    if(have_low) packed[active++]=p->clamp_low;
    if(have_high) packed[active++]=p->clamp_high;
#endif
    for(size_t z=0;z<nz;++z)
        if(!(temperature[z]<=p->clamp_low || temperature[z]>=p->clamp_high))
            packed[active++]=temperature[z];
#if TT_SCATTER == 2
    if(active==nz) {
        cs_evaluate(p,nz,temperature,scratch,rows,1);
        return;
    }
#endif
    if(active) cs_evaluate(p,active,packed,work,values,1);
#if TT_SCATTER == 1
    size_t j=(size_t)(have_low+have_high);
    for(size_t z=0;z<nz;++z)
        scratch[z]=(double)(temperature[z]<=p->clamp_low ? low : temperature[z]>=p->clamp_high ? high : j++);
    for(size_t k=0;k<p->count;++k) {
        if(have_low) values[k*active+low]=p->clamped[2*k];
        if(have_high) values[k*active+high]=p->clamped[2*k+1];
        for(size_t z=0;z<nz;++z) rows[k*nz+z]=values[k*active+(size_t)scratch[z]];
    }
#else
    size_t nr=0,z=0;
    while(z<nz) {
        int kind=temperature[z]<=p->clamp_low ? 1 : temperature[z]>=p->clamp_high ? 2 : 0;
        size_t end=z+1;
        while(end<nz) {
            int next=temperature[end]<=p->clamp_low ? 1 : temperature[end]>=p->clamp_high ? 2 : 0;
            if(next!=kind) break;
            ++end;
        }
        scratch[nr++]=(double)(3*end+(size_t)kind);
        z=end;
    }
    for(size_t k=0;k<p->count;++k) {
        size_t pos=0,j=0;
        for(size_t r=0;r<nr;++r) {
            size_t code=(size_t)scratch[r],end=code/3,kind=code%3;
            if(!kind) {
                memcpy(rows+k*nz+pos,values+k*active+j,(end-pos)*sizeof(double));
                j+=end-pos;
            } else {
                double value=p->clamped[2*k+kind-1];
                for(size_t i=pos;i<end;++i) rows[k*nz+i]=value;
            }
            pos=end;
        }
    }
#endif
}
'''
s=s[:start]+old+new+s[end:]
Path('build/reaper/scatter-prototype/spectrum_plan.c').write_text(s)
s=Path('build/reaper/fusion-sweep.py').read_text().replace('r4-selected-', 'r3b-selected-').replace("root=Path(f'build/reaper/fusion-{f}')", "root=Path(f'build/reaper/spectral-scatter-{f}')").replace('docs/evidence/reaper/continuation/fusion','docs/evidence/reaper/further/spectral-scatter')
s=s.replace('build/reaper/continuation-prototype/radiation_fusion.c','build/reaper/scatter-prototype/spectrum_plan.c').replace('radiation_reaper.c','spectrum_plan.c')
s=s.replace("commands=[('r4',base/'turbotuv_bench')]", "commands=[('selected',base/'turbotuv_bench')]")
s=s.replace('for block,fused in [(8,1),(16,1),(32,1),(16,0)]:', 'for mode in [1,2]:').replace("name=f'block-{block}-fused-{fused}'", "name=f'mode-{mode}'").replace("f'-DTT_REAPER_BLOCK={block}',f'-DTT_REAPER_FUSE={fused}'", "f'-DTT_SCATTER={mode}'")
Path('build/reaper/spectral-scatter-sweep.py').write_text(s)
Path('docs/evidence/reaper/further/spectral-scatter/commands.py').write_text(s)

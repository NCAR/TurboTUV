from pathlib import Path
import re
s=Path('src/radiation_reaper.c').read_text()
s=s.replace('BLOCK = TT_REAPER_BLOCK,','BLOCK = TT_REAPER_BLOCK,\n    WIDE = TT_SLANT_WIDTH,')
s=s.replace('* BLOCK','* WIDE').replace('/ BLOCK','/ WIDE').replace('+ BLOCK', '+ WIDE').replace('[BLOCK +', '[WIDE +')
a=s.index('static tt_status scale_layers(');b=s.index('/* Eliminate a pair',a)
segment=s[a:b].replace('BLOCK','WIDE');s=s[:a]+segment+s[b:]
a=s.index('    for (size_t w0 = 0; w0 < nw; w0 += BLOCK) {')
s=s[:a]+s[a:].replace('w0 += BLOCK','w0 += WIDE').replace('nw - w0 < BLOCK ? nw - w0 : BLOCK','nw - w0 < WIDE ? nw - w0 : WIDE').replace('double albedo[BLOCK]','double albedo[WIDE]').replace('c < BLOCK','c < WIDE')
a=s.index('        TT_TIME_START(coefficients_time);',a)
b=s.index('        TT_TIME_END(diagnostics, TT_STAGE_OUTPUT, output_time);',a)+len('        TT_TIME_END(diagnostics, TT_STAGE_OUTPUT, output_time);')
old=s[a:b]
old=old.replace('coefficients(n, nid, mu, &s)','coefficients(n, nid, mu, &lane)').replace('finish_solve(n, mu, albedo, &s)','finish_solve(n, mu, albedo+offset, &lane)').replace('output(n, w0, count, mu, scale, &s, out)','output(n, w0+offset, width, mu, scale, &lane, out)')
fields='g om taun slant ex e1 e2 e3 e4 cup cdn cuptn cdntn cp u'.split()
new='''        for(size_t offset=0;offset<count;offset+=BLOCK) {
            size_t width=count-offset<BLOCK ? count-offset : BLOCK;
            block_work lane=s;
'''+''.join(f'            lane.{field}+=offset;\n' for field in fields)+old+'\n        }'
s=s[:a]+new+s[b:]
Path('build/reaper/wide-slant-prototype/radiation_reaper.c').write_text(s)
s=Path('build/reaper/accurate-transfer-sweep.py').read_text().replace('accurate-transfer','wide-slant').replace('accuracy-prototype','wide-slant-prototype').replace('TT_TRANSFER_EXP','TT_SLANT_WIDTH').replace('for mode in [1,2,3]:','for mode in [16,32,64,128]:')
Path('build/reaper/wide-slant-sweep.py').write_text(s)
Path('docs/evidence/reaper/further/wide-slant/commands.py').write_text(s)

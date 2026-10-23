#!/usr/bin/env python3
"""Generate and gate untuned model shapes/rate lists with a fresh input seed.

This development test exports every model through the public C builder, builds
ordered and roundoff artifacts, then compares all returned values. It tests
selection/fallback on an artifact built for the original two primary models too.
"""
import argparse
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
from artifact import build


def run(cmd):
    return subprocess.check_output(list(map(str,cmd)),text=True)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base',type=Path,required=True)
    p.add_argument('--primary',type=Path,required=True)
    p.add_argument('--cc',required=True)
    p.add_argument('--sysroot',required=True)
    p.add_argument('--work',type=Path,required=True)
    p.add_argument('--evidence',type=Path,required=True)
    a=p.parse_args()
    a.work.mkdir(parents=True,exist_ok=True)
    a.evidence.mkdir(parents=True,exist_ok=True)
    specs=[('one-layer', 'tuv_5_4',1,37,0),('two-layer','ts1_tsmlt',2,43,0),
           ('odd-level','tuv_5_4',16,51,0),('odd-layer','ts1_tsmlt',17,55,0),
           ('discrete','ts1_tsmlt',7,31,4),('reordered','tuv_5_4',120,0,0)]
    plans=[]
    for name,original,nl,nw,streams in specs:
        c=json.loads(Path(f'examples/{original}.json').read_text())
        c['grids'][0]['cell delta']=120/nl
        if nw:
            original_grid = json.loads(run([a.base/'turbotuv',f'examples/{original}.json',
                                           '--zenith','30']))['wavelength_edges_nm']
            # O2's fixed bands remain complete; vary the rest of the grid.
            prefix = [120.,121.4,121.9] + [x for x in original_grid if 175.4 <= x <= 206.2]
            tail = nw + 1 - len(prefix)
            assert tail > 0
            c['grids'][1]={'name':'wavelength','units':'nm','type':'from config file',
                           'values':prefix + [206.2+(800-206.2)*i/tail for i in range(1,tail+1)]}
        # Change ordering, count, scale and embedded rate constants.
        reactions=c['photolysis']['reactions']
        c['photolysis']['reactions']=list(reversed(reactions if name=='reordered' else reactions[::3]))
        for i,r in enumerate(c['photolysis']['reactions']):
            r['scaling factor']=0.3+0.1*i
        if streams:
            c['radiative transfer']['solver']={'type':'discrete ordinate','number of streams':streams}
        path=a.work/f'{name}.json'
        path.write_text(json.dumps(c,indent=2)+'\n')
        plan=a.work/f'{name}-plan.json'
        run([a.base/'turbotuv_reaper_export',path,plan])
        plans.append(plan)
    flags=['-std=c11','-O3','-fPIC','-DNDEBUG','-ffp-contract=off','-Iinclude','-isysroot',a.sysroot]
    records=[]
    for mode,acc in [('ordered',1),('roundoff',4)]:
        directory=a.work/mode
        record=build(plans,directory,a.cc,flags,32,acc,8,2,False,True)
        lib=directory/'libturbotuv.a'
        shutil.copy2(a.base/'libturbotuv.a',lib)
        members = run(['ar','t',lib]).splitlines()
        remove = [m for m in members if m == 'reaper_dispatch.c.o' or m == 'registry.c.o'
                  or re.fullmatch(r'kernel-\d+\.c\.o',m)]
        if remove:
            run(['ar','d',lib,*remove])
        run(['ar','r',lib,*record['objects']])
        exe=directory/'turbotuv_bench'
        run([a.cc,*flags,a.base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
        shutil.copy2(a.base/'turbotuv',directory/'turbotuv')
        record['gates']=[]
        for name,*_ in specs:
            config=a.work/f'{name}.json'
            gate=a.evidence/f'{mode}-{name}.json'
            run([sys.executable,'tools/reaper/compare.py',str(a.base/'turbotuv_bench'),
                 str(exe)+':--reaper','--config',config,'--columns','48','--seed','934897','--json',gate])
            report=json.loads(gate.read_text())
            assert report['passed'] and int(report['candidate']['fields']['generated_calls'])>=48
            if mode=='ordered':
                assert report['reference']['output_sha256']==report['candidate']['output_sha256']
            record['gates'].append(str(gate))
            # Primary artifacts must not attach to altered model shapes/rate lists.
            if mode=='ordered':
                gate=a.evidence/f'fallback-{name}.json'
                run([sys.executable,'tools/reaper/compare.py',str(a.base/'turbotuv_bench'),
                     str(a.primary)+':--reaper','--config',config,'--columns','48','--seed','934897','--json',gate])
                report=json.loads(gate.read_text())
                assert report['passed'] and not int(report['candidate']['fields']['generated_calls'])
        records.append(record)
    (a.evidence/'artifacts.json').write_text(json.dumps(records,indent=2)+'\n')
    print('Held-out ordered, roundoff and incompatible-primary fallback gates passed.')


if __name__=='__main__':
    main()

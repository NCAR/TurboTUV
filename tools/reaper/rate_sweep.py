#!/usr/bin/env python3
"""R2: build bounded C11 variants, gate full outputs, then time serially.

Screening uses varied columns and the full scientific output comparator. It
is not the final ten-round performance claim. Every measured candidate has
its own gate report; rejected candidates remain in the experiment ledger.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import time
from artifact import build


def run(cmd):
    return subprocess.check_output(list(map(str, cmd)), text=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--family', required=True)
    p.add_argument('--cc', required=True)
    p.add_argument('--sysroot', required=True)
    p.add_argument('--base', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--evidence', type=Path, required=True)
    p.add_argument('--variants', nargs='+')
    p.add_argument('--rounds', type=int, default=3)
    p.add_argument('--measure-only', action='store_true')
    a = p.parse_args()
    variants = [(f'ordered-{b}',b,1,0,1,False) for b in [32,64,128,256]]
    variants += [(f'acc-{n}',64,n,0,1,False) for n in [2,4,8]]
    variants += [(f'interleave-{n}',64,4,0,n,False) for n in [2,4,8]]
    variants += [('fma',64,4,0,1,True)]
    variants += [(f'tile-{n}',64,4,n,1,False) for n in [8,16,32]]
    variants += [('reuse-products',64,4,0,4,False)]
    variants += [('small-acc',32,8,0,1,False), ('small-interleave',32,4,0,4,False),
                 ('large-interleave',128,4,0,4,False), ('interleave-fma',64,4,0,4,True)]
    if a.variants:
        variants = [v for v in variants if v[0] in a.variants]
    a.evidence.mkdir(parents=True, exist_ok=True)
    a.work.mkdir(parents=True, exist_ok=True)
    configs = ['tuv_5_4','ts1_tsmlt']
    plans = []
    for config in configs:
        path = a.work / f'{config}-plan.json'
        digest = run([a.base/'turbotuv_reaper_export',f'examples/{config}.json',path]).strip()
        assert digest == hashlib.sha256(path.read_bytes()).hexdigest()
        check = a.work / 'repeat-plan.json'
        assert run([a.base/'turbotuv_reaper_export',f'examples/{config}.json',check]).strip() == digest
        assert check.read_bytes() == path.read_bytes()
        plans.append(path)
    flags = ['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-isysroot',a.sysroot]
    report = {'phase':'R2', 'family':a.family, 'variants':[], 'samples':[], 'summary':[]}
    commands = [('interpreted',a.base/'turbotuv_bench')]
    if a.measure_only:
        report = json.loads((a.evidence/f'sweep-{a.family}.json').read_text())
        report['samples'], report['summary'] = [], []
        commands += [(r['name'],a.work/r['name']/'turbotuv_bench') for r in report['variants'] if r['accepted']]
        variants = []
    for name,budget,acc,tile,interleave,fma in variants:
        directory = a.work/name
        record = build(plans,directory,a.cc,flags,budget,acc,tile,interleave,fma,name=='reuse-products')
        record.update(name=name,budget=budget,accumulators=acc,tile=tile,interleave=interleave,fma=fma)
        lib = directory/'libturbotuv.a'
        shutil.copy2(a.base/'libturbotuv.a',lib)
        run(['ar','d',lib,'reaper_dispatch.c.o'])
        run(['ar','r',lib,*record['objects']])
        exe = directory/'turbotuv_bench'
        cmd = [a.cc,*flags,str(a.base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o'),str(lib),'-lm','-o',str(exe)]
        run(cmd)
        record['commands'].append(cmd)
        record['binary_bytes'] = exe.stat().st_size
        record['binary_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
        record['size'] = run(['size','-m',exe])
        shutil.copy2(a.base/'turbotuv',directory/'turbotuv')
        record['gates'] = []
        accepted = True
        for config in configs:
            gate = a.evidence/f'{a.family}-{name}-{config}.json'
            cmd = [sys.executable,'tools/reaper/compare.py',str(a.base/'turbotuv_bench') + ':--reaper',
                   str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',str(gate)]
            result = subprocess.run(cmd, capture_output=True,text=True)
            record['gates'].append({'path':str(gate),'exit_code':result.returncode,'output':result.stdout + result.stderr})
            if result.returncode:
                accepted = False
            else:
                g = json.loads(gate.read_text())
                # Every primary model has generated photolysis; fallbacks must not fake a speedup.
                if int(g['candidate']['fields']['generated_calls']) < 48:
                    raise ValueError('generated kernels did not execute')
        record['accepted'] = accepted
        report['variants'].append(record)
        if accepted:
            commands.append((name,exe))
        print(f'{a.family}: {name}: {"accepted" if accepted else "rejected"}, compile={record["compile_seconds"]:.2f}s',flush=True)
        (a.evidence/f'sweep-{a.family}.json').write_text(json.dumps(report,indent=2)+'\n')
    for round_ in range(a.rounds):
        for config in configs:
            order = commands[round_%len(commands):] + commands[:round_%len(commands)]
            for name,exe in order:
                text = run([exe,f'examples/{config}.json','--reaper','--columns','300','--repeat','3'])
                fields = dict(re.findall(r'(\w+)=(\S+)',text))
                report['samples'].append({**fields,'round':round_,'config':config,'variant':name})
            print(f'{a.family}: timed round {round_+1}, {config}',flush=True)
            (a.evidence/f'sweep-{a.family}.json').write_text(json.dumps(report,indent=2)+'\n')
    for config in configs:
        for name,_ in commands:
            values = [float(s['per_column_us']) for s in report['samples'] if s['config']==config and s['variant']==name]
            report['summary'].append({'config':config,'variant':name,'median_us':statistics.median(values),'range_us':[min(values),max(values)]})
    (a.evidence/f'sweep-{a.family}.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__ == '__main__':
    main()

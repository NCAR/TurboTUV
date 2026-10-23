import argparse,gzip,hashlib,json,re,shutil,statistics,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('family');a=p.parse_args();f=a.family
cc='/opt/homebrew/opt/llvm/bin/clang' if f=='clang' else '/opt/homebrew/bin/gcc-15'
base=Path(f'build/reaper/r4-selected-{f}')
root=Path(f'build/reaper/math-lanes-{f}');root.mkdir(exist_ok=True)
evidence=Path('docs/evidence/reaper/continuation/math-lanes');evidence.mkdir(parents=True,exist_ok=True)
prototype=Path('build/reaper/continuation-prototype')
flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk']
variants=[(f'w{w}-i{i}',[f'-DTT_REAPER_MATH_WIDTH={w}',f'-DTT_REAPER_MATH_INTERLEAVE={i}']) for w,i in [(2,1),(2,2),(2,4),(4,1),(4,2),(8,1),(1,4)]] if f=='clang' else [(f'unroll-{i}',[f'-DTT_REAPER_MATH_UNROLL={i}']) for i in [2,4,8]]
def run(cmd):return subprocess.check_output(list(map(str,cmd)),text=True)
report={'family':f,'variants':[],'samples':[]}
for name in ['spectrum_plan.c','reaper_math.h']:
 (evidence/f'{name}.gz').write_bytes(gzip.compress((prototype/name).read_bytes(),mtime=0))
commands=[('r4',base/'turbotuv_bench')]
for name,macros in variants:
 directory=root/name;directory.mkdir(exist_ok=True)
 lib=directory/'libturbotuv.a';shutil.copy2(base/'libturbotuv.a',lib)
 obj=directory/'spectrum_plan.c.o';exe=directory/'turbotuv_bench'
 cmd=[cc,*flags,*macros,'-c',prototype/'spectrum_plan.c','-o',obj]
 run(cmd);run(['ar','r',lib,obj]);run([cc,*flags,base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
 shutil.copy2(base/'turbotuv',directory/'turbotuv')
 test=directory/'test_reaper';run([cc,*flags,base/'tests/CMakeFiles/test_reaper.dir/test_reaper.c.o',base/'tests/libtest_fixtures.a',lib,'-lm','-o',test])
 item={'variant':name,'command':list(map(str,cmd)),'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'catalogue':run([test]),'gates':[]}
 for config in ['tuv_5_4','ts1_tsmlt']:
  gate=evidence/f'{f}-{name}-{config}.json'
  run([sys.executable,'tools/reaper/compare.py',str(base/'turbotuv_bench')+':--reaper',str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',gate])
  j=json.loads(gate.read_text());assert j['passed'] and j['reference']['output_sha256']==j['candidate']['output_sha256'];item['gates'].append(str(gate))
 commands.append((name,exe));report['variants'].append(item);print(f,name,'gated',flush=True)
for round_ in range(3):
 for config in ['tuv_5_4','ts1_tsmlt']:
  for name,exe in commands[round_%len(commands):]+commands[:round_%len(commands)]:
   fields=dict(re.findall(r'(\w+)=(\S+)',run([exe,f'examples/{config}.json','--reaper','--columns','300','--repeat','3'])))
   report['samples'].append({**fields,'round':round_,'config':config,'variant':name})
 print(f,round_,'timed',flush=True)
 (evidence/f'sweep-{f}.json').write_text(json.dumps(report,indent=2)+'\n')
report['summary']=[{'config':config,'variant':name,'median_us':statistics.median(float(s['per_column_us']) for s in report['samples'] if s['variant']==name and s['config']==config)} for config in ['tuv_5_4','ts1_tsmlt'] for name,_ in commands]
(evidence/f'sweep-{f}.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['summary'],flush=True)

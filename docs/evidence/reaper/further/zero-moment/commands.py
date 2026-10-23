import argparse,gzip,hashlib,json,re,shutil,statistics,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('family');a=p.parse_args();f=a.family
cc='/opt/homebrew/opt/llvm/bin/clang' if f=='clang' else '/opt/homebrew/bin/gcc-15'
base=Path(f'build/reaper/r3b-selected-{f}');main=Path(f'build/reaper/main-{f}/turbotuv_bench')
root=Path(f'build/reaper/zero-moment-{f}');root.mkdir(exist_ok=True)
evidence=Path('docs/evidence/reaper/further/zero-moment');evidence.mkdir(parents=True,exist_ok=True)
source=Path('build/reaper/zero-moment-prototype/radiation_reaper.c')
flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk']
def run(cmd):return subprocess.check_output(list(map(str,cmd)),text=True)
report={'family':f,'variants':[],'samples':[]}
(evidence/'radiation_reaper.c.gz').write_bytes(gzip.compress(source.read_bytes(),mtime=0))
commands=[('selected',base/'turbotuv_bench')]
for mode in [1,2,3]:
 name=f'mode-{mode}';directory=root/name;directory.mkdir(exist_ok=True)
 lib=directory/'libturbotuv.a';shutil.copy2(base/'libturbotuv.a',lib)
 obj=directory/'radiation_reaper.c.o';exe=directory/'turbotuv_bench'
 cmd=[cc,*flags,f'-DTT_ZERO_MOMENT={mode}','-c',source,'-o',obj]
 run(cmd);run(['ar','r',lib,obj]);run([cc,*flags,base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
 shutil.copy2(base/'turbotuv',directory/'turbotuv')
 test=directory/'test_reaper';run([cc,*flags,base/'tests/CMakeFiles/test_reaper.dir/test_reaper.c.o',base/'tests/libtest_fixtures.a',lib,'-lm','-o',test])
 result=subprocess.run([str(test)],capture_output=True,text=True)
 item={'variant':name,'command':list(map(str,cmd)),'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'catalogue':result.stdout+result.stderr,'catalogue_passed':result.returncode==0,'gates':[],'passed':result.returncode==0}
 for config in ['tuv_5_4','ts1_tsmlt']:
  gate=evidence/f'{f}-{name}-{config}.json'
  result=subprocess.run([sys.executable,'tools/reaper/compare.py',str(main),str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',str(gate)],capture_output=True,text=True)
  item['gates'].append(str(gate));item['passed'] &= result.returncode==0
  if result.returncode: item.setdefault('errors',[]).append(result.stdout+result.stderr)
 if item['passed']:commands.append((name,exe))
 report['variants'].append(item);print(f,name,'pass',item['passed'],flush=True)
 (evidence/f'sweep-{f}.json').write_text(json.dumps(report,indent=2)+'\n')
if len(commands)==1: sys.exit(0)
for round_ in range(3):
 for config in ['tuv_5_4','ts1_tsmlt']:
  for name,exe in commands[round_%len(commands):]+commands[:round_%len(commands)]:
   fields=dict(re.findall(r'(\w+)=(\S+)',run([exe,f'examples/{config}.json','--reaper','--columns','300','--repeat','3'])))
   report['samples'].append({**fields,'round':round_,'config':config,'variant':name})
report['summary']=[{'config':config,'variant':name,'median_us':statistics.median(float(s['per_column_us']) for s in report['samples'] if s['variant']==name and s['config']==config)} for config in ['tuv_5_4','ts1_tsmlt'] for name,_ in commands]
(evidence/f'sweep-{f}.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['summary'],flush=True)

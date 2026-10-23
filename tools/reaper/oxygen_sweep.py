#!/usr/bin/env python3
"""Gate and screen exact batched O2 recurrence variants."""
import argparse,gzip,hashlib,json,re,shutil,statistics,subprocess,sys,time
from pathlib import Path

def run(args):return subprocess.check_output(list(map(str,args)),text=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['family','cc','sysroot']:p.add_argument('--'+k,required=True)
 for k in ['base','work','evidence']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();a.work.mkdir(parents=True,exist_ok=True);a.evidence.mkdir(parents=True,exist_ok=True)
 source=a.work/'oxygen.c';shutil.copy2('src/oxygen.c',source)
 (a.evidence/f'source-{a.family}.c.gz').write_bytes(gzip.compress(source.read_bytes(),mtime=0))
 flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot',a.sysroot]
 report={'phase':'R3-oxygen','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'variants':[],'samples':[]}
 commands=[('scalar',a.base/'turbotuv_bench')]
 for lanes in [8,16,32,64,128]:
  name=f'lanes-{lanes}';directory=a.work/name;directory.mkdir(exist_ok=True)
  lib=directory/'libturbotuv.a';shutil.copy2(a.base/'libturbotuv.a',lib)
  obj=directory/'oxygen.c.o';exe=directory/'turbotuv_bench';test=directory/'test_reaper'
  cmd=[a.cc,*flags,f'-DTT_REAPER_O2_LANES={lanes}','-c',source,'-o',obj]
  begin=time.monotonic();run(cmd);run(['ar','r',lib,obj])
  run([a.cc,*flags,a.base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
  shutil.copy2(a.base/'turbotuv',directory/'turbotuv')
  run([a.cc,*flags,a.base/'tests/CMakeFiles/test_reaper.dir/test_reaper.c.o',a.base/'tests/libtest_fixtures.a',lib,'-lm','-o',test])
  record={'name':name,'command':list(map(str,cmd)),'build_seconds':time.monotonic()-begin,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'catalogue':run([test]),'gates':[]}
  for config in ['tuv_5_4','ts1_tsmlt']:
   gate=a.evidence/f'{a.family}-{name}-{config}.json'
   run([sys.executable,'tools/reaper/compare.py',str(a.base/'turbotuv_bench')+':--reaper',str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',gate])
   g=json.loads(gate.read_text());assert g['passed'] and g['reference']['output_sha256']==g['candidate']['output_sha256']
   record['gates'].append(str(gate))
  commands.append((name,exe));report['variants'].append(record)
  print(a.family,'built/gated',name,flush=True)
 import fcntl
 lock=open('build/reaper/timing.lock','w');fcntl.flock(lock,fcntl.LOCK_EX)
 for round_ in range(2):
  for config in ['tuv_5_4','ts1_tsmlt']:
   for name,exe in commands[round_%len(commands):]+commands[:round_%len(commands)]:
    s=dict(re.findall(r'(\w+)=(\S+)',run([exe,f'examples/{config}.json','--reaper','--columns','300','--repeat','3'])))
    assert int(s['generated_calls'])==1800 and not int(s['generic_calls'])
    report['samples'].append({**s,'round':round_,'config':config,'variant':name})
   print(a.family,'round',round_+1,config,flush=True)
  (a.evidence/f'sweep-{a.family}.json').write_text(json.dumps(report,indent=2)+'\n')
 report['summary']=[]
 for config in ['tuv_5_4','ts1_tsmlt']:
  for name,exe in commands:
   values=[float(s['per_column_us']) for s in report['samples'] if s['config']==config and s['variant']==name]
   report['summary'].append({'config':config,'variant':name,'median_us':statistics.median(values),'range_us':[min(values),max(values)]})
 (a.evidence/f'sweep-{a.family}.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()

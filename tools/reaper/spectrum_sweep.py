#!/usr/bin/env python3
"""R3 exact spectral reuse experiments; replace one private archive member."""
import argparse,json,re,shutil,statistics,subprocess,sys,time,hashlib
from pathlib import Path

def run(args):return subprocess.check_output(list(map(str,args)),text=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--family',required=True);p.add_argument('--cc',required=True)
 p.add_argument('--base',type=Path,required=True);p.add_argument('--sysroot',required=True)
 p.add_argument('--work',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True)
 p.add_argument('--variants',choices=['cache','tiles','batch'],default='cache')
 p.add_argument('--rounds',type=int,default=3)
 a=p.parse_args();a.work.mkdir(parents=True,exist_ok=True);a.evidence.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot',a.sysroot]
 import gzip
 source=a.work/'spectrum_plan.c';shutil.copy2('src/spectrum_plan.c',source)
 shutil.copy2('src/reaper_math.h',a.work/'reaper_math.h')
 (a.evidence/f'source-{a.family}.c.gz').write_bytes(gzip.compress(source.read_bytes(),mtime=0))
 report={'phase':'R3-spectra','source_sha256':hashlib.sha256(Path('src/spectrum_plan.c').read_bytes()).hexdigest(),'variants':[],'samples':[]}
 commands=[('tile-1' if a.variants!='cache' else 'R2',a.base/'turbotuv_bench')]
 variants=[('fixed',1,0,1),('direct-expensive',0,1,1),('direct-all',0,2,1),('both-expensive',1,1,1),('both-all',1,2,1)]
 if a.variants!='cache':variants=[(f'tile-{n}',1,1,n) for n in [2,4,8,16,32]]
 if a.variants=='batch':variants=[(f'batch-{n}',1,1,n) for n in [2,4,8,16,32]]
 for name,fixed,direct,tile in variants:
  directory=a.work/name;directory.mkdir(parents=True,exist_ok=True)
  lib=directory/'libturbotuv.a';shutil.copy2(a.base/'libturbotuv.a',lib)
  obj=directory/'spectrum_plan.c.o';exe=directory/'turbotuv_bench'
  cmd=[a.cc,*flags,f'-DTT_REAPER_PARAMETER_BATCH={int(a.variants=="batch")}',f'-DTT_REAPER_SPECTRUM_EXP={int(a.variants!="cache")}',f'-DTT_REAPER_TABLES={int(a.variants!="cache")}',f'-DTT_REAPER_PARAMETER_TILE={tile}',f'-DTT_REAPER_FIXED_CACHE={fixed}',f'-DTT_REAPER_DIRECT_CACHE={direct}','-c',source,'-o',obj]
  begin=time.monotonic();run(cmd);run(['ar','r',lib,obj])
  run([a.cc,*flags,a.base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
  shutil.copy2(a.base/'turbotuv',directory/'turbotuv')
  record={'name':name,'command':list(map(str,cmd)),'build_seconds':time.monotonic()-begin,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'gates':[]}
  if a.variants!='cache':
   test=directory/'test_reaper'
   run([a.cc,*flags,a.base/'tests/CMakeFiles/test_reaper.dir/test_reaper.c.o',a.base/'tests/libtest_fixtures.a',lib,'-lm','-o',test])
   record['catalogue']=run([test])
  for config in ['tuv_5_4','ts1_tsmlt']:
   gate=a.evidence/f'{a.family}-{name}-{config}.json'
   run([sys.executable,'tools/reaper/compare.py',str(a.base/'turbotuv_bench')+':--reaper',str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',gate])
   g=json.loads(gate.read_text());assert g['passed']
   assert g['reference']['output_sha256']==g['candidate']['output_sha256']
   record['gates'].append(str(gate))
  commands.append((name,exe));report['variants'].append(record)
  print(a.family,'built/gated',name,flush=True)
 import fcntl
 lock=open('build/reaper/timing.lock','w')
 fcntl.flock(lock,fcntl.LOCK_EX)
 for round_ in range(a.rounds):
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

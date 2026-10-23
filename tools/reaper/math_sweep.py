#!/usr/bin/env python3
"""R3 separately gate bounded-math uses before timing accepted variants."""
import argparse,gzip,hashlib,json,re,shutil,statistics,subprocess,sys,time
from pathlib import Path

def run(args):return subprocess.check_output(list(map(str,args)),text=True,stderr=subprocess.STDOUT)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['family','cc','sysroot']:p.add_argument('--'+k,required=True)
 for k in ['base','control','work','evidence']:p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--rounds',type=int,default=2)
 p.add_argument('--variants',choices=['math','tables'],default='math')
 a=p.parse_args();a.work.mkdir(parents=True,exist_ok=True);a.evidence.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot',a.sysroot]
 report={'phase':'R3-math','source_sha256':{},'variants':[],'samples':[]}
 for file in ['spectrum_plan.c','radiation_reaper.c','reaper_math.h']:
  data=Path('src',file).read_bytes();(a.work/file).write_bytes(data)
  report['source_sha256'][file]=hashlib.sha256(data).hexdigest()
  (a.evidence/f'{a.family}-{file}.gz').write_bytes(gzip.compress(data,mtime=0))
 commands=[('cache-only',a.control)]
 variants=[('vector-guards',0,0,0),('spectra',1,0,0),('slant',0,1,0),('coefficients',0,2,0),('transfer',0,3,0),('all',1,3,0)]
 if a.variants=='tables':variants=[('guards',0,0,0),('tables',0,0,1),('math',1,0,0),('combined',1,0,1)]
 for name,spec,transfer,tables in variants:
  directory=a.work/name;directory.mkdir(exist_ok=True)
  lib=directory/'libturbotuv.a';shutil.copy2(a.base/'libturbotuv.a',lib)
  record={'name':name,'commands':[],'gates':[]}
  begin=time.monotonic()
  for file in ['spectrum_plan.c','radiation_reaper.c']:
   obj=directory/(file+'.o')
   cmd=[a.cc,*flags,'-DTT_REAPER_BLOCK=16','-DTT_REAPER_MIX=3','-DTT_REAPER_ALIAS=1',
        f'-DTT_REAPER_SPECTRUM_EXP={spec}',f'-DTT_REAPER_TRANSFER_EXP={transfer}',f'-DTT_REAPER_TABLES={tables}','-c',a.work/file,'-o',obj]
   run(cmd);run(['ar','r',lib,obj]);record['commands'].append(list(map(str,cmd)))
  exe=directory/'turbotuv_bench';test=directory/'test_reaper'
  run([a.cc,*flags,a.base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
  shutil.copy2(a.base/'turbotuv',directory/'turbotuv')
  run([a.cc,*flags,a.base/'tests/CMakeFiles/test_reaper.dir/test_reaper.c.o',a.base/'tests/libtest_fixtures.a',lib,'-lm','-o',test])
  record.update(build_seconds=time.monotonic()-begin,binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
  try:record['catalogue']=run([test]);record['catalogue_passed']=True
  except subprocess.CalledProcessError as e:record['catalogue']=e.output;record['catalogue_passed']=False
  passed=record['catalogue_passed']
  for config in ['tuv_5_4','ts1_tsmlt']:
   gate=a.evidence/f'{a.family}-{name}-{config}.json'
   cmd=[sys.executable,'tools/reaper/compare.py',str(a.control)+':--reaper',str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',gate]
   try:run(cmd)
   except subprocess.CalledProcessError as e:record.setdefault('errors',[]).append(e.output)
   g=json.loads(gate.read_text());passed &=g['passed'];record['gates'].append(str(gate))
  record['passed']=passed;report['variants'].append(record)
  if passed:commands.append((name,exe))
  print(a.family,name,'passed' if passed else 'REJECTED',flush=True)
  (a.evidence/f'sweep-{a.family}.json').write_text(json.dumps(report,indent=2)+'\n')
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

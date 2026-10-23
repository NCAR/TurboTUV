#!/usr/bin/env python3
"""R3 regular C transfer block sweep, with the full catalogue and output gates."""
import argparse,json,re,shutil,statistics,subprocess,sys,time,hashlib
from pathlib import Path

def run(args):return subprocess.check_output(list(map(str,args)),text=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--family',required=True);p.add_argument('--cc',required=True)
 p.add_argument('--base',type=Path,required=True);p.add_argument('--control',type=Path,required=True)
 p.add_argument('--sysroot',required=True);p.add_argument('--work',type=Path,required=True)
 p.add_argument('--evidence',type=Path,required=True)
 p.add_argument('--variants',choices=['blocks','aliases','guards','slants','coefficients','roots','symmetric'],default='blocks')
 p.add_argument('--source',type=Path,default=Path('src/radiation_reaper.c'))
 p.add_argument('--rounds',type=int,default=3)
 a=p.parse_args();a.work.mkdir(parents=True,exist_ok=True);a.evidence.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot',a.sysroot]
 source=a.work/'radiation_reaper.c';shutil.copy2(a.source,source)
 import gzip
 (a.evidence/f'source-{a.family}.c.gz').write_bytes(gzip.compress(source.read_bytes(),mtime=0))
 report={'phase':'R3-transfer','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'variants':[],'samples':[]}
 commands=[('cache-only',a.control)]
 variants=[(f'block-{b}',b,1,0) for b in [2,4,8,16,32]]
 if a.variants=='aliases':
  variants=[('old-mix',16,0,0),('alias-8',8,0,1),('alias-16',16,0,1),('alias-32',32,0,1),
            ('contiguous-16',16,1,1),('homogeneous-8',8,2,1),('homogeneous-16',16,2,1)]
 if a.variants=='guards':
  variants=[('bitwise-8',8,3,1),('bitwise-16',16,3,1),('bitwise-32',32,3,1),
            ('classified-8',8,4,1),('classified-16',16,4,1)]
 if a.variants=='slants':
  variants=[('rows1-block16',16,3,1),('rows2-block16',16,3,1),('rows4-block16',16,3,1),
            ('rows2-block8',8,3,1),('rows4-block8',8,3,1)]
 if a.variants=='coefficients':
  variants=[('roles',16,3,1),('vector',16,3,1),('zeros',16,3,1),('both',16,3,1),('both-8',8,3,1)]
 if a.variants=='roots':
  variants=[('root-control',16,3,1),('root-8',8,3,1),('root-16',16,3,1),('root-32',32,3,1)]
 if a.variants=='symmetric':
  variants=[('symmetric-control',16,3,1),('symmetric-8',8,3,1),('symmetric-16',16,3,1),('symmetric-32',32,3,1)]
 for name,block,mix,alias in variants:
  directory=a.work/name;directory.mkdir(parents=True,exist_ok=True)
  lib=directory/'libturbotuv.a';shutil.copy2(a.base/'libturbotuv.a',lib)
  obj=directory/'radiation_reaper.c.o';exe=directory/'turbotuv_bench'
  rows=int(name[4]) if a.variants=='slants' else 1
  vector=int(a.variants=='coefficients' and name in ['vector','both','both-8'])
  if a.variants in ['roots','symmetric']:vector=1
  root=int(a.variants=='roots' and name!='root-control')
  symmetric=int(a.variants=='symmetric' and name!='symmetric-control')
  zeros=3 if a.variants=='coefficients' and name in ['zeros','both','both-8'] else 0
  cmd=[a.cc,*flags,f'-DTT_REAPER_SYMMETRIC={symmetric}',f'-DTT_REAPER_COEFF_SQRT={root}',f'-DTT_REAPER_COEFF_VECTOR={vector}',f'-DTT_REAPER_ZERO_EXP={zeros}',f'-DTT_REAPER_SLANT_ROWS={rows}',f'-DTT_REAPER_BLOCK={block}',f'-DTT_REAPER_MIX={mix}',f'-DTT_REAPER_ALIAS={alias}','-c',source,'-o',obj]
  begin=time.monotonic();run(cmd);run(['ar','r',lib,obj])
  run([a.cc,*flags,a.base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
  shutil.copy2(a.base/'turbotuv',directory/'turbotuv')
  test=directory/'test_reaper'
  run([a.cc,*flags,a.base/'tests/CMakeFiles/test_reaper.dir/test_reaper.c.o',a.base/'tests/libtest_fixtures.a',lib,'-lm','-o',test])
  record={'name':name,'command':list(map(str,cmd)),'build_seconds':time.monotonic()-begin,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'gates':[],'catalogue':run([test])}
  for config in ['tuv_5_4','ts1_tsmlt']:
   gate=a.evidence/f'{a.family}-{name}-{config}.json'
   run([sys.executable,'tools/reaper/compare.py',str(a.control)+':--reaper',str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',gate])
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

import argparse,gzip,hashlib,json,math,re,shutil,statistics,struct,subprocess,sys
from decimal import Decimal,localcontext
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('family');a=p.parse_args();f=a.family
cc='/opt/homebrew/opt/llvm/bin/clang' if f=='clang' else '/opt/homebrew/bin/gcc-15'
base=Path(f'build/reaper/r4-selected-{f}');main=Path(f'build/reaper/main-{f}/turbotuv_bench')
root=Path(f'build/reaper/small-arguments-{f}');root.mkdir(exist_ok=True)
evidence=Path('docs/evidence/reaper/continuation/small-arguments');evidence.mkdir(parents=True,exist_ok=True)
flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk']
def run(cmd):return subprocess.check_output(list(map(str,cmd)),text=True)
report={'family':f,'status':'small-exp screening; final independent proof/oracle pending; pow10 remains unchanged','variants':[],'samples':[]}
commands=[('r4',base/'turbotuv_bench')]
for radius,degree in [(0.0625,8),(0.1,9),(0.125,10)]:
 name=f'radius-{radius}-degree-{degree}';directory=root/name;directory.mkdir(exist_ok=True)
 hdr=Path('src/reaper_math.h').read_text()
 marker='    int valid = 1;\n    for (size_t i = 0; i < count; ++i)\n        valid &= (arg[i] >= -limit) & (arg[i] <= limit);'
 assert marker in hdr
 guard=f'    int valid = 1, small = !base10;\n    for (size_t i = 0; i < count; ++i) {{\n        valid &= (arg[i] >= -limit) & (arg[i] <= limit);\n        small &= (arg[i] >= -{radius.hex()}) & (arg[i] <= {radius.hex()});\n    }}'
 poly='    if (small) {\n        for (size_t i = 0; i < count; ++i) {\n            double x = arg[i];\n'
 poly+=f'            double p = {(1/math.factorial(degree)).hex()};\n'
 for j in range(degree-1,-1,-1): poly+=f'            p = fma(p, x, {(1/math.factorial(j)).hex()});\n'
 poly+='            out[i] = p;\n        }\n        return;\n    }\n'
 hdr=hdr.replace(marker,guard+'\n'+poly)
 with localcontext() as ctx:
  ctx.prec=100;epsilon=Decimal(2)**-52;u=epsilon/2;r=Decimal(radius)
  bound=degree*u/(1-degree*u)*(2*r).exp()/epsilon+u*(2*r).exp()/epsilon+(2*r).exp()*r**(degree+1)/Decimal(math.factorial(degree+1))/epsilon
  assert bound<8
  limits={'radius':radius,'degree':degree,'bound_eps':str(bound),'note':'Horner/Taylor uniform budget on small arguments; independent oracle and integration proof required before retention.'}
 (directory/'reaper_math.h').write_text(hdr);shutil.copy2('src/spectrum_plan.c',directory/'spectrum_plan.c')
 (evidence/f'{name}.h.gz').write_bytes(gzip.compress(hdr.encode(),mtime=0))
 lib=directory/'libturbotuv.a';shutil.copy2(base/'libturbotuv.a',lib)
 obj=directory/'spectrum_plan.c.o';exe=directory/'turbotuv_bench'
 cmd=[cc,*flags,'-c',directory/'spectrum_plan.c','-o',obj]
 run(cmd);run(['ar','r',lib,obj]);run([cc,*flags,base/'CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o',lib,'-lm','-o',exe])
 shutil.copy2(base/'turbotuv',directory/'turbotuv')
 test=directory/'test_reaper';run([cc,*flags,base/'tests/CMakeFiles/test_reaper.dir/test_reaper.c.o',base/'tests/libtest_fixtures.a',lib,'-lm','-o',test])
 result=subprocess.run([str(test)],capture_output=True,text=True)
 item={'variant':name,'command':list(map(str,cmd)),'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'math_header_sha256':hashlib.sha256(hdr.encode()).hexdigest(),'limits':limits,'catalogue':result.stdout+result.stderr,'catalogue_passed':result.returncode==0,'gates':[],'passed':result.returncode==0}
 for config in ['tuv_5_4','ts1_tsmlt']:
  gate=evidence/f'{f}-{name}-{config}.json'
  result=subprocess.run([sys.executable,'tools/reaper/compare.py',str(main),str(exe)+':--reaper','--config',f'examples/{config}.json','--columns','48','--json',str(gate)],capture_output=True,text=True)
  item['gates'].append(str(gate));item['passed'] &= result.returncode==0
  if result.returncode:item.setdefault('errors',[]).append(result.stdout+result.stderr)
 if item['passed']:commands.append((name,exe))
 report['variants'].append(item);print(f,name,'pass',item['passed'],flush=True)
 (evidence/f'sweep-{f}.json').write_text(json.dumps(report,indent=2)+'\n')
for round_ in range(3):
 for config in ['tuv_5_4','ts1_tsmlt']:
  for name,exe in commands[round_%len(commands):]+commands[:round_%len(commands)]:
   fields=dict(re.findall(r'(\w+)=(\S+)',run([exe,f'examples/{config}.json','--reaper','--columns','300','--repeat','3'])))
   report['samples'].append({**fields,'round':round_,'config':config,'variant':name})
report['summary']=[{'config':config,'variant':name,'median_us':statistics.median(float(s['per_column_us']) for s in report['samples'] if s['variant']==name and s['config']==config)} for config in ['tuv_5_4','ts1_tsmlt'] for name,_ in commands]
(evidence/f'sweep-{f}.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['summary'],flush=True)

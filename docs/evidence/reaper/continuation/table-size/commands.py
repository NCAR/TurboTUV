import argparse,gzip,hashlib,json,math,re,shutil,statistics,struct,subprocess,sys
from decimal import Decimal,localcontext
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('family');a=p.parse_args();f=a.family
cc='/opt/homebrew/opt/llvm/bin/clang' if f=='clang' else '/opt/homebrew/bin/gcc-15'
base=Path(f'build/reaper/r4-selected-{f}');main=Path(f'build/reaper/main-{f}/turbotuv_bench')
root=Path(f'build/reaper/table-size-{f}');root.mkdir(exist_ok=True)
evidence=Path('docs/evidence/reaper/continuation/table-size');evidence.mkdir(parents=True,exist_ok=True)
flags=['-std=c11','-O3','-DNDEBUG','-fPIC','-ffp-contract=off','-Iinclude','-Isrc','-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk']
def run(cmd):return subprocess.check_output(list(map(str,cmd)),text=True)
report={'family':f,'status':'screening only; exhaustive independent oracle pending for any retained new approximation','variants':[],'samples':[]}
commands=[('r4',base/'turbotuv_bench')]
for bits,degree in [(8,4),(9,4),(10,3),(11,3)]:
 n=1<<bits;name=f'table-{n}-degree-{degree}';directory=root/name;directory.mkdir(exist_ok=True)
 with localcontext() as ctx:
  ctx.prec=100;ln2=Decimal(2).ln();ln10=Decimal(10).ln();h=ln2/n
  maximum_k=math.ceil(600*float(1/h));k_bits=maximum_k.bit_length();high_bits=min(33,52-k_bits)
  hb=struct.unpack('>Q',struct.pack('>d',float(h)))[0];hb &= ~((1<<(53-high_bits))-1)
  high=struct.unpack('>d',struct.pack('>Q',hb))[0];low=float(h-Decimal(high))
  values=[float((ln2*Decimal(i)/n).exp()).hex() for i in range(n)]
  hdr=Path('src/reaper_math.h').read_text()
  start=hdr.index('    static const double powers[64] = {');end=hdr.index('    double limit',start)
  hdr=hdr[:start]+f'    static const double powers[{n}] = {{\n'+''.join('        '+', '.join(values[i:i+4])+',\n' for i in range(0,n,4))+'    };\n'+hdr[end:]
  hdr=hdr.replace('0x1.a934f0979a371p+7',float(ln10/h).hex()).replace('0x1.71547652b82fep+6',float(1/h).hex())
  hdr=hdr.replace('0x1.a39ef35793c76p-39',low.hex()).replace('0x1.62e42fee00000p-7',high.hex())
  start=hdr.index('        double p = 0x1.1111111111111p-7;');end=hdr.index('        uint64_t index',start)
  poly=f'        double p = {(1/math.factorial(degree)).hex()};\n'
  for i in range(degree-1,-1,-1): poly+=f'        p = fma(p, r, {(1/math.factorial(i)).hex()});\n'
  hdr=hdr[:start]+poly+hdr[end:]
  hdr=hdr.replace('k + 65536',f'k + {n*1024}').replace('index & 63',f'index & {n-1}').replace('index >> 6',f'index >> {bits}')
  epsilon=Decimal(2)**-52;u=epsilon/2;r=ln2/(2*n)+Decimal('1e-12')
  bound=Decimal('.126')+degree*u/(1-degree*u)*(2*r).exp()/epsilon+u*(2*r).exp()/epsilon+(2*r).exp()*r**(degree+1)/Decimal(math.factorial(degree+1))/epsilon+1
  assert bound<8
  limits={'table':n,'degree':degree,'maximum_k':maximum_k,'integer_bits':k_bits,'high_significant_bits':high_bits,'high':high.hex(),'low':low.hex(),'preliminary_bound_eps':str(bound),'note':'Conservative Horner/table/remainder budget; final reduction derivation and exhaustive MPFR boundaries required before retention.'}
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

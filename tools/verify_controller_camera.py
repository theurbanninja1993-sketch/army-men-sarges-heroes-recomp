#!/usr/bin/env python3
"""Check Classic/Modern native camera behavior using deterministic first-mission runs."""
import subprocess,os,struct,concurrent.futures,json,math,argparse,tempfile
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--disc',type=Path,required=True)
parser.add_argument('--out',type=Path)
args=parser.parse_args()
args.disc=args.disc.resolve()
out=(args.out or Path(tempfile.mkdtemp(prefix='army-controller-check-'))).resolve()
out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]
os.chdir(root)
press='start@2200,start@2300,start@2400,start@2500,start@2600,b@2800,down@3100,a@3300,a@3500,start@3700,a@3900,a@7800'
cases={'base':('',False,False),'classic':('8400:8420:1:1',False,False),'modern':('8400:8420:1:1',True,False),'leftup':('8400:8420:-1:-1',True,False),'mouseoff':('8400:8420:1:1',True,False),'aimbase':('',False,True),'aim':('8400:8420:1:1',True,True),'cinematic':('4000:4100:1:1',True,False),'clamp':('8200:8420:1:1',True,False),'paused':('8400:8420:1:1',True,False)}
def run(name):
 sample,modern,aim=cases[name];env=os.environ.copy()
 for key in ('ARMY_TEST_MOUSE','ARMY_TEST_STICK','DREAM_EXP_PAD','DREAM_EXP_ROT','DREAM_WATCH_READ','DREAM_WATCH_WRITE'):
  env.pop(key,None)
 env['ARMY_TEST_STICK']=sample;env['DREAM_DUMP_RAM']=str(out/f'{name}.ram')
 if aim:env['DREAM_EXP_PAD']='8200:8500:255:128:128'
 else:env.pop('DREAM_EXP_PAD',None)
 cmd=['./build/game/army-men-sarges-heroes-recomp','--config','game/armymensh.toml','--disc',str(args.disc.resolve()),'--max-frames','8450','--rtc-seed','1000000','--no-audio','--settings',str(out/f'{name}-settings.ini'),'--vmu',str(out/f'{name}-vmu.bin'),'--press',press+(',start@8200' if name=='paused' else ''),'--set','controller_mode='+('modern' if modern else 'classic')]
 if name=='mouseoff':cmd+=['--set','mouse_look=false','--set','mouse_vertical=false','--set','mouse_sensitivity=0.001']
 with open(out/f'{name}.log','w') as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
 log=(out/f'{name}.log').read_text();assert 'untranslated call targets: 0' in log;assert 'unmapped memory accesses: 1' in log
 ram=(out/f'{name}.ram').read_bytes();f=lambda a:struct.unpack_from('<f',ram,a)[0]
 return name,{'yaw':f(0x1a1020),'pitch':f(0x1a101c),'player_yaw':f(0x411820),'pos':[f(0x411810),f(0x411818)]}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool: results=dict(pool.map(run,cases))
(out/'results.json').write_text(json.dumps(results,indent=2));assert (out/'base.ram').read_bytes() == (out/'classic.ram').read_bytes()
assert results['paused'] == results['base'], 'native pause must suppress look and leave menu input stock'
assert results['cinematic'] == results['base'], 'cinematic motion must not leak into gameplay'
assert results['mouseoff'] == results['modern'], 'mouse settings must not change the controller camera'
for name in ('modern','leftup','aim','clamp'):
 r=results[name];reference=results['aimbase' if name=='aim' else 'base']
 assert abs(math.remainder(r['yaw']-r['player_yaw'],360)) < .02,name
 assert r['pos'] == reference['pos'], 'right stick must not move the actor'
 if name in ('modern','aim'):
  assert 55 < math.remainder(r['yaw']-reference['yaw'],360) < 65,name
  assert 25 < math.remainder(r['pitch']-reference['pitch'],360) < 35,name
 if name=='leftup':
  assert -65 < math.remainder(r['yaw']-reference['yaw'],360) < -50,name
  assert -35 < math.remainder(r['pitch']-reference['pitch'],360) < -25,name
 assert -75 <= math.remainder(r['pitch'],360) <= 75,name
assert results['clamp']['pitch'] == 75
print(f"PASS: {len(cases)} controller scenarios; logs and RAM in {out}")

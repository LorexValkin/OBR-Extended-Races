import pathlib, json, hashlib, shutil, datetime, subprocess, sys
W=pathlib.Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
C=W/(sys.argv[1] if len(sys.argv)>1 else 'extended-races-uml-overlay-candidate-20261001-v2/candidate')
G=pathlib.Path('D:/SteamLibrary/steamapps/common/Oblivion Remastered').resolve()
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def target(rel):
 p=(G/rel).resolve()
 if not p.is_relative_to(G): raise RuntimeError('Unsafe target')
 return p
def closed():
 r=subprocess.run(['powershell','-NoProfile','-Command', "(Get-Process OblivionRemastered-Win64-Shipping -ErrorAction SilentlyContinue).Count"],capture_output=True,text=True,check=True)
 if r.stdout.strip()!='0': raise RuntimeError('Game must be closed')
closed()
m=json.loads((C/'candidate-manifest.json').read_text())
for f in m['files']:
 p=C/f['path']
 assert p.stat().st_size==f['bytes'] and digest(p)==f['sha256'],f['path']
B=W/('install-backup-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
B.mkdir()
plugins='OblivionRemastered/Content/Dev/ObvData/Data/Plugins.txt'
paks=G/'OblivionRemastered/Content/Paks'
names={pathlib.Path(f['path']).name for f in m['files'] if '/Paks/' in f['path']}
active={target(f['path']) for f in m['files']}
old=[p.resolve() for p in paks.rglob('*') if p.is_file() and p.name in names and p.resolve() not in active]
affected=list(dict.fromkeys([target(f['path']) for f in m['files']]+old+[target(plugins)]))
records=[]
for p in affected:
 rel=p.relative_to(G).as_posix()
 exists=p.is_file()
 if exists:
  b=B/'files'/rel; b.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(p,b)
  assert digest(b)==digest(p)
 records.append({'path':rel,'existed':exists,'sha256':digest(p) if exists else None})
receipt={'game':str(G),'backup':str(B),'before':records,'removed_duplicates':[str(p.relative_to(G)) for p in old],'files':m['files'],'status':'backed-up'}
receipt_path=B/'receipt.json'
receipt_path.write_text(json.dumps(receipt,indent=2))
closed()
try:
 for p in old: p.unlink()
 for f in m['files']:
  p=target(f['path']); p.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(C/f['path'],p)
 p=target(plugins); raw=p.read_bytes()
 plugin_names=[pathlib.Path(f['path']).name for f in m['files'] if f['path'].lower().endswith('.esp')]
 for name in plugin_names:
  if not any(x.strip().lstrip(b'*').lower()==name.encode('ascii').lower() for x in raw.splitlines()):
   raw=raw.rstrip(b'\r\n')+b'\r\n'+name.encode('ascii')+b'\r\n'
 p.write_bytes(raw)
 for f in m['files']: assert digest(target(f['path']))==f['sha256'],f['path']
 receipt['status']='installed-verified'
 receipt['plugins_sha256']=digest(target(plugins))
except Exception:
 for r in records:
  p=target(r['path'])
  if r['existed']: shutil.copy2(B/'files'/r['path'],p)
  elif p.exists(): p.unlink()
 receipt['status']='rolled-back'
 raise
finally: receipt_path.write_text(json.dumps(receipt,indent=2))
print(json.dumps({'status':receipt['status'],'verified_files':len(m['files']),'removed_duplicates':len(old),'receipt':str(receipt_path)},indent=2))

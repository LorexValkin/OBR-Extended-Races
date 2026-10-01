"""Reopen every saved mesh and compare geometry to authored source sidecars."""
from pathlib import Path
import json,subprocess,sys
W=Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
P=Path('D:/Unblivion Editor/Manifests/ExtendedRacesSkeleton20261001V5/project')
O=W/'skeleton-mesh-prototype/unreal-reopen-v1'
O.mkdir()
rows=[]
for folder in ('rebind-v1','cosmetics-v1'):
 for row in json.loads((W/'skeleton-mesh-prototype'/folder/'build-batch.json').read_text())['meshes']:
  name=row['object'].split('.')[-1]
  spec=O/(name+'.json')
  spec.write_text(json.dumps({'schema':'obr-bow-skeletal-pilot-v1','meshes':[{'object':row['object']}]}))
  rows.append({'object':row['object'],'inspect_spec':str(spec),'inspect_folder':str(O/name),'skin':str(Path(row['spec']).with_suffix('').with_suffix('.skin.json'))})
batch=O/'batch.json';batch.write_text(json.dumps({'schema':'character-source-mesh-batch-v1','meshes':rows},indent=2))
args=['F:/UE_5.3.2-src/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(P/'UnblivionEditor.uproject'),'-run=CharacterSourceMesh','-Mode=inspect','-Batch='+str(batch),'-unattended','-nop4','-nosound','-NullRHI','-NoAssetManagerScan','-AbsLog='+str(O/'inspect.log')]
with (O/'console.log').open('w') as out:r=subprocess.run(args,stdout=out,stderr=subprocess.STDOUT)
if r.returncode:raise RuntimeError('Reopen failed '+str(r.returncode))
sys.path.insert(0,'C:/Users/User/Desktop/Projects/OBR_Alter_Map/tools')
from compare_bow_skeletal_resource import compare_geometry
report=[]
for row in rows:
 observed=json.loads((Path(row['inspect_folder'])/'skeletal-inspect.json').read_text())
 if observed['failures']:raise RuntimeError(row['object'])
 result=compare_geometry(observed,json.loads(Path(row['skin']).read_text()))
 report.append({'object':row['object'],'comparison':result,'materials':observed['materials'],'physics_bound':observed['physics_bound']})
(O/'comparison.json').write_text(json.dumps(report,indent=2))
print('Reopened and compared',len(report),'meshes')

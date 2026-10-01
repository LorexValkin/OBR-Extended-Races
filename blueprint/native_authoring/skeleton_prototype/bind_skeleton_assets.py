"""Bind only owned meshes; canonical dependencies are read-only directory links."""
from pathlib import Path
import subprocess,json,shutil
W=Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
P=Path('D:/Unblivion Editor/Manifests/ExtendedRacesSkeleton20261001V5/project')
canonical=Path('D:/Unblivion Editor/Editor/Content')
for relative in ['ArtOriginal','Materials']+[str(x.relative_to(canonical)) for x in (canonical/'Art').iterdir() if x.is_dir() and x.name!='Character']:
 dst=P/'Content'/relative
 if not dst.exists():
  dst.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run(['cmd','/c','mklink','/J',str(dst),str(canonical/relative)],check=True,capture_output=True)
loader=P/'Content/Mods/UnblivionModLoader'
if not loader.exists():shutil.copytree(W/'Content/Mods/UnblivionModLoader',loader)
script=P.parent/'bind.py'
script.write_text('''import unreal,json
from pathlib import Path
root=Path('''+repr(str(W/'skeleton-mesh-prototype'))+''')
materials=json.loads((root/'cosmetics-v1/material-bindings.json').read_text())
body=[{'material':'/Game/Art/Creatures/Skeleton/MIC_Skeleton_2.MIC_Skeleton_2'}]
materials['HumanoidBody']=body
materials['HumanoidArms']=body
skeleton=unreal.load_asset('/Game/Art/Character/Humanoid/SKEL_HumanoidSkeleton')
if not isinstance(skeleton,unreal.Skeleton):raise RuntimeError('Humanoid skeleton missing')
report=[]
for label,bindings in materials.items():
 path='/Game/Art/Character/ExtendedRacesSkeleton/SK_Skeleton_'+label
 mesh=unreal.load_asset(path)
 if not isinstance(mesh,unreal.SkeletalMesh):raise RuntimeError(path)
 slots=mesh.get_editor_property('materials')
 if len(slots)!=len(bindings):raise RuntimeError('Slot count '+path)
 for slot,binding in zip(slots,bindings):
  material=unreal.load_asset(binding['material'])
  if not isinstance(material,unreal.MaterialInterface):raise RuntimeError(binding['material'])
  slot.set_editor_property('material_interface',material)
 mesh.set_editor_property('materials',slots)
 # Leader-pose preview maps by bone identity; retain the owned imported skeleton.
 if not unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False):raise RuntimeError('Save '+path)
 report.append({'mesh':path,'skeleton':mesh.get_editor_property('skeleton').get_path_name(),'materials':[x['material'] for x in bindings]})
Path(unreal.Paths.project_dir()).parent.joinpath('binding-receipt.json').write_text(json.dumps(report,indent=2))
''')
args=['F:/UE_5.3.2-src/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(P/'UnblivionEditor.uproject'),'-run=pythonscript','-script='+str(script),'-unattended','-nop4','-nosound','-NullRHI','-NoAssetManagerScan','-AbsLog='+str(P.parent/'bind.log')]
with (P.parent/'bind-console.log').open('w') as out:r=subprocess.run(args,stdout=out,stderr=subprocess.STDOUT)
print('Binding exit',r.returncode)
raise SystemExit(r.returncode)

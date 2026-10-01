"""Create an isolated project and import the authored Skeleton mesh prototypes."""
from pathlib import Path
import os,shutil,subprocess,json,hashlib
W=Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
P=Path('D:/Unblivion Editor/Manifests/ExtendedRacesSkeleton20261001V5/project')
# This job only writes its private project; retain another task's editor lock.
L=W/'skeleton-mesh-prototype/editor-validation.lock'
tag=f'ExtendedRacesSkeleton {os.getpid()}'
with L.open('x') as f:f.write(tag)
try:
 if P.exists():raise RuntimeError('Fresh isolated project required')
 P.mkdir(parents=True)
 prior=Path('D:/Unblivion Editor/Manifests/ExtendedRaces20261001Mapped/project')
 shutil.copy2(prior/'UnblivionEditor.uproject',P/'UnblivionEditor.uproject')
 shutil.copytree(prior/'Config',P/'Config')
 b=P/'Binaries/Win64';b.mkdir(parents=True)
 for source in Path('D:/Unblivion Editor/Editor/Binaries/Win64').iterdir():
  if source.is_file() and source.suffix.lower() in {'.dll','.modules','.target'}:os.link(source,b/source.name)
 (P/'Content').mkdir()
 for relative in ('Art/Character/Humanoid/MorphMeshes/SK_Human_Male_Morphs.uasset','Art/Character/Humanoid/SKEL_HumanoidSkeleton.uasset'):
  dst=P/'Content'/relative;dst.parent.mkdir(parents=True,exist_ok=True)
  original=Path('D:/Unblivion Editor/Editor/Content')/relative
  for sidecar in original.parent.glob(original.stem+'.*'):
   shutil.copy2(sidecar,dst.parent/sidecar.name)
 script=P.parent/'import.py'
 script.write_text('''import unreal,json
from pathlib import Path
root=Path('''+repr(str(W/'skeleton-mesh-prototype/rebind-v1'))+''')
report={'assets':[],'errors':[]}
try:
 for label in ('HumanoidBody','HumanoidArms'):
  name='SK_Skeleton_'+label
  dest='/Game/CharacterReference/SystemImport/ExtendedRacesSkeleton/'+name+'_Source'
  task=unreal.AssetImportTask()
  pipeline=unreal.InterchangeGenericAssetsPipeline()
  overrides=unreal.InterchangePipelineStackOverride()
  overrides.add_pipeline(pipeline)
  task.set_editor_property('options',overrides)
  source=root/(name+'_Source.glb')
  if not source.is_file():raise RuntimeError('Missing mesh source: '+str(source))
  for k,v in dict(filename=str(source),destination_path=dest.rsplit('/',1)[0],destination_name=dest.rsplit('/',1)[1],automated=True,replace_existing=False,save=True).items():task.set_editor_property(k,v)
  unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
  imported=list(task.get_objects())
  mesh=unreal.load_asset(dest)
  if not isinstance(mesh,unreal.SkeletalMesh):
   candidates=[x for x in imported if isinstance(x,unreal.SkeletalMesh)]
   if len(candidates)!=1:raise RuntimeError('Expected one skeletal mesh')
   if not unreal.EditorAssetLibrary.rename_asset(candidates[0].get_path_name(),dest):raise RuntimeError('Rename failed')
   mesh=unreal.load_asset(dest)
  if not unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False):raise RuntimeError('Save failed')
  report['assets'].append({'path':mesh.get_path_name(),'skeleton':mesh.get_editor_property('skeleton').get_path_name()})
except Exception as e:
 report['errors'].append(str(e))
 raise
finally:
 Path(unreal.Paths.project_dir()).parent.joinpath('import-receipt.json').write_text(json.dumps(report,indent=2))
''')
 editor='F:/UE_5.3.2-src/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
 log=P.parent/'import.log'
 args=[editor,str(P/'UnblivionEditor.uproject'),'-run=pythonscript','-script='+str(script),'-unattended','-nop4','-nosound','-nosplash','-NullRHI','-NoLoadStartupPackages','-NoAssetManagerScan','-NoDDCCleanup','-UTF8Output','-AbsLog='+str(log)]
 with (P.parent/'import-console.log').open('w') as out:r=subprocess.run(args,stdout=out,stderr=subprocess.STDOUT)
 print('Import exit',r.returncode,'log',log)
 if r.returncode:raise RuntimeError('Import failed')
 report=json.loads((P.parent/'import-receipt.json').read_text())
 if report['errors']:raise RuntimeError(report['errors'])
 print(json.dumps(report,indent=2))
finally:
 if L.read_text()==tag:L.unlink()

"""Import owned cosmetic source meshes into the already isolated prototype project."""
from pathlib import Path
import subprocess
W=Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
P=Path('D:/Unblivion Editor/Manifests/ExtendedRacesSkeleton20261001V5/project')
script=P.parent/'import-cosmetics.py'
script.write_text('''import unreal,json
from pathlib import Path
root=Path('''+repr(str(W/'skeleton-mesh-prototype/cosmetics-v1'))+''')
report={'assets':[]}
for row in json.loads((root/'build-batch.json').read_text())['meshes']:
 spec=json.loads(Path(row['spec']).read_text())
 dest=spec['source_mesh'].split('.')[0]
 source=root/(dest.rsplit('/',1)[1]+'.glb')
 if not source.is_file():raise RuntimeError(str(source))
 if unreal.EditorAssetLibrary.does_asset_exist(dest):raise RuntimeError('Already exists '+dest)
 task=unreal.AssetImportTask()
 pipeline=unreal.InterchangeGenericAssetsPipeline()
 overrides=unreal.InterchangePipelineStackOverride()
 overrides.add_pipeline(pipeline)
 for k,v in dict(filename=str(source),destination_path=dest.rsplit('/',1)[0],automated=True,save=True,options=overrides).items():task.set_editor_property(k,v)
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 meshes=[x for x in task.get_objects() if isinstance(x,unreal.SkeletalMesh)]
 if len(meshes)!=1:raise RuntimeError('Expected one mesh: '+dest)
 if meshes[0].get_path_name().split('.')[0]!=dest:
  if not unreal.EditorAssetLibrary.rename_asset(meshes[0].get_path_name(),dest):raise RuntimeError('Rename failed')
 mesh=unreal.load_asset(dest)
 if not unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False):raise RuntimeError('Save failed')
 report['assets'].append(mesh.get_path_name())
Path(unreal.Paths.project_dir()).parent.joinpath('cosmetics-import-receipt.json').write_text(json.dumps(report,indent=2))
''')
args=['F:/UE_5.3.2-src/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(P/'UnblivionEditor.uproject'),'-run=pythonscript','-script='+str(script),'-unattended','-nop4','-nosound','-NullRHI','-NoAssetManagerScan','-AbsLog='+str(P.parent/'import-cosmetics.log')]
with (P.parent/'import-cosmetics-console.log').open('w') as out:r=subprocess.run(args,stdout=out,stderr=subprocess.STDOUT)
print('Cosmetics import exit',r.returncode)
raise SystemExit(r.returncode)

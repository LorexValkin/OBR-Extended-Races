"""Author UML metadata and cook a separate, removable Skeleton follower prototype."""
from pathlib import Path
import subprocess,json,struct
W=Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
P=Path('D:/Unblivion Editor/Manifests/ExtendedRacesSkeleton20261001V5/project')
O=W/'skeleton-preview-candidate-20261001-v1'
marker=W/'skeleton-mesh-prototype/ExtendedRacesSkeletonPrototype.esp'
def sub(name,data):return name+struct.pack('<H',len(data))+data
payload=sub(b'HEDR',struct.pack('<fII',1.0,0,0x800))+sub(b'CNAM',b'LorexValkin\0')+sub(b'SNAM',b'Skeleton follower preview; no race or gameplay records.\0')+sub(b'MAST',b'Oblivion.esm\0')+sub(b'DATA',bytes(8))
marker.write_bytes(b'TES4'+struct.pack('<IIII',len(payload),0,0,0)+payload)
script=P.parent/'author-preview-metadata.py'
script.write_text('''import unreal,re
from pathlib import Path
root=Path(unreal.Paths.project_content_dir())/'Mods/UnblivionModLoader'
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Mods'],force_rescan=True)
def field(struct,name):
 pattern=rb'(?<![A-Za-z0-9_])'+name.encode()+rb'_[0-9]+_[A-F0-9]{32}(?![A-Za-z0-9_])'
 matches={x.decode() for x in re.findall(pattern,(root/(struct+'.uasset')).read_bytes())}
 if len(matches)!=1:raise RuntimeError(str(matches))
 return matches.pop()
path='/Game/Mods/ExtendedRacesSkeletonPrototype/DA_SkeletonPrototypeModInfo'
if unreal.EditorAssetLibrary.does_asset_exist(path):raise RuntimeError('Already authored')
cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Mods/UnblivionModLoader/PDA_ModInfo')
if not cls:raise RuntimeError('UML metadata class missing')
factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',cls)
asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],cls,factory)
if not asset:raise RuntimeError('Metadata create failed')
info=asset.get_editor_property('ModInfo')
for name,value in {'ModName':'Extended Races Skeleton Prototype','ModAuthor':'LorexValkin','ModVersion':'0.1.0','ModDescription':'Four animated Skeleton cosmetic previews alongside the player and offset first-person arms. No Skeleton race selection yet.'}.items():info.set_editor_property(field('S_ModInfo',name),value)
esp=info.get_editor_property(field('S_ModInfo','ESPModInfo'))
esp.set_editor_property(field('S_ESPMod','ESPPluginName'),'ExtendedRacesSkeletonPrototype.esp')
esp.set_editor_property(field('S_ESPMod','RequiredMasterPlugins'),['Oblivion.esm'])
info.set_editor_property(field('S_ModInfo','ESPModInfo'),esp)
logic=info.get_editor_property(field('S_ModInfo','LogicModInfo'))
actor=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Mods/ExtendedRacesSkeletonPrototype/BP_SkeletonPreview')
if not actor:raise RuntimeError('Preview actor missing')
logic.set_editor_property(field('S_LogicMod','CustomBPLogicModActor'),actor)
info.set_editor_property(field('S_ModInfo','LogicModInfo'),logic)
asset.set_editor_property('ModInfo',info)
if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False):raise RuntimeError('Metadata save failed')
unreal.log('SKELETON_METADATA saved='+path)
''')
args=['F:/UE_5.3.2-src/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(P/'UnblivionEditor.uproject'),'-run=pythonscript','-script='+str(script),'-unattended','-nop4','-nosound','-NullRHI','-NoAssetManagerScan','-AbsLog='+str(P.parent/'metadata.log')]
with (P.parent/'metadata-console.log').open('w') as out:r=subprocess.run(args,stdout=out,stderr=subprocess.STDOUT)
if r.returncode:raise RuntimeError('Metadata failed '+str(r.returncode))
config=json.loads((W.parent/'extended-races-cooker/skin-cook.json').read_text())
content=P/'Content'
files=list((content/'Art/Character/ExtendedRacesSkeleton').glob('*.uasset'))+list((content/'CharacterReference/SystemImport/ExtendedRacesSkeleton').glob('*_Skeleton.uasset'))+list((content/'Mods/ExtendedRacesSkeletonPrototype').glob('*.uasset'))
config.update(modName='ExtendedRacesSkeletonPrototype',project=str(P/'UnblivionEditor.uproject'),output=str(O),esp=str(marker),packages=['/Game/'+x.relative_to(content).with_suffix('').as_posix() for x in files],discover=['/Game/Mods/ExtendedRacesSkeletonPrototype/DA_SkeletonPrototypeModInfo'])
cfg=P.parent/'preview-cook.json';cfg.write_text(json.dumps(config,indent=2))
r=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(W.parent/'extended-races-cooker/Cook-Mod.ps1'),'-Config',str(cfg)])
raise SystemExit(r.returncode)

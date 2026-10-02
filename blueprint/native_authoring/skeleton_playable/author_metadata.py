"""Connect the playable form and appearance Blueprint to UML discovery."""
import unreal
import json
import re
from pathlib import Path

ROOT = '/Game/Mods/ExtendedRacesSkeleton'
content = Path(unreal.Paths.project_content_dir())
loader = content / 'Mods/UnblivionModLoader'
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Mods'], force_rescan=True)


def field(struct, name):
    found = {x.decode() for x in re.findall(rb'(?<![A-Za-z0-9_])' + name.encode()
                                            + rb'_[0-9]+_[A-F0-9]{32}(?![A-Za-z0-9_])',
                                            (loader / (struct + '.uasset')).read_bytes())}
    if len(found) != 1: raise RuntimeError(str(found))
    return found.pop()


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(asset.get_path_name())


race = unreal.load_asset(ROOT + '/Race_Skeleton')
actor = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + '/BP_SkeletonAppearance')
if not race or not actor: raise RuntimeError('Missing playable assets')
factory = unreal.DataTableFactory()
factory.set_editor_property('struct',
                            unreal.load_object(None, '/Game/Mods/UnblivionModLoader/ST_SyncMapping.ST_SyncMapping'))
table = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DT_SkeletonSync', ROOT, unreal.DataTable, factory)
row = {'Name': 'Skeleton', field('ST_SyncMapping', 'OriginPlugin'): 'ExtendedRacesSkeleton.esp',
       field('ST_SyncMapping', 'LocalFormID'): 0x800, field('ST_SyncMapping', 'AssetPath'): race.get_path_name()}
if not table or not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps([row])):
    raise RuntimeError('Form mapping failed')
save(table)
cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Mods/UnblivionModLoader/PDA_ModInfo')
factory = unreal.DataAssetFactory();
factory.set_editor_property('data_asset_class', cls)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_SkeletonModInfo', ROOT, cls, factory)
info = asset.get_editor_property('ModInfo')
for name, value in {'ModName': 'Extended Races Skeleton', 'ModAuthor': 'LorexValkin', 'ModVersion': '0.2.0',
                    'ModDescription': 'Playable Skeleton test candidate. Native race body with reversible head and first-person arms appearance.'}.items():
    info.set_editor_property(field('S_ModInfo', name), value)
esp = info.get_editor_property(field('S_ModInfo', 'ESPModInfo'))
esp.set_editor_property(field('S_ESPMod', 'ESPPluginName'), 'ExtendedRacesSkeleton.esp')
masters = json.loads(Path(unreal.Paths.project_dir()).parent.joinpath('records/verification.json').read_text())[
    'masters']
esp.set_editor_property(field('S_ESPMod', 'RequiredMasterPlugins'), masters)
esp.set_editor_property(field('S_ESPMod', 'SyncTable'), table)
info.set_editor_property(field('S_ModInfo', 'ESPModInfo'), esp)
logic = info.get_editor_property(field('S_ModInfo', 'LogicModInfo'))
logic.set_editor_property(field('S_LogicMod', 'CustomBPLogicModActor'), actor)
info.set_editor_property(field('S_ModInfo', 'LogicModInfo'), logic)
asset.set_editor_property('ModInfo', info);
save(asset)
report = {'race': race.get_path_name(), 'actor': actor.get_path_name(), 'syncRow': row,
          'metadata': asset.get_path_name(),
          'bodies': {name: str(race.get_editor_property(name)) for name in ('MaleFullBodies', 'FemaleFullBodies')}}
Path(unreal.Paths.project_dir()).parent.joinpath('race-assets.json').write_text(json.dumps(report, indent=2))
unreal.log('SKELETON_METADATA complete')

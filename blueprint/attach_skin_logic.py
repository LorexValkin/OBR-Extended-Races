"""Bind the cooked first-person skin actor to Extended Races loader metadata.

Run in the game-schema Unreal 5.3.2 authoring project after the actor Blueprint
has compiled. This only saves the mod-owned data asset.
"""

from pathlib import Path
import re

import unreal


ROOT = Path(unreal.Paths.project_content_dir()) / "Mods" / "UnblivionModLoader"
INFO_PATH = "/Game/Mods/ExtendedRaces/DA_ExtendedRacesModInfo"
ACTOR_PATH = "/Game/Mods/ExtendedRaces/BP_ExtendedRacesFirstPersonSkin"


def field(struct_name, display_name):
    source = (ROOT / f"{struct_name}.uasset").read_bytes()
    pattern = rb"(?<![A-Za-z0-9_])" + display_name.encode("ascii")
    pattern += rb"_[0-9]+_[A-F0-9]{32}(?![A-Za-z0-9_])"
    matches = {part.decode("ascii") for part in re.findall(pattern, source)}
    if len(matches) != 1:
        raise RuntimeError(f"Expected one {struct_name}.{display_name}: {matches}")
    return matches.pop()


asset = unreal.EditorAssetLibrary.load_asset(INFO_PATH)
actor_class = unreal.EditorAssetLibrary.load_blueprint_class(ACTOR_PATH)
if not asset or not actor_class:
    raise RuntimeError(f"Missing metadata or compiled actor: {INFO_PATH}, {ACTOR_PATH}")

info = asset.get_editor_property("ModInfo")
logic_field = field("S_ModInfo", "LogicModInfo")
actor_field = field("S_LogicMod", "CustomBPLogicModActor")
logic = info.get_editor_property(logic_field)
logic.set_editor_property(actor_field, actor_class)
info.set_editor_property(logic_field, logic)
asset.set_editor_property("ModInfo", info)
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
    raise RuntimeError(f"Could not save {INFO_PATH}")

reloaded = unreal.EditorAssetLibrary.load_asset(INFO_PATH)
saved_info = reloaded.get_editor_property("ModInfo")
saved_logic = saved_info.get_editor_property(logic_field)
saved_class = saved_logic.get_editor_property(actor_field)
if not saved_class or saved_class.get_path_name() != actor_class.get_path_name():
    raise RuntimeError(f"Logic actor did not persist: {saved_class!r}")
for name, expected in (("ModName", "Extended Races"), ("ModAuthor", "LorexValkin")):
    actual = saved_info.get_editor_property(field("S_ModInfo", name))
    if actual != expected:
        raise RuntimeError(f"{name}: expected {expected!r}, got {actual!r}")
unreal.log(f"EXTENDED_RACES_SKIN_LOGIC saved={INFO_PATH} actor={saved_class.get_path_name()}")

"""Author the Extended Races metadata asset against the checked-out loader schema.

Run with an installed UE 5.3.2 UnrealEditor-Cmd -ExecutePythonScript in an isolated
project containing the current loader struct and PDA_ModInfo drop-ins. The
source-built editor used during initial authoring stamped an empty engine version,
which caused a compatibility warning when the installed editor cooked the asset.
Only the mod-owned data asset is saved; loader declarations are authoring inputs.
"""

from pathlib import Path
import re
import struct

import unreal


PACKAGE = "/Game/Mods/ExtendedRaces/DA_ExtendedRacesModInfo"
PDA_CLASS = "/Game/Mods/UnblivionModLoader/PDA_ModInfo"
STRUCT_FILE = (
    Path(unreal.Paths.project_content_dir())
    / "Mods"
    / "UnblivionModLoader"
    / "S_ModInfo.uasset"
)


def field_name(display_name, struct_name="S_ModInfo"):
    """User-defined struct fields have a generated GUID suffix in UE 5.3."""
    pattern = rb"(?<![A-Za-z0-9_])" + display_name.encode("ascii")
    pattern += rb"_[0-9]+_[A-F0-9]{32}(?![A-Za-z0-9_])"
    source = STRUCT_FILE.with_name(f"{struct_name}.uasset")
    matches = {match.decode("ascii") for match in re.findall(pattern, source.read_bytes())}
    if len(matches) != 1:
        raise RuntimeError(f"Expected one {display_name} field in {source}: {matches}")
    return matches.pop()


def plugin_masters(path):
    """Use the actual ESP header so metadata cannot drift from plugin dependencies."""
    data = path.read_bytes()
    # Oblivion's TES4 record header is 20 bytes (later Bethesda games use 24).
    if data[:4] != b"TES4" or len(data) < 20:
        raise RuntimeError(f"Invalid ESP header: {path}")
    end = 20 + struct.unpack_from("<I", data, 4)[0]
    if end > len(data):
        raise RuntimeError(f"Truncated ESP header: {path}")
    cursor, masters = 20, []
    while cursor < end:
        if cursor + 6 > end:
            raise RuntimeError(f"Truncated ESP subrecord: {path}")
        kind, size = struct.unpack_from("<4sH", data, cursor)
        cursor += 6
        if kind == b"XXXX" or cursor + size > end:
            raise RuntimeError(f"Unsupported or invalid ESP subrecord: {path}")
        if kind == b"MAST":
            value = data[cursor:cursor + size]
            if not value.endswith(b"\0"):
                raise RuntimeError(f"Unterminated ESP master: {path}")
            masters.append(value[:-1].decode("ascii"))
        cursor += size
    if not masters or len(set(masters)) != len(masters):
        raise RuntimeError(f"Missing or duplicate ESP masters: {path}")
    return masters


def main():
    loader_class = unreal.EditorAssetLibrary.load_blueprint_class(PDA_CLASS)
    if not loader_class:
        raise RuntimeError(f"Loader data asset class missing: {PDA_CLASS}")

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", loader_class)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = (unreal.EditorAssetLibrary.load_asset(PACKAGE)
             if unreal.EditorAssetLibrary.does_asset_exist(PACKAGE) else None)
    if not asset:
        asset = tools.create_asset(
            "DA_ExtendedRacesModInfo",
            "/Game/Mods/ExtendedRaces",
            loader_class,
            factory,
        )
    if not asset:
        raise RuntimeError(f"Could not create {PACKAGE}")

    info = asset.get_editor_property("ModInfo")
    info.set_editor_property(field_name("ModName"), "Extended Races")
    info.set_editor_property(field_name("ModAuthor"), "LorexValkin")
    info.set_editor_property(field_name("ModVersion"), "1.0.0")
    info.set_editor_property(
        field_name("ModDescription"),
        "Play as a Dark Seducer, Golden Saint, Dremora, or Sheogorath.",
    )
    esp_path = Path(__file__).resolve().parents[1] / "mod/esp/ExtendedRaces.esp"
    masters = plugin_masters(esp_path)
    esp_field = field_name("ESPModInfo")
    esp_info = info.get_editor_property(esp_field)
    plugin_field = field_name("ESPPluginName", "S_ESPMod")
    masters_field = field_name("RequiredMasterPlugins", "S_ESPMod")
    esp_info.set_editor_property(plugin_field, esp_path.name)
    esp_info.set_editor_property(masters_field, masters)
    info.set_editor_property(esp_field, esp_info)
    asset.set_editor_property("ModInfo", info)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {PACKAGE}")

    saved = unreal.EditorAssetLibrary.load_asset(PACKAGE)
    if not saved:
        raise RuntimeError(f"Saved asset cannot be loaded: {PACKAGE}")
    saved_info = saved.get_editor_property("ModInfo")
    saved_esp = saved_info.get_editor_property(esp_field)
    if saved_esp.get_editor_property(plugin_field) != esp_path.name:
        raise RuntimeError("ESP dependency did not persist")
    if list(saved_esp.get_editor_property(masters_field)) != masters:
        raise RuntimeError("Required ESP masters did not persist")
    for field, expected in (
        ("ModName", "Extended Races"),
        ("ModAuthor", "LorexValkin"),
        ("ModVersion", "1.0.0"),
    ):
        actual = saved_info.get_editor_property(field_name(field))
        if actual != expected:
            raise RuntimeError(f"{field}: expected {expected!r}, got {actual!r}")
    unreal.log(f"EXTENDED_RACES_MOD_INFO saved={PACKAGE}")
    unreal.log(f"EXTENDED_RACES_MOD_INFO metadata={saved_info.export_text()}")


main()

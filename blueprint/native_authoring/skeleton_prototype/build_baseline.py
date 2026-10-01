"""Build an untouched Skeleton baseline and refuse unproved rig remapping."""
from pathlib import Path
import hashlib
import json
import sys

HERE = Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring/skeleton-mesh-prototype'
TOOLS = Path('C:/Users/User/Desktop/Projects/OBR_Alter_Map/tools')
LIBRARY = Path('D:/OBR- Raw Texture Meshes')
sys.path.insert(0, str(TOOLS))
from character_skin_glb import emit
from validate_character_skin import validate


def digest(path):
    return hashlib.file_digest(Path(path).open('rb'), 'sha256').hexdigest()


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False), encoding='utf-8')


def authenticate_mount(relative, mount_path):
    mount = json.loads(mount_path.read_text())
    row = next(row for row in mount['files'] if row['target'].lower() == relative.lower())
    actual = digest(LIBRARY / relative)
    if actual != row['sha256']:
        raise ValueError('Published input changed: ' + relative)
    result = dict(target=relative, sha256=actual, mount=str(mount_path),
                  mount_sha256=digest(mount_path), publication=row)
    manifest = Path(mount['source']) / 'manifest.json'
    result['source_manifest'] = str(manifest)
    result['source_manifest_present'] = manifest.exists()
    if manifest.exists():
        result['source_manifest_sha256'] = digest(manifest)
        if digest(manifest) != mount['source_manifest_sha256']:
            raise ValueError('Publication manifest changed')
    return result


def main():
    output = HERE / 'baseline-v1'
    if output.exists():
        raise ValueError('Retain prior evidence; use a fresh run directory')
    output.mkdir()
    relative = 'Metadata/OblivionRemastered/Content/Art/Creatures/Skeleton/SK_skeleton.skin.json'
    source_path = LIBRARY / relative
    source = json.loads(source_path.read_text())
    mount = LIBRARY / 'Receipts/all-resources-20260905-01-export-05-0128/mount.json'
    provenance = authenticate_mount(relative, mount)
    glb = output / 'SK_Skeleton_Source_LOD0.glb'
    emitted = emit(source, glb)
    checked = validate(glb, source)
    lod = next(row for row in source['lods'] if row['SourceLodIndex'] == 0)
    vertices = []
    for vertex in lod['vertices']:
        positive = [row for row in vertex['Influences'] if row['Weight'] > 0]
        total = sum(row['Weight'] for row in positive)
        if not 0 < len(positive) <= 12 or total <= 0:
            raise ValueError('Unsupported weight domain')
        vertices.append(dict(position=[vertex['Position'][key] for key in 'XYZ'],
                             uv=[vertex['Uv'][key] for key in 'UV'],
                             normal=[vertex['Normal'][key] for key in 'XYZ'],
                             tangent=[vertex['Tangent'][key] for key in 'XYZ'],
                             binormal_sign=1 if vertex['Normal']['W'] >= 0 else -1,
                             weights=[dict(bone=source['bones'][row['Bone']]['Name'],
                                           weight=row['Weight'] / total) for row in positive]))
    scaffold = '/Game/Mods/ExtendedRaces/Prototype/SK_Skeleton_Source_LOD0.SK_Skeleton_Source_LOD0'
    target = '/Game/Mods/ExtendedRaces/Prototype/SK_Skeleton_Baseline.SK_Skeleton_Baseline'
    spec = dict(schema='character-source-mesh-v1', source_mesh=scaffold, target_mesh=target,
                lod_index=0, bones=source['bones'], vertices=vertices, indices=lod['indices'],
                sections=lod['sections'], material_slots=[row['MaterialSlotName'] for row in source['materials']])
    write(output / 'lod0-source-mesh.json', spec)
    write(output / 'build-batch.json', dict(schema='character-source-mesh-batch-v1', meshes=[
        dict(object=target, spec=str(output / 'lod0-source-mesh.json'), receipt=str(output / 'unreal-build.json'))]))

    # Equal names alone do not establish compatible local poses or weighted chains.
    # Preserve the missing correspondences instead of silently binding to a nearby bone.
    humanoid_path = LIBRARY / 'Metadata/OblivionRemastered/Content/Art/Character/Imperial/SK_Imperial_Body_m.skin.json'
    humanoid = json.loads(humanoid_path.read_text())
    humanoid_names = {row['Name'].casefold(): row for row in humanoid['bones']}
    weighted = sorted({source['bones'][row['Bone']]['Name'] for vertex in lod['vertices']
                       for row in vertex['Influences'] if row['Weight'] > 0})
    absent = [name for name in weighted if name.casefold() not in humanoid_names]
    same_names = [name for name in weighted if name.casefold() in humanoid_names]
    rig = dict(schema='skeleton-rig-compatibility-v1', humanoid_source=str(humanoid_path),
               humanoid_sha256=digest(humanoid_path), skeleton_weighted_bones=weighted,
               same_name_weighted_bones=same_names, unsupported_weighted_bones=absent,
               verdict='No humanoid rebind emitted: different weighted chains and unproved pose transfer.',
               limits=['Humanoid body publication provenance remains to be authenticated.',
                       'This comparison does not derive anatomical bone mappings or weight transfer.',
                       'No Unreal import, deformation, retarget, rendering or in-game test performed.'])
    write(output / 'rig-compatibility.json', rig)
    receipt = dict(schema='skeleton-baseline-receipt-v1', source=provenance,
                   helpers=[dict(path=str(TOOLS / name), sha256=digest(TOOLS / name))
                            for name in ('character_skin_glb.py', 'validate_character_skin.py')],
                   emission=emitted, independent_retained_source_comparison=checked,
                   output_scope='Untouched creature Skeleton LOD0, not a player-compatible mesh.',
                   artifacts=[dict(path=str(path), bytes=path.stat().st_size, sha256=digest(path))
                              for path in sorted(output.iterdir())],
                   next_proof='Isolated import, CharacterSourceMesh build, fresh geometry/skin validation and material rendering.')
    write(output / 'receipt.json', receipt)
    print(json.dumps(dict(output=str(output), checked=checked, unsupported_weighted_bones=absent), indent=2))


if __name__ == '__main__':
    main()

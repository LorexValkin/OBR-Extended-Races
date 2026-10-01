"""Author a rest-pose approximation on the real Imperial humanoid rig."""
from collections import defaultdict
from copy import deepcopy
from pathlib import Path
import json
import re
import sys
import numpy as np

from build_baseline import digest, write, LIBRARY, TOOLS, HERE
from character_skin_glb import emit, matrix
from validate_character_skin import validate


def globals_for(bones):
    result = []
    for index, bone in enumerate(bones):
        parent = bone['ParentIndex']
        if not -1 <= parent < index:
            raise ValueError('Bone hierarchy is not parent-first')
        pose = bone['Transform']
        local = matrix([pose['Translation'][k] for k in 'XYZ'],
                       [pose['Rotation'][k] for k in 'XYZW'],
                       [pose['Scale3D'][k] for k in 'XYZ'])
        global_pose = result[parent] @ local if parent >= 0 else local
        if not np.isfinite(global_pose).all() or abs(np.linalg.det(global_pose[:3, :3])) < 1e-8:
            raise ValueError('Non-finite or singular reference pose')
        result.append(global_pose)
    return result


def target_name(name):
    center = dict(Root_M='pelvis', Spine1_M='spine_01', Spine2_M='spine_03',
                  Chest_M='spine_05', Neck1_M='neck_01', Neck2_M='neck_02',
                  Head_M='head', Jaw_M='head')
    if name in center:
        return center[name]
    match = re.fullmatch(r'(Hip|Knee|Ankle|CorectClavicle|Shoulder|Elbow|Wrist)_([LR])(?:_50)?', name)
    if match:
        stem, side = match.groups()
        return dict(Hip='thigh', Knee='calf', Ankle='foot', CorectClavicle='clavicle',
                    Shoulder='upperarm', Elbow='lowerarm', Wrist='hand')[stem] + '_' + side.lower()
    match = re.fullmatch(r'(Index|Middle|Pinky|Ring)Finger([1-4])_([LR])', name)
    if match:
        stem, segment, side = match.groups()
        return stem.lower() + ('_metacarpal' if segment == '1' else '_0' + str(int(segment) - 1)) + '_' + side.lower()
    match = re.fullmatch(r'ThumbFinger([1-3])_([LR])', name)
    if match:
        segment, side = match.groups()
        return 'thumb_0' + segment + '_' + side.lower()
    match = re.fullmatch(r'(Big|Long|Middle|Pinky|Ring)Toe([1-3])_([LR])', name)
    if match:
        stem, segment, side = match.groups()
        return dict(Big='bigtoe', Long='indextoe', Middle='middletoe', Pinky='littletoe', Ring='ringtoe')[
            stem] + '_0' + str(min(int(segment), 2)) + '_' + side.lower()
    raise ValueError('Unmapped positively weighted source bone: ' + name)


def spec_for(skin, label, folder):
    lod = skin['lods'][0]
    scaffold = '/Game/CharacterReference/SystemImport/ExtendedRacesSkeleton/SK_Skeleton_' + label + '_Source'
    target = '/Game/Art/Character/ExtendedRacesSkeleton/SK_Skeleton_' + label
    vertices = []
    for vertex in lod['vertices']:
        vertices.append(dict(position=[vertex['Position'][k] for k in 'XYZ'],
                             uv=[vertex['Uv'][k] for k in 'UV'],
                             normal=[vertex['Normal'][k] for k in 'XYZ'],
                             tangent=[vertex['Tangent'][k] for k in 'XYZ'],
                             binormal_sign=1 if vertex['Normal']['W'] >= 0 else -1,
                             weights=[dict(bone=skin['bones'][w['Bone']]['Name'], weight=w['Weight']) for w in
                                      vertex['Influences']]))
    spec = dict(schema='character-source-mesh-v1', source_mesh=scaffold + '.' + scaffold.rsplit('/', 1)[1],
                target_mesh=target + '.' + target.rsplit('/', 1)[1], lod_index=0,
                bones=skin['bones'], vertices=vertices, indices=lod['indices'], sections=lod['sections'],
                material_slots=[row['MaterialSlotName'] for row in skin['materials']])
    path = folder / ('SK_Skeleton_' + label + '.spec.json')
    write(path, spec)
    return dict(object=spec['target_mesh'], spec=str(path), receipt=str(folder / (label + '-unreal-build.json')))


def main():
    output = HERE / 'rebind-v1'
    if output.exists():
        raise ValueError('Use a fresh output revision')
    source_path = LIBRARY / 'Metadata/OblivionRemastered/Content/Art/Creatures/Skeleton/SK_skeleton.skin.json'
    target_path = LIBRARY / 'Metadata/OblivionRemastered/Content/Art/Character/Imperial/SK_Imperial_Body_m.skin.json'
    input_hashes = {str(path): digest(path) for path in (source_path, target_path)}
    source = json.loads(source_path.read_text())
    target = json.loads(target_path.read_text())
    sg, tg = globals_for(source['bones']), globals_for(target['bones'])
    target_indices = {bone['Name']: i for i, bone in enumerate(target['bones'])}
    lod = next(row for row in source['lods'] if row['SourceLodIndex'] == 0)
    used = sorted({w['Bone'] for vertex in lod['vertices'] for w in vertex['Influences'] if w['Weight'] > 0})
    mappings, transforms, normals = {}, {}, {}
    audit = []
    for bone_index in used:
        name = source['bones'][bone_index]['Name']
        destination = target_name(name)
        if destination not in target_indices:
            raise ValueError('Authored mapping target is absent: ' + destination)
        ti = target_indices[destination]
        mappings[bone_index] = ti
        transforms[bone_index] = tg[ti] @ np.linalg.inv(sg[bone_index])
        normals[bone_index] = np.linalg.inv(transforms[bone_index][:3, :3]).T
        a, b = sg[bone_index][:3, 3], tg[ti][:3, 3]
        # Both retained meshes are centimeter Z-up, with left limbs at positive X.
        # A side reversal or a distant scale domain invalidates this authored mapping.
        if name.endswith('_L') and (a[0] < -1 or b[0] < -1):
            raise ValueError('Left-side anatomical coordinate mismatch')
        if name.endswith('_R') and (a[0] > 1 or b[0] > 1):
            raise ValueError('Right-side anatomical coordinate mismatch')
        if max(abs(a[2]), abs(b[2])) > 250:
            raise ValueError('Reference-pose unit/height domain mismatch')
        audit.append(dict(source=name, target=destination, source_global_cm=a.tolist(),
                          target_global_cm=b.tolist(), authored_warp=transforms[bone_index].tolist()))

    rebound = dict(bones=deepcopy(target['bones']), materials=deepcopy(source['materials']), lods=[deepcopy(lod)])
    arm_mass = []
    for original, vertex in zip(lod['vertices'], rebound['lods'][0]['vertices']):
        weights = [w for w in original['Influences'] if w['Weight'] > 0]
        total = sum(w['Weight'] for w in weights)
        position = np.array([original['Position'][k] for k in 'XYZ'] + [1.0])
        normal = np.array([original['Normal'][k] for k in 'XYZ'])
        tangent = np.array([original['Tangent'][k] for k in 'XYZ'])
        p, n, t = np.zeros(3), np.zeros(3), np.zeros(3)
        merged = defaultdict(float)
        mass = 0.0
        for w in weights:
            bi, weight = w['Bone'], w['Weight'] / total
            p += weight * (transforms[bi] @ position)[:3]
            n += weight * (normals[bi] @ normal)
            t += weight * (transforms[bi][:3, :3] @ tangent)
            merged[mappings[bi]] += weight
            if re.fullmatch(r'(Shoulder|Elbow|Wrist)_[LR]', source['bones'][bi]['Name']) or 'Finger' in \
                    source['bones'][bi]['Name']:
                mass += weight
        n /= np.linalg.norm(n)
        t -= n * np.dot(n, t)
        t /= np.linalg.norm(t)
        if not np.isfinite(np.concatenate((p, n, t))).all():
            raise ValueError('Degenerate transformed vertex frame')
        for field, vector in [('Position', p), ('Normal', n), ('Tangent', t)]:
            for k, value in zip('XYZ', vector):
                vertex[field][k] = float(value)
        vertex['Influences'] = [dict(Bone=bi, Weight=float(weight)) for bi, weight in sorted(merged.items())]
        arm_mass.append(mass)

    # Requiring every triangle vertex to have >=95% arm-chain mass excludes torso,
    # skull and legs; clavicle-only shoulder transition triangles are intentionally cut.
    selected = []
    for i in range(0, len(lod['indices']), 3):
        triangle = lod['indices'][i:i + 3]
        if all(arm_mass[v] >= 0.95 for v in triangle):
            selected.append(triangle)
    indices_used = sorted({v for triangle in selected for v in triangle})
    remap = {v: i for i, v in enumerate(indices_used)}
    arms = deepcopy(rebound)
    alod = arms['lods'][0]
    alod['vertices'] = [deepcopy(rebound['lods'][0]['vertices'][v]) for v in indices_used]
    alod['indices'] = [remap[v] for triangle in selected for v in triangle]
    if len(lod['sections']) != 1:
        raise ValueError('Subset section adapter requires one source section')
    alod['sections'] = [deepcopy(lod['sections'][0])]
    alod['sections'][0].update(FirstIndex=0, NumFaces=len(selected))
    output.mkdir()
    checks = []
    batches = []
    for label, skin in [('HumanoidBody', rebound), ('HumanoidArms', arms)]:
        path = output / ('SK_Skeleton_' + label + '.skin.json')
        write(path, skin)
        glb = output / ('SK_Skeleton_' + label + '_Source.glb')
        emitted = emit(skin, glb)
        comparison = validate(glb, skin)
        assert all(abs(sum(w['Weight'] for w in v['Influences']) - 1) < 1e-12 for v in skin['lods'][0]['vertices'])
        batches.append(spec_for(skin, label, output))
        checks.append(dict(label=label, emission=emitted, comparison=comparison))
    assert rebound['lods'][0]['indices'] == lod['indices']
    assert all(a['Uv'] == b['Uv'] for a, b in zip(lod['vertices'], rebound['lods'][0]['vertices']))
    assert all(alod['vertices'][i]['Uv'] == lod['vertices'][v]['Uv'] for i, v in enumerate(indices_used))
    assert all(digest(path) == value for path, value in input_hashes.items())
    write(output / 'build-batch.json', dict(schema='character-source-mesh-batch-v1', meshes=batches))
    write(output / 'mapping-audit.json', dict(mappings=audit, arms_source_vertex_indices=indices_used,
                                              threshold=0.95, source_triangle_count=len(lod['indices']) // 3,
                                              arms_triangle_count=len(selected)))
    write(output / 'receipt.json', dict(schema='skeleton-authored-rebind-v1', inputs=input_hashes,
                                        helper_hashes={str(TOOLS / name): digest(TOOLS / name) for name in
                                                       ('character_skin_glb.py', 'validate_character_skin.py')},
                                        mapping_count=len(mappings), checks=checks,
                                        target_bone_count=len(target['bones']),
                                        artifacts=[dict(path=str(p), sha256=digest(p), bytes=p.stat().st_size) for p in
                                                   sorted(output.iterdir())],
                                        limitations=['Authored approximation, not recovered retail retargeting.',
                                                     'Jaw merges to head; third toe segments merge to second; no facial articulation.',
                                                     'Linear blended rest-pose warp can distort joints and thin bones; rendered QA required.',
                                                     'Arms are an explicit triangle subset with open shoulder cuts; no cap geometry added.',
                                                     'Material placeholders, no physics, morphs, clothing masks, gameplay or camera setup.',
                                                     'Target reference skeleton retains all Imperial body bones; shared skeleton binding still needs isolated editor proof.',
                                                     'No Unreal import, pose deformation, rendering or runtime compatibility claimed.']))
    print(json.dumps(dict(output=str(output), checks=checks), indent=2))


if __name__ == '__main__':
    main()

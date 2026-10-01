"""Author Skeleton armor presets with each leaf's actual reference poses."""
from copy import deepcopy
from collections import defaultdict
import json
import numpy as np
from build_baseline import HERE, LIBRARY, TOOLS, digest, write
from build_rebind import globals_for, target_name, spec_for
from character_skin_glb import emit
from validate_character_skin import validate


def material_path(row):
    path = row['Material']['ObjectPath']
    path = path.removeprefix('OblivionRemastered/Content/').removesuffix('.0')
    if not path.startswith('/Game/'):
        path = '/Game/' + path
    return path + '.' + path.rsplit('/', 1)[1]


def warp(source, target, body, label):
    result = dict(bones=deepcopy(target['bones']), materials=deepcopy(source['materials']),
                  lods=[deepcopy(next(l for l in source['lods'] if l['SourceLodIndex'] == 0))])
    sg, tg, bg = globals_for(source['bones']), globals_for(target['bones']), globals_for(body['bones'])
    ti = {b['Name']: i for i, b in enumerate(target['bones'])}
    bi = {b['Name']: i for i, b in enumerate(body['bones'])}
    used = {w['Bone'] for v in result['lods'][0]['vertices'] for w in v['Influences'] if w['Weight'] > 0}
    transforms, normal_transforms, mappings, audit = {}, {}, {}, []
    for i in sorted(used):
        name = source['bones'][i]['Name']
        destination = 'ball_' + name[-1].lower() if name in ('Toes_L', 'Toes_R') else target_name(name)
        if destination not in ti or name not in bi:
            raise ValueError('Unsupported weighted armor bone: ' + name)
        source_xyz, target_xyz = sg[i][:3, 3], tg[ti[destination]][:3, 3]
        if name.startswith('Toes_'):
            # Both joints sit in the forefoot, forward of the ankle, near the ground.
            # This is an authored anatomical correspondence, not an equal-pose claim.
            if not (source_xyz[1] > 5 and target_xyz[1] > 5 and
                    0 <= source_xyz[2] < 5 and 0 <= target_xyz[2] < 5 and
                    np.linalg.norm(source_xyz - target_xyz) < 8):
                raise ValueError('Forefoot mapping falls outside inspected anatomical domain')
        translation_difference = float(np.linalg.norm(source_xyz - bg[bi[name]][:3, 3]))
        basis_difference = float(np.max(np.abs(sg[i][:3, :3] - bg[bi[name]][:3, :3])))
        if translation_difference > 2 or basis_difference > 0.05:
            raise ValueError('Armor pose differs beyond inspected source domain: ' + name)
        mapping = tg[ti[destination]] @ np.linalg.inv(sg[i])
        transforms[i] = mapping
        normal_transforms[i] = np.linalg.inv(mapping[:3, :3]).T
        mappings[i] = ti[destination]
        audit.append(dict(source=name, target=destination, source_global_cm=source_xyz.tolist(),
                          target_global_cm=target_xyz.tolist(),
                          source_body_translation_difference_cm=translation_difference,
                          source_body_basis_difference=basis_difference, authored_warp=mapping.tolist()))
    source_lod = next(l for l in source['lods'] if l['SourceLodIndex'] == 0)
    for original, vertex in zip(source_lod['vertices'], result['lods'][0]['vertices']):
        positive = [w for w in original['Influences'] if w['Weight'] > 0]
        total = sum(w['Weight'] for w in positive)
        position = np.array([original['Position'][k] for k in 'XYZ'] + [1])
        n0, t0 = [np.array([original[f][k] for k in 'XYZ']) for f in ('Normal', 'Tangent')]
        p, n, t, weights = np.zeros(3), np.zeros(3), np.zeros(3), defaultdict(float)
        for w in positive:
            i, weight = w['Bone'], w['Weight'] / total
            p += weight * (transforms[i] @ position)[:3]
            n += weight * (normal_transforms[i] @ n0)
            t += weight * (transforms[i][:3, :3] @ t0)
            weights[mappings[i]] += weight
        n /= np.linalg.norm(n)
        t -= n * np.dot(n, t)
        t /= np.linalg.norm(t)
        if not np.isfinite(np.r_[p, n, t]).all():
            raise ValueError('Invalid transformed frame')
        for field, vector in [('Position', p), ('Normal', n), ('Tangent', t)]:
            for k, value in zip('XYZ', vector):
                vertex[field][k] = float(value)
        vertex['Influences'] = [dict(Bone=i, Weight=float(w)) for i, w in sorted(weights.items())]
    assert result['lods'][0]['indices'] == source_lod['indices']
    assert result['lods'][0]['sections'] == source_lod['sections']
    assert all(a['Uv'] == b['Uv'] for a, b in zip(result['lods'][0]['vertices'], source_lod['vertices']))
    return result, audit


def combine(leaves, target):
    output = dict(bones=deepcopy(target['bones']), materials=[],
                  lods=[dict(SourceLodIndex=0, vertices=[], indices=[], sections=[])])
    lod = output['lods'][0]
    for label, skin in leaves:
        source = skin['lods'][0]
        vertex_offset, index_offset, material_offset = len(lod['vertices']), len(lod['indices']), len(
            output['materials'])
        lod['vertices'].extend(deepcopy(source['vertices']))
        lod['indices'].extend(i + vertex_offset for i in source['indices'])
        for row in skin['materials']:
            row = deepcopy(row)
            row['SourceMaterialSlotName'] = row['MaterialSlotName']
            row['MaterialSlotName'] = label + '__' + row['MaterialSlotName']
            output['materials'].append(row)
        for section in source['sections']:
            section = deepcopy(section)
            section['FirstIndex'] += index_offset
            section['MaterialIndex'] += material_offset
            lod['sections'].append(section)
    return output


def main():
    output = HERE / 'cosmetics-v1'
    if output.exists():
        raise ValueError('Keep prior evidence; choose a fresh revision')
    root = LIBRARY / 'Metadata/OblivionRemastered/Content/Art/Creatures/Skeleton'
    body_path = root / 'SK_skeleton.skin.json'
    target_path = LIBRARY / 'Metadata/OblivionRemastered/Content/Art/Character/Imperial/SK_Imperial_Body_m.skin.json'
    paths = [body_path, target_path] + [root / ('SK_Skeleton_' + name + '.skin.json') for name in
                                        ('Helmet', 'Boots', 'Cuirass')]
    input_hashes = {str(p): digest(p) for p in paths}
    source_body, target = json.loads(body_path.read_text()), json.loads(target_path.read_text())
    leaves = {'Body': json.loads((HERE / 'rebind-v1/SK_Skeleton_HumanoidBody.skin.json').read_text())}
    input_hashes[str(HERE / 'rebind-v1/SK_Skeleton_HumanoidBody.skin.json')] = digest(
        HERE / 'rebind-v1/SK_Skeleton_HumanoidBody.skin.json')
    audits = {}
    for label in ('Helmet', 'Boots', 'Cuirass'):
        leaves[label], audits[label] = warp(json.loads((root / ('SK_Skeleton_' + label + '.skin.json')).read_text()),
                                            target, source_body, label)
    output.mkdir()
    checks, batches, bindings = [], [], {}
    variants = dict(Bare=['Body'], Helmet=['Body', 'Helmet'], BootsCuirass=['Body', 'Boots', 'Cuirass'],
                    FullArmor=['Body', 'Boots', 'Cuirass', 'Helmet'])
    for label, skin in [(name + 'Leaf', leaves[name]) for name in ('Helmet', 'Boots', 'Cuirass')] + [
        (name, combine([(leaf, leaves[leaf]) for leaf in selected], target)) for name, selected in variants.items()]:
        write(output / ('SK_Skeleton_' + label + '.skin.json'), skin)
        glb = output / ('SK_Skeleton_' + label + '_Source.glb')
        emission = emit(skin, glb)
        comparison = validate(glb, skin)
        assert all(abs(sum(w['Weight'] for w in v['Influences']) - 1) < 1e-12 for v in skin['lods'][0]['vertices'])
        batches.append(spec_for(skin, label, output))
        bindings[label] = [
            dict(slot=row['MaterialSlotName'], source_slot=row.get('SourceMaterialSlotName', row['MaterialSlotName']),
                 material=material_path(row)) for row in skin['materials']]
        checks.append(dict(label=label, emission=emission, comparison=comparison))
    assert all(digest(p) == value for p, value in input_hashes.items())
    write(output / 'build-batch.json', dict(schema='character-source-mesh-batch-v1', meshes=batches))
    write(output / 'material-bindings.json', bindings)
    write(output / 'mapping-audit.json', audits)
    write(output / 'receipt.json', dict(schema='skeleton-cosmetic-prototype-v1', inputs=input_hashes, checks=checks,
                                        recipes={str(p): digest(p) for p in
                                                 [HERE / 'build_cosmetics.py', HERE / 'build_rebind.py',
                                                  TOOLS / 'character_skin_glb.py',
                                                  TOOLS / 'validate_character_skin.py']},
                                        artifacts=[dict(path=str(p), sha256=digest(p), bytes=p.stat().st_size) for p in
                                                   sorted(output.iterdir())],
                                        limitations=[
                                            'Authored approximate humanoid rebind, not recovered source-equivalent rig behavior.',
                                            'Boots left-limb source bind differs from body; actual per-leaf matrices are used.',
                                            'Toes map to humanoid ball joints as inspected authored forefoot correspondence.',
                                            'Combined presets retain overlapping body and armor geometry; no hidden-body masks authored.',
                                            'Source material identities retained; GLBs contain placeholders, actual bindings need editor/runtime proof.',
                                            'No Unreal import/build/render/deformation or gameplay test performed.',
                                            'Armor publication provenance not independently authenticated in this run.']))
    print(json.dumps(
        [dict(label=c['label'], vertices=c['comparison']['vertices'], triangles=c['comparison']['triangles']) for c in
         checks], indent=2))


if __name__ == '__main__':
    main()

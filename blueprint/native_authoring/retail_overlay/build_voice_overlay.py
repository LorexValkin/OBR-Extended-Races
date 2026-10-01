"""Extend retail combat mappings and provide missing legacy voice paths.

Original race VNAM and dialogue entries stay intact. This candidate adds
Sheogorath keys only to Imperial generic combat responses, and normal female
Dremora keys to responses that already have her alternate voice recordings.
"""
from pathlib import Path
import copy
import hashlib
import json
import struct
import subprocess
import sys
import zlib

HERE = Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
sys.path.insert(0, 'D:/Dump Experiment/FullDump/src')
import query_cache as cache


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def bsa_entries(path):
    with path.open('rb') as stream:
        magic, version, offset, flags, folders, files, folder_bytes, file_bytes, types = struct.unpack('<4s8I',
                                                                                                       stream.read(36))
        if magic != b'BSA\0' or version != 103 or not flags & 3 == 3:
            raise ValueError(f'Unsupported archive contract: {path}')
        stream.seek(offset)
        folder_rows = [struct.unpack('<QII', stream.read(16)) for _ in range(folders)]
        entries = []
        for _, count, _ in folder_rows:
            length = stream.read(1)[0]
            name = stream.read(length).rstrip(b'\0').decode('cp1252').lower()
            entries.extend((name, *struct.unpack('<QII', stream.read(16))[1:]) for _ in range(count))
        names = stream.read(file_bytes).split(b'\0')
        if len(entries) != files or len(names) != files + 1 or names[-1]:
            raise ValueError('BSA filename inventory mismatch')
        return flags, [(folder + '/' + name.decode('cp1252').lower(), size, start)
                       for (folder, size, start), name in zip(entries, names)]


def main():
    root = HERE / 'extended-races-voice-overlay-20261001-v3'
    root.mkdir(exist_ok=False)
    stage = root / 'stage/OblivionRemastered/Content'
    legacy = stage / 'Dev/ObvData/Data'
    legacy.mkdir(parents=True)
    input_data = Path(
        'D:/SteamLibrary/steamapps/common/Oblivion Remastered/OblivionRemastered/Content/Dev/ObvData/Data')
    copied = []
    info_targets = {}
    input_archives = {}
    for archive in [input_data / 'Oblivion - Voices1.bsa', input_data / 'Oblivion - Voices2.bsa']:
        flags, entries = bsa_entries(archive)
        with archive.open('rb') as source:
            for name, packed_size, offset in entries:
                name = name.replace('\\', '/')
                target = None
                mode = None
                if '/imperial/' in name and '/altvoice/' not in name and '/beggar/' not in name:
                    filename = name.rsplit('/', 1)[-1]
                    if filename.startswith(('generic_attack_', 'generic_hit_', 'generic_powerattack_')):
                        target = name.replace('/imperial/', '/sheogorath/')
                        mode = 'Sheogorath'
                elif '/dremora/f/altvoice/' in name:
                    target = name.replace('/dremora/f/altvoice/', '/dremora/f/')
                    mode = 'DremoraFemale'
                if target is None or not name.endswith(('.mp3', '.lip')):
                    continue
                relative = Path(target)
                if relative.is_absolute() or '..' in relative.parts:
                    raise ValueError('Archive member escapes stage')
                source.seek(offset)
                payload = source.read(packed_size & 0x3fffffff)
                if flags & 0x100:
                    payload = payload[1 + payload[0]:]
                if bool(flags & 4) != bool(packed_size & 0x40000000):
                    expected = struct.unpack_from('<I', payload)[0]
                    payload = zlib.decompress(payload[4:])
                    if len(payload) != expected:
                        raise ValueError('BSA member decompression size mismatch')
                destination = legacy / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                if destination.exists() and destination.read_bytes() != payload:
                    raise ValueError('Conflicting donor voice members')
                destination.write_bytes(payload)
                copied.append({'path': target, 'source': name, 'archive': archive.name,
                               'offset': offset, 'sha256': sha(destination), 'bytes': len(payload)})
                info_id = name.rsplit('/', 1)[-1].rsplit('_', 2)[-2]
                if len(info_id) != 8 or any(c not in '0123456789abcdef' for c in info_id):
                    raise ValueError(f'Unexpected response filename: {name}')
                info_targets.setdefault(info_id, set()).add(mode)
        input_archives[archive.name] = sha(archive)
    if not copied or not info_targets:
        raise RuntimeError('No donor voice recordings found')
    print(f'Legacy donor readback: {len(copied)} files, {len(info_targets)} INFO identities', flush=True)
    cooked = Path('D:/OBR-Retail/Cooked/OblivionRemastered/Content/Forms/miscellaneous/dialog')
    by_id = {p.stem.rsplit('_', 1)[-1].lower(): p for p in cooked.glob('*.uasset')}
    patches = []
    db = cache.database()
    try:
        for info_id, modes in sorted(info_targets.items()):
            source_path = by_id.get(info_id)
            if source_path is None:
                raise RuntimeError(f'Missing cooked INFO: {info_id}')
            owner = source_path.relative_to('D:/OBR-Retail/Cooked').as_posix()
            rows = db.execute(
                f'SELECT {cache.DOCUMENT_FIELDS} FROM documents INDEXED BY doc_name WHERE name=? COLLATE NOCASE AND kind=? AND owner=?',
                (source_path.stem, 'asset', owner)).fetchall()
            if len(rows) != 1:
                raise RuntimeError(f'Ambiguous captured INFO: {owner}')
            document = dict(rows[0])
            asset = cache.payload(document)
            original = copy.deepcopy(asset)
            additions = 0
            for export in asset['Exports']:
                if not isinstance(export.get('Data'), list):
                    continue
                for prop in export['Data']:
                    if prop['Name'] != 'Responses':
                        continue
                    for response in prop['Value']:
                        for mapping in response['Value']:
                            if mapping.get('Name') not in ('AkAudioEvents', 'Animations'):
                                continue
                            entries = mapping['Value']
                            for key, value in list(entries):
                                if key.get('StructType') != 'VResponseKey':
                                    raise RuntimeError('Unexpected voice-map key schema')
                                parts = {p['Name']: p for p in key['Value']}
                                race = parts['Race']['Value']['AssetPath']['AssetName']
                                voice = parts['VoiceType']['Value']
                                sex = parts['Sex']['Value']
                                mode = (
                                    'Sheogorath' if race == 'Imperial' and voice == 'LEGACY' and 'Sheogorath' in modes
                                    else 'DremoraFemale' if race == 'Dremora' and sex == 'FEMALE' and voice == 'ALTVOICE' and 'DremoraFemale' in modes else None)
                                if mode is None:
                                    continue
                                new_key = copy.deepcopy(key)
                                fields = {p['Name']: p for p in new_key['Value']}
                                if mode == 'Sheogorath':
                                    fields['Race']['Value']['AssetPath'][
                                        'PackageName'] = '/Game/Forms/actors/race/Sheogorath'
                                    fields['Race']['Value']['AssetPath']['AssetName'] = 'Sheogorath'
                                else:
                                    fields['VoiceType']['Value'] = 'LEGACY'
                                    fields['VoiceType']['IsZero'] = True
                                if any(pair[0]['Value'] == new_key['Value'] for pair in entries):
                                    raise RuntimeError('Target voice mapping already exists')
                                entries.append([new_key, copy.deepcopy(value)])
                                additions += 1
            if additions == 0:
                raise RuntimeError(f'No matching cooked donor mapping: {owner}')
            for name in ('/Game/Forms/actors/race/Sheogorath', 'Sheogorath'):
                if 'Sheogorath' in modes and name not in asset['NameMap']:
                    asset['NameMap'].append(name)
            asset['NamesReferencedFromExportDataCount'] = len(asset['NameMap'])
            json_path = root / (source_path.stem + '.json')
            json_path.write_text(json.dumps(asset), encoding='utf-8')
            output = stage / source_path.relative_to('D:/OBR-Retail/Cooked/OblivionRemastered/Content')
            output.parent.mkdir(parents=True, exist_ok=True)
            result = subprocess.run(
                ['dotnet', str(HERE / 'OverlayWriter/bin/Debug/net8.0/OverlayWriter.dll'), str(json_path), str(output),
                 'D:/Dump Experiment/FullDump/dependencies/mapping.usmap'], capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
            readback = json.loads(Path(str(output) + '.readback.json').read_text())
            if readback['Exports'][0]['Data'] != asset['Exports'][0]['Data']:
                raise RuntimeError(f'Voice mapping readback mismatch: {owner}')
            # All original properties and every original map entry must survive.
            patches.append({'owner': owner, 'captureSha256': document['sha256'], 'record': document['id'],
                            'sourceSha256': sha(source_path), 'addedKeys': additions,
                            'uassetSha256': sha(output), 'uexpSha256': sha(output.with_suffix('.uexp'))})
            Path(str(output) + '.readback.json').rename(root / (source_path.stem + '.readback.json'))
            print(f'{source_path.stem}: added {additions} keys', flush=True)
    finally:
        db.close()
    receipt = {'schema': 'extended-races-voice-overlay-v1', 'status': 'offline-candidate-unverified-in-retail',
               'archives': input_archives, 'legacyFiles': copied, 'packages': patches,
               'remaining': [
                   'Package IoStore and loose voice paths, then test hit/power attack and NPC dialogue in retail.',
                   'This overlay does not alter or solve native character-creation Confirm.']}
    (root / 'voice-overlay-manifest.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    print(f'Verified {len(patches)} cooked response packages and {len(copied)} legacy files')


if __name__ == '__main__':
    main()

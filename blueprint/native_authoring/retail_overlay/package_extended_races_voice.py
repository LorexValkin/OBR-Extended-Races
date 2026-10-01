"""Combine verified core, UML Blueprint and voice overlay into a fresh archive."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import zipfile

HERE = Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
voice = HERE / 'extended-races-voice-overlay-20261001-v3'
core = HERE / 'extended-races-core-20261001-v3/candidate'
root = HERE / 'extended-races-uml-overlay-candidate-20261001-v2'
root.mkdir(exist_ok=False)
container_stage = root / 'container-stage/OblivionRemastered/Content/Forms/miscellaneous/dialog'
container_stage.mkdir(parents=True)
receipt = json.loads((voice / 'voice-overlay-manifest.json').read_text())
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
for item in receipt['packages']:
    name = Path(item['owner']).name
    source = voice / 'stage' / item['owner']
    if digest(source) != item['uassetSha256'] or digest(source.with_suffix('.uexp')) != item['uexpSha256']:
        raise RuntimeError('Changed voice package input')
    for member in (source, source.with_suffix('.uexp')):
        shutil.copy2(member, container_stage / member.name)
retoc = 'C:/Users/User/Desktop/Projects/Oblivion-Remaster-Mod-Updater-Tool/third_party/retoc/retoc.exe'
packed = root / 'packed'
packed.mkdir()
utoc = packed / 'zz_ExtendedRacesVoices_P.utoc'
for args, log in [([retoc, 'to-zen', '--version', 'UE5_3', str(root / 'container-stage'), str(utoc)], 'build.log'),
                  ([retoc, 'verify', str(utoc)], 'verify.log'),
                  ([retoc, 'manifest', str(utoc)], 'manifest.log')]:
    result = subprocess.run(args, cwd=root, capture_output=True, text=True)
    (root / log).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'Container operation failed; see {log}')
manifest = json.loads((root / 'pakstore.json').read_text())
actual = {entry['packagestoreentry']['packagename'] for entry in manifest['oplog']['entries']}
expected = {'/Game/Forms/miscellaneous/dialog/' + Path(item['owner']).stem for item in receipt['packages']}
if actual != expected:
    raise RuntimeError('Packed response identity mismatch')
candidate = root / 'candidate'
candidate.mkdir()
files = []
def include(source, relative, expected_hash=None):
    source_hash = digest(source)
    if expected_hash and source_hash != expected_hash:
        raise RuntimeError(f'Changed input: {source}')
    destination = candidate / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        raise RuntimeError('Duplicate release path')
    shutil.copy2(source, destination)
    if digest(destination) != source_hash:
        raise RuntimeError('Copy verification failed')
    files.append({'path': Path(relative).as_posix(), 'bytes': destination.stat().st_size, 'sha256': source_hash})
for item in json.loads((core / 'candidate-manifest.json').read_text())['files']:
    include(core / item['path'], item['path'], item['sha256'])
for source in sorted(packed.glob('zz_ExtendedRacesVoices_P.*')):
    include(source, Path('OblivionRemastered/Content/Paks/~mods') / source.name)
for item in receipt['legacyFiles']:
    relative = Path('OblivionRemastered/Content/Dev/ObvData/Data') / item['path']
    include(voice / 'stage' / relative, relative, item['sha256'])
manifest = {'schema': 'extended-races-uml-overlay-candidate-v1', 'status': 'offline-candidate-needs-retail-acceptance',
            'files': files, 'raceRowsVerified': 14, 'voicePackagesVerified': len(actual),
            'voiceFilesVerified': len(receipt['legacyFiles']),
            'runtimeLoader': 'Installed UML; loader excluded from this archive',
            'requiredRuntimeCapability': 'Existing native fourteen-race Confirm registration. A pak does not alter the compiled C++ race lookup.',
            'excluded': ['UE4SS', 'UNBSE', 'Horns side-mod assets'],
            'remaining': ['Retail load, race Confirm, hit/power-attack voices, first-person visuals and save/load testing.',
                          'The separate Body Guard compatibility logic has not been ported.'],
            'voiceReceiptSha256': digest(voice / 'voice-overlay-manifest.json')}
(candidate / 'candidate-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
readme = '''Extended Races UML overlay candidate - 2026-10-01

Offline candidate; this is not an in-game compatibility claim.
Install through the mod manager with the existing UML installed separately.
This archive does not provide a native race-map patch. The existing native
fourteen-race Confirm registration remains required for character creation.
Horns are excluded. Body Guard third-party body compatibility is not ported.

The voice package extends existing response maps without removing original
keys. Loose legacy voice files supply the paths checked before Wwise playback.
Sheogorath borrows Imperial generic combat sounds; his original dialogue and
shared race VNAM remain intact. Female Dremora can use her recordings without
depending on the player alternate-voice faction writer.

Test all four races and both sexes, all vanilla race Confirm selections,
hit/power attacks, Sheogorath NPC dialogue, bare arms/gloves near walls,
appearance rebuild, inventory doll, save/reload and travel before promotion.
Keep this candidate separate from older Extended Races and Lua skin writers;
do not run two skin writers during visual acceptance.
'''
(candidate / 'README.txt').write_text(readme, encoding='utf-8')
archive = root / 'ExtendedRaces-UML-Overlay-Candidate-20261001.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
    for item in files:
        output.write(candidate / item['path'], item['path'])
    for name in ('candidate-manifest.json', 'README.txt'):
        output.write(candidate / name, name)
with zipfile.ZipFile(archive) as output:
    if output.testzip() is not None:
        raise RuntimeError('Archive CRC failed')
    for item in files:
        if hashlib.sha256(output.read(item['path'])).hexdigest() != item['sha256']:
            raise RuntimeError('Archive payload hash failed')
print(json.dumps({'archive': str(archive), 'filesVerified': len(files), 'responsePackages': len(actual),
                  'bytes': archive.stat().st_size, 'sha256': digest(archive)}, indent=2))

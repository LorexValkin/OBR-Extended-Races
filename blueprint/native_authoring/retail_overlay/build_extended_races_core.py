"""Build the race-only retail overlay and combine it with the UML skin slice.

All outputs stay in this authoring workspace. Existing sources and retail
inputs are read-only; the horns side-mod packages are excluded explicitly.
"""
from pathlib import Path
import base64
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile

HERE = Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / 'tools'))
import build_pak
from obrpkg import Package, Usmap
from obrpkg.unversioned import Reader


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, log):
    result = subprocess.run([str(x) for x in args], cwd=HERE, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log.write_text(result.stdout, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{args[0]} failed: {result.returncode}; see {log}')
    return result.stdout


def main():
    root = HERE / 'extended-races-core-20261001-v3'
    root.mkdir(exist_ok=False)
    edited = root / 'edited'
    edited.mkdir()
    table = 'DT_RaceSex_AllRacesModificationProperties'
    source = REPO / '.work/ui/json' / (table + '.json')
    target = edited / (table + '.json')
    for race in build_pak.NEW_RACES:
        race.pop('table', None)
    sys.argv = ['build_pak.py', '--json', str(source), '--out', str(target)]
    if build_pak.main() != 0:
        raise RuntimeError('Race table authoring failed')
    pkg = Package(str(target), Usmap(build_pak.USMAP))
    data = pkg.export_data(pkg.main_export())
    start, count = build_pak.row_payload_start(pkg, pkg.main_export(), data)
    reader = Reader(data)
    reader.p = start + 4
    rows = {}
    for _ in range(count):
        name = pkg.ctx.name(reader.u32(), reader.u32())
        values = pkg.decoder._read_block(pkg.usmap.flatten(build_pak.ROW_STRUCT), reader)
        rows[name] = values
    if count != 14 or reader.p != len(data):
        raise RuntimeError('Incomplete race table readback')
    for row in rows.values():
        if row.get('RaceId', 0) != build_pak.RACE_ORDER[row['RaceName']]:
            raise RuntimeError('Race ordinal mismatch')
    for race in build_pak.NEW_RACES:
        if rows[race['row']][
            'Table'] != '/Game/UI/Legacy/GameMenuLayer/RaceSex/DT_RaceSex_Imperial/DT_RaceSex_Imperial':
            raise RuntimeError(f'Unexpected customization table: {rows[race["row"]]["Table"]}')
    if any('DT_RaceSex_Dremora' in name for name in pkg.names):
        raise RuntimeError('Horns table leaked into the core race grid')
    sheo = REPO / '.work/pak/edited/Sheogorath.json'
    shutil.copy2(sheo, edited / 'Sheogorath.json')
    stage = root / 'stage/OblivionRemastered/Content'
    gui = Path('C:/Users/User/Desktop/Projects/UE5 Oblivion/tools/UAssetGUI/UAssetGUI.exe')
    retoc = Path('C:/Users/User/Desktop/Projects/Oblivion-Remaster-Mod-Updater-Tool/third_party/retoc/retoc.exe')
    mounted = {table: 'UI/Legacy/GameMenuLayer/RaceSex', 'Sheogorath': 'Forms/actors/race'}
    for name, parent in mounted.items():
        folder = stage / parent
        folder.mkdir(parents=True, exist_ok=True)
        run([gui, 'fromjson', edited / (name + '.json'), folder / (name + '.uasset'), 'VER_UE5_3'],
            root / (name + '-convert.log'))
        if not (folder / (name + '.uasset')).exists():
            raise RuntimeError('Missing converted asset')
    packed = root / 'packed'
    packed.mkdir()
    utoc = packed / 'zz_ExtendedRaces_P.utoc'
    run([retoc, 'to-zen', '--version', 'UE5_3', root / 'stage', utoc], root / 'retoc-build.log')
    run([retoc, 'verify', utoc], root / 'retoc-verify.log')
    result = subprocess.run([str(retoc), 'manifest', str(utoc)], cwd=root,
                            capture_output=True, text=True)
    (root / 'retoc-manifest.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError('Container manifest extraction failed')
    source_candidate = REPO / '.work/extended-races-uml-candidate-20261001'
    original = json.loads((source_candidate / 'candidate-manifest.json').read_text())
    release = root / 'candidate'
    files = []
    for item in original['files']:
        relative = Path(item['path'])
        old = source_candidate / relative
        if digest(old) != item['sha256'] or old.stat().st_size != item['bytes']:
            raise RuntimeError(f'Changed prior input: {relative}')
        source_file = packed / relative.name if relative.name.startswith('zz_ExtendedRaces_P.') else old
        destination = release / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_file, destination)
        files.append({'path': relative.as_posix(), 'bytes': destination.stat().st_size, 'sha256': digest(destination)})
    receipt = {'schema': 'extended-races-core-candidate-v1', 'status': 'candidate-needs-retail-acceptance',
               'excluded': 'Horns side-mod assets and runtime', 'raceRows': 14,
               'raceOrdinals': build_pak.RACE_ORDER, 'files': files,
               'retailOverrides': ['/Game/' + parent + '/' + name for name, parent in mounted.items()],
               'sourceInputs': {str(source): digest(source), str(sheo): digest(sheo)},
               'remaining': ['Retail Confirm needs the existing native fourteen-race registration or a proven bypass.',
                             'Body compatibility and player voice conversion remain unfinished.',
                             'Retail UML discovery, rendering, dialogue and save/load acceptance remain untested.']}
    (release / 'candidate-manifest.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    archive = root / 'ExtendedRaces-UML-core-candidate-20261001.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for item in files:
            output.write(release / item['path'], item['path'])
        output.write(release / 'candidate-manifest.json', 'candidate-manifest.json')
    with zipfile.ZipFile(archive) as check:
        if check.testzip() is not None:
            raise RuntimeError('Archive CRC failed')
        for item in files:
            if hashlib.sha256(check.read(item['path'])).hexdigest() != item['sha256']:
                raise RuntimeError('Archive payload hash failed')
    print(json.dumps({'archive': str(archive), 'filesVerified': len(files), 'raceRowsVerified': count,
                      'sha256': digest(archive)}, indent=2))


if __name__ == '__main__':
    main()

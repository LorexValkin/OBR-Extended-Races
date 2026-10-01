"""Compile-check the mod-owned authoring source with UBT's existing response file."""
from pathlib import Path
import os
import re
import subprocess
import json
import sys

repo = Path(__file__).resolve().parents[3]
scratch = repo / '.work/extended-races-bp-authoring/compile-skin-20261001'
scratch.mkdir(exist_ok=True)
lock = Path('D:/Unblivion Editor/Manifests/PerfEditor.lock')
tag = f'ExtendedRacesCompile {os.getpid()}'
with lock.open('x', encoding='utf-8') as stream:
    stream.write(tag)
try:
    folder = Path('D:/Unblivion Editor/Editor/Intermediate/Build/Win64/x64/UnrealEditor/Development/LongswordPilot')
    source = repo / 'blueprint/native_authoring/ExtendedRacesSkinCommandlet.cpp'
    link = '--link' in sys.argv
    if link:
        status = subprocess.check_output(['git', 'status', '--porcelain', '--', 'Plugins', 'Source'], cwd='D:/Unblivion Editor/Editor', text=True)
        if status.strip():
            raise RuntimeError('Canonical module inputs have uncommitted changes')
        unity = (folder / 'Module.LongswordPilot.3.cpp').read_text()
        unity = unity.replace('D:/Unblivion Editor/Editor/Source/LongswordPilot/ExtendedRacesSkinCommandlet.cpp', source.as_posix())
        source = scratch / 'Module.LongswordPilot.3.cpp'
        source.write_text(unity)
    rsp = (folder / 'Module.LongswordPilot.3.cpp.obj.rsp').read_text()
    lines = rsp.splitlines()
    lines[0] = f'"{source}"'
    rsp = '\n'.join(lines) + '\n'
    rsp = re.sub(r'/Fo"[^"]+"', f'/Fo"{(scratch / "skin.obj").as_posix()}"', rsp)
    rsp = re.sub(r'/sourceDependencies "[^"]+"', f'/sourceDependencies "{(scratch / "skin.dep.json").as_posix()}"', rsp)
    response = scratch / 'skin.rsp'
    response.write_text(rsp)
    compiler = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe')
    result = subprocess.CompletedProcess([], 0, '', '') if '--link-only' in sys.argv else subprocess.run([str(compiler), '@' + str(response)], cwd='F:/UE_5.3.2-src/Engine/Source', capture_output=True, text=True)
    (scratch / 'compile.log').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr)
    if link and result.returncode == 0:
        response_text = (folder / 'UnrealEditor-LongswordPilot.dll.rsp').read_text()
        old_object = str(folder / 'Module.LongswordPilot.3.cpp.obj')
        response_text = response_text.replace(old_object, str(scratch / 'skin.obj'))
        for option, filename in [('OUT', 'UnrealEditor-LongswordPilot.dll'), ('IMPLIB', 'UnrealEditor-LongswordPilot.lib'), ('PDB', 'UnrealEditor-LongswordPilot.pdb')]:
            response_text = re.sub('/' + option + r':"[^"]+"', lambda match: '/' + option + ':"' + str(scratch / filename) + '"', response_text)
        link_response = scratch / 'link.rsp'
        link_response.write_text(response_text)
        linker = compiler.with_name('link.exe')
        environment = os.environ.copy()
        environment['PATH'] = 'C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64;' + environment.get('PATH', '')
        if (scratch / 'link.log').exists() and not (scratch / 'link-first.log').exists():
            (scratch / 'link-first.log').write_bytes((scratch / 'link.log').read_bytes())
        result = subprocess.run([str(linker), '@' + str(link_response)], cwd='F:/UE_5.3.2-src/Engine/Source', capture_output=True, text=True, env=environment)
        (scratch / 'link.log').write_text(result.stdout + result.stderr)
        print(result.stdout + result.stderr)
    (scratch / 'receipt.json').write_text(json.dumps({'exitCode': result.returncode, 'object': str(scratch / 'skin.obj')}, indent=2))
    raise SystemExit(result.returncode)
finally:
    if lock.read_text() == tag:
        lock.unlink()

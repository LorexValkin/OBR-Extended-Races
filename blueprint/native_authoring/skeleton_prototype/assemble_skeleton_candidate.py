"""Combine the verified Dremora correction with the removable Skeleton preview."""
from pathlib import Path
import json,hashlib,shutil,zipfile,subprocess
W=Path(__file__).resolve().parents[3] / '.work/extended-races-bp-authoring'
preview=W/'skeleton-preview-candidate-20261001-v1'
base=W/'extended-races-required-horns-20261001-v1/candidate'
O=W/'extended-races-skeleton-test-20261001-v1';O.mkdir()
C=O/'candidate';C.mkdir()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
config=json.loads(Path('D:/Unblivion Editor/Manifests/ExtendedRacesSkeleton20261001V5/preview-cook.json').read_text())
retoc=config['retoc'];utoc=preview/'OblivionRemastered/Content/Paks/~mods/ExtendedRacesSkeletonPrototype_P.utoc'
subprocess.run([retoc,'manifest',str(utoc)],cwd=O,check=True,capture_output=True)
actual={x['packagestoreentry']['packagename'] for x in json.loads((O/'pakstore.json').read_text())['oplog']['entries']}
if actual!=set(config['packages']):raise RuntimeError('Unexpected cooked package membership')
if any(not x.startswith(('/Game/Mods/ExtendedRacesSkeletonPrototype/','/Game/Art/Character/ExtendedRacesSkeleton/','/Game/CharacterReference/SystemImport/ExtendedRacesSkeleton/')) for x in actual):raise RuntimeError('Retail override in prototype')
manifest=json.loads((base/'candidate-manifest.json').read_text())
pm=json.loads((preview/'mod-manifest.json').read_text(encoding='utf-8-sig'))
if pm['cookExit']!=0:raise RuntimeError('Cook failed')
files=[]
for source,items in ((base,manifest['files']),(preview,pm['files'])):
 for item in items:
  src=source/item['path']
  if sha(src).lower()!=item['sha256'].lower():raise RuntimeError(item['path'])
  dst=C/item['path'];dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
  files.append({'path':item['path'],'bytes':dst.stat().st_size,'sha256':sha(dst)})
manifest['files']=files
manifest['skeletonPrototype']={'status':'animated-follower-preview-not-playable-race','cosmetics':['Bare','Helmet','BootsCuirass','FullArmor'],'firstPerson':'Offset owned arms follower; original arms unchanged','runtime':'UML Blueprint actor','packages':sorted(actual),'meshReopenComparisons':9,'cookExit':pm['cookExit'],'retailAcceptance':'pending'}
(C/'candidate-manifest.json').write_text(json.dumps(manifest,indent=2))
(C/'README.txt').write_text('Dremora horns correction plus Skeleton UML preview. Restart game. Four skeletons follow your player to the side in third person. Switch first person to inspect extra skeletal arms offset 8 cm from the normal arms. Walk, sprint, attack, block, cast and equip gloves. This is a deformation preview; Skeleton is not yet selectable as a race. Dremora horn selection and original Argonian horns must be tested separately. To remove only the preview: disable ExtendedRacesSkeletonPrototype.esp and remove ExtendedRacesSkeletonPrototype_P.pak/.ucas/.utoc and ExtendedRacesSkeletonPrototypeDiscovery_P.pak. Keep zz_ExtendedRaces files for the Dremora fix.\n')
archive=O/'ExtendedRaces-Skeleton-Prototype-With-Dremora-Fix.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for p in C.rglob('*'):
  if p.is_file():z.write(p,p.relative_to(C))
with zipfile.ZipFile(archive) as z:
 if z.testzip():raise RuntimeError('ZIP CRC failed')
 for f in files:
  if hashlib.sha256(z.read(f['path'])).hexdigest()!=f['sha256']:raise RuntimeError('ZIP hash failed')
receipt={'archive':str(archive),'sha256':sha(archive),'files':len(files),'prototypePackages':len(actual),'DremoraFixIncluded':True,'noRetailPrototypeOverrides':True}
(O/'verification.json').write_text(json.dumps(receipt,indent=2))
print(json.dumps(receipt,indent=2))

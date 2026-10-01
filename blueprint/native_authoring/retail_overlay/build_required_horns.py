"""Restore required Dremora horns through cooked Blueprint selection, without the optional all-races pack."""
from pathlib import Path
import json,sys,copy,subprocess,hashlib,shutil,zipfile
R=Path(__file__).resolve().parents[3]; W=R/'.work/extended-races-bp-authoring'
sys.path.insert(0,str(R/'tools'))
import build_pak,build_race_table
from obrpkg import Package,Usmap
from obrpkg.unversioned import Reader
PY=sys.executable
O=W/'extended-races-required-horns-20261001-v1';O.mkdir(exist_ok=False)
E=O/'edited';E.mkdir()
def run(cmd,name,cwd=R):
 r=subprocess.run([str(x) for x in cmd],cwd=cwd,capture_output=True,text=True)
 (O/(name+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode: raise RuntimeError(name+' failed')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
run([PY,R/'tools/build_pak.py','--out',E/'DT_RaceSex_AllRacesModificationProperties.json'],'race-grid')
run([PY,R/'tools/build_race_table.py','--out',E/'DT_RaceSex_Dremora.json'],'dremora-table')
run([PY,R/'tools/build_hair_off_material.py','--out-dir',E],'horn-material')
run([PY,R/'tools/build_horn_mesh.py','--out-dir',E],'horn-meshes')
run([PY,R/'tools/build_hair_piece.py','--out-dir',E],'horn-pieces')
shutil.copy2(R/'.work/pak/edited/Sheogorath.json',E/'Sheogorath.json')
us=Usmap(build_pak.USMAP)
orig=Package(str(R/'.work/ui/json/DT_RaceSex_AllRacesModificationProperties.json'),us)
new=Package(str(E/'DT_RaceSex_AllRacesModificationProperties.json'),us)
def rows(p):
 data=p.export_data(p.main_export());start,count=build_pak.row_payload_start(p,p.main_export(),data)
 rd=Reader(data);rd.p=start+4;result={}
 for i in range(count):
  at=rd.p;n=p.ctx.name(rd.u32(),rd.u32());v=p.decoder._read_block(us.flatten(build_pak.ROW_STRUCT),rd)
  result[n]={'values':v,'bytes':data[at:rd.p]}
 assert rd.p==len(data)
 return result
a,b=rows(orig),rows(new)
assert len(b)==14 and a['Argonian']['bytes']==b['Argonian']['bytes']
for name,item in a.items():
 av=copy.deepcopy(item['values']);bv=copy.deepcopy(b[name]['values'])
 av.pop('RaceId',None);bv.pop('RaceId',None)
 assert av==bv,name
assert 'DT_RaceSex_Dremora' in b['Dremora']['values']['Table']
dt=Package(str(E/'DT_RaceSex_Dremora.json'),us)
_,n,spans=build_race_table.walk_rows(dt,dt.export_data(dt.main_export()))
assert 'Dremora_Horns' in spans
# The retail Blueprint keeps its normal dispatch; type 7 additionally uses the
# native hair-piece setter, which selects the piece's eyebrows slot itself.
sys.path.insert(0,'D:/Dump Experiment/FullDump/src')
import query_cache as cache
db=cache.database()
try:
 document=dict(db.execute(f'SELECT {cache.DOCUMENT_FIELDS} FROM documents WHERE id=11274965').fetchone())
finally:
 db.close()
p=cache.payload(document)
(O/'widget-source-identity.json').write_text(json.dumps(document,indent=2))
baseline=copy.deepcopy(p)
def imp(name,cls,outer):
 for i,x in enumerate(p['Imports']):
  if x['ObjectName']==name and x['ClassName']==cls and x['OuterIndex']==outer:return -i-1
 p['Imports'].append({'$type':'UAssetAPI.Import, UAssetAPI','ObjectName':name,'OuterIndex':outer,'ClassPackage':'/Script/CoreUObject','ClassName':cls,'PackageName':None,'bImportOptional':False})
 return -len(p['Imports'])
vm=-7;math=-15;system=-16
update=imp('UpdateHair','Object',vm)
equal=imp('EqualEqual_ByteByte','Object',math)
load=imp('LoadAsset_Blocking','Object',system)
def ex(kind,**kw):return {'$type':'UAssetAPI.Kismet.Bytecode.Expressions.'+kind+', UAssetAPI',**kw}
def ptr(name,owner):return {'$type':'UAssetAPI.Kismet.Bytecode.KismetPropertyPointer, UAssetAPI','New':{'$type':'UAssetAPI.UnrealTypes.FFieldPath, UAssetAPI','Path':[name],'ResolvedOwner':owner}}
def member(name,owner,expr):return ex('EX_StructMemberContext',StructMemberExpression=ptr(name,owner),StructExpression=expr)
f=next(x for x in p['Exports'] if x['ObjectName']=='OnPropertySelected')
old=copy.deepcopy(f['ScriptBytecode'])
toggle=copy.deepcopy(old[0]['ContextExpression']['Parameters'][0])
index=copy.deepcopy(old[0]['ContextExpression']['Parameters'][1])
kind=member('Type',-105,copy.deepcopy(toggle))
opts=member('Options',-105,copy.deepcopy(toggle))
opt=ex('EX_ArrayGetByRef',ArrayVariable=opts,ArrayIndex=copy.deepcopy(index))
hair=member('HairPiece',-107,opt)
load_expr=ex('EX_FinalFunction',StackNode=load,Parameters=[hair])
apply=copy.deepcopy(old[0])
apply['ContextExpression']=ex('EX_FinalFunction',StackNode=update,Parameters=[load_expr,copy.deepcopy(index),ex('EX_True')])
guard=ex('EX_JumpIfNot',CodeOffset=0,BooleanExpression=ex('EX_FinalFunction',StackNode=equal,Parameters=[kind,ex('EX_ByteConst',Value=7)]))
f['ScriptBytecode']=[old[0],guard,apply,old[1],old[2]]
# Names only append, preserving every retail name index.
for name in ('UpdateHair','EqualEqual_ByteByte','LoadAsset_Blocking','Type','Options','HairPiece'):
 if name not in p['NameMap']:p['NameMap'].append(name)
p['NamesReferencedFromExportDataCount']=len(p['NameMap'])
widget='WBP_Modern_CharacterCreation_Toggle'
jp=E/(widget+'.json');jp.write_text(json.dumps(p,indent=2))
S=O/'stage/OblivionRemastered/Content'
mounts={}
for path in E.glob('*.json'):
 name=path.stem
 parent=('UI/Modern/GameMenuLayer/CharacterCreation/Common' if name==widget else
 'UI/Legacy/GameMenuLayer/RaceSex' if name.startswith('DT_') else
 'Dev/Phenotypes/Eyebrows' if name.startswith('HP_') else
 'Dev/Phenotypes/Meshes' if name.startswith('SK_') else
 'Dev/Phenotypes/Materials' if name.startswith('MIC_') else 'Forms/actors/race')
 mounts[name]=parent
 folder=S/parent;folder.mkdir(parents=True,exist_ok=True)
 if name==widget:
  run(['dotnet',W/'HornOverlayWriter/bin/Debug/net8.0/HornOverlayWriter.dll',path,folder/(name+'.uasset'),'D:/Dump Experiment/FullDump/dependencies/mapping.usmap'],name)
 else:
  run(['C:/Users/User/Desktop/Projects/UE5 Oblivion/tools/UAssetGUI/UAssetGUI.exe','fromjson',path,folder/(name+'.uasset'),'VER_UE5_3'],name)
 assert (folder/(name+'.uasset')).exists(),name
rb=json.loads((S/mounts[widget]/(widget+'.uasset.readback.json')).read_text())
for i,x in enumerate(baseline['Exports']):
 if x['ObjectName']!='OnPropertySelected':
  assert x.get('Data')==rb['Exports'][i].get('Data'),x['ObjectName']
  assert x.get('ScriptBytecode')==rb['Exports'][i].get('ScriptBytecode'),x['ObjectName']
newfn=next(x for x in rb['Exports'] if x['ObjectName']=='OnPropertySelected')
assert newfn['ScriptBytecode'][0]==old[0]
assert newfn['ScriptBytecode'][-2:]==old[-2:]
# Readback JSON is evidence only, never a container member.
for path in S.rglob('*.readback.json'):path.unlink()
T='C:/Users/User/Desktop/Projects/Oblivion-Remaster-Mod-Updater-Tool/third_party/retoc/retoc.exe'
pack=O/'packed';pack.mkdir()
utoc=pack/'zz_ExtendedRaces_P.utoc'
run([T,'to-zen','--version','UE5_3',O/'stage',utoc],'retoc-build',O)
run([T,'verify',utoc],'retoc-verify',O)
run([T,'manifest',utoc],'retoc-manifest',O)
actual={x['packagestoreentry']['packagename'] for x in json.loads((O/'pakstore.json').read_text())['oplog']['entries']}
expected={'/Game/'+parent+'/'+name for name,parent in mounts.items()}
assert actual==expected and len(actual)==14
assert not any('Argonian' in x for x in actual)
C=O/'candidate';C.mkdir()
prior=W/'extended-races-uml-overlay-candidate-20261001-v2/candidate'
manifest=json.loads((prior/'candidate-manifest.json').read_text())
for item in manifest['files']:
 src=prior/item['path'];assert sha(src)==item['sha256']
 if Path(item['path']).name.startswith('zz_ExtendedRaces_P.'):src=pack/Path(item['path']).name
 dest=C/item['path'];dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
 item['sha256']=sha(dest);item['bytes']=dest.stat().st_size
manifest['excluded']=['UE4SS','UNBSE','Optional Horns For Everyone pack']
manifest['requiredHorns']={'race':'Dremora','hornSets':4,'femaleSets':1,'selection':'cooked Blueprint UpdateHair; eyebrows type retained','Argonian':'retail race-grid row byte-identical; retail customization assets untouched','retailAcceptance':'pending'}
(C/'candidate-manifest.json').write_text(json.dumps(manifest,indent=2))
(C/'README.txt').write_text('Required Dremora horns restored. Optional horns for other races excluded. Argonian retail options preserved. Horn selection uses cooked Blueprint native UpdateHair, with no UE4SS hook. Test male/female horns, hair/beard preservation, Confirm, save/reload and travel. Skeleton race is separate work, not included.\n')
receipt={'packages':sorted(actual),'ArgonianRowByteIdentical':True,'vanillaRowsPreservedExceptOrdinals':True,'otherWidgetFunctionsPreserved':True,'nativeCall':'UpdateHair','payloadFiles':len(manifest['files'])}
(O/'verification.json').write_text(json.dumps(receipt,indent=2))
z=O/'ExtendedRaces-UML-Required-Horns-20261001.zip'
with zipfile.ZipFile(z,'w',zipfile.ZIP_DEFLATED) as out:
 for path in C.rglob('*'):
  if path.is_file():out.write(path,path.relative_to(C))
with zipfile.ZipFile(z) as check:
 assert check.testzip() is None
 for item in manifest['files']:assert hashlib.sha256(check.read(item['path'])).hexdigest()==item['sha256']
print(json.dumps({'archive':str(z),'sha256':sha(z),**receipt},indent=2))

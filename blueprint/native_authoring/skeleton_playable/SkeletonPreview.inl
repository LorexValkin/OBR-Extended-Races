// Author ordinary shipping Blueprint nodes; this module is never shipped.
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "UVHumanoidHeadComponent.h"
#include "UTESRace.h"
#include "UVCharacterPhenotypePreset.h"
#include "UVCharacterPhenotypeData.h"
#include "K2Node_VariableSet.h"
#include "Kismet/BlueprintMapLibrary.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "UVCharacterBodyPairingComponent.h"

namespace SkeletonPlayable
{
using namespace ExtendedRacesSkin;
UEdGraphPin* Ret(UEdGraphNode* N) { return Pin(N,UEdGraphSchema_K2::PN_ReturnValue); }
UEdGraphPin* Then(UEdGraphNode* N) { return Pin(N,UEdGraphSchema_K2::PN_Then); }
void Exec(UEdGraphPin* E,UEdGraphNode* N) { Wire(E,Pin(N,UEdGraphSchema_K2::PN_Execute)); }
UEdGraphPin* Var(UEdGraph* G,const TCHAR* N) { return Member(G,nullptr,nullptr,N,0,400); }
UK2Node_CallFunction* Invoke(UEdGraph* G,UEdGraphPin* E,const TCHAR* N)
{ auto* C=SelfCall(G,N,400,0); Exec(E,C); return C; }
UEdGraphPin* Set(UEdGraph* G,UEdGraphPin* E,const TCHAR* N,UEdGraphPin* V)
{ auto* C=Node<UK2Node_VariableSet>(G,600,0); C->VariableReference.SetSelfMember(N); C->AllocateDefaultPins(); Exec(E,C); if(V) Wire(V,Pin(C,N)); return Then(C); }
UEdGraphPin* Equal(UEdGraph* G,UEdGraphPin* A,UEdGraphPin* B,UObject* Object=nullptr)
{ auto* C=Call(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ObjectObject"),0,200); Wire(A,Pin(C,TEXT("A"))); if(B) Wire(B,Pin(C,TEXT("B"))); else Pin(C,TEXT("B"))->DefaultObject=Object; return Ret(C); }
UK2Node_MacroInstance* Each(UEdGraph* G,UEdGraphPin* E,UEdGraphPin* Array)
{
 auto* Macros=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros")); check(Macros);
 UEdGraph* Macro=nullptr; for(UEdGraph* M:Macros->MacroGraphs) if(M->GetFName()==TEXT("ForEachLoop")) Macro=M; check(Macro);
 auto* L=Node<UK2Node_MacroInstance>(G,400,0); L->SetMacroGraph(Macro); L->AllocateDefaultPins(); Wire(Array,Pin(L,TEXT("Array"))); Wire(E,Pin(L,TEXT("Exec"))); return L;
}
UEdGraphPin* Hide(UEdGraph* G,UEdGraphPin* E,UEdGraphPin* Target,UEdGraphPin* Value=nullptr)
{ auto* C=Call(G,USceneComponent::StaticClass(),TEXT("SetHiddenInGame"),800,0); Exec(E,C); Wire(Target,Pin(C,UEdGraphSchema_K2::PN_Self)); if(Value) Wire(Value,Pin(C,TEXT("NewHidden"))); else Pin(C,TEXT("NewHidden"))->DefaultValue=TEXT("true"); return Then(C); }
UEdGraphPin* Mesh(UEdGraph* G,UEdGraphPin* Target)
{ auto* C=Call(G,USkeletalMeshComponent::StaticClass(),TEXT("GetSkeletalMeshAsset"),0,200); Wire(Target,Pin(C,UEdGraphSchema_K2::PN_Self)); return Ret(C); }
UEdGraphPin* SetMesh(UEdGraph* G,UEdGraphPin* E,UEdGraphPin* Target,UEdGraphPin* Value,USkeletalMesh* Asset=nullptr)
{ auto* C=Call(G,USkeletalMeshComponent::StaticClass(),TEXT("SetSkinnedAssetAndUpdate"),1000,0); Exec(E,C); Wire(Target,Pin(C,UEdGraphSchema_K2::PN_Self)); if(Value) Wire(Value,Pin(C,TEXT("NewMesh"))); else Pin(C,TEXT("NewMesh"))->DefaultObject=Asset; Pin(C,TEXT("bReinitPose"))->DefaultValue=TEXT("false"); return Then(C); }
UEdGraphPin* Material(UEdGraph* G,UEdGraphPin* Target)
{ auto* C=Call(G,UMeshComponent::StaticClass(),TEXT("GetMaterial"),0,300); Wire(Target,Pin(C,UEdGraphSchema_K2::PN_Self)); return Ret(C); }
UEdGraphPin* SetMaterial(UEdGraph* G,UEdGraphPin* E,UEdGraphPin* Target,UEdGraphPin* Value,UMaterialInterface* Asset=nullptr)
{ auto* C=Call(G,UMeshComponent::StaticClass(),TEXT("SetMaterial"),1200,0); Exec(E,C); Wire(Target,Pin(C,UEdGraphSchema_K2::PN_Self)); if(Value) Wire(Value,Pin(C,TEXT("Material"))); else Pin(C,TEXT("Material"))->DefaultObject=Asset; return Then(C); }
void AddVar(UBlueprint* BP,const TCHAR* Name,UClass* Class,bool Map=false)
{ FEdGraphPinType T; T.PinCategory=UEdGraphSchema_K2::PC_Object; T.PinSubCategoryObject=Class; if(Map){T.ContainerType=EPinContainerType::Map;T.PinValueType.TerminalCategory=UEdGraphSchema_K2::PC_Boolean;} check(FBlueprintEditorUtils::AddMemberVariable(BP,Name,T)); }
bool Save(UObject* Asset)
{ FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()); IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true); Asset->MarkPackageDirty(); FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;return UPackage::SavePackage(Asset->GetOutermost(),Asset,*File,A); }
}

static int32 AuthorSkeletonPreview()
{
 using namespace SkeletonPlayable;
 auto* Rig=LoadObject<USkeleton>(nullptr,TEXT("/Game/Art/Character/Humanoid/SKEL_HumanoidSkeleton.SKEL_HumanoidSkeleton")); check(Rig);
 USkeletalMesh* Arms=nullptr;USkeletalMesh* Body=nullptr;
 for(const TCHAR* Name:{TEXT("HumanoidBody"),TEXT("HumanoidArms")})
 {
  FString P=FString::Printf(TEXT("/Game/Art/Character/ExtendedRacesSkeleton/SK_Skeleton_%s.SK_Skeleton_%s"),Name,Name);
  auto* M=LoadObject<USkeletalMesh>(nullptr,*P); check(M);
  // The rebound mesh must share the humanoid's complete name/parent topology.
  const auto& A=M->GetRefSkeleton();const auto& B=Rig->GetReferenceSkeleton();
  for(int32 I=0;I<A.GetNum();++I){int32 J=B.FindBoneIndex(A.GetBoneName(I));checkf(J!=INDEX_NONE,TEXT("Unmapped bone %s"),*A.GetBoneName(I).ToString());int32 Parent=A.GetParentIndex(I);check(Parent==INDEX_NONE?B.GetParentIndex(J)==INDEX_NONE:B.GetBoneName(B.GetParentIndex(J))==A.GetBoneName(Parent));}
  M->SetSkeleton(Rig);
  auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Creatures/Skeleton/MIC_Skeleton_2.MIC_Skeleton_2"));check(Mat);
  for(auto& Slot:M->GetMaterials()) Slot.MaterialInterface=Mat;
  if(!Save(M)) return 2; if(FString(Name)==TEXT("HumanoidArms")) Arms=M;else Body=M;
 }
 auto* Donor=LoadObject<UTESRace>(nullptr,TEXT("/Game/Forms/actors/race/Imperial.Imperial"));check(Donor);
 auto* Race=LoadObject<UTESRace>(nullptr,TEXT("/Game/Mods/ExtendedRacesSkeleton/Race_Skeleton.Race_Skeleton"));
 if(!Race)Race=DuplicateObject<UTESRace>(Donor,CreatePackage(TEXT("/Game/Mods/ExtendedRacesSkeleton/Race_Skeleton")),TEXT("Race_Skeleton"));check(Race);
 Race->FullName=TEXT("Skeleton");Race->m_formID=0x0c000800;Race->m_formEditorID=TEXT("ExtendedRacesSkeleton");
 for(int32 I=0;I<2;++I)
 {
  const TCHAR* Sex=I?TEXT("f"):TEXT("m");FString Original=FString::Printf(TEXT("/Game/Dev/Phenotypes/PhenotypePreset_Imperial_%s.PhenotypePreset_Imperial_%s"),Sex,Sex);
  auto* Source=LoadObject<UVCharacterPhenotypePreset>(nullptr,*Original);check(Source);
  FString Name=FString::Printf(TEXT("Phenotype_Skeleton_%s"),Sex);FString Package=TEXT("/Game/Mods/ExtendedRacesSkeleton/")+Name;
  auto* Preset=LoadObject<UVCharacterPhenotypePreset>(nullptr,*(Package+TEXT(".")+Name));if(!Preset)Preset=DuplicateObject<UVCharacterPhenotypePreset>(Source,CreatePackage(*Package),*Name);check(Preset&&Preset->PhenotypeData&&Preset->PhenotypeData->IsIn(Preset));
  auto* Data=Preset->PhenotypeData;Data->Hair=nullptr;Data->Eyebrows=nullptr;Data->Beard=nullptr;Data->Mustache=nullptr;if(!Save(Preset))return 2;
  auto& Bodies=I?Race->FemaleFullBodies:Race->MaleFullBodies;check(Bodies.Num()==1);Bodies[0].FullBodySkeletalMesh=Body;Bodies[0].PhenotypePreset=Preset;
 }
 if(!Save(Race))return 2;
 auto* BoneMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Creatures/Skeleton/MIC_Skeleton_2.MIC_Skeleton_2"));check(BoneMaterial);
 const FString Path=TEXT("/Game/Mods/ExtendedRacesSkeleton/BP_SkeletonAppearance");
 if(FPaths::FileExists(FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()))) return 2;
 auto* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),CreatePackage(*Path),TEXT("BP_SkeletonAppearance"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());check(BP);
 BP->BlueprintDescription=TEXT("Skeleton race appearance: native full body; reversible head hiding and first-person arms. Refreshes paired characters every 0.25 seconds.");
 AddVar(BP,TEXT("HiddenHeads"),USceneComponent::StaticClass(),true);
 AddVar(BP,TEXT("BoundArms"),USkeletalMeshComponent::StaticClass());AddVar(BP,TEXT("SavedArms"),USkeletalMesh::StaticClass());AddVar(BP,TEXT("SavedMaterial"),UMaterialInterface::StaticClass());
 AddVar(BP,TEXT("HiddenChests"),USkeletalMeshComponent::StaticClass(),true);
 FunctionGraph RestoreChests(BP,TEXT("RestoreChests")),Chest(BP,TEXT("UpdateChest"));
 Chest.Input(TEXT("Character"),AVPairedCharacter::StaticClass());
 FunctionGraph RestoreHeads(BP,TEXT("RestoreHeads")),HideOne(BP,TEXT("HideOne")),RestoreArms(BP,TEXT("RestoreArms")),ApplyArms(BP,TEXT("ApplyArms")),Process(BP,TEXT("ProcessCharacter")),Update(BP,TEXT("RefreshAppearance"));
 HideOne.Input(TEXT("Component"),USceneComponent::StaticClass());ApplyArms.Input(TEXT("Component"),USkeletalMeshComponent::StaticClass());Process.Input(TEXT("Character"),AVPairedCharacter::StaticClass());
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Initial;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Initial);if(Initial.NumErrors) return 1;
 // A dedicated material section hides ribs without hiding the spine's children.
 auto ShowChest=[&](UEdGraph* G,UEdGraphPin* E,UEdGraphPin* C,UEdGraphPin* Value)
 {
  auto* N=Call(G,USkinnedMeshComponent::StaticClass(),TEXT("ShowMaterialSection"),800,0);
  Exec(E,N);Wire(C,Pin(N,UEdGraphSchema_K2::PN_Self));
  Pin(N,TEXT("MaterialID"))->DefaultValue=TEXT("1");Pin(N,TEXT("SectionIndex"))->DefaultValue=TEXT("1");
  Pin(N,TEXT("LODIndex"))->DefaultValue=TEXT("0");
  if(Value)Wire(Value,Pin(N,TEXT("bShow")));else Pin(N,TEXT("bShow"))->DefaultValue=TEXT("false");
 };
 {
  auto* G=RestoreChests.Graph;auto* Map=Var(G,TEXT("HiddenChests"));
  auto* Keys=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Keys"),0,200);Wire(Map,Pin(Keys,TEXT("TargetMap")));Exec(Then(RestoreChests.Entry),Keys);
  auto* L=Each(G,Then(Keys),Pin(Keys,TEXT("Keys")));auto* C=Pin(L,TEXT("Array Element"));
  auto* V=Branch(G,Pin(L,TEXT("LoopBody")),IsValid(G,C,0,200),200,0);
  auto* Own=Branch(G,V->GetThenPin(),Equal(G,Mesh(G,C),nullptr,Body),400,0);
  auto* Find=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"),600,300);Wire(Map,Pin(Find,TEXT("TargetMap")));Wire(C,Pin(Find,TEXT("Key")));
  ShowChest(G,Own->GetThenPin(),C,Pin(Find,TEXT("Value")));
  auto* Clear=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Clear"),1000,400);Exec(Pin(L,TEXT("Completed")),Clear);Wire(Map,Pin(Clear,TEXT("TargetMap")));
 }
 {
  auto* G=Chest.Graph;auto* C=Pin(Chest.Entry,TEXT("Character"));
  auto* Pair=Member(G,C,AVPairedCharacter::StaticClass(),TEXT("CharacterBodyPairingComponent"),0,200);
  auto* V=Branch(G,Then(Chest.Entry),IsValid(G,Pair,0,200),200,0);
  auto* Form=Call(G,UVCharacterBodyPairingComponent::StaticClass(),TEXT("GetBodyPartForm"),200,200);
  Wire(Pair,Pin(Form,UEdGraphSchema_K2::PN_Self));Pin(Form,TEXT("Slot"))->DefaultValue=TEXT("UpperBody");
  auto* Equipped=Branch(G,V->GetThenPin(),IsValid(G,Ret(Form),400,200),400,0);
  auto* Components=Call(G,AActor::StaticClass(),TEXT("K2_GetComponentsByClass"),400,400);
  Wire(C,Pin(Components,UEdGraphSchema_K2::PN_Self));Pin(Components,TEXT("ComponentClass"))->DefaultObject=USkeletalMeshComponent::StaticClass();Components->PinDefaultValueChanged(Pin(Components,TEXT("ComponentClass")));
  UEdGraphPin* ComponentExec=Equipped->GetThenPin();
  if(Components->FindPin(UEdGraphSchema_K2::PN_Execute)){Exec(ComponentExec,Components);ComponentExec=Then(Components);}
  auto* L=Each(G,ComponentExec,Ret(Components));
  auto* Cast=Node<UK2Node_DynamicCast>(G,600,0);Cast->TargetType=USkeletalMeshComponent::StaticClass();Cast->AllocateDefaultPins();Exec(Pin(L,TEXT("LoopBody")),Cast);Wire(Pin(L,TEXT("Array Element")),Cast->GetCastSourcePin());
  auto* M=Cast->GetCastResultPin();auto* Own=Branch(G,Cast->GetValidCastPin(),Equal(G,Mesh(G,M),nullptr,Body),800,0);
  auto* Shown=Call(G,USkinnedMeshComponent::StaticClass(),TEXT("IsMaterialSectionShown"),800,200);Exec(Own->GetThenPin(),Shown);Wire(M,Pin(Shown,UEdGraphSchema_K2::PN_Self));Pin(Shown,TEXT("MaterialID"))->DefaultValue=TEXT("1");Pin(Shown,TEXT("LODIndex"))->DefaultValue=TEXT("0");
  auto* Add=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Add"),1000,0);Exec(Then(Shown),Add);Wire(Var(G,TEXT("HiddenChests")),Pin(Add,TEXT("TargetMap")));Wire(M,Pin(Add,TEXT("Key")));Wire(Ret(Shown),Pin(Add,TEXT("Value")));
  ShowChest(G,Then(Add),M,nullptr);
 }
 {
  auto* G=RestoreHeads.Graph;auto* Map=Var(G,TEXT("HiddenHeads"));auto* Keys=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Keys"),0,200);Wire(Map,Pin(Keys,TEXT("TargetMap")));
  Exec(Then(RestoreHeads.Entry),Keys);auto* L=Each(G,Then(Keys),Pin(Keys,TEXT("Keys")));auto* Item=Pin(L,TEXT("Array Element"));
  auto* Valid=Branch(G,Pin(L,TEXT("LoopBody")),IsValid(G,Item,600,200),600,0);
  auto* Find=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Find"),600,300);Wire(Map,Pin(Find,TEXT("TargetMap")));Wire(Item,Pin(Find,TEXT("Key")));
  Hide(G,Valid->GetThenPin(),Item,Pin(Find,TEXT("Value")));
  auto* Clear=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Clear"),1000,400);Exec(Pin(L,TEXT("Completed")),Clear);Wire(Map,Pin(Clear,TEXT("TargetMap")));
 }
 {
  auto* G=HideOne.Graph;auto* C=Pin(HideOne.Entry,TEXT("Component"));auto* Valid=Branch(G,Then(HideOne.Entry),IsValid(G,C,0,200),200,0);
  auto* Add=Call(G,UBlueprintMapLibrary::StaticClass(),TEXT("Map_Add"),400,0);Wire(Var(G,TEXT("HiddenHeads")),Pin(Add,TEXT("TargetMap")));Wire(C,Pin(Add,TEXT("Key")));Wire(Member(G,C,USceneComponent::StaticClass(),TEXT("bHiddenInGame"),0,400),Pin(Add,TEXT("Value")));Exec(Valid->GetThenPin(),Add);Hide(G,Then(Add),C);
 }
 {
  auto* G=RestoreArms.Graph;auto* C=Var(G,TEXT("BoundArms"));auto* V=Branch(G,Then(RestoreArms.Entry),IsValid(G,C,0,200),200,0);
  auto* Own=Branch(G,V->GetThenPin(),Equal(G,Mesh(G,C),nullptr,Arms),400,0);
  auto* End=SetMaterial(G,SetMesh(G,Own->GetThenPin(),C,Var(G,TEXT("SavedArms"))),C,Var(G,TEXT("SavedMaterial")));
  // A native refresh may already have replaced the arms; never overwrite it.
  auto* Clear=Set(G,End,TEXT("BoundArms"),nullptr);(void)Clear;
  Set(G,Own->GetElsePin(),TEXT("BoundArms"),nullptr);
 }
 {
  auto* G=ApplyArms.Graph;auto* C=Pin(ApplyArms.Entry,TEXT("Component"));auto* V=Branch(G,Then(ApplyArms.Entry),IsValid(G,C,0,200),200,0);
  auto* Owned=Branch(G,V->GetThenPin(),Equal(G,Mesh(G,C),nullptr,Arms),400,0);
  auto* Restore=Invoke(G,Owned->GetElsePin(),TEXT("RestoreArms"));
  auto* E=Set(G,Then(Restore),TEXT("BoundArms"),C);E=Set(G,E,TEXT("SavedArms"),Mesh(G,C));E=Set(G,E,TEXT("SavedMaterial"),Material(G,C));
  E=SetMesh(G,E,C,nullptr,Arms);SetMaterial(G,E,C,nullptr,BoneMaterial);
 }
 {
  auto* G=Process.Graph;auto* C=Pin(Process.Entry,TEXT("Character"));auto* RaceCall=Call(G,AVPairedCharacter::StaticClass(),TEXT("GetRace"),0,200);Wire(C,Pin(RaceCall,UEdGraphSchema_K2::PN_Self));
  auto* IsSkeleton=Branch(G,Then(Process.Entry),Equal(G,Ret(RaceCall),nullptr,Race),300,0);
  auto* Fit=Invoke(G,IsSkeleton->GetThenPin(),TEXT("UpdateChest"));Wire(C,Pin(Fit,TEXT("Character")));
  auto* Head=Member(G,C,AVPairedCharacter::StaticClass(),TEXT("HumanoidHeadComponent"),0,400);auto* V=Branch(G,Then(Fit),IsValid(G,Head,400,200),600,0);
  auto* H=Invoke(G,V->GetThenPin(),TEXT("HideOne"));Wire(Head,Pin(H,TEXT("Component")));
  auto* Children=Call(G,USceneComponent::StaticClass(),TEXT("GetChildrenComponents"),600,300);Wire(Head,Pin(Children,UEdGraphSchema_K2::PN_Self));Pin(Children,TEXT("bIncludeAllDescendants"))->DefaultValue=TEXT("true");
  auto* L=Each(G,Then(H),Pin(Children,TEXT("Children")));auto* HC=Invoke(G,Pin(L,TEXT("LoopBody")),TEXT("HideOne"));Wire(Pin(L,TEXT("Array Element")),Pin(HC,TEXT("Component")));
 }
 {
  auto* G=Update.Graph;auto* RC=Invoke(G,Then(Update.Entry),TEXT("RestoreChests"));auto* R=Invoke(G,Then(RC),TEXT("RestoreHeads"));
  auto* All=Call(G,UGameplayStatics::StaticClass(),TEXT("GetAllActorsOfClass"),400,0);Exec(Then(R),All);Pin(All,TEXT("ActorClass"))->DefaultObject=AVPairedCharacter::StaticClass();All->PinDefaultValueChanged(Pin(All,TEXT("ActorClass")));
  auto* L=Each(G,Then(All),Pin(All,TEXT("OutActors")));auto* P=Invoke(G,Pin(L,TEXT("LoopBody")),TEXT("ProcessCharacter"));Wire(Pin(L,TEXT("Array Element")),Pin(P,TEXT("Character")));
  auto* Player=Call(G,UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"),800,400);auto* Cast=Node<UK2Node_DynamicCast>(G,1000,0);Cast->TargetType=AVOblivionPlayerCharacter::StaticClass();Cast->AllocateDefaultPins();Exec(Pin(L,TEXT("Completed")),Cast);Wire(Ret(Player),Cast->GetCastSourcePin());
  auto* RaceCall=Call(G,AVPairedCharacter::StaticClass(),TEXT("GetRace"),1000,300);Wire(Cast->GetCastResultPin(),Pin(RaceCall,UEdGraphSchema_K2::PN_Self));auto* IsSkeleton=Branch(G,Cast->GetValidCastPin(),Equal(G,Ret(RaceCall),nullptr,Race),1200,0);
  auto* A=Invoke(G,IsSkeleton->GetThenPin(),TEXT("ApplyArms"));Wire(Member(G,Cast->GetCastResultPin(),AVOblivionPlayerCharacter::StaticClass(),TEXT("FirstPersonSkeletalMeshComponent"),1200,400),Pin(A,TEXT("Component")));
  Invoke(G,IsSkeleton->GetElsePin(),TEXT("RestoreArms"));Invoke(G,Cast->GetInvalidCastPin(),TEXT("RestoreArms"));
 }
 auto* Events=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("AppearanceEvents"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(BP,Events);
 for(const TCHAR* Name:{TEXT("ReceiveBeginPlay"),TEXT("ReceiveTick"),TEXT("ReceiveEndPlay")})
 {auto* E=Node<UK2Node_Event>(Events,0,0);E->EventReference.SetExternalMember(Name,AActor::StaticClass());E->bOverrideFunction=true;E->AllocateDefaultPins();if(FString(Name)==TEXT("ReceiveEndPlay")){auto* RC=Invoke(Events,Then(E),TEXT("RestoreChests"));auto* R=Invoke(Events,Then(RC),TEXT("RestoreHeads"));Invoke(Events,Then(R),TEXT("RestoreArms"));}else Invoke(Events,Then(E),TEXT("RefreshAppearance"));}
 // Give generated graphs a readable grid without changing their topology.
 for(UEdGraph* G:BP->FunctionGraphs){int32 I=0;for(UEdGraphNode* N:G->Nodes){N->NodePosX=(I%6)*380;N->NodePosY=(I/6)*300;++I;}}
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Final;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Final);if(Final.NumErrors||Final.NumWarnings||BP->Status==BS_Error)return 1;
 auto* Defaults=CastChecked<AActor>(BP->GeneratedClass->GetDefaultObject());Defaults->PrimaryActorTick.bCanEverTick=true;Defaults->PrimaryActorTick.bStartWithTickEnabled=true;Defaults->PrimaryActorTick.TickInterval=0.25f;
 // Exercise the generated bytecode on transient components. This checks
 // restoration semantics without claiming a retail animation or menu test.
 FEditorScriptExecutionGuard ScriptExecution;
 auto* World=UWorld::CreateWorld(EWorldType::Editor,false);check(World);
 auto* Manager=World->SpawnActor<AActor>(BP->GeneratedClass);check(Manager);
 auto* Visible=NewObject<USceneComponent>(Manager);auto* Hidden=NewObject<USceneComponent>(Manager);
 Visible->SetHiddenInGame(false);Hidden->SetHiddenInGame(true);
 Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("HideOne")),&Visible);
 Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("HideOne")),&Hidden);
 check(Visible->bHiddenInGame&&Hidden->bHiddenInGame);
 Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("RestoreHeads")),nullptr);
 check(!Visible->bHiddenInGame&&Hidden->bHiddenInGame);
 auto* TestArms=NewObject<USkeletalMeshComponent>(Manager);auto* OriginalMaterial=UMaterial::GetDefaultMaterial(MD_Surface);
 TestArms->SetSkeletalMesh(Body);TestArms->SetMaterial(0,OriginalMaterial);
 Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("ApplyArms")),&TestArms);
 check(TestArms->GetSkeletalMeshAsset()==Arms&&TestArms->GetMaterial(0)==BoneMaterial);
 Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("RestoreArms")),nullptr);
 check(TestArms->GetSkeletalMeshAsset()==Body&&TestArms->GetMaterial(0)==OriginalMaterial);
 Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("ApplyArms")),&TestArms);
 TestArms->SetSkeletalMesh(Body);TestArms->SetMaterial(0,BoneMaterial);
 Manager->ProcessEvent(Manager->FindFunctionChecked(TEXT("RestoreArms")),nullptr);
 check(TestArms->GetSkeletalMeshAsset()==Body&&TestArms->GetMaterial(0)==BoneMaterial);
 World->DestroyWorld(false);
 if(!Save(BP))return 2;UE_LOG(LogTemp,Display,TEXT("SKELETON_PLAYABLE saved=%s compile_errors=0 compile_warnings=0 head_restore=pass arms_restore=pass native_replacement_preserved=pass"),*Path);return 0;
}

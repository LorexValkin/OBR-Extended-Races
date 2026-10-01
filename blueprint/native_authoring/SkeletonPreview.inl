// The prototype owns follower components. Native player meshes stay untouched.
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/SkeletalMesh.h"
#include "AVPairedPawn.h"

static int32 AuthorSkeletonPreview()
{
    using namespace ExtendedRacesSkin;
    const FString Asset=TEXT("/Game/Mods/ExtendedRacesSkeletonPrototype/BP_SkeletonPreview");
    const FString Filename=FPackageName::LongPackageNameToFilename(Asset,FPackageName::GetAssetPackageExtension());
    if (FPaths::FileExists(Filename)) return 2;
    auto* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),CreatePackage(*Asset),TEXT("BP_SkeletonPreview"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!BP) return 2;
    BP->BlueprintDescription=TEXT("Skeleton prototype: four third-person cosmetic followers and a first-person arms follower. No race or player component replacement.");
    const TCHAR* Names[]={TEXT("Bare"),TEXT("Helmet"),TEXT("BootsCuirass"),TEXT("FullArmor"),TEXT("HumanoidArms")};
    for (const TCHAR* Name:Names)
    {
        auto* SCS=BP->SimpleConstructionScript->CreateNode(USkeletalMeshComponent::StaticClass(),FName(Name));
        auto* Component=CastChecked<USkeletalMeshComponent>(SCS->ComponentTemplate);
        const FString MeshPath=FString::Printf(TEXT("/Game/Art/Character/ExtendedRacesSkeleton/SK_Skeleton_%s.SK_Skeleton_%s"),Name,Name);
        auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*MeshPath);
        if (!Mesh) return 2;
        Component->SetSkeletalMesh(Mesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(false);
        Component->SetOnlyOwnerSee(Name==Names[4]);
        BP->SimpleConstructionScript->AddNode(SCS);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Initial;
    FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Initial);
    if (Initial.NumErrors || Initial.NumWarnings) return 1;
    auto* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("PreviewEvents"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    auto* Event=Node<UK2Node_Event>(Graph,0,0);
    Event->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"),AActor::StaticClass());Event->bOverrideFunction=true;Event->AllocateDefaultPins();
    auto* Player=Call(Graph,UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"),0,200);
    auto* Cast=Node<UK2Node_DynamicCast>(Graph,350,0);
    Cast->TargetType=AVOblivionPlayerCharacter::StaticClass();Cast->AllocateDefaultPins();
    Wire(Pin(Event,UEdGraphSchema_K2::PN_Then),Pin(Cast,UEdGraphSchema_K2::PN_Execute));
    Wire(Pin(Player,UEdGraphSchema_K2::PN_ReturnValue),Cast->GetCastSourcePin());
    auto* Main=Member(Graph,Cast->GetCastResultPin(),AVPairedPawn::StaticClass(),TEXT("MainSkeletalMeshComponent"),600,300);
    auto* Arms=Member(Graph,Cast->GetCastResultPin(),AVOblivionPlayerCharacter::StaticClass(),TEXT("FirstPersonSkeletalMeshComponent"),600,500);
    auto* Owner=Call(Graph,AActor::StaticClass(),TEXT("SetOwner"),900,-200);
    Wire(Cast->GetValidCastPin(),Pin(Owner,UEdGraphSchema_K2::PN_Execute));
    Wire(Cast->GetCastResultPin(),Pin(Owner,TEXT("NewOwner")));
    auto* Tick=Node<UK2Node_Event>(Graph,0,-300);
    Tick->EventReference.SetExternalMember(TEXT("ReceiveTick"),AActor::StaticClass());Tick->bOverrideFunction=true;Tick->AllocateDefaultPins();
    Wire(Pin(Tick,UEdGraphSchema_K2::PN_Then),Pin(Cast,UEdGraphSchema_K2::PN_Execute));
    UEdGraphPin* Exec=Pin(Owner,UEdGraphSchema_K2::PN_Then);
    for (int32 I=0;I<5;++I)
    {
        auto* Follower=Member(Graph,nullptr,nullptr,FName(Names[I]),900,I*400);
        auto* Leader=I==4?Arms:Main;
        auto* Attach=Call(Graph,USceneComponent::StaticClass(),TEXT("K2_AttachToComponent"),1200,I*400);
        Wire(Exec,Pin(Attach,UEdGraphSchema_K2::PN_Execute));
        Wire(Follower,Pin(Attach,UEdGraphSchema_K2::PN_Self));Wire(Leader,Pin(Attach,TEXT("Parent")));
        for (FName Rule:{FName(TEXT("LocationRule")),FName(TEXT("RotationRule")),FName(TEXT("ScaleRule"))}) Pin(Attach,Rule)->DefaultValue=TEXT("KeepRelative");
        auto* Offset=Call(Graph,USceneComponent::StaticClass(),TEXT("K2_SetRelativeLocation"),1500,I*400);
        Wire(Pin(Attach,UEdGraphSchema_K2::PN_Then),Pin(Offset,UEdGraphSchema_K2::PN_Execute));Wire(Follower,Pin(Offset,UEdGraphSchema_K2::PN_Self));
        Pin(Offset,TEXT("NewLocation"))->DefaultValue=I==4?TEXT("0,8,0"):FString::Printf(TEXT("%d,0,0"),140*(I+1));
        auto* Pose=Call(Graph,USkinnedMeshComponent::StaticClass(),TEXT("SetLeaderPoseComponent"),1800,I*400);
        Wire(Pin(Offset,UEdGraphSchema_K2::PN_Then),Pin(Pose,UEdGraphSchema_K2::PN_Execute));Wire(Follower,Pin(Pose,UEdGraphSchema_K2::PN_Self));Wire(Leader,Pin(Pose,TEXT("NewLeaderBoneComponent")));
        Pin(Pose,TEXT("bForceUpdate"))->DefaultValue=TEXT("true");
        Exec=Pin(Pose,UEdGraphSchema_K2::PN_Then);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Final;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Final);
    if (Final.NumErrors || Final.NumWarnings || BP->Status==BS_Error) return 1;
    auto* Defaults=CastChecked<AActor>(BP->GeneratedClass->GetDefaultObject());
    Defaults->PrimaryActorTick.bCanEverTick=true;Defaults->PrimaryActorTick.bStartWithTickEnabled=true;Defaults->PrimaryActorTick.TickInterval=1.5f;
    BP->MarkPackageDirty();IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    if (!UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Args)) return 2;
    UE_LOG(LogTemp,Display,TEXT("SKELETON_PREVIEW saved=%s compile_errors=0 compile_warnings=0"),*Asset);
    return 0;
}

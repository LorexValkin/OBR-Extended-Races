#include "ExtendedRacesSkinCommandlet.h"

#include "AVOblivionPlayerCharacter.h"
#include "AVPairedCharacter.h"
#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace ExtendedRacesSkin
{
constexpr TCHAR AssetPath[] = TEXT("/Game/Mods/ExtendedRaces/BP_ExtendedRacesFirstPersonSkin");

template <typename T> T* Node(UEdGraph* Graph, int32 X, int32 Y)
{
    auto* Result = NewObject<T>(Graph);
    Graph->AddNode(Result, false, false);
    Result->CreateNewGuid();
    Result->NodePosX = X;
    Result->NodePosY = Y;
    return Result;
}

UEdGraphPin* Pin(UEdGraphNode* Node, FName Name)
{
    auto* Result = Node->FindPin(Name);
    checkf(Result, TEXT("Missing Extended Races skin pin %s on %s"), *Name.ToString(), *Node->GetName());
    return Result;
}

void Wire(UEdGraphPin* From, UEdGraphPin* To)
{
    checkf(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To),
        TEXT("Extended Races skin graph connection failed: %s -> %s"), *From->PinName.ToString(), *To->PinName.ToString());
}

UK2Node_CallFunction* Call(UEdGraph* Graph, UClass* Owner, FName Name, int32 X, int32 Y)
{
    auto* Function = Owner->FindFunctionByName(Name);
    checkf(Function, TEXT("Missing reflected skin function %s.%s"), *Owner->GetName(), *Name.ToString());
    auto* Result = Node<UK2Node_CallFunction>(Graph, X, Y);
    Result->SetFromFunction(Function);
    Result->AllocateDefaultPins();
    return Result;
}

UK2Node_CallFunction* SelfCall(UEdGraph* Graph, FName Name, int32 X, int32 Y)
{
    auto* Result = Node<UK2Node_CallFunction>(Graph, X, Y);
    Result->FunctionReference.SetSelfMember(Name);
    Result->AllocateDefaultPins();
    return Result;
}

UEdGraphPin* Member(UEdGraph* Graph, UEdGraphPin* Object, UClass* Owner, FName Name, int32 X, int32 Y)
{
    auto* Result = Node<UK2Node_VariableGet>(Graph, X, Y);
    if (Owner)
        Result->VariableReference.SetExternalMember(Name, Owner);
    else
        Result->VariableReference.SetSelfMember(Name);
    Result->AllocateDefaultPins();
    if (Object)
        Wire(Object, Pin(Result, UEdGraphSchema_K2::PN_Self));
    return Pin(Result, Name);
}

UK2Node_IfThenElse* Branch(UEdGraph* Graph, UEdGraphPin* Exec, UEdGraphPin* Condition, int32 X, int32 Y)
{
    auto* Result = Node<UK2Node_IfThenElse>(Graph, X, Y);
    Result->AllocateDefaultPins();
    Wire(Exec, Pin(Result, UEdGraphSchema_K2::PN_Execute));
    Wire(Condition, Result->GetConditionPin());
    return Result;
}

UEdGraphPin* IsValid(UEdGraph* Graph, UEdGraphPin* Object, int32 X, int32 Y)
{
    auto* Result = Call(Graph, UKismetSystemLibrary::StaticClass(), TEXT("IsValid"), X, Y);
    Wire(Object, Pin(Result, TEXT("Object")));
    return Pin(Result, UEdGraphSchema_K2::PN_ReturnValue);
}

UEdGraphPin* Contains(UEdGraph* Graph, UEdGraphPin* Value, const TCHAR* Text, int32 X, int32 Y)
{
    auto* Result = Call(Graph, UKismetStringLibrary::StaticClass(), TEXT("Contains"), X, Y);
    Wire(Value, Pin(Result, TEXT("SearchIn")));
    Pin(Result, TEXT("Substring"))->DefaultValue = Text;
    return Pin(Result, UEdGraphSchema_K2::PN_ReturnValue);
}

UEdGraphPin* ObjectName(UEdGraph* Graph, UEdGraphPin* Object, int32 X, int32 Y)
{
    auto* Result = Call(Graph, UKismetSystemLibrary::StaticClass(), TEXT("GetObjectName"), X, Y);
    Wire(Object, Pin(Result, TEXT("Object")));
    return Pin(Result, UEdGraphSchema_K2::PN_ReturnValue);
}

UEdGraphPin* Equals(UEdGraph* Graph, UEdGraphPin* Value, const TCHAR* Text, int32 X, int32 Y)
{
    auto* Result = Call(Graph, UKismetStringLibrary::StaticClass(), TEXT("EqualEqual_StrStr"), X, Y);
    Wire(Value, Pin(Result, TEXT("A")));
    Pin(Result, TEXT("B"))->DefaultValue = Text;
    return Pin(Result, UEdGraphSchema_K2::PN_ReturnValue);
}

UEdGraphPin* Or(UEdGraph* Graph, UEdGraphPin* A, UEdGraphPin* B, int32 X, int32 Y)
{
    auto* Result = Call(Graph, UKismetMathLibrary::StaticClass(), TEXT("BooleanOR"), X, Y);
    Wire(A, Pin(Result, TEXT("A")));
    Wire(B, Pin(Result, TEXT("B")));
    return Pin(Result, UEdGraphSchema_K2::PN_ReturnValue);
}

struct FunctionGraph
{
    UEdGraph* Graph;
    UK2Node_FunctionEntry* Entry = nullptr;
    FunctionGraph(UBlueprint* Blueprint, FName Name)
    {
        Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph(Blueprint, Graph, true, static_cast<UFunction*>(nullptr));
        for (UEdGraphNode* Candidate : Graph->Nodes)
            if (auto* Found = Cast<UK2Node_FunctionEntry>(Candidate)) Entry = Found;
        check(Entry);
    }
    UEdGraphPin* Input(FName Name, UClass* Type)
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = Type;
        return Entry->CreateUserDefinedPin(Name, PinType, EGPD_Output);
    }
    UEdGraphPin* Integer(FName Name)
    {
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        return Entry->CreateUserDefinedPin(Name, PinType, EGPD_Output);
    }
};

void EmitScaleActor(UBlueprint* Blueprint)
{
    FunctionGraph Function(Blueprint, TEXT("ScaleFirstPersonComponents"));
    auto* Graph = Function.Graph;
    auto* Actor = Function.Input(TEXT("TargetActor"), AActor::StaticClass());
    auto* Valid = Branch(Graph, Pin(Function.Entry, UEdGraphSchema_K2::PN_Then), IsValid(Graph, Actor, 50, 220), 300, 0);
    auto* Components = Call(Graph, AActor::StaticClass(), TEXT("K2_GetComponentsByClass"), 350, 260);
    Wire(Actor, Pin(Components, UEdGraphSchema_K2::PN_Self));
    Pin(Components, TEXT("ComponentClass"))->DefaultObject = USkeletalMeshComponent::StaticClass();
    Components->PinDefaultValueChanged(Pin(Components, TEXT("ComponentClass")));
    auto* Macros = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    check(Macros);
    UEdGraph* ForEach = nullptr;
    for (UEdGraph* Candidate : Macros->MacroGraphs)
        if (Candidate->GetFName() == TEXT("ForEachLoop")) ForEach = Candidate;
    check(ForEach);
    auto* Loop = Node<UK2Node_MacroInstance>(Graph, 650, 0);
    Loop->SetMacroGraph(ForEach);
    Loop->AllocateDefaultPins();
    Wire(Valid->GetThenPin(), Pin(Loop, TEXT("Exec")));
    Wire(Pin(Components, UEdGraphSchema_K2::PN_ReturnValue), Pin(Loop, TEXT("Array")));
    auto* Cast = Node<UK2Node_DynamicCast>(Graph, 900, 0);
    Cast->TargetType = USkeletalMeshComponent::StaticClass();
    Cast->SetPurity(false);
    Cast->AllocateDefaultPins();
    Wire(Pin(Loop, TEXT("LoopBody")), Pin(Cast, UEdGraphSchema_K2::PN_Execute));
    Wire(Pin(Loop, TEXT("Array Element")), Cast->GetCastSourcePin());
    auto* Shadow = Member(Graph, Cast->GetCastResultPin(), UPrimitiveComponent::StaticClass(), TEXT("CastShadow"), 1100, 260);
    auto* NoShadow = Branch(Graph, Cast->GetValidCastPin(), Shadow, 1300, 0);
    auto* Scale = Call(Graph, UPrimitiveComponent::StaticClass(), TEXT("SetBoundsScale"), 1550, 0);
    Wire(NoShadow->GetElsePin(), Pin(Scale, UEdGraphSchema_K2::PN_Execute));
    Wire(Cast->GetCastResultPin(), Pin(Scale, UEdGraphSchema_K2::PN_Self));
    Pin(Scale, TEXT("NewBoundsScale"))->DefaultValue = TEXT("6.0");
    // Only camera-visible rig pieces have their bounds expanded. The engine
    // node calls UpdateBounds and dirties the render transform itself.
    Scale->NodeComment = TEXT("First-person rig only: CastShadow=false keeps the third-person shadow body and gear untouched. SetBoundsScale refreshes render bounds.");
}

void EmitBody(UBlueprint* Blueprint)
{
    FunctionGraph Function(Blueprint, TEXT("ConvertFirstPersonSkin"));
    auto* Graph = Function.Graph;
    auto* Body = Function.Input(TEXT("Body"), USkeletalMeshComponent::StaticClass());
    auto* Slot = Function.Integer(TEXT("SkinSlot"));
    auto* Donor = Function.Input(TEXT("Donor"), UMaterialInterface::StaticClass());
    auto* ValidBody = Branch(Graph, Pin(Function.Entry, UEdGraphSchema_K2::PN_Then), IsValid(Graph, Body, 50, 220), 300, 0);
    auto* NoShadow = Branch(Graph, ValidBody->GetThenPin(), Member(Graph, Body, UPrimitiveComponent::StaticClass(), TEXT("CastShadow"), 340, 230), 600, 0);
    auto* Mesh = Call(Graph, USkeletalMeshComponent::StaticClass(), TEXT("GetSkeletalMeshAsset"), 650, 260);
    Wire(Body, Pin(Mesh, UEdGraphSchema_K2::PN_Self));
    auto* IsBody = Branch(Graph, NoShadow->GetElsePin(), Contains(Graph, ObjectName(Graph, Pin(Mesh, UEdGraphSchema_K2::PN_ReturnValue), 880, 300), TEXT("_Body_"), 1100, 300), 1250, 0);
    auto* Current = Call(Graph, UMeshComponent::StaticClass(), TEXT("GetMaterial"), 1350, 260);
    Wire(Body, Pin(Current, UEdGraphSchema_K2::PN_Self));
    Wire(Slot, Pin(Current, TEXT("ElementIndex")));
    auto* Source = Pin(Current, UEdGraphSchema_K2::PN_ReturnValue);
    auto* HasSource = Branch(Graph, IsBody->GetThenPin(), IsValid(Graph, Source, 1550, 260), 1750, 0);
    auto* Converted = Branch(Graph, HasSource->GetThenPin(), Contains(Graph, ObjectName(Graph, Source, 1800, 300), TEXT("FPSkin"), 2000, 300), 2200, 0);
    auto* HasDonor = Branch(Graph, Converted->GetElsePin(), IsValid(Graph, Donor, 2250, 290), 2450, 0);
    // Keep the component's original material installed until its parameters
    // have been copied. GetMaterial is pure and is evaluated at each consumer.
    auto* Create = Call(Graph, UKismetMaterialLibrary::StaticClass(), TEXT("CreateDynamicMaterialInstance"), 2700, 0);
    Wire(HasDonor->GetThenPin(), Pin(Create, UEdGraphSchema_K2::PN_Execute));
    Wire(Donor, Pin(Create, TEXT("Parent")));
    Pin(Create, TEXT("OptionalName"))->DefaultValue = TEXT("FPSkin");
    auto* Instance = Pin(Create, UEdGraphSchema_K2::PN_ReturnValue);
    auto* HasInstance = Branch(Graph, Pin(Create, UEdGraphSchema_K2::PN_Then), IsValid(Graph, Instance, 2850, 270), 3050, 0);
    auto* Copy = Call(Graph, UMaterialInstanceDynamic::StaticClass(), TEXT("K2_CopyMaterialInstanceParameters"), 3300, 0);
    Wire(HasInstance->GetThenPin(), Pin(Copy, UEdGraphSchema_K2::PN_Execute));
    Wire(Instance, Pin(Copy, UEdGraphSchema_K2::PN_Self));
    Wire(Source, Pin(Copy, TEXT("Source")));
    Pin(Copy, TEXT("bQuickParametersOnly"))->DefaultValue = TEXT("false");
    auto* Enable = Call(Graph, UMaterialInstanceDynamic::StaticClass(), TEXT("SetScalarParameterValue"), 3550, 0);
    Wire(Pin(Copy, UEdGraphSchema_K2::PN_Then), Pin(Enable, UEdGraphSchema_K2::PN_Execute));
    Wire(Instance, Pin(Enable, UEdGraphSchema_K2::PN_Self));
    Pin(Enable, TEXT("ParameterName"))->DefaultValue = TEXT("FPSClippingFix_Enable");
    Pin(Enable, TEXT("Value"))->DefaultValue = TEXT("1.0");
    auto* Assign = Call(Graph, UMeshComponent::StaticClass(), TEXT("SetMaterial"), 3800, 0);
    Wire(Pin(Enable, UEdGraphSchema_K2::PN_Then), Pin(Assign, UEdGraphSchema_K2::PN_Execute));
    Wire(Body, Pin(Assign, UEdGraphSchema_K2::PN_Self));
    Wire(Slot, Pin(Assign, TEXT("ElementIndex")));
    Wire(Instance, Pin(Assign, TEXT("Material")));
    // The first-person material owns the wall correction; copying the race
    // parameters preserves the original skin colour and detail.
    Copy->NodeComment = TEXT("Copy race skin parameters onto the same-sex retail first-person material. Never replace a shadow-casting component.");
}

void EmitApply(UBlueprint* Blueprint)
{
    FunctionGraph Function(Blueprint, TEXT("ApplyFirstPersonSkin"));
    auto* Graph = Function.Graph;
    auto* Player = Call(Graph, UGameplayStatics::StaticClass(), TEXT("GetPlayerCharacter"), 0, 260);
    Pin(Player, TEXT("PlayerIndex"))->DefaultValue = TEXT("0");
    auto* Cast = Node<UK2Node_DynamicCast>(Graph, 250, 0);
    Cast->TargetType = AVOblivionPlayerCharacter::StaticClass();
    Cast->SetPurity(false);
    Cast->AllocateDefaultPins();
    Wire(Pin(Function.Entry, UEdGraphSchema_K2::PN_Then), Pin(Cast, UEdGraphSchema_K2::PN_Execute));
    Wire(Pin(Player, UEdGraphSchema_K2::PN_ReturnValue), Cast->GetCastSourcePin());
    auto* Character = Cast->GetCastResultPin();
    auto* Race = Member(Graph, Character, AVPairedCharacter::StaticClass(), TEXT("Race"), 500, 280);
    auto* RaceName = ObjectName(Graph, Race, 750, 280);
    auto* Dremora = Equals(Graph, RaceName, TEXT("Dremora"), 1000, 250);
    auto* Saint = Equals(Graph, RaceName, TEXT("GoldenSaint"), 1000, 420);
    auto* Seducer = Equals(Graph, RaceName, TEXT("DarkSeducer"), 1000, 590);
    auto* Sheogorath = Equals(Graph, RaceName, TEXT("Sheogorath"), 1000, 760);
    auto* RaceAllowed = Or(Graph, Or(Graph, Dremora, Saint, 1250, 300), Or(Graph, Seducer, Sheogorath, 1250, 650), 1500, 460);
    auto* Allowed = Branch(Graph, Cast->GetValidCastPin(), RaceAllowed, 1750, 0);
    auto* Body = Member(Graph, Character, AVOblivionPlayerCharacter::StaticClass(), TEXT("FirstPersonSkeletalMeshComponent"), 1800, 300);
    auto* Sex = Call(Graph, AVPairedCharacter::StaticClass(), TEXT("GetSex"), 1800, 520);
    Wire(Character, Pin(Sex, UEdGraphSchema_K2::PN_Self));
    auto* Female = Call(Graph, UKismetMathLibrary::StaticClass(), TEXT("EqualEqual_ByteByte"), 2050, 520);
    Wire(Pin(Sex, UEdGraphSchema_K2::PN_ReturnValue), Pin(Female, TEXT("A")));
    Pin(Female, TEXT("B"))->DefaultValue = TEXT("1");
    auto* SexBranch = Branch(Graph, Allowed->GetThenPin(), Pin(Female, UEdGraphSchema_K2::PN_ReturnValue), 2300, 0);
    auto* MaleDonor = Member(Graph, nullptr, nullptr, TEXT("MaleDonor"), 2300, 470);
    auto* FemaleDonor = Member(Graph, nullptr, nullptr, TEXT("FemaleDonor"), 2300, 650);
    auto* MaleSlot = Branch(Graph, SexBranch->GetElsePin(), Saint, 2550, 0);
    auto* FemaleSlot = Branch(Graph, SexBranch->GetThenPin(), Saint, 2550, 470);
    TArray<UEdGraphPin*> Converted;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const bool IsFemale = Index >= 2;
        const bool IsSaint = (Index % 2) == 0;
        auto* InputExec = IsFemale ? (IsSaint ? FemaleSlot->GetThenPin() : FemaleSlot->GetElsePin())
                                   : (IsSaint ? MaleSlot->GetThenPin() : MaleSlot->GetElsePin());
        auto* Convert = SelfCall(Graph, TEXT("ConvertFirstPersonSkin"), 2850, Index * 400);
        Wire(InputExec, Pin(Convert, UEdGraphSchema_K2::PN_Execute));
        Wire(Body, Pin(Convert, TEXT("Body")));
        Pin(Convert, TEXT("SkinSlot"))->DefaultValue = IsSaint ? TEXT("1") : TEXT("0");
        Wire(IsFemale ? FemaleDonor : MaleDonor, Pin(Convert, TEXT("Donor")));
        Converted.Add(Pin(Convert, UEdGraphSchema_K2::PN_Then));
    }
    auto* ScalePlayer = SelfCall(Graph, TEXT("ScaleFirstPersonComponents"), 3150, 0);
    for (UEdGraphPin* Exec : Converted) Wire(Exec, Pin(ScalePlayer, UEdGraphSchema_K2::PN_Execute));
    Wire(Character, Pin(ScalePlayer, TEXT("TargetActor")));
    auto* Attached = Call(Graph, AActor::StaticClass(), TEXT("GetAttachedActors"), 3350, 350);
    Wire(Character, Pin(Attached, UEdGraphSchema_K2::PN_Self));
    Pin(Attached, TEXT("bRecursivelyIncludeAttachedActors"))->DefaultValue = TEXT("false");
    auto* Macros = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    check(Macros);
    UEdGraph* ForEach = nullptr;
    for (UEdGraph* Candidate : Macros->MacroGraphs)
        if (Candidate->GetFName() == TEXT("ForEachLoop")) ForEach = Candidate;
    check(ForEach);
    auto* Loop = Node<UK2Node_MacroInstance>(Graph, 3550, 0);
    Loop->SetMacroGraph(ForEach);
    Loop->AllocateDefaultPins();
    Wire(Pin(ScalePlayer, UEdGraphSchema_K2::PN_Then), Pin(Loop, TEXT("Exec")));
    Wire(Pin(Attached, TEXT("OutActors")), Pin(Loop, TEXT("Array")));
    auto* ScaleAttached = SelfCall(Graph, TEXT("ScaleFirstPersonComponents"), 3800, 0);
    Wire(Pin(Loop, TEXT("LoopBody")), Pin(ScaleAttached, UEdGraphSchema_K2::PN_Execute));
    Wire(Pin(Loop, TEXT("Array Element")), Pin(ScaleAttached, TEXT("TargetActor")));
    // Direct attachments include the player's child equipment actors. Avoid
    // recursively walking actors that may belong to another presentation rig.
    Loop->NodeComment = TEXT("Include first-person equipment child actors after every appearance or equipment rebuild; filter each mesh by CastShadow.");
}

void EmitEvents(UBlueprint* Blueprint)
{
    auto* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("SkinEvents"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
    for (FName EventName : {FName(TEXT("ReceiveBeginPlay")), FName(TEXT("ReceiveTick"))})
    {
        const int32 Y = EventName == TEXT("ReceiveTick") ? 500 : 0;
        auto* Event = Node<UK2Node_Event>(Graph, 0, Y);
        Event->EventReference.SetExternalMember(EventName, AActor::StaticClass());
        Event->bOverrideFunction = true;
        Event->AllocateDefaultPins();
        auto* Apply = SelfCall(Graph, TEXT("ApplyFirstPersonSkin"), 300, Y);
        Wire(Pin(Event, UEdGraphSchema_K2::PN_Then), Pin(Apply, UEdGraphSchema_K2::PN_Execute));
    }
}

void AddDonorVariable(UBlueprint* Blueprint, FName Name)
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Object;
    Type.PinSubCategoryObject = UMaterialInterface::StaticClass();
    check(FBlueprintEditorUtils::AddMemberVariable(Blueprint, Name, Type));
}

void SetDonor(UBlueprint* Blueprint, FName Name, const TCHAR* Path)
{
    auto* Material = LoadObject<UMaterialInterface>(nullptr, Path);
    checkf(Material, TEXT("Missing retail donor %s"), Path);
    auto* Property = FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, Name);
    check(Property);
    Property->SetObjectPropertyValue_InContainer(Blueprint->GeneratedClass->GetDefaultObject(), Material);
}
}

UExtendedRacesSkinCommandlet::UExtendedRacesSkinCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UExtendedRacesSkinCommandlet::Main(const FString& Params)
{
    using namespace ExtendedRacesSkin;
    const FString Asset(AssetPath);
    const FString Name = FPackageName::GetLongPackageAssetName(Asset);
    const FString Filename = FPackageName::LongPackageNameToFilename(Asset, FPackageName::GetAssetPackageExtension());
    if (FPaths::FileExists(Filename))
    {
        UE_LOG(LogTemp, Error, TEXT("Skin Blueprint already exists: %s"), *Filename);
        return 2;
    }
    auto* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), CreatePackage(*Asset), FName(*Name),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!Blueprint) return 2;
    Blueprint->BlueprintDescription = TEXT("Extended Races first-person skin and bounds correction. Touches the player and no-shadow first-person rig only.");
    AddDonorVariable(Blueprint, TEXT("MaleDonor"));
    AddDonorVariable(Blueprint, TEXT("FemaleDonor"));
    EmitScaleActor(Blueprint);
    EmitBody(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog FirstCompile;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &FirstCompile);
    if (FirstCompile.NumErrors || FirstCompile.NumWarnings || Blueprint->Status == BS_Error) return 1;
    EmitApply(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog SecondCompile;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &SecondCompile);
    if (SecondCompile.NumErrors || SecondCompile.NumWarnings || Blueprint->Status == BS_Error) return 1;
    EmitEvents(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog FinalCompile;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &FinalCompile);
    if (FinalCompile.NumErrors || FinalCompile.NumWarnings || Blueprint->Status == BS_Error) return 1;
    SetDonor(Blueprint, TEXT("MaleDonor"), TEXT("/Game/Art/Character/Imperial/MIC_Imperial_Body_m.MIC_Imperial_Body_m"));
    SetDonor(Blueprint, TEXT("FemaleDonor"), TEXT("/Game/Art/Character/Imperial/MIC_Imperial_Body_F.MIC_Imperial_Body_F"));
    auto* Defaults = CastChecked<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    Defaults->PrimaryActorTick.bCanEverTick = true;
    Defaults->PrimaryActorTick.bStartWithTickEnabled = true;
    Defaults->PrimaryActorTick.TickInterval = 1.5f;
    Blueprint->MarkPackageDirty();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, SaveArgs)) return 2;
    UE_LOG(LogTemp, Display, TEXT("EXTENDED_RACES_SKIN saved=%s functions=3 events=2 compile_errors=0 compile_warnings=0"), *Asset);
    return 0;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "Flight/SimCopterAirOperations.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Formats/SimCopterPeopleReader.h"
#include "Ground/SimCopterInteraction.h"

namespace
{
UWorld* MakeAmbientWorld()
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	return World;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCasualtyGawkingTest, "SimCopter.Behavior.World.CasualtyGawking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCasualtyGawkingTest::RunTest(const FString&)
{
	auto* World = MakeAmbientWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	Traffic->ActiveTileSize = 400;
	auto Model = MakeShared<FPeopleBehaviorModel>(); FString Error;
	if (!TestTrue(TEXT("Original people programs load"), FSimCopterPeopleReader::LoadFromFile(
		FSimCopterPeopleReader::ResolvePeoplePath(SimCopterOriginalGame::ResolveRoot()), *Model, Error))) return false;
	auto Person = [&](int32 State, FVector Location)
	{
		auto* P = World->SpawnActor<ASimCopterGroundAgent>(); P->SetOwner(Traffic);
		P->BehaviorModel = Model; P->bBehaviorActive = true; P->BehaviorContext.ResetToState(State);
		P->BehaviorContext.Attributes[EBhavAttr::Visible] = 1;
		P->SetActorLocation(Location + FVector(0,0,P->GetCapsuleHalfHeightCm()));
		Traffic->PedestrianAgents.Add(P); return P;
	};
	auto* Observer = Person(0, FVector(200,-200,0));
	auto* Healthy = Person(0, FVector(230,-200,0));
	auto* Patient = Person(6, FVector(350,-200,0));
	int32 Distance = INDEX_NONE;
	TestTrue(TEXT("Class 15 finds a medical casualty"), Observer->SelectObjectOfClass(Observer->BehaviorContext,15,Distance));
	TestEqual(TEXT("Healthy nearer person is skipped"), Observer->BehaviorContext.SelectedObject.Get(), static_cast<AActor*>(Patient));
	TestFalse(TEXT("Gawking does not claim a patient"), Patient->HospitalPatientClaim.IsValid());
	Patient->BehaviorCarrier = Healthy;
	TestFalse(TEXT("Carried casualties are not gawk targets"), Observer->SelectObjectOfClass(Observer->BehaviorContext,15,Distance));
	Patient->BehaviorCarrier.Reset(); Patient->BehaviorContext.Attributes[EBhavAttr::Visible] = 0;
	TestFalse(TEXT("Invisible casualties are skipped"), Observer->SelectObjectOfClass(Observer->BehaviorContext,15,Distance));
	Patient->BehaviorContext.Attributes[EBhavAttr::Visible] = 1;
	int32 X,Y; Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Patient->GetActorLocation(),X,Y);
	Traffic->XbldTileIds[Y*128+X] = 0xd1;
	TestFalse(TEXT("Hospital casualties are excluded by original class 15"), Observer->SelectObjectOfClass(Observer->BehaviorContext,15,Distance));
	Traffic->XbldTileIds[Y*128+X] = 0x1d;
	Patient->BehaviorContext.ResetToState(0); Patient->BehaviorContext.Attributes[EBhavAttr::WrittenOff] = 1;
	Patient->bMissionPatientDead = true; Patient->bBehaviorActive = false;
	TestTrue(TEXT("Recoverable bodies still count after their VM stops"), Observer->SelectObjectOfClass(Observer->BehaviorContext,15,Distance));
	Patient->SetActorHiddenInGame(true);
	TestFalse(TEXT("Hidden hospital handoffs cannot attract a crowd"), Observer->SelectObjectOfClass(Observer->BehaviorContext,15,Distance));
	Patient->SetActorHiddenInGame(false); Patient->bMissionPatientDead = false; Patient->bBehaviorActive = true;
	Patient->BehaviorContext.ResetToState(6); Patient->BehaviorContext.Attributes[EBhavAttr::WrittenOff] = 0;
	Observer->BehaviorContext.Stack.Reset(); FSimCopterPersonContext::FFrame Frame; Frame.ProgramId = 273;
	Observer->BehaviorContext.Stack.Add(Frame);
	FSimCopterBehaviorVM::Tick(Observer->BehaviorContext,*Model,*Observer);
	TestEqual(TEXT("Shipped gawk program reaches a real target"), Observer->BehaviorContext.SelectedObject.Get(), static_cast<AActor*>(Patient));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterBehaviorAircraftTest, "SimCopter.Behavior.World.CurrentAircraft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterBehaviorAircraftTest::RunTest(const FString&)
{
	auto* World = MakeAmbientWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Nearby = World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Active = World->SpawnActor<ASimCopterHelicopterPawn>(); Active->SetActorLocation(FVector(1000,0,0));
	auto* Helper = World->SpawnActor<ASimCopterHelicopterPawn>(); Helper->GetAirOperations()->SetSupportAircraft(true);
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>();
	auto* PC = World->SpawnActor<APlayerController>(); PC->Player = NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	PC->Possess(Pilot); Pilot->SetParkedHelicopter(Active);
	TestEqual(TEXT("Fixture exposes the on-foot player"), UGameplayStatics::GetPlayerPawn(World,0), static_cast<APawn*>(Pilot));
	auto* Observer = World->SpawnActor<ASimCopterGroundAgent>();
	TestEqual(TEXT("NPC targets the exited helicopter despite closer fleet aircraft"), Observer->ResolvePlayerHelicopter(), Active);
	Pilot->SetParkedHelicopter(Nearby);
	TestEqual(TEXT("Switching active aircraft updates NPC target"), Observer->ResolvePlayerHelicopter(), Nearby);
	PC->Possess(Active);
	TestEqual(TEXT("Possessed aircraft wins"), Observer->ResolvePlayerHelicopter(), Active);
	PC->Possess(Pilot); Pilot->SetParkedHelicopter(nullptr); Pilot->SetOwner(Active);
	TestEqual(TEXT("Older owner-only pilot retains aircraft identity"), Observer->ResolvePlayerHelicopter(), Active);
	Pilot->SetOwner(Helper); Pilot->SetParkedHelicopter(Helper);
	TestNotEqual(TEXT("Support aircraft cannot become the player's NPC target"), Observer->ResolvePlayerHelicopter(), Helper);
	Nearby->SetActorHiddenInGame(true);
	TestEqual(TEXT("Fallback ignores hidden aircraft and support crew"), Observer->ResolvePlayerHelicopter(), Active);
	PC->UnPossess(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterAmbientCuriosityTest, "SimCopter.Behavior.World.HelicopterCuriosity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterAmbientCuriosityTest::RunTest(const FString&)
{
	auto* World = MakeAmbientWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	const auto Box = [&](FVector Location, FVector Extent)
	{
		auto* Actor = World->SpawnActor<AActor>(); auto* Component = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Component); Component->SetBoxExtent(Extent);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionResponseToAllChannels(ECR_Block); Component->RegisterComponent();
		Actor->SetActorLocation(Location); return Actor;
	};
	Box(FVector(0,0,-10),FVector(10000,10000,10));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount); Traffic->ActiveTileSize = 400;
	auto* Aircraft = World->SpawnActor<ASimCopterHelicopterPawn>();
	if (!TestTrue(TEXT("Real helicopter model loads"),Aircraft->SwitchHelicopterModel(4))) return false;
	Aircraft->PlaceOnHelipad(FVector::ZeroVector,0);
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>(); Pilot->SetActorLocation(FVector(0,-2000,40));
	auto* PC = World->SpawnActor<APlayerController>(); PC->Player = NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	PC->Possess(Pilot); Pilot->SetParkedHelicopter(Aircraft);
	const auto Bounds = Aircraft->GetParkingWorldBounds();
	TArray<ASimCopterGroundAgent*> People;
	for (int32 I=0; I<8; ++I)
	{
		auto* P = World->SpawnActor<ASimCopterGroundAgent>(); P->SetOwner(Traffic);
		P->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT(""),SimCopterOriginalGame::ResolveRoot(),85);
		P->BehaviorContext.Attributes[EBhavAttr::AmbientFlag] = 1;
		P->AmbientCuriosityCooldown = 0;
		P->SetActorLocation(FVector(Bounds.Max.X+380+(I/3)*200,(I%3-1)*110,P->GetCapsuleHalfHeightCm()+1));
		Traffic->PedestrianAgents.Add(P); People.Add(P);
		TestTrue(TEXT("Only healthy ordinary Sims are eligible"),P->IsEligibleForAmbientCuriosity());
	}
	for (auto* P : People) P->UpdateOriginalBehavior(1.f/30);
	auto Count = [&]() { int32 N=0; for (auto* P : People) N += P->AmbientCuriositySlot != INDEX_NONE; return N; };
	TestEqual(TEXT("Eight nearby Sims reserve only three visits"),Count(),3);
	if (Count() != 3) { PC->UnPossess(); return false; }
	TArray<ASimCopterGroundAgent*> Visitors;
	for (auto* P : People) if (P->AmbientCuriositySlot != INDEX_NONE) Visitors.Add(P);
	for (auto* P : Visitors)
	{
		TestTrue(TEXT("Visitor stands beyond rotor footprint"), P->AmbientCuriosityGoal.X > Bounds.Max.X+100);
		TestEqual(TEXT("Visitors gather on one side"),P->AmbientCuriositySide,Visitors[0]->AmbientCuriositySide);
		for (int32 Door=0; Door<2; ++Door)
			TestTrue(TEXT("Both boarding doors retain space"),FVector::Dist2D(P->AmbientCuriosityGoal,Aircraft->GetPassengerDropWorldLocation(Door)) > 110);
		for (auto* Other : Visitors) if (P!=Other)
			TestTrue(TEXT("Visit locations leave walking gaps"),FVector::Dist2D(P->AmbientCuriosityGoal,Other->AmbientCuriosityGoal) >= 109);
	}
	const FVector Start = Visitors[0]->GetActorLocation();
	for (int32 Tick=0; Tick<130; ++Tick) for (auto* P : Visitors) P->Tick(1.f/30);
	TestTrue(TEXT("Sims physically approach rather than teleport"),FVector::Dist2D(Start,Visitors[0]->GetActorLocation()) > 150);
	for (auto* P : Visitors) TestTrue(TEXT("Reachable visitor arrives"),P->bAmbientCuriosityArrived);
	for (int32 Tick=0; Tick<150; ++Tick) for (auto* P : Visitors) P->Tick(1.f/30);
	int32 Remarks=0; bool bPeerChat=false;
	for (auto* P : Visitors) { Remarks += P->AmbientCuriosityRemarks; bPeerChat |= P->AmbientCuriosityRemarks > 1; }
	TestTrue(TEXT("Visitors use the original conversation selector"),Remarks >= 3);
	TestTrue(TEXT("Visits include a reply facing another Sim"),bPeerChat);
	TestTrue(TEXT("Visit leaves the original ambient stack intact"),Visitors[0]->BehaviorContext.Stack[0].ProgramId == 600);
	TestFalse(TEXT("Admiring a helicopter never boards it"),Visitors[0]->GetBehaviorCarrier() != nullptr);
	// Move the pilot into a still-active visitor's personal space.
	ASimCopterGroundAgent* Yielding = Visitors.Last();
	Pilot->SetActorLocation(Yielding->GetActorLocation()+FVector(0,40,0));
	Yielding->Tick(1.f/30);
	TestEqual(TEXT("Pilot approach immediately ends the stop"),Yielding->AmbientCuriositySlot,INDEX_NONE);
	TestTrue(TEXT("Yielding Sim cannot immediately rejoin"),Yielding->AmbientCuriosityCooldown > 20);
	Pilot->SetActorLocation(FVector(0,-2000,40));
	for (int32 Tick=0; Tick<300; ++Tick) for (auto* P : Visitors) P->Tick(1.f/30);
	TestEqual(TEXT("All visits expire; crowd cannot remain indefinitely"),Count(),0);

	// Start a fresh controlled visitor for interruption/lifecycle checks.
	for (auto* P : People) { P->StopAmbientCuriosity(); P->SetActorHiddenInGame(true); }
	auto* P = People[0]; P->SetActorHiddenInGame(false);
	const auto Restart = [&]()
	{
		// Each interruption is independent. ResetToState deliberately preserves reaction
		// attributes, so it cannot stand in for spawning a fresh healthy civilian here.
		P->BehaviorContext = FSimCopterPersonContext();
		P->BehaviorContext.ResetToState(0); P->BehaviorContext.Attributes[EBhavAttr::AmbientFlag]=1;
		P->BehaviorContext.Attributes[EBhavAttr::Visible]=1;
		P->BehaviorContext.Attributes[EBhavAttr::RecentlySpooked]=0;
		P->AvoidanceMoveTimeRemainingSeconds=P->AvoidancePathOffsetTimeRemainingSeconds=0;
		P->AmbientCuriosityCooldown=0; P->SetActorLocation(Start);
		P->UpdateOriginalBehavior(1.f/30);
		if (P->AmbientCuriositySlot == INDEX_NONE) AddInfo(FString::Printf(TEXT("Restart: eligible=%d active=%d parked=%d clearance=%f selected=%d pos=%s aircraft=%s depth=%d"),
			P->IsEligibleForAmbientCuriosity(),P->bBehaviorActive,Aircraft->GetFlightModel().State==ESimCopterFlightState::Parked,
			Aircraft->GetLandingSurfaceClearanceCm(),P->ResolvePlayerHelicopter()==Aircraft,*P->GetActorLocation().ToString(),*Aircraft->GetActorLocation().ToString(),P->BehaviorContext.Attributes[EBhavAttr::ReactionDepth]));
		return P->AmbientCuriositySlot != INDEX_NONE;
	};
	TestTrue(TEXT("Fresh visitor starts"),Restart());
	Aircraft->SetActorLocation(Aircraft->GetActorLocation()+FVector(0,0,100));
	P->Tick(1.f/30);
	TestEqual(TEXT("Moving aircraft cancels its gathering"),P->AmbientCuriositySlot,INDEX_NONE);
	Aircraft->PlaceOnHelipad(FVector::ZeroVector,0);
	TestTrue(TEXT("Visitor can start at a safely parked aircraft again"),Restart());
	P->BehaviorContext.PushReactionProgram(916,false); P->Tick(1.f/30);
	TestEqual(TEXT("Original don't-gawk reaction interrupts a visit"),P->AmbientCuriositySlot,INDEX_NONE);
	TestTrue(TEXT("Responder fixture starts visiting"),Restart());
	auto* Responder = People[1]; Responder->SetActorHiddenInGame(false);
	Responder->BehaviorContext.ResetToState(5); Responder->SetActorLocation(P->GetActorLocation()+FVector(0,40,0));
	P->Tick(1.f/30);
	TestEqual(TEXT("A responding medic has priority over curiosity"),P->AmbientCuriositySlot,INDEX_NONE);
	Responder->SetActorHiddenInGame(true);
	TestTrue(TEXT("Casualty fixture starts visiting"),Restart());
	Responder->BehaviorContext.ResetToState(6); Responder->BehaviorContext.Attributes[EBhavAttr::Visible]=1;
	Responder->SetActorHiddenInGame(false); Responder->SetActorLocation(P->GetActorLocation()+FVector(0,200,0));
	for (int32 Tick=0; Tick<20; ++Tick) P->Tick(1.f/30);
	TestEqual(TEXT("Nearby casualty restores the original gawk/rescue priority"),P->AmbientCuriositySlot,INDEX_NONE);
	Responder->SetActorHiddenInGame(true);
	TestTrue(TEXT("Save fixture starts visiting"),Restart());
	TArray<uint8> Save;
	TestTrue(TEXT("Save while approaching"),P->CaptureRuntimeSaveState(Save));
	TestTrue(TEXT("Restore visiting Sim"),P->RestoreRuntimeSaveState(Save));
	TestEqual(TEXT("Save/load releases transient crowd reservation"),P->AmbientCuriositySlot,INDEX_NONE);
	TestEqual(TEXT("Save/load retains the original ambient root"),P->BehaviorContext.Stack[0].ProgramId,600);
	TestTrue(TEXT("Ordinary person resumes after save"),P->bBehaviorActive);
	// An actual wall prevents approaching the front; the same small group may use a clear flank.
	auto* Wall = Box(FVector(Bounds.Max.X+140,0,100),FVector(50,220,100));
	P->SetActorLocation(FVector(0,Bounds.Max.Y+380,P->GetCapsuleHalfHeightCm()+1));
	P->BehaviorContext.Attributes[EBhavAttr::AmbientFlag]=1; P->AmbientCuriosityCooldown=0;
	P->UpdateOriginalBehavior(1.f/30);
	TestTrue(TEXT("Base-facing parked aircraft can attract visitors along a clear flank"),P->AmbientCuriositySlot != INDEX_NONE);
	if (P->AmbientCuriositySlot != INDEX_NONE) TestNotEqual(TEXT("Blocked nose is not selected"),P->AmbientCuriositySide,0);
	Wall->Destroy(); P->StopAmbientCuriosity();
	for (int32 State : {5,6,7,8,11,12,17,18})
	{
		P->BehaviorContext.ResetToState(State); P->BehaviorContext.Attributes[EBhavAttr::AmbientFlag]=1;
		TestFalse(TEXT("Medical, criminal and band behavior never gets borrowed"),P->IsEligibleForAmbientCuriosity());
	}
	P->BehaviorContext.ResetToState(0); P->BehaviorContext.Attributes[EBhavAttr::AmbientFlag]=1;
	P->InitialBehaviorProgramId=1204;
	TestFalse(TEXT("Service mechanics keep their own work"),P->IsEligibleForAmbientCuriosity());
	PC->UnPossess(); return true;
}
#endif

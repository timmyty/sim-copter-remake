#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/SimCopterAudioSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Components/SceneComponent.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterUfoMesh.h"
#include "Ground/SimCopterFlashingLights.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Formats/SimCity2000Reader.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
UWorld* MakePolishWorld()
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	return World;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGameplayPolishTest, "SimCopter.Polish.ControlsAudioAndPeople",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGameplayPolishTest::RunTest(const FString& Parameters)
{
	auto* World = MakePolishWorld();
	auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Controller->Possess(Heli);
	TestTrue(TEXT("Fixture owns a local aircraft"), Heli->IsLocallyControlled());
	Heli->ControllerLeftX(-0.9f);
	Heli->ControllerLeftX(0.0f);
	Heli->ControllerLeftX(0.12f);
	TestEqual(TEXT("Centred stick/drift retain left exit"), Heli->PreferredExitSide, -1.0f);
	Heli->bIsLanded = true;
	Heli->GroundClearanceCm = 0;
	Heli->SetActorLocation(FVector(0, 0, 250));
	Heli->SetActorRotation(FRotator(0, 90, 0));
	Heli->ExitHelicopter();
	TestNotNull(TEXT("Exiting possesses pilot"), Cast<ASimCopterOnFootPawn>(Controller->GetPawn()));
	if (Controller->GetPawn() != Heli)
	{
		TestTrue(TEXT("Pilot exits on airframe left after rotation"), FVector::DotProduct(
			Controller->GetPawn()->GetActorLocation() - Heli->GetActorLocation(), Heli->GetActorRightVector()) < 0);
		APawn* Pilot = Controller->GetPawn();
		Controller->UnPossess();
		Pilot->Destroy();
	}
	Controller->Possess(Heli);
	Heli->MoveRoll(1.0f);
	Heli->MoveRoll(0.0f);
	TestEqual(TEXT("Keyboard horizontal motion also selects right"), Heli->PreferredExitSide, 1.0f);
	Heli->bIsLanded = true;
	Heli->GroundClearanceCm = 0;
	Heli->ExitHelicopter();
	if (Controller->GetPawn() != Heli)
	{
		TestTrue(TEXT("Pilot exits on airframe right"), FVector::DotProduct(
			Controller->GetPawn()->GetActorLocation() - Heli->GetActorLocation(), Heli->GetActorRightVector()) > 0);
		APawn* Pilot = Controller->GetPawn();
		Controller->UnPossess();
		Pilot->Destroy();
	}
	Controller->Possess(Heli);
	auto* Audio = World->GetSubsystem<USimCopterAudioSubsystem>();
	Audio->bSoundsAvailable = true;
	FSimCopterFlightEvents Crash;
	Crash.bCrashed = true;
	Heli->PlayFlightEventAudio(Crash);
	TestTrue(TEXT("Crash starts the rescue cue"), Audio->IsPlaying(SimCopterSound::SND_AMBSRN2));
	Heli->UpdateHelicopterAudio(2.0f);
	TestTrue(TEXT("Crash cue lasts a few seconds"), Audio->IsPlaying(SimCopterSound::SND_AMBSRN2));
	Controller->UnPossess();
	Heli->UpdateHelicopterAudio(1.1f);
	TestFalse(TEXT("Siren stops even after possession changes"), Audio->IsPlaying(SimCopterSound::SND_AMBSRN2));
	Controller->Possess(Heli);
	Heli->PlayFlightEventAudio(Crash);
	Heli->ReturnToAirportAfterCrash();
	TestFalse(TEXT("Respawn always stops the siren even if no airport is available"), Audio->IsPlaying(SimCopterSound::SND_AMBSRN2));
	Controller->Possess(Heli);
	Heli->CurrentFuelGallons = Heli->HelicopterTuning.FuelGallons * 0.11f;
	Heli->UpdateHelicopterAudio(0.016f);
	TestFalse(TEXT("No premature low-fuel warning"), Heli->bLowFuelWarningPlayed);
	Heli->CurrentFuelGallons = Heli->HelicopterTuning.FuelGallons * 0.10f;
	Heli->UpdateHelicopterAudio(0.016f);
	TestTrue(TEXT("Warn at ten percent"), Heli->bLowFuelWarningPlayed);
	const int32 CueCount = Audio->LooseComponents.Num();
	TestTrue(TEXT("Fuel warning creates an audible PCM cue"), CueCount > 0);
	Heli->UpdateHelicopterAudio(0.016f);
	TestEqual(TEXT("Low fuel does not restart every frame"), Audio->LooseComponents.Num(), CueCount);
	Heli->CurrentFuelGallons = Heli->HelicopterTuning.FuelGallons * 0.5f;
	Heli->UpdateHelicopterAudio(0.016f);
	TestFalse(TEXT("Refuelling rearms fuel warning"), Heli->bLowFuelWarningPlayed);

	Controller->Screen = ESimCopterSettingsScreen::Menu;
	Controller->PushPause();
	Controller->HandleApplicationActivationChanged(false);
	Controller->HandleApplicationActivationChanged(false);
	TestEqual(TEXT("Repeated focus loss adds only one pause owner"), Controller->PauseDepth, 2);
	Controller->HandleApplicationActivationChanged(true);
	TestEqual(TEXT("Focus return preserves existing menu pause"), Controller->PauseDepth, 1);
	Controller->PopPause();
	Controller->Screen = ESimCopterSettingsScreen::None;

	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	TArray<ASimCopterGroundAgent*> Survivors;
	TestEqual(TEXT("Five rescue survivors spawn"), Traffic->SpawnMissionSwimmersAtWorldLocation(
		5, FVector(2000, 0, 30), 777, 1, 100, false, &Survivors), 5);
	TSet<FString> Appearances;
	for (auto* Person : Survivors) Appearances.Add(Person->PedestrianFigureName);
	TestEqual(TEXT("All five survivors have distinct civilian figures"), Appearances.Num(), 5);
	if (!Survivors.IsEmpty())
	{
		auto* Person = Survivors[0];
		Person->SetMissionAwaitingRescue(false);
		TestTrue(TEXT("Pedestrian contact accepted"), Person->ApplyVehicleKnockdown(*Heli, FVector(200, 0, 0)));
		TestTrue(TEXT("Visible impact tilt exists before the first animation tick"), FMath::Abs(Person->KnockdownSpinDegrees) >= 16.0f);
		TestFalse(TEXT("Mesh itself is visibly tilted at contact"), Person->VisualRoot->GetRelativeRotation().IsNearlyZero());
		TestEqual(TEXT("Recoil precedes downed animation"), Person->ForcedFigureMnemonic, FString(TEXT("Whoa")));
	}
	if (Survivors.Num() > 1)
	{
		auto* Person = Survivors[1];
		Person->ClearMissionPose();
		Person->InitialPersonState = 0;
		Person->StartOriginalBehavior();
		TestTrue(TEXT("Fatal helicopter contact starts the original reaction"), Person->ApplyHelicopterRunOver(*Heli));
		TestFalse(TEXT("Fatal contact also visibly tilts the figure immediately"), Person->VisualRoot->GetRelativeRotation().IsNearlyZero());
		TestFalse(TEXT("Contact does not skip straight to the death pose"), Person->IsMissionPatientDead());
		Person->EnterKnockdownPhase(ESimCopterKnockdownPhase::Settle);
		Person->UpdateKnockdown(Person->KnockdownSettleSeconds + 0.1f);
		for (int32 Step = 0; Step < 200 && !Person->IsMissionPatientDead(); ++Step)
			Person->UpdateOriginalBehavior(0.05f);
		TestTrue(TEXT("The queued fatal reaction completes after the impact"), Person->IsMissionPatientDead());
		TestFalse(TEXT("Dead actor remains available for ambulance pickup"), Person->IsActorBeingDestroyed() || Person->IsHidden());
		Traffic->RemoveMissionPeople(777);
		TestFalse(TEXT("Mission cleanup preserves the body"), Person->IsActorBeingDestroyed());
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterBodyRecoveryTest, "SimCopter.Polish.AutomaticBodyRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterBodyRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterDispatch;
	auto* World = MakePolishWorld();
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0, FSimCity2000City::TileCount);
	for (int32 X = 63; X <= 67; ++X)
	{
		FVector Location;
		Traffic->TryGetTileCenterWorldLocation(X, 64, Location);
		auto& Node = Traffic->RoadNodes.AddDefaulted_GetRef();
		Node.Location = Node.LocalLocation = Location;
		Node.FileX = X; Node.FileY = 64; Node.BuildingId = 0x1d;
		if (X > 63) Node.Neighbors.Add(X - 64);
		if (X < 67) Node.Neighbors.Add(X - 62);
		Traffic->RoadNodeIndexByTile.Add(FIntPoint(X, 64), X - 63);
		Traffic->XbldTileIds[64 * 128 + X] = 0x1d;
	}
	const int32 Service = static_cast<int32>(EService::Ambulance);
	Traffic->DispatchVehicles[Service].SetNum(VehiclesPerService);
	FStation Station;
	Station.Service = EService::Ambulance; Station.Tile = FIntPoint(63, 62); Station.RoadTile = FIntPoint(63, 64);
	Traffic->DispatchStations[Service].Add(Station);
	auto* Body = World->SpawnActor<ASimCopterGroundAgent>();
	Body->SetOwner(Traffic);
	Body->SetActorLocation(Traffic->RoadNodes[3].Location + FVector(0, 0, Body->GetCapsuleHalfHeightCm()));
	Body->SetMissionDeadPose();
	Traffic->PedestrianAgents.Add(Body);
	Traffic->DispatchAmbulancesForBodies(2.0f);
	TestEqual(TEXT("Death automatically dispatches an ambulance"), Traffic->GetActiveDispatchCount(EService::Ambulance), 1);
	Traffic->DispatchAmbulancesForBodies(2.0f);
	TestEqual(TEXT("Same body cannot duplicate dispatches"), Traffic->GetActiveDispatchCount(EService::Ambulance), 1);
	const int32 Slot = Traffic->DispatchVehicles[Service].IndexOfByPredicate([Body](const FSimCopterDispatchVehicle& V) { return V.RecoveryBody == Body; });
	if (!TestTrue(TEXT("Body has a dispatched slot"), Slot != INDEX_NONE)) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	auto& Vehicle = Traffic->DispatchVehicles[Service][Slot];
	if (!TestNotNull(TEXT("Assigned ambulance exists"), Vehicle.Agent.Get())) { World->DestroyWorld(false); return false; }
	TestTrue(TEXT("Dispatch owns the actual corpse"), Vehicle.RecoveryBody == Body);
	Vehicle.Agent->SetActorLocation(Traffic->RoadNodes[3].Location + FVector(0, 0, 35));
	Vehicle.State = ESimCopterDispatchVehicleState::OnScene;
	// Place a real medic explicitly so this fixture does not need city triangle spawn probes.
	auto* Medic = World->SpawnActor<ASimCopterGroundAgent>();
	Medic->SetOwner(Traffic); Medic->SetMissionScriptedMover(); Medic->SetBehaviorGroundSnap(true);
	Medic->SetActorLocation(Body->GetActorLocation());
	Vehicle.DeployedParamedic = Medic;
	Traffic->UpdateBodyRecovery(Vehicle, 0.1f);
	TestTrue(TEXT("Medic carries the actual body"), Body->GetBehaviorCarrier() == Medic);
	TestFalse(TEXT("Carried corpse remains visible"), Body->IsHidden());
	Traffic->UpdateBodyRecovery(Vehicle, 0.1f);
	TestTrue(TEXT("Body is loaded into the ambulance"), Body->GetBehaviorCarrier() == Vehicle.Agent.Get());
	TestTrue(TEXT("Loaded body is inside the vehicle"), Body->IsHidden());
	TestEqual(TEXT("Ambulance heads back to hospital"), Vehicle.State, ESimCopterDispatchVehicleState::Returning);
	TestFalse(TEXT("Body is retained during the return drive"), Body->IsActorBeingDestroyed());
	TArray<uint8> Saved;
	TestTrue(TEXT("Recovery can be saved in transit"), Traffic->CaptureRuntimeSaveState(Saved));
	TestTrue(TEXT("Recovery can be restored in transit"), Traffic->RestoreRuntimeSaveState(Saved, nullptr));
	auto& Restored = Traffic->DispatchVehicles[Service][Slot];
	Body = Restored.RecoveryBody.Get();
	TestEqual(TEXT("Loaded recovery phase survives"), Restored.BodyRecoveryPhase, uint8(3));
	if (TestNotNull(TEXT("Same saved body restored"), Body) && TestNotNull(TEXT("Saved ambulance restored"), Restored.Agent.Get()))
	{
		TestTrue(TEXT("Carrier relationship survives save"), Body->GetBehaviorCarrier() == Restored.Agent.Get());
		Restored.Agent->SetActorLocation(Traffic->RoadNodes[0].Location);
		Traffic->UpdateOneDispatchVehicle(EService::Ambulance, Slot, 0.1f);
		TestTrue(TEXT("Hospital receives the body"), Body->IsActorBeingDestroyed());
		TestEqual(TEXT("Hospital ambulance capacity is released"), Traffic->DispatchStations[Service][0].Outstanding, 0);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterUfoMeshTest, "SimCopter.Polish.UfoGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterUfoMeshTest::RunTest(const FString& Parameters)
{
	TArray<FMaxisMeshSection> Parts;
	FMaxisMeshLibrary Library; FString Error;
	if (!TestTrue(TEXT("Original mesh library loads"), Library.LoadFromOriginalGameRoot(SimCopterOriginalGame::ResolveRoot(), Error))) return false;
	const TArray<FColor>* Palette = nullptr;
	const auto* Object = Library.FindObjectByObjectId(0x17c, &Palette);
	if (!TestNotNull(TEXT("Original UFO object exists"), Object)) return false;
	SimCopterUfoMesh::Build(*Object, Palette, 2621.44f, .25f, Parts);
	TestEqual(TEXT("Only authored hull is opaque"), Parts.Num(), 1);
	TArray<FSimCopterFlashingLightPoint> Lights;
	FSimCopterFlashingLightSchedule::ExtractLightPoints(*Object, Palette, 2621.44f, .25f, false, Lights);
	int32 ExpectedLights = 0;
	for (const auto& Face : Object->Faces) if (Face.FaceType == 25) ++ExpectedLights;
	TestTrue(TEXT("Authored rim lights exist"), ExpectedLights > 0);
	TestEqual(TEXT("Every original rim marker retained"), Lights.Num(), ExpectedLights);
	FString Obj;
	int32 Offset = 1;
	for (int32 PartIndex = 0; PartIndex < Parts.Num(); ++PartIndex)
	{
		const auto& Part = Parts[PartIndex];
		TestFalse(TEXT("Saucer material section has geometry"), Part.IsEmpty());
		Obj += FString::Printf(TEXT("g part%d\n"), PartIndex);
		for (int32 Index = 0; Index < Part.Vertices.Num(); ++Index)
		{
			const auto& V = Part.Vertices[Index]; const auto& UV = Part.UVs[Index];
			TestFalse(TEXT("Finite saucer vertex"), V.ContainsNaN());
			Obj += FString::Printf(TEXT("v %.5f %.5f %.5f %.5f %.5f %.5f\nvt %.6f %.6f\n"), V.X, V.Y, V.Z, Part.VertexColors[Index].R, Part.VertexColors[Index].G, Part.VertexColors[Index].B, UV.X, UV.Y);
		}
		for (int32 Index = 0; Index < Part.Triangles.Num(); Index += 3)
		{
			int32 A = Part.Triangles[Index], B = Part.Triangles[Index+1], C = Part.Triangles[Index+2];
			const FVector Normal = FVector::CrossProduct(Part.Vertices[B]-Part.Vertices[A], Part.Vertices[C]-Part.Vertices[A]);
			// The shared original-mesh builder adds reversed backfaces using the same
			// vertices/normals. Both windings must be finite and nondegenerate.
			TestTrue(TEXT("Nondegenerate original saucer triangles"), FMath::Abs(FVector::DotProduct(Normal, Part.Normals[A])) > 0.01);
			Obj += FString::Printf(TEXT("f %d/%d %d/%d %d/%d\n"), A+Offset,A+Offset,B+Offset,B+Offset,C+Offset,C+Offset);
		}
		Offset += Part.Vertices.Num();
	}
	FFileHelper::SaveStringToFile(Obj, *FPaths::Combine(FPaths::ProjectDir(), TEXT("../Docs/scratchpad/drive-in-ufo/ufo-mesh.obj")));
	return true;
}
#endif

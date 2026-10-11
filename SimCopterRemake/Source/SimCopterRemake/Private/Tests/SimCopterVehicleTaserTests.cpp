#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterVehicleTaserTest,"SimCopter.AirOperations.VehicleTaserHitRecoveryAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterVehicleTaserTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetActorLocation(FVector(0,0,800));
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	PC->Player = NewObject<ULocalPlayer>(GEngine); World->AddController(PC); PC->Possess(Heli);
	auto* Ops = Heli->GetAirOperations();
	Ops->bRemoteTaserActive = true;
	Ops->RemoteTaserCamera = World->SpawnActor<ACameraActor>();
	Ops->UpdateRemoteTaserMount();
	const auto NewCar = [&](const FVector& Location)
	{
		auto* Car = World->SpawnActor<ASimCopterGroundAgent>();
		Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle, TEXT(""), TEXT(""), 600);
		Car->SetOwner(Traffic); Car->SetActorLocation(Location); Car->bSnapToGround = false;
		Traffic->VehicleAgents.Add(Car);
		return Car;
	};
	auto* Car = NewCar(FVector(3000,0,100));
	Car->SetMoveTarget(FVector(6000,0,100)); Car->Tick(.1f);
	TestTrue(TEXT("Fixture car is driving before the hit"), Car->GetCurrentVelocityCmPerSec().Size() > 0);
	const FVector Route = Car->GetMoveTargetLocation();
	const FVector StoppedAt = Car->GetActorLocation();
	const auto FireAt = [&](const FVector& Target)
	{
		Ops->ShotCooldown = 0;
		Ops->RemoteTaserAim = (Target-Ops->RemoteTaserMount->GetComponentLocation()).Rotation();
		Ops->FireRemoteTaser();
	};
	FireAt(Car->GetActorLocation());
	TestTrue(TEXT("Real helicopter firing disables the hit car"), Car->IsVehicleTaserDisabled());
	TestTrue(TEXT("Disabled car reports immobilized"), Car->IsVehicleImmobilized());
	TestTrue(TEXT("Hit readout explains temporary stop"), Ops->Status.Contains(TEXT("car disabled")));
	TestTrue(TEXT("Hit stops motion immediately"), Car->GetCurrentVelocityCmPerSec().IsNearlyZero());
	TestFalse(TEXT("Electrical stop is not a tow breakdown"), Car->IsTowableVehicle());
	TestEqual(TEXT("Taser creates no tow mission"), Car->GetTowMissionId(), INDEX_NONE);
	TestTrue(TEXT("No impact damage, tip, fire or explosion"), Car->GetHelicopterImpactCount()==0 &&
		!Car->IsVehicleTipped() && !Car->IsVehicleAccidentBurning() && !Car->HasVehicleExploded());
	Car->AddTrafficVelocityImpulse(FVector(500,300,0));
	Car->MoveByTrafficSeparation(FVector(200,200,0));
	Car->SetTrafficSpeedScale(3.0f);
	Car->Tick(2.0f);
	TestTrue(TEXT("Traffic impulses and separation cannot displace the stopped car"), Car->GetActorLocation().Equals(StoppedAt));
	TestEqual(TEXT("Timer advances in simulation time"), Car->VehicleTaserSeconds, 6.0f);
	Traffic->SyncVehicleTrafficStates(.1f);
	auto& State = Traffic->VehicleTrafficStates.FindChecked(TObjectKey<ASimCopterGroundAgent>(Car));
	State.BlockedSeconds = 20;
	Traffic->SyncVehicleTrafficStates(.1f);
	TestEqual(TEXT("Electrical stop does not accumulate blockage"), State.BlockedSeconds, 0.0f);
	State.BlockedSeconds = 20;
	Traffic->UpdateVehicleBlockageRecovery();
	TestTrue(TEXT("Blocked-road recovery preserves the route while disabled"), Car->GetMoveTargetLocation().Equals(Route));
	TestEqual(TEXT("No bypass recovery starts while disabled"), State.RecoveryBypassSeconds, 0.0f);
	Ops->FireRemoteTaser();
	TestEqual(TEXT("Recharge blocks repeated fire"), Car->VehicleTaserSeconds, 6.0f);
	FireAt(Car->GetActorLocation());
	TestEqual(TEXT("Fresh hit refreshes rather than stacks duration"), Car->VehicleTaserSeconds, 8.0f);
	Car->Tick(3.0f);
	TArray<uint8> Save;
	TestTrue(TEXT("Disabled vehicle saves"), Car->CaptureRuntimeSaveState(Save));
	auto* Restored = NewCar(FVector(0,4000,100));
	TestTrue(TEXT("Disabled vehicle restores"), Restored->RestoreRuntimeSaveState(Save));
	TestEqual(TEXT("Save retains remaining duration"), Restored->VehicleTaserSeconds, 5.0f);
	TestTrue(TEXT("Save retains the route"), Restored->GetMoveTargetLocation().Equals(Route));
	Restored->Tick(4.0f);
	TestTrue(TEXT("Restored car remains stopped before expiry"), Restored->IsVehicleTaserDisabled() && Restored->GetActorLocation().Equals(StoppedAt));
	Restored->Tick(1.0f); Restored->Tick(.1f);
	TestFalse(TEXT("Timer expires automatically"), Restored->IsVehicleImmobilized());
	TestTrue(TEXT("Restored car resumes its route"), Restored->GetActorLocation().X > StoppedAt.X);
	Restored->Destroy();
	TArray<uint8> Legacy = Save;
	Legacy.SetNum(Legacy.Num()-sizeof(float));
	const int32 LegacyVersion = 8;
	FMemory::Memcpy(Legacy.GetData()+sizeof(uint32), &LegacyVersion, sizeof(LegacyVersion));
	auto* LegacyCar = NewCar(FVector(0,5000,100));
	LegacyCar->DisableVehicleWithTaser();
	TestTrue(TEXT("Version 8 vehicle saves remain readable"), LegacyCar->RestoreRuntimeSaveState(Legacy));
	TestFalse(TEXT("Old saves reset a previous transient stop"), LegacyCar->IsVehicleTaserDisabled());
	LegacyCar->Destroy();
	auto* InvalidCar = NewCar(FVector(0,6000,100));
	const float InvalidDuration = -1.0f;
	FMemory::Memcpy(Save.GetData()+Save.Num()-sizeof(float), &InvalidDuration, sizeof(float));
	TestFalse(TEXT("Invalid saved duration is rejected"), InvalidCar->RestoreRuntimeSaveState(Save));
	InvalidCar->Destroy();
	Car->Tick(5.0f); Car->Tick(.1f);
	TestTrue(TEXT("Original car resumes driving after eight seconds"), !Car->IsVehicleTaserDisabled() && Car->GetActorLocation().X > StoppedAt.X);

	// Cars use non-colliding render meshes. Exercise the rotated body away from its capsule.
	Car->SetActorLocationAndRotation(FVector(3000,0,100), FRotator(0,55,0));
	Car->CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FBox Body(ForceInit);
	TestTrue(TEXT("Car has visible body bounds"), Car->TryGetBodyLocalBoundsCm(Body));
	const FVector Roof = Car->GetActorTransform().TransformPosition(Body.GetCenter()+FVector(Body.GetExtent().X*.75,0,Body.GetExtent().Z));
	FVector End;
	TestTrue(TEXT("Roof hit outside capsule reaches the rotated car body"),
		SimCopterAirOperations::TraceRemoteTaserTarget(World,Heli,Roof+FVector(0,0,1000),-FVector::UpVector,1500,End)==Car);
	TestTrue(TEXT("Discharge ends at the body surface"), End.Equals(Roof,.1));
	const FVector Miss = Car->GetActorTransform().TransformPosition(Body.Max+FVector(100,100,0));
	TestNull(TEXT("Near miss does not select the car"),
		SimCopterAirOperations::TraceRemoteTaserTarget(World,Heli,Miss+FVector(0,0,1000),-FVector::UpVector,1500,End));
	TestNull(TEXT("Body beyond taser range is not selected"),
		SimCopterAirOperations::TraceRemoteTaserTarget(World,Heli,Roof+FVector(0,0,6100),-FVector::UpVector,6000,End));
	auto* NearCar = NewCar(Roof+FVector(0,0,500));
	NearCar->CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Nearest car body blocks the farther car"),
		SimCopterAirOperations::TraceRemoteTaserTarget(World,Heli,Roof+FVector(0,0,1000),-FVector::UpVector,1500,End)==NearCar);
	NearCar->Destroy();
	auto* Bystander = World->SpawnActor<ASimCopterGroundAgent>();
	Bystander->SetActorLocation(Roof+FVector(0,0,500));
	TestTrue(TEXT("Intervening civilian blocks the car"),
		SimCopterAirOperations::TraceRemoteTaserTarget(World,Heli,Roof+FVector(0,0,1000),-FVector::UpVector,1500,End)==Bystander);
	Bystander->Destroy();
	auto* Blocker = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Blocker);
	Blocker->SetRootComponent(Box); Box->SetBoxExtent(FVector(150,150,20));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent(); Blocker->SetActorLocation(Roof+FVector(0,0,400));
	TestNull(TEXT("World obstacle blocks the car hit"),
		SimCopterAirOperations::TraceRemoteTaserTarget(World,Heli,Roof+FVector(0,0,1000),-FVector::UpVector,1500,End));
	Blocker->SetActorLocation(FMath::Lerp(Car->GetActorLocation(),Ops->GetRemoteTaserMuzzle(),.5));
	Box->SetBoxExtent(FVector(250));
	FireAt(Car->GetActorLocation());
	TestFalse(TEXT("Actual firing through a wall cannot disable the car"), Car->IsVehicleTaserDisabled());
	Blocker->Destroy();
	FireAt(Roof);
	TestTrue(TEXT("Actual firing at body disables a car without capsule collision"), Car->IsVehicleTaserDisabled());
	Car->SetActorHiddenInGame(true);
	TestNull(TEXT("Hidden vehicle body is not a target"),
		SimCopterAirOperations::TraceRemoteTaserTarget(World,Heli,Roof+FVector(0,0,1000),-FVector::UpVector,1500,End));
	Car->SetActorHiddenInGame(false);
	Car->SetVehicleStalled(); Car->Tick(9.0f);
	TestTrue(TEXT("Expiry never repairs a subsequently stalled car"), Car->IsVehicleImmobilized() && Car->IsTowableVehicle());
	TestFalse(TEXT("Permanent breakdown rejects temporary disable"), Car->DisableVehicleWithTaser());
	auto* Towed = NewCar(FVector(0,7000,100)); Towed->SetVehicleTowed(true);
	TestFalse(TEXT("Towed vehicle cannot be disabled"), Towed->DisableVehicleWithTaser());
	auto* Wreck = NewCar(FVector(0,8000,100)); Wreck->bVehicleExploded = true;
	TestFalse(TEXT("Wreck cannot be disabled"), Wreck->DisableVehicleWithTaser());
	auto* Civilian = World->SpawnActor<ASimCopterGroundAgent>(); Civilian->SetActorLocation(FVector(0,9000,100));
	TestFalse(TEXT("Vehicle disable cannot affect pedestrians"), Civilian->DisableVehicleWithTaser());
	Ops->EndRemoteTaser();
	return true;
}
#endif

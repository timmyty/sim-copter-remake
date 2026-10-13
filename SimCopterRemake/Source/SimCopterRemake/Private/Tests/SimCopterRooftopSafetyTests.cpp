#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRooftopSafetyTest, "SimCopter.RooftopSafety.ExitAndContainment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRooftopSafetyTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto AddBox = [World](FVector Center, FVector Extent)
	{
		auto* Actor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box); Box->SetBoxExtent(Extent);
		Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
		Actor->SetActorLocation(Center);
		return Box;
	};
	AddBox(FVector(0,0,-10), FVector(5000,5000,10));
	auto* Roof = AddBox(FVector(0,0,990), FVector(350,120,10));
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	PC->Player = NewObject<ULocalPlayer>(GEngine);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->bIsLanded = true; Heli->GroundClearanceCm = 0;
	Heli->SetActorLocation(FVector(0,100,1000 + Heli->CollisionComponent->GetScaledCapsuleHalfHeight()));
	PC->Possess(Heli); Heli->PreferredExitSide = 1;
	Heli->ExitHelicopter();
	auto* Pilot = Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (TestNotNull(TEXT("A supported opposite door allows rooftop exit"), Pilot))
	{
		Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay = false;
		Pilot->DispatchBeginPlay();
		TestTrue(TEXT("Pilot stays on the landing roof, not the street"), FMath::Abs(Pilot->GetActorLocation().Z - Pilot->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 1000) < 4);
		TestTrue(TEXT("Pilot capsule fits completely inside the roof edge"), FMath::Abs(Pilot->GetActorLocation().Y) + Pilot->GetCapsuleComponent()->GetScaledCapsuleRadius() < 120);
		PC->Possess(Heli); Pilot->Destroy();
	}
	Roof->SetBoxExtent(FVector(30,30,10));
	Heli->SetActorLocation(FVector(0,0,1000 + Heli->CollisionComponent->GetScaledCapsuleHalfHeight()));
	Heli->ExitHelicopter();
	TestTrue(TEXT("A roof with no safe door position keeps the pilot aboard"), PC->GetPawn() == Heli);
	if (PC->GetPawn() != Heli) { APawn* Out = PC->GetPawn(); PC->Possess(Heli); if (Out) Out->Destroy(); }
	// Actual GEO fuselage widths, remembered sides and yaw all share the same exit search.
	Roof->SetBoxExtent(FVector(2000,2000,10));
	for (int32 Model = 0; Model < 9; ++Model)
	{
		TestTrue(TEXT("Original helicopter model loads"), Heli->SwitchHelicopterModel(Model));
		for (float Side : {-1.0f, 1.0f})
		{
			Heli->SetActorRotation(FRotator(0,37,0)); Heli->PreferredExitSide = Side;
			Heli->SetActorLocation(FVector(0,Side*1980,1007.5 + Heli->CollisionComponent->GetScaledCapsuleHalfHeight()));
			Heli->bIsLanded = true; Heli->GroundClearanceCm = 7.5f;
			Heli->ExitHelicopter(); Pilot = Cast<ASimCopterOnFootPawn>(PC->GetPawn());
			if (TestNotNull(TEXT("Each real helicopter permits a supported exit"), Pilot))
			{
				Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay = false; Pilot->DispatchBeginPlay();
				TestTrue(TEXT("Spawn initialization preserves the selected rooftop"), FMath::Abs(Pilot->GetActorLocation().Z-Pilot->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-1000)<4);
				TestTrue(TEXT("Rotated real aircraft exit keeps the whole pilot inside the roof"),FMath::Abs(Pilot->GetActorLocation().Y)+Pilot->GetCapsuleComponent()->GetScaledCapsuleRadius()<2000);
				PC->Possess(Heli); Pilot->Destroy();
			}
		}
	}
	Heli->bIsLanded = false; Heli->SetActorLocation(FVector(3000,0,2500));
	Heli->ExitHelicopter(); Pilot = Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (TestNotNull(TEXT("Deliberate airborne exits still work"), Pilot))
	{
		Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay = false; Pilot->DispatchBeginPlay();
		TestTrue(TEXT("Airborne initialization does not snap down to street"),Pilot->GetActorLocation().Z>2000);
		PC->Possess(Heli); Pilot->Destroy();
	}
	Heli->Destroy(); PC->Destroy();
	Roof->SetBoxExtent(FVector(350,120,10));
	AddBox(FVector(0,0,930), FVector(350,300,10)); // lower setback inside the square post
	for (bool bService : {false, true})
	{
		auto* Person = World->SpawnActor<ASimCopterGroundAgent>();
		Person->bBehaviorActive = false;
		const float Half = Person->GetCapsuleHalfHeightCm();
		Person->SetActorLocation(FVector(0,0,1001+Half));
		if (bService) Person->SetHospitalRoofPost(FVector(0,0,1000),250);
		else Person->SetMissionRoofPost(FVector(0,0,1000),250);
		Person->Tick(0.05f);
		TestFalse(TEXT("A roof step rejects the lower setback inside the post square"), Person->IsWithinHospitalRoofPost(FVector(0,160,1001+Half)));
		Person->MoveAgainstCityGeometry(FVector(0,170,0),FQuat::Identity);
		Person->Tick(0.05f);
		TestTrue(TEXT("A push cannot leave a posted person beyond the actual roof"), FMath::Abs(Person->GetActorLocation().Y)+Person->GetCollisionRadiusCm()<120);
		Person->SetActorLocation(FVector(0,160,940+Half));
		for (int32 Frame=0; Frame<30; ++Frame) Person->Tick(0.05f);
		TestTrue(TEXT("A person already partway down returns to supported roof"), FMath::Abs(Person->GetActorLocation().Z-Half-1000)<4 && FMath::Abs(Person->GetActorLocation().Y)+Person->GetCollisionRadiusCm()<120);
		Person->SetActorLocation(FVector(0,-160,940+Half));
		TArray<uint8> Saved;
		TestTrue(TEXT("Stranded rooftop person saves"),Person->CaptureRuntimeSaveState(Saved));
		TestTrue(TEXT("Stranded rooftop person restores"),Person->RestoreRuntimeSaveState(Saved));
		for (int32 Frame=0; Frame<30; ++Frame) Person->Tick(0.05f);
		TestTrue(TEXT("Saved stragglers recover without a cached safe point"), FMath::Abs(Person->GetActorLocation().Z-Half-1000)<4 && FMath::Abs(Person->GetActorLocation().Y)+Person->GetCollisionRadiusCm()<120);
		Person->Destroy();
	}
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "City/SimCity2000CityActor.h"
#include "Game/SimCopterCityIntro.h"
#include "Game/SimCopterGameMode.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Missions/SimCopterMissionSystemActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCarAccidentTest,"SimCopter.Accidents.FirstImpactOccupantsAndPersistence",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterCarAccidentTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterMissions;
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->FinishSpawning(FTransform::Identity);
	City->WaterGameplayCornerZ.Init(0,129*129); City->WaterGameplayTerrainClasses.Init(10,128*128);
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->SourceCityActor=City; Traffic->PeopleTileClasses.Init(7,128*128);
	Traffic->TileCenterWorldZ.Init(0,128*128); Traffic->WaterTileFlags.Init(0,128*128);
	Traffic->XbldTileIds.Init(0x1d,128*128); Traffic->ActiveTileSize=400;
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
	Missions->MissionSystem.AddScore(500);
	auto NewCar=[&](FVector At)
	{
		auto* Car=World->SpawnActor<ASimCopterGroundAgent>();
		Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),100);
		Car->SetOwner(Traffic); Car->SetActorLocation(At+FVector(0,0,Car->GetCapsuleHalfHeightCm()));
		return Car;
	};
	auto* Car=NewCar(FVector(2000,2000,0));
	const int32 Score=Missions->MissionSystem.GetScore(), Cash=Missions->MissionSystem.GetCash();
	Car->ApplyHelicopterVehicleImpact(Car->GetActorLocation()+FVector(0,40,0));
	TestTrue(TEXT("First collision tips and immobilizes the car"),Car->IsVehicleTipped() && Car->IsVehicleImmobilized());
	TestTrue(TEXT("Rendered body lies on its side"),FMath::IsNearlyEqual(FMath::Abs(Car->VisualRoot->GetRelativeRotation().Roll),90.0f));
	TestTrue(TEXT("First collision immediately burns"),Car->IsVehicleAccidentBurning());
	TestFalse(TEXT("Burning car cannot be towed"),Car->IsTowableVehicle());
	TestEqual(TEXT("Original accident points penalty"),Missions->MissionSystem.GetScore(),Score-100);
	TestEqual(TEXT("Original accident cash penalty"),Missions->MissionSystem.GetCash(),Cash-75);
	TArray<FSimCopterBurningVehicle> Fires; Traffic->GetBurningVehicles(Fires);
	TestEqual(TEXT("Accident appears in the actual fire rendering list without a mission record"),Fires.Num(),1);
	TestEqual(TEXT("Implicit ambient driver exits once"),Traffic->PedestrianAgents.Num(),1);
	ASimCopterGroundAgent* Driver=Traffic->PedestrianAgents.IsEmpty()?nullptr:Traffic->PedestrianAgents[0].Get();
	if (!TestNotNull(TEXT("Driver exists"),Driver)) return false;
	TestTrue(TEXT("Driver is visible and running from car"),!Driver->IsHidden() && Driver->VehicleEscapeSeconds>0 && Driver->ForcedFigureMnemonic==TEXT("1Run"));
	const FVector DriverStart=Driver->GetActorLocation();
	const float DriverFrameTime=Driver->FigureFrameTime;
	Driver->TickVehicleEscape(0.1f);
	TestTrue(TEXT("Driver makes progress away from wreck"),FVector::Dist2D(Driver->GetActorLocation(),Car->GetActorLocation())>FVector::Dist2D(DriverStart,Car->GetActorLocation()));
	TestTrue(TEXT("Running figure advances its animation"),Driver->FigureFrameTime>DriverFrameTime);
	Car->TickAirOperations(1.0f);
	Car->ApplyHelicopterVehicleImpact(Car->GetActorLocation());
	TestEqual(TEXT("Recontact cannot repeat penalty"),Missions->MissionSystem.GetScore(),Score-100);
	TestEqual(TEXT("Recontact cannot restart fuse"),Car->VehicleExplosionSeconds,3.0f);
	TestEqual(TEXT("Recontact cannot duplicate driver"),Traffic->PedestrianAgents.Num(),1);
	FSimCopterMissionUiMessage Message; Message.EventId=INDEX_NONE; Message.TextId=0x3bf; Message.Kind=8;
	FLinearColor Color;
	TestEqual(TEXT("Exact requested non-mission message"),Missions->FormatMissionUiMessage(Message,Color),FString(TEXT("Non-Mission Event: You caused an accident")));
	Message.Kind=9;
	TestTrue(TEXT("Cash accounting does not duplicate HUD notice"),Missions->FormatMissionUiMessage(Message,Color).IsEmpty());
	TArray<uint8> Saved;
	TestTrue(TEXT("Burning car saves"),Car->CaptureRuntimeSaveState(Saved));
	auto* Restored=NewCar(FVector(2000,2000,0));
	TestTrue(TEXT("Burning car restores"),Restored->RestoreRuntimeSaveState(Saved));
	TestTrue(TEXT("Restored car remains tipped and burning"),Restored->IsVehicleTipped() && Restored->IsVehicleAccidentBurning());
	TestEqual(TEXT("Remaining fuse preserved"),Restored->VehicleExplosionSeconds,3.0f);
	Restored->TickAirOperations(2.9f);
	TestFalse(TEXT("No early explosion"),Restored->bVehicleExploded);
	Restored->TickAirOperations(0.2f);
	TestTrue(TEXT("Explosion removes visible collision body"),Restored->bVehicleExploded && Restored->IsHidden() && !Restored->GetActorEnableCollision());
	TestEqual(TEXT("Restoration does not spawn another driver"),Traffic->PedestrianAgents.Num(),1);
	TestTrue(TEXT("Post-explosion state saves"),Restored->CaptureRuntimeSaveState(Saved));
	auto* Removed=NewCar(FVector(2000,2000,0));
	TestTrue(TEXT("Post-explosion state restores"),Removed->RestoreRuntimeSaveState(Saved));
	TestTrue(TEXT("Saving during removal cannot resurrect wreck"),Removed->IsHidden() && !Removed->GetActorEnableCollision() && Removed->GetLifeSpan()>0);
	TestTrue(TEXT("Save keeps driver escape"),Driver->CaptureRuntimeSaveState(Saved));
	TestTrue(TEXT("Driver escape restores"),Driver->RestoreRuntimeSaveState(Saved));
	TestTrue(TEXT("Restored driver still fleeing"),Driver->VehicleEscapeSeconds>0);
	TArray<int32> Doused; Traffic->DouseBurningVehiclesNear(Car->GetActorLocation(),100,Doused);
	Car->TickAirOperations(10);
	TestTrue(TEXT("Extinguishing cancels explosion but keeps tipped car recoverable"),!Car->bVehicleExploded && Car->IsVehicleTipped() && Car->IsTowableVehicle());
	if (const auto* Tow=Missions->MissionSystem.FindRecord(Car->TowMissionId)) TestTrue(TEXT("Recovery cannot farm rewards"),Tow->bSuppressCompletionRewards);
	// A real passenger must be detached, not replaced by a generated driver.
	auto* Occupied=NewCar(FVector(6000,6000,0));
	auto* Passenger=Traffic->SpawnVehicleDriver(FVector(6000,6100,92));
	if (!TestNotNull(TEXT("Real passenger fixture"),Passenger)) return false;
	TestTrue(TEXT("Passenger boards vehicle"),Passenger->BoardCarrier(Occupied,false));
	const int32 PeopleBefore=Traffic->PedestrianAgents.Num();
	Occupied->ApplyHelicopterVehicleImpact(Occupied->GetActorLocation());
	TestNull(TEXT("Passenger has left the tipped vehicle"),Passenger->GetBehaviorCarrier());
	TestTrue(TEXT("Same passenger flees visibly"),Passenger->VehicleEscapeSeconds>0 && !Passenger->IsHidden());
	TestEqual(TEXT("Real occupant is not duplicated"),Traffic->PedestrianAgents.Num(),PeopleBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCityIntroTest,"SimCopter.CityIntro.RouteAndControlRestoration",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterCityIntroTest::RunTest(const FString& Parameters)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->FinishSpawning(FTransform::Identity);
	City->WaterGameplayCornerZ.Init(0,129*129); City->WaterGameplayTerrainClasses.Init(10,128*128);
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->SourceCityActor=City; Traffic->TileCenterWorldZ.Init(0,128*128);
	Traffic->XbldTileIds.Init(0,128*128); Traffic->ActiveTileSize=400;
	Traffic->AirportOriginTile=FIntPoint(10,10);
	const auto Land=USimCopterCityIntro::BuildShots(*Traffic);
	TestTrue(TEXT("Imported empty land city still gets an overview and tour"),Land.Num()>=3);
	TestEqual(TEXT("Tour ends at airport"),Land.Last().Caption,FString(TEXT("Your home base")));
	for (const auto& Shot:Land)
	{
		const FTransform A=Shot.Evaluate(0),B=Shot.Evaluate(1);
		TestFalse(TEXT("Every shot moves"),A.GetLocation().Equals(B.GetLocation()));
		TestTrue(TEXT("Camera stays above city"),A.GetLocation().Z>Shot.Focus.Z);
		TestTrue(TEXT("Camera faces landmark"),FVector::DotProduct(A.GetRotation().GetForwardVector(),(Shot.Focus-A.GetLocation()).GetSafeNormal())>0.999);
		TestFalse(TEXT("Camera transform is finite"),Shot.Evaluate(0.5f).ContainsNaN());
	}
	Traffic->XbldTileIds[64*128+64]=0xd1;
	const auto Service=USimCopterCityIntro::BuildShots(*Traffic);
	TestTrue(TEXT("Land city selects its emergency services"),Service.ContainsByPredicate([](const auto& S){return S.Caption==TEXT("Emergency services");}));
	for (int32 Y=0;Y<128;++Y) for(int32 X=0;X<40;++X) City->WaterGameplayTerrainClasses[Y*128+X]=0;
	const auto Coastal=USimCopterCityIntro::BuildShots(*Traffic);
	TestTrue(TEXT("Coastal city showcases its waterfront"),Coastal.ContainsByPredicate([](const auto& S){return S.Caption==TEXT("Waterfront");}));
	auto* Mode=World->SpawnActor<ASimCopterGameMode>();
	auto* Tour=Mode->FindComponentByClass<USimCopterCityIntro>();
	TestNotNull(TEXT("Every city game mode owns an intro"),Tour);
	TestTrue(TEXT("Tour ticks through its own pause"),Tour->PrimaryComponentTick.bTickEvenWhenPaused);
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); World->AddController(PC);
	// Synthetic worlds do not call PostInitializeComponents, which normally creates this.
	PC->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>();
	PC->PlayerCameraManager->InitializeFor(PC);
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>(); PC->Possess(Pilot);
	Tour->Controller=PC; Tour->Camera=World->SpawnActor<ACameraActor>();
	PC->SetViewTarget(Tour->Camera); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
	PC->bShouldPerformFullTickWhenPaused=true;
	Tour->bPlaying=true; Tour->Elapsed=0.1f; Tour->RequestSkip();
	TestFalse(TEXT("Loading key cannot immediately skip tour"),Tour->bSkipRequested);
	Tour->Elapsed=1; Tour->RequestSkip();
	TestTrue(TEXT("Skip request queued for next tick"),Tour->bSkipRequested);
	Tour->Finish();
	TestFalse(TEXT("Tour releases play state"),Tour->IsPlaying());
	TestTrue(TEXT("Camera returns to actual possessed pawn"),PC->GetViewTarget()==Pilot);
	TestFalse(TEXT("Move and look input released"),PC->IsMoveInputIgnored() || PC->IsLookInputIgnored());
	TestFalse(TEXT("Normal paused camera policy restored"),PC->bShouldPerformFullTickWhenPaused);
	Tour->Finish();
	TestFalse(TEXT("Repeated cleanup is harmless"),PC->IsMoveInputIgnored());
	return true;
}
#endif

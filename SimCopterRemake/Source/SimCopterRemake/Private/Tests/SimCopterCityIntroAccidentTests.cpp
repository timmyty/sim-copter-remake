#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterDayNight.h"
#include "Game/SimCopterCityIntro.h"
#include "Game/SimCopterGameMode.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
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
	auto Route=USimCopterCityIntro::BuildRoute(*Traffic);
	TestTrue(TEXT("Complete 128-tile map edges included"),Route.Bounds.GetSize().Equals(FVector(51200,51200,0),0.1));
	// Tall/translated maps and narrow/ultrawide windows must keep all eight corners
	// visible throughout the orbit, not just at its cardinal points.
	for (const float Height : {0.0f,18000.0f})
	for (const float Aspect : {0.5625f,1.0f,4.0f/3,16.0f/9,32.0f/9})
	{
		Route.Bounds.Min=FVector(-12000,23000,800);
		Route.Bounds.Max=Route.Bounds.Min+FVector(51200,51200,Height);
		Route.Fit(Aspect);
		const float TanX=FMath::Tan(FMath::DegreesToRadians(Route.FieldOfView*0.5f)), TanY=TanX/Aspect;
		for (int32 Frame=0;Frame<=240;++Frame)
		{
			const FTransform Camera=Route.Evaluate(Frame/240.0f);
			for (int32 Corner=0;Corner<8;++Corner)
			{
				const FVector Point=Route.Bounds.GetCenter()+Route.Bounds.GetExtent()*FVector(Corner&1?1:-1,Corner&2?1:-1,Corner&4?1:-1);
				const FVector Local=Camera.InverseTransformPosition(Point);
				if (Local.X<=0 || FMath::Abs(Local.Y/Local.X)>TanX*0.92 || FMath::Abs(Local.Z/Local.X)>TanY*0.80)
				{ AddError(TEXT("City corner leaves the unobscured frame during its orbit")); return false; }
			}
			TestTrue(TEXT("Camera clears the highest roof"),Camera.GetLocation().Z>Route.Bounds.Max.Z);
		}
	}
	TestTrue(TEXT("One seamless full revolution"),Route.Evaluate(0).Equals(Route.Evaluate(1),0.01));
	TestFalse(TEXT("The midpoint shows the other side of the map"),Route.Evaluate(0).GetLocation().Equals(Route.Evaluate(0.5f).GetLocation()));
	auto* Ambient=World->SpawnActor<ASimCopterAmbientVehiclesActor>();
	Ambient->EnsurePools();
	TArray<uint8> Before,After;
	TestTrue(TEXT("Ambient fleet baseline captured"),Ambient->CaptureRuntimeSaveState(Before));
	const int32 MeshesBefore=Ambient->OwnedMeshes.Num();
	Ambient->BeginCityTour(Route.Bounds);
	TestEqual(TEXT("Two original airliners are visible during the whole-map tour"),Ambient->CityTourPlanes.Num(),2);
	if (Ambient->CityTourPlanes.Num()!=2) return false;
	const FVector PlaneStart=Ambient->CityTourPlanes[0]->GetComponentLocation();
	Ambient->UpdateCityTour(2);
	TestTrue(TEXT("Aircraft visibly move while gameplay is held"),FVector::Distance(PlaneStart,Ambient->CityTourPlanes[0]->GetComponentLocation())>100);
	TestEqual(TEXT("Tour aircraft cannot collide with gameplay"),Ambient->CityTourPlanes[0]->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	Ambient->CaptureRuntimeSaveState(After);
	TestTrue(TEXT("Tour cannot advance or overwrite saved ambient fleet state"),Before==After);
	Ambient->EndCityTour(); Ambient->EndCityTour();
	TestEqual(TEXT("Tour removes its temporary meshes exactly once"),Ambient->OwnedMeshes.Num(),MeshesBefore);
	Ambient->CaptureRuntimeSaveState(After);
	TestTrue(TEXT("Fleet save is identical after the intro"),Before==After);
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
	PC->PushPause(); PC->PushPause();
	Tour->bPlaying=true; Tour->Elapsed=0.1f; Tour->RequestSkip();
	TestFalse(TEXT("Loading key cannot immediately skip tour"),Tour->bSkipRequested);
	Tour->Elapsed=1; Tour->RequestSkip();
	TestTrue(TEXT("Skip request queued for next tick"),Tour->bSkipRequested);
	Tour->Finish();
	TestFalse(TEXT("Tour releases play state"),Tour->IsPlaying());
	TestTrue(TEXT("Camera returns to actual possessed pawn"),PC->GetViewTarget()==Pilot);
	TestFalse(TEXT("Move and look input released"),PC->IsMoveInputIgnored() || PC->IsLookInputIgnored());
	TestFalse(TEXT("Normal paused camera policy restored"),PC->bShouldPerformFullTickWhenPaused);
	TestEqual(TEXT("Intro releases only its own pause"),PC->PauseDepth,1);
	PC->PopPause();
	Tour->Finish();
	TestFalse(TEXT("Repeated cleanup is harmless"),PC->IsMoveInputIgnored());
	return true;
}

// Render on separate engine frames so material uniforms, visibility and exposure
// history can reach the GPU. Multiple synchronous captures in one frame reuse state.
class FSimCopterTourCaptureCommand : public IAutomationLatentCommand
{
public:
	FAutomationTestBase* Test;
	UGameInstance* Instance;
	UWorld* World;
	ASimCity2000CityActor* City;
	ASimCopterAmbientVehiclesActor* Ambient;
	USceneCaptureComponent2D* Capture;
	UTextureRenderTarget2D* Target;
	TArray<UPrimitiveComponent*> Aircraft;
	FSimCopterCityIntroRoute Route;
	FString Evidence;
	int32 CityIndex, Stage=0;
	double NextFrame=0;
	TArray<FColor> Overview,Before;
	virtual bool Update() override
	{
		const double Now=FPlatformTime::Seconds();
		if (Now<NextFrame) return false;
		NextFrame=Now+0.15;
		switch (Stage++)
		{
		case 0: Read(0,0,TEXT("warmup")); break;
		case 1:
			Overview=Read(0,0,TEXT("start"));
			// Isolate the two meshes and read their visible normals. Motion must not
			// depend on water, emissive city lights or the aircraft paint's brightness.
			Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
			for (auto* Mesh:Aircraft) Capture->ShowOnlyComponent(Mesh);
			Capture->CaptureSource=SCS_Normal;
			break;
		case 2: Read(0,0,TEXT("aircraft-warmup")); break;
		case 3: Before=Read(0,0,TEXT("aircraft-before")); break;
		case 4:
		{
			const auto Moved=Read(4,0,TEXT("aircraft-after"));
			int32 Changed=0,Lit=0;
			for (int32 Pixel=0;Pixel<Before.Num();++Pixel)
			{
				const auto A=Before[Pixel],B=Moved[Pixel],O=Overview[Pixel];
				if (FMath::Max3(O.R,O.G,O.B)>20) ++Lit;
				if (FMath::Abs(int(A.R)-int(B.R))+FMath::Abs(int(A.G)-int(B.G))+FMath::Abs(int(A.B)-int(B.B))>30) ++Changed;
			}
			Test->TestTrue(TEXT("Full city renders visible scenery"),Lit>Before.Num()/10);
			Test->TestTrue(TEXT("Moving aircraft change actual scene pixels with a stationary camera"),Changed>10);
			Test->AddInfo(FString::Printf(TEXT("City %d: %d lit pixels; %d pixels changed by aircraft"),CityIndex,Lit,Changed));
			Capture->ShowOnlyComponents.Reset();
			Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
			Capture->CaptureSource=SCS_FinalColorLDR;
			break;
		}
		case 5: Read(6,6,TEXT("quarter")); break;
		case 6: Read(12,12,TEXT("half")); break;
		case 7: Read(18,18,TEXT("three-quarter")); break;
		default:
			Ambient->EndCityTour(); Capture->DestroyComponent();
			Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
			return true;
		}
		return false;
	}
private:
	TArray<FColor> Read(float Time,float CameraTime,const TCHAR* Label)
	{
		Ambient->UpdateCityTour(Time); Capture->SetWorldTransform(Route.Evaluate(CameraTime/Route.Duration));
		World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
		TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		for (auto& Pixel:Pixels) Pixel.A=255;
		FImageUtils::SaveImageByExtension(*(Evidence/FString::Printf(TEXT("city%d-%s.png"),CityIndex,Label)),FImageView(Pixels.GetData(),1280,720));
		return Pixels;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCityTourRenderTest,"SimCopter.CityIntro.FullMapRendering",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FSimCopterCityTourRenderTest::RunTest(const FString&)
{
	const FString Evidence=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/city-tour-revision");
	IFileManager::Get().MakeDirectory(*Evidence,true);
	for (const int32 CityIndex : {0,29})
	{
		auto* Instance=NewObject<UGameInstance>(GEngine);
		Instance->InitializeStandalone();
		auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
		auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
		City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false;
		City->bRenderStreetLightSpotLights=false;
		City->CityFile.FilePath=SimCopterOriginalGame::ResolveRoot()/FString::Printf(TEXT("cities/career/city%d.sc2"),CityIndex);
		City->FinishSpawning(FTransform::Identity); City->RebuildCity();
		auto* Sun=NewObject<UDirectionalLightComponent>(City);
		Sun->SetIntensity(120000); Sun->SetWorldRotation(FRotator(-55,-25,0)); Sun->RegisterComponent();
		auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>(); Traffic->SourceCityActor=City;
		if (!TestTrue(TEXT("Actual city routing loads"),Traffic->RebuildSpawnData())) return false;
		auto Route=USimCopterCityIntro::BuildRoute(*Traffic); Route.Fit(16.0f/9);
		AddInfo(FString::Printf(TEXT("City %d bounds %s, orbit distance %.1f"),CityIndex,*Route.Bounds.ToString(),Route.Distance));
		auto* Ambient=World->SpawnActor<ASimCopterAmbientVehiclesActor>(); Ambient->BeginCityTour(Route.Bounds);
		if (!TestEqual(TEXT("Original airliner geometry loads"),Ambient->CityTourPlanes.Num(),2)) return false;
		auto* Capture=NewObject<USceneCaptureComponent2D>(City);
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->bAlwaysPersistRenderingState=true;
		Capture->CaptureSource=SCS_FinalColorLDR; Capture->FOVAngle=Route.FieldOfView;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;
		Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
		Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=0;
		Capture->UnlitViewmode=ESceneCaptureUnlitViewmode::Disabled;
		Capture->ShowFlags.SetBloom(false);
		Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
		auto* Target=NewObject<UTextureRenderTarget2D>(City);
		Target->RenderTargetFormat=RTF_RGBA8; Target->InitAutoFormat(1280,720); Target->UpdateResourceImmediate(true);
		Capture->TextureTarget=Target; Capture->RegisterComponent();
		// This synthetic world has not ticked. Publish the normal city/vehicle
		// albedo ceilings before capturing; uninitialized collection values are black.
		World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);
		World->UpdateParameterCollectionInstances(true,true);
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		for (const auto& Mesh:Ambient->CityTourPlanes)
			AddInfo(FString::Printf(TEXT("Aircraft bounds %s, visible %d"),*Mesh->Bounds.GetBox().ToString(),Mesh->IsVisible()));
		auto Command=MakeShared<FSimCopterTourCaptureCommand>();
		Command->Test=this; Command->Instance=Instance; Command->World=World; Command->City=City;
		Command->Ambient=Ambient; Command->Capture=Capture; Command->Target=Target;
		for (const auto& Mesh:Ambient->CityTourPlanes) Command->Aircraft.Add(Mesh);
		Command->Route=Route; Command->Evidence=Evidence; Command->CityIndex=CityIndex;
		FAutomationTestFramework::Get().EnqueueLatentCommand(Command);
	}
	return true;
}
#endif

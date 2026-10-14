#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/SimCopterCityIntro.h"
#include "Game/SimCopterSettings.h"
#include "City/SimCopterDayNight.h"
#include "DaySequenceActor.h"
#include "Game/SimCopterPlayerController.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterSmokeStacks.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "ProceduralMeshComponent.h"
#include "Ground/SimCopterEffectRasterizer.h"
#include "Ground/SimCopterFlashingLights.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCameraObstructionStabilityTest,
	"SimCopter.Camera.StationaryObstacleStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCameraObstructionStabilityTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto Box=[&](FVector Position,FVector Extent)
	{
		auto* Actor=World->SpawnActor<AActor>();
		auto* Shape=NewObject<UBoxComponent>(Actor); Actor->SetRootComponent(Shape);
		Shape->SetBoxExtent(Extent); Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent();
		Actor->SetActorLocation(Position); return Actor;
	};
	Box(FVector(0,0,-50),FVector(10000,10000,50));
	auto* Obstacle=Box(FVector(-400,0,100),FVector(100,1000,100));
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>(); Heli->SetActorLocation(FVector(0,0,120));
	Heli->CameraMode=ESimCopterCameraMode::Chase;
	Heli->UpdateCameraAnchorFromVisibleBody();
	for (float X : {-200.f,-400.f,-600.f,-800.f}) for (float Height : {100.f,250.f,500.f})
	{
		auto* Shape=CastChecked<UBoxComponent>(Obstacle->GetRootComponent()); Shape->SetBoxExtent(FVector(100,1000,Height));
		Obstacle->SetActorLocation(FVector(X,0,Height));
		Heli->bCameraViewSmoothingInitialized=false; Heli->CurrentCameraGroundLiftCm=0;
		Heli->CurrentCameraAvoidanceOffsetDeg=FRotator::ZeroRotator; Heli->CurrentCameraPullInAlpha=1;
		float MinPitch=1000,MaxPitch=-1000,MinZ=1e9f,MaxZ=-1e9f;
		for (int32 Frame=0;Frame<900;++Frame)
		{
			Heli->UpdateCamera(1.f/60); Heli->CameraBoom->TickComponent(1.f/60,LEVELTICK_All,nullptr);
			if (Frame<720) continue;
			const float Pitch=Heli->CameraBoom->GetComponentRotation().Pitch, Z=Heli->CameraComponent->GetComponentLocation().Z;
			MinPitch=FMath::Min(MinPitch,Pitch); MaxPitch=FMath::Max(MaxPitch,Pitch);
			MinZ=FMath::Min(MinZ,Z); MaxZ=FMath::Max(MaxZ,Z);
		}
		TestTrue(FString::Printf(TEXT("Static obstacle x=%.0f height=%.0f settles (pitch range %.3f, height range %.3f)"),X,Height,MaxPitch-MinPitch,MaxZ-MinZ),MaxPitch-MinPitch<.1f && MaxZ-MinZ<1.f);
		Heli->ResetStartupCamera(); Heli->CameraBoom->TickComponent(0,LEVELTICK_All,nullptr);
		const FVector FirstRevealedCamera=Heli->CameraComponent->GetComponentLocation();
		float StartupDrift=0;
		for (int32 Frame=0;Frame<60;++Frame)
		{
			Heli->UpdateCamera(1.f/60); Heli->CameraBoom->TickComponent(1.f/60,LEVELTICK_All,nullptr);
			StartupDrift=FMath::Max(StartupDrift,float(FVector::Distance(FirstRevealedCamera,Heli->CameraComponent->GetComponentLocation())));
		}
		TestTrue(FString::Printf(TEXT("Revealed camera beside x=%.0f height=%.0f starts settled (drift %.3f cm)"),X,Height,StartupDrift),StartupDrift<1.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterLoadingEffectsTest,
	"SimCopter.CityIntro.CameraFacingEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterLoadingEffectsTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->FinishSpawning(FTransform::Identity);
	auto* Smoke=City->FindComponentByClass<USimCopterSmokeStacksComponent>();
	Smoke->bCastPointLights=false;
	TArray<FColor> Palette; Palette.Init(FColor::White,256); FString Error;
	if (!TestTrue(TEXT("Original smoke kernel initializes"),Smoke->InitSmokeAssets(Palette,nullptr,Error))) return false;
	USimCopterSmokeStacksComponent::FSmokeMarker Marker; Marker.LocalOffset=FVector(4000,0,0); Marker.EffectClass=1;
	Smoke->SetSmokeMarkers({Marker});
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); World->AddController(PC);
	PC->Player=NewObject<ULocalPlayer>(GEngine);
	PC->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>(); PC->PlayerCameraManager->InitializeFor(PC);
	auto* Camera=World->SpawnActor<ACameraActor>(); PC->SetViewTarget(Camera); PC->PlayerCameraManager->UpdateCamera(0);
	Smoke->SyncSmokeFromPlayerCamera(0);
	UProceduralMeshComponent* Mesh=nullptr;
	for (auto Child:Smoke->GetAttachChildren()) if (auto* Candidate=Cast<UProceduralMeshComponent>(Child.Get())) Mesh=Candidate;
	if (!TestNotNull(TEXT("Smoke creates actual billboard geometry"),Mesh)) return false;
	const auto* Before=Mesh->GetProcMeshSection(0);
	if (!TestTrue(TEXT("Smoke is initially in front of the gameplay camera"),Before && Before->ProcVertexBuffer.Num()>0)) return false;
	// Loading/minimized windows can have a camera before they have a usable viewport.
	auto* Lights=NewObject<USimCopterFlashingLightsComponent>(City);
	Lights->RegisterComponent(); Lights->bCastPointLights=false;
	FSimCopterFlashingLightPoint Light; Light.LocalOffset=FVector(50000,0,0);
	Light.PaletteIndex=FSimCopterFlashingLightSchedule::WhitePaletteIndex; Light.Color=FLinearColor::White;
	Lights->SetLightPoints({Light}); Lights->SyncLightsFromPlayerCamera(0);
	UProceduralMeshComponent* LightMesh=nullptr;
	for (auto Child:Lights->GetAttachChildren()) if (auto* Candidate=Cast<UProceduralMeshComponent>(Child.Get())) LightMesh=Candidate;
	if (!TestNotNull(TEXT("Beacon creates actual card geometry"),LightMesh)) return false;
	const auto* LightSection=LightMesh->GetProcMeshSection(0);
	if (!TestNotNull(TEXT("White beacon is visible in phase zero"),LightSection)) return false;
	const float WideSize=LightSection->SectionLocalBox.GetSize().GetMax();
	TestTrue(TEXT("Unavailable viewport cannot create a map-sized beacon sheet"),WideSize>0 && WideSize<1000);
	Camera->GetCameraComponent()->SetFieldOfView(45); PC->PlayerCameraManager->UpdateCamera(0);
	Lights->SyncLightsFromPlayerCamera(0);
	TestTrue(TEXT("Projection changes refresh card size even at identical camera position and blink phase"),LightMesh->GetProcMeshSection(0)->SectionLocalBox.GetSize().GetMax()<WideSize*.75f);
	for (float InvalidWidth : {0.f,1.f,-1.f})
		TestEqual(TEXT("Missing viewport contributes no screen-pixel size floor"),FSimCopterFlashingLightSchedule::GetWorldSizeForScreenPixels(50000,InvalidWidth,90,1),0.f);
	Camera->SetActorRotation(FRotator(0,180,0)); PC->PlayerCameraManager->UpdateCamera(0);
	TestTrue(TEXT("Fixture really turns the evaluated player camera away from the plume"),PC->PlayerCameraManager->GetCameraRotation().Vector().X<-.99f);
	TestTrue(TEXT("Without a paused presentation refresh the old cards remain after a camera cut"),Mesh->GetProcMeshSection(0)->ProcVertexBuffer.Num()>0);
	auto* Tour=NewObject<USimCopterCityIntro>(City); Tour->Controller=PC; Tour->Camera=Camera;
	Tour->Route.Bounds=FBox(FVector(-100),FVector(100));
	// A camera cut turns completely away from the plume while gameplay remains frozen.
	FSimCopterCityIntroFrame Frame; Frame.Position=FVector::ZeroVector; Frame.Target=FVector(-1000,0,0);
	Tour->Route.Frames={Frame,Frame}; Tour->UpdateCamera();
	const auto* After=Mesh->GetProcMeshSection(0);
	TestTrue(TEXT("Flyover camera cut removes stale smoke cards behind the new view"),!After || After->ProcVertexBuffer.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDistantEffectsTest,
	"SimCopter.Effects.DistantCameraCulling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterDistantEffectsTest::RunTest(const FString&)
{
	for (float Depth : {9000.f,100000.f,205000.f,500000.f,1e12f})
		TestEqual(FString::Printf(TEXT("Far camera depth %.0f cannot wrap into a full-size effect"),Depth),FSimCopterEffectRasterizer::ComputeDepthScale1616(Depth),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCityIntroLightingTest,
	"SimCopter.CityIntro.LightingRestoration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCityIntroLightingTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->SetGameMode(FURL()); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Sky=World->SpawnActor<ADaySequenceActor>();
	auto* Day=World->GetSubsystem<USimCopterDayNightSubsystem>(); Day->CachedDaySequenceActor=Sky;
	auto* Owner=World->SpawnActor<AActor>();
	auto* Tour=NewObject<USimCopterCityIntro>(Owner);
	Owner->AddInstanceComponent(Tour); Tour->RegisterComponent(); Owner->DispatchBeginPlay();
	auto* Settings=USimCopterSettings::Get(Owner);
	if (!TestNotNull(TEXT("Tour has session lighting settings"),Settings)) return false;
	for (const auto Mode : {ESimCopterTimeOfDayMode::Dynamic,ESimCopterTimeOfDayMode::Static,ESimCopterTimeOfDayMode::RealTime})
	{
		Settings->SetTimeOfDayMode(Mode); Settings->SetStaticTimeOfDayHours(21);
		Day->ApplyTimeOfDaySettings(); Day->RestoreSavedTimeOfDay(21);
		const float Before=Sky->GetTimeOfDay();
		Tour->BeginTourLighting(); Tour->BeginTourLighting();
		TestEqual(TEXT("Flyover pins the time mode"),Settings->GetTimeOfDayMode(),ESimCopterTimeOfDayMode::Static);
		TestTrue(TEXT("Actual day-sequence clock seeks to 1 PM"),FMath::IsNearlyEqual(Sky->GetTimeOfDay(),13.f,.01f));
		Day->Tick(1);
		TestTrue(TEXT("Paused lighting updates retain 1 PM"),FMath::IsNearlyEqual(Sky->GetTimeOfDay(),13.f,.01f));
		Tour->bPlaying=true;
		Tour->Finish(); Tour->Finish();
		TestEqual(TEXT("Finishing or skipping restores the time mode"),Settings->GetTimeOfDayMode(),Mode);
		TestEqual(TEXT("Temporary tour lighting never replaces the player's static hour"),Settings->GetStaticTimeOfDayHours(),21.f);
		TestTrue(TEXT("Gameplay resumes its prior live hour (wall-clock mode repins to local time)"),FMath::IsNearlyEqual(Sky->GetTimeOfDay(),Before,.02f));
	}
	// EndPlay uses the same cleanup when a level is abandoned during its intro.
	Settings->SetTimeOfDayMode(ESimCopterTimeOfDayMode::Dynamic);
	Sky->SetTimeOfDay(9);
	Day->PendingSavedTimeOfDayHours=23; // Save loaded before the sky was discovered.
	Day->CachedDaySequenceActor.Reset(); Day->LastActorScanSeconds=FPlatformTime::Seconds();
	Tour->BeginTourLighting(); Day->Tick(0);
	TestTrue(TEXT("Queued save time cannot overwrite the intro's 1 PM lighting"),FMath::IsNearlyEqual(Sky->GetTimeOfDay(),13.f,.01f));
	Tour->EndPlay(EEndPlayReason::LevelTransition);
	TestTrue(TEXT("Tour finds a newly loaded sky before the normal rescan interval"),Day->GetDaySequenceActor()==Sky);
	TestEqual(TEXT("Level transition cannot leak the lighting override into the next city"),Settings->GetTimeOfDayMode(),ESimCopterTimeOfDayMode::Dynamic);
	TestTrue(TEXT("Queued save's original hour survives presentation cleanup"),FMath::IsNearlyEqual(Sky->GetTimeOfDay(),23.f,.01f));
	return true;
}
#endif

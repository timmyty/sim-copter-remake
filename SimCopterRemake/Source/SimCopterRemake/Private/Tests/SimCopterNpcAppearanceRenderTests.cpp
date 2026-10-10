#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "City/SimCopterDayNight.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterHelicopterPresentation.h"
#include "ProceduralMeshComponent.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterNpcAppearanceRenderTest, "SimCopter.Appearance.RenderedCharacters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterNpcAppearanceRenderTest::RunTest(const FString&)
{
	if (GUsingNullRHI) { AddInfo(TEXT("Use RenderOffscreen to inspect the character comparison.")); return true; }
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone();
	auto* World = Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	TArray<UProceduralMeshComponent*> Figures;
	auto* Rig = World->SpawnActor<AActor>();
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SwitchHelicopterModel(5);
	for (int32 Class : {0, 1, 2, 3, 4, 11, 12, 13, 14, 19})
	{
		auto* Person = World->SpawnActor<ASimCopterGroundAgent>();
		Person->SetInitialBehaviorClass(Class);
		Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
		Person->SetPedestrianFigureClothesOffset(0);
		Person->SetActorLocation(FVector(0, -18, Person->GetCapsuleHalfHeightCm()));
		Figures.Add(Person->FindComponentByClass<UProceduralMeshComponent>());
		Heli->RemoveMissionPassengers(100);
		Heli->AddMissionPassengersForMission(1, 77, ESimCopterMissionPassengerKind::Transport, Person);
		Heli->RefreshCabinOccupants();
		auto* Seated = NewObject<UProceduralMeshComponent>(Rig);
		const FVector Seat = SimCopterHelicopterPresentation::CabinSeat(5, 1);
		for (int32 Section : {0, 2})
		{
			if (const auto* Mesh = Heli->CabinOccupantsMesh->GetProcMeshSection(Section))
			{
				Seated->SetProcMeshSection(Section, *Mesh);
				Seated->SetMaterial(Section, Heli->CabinOccupantsMesh->GetMaterial(Section));
			}
		}
		Seated->SetWorldLocation(FVector(0, 18, 18) - Seat);
		Seated->RegisterComponent(); Figures.Add(Seated);
	}
	auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Heli);
	Heli->RemoveMissionPassengers(100);
	auto* Passenger = World->SpawnActor<ASimCopterGroundAgent>(); Passenger->SetInitialBehaviorClass(0);
	Passenger->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
	Passenger->SetPedestrianFigureClothesOffset(0);
	Heli->AddMissionPassengersForMission(1, 78, ESimCopterMissionPassengerKind::Transport, Passenger);
	Heli->RefreshCabinOccupants();
	World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);
	World->UpdateParameterCollectionInstances(true, true);
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Instance, World, Rig, Heli, Figures]()
	{
		ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		for (auto* Figure : Figures) Figure->RecreateRenderState_Concurrent();
		auto* Sun = NewObject<UDirectionalLightComponent>(Rig);
		Sun->SetIntensity(120000); Sun->SetWorldRotation(FRotator(-25, 165, 0)); Sun->RegisterComponent();
		auto* Capture = NewObject<USceneCaptureComponent2D>(Rig);
		Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true; Capture->CaptureSource = SCS_FinalColorLDR;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod = true; Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = true;
		Capture->PostProcessSettings.bOverride_CameraISO = true; Capture->PostProcessSettings.CameraISO = 100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed = true; Capture->PostProcessSettings.CameraShutterSpeed = 125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop = true; Capture->PostProcessSettings.DepthOfFieldFstop = 8;
		Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetBloom(false);
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		auto* Target = NewObject<UTextureRenderTarget2D>(Rig);
		Target->RenderTargetFormat = RTF_RGBA8; Target->InitAutoFormat(640, 480); Target->UpdateResourceImmediate(true);
		Capture->TextureTarget = Target; Capture->RegisterComponent();
		Capture->SetWorldLocationAndRotation(FVector(100, 0, 28), FRotator(0, 180, 0)); Capture->FOVAngle = 44;
		// UE queues on-demand material shaders only after the first real draw. Finishing
		// compilation before that draw leaves this entire synchronous capture on checkers.
		for (auto* Figure : Figures) Capture->ShowOnlyComponent(Figure);
		Capture->ShowOnlyComponent(Heli->HeliBodyMeshComponent); Capture->ShowOnlyComponent(Heli->CabinOccupantsMesh);
		World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FAssetCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
		for (int32 Index = 0; Index < Figures.Num(); Index += 2)
		{
			Capture->ShowOnlyComponents.Empty();
			Capture->ShowOnlyComponent(Figures[Index]); Capture->ShowOnlyComponent(Figures[Index+1]);
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			int32 VisiblePixels = 0, ColoredPixels = 0;
			for (auto& Pixel : Pixels)
			{
				Pixel.A = 255; if (Pixel.R + Pixel.G + Pixel.B > 50) ++VisiblePixels;
				if (FMath::Max3(Pixel.R, Pixel.G, Pixel.B) - FMath::Min3(Pixel.R, Pixel.G, Pixel.B) > 25) ++ColoredPixels;
			}
			TestTrue(TEXT("Character comparison contains visible figures"), VisiblePixels > 1000);
			TestTrue(TEXT("Actual colored materials render instead of gray shader fallbacks"), ColoredPixels > 100);
			const FString Path = FPaths::ProjectDir() / TEXT("../Docs/scratchpad/npc-appearance") /
				FString::Printf(TEXT("characters-%02d.png"), Index/2);
			FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), 640, 480));
		}
		// Inspect the actual canopy too: isolated seated figures alone cannot prove they are
		// visible through the airframe. Compare the same render with/without its occupants.
		Capture->ShowOnlyComponents.Empty();
		Capture->ShowOnlyComponent(Heli->HeliBodyMeshComponent); Capture->ShowOnlyComponent(Heli->CabinOccupantsMesh);
		Capture->SetWorldLocationAndRotation(FVector(210, 180, 100), (FVector(30, 0, 25)-FVector(210, 180, 100)).Rotation());
		Capture->FOVAngle = 40;
		auto RenderCabin = [&]()
		{
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			for (auto& Pixel : Pixels) Pixel.A = 255;
			return Pixels;
		};
		const auto Occupied = RenderCabin();
		Heli->CabinOccupantsMesh->SetVisibility(false);
		const auto Empty = RenderCabin();
		int32 OccupantPixels = 0;
		for (int32 I = 0; I < Occupied.Num(); ++I)
			if (FMath::Abs(int32(Occupied[I].R)-Empty[I].R) + FMath::Abs(int32(Occupied[I].G)-Empty[I].G) + FMath::Abs(int32(Occupied[I].B)-Empty[I].B) > 20) ++OccupantPixels;
		TestTrue(TEXT("Pilot and passenger are visible through the actual aircraft canopy"), OccupantPixels > 50);
		AddInfo(FString::Printf(TEXT("Visible cabin occupant pixels: %d"), OccupantPixels));
		FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/npc-appearance/cabin.png")), FImageView(Occupied.GetData(), 640, 480));
	}, 1.0f));
	return true;
}
#endif

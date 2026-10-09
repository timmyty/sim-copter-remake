#if WITH_DEV_AUTOMATION_TESTS

#include "Missions/SimCopterWitnessBriefing.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Slate/WidgetRenderer.h"
#include "ShaderCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterWitnessPhotoRenderingTest,
	"SimCopter.Witness.PhotoRendering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FSimCopterWitnessPhotoRenderingTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

	AActor* Owner = World->SpawnActor<AActor>();
	USimCopterWitnessBriefing* Briefing = NewObject<USimCopterWitnessBriefing>(Owner);
	Briefing->RegisterComponent();
	ASimCopterGroundAgent* Robber = World->SpawnActor<ASimCopterGroundAgent>();
	Robber->InitialPersonState = 10;
	Robber->MissionEventId = 73;
	Robber->SetPedestrianFigureName(TEXT("SHADES"));
	Robber->SetPedestrianFigureClothesOffset(3);
	Robber->SetActorLocation(FVector(0, 0, 24));
	if (!TestTrue(TEXT("The actual SHADES robber figure builds"), Robber->BuildPedestrianFigure()))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	// Match the city's physically scaled noon sun, not the obsolete four-lux lighting.
	UDirectionalLightComponent* Light = NewObject<UDirectionalLightComponent>(Owner);
	Light->SetIntensity(120000.0f);
	Light->SetWorldRotation(FRotator(-35, 180, 0));
	Light->RegisterComponent();
	UStaticMeshComponent* Backdrop = NewObject<UStaticMeshComponent>(Owner);
	Backdrop->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Backdrop->SetWorldLocation(FVector(-120, 0, 0));
	Backdrop->SetWorldScale3D(FVector(0.1, 10, 10));
	Backdrop->RegisterComponent();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	// Let the editor finish applying compiled materials before reading scene pixels.
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, World, Briefing, Robber]()
	{
		ON_SCOPE_EXIT
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		};
		Briefing->Report(Robber);
		Briefing->TickComponent(0.3f, LEVELTICK_All, nullptr);
		if (!TestNotNull(TEXT("Witness report captures a photo"), Briefing->Photo.Get())) return;
		TestTrue(TEXT("Caption identifies this robber"), Briefing->Caption.ToString().Contains(TEXT("ROBBER 73")));
		TestTrue(TEXT("Caption retains the suspect's sunglasses"), Briefing->bSuspectWearsShades);
		TestFalse(TEXT("The photo is not a live feed"), Briefing->Capture->bCaptureEveryFrame);

		FlushRenderingCommands();
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		TArray<FColor> CapturePixels;
		Briefing->Photo->GameThread_GetRenderTargetResource()->ReadPixels(CapturePixels, Flags);
		int32 ColoredPixels = 0, TransparentColoredPixels = 0, MinAlpha = 255, MaxAlpha = 0, WhitePixels = 0;
		for (const FColor& Pixel : CapturePixels)
		{
			MinAlpha = FMath::Min(MinAlpha, int32(Pixel.A));
			MaxAlpha = FMath::Max(MaxAlpha, int32(Pixel.A));
			if (FMath::Min3(Pixel.R, Pixel.G, Pixel.B) > 250) ++WhitePixels;
			if (FMath::Max3(Pixel.R, Pixel.G, Pixel.B) > 20)
			{
				++ColoredPixels;
				if (Pixel.A == 0) ++TransparentColoredPixels;
			}
		}
		AddInfo(FString::Printf(TEXT("Robber capture: %d colored pixels, %d with zero alpha"), ColoredPixels, TransparentColoredPixels));
		AddInfo(FString::Printf(TEXT("Capture alpha range: %d..%d"), MinAlpha, MaxAlpha));
		AddInfo(FString::Printf(TEXT("White-clipped pixels in noon sunlight: %d / %d"), WhitePixels, CapturePixels.Num()));
		TestTrue(TEXT("Noon sunlight cannot wash the witness photo to solid white"), WhitePixels < CapturePixels.Num() / 2);
		TestTrue(TEXT("The capture contains the robber mesh"), ColoredPixels > 200);

		const FString Evidence = FPaths::ProjectDir() / TEXT("../Docs/scratchpad/witness-photo");
		IFileManager::Get().MakeDirectory(*Evidence, true);
		auto SaveImage = [&](const TCHAR* Name, int32 Width, int32 Height, TArray<FColor> Pixels)
		{
			// Save RGB evidence even when the source deliberately carries zero alpha.
			for (FColor& Pixel : Pixels) Pixel.A = 255;
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
			TestTrue(TEXT("Rendering evidence saved"), FFileHelper::SaveArrayToFile(Png, *(Evidence / Name)));
		};
		SaveImage(TEXT("capture.png"), 512, 384, CapturePixels);

		FWidgetRenderer Renderer(true);
		const TSharedRef<SWidget> Widget = Briefing->CreatePhotoWidget();
		TestFalse(TEXT("Witness photo cannot steal keyboard focus"), Widget->SupportsKeyboardFocus());
		UTextureRenderTarget2D* Presented = Renderer.DrawWidget(Widget, FVector2D(256, 192));
		if (!TestNotNull(TEXT("The actual photo widget renders offscreen"), Presented)) return;
		TArray<FColor> PresentedPixels;
		Presented->GameThread_GetRenderTargetResource()->ReadPixels(PresentedPixels, Flags);
		int32 VisiblePixels = 0;
		for (const FColor& Pixel : PresentedPixels)
		{
			if (FMath::Max3(Pixel.R, Pixel.G, Pixel.B) > 20) ++VisiblePixels;
		}
		AddInfo(FString::Printf(TEXT("Presented robber: %d visible pixels"), VisiblePixels));
		TestTrue(TEXT("The photograph remains visible through Slate"), VisiblePixels > 50);
		SaveImage(TEXT("photo-widget.png"), 256, 192, PresentedPixels);

		// Moving the subject after the report must not change the saved scene photograph.
		Robber->SetActorLocation(FVector(10000, 0, 24));
		Briefing->TickComponent(1.0f, LEVELTICK_All, nullptr);
		FlushRenderingCommands();
		TArray<FColor> FrozenPixels;
		Briefing->Photo->GameThread_GetRenderTargetResource()->ReadPixels(FrozenPixels, Flags);
		TestTrue(TEXT("The witness photo remains a one-time snapshot"), FrozenPixels == CapturePixels);

		// Recreate the old missing-view-state behavior under exactly the same noon sun.
		Robber->SetActorLocation(FVector(0, 0, 24));
		Briefing->Capture->bAlwaysPersistRenderingState = false;
		Briefing->Capture->CaptureScene();
		FlushRenderingCommands();
		TArray<FColor> LegacyPixels;
		Briefing->Photo->GameThread_GetRenderTargetResource()->ReadPixels(LegacyPixels, Flags);
		int32 LegacyWhite = 0;
		for (const FColor& Pixel : LegacyPixels)
		{
			if (FMath::Min3(Pixel.R, Pixel.G, Pixel.B) > 250) ++LegacyWhite;
		}
		AddInfo(FString::Printf(TEXT("Without exposure history: %d / %d white-clipped pixels"), LegacyWhite, LegacyPixels.Num()));
		TestTrue(TEXT("Removing the fix reproduces the washed-out photo"), LegacyWhite > LegacyPixels.Num() * 9 / 10);
		SaveImage(TEXT("without-exposure-fix.png"), 512, 384, LegacyPixels);

	}));
	return true;
}

#endif

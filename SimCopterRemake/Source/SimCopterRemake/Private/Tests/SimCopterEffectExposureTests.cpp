// Copyright Epic Games, Inc. All Rights Reserved.

#include "City/SimCopterEffectExposure.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "City/SimCopterHangar.h"
#include "City/SimCopterSmokeStacks.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Tickable.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterEffectExposureTest,
	"SimCopter.City.EffectExposure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterEffectExposureTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterEffectExposure;

	// A light component's forward axis points the way the light travels, so the sun overhead points
	// straight DOWN. Getting this backwards is silent - it just makes the effects brightest at
	// midnight - so it is pinned here.
	{
		const float Noon = ComputeGroundIlluminanceLux(120000.0f, FVector(0.0f, 0.0f, -1.0f));
		TestEqual(TEXT("Sun overhead delivers its full intensity"), Noon, 120000.0f);

		const float Underneath = ComputeGroundIlluminanceLux(120000.0f, FVector(0.0f, 0.0f, 1.0f));
		TestEqual(TEXT("A light pointing up lights nothing"), Underneath, 0.0f);

		const float Horizon = ComputeGroundIlluminanceLux(120000.0f, FVector(1.0f, 0.0f, 0.0f));
		TestEqual(TEXT("A light along the horizon lights nothing"), Horizon, 0.0f);

		// 30 degrees above the horizon: cos of the angle from vertical is sin(30) = 0.5.
		const FVector Low = FVector(FMath::Cos(FMath::DegreesToRadians(30.0f)), 0.0f,
			-FMath::Sin(FMath::DegreesToRadians(30.0f)));
		TestNearlyEqual(TEXT("Low sun delivers half"), ComputeGroundIlluminanceLux(120000.0f, Low), 60000.0f, 1.0f);

		TestEqual(TEXT("A dark light delivers nothing"), ComputeGroundIlluminanceLux(0.0f, FVector(0.0f, 0.0f, -1.0f)), 0.0f);
		TestEqual(TEXT("A degenerate direction delivers nothing"), ComputeGroundIlluminanceLux(120000.0f, FVector::ZeroVector), 0.0f);
	}

	// The number that matters: at the celestial vault's 120,000 lux noon an effect card has to emit
	// tens of thousands of nits to read at all. This is the assertion that would have caught the
	// original bug, where the cards emitted 1.4 and tonemapped to black.
	{
		const float Noon = ComputeEffectEmissiveNits(120000.0f);
		TestTrue(TEXT("A noon effect card is tens of thousands of nits"), Noon > 20000.0f);
		TestNearlyEqual(TEXT("Noon card is Brightness x white ground"), Noon,
			120000.0f / UE_PI * DefaultEffectBrightness, 1.0f);
		// The material's own authored 1.4 boost rides on top of this, so the default here is 1 and
		// not 1.4 - the two multiplying would double every effect's brightness.
		TestEqual(TEXT("Default brightness leaves the material's boost alone"), DefaultEffectBrightness, 1.0f);

		// The old day/night actor ran the sun at 4 lux, which is the scale everything unlit in the
		// remake was authored against. That the two answers differ by ~30,000x IS the bug.
		const float OldScale = ComputeEffectEmissiveNits(4.0f);
		TestTrue(TEXT("The old 4 lux sun asks for a tiny fraction of the nits"), OldScale * 10000.0f < Noon);
	}

	// Night has to floor, or the effects go dark exactly when they are the only thing to look at.
	{
		const float Midnight = ComputeEffectEmissiveNits(0.1f);
		TestEqual(TEXT("Moonlight falls back to the floor"), Midnight, DefaultMinimumEmissiveNits);
		TestEqual(TEXT("No light at all still floors"), ComputeEffectEmissiveNits(0.0f), DefaultMinimumEmissiveNits);
		TestEqual(TEXT("Negative illuminance floors"), ComputeEffectEmissiveNits(-5.0f), DefaultMinimumEmissiveNits);
	}

	// Monotonic across the day, so dusk cannot be brighter than noon.
	{
		float Previous = 0.0f;
		for (const float Lux : {0.0f, 100.0f, 1000.0f, 20000.0f, 60000.0f, 120000.0f})
		{
			const float Nits = ComputeEffectEmissiveNits(Lux);
			TestTrue(TEXT("Effect brightness never falls as the sun rises"), Nits >= Previous);
			Previous = Nits;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPausedExposureTest,
	"SimCopter.City.ExposureRefreshWhilePaused", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPausedExposureTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World = Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	bool bDeferredCleanup = false;
	ON_SCOPE_EXIT { if (!bDeferredCleanup) { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); } };
	auto* Rig = World->SpawnActor<AActor>();
	auto* Sun = NewObject<UDirectionalLightComponent>(Rig);
	Sun->SetIntensity(120000); Sun->SetWorldRotation(FRotator(-90,0,0)); Sun->RegisterComponent();
	auto* Moon = NewObject<UDirectionalLightComponent>(Rig);
	Moon->SetIntensity(.1f); Moon->SetWorldRotation(FRotator(90,0,0)); Moon->RegisterComponent();
	auto* Hangar = World->SpawnActor<ASimCopterHangar>(); Hangar->DispatchBeginPlay();
	auto* Shell = CastChecked<UProceduralMeshComponent>(Hangar->GetRootComponent());
	auto* ShellMaterial = CastChecked<UMaterialInstanceDynamic>(Shell->GetMaterial(0));
	auto* Smoke = NewObject<USimCopterSmokeStacksComponent>(Rig); Smoke->RegisterComponent();
	Smoke->bCastPointLights = false;
	TArray<FColor> Palette; Palette.Init(FColor::White,256); FString Error;
	auto* Parent = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_SimCopterSpriteTexture.M_SimCopterSpriteTexture"));
	if (!TestNotNull(TEXT("Smoke uses the shipped card material"),Parent) || !Smoke->InitSmokeAssets(Palette,Parent,Error)) return false;
	USimCopterSmokeStacksComponent::FSmokeMarker Marker; Marker.LocalOffset = FVector(4000,0,0); Marker.EffectClass = 1;
	Smoke->SetSmokeMarkers({Marker}); Smoke->SyncSmoke(0,FVector::ZeroVector);
	UMaterialInstanceDynamic* SmokeMaterial = nullptr;
	for (const auto& Child : Smoke->GetAttachChildren())
		if (auto* Mesh = Cast<UProceduralMeshComponent>(Child.Get())) SmokeMaterial = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!TestNotNull(TEXT("Smoke has its live material"),SmokeMaterial)) return false;
	const auto Nits = [](UMaterialInstanceDynamic* Material) { return Material->K2_GetScalarParameterValue(TEXT("EmissiveNits")); };
	TestTrue(TEXT("Daytime smoke and warehouse start at daylight brightness"),Nits(SmokeMaterial)>10000 && Nits(ShellMaterial)>10000);
	const double FrozenTime = World->GetTimeSeconds();
	World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
	TestTrue(TEXT("Fixture freezes gameplay as Photo Mode does"),World->IsPaused());
	FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(nullptr,World->Scene,FEngineShowFlags(ESFIM_Game))
		.SetTime(FGameTime::CreateDilated(100,1.0/60,42,0)));
	Family.bWorldIsPaused = true;
	for (const auto& Extension : GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(World->Scene)))
		Extension->SetupViewFamily(Family);
	TestTrue(TEXT("Paused exposure receives real frame delta"),Family.Time.GetDeltaWorldTimeSeconds()>0);
	TestEqual(TEXT("Paused shader positions keep their frozen clock"),Family.Time.GetWorldTimeSeconds(),42.0);
	TestTrue(TEXT("Rendering retains the paused flag"),Family.bWorldIsPaused);
	Sun->SetWorldRotation(FRotator(90,0,0)); Moon->SetWorldRotation(FRotator(-90,0,0));
	FTickableGameObject::TickObjects(World,LEVELTICK_PauseTick,true,0);
	TestTrue(TEXT("Paused smoke immediately follows moonlight"),Nits(SmokeMaterial)>0 && Nits(SmokeMaterial)<1);
	TestTrue(TEXT("Paused warehouse immediately loses daylight glow"),Nits(ShellMaterial)<5);
	TArray<UPointLightComponent*> FillLights; Hangar->GetComponents(FillLights);
	for (auto* Light : FillLights) TestTrue(TEXT("Paused hangar fill lights lose daylight intensity"),Light->Intensity<=250);
	TestFalse(TEXT("Lighting refresh cannot open hangar gameplay"),Hangar->IsShellOpen());
	TestEqual(TEXT("Lighting refresh consumes no simulation time"),World->GetTimeSeconds(),FrozenTime);
	Sun->SetWorldRotation(FRotator(-90,0,0)); Moon->SetWorldRotation(FRotator(90,0,0));
	FTickableGameObject::TickObjects(World,LEVELTICK_PauseTick,true,0);
	TestTrue(TEXT("Paused return to noon restores all materials"),Nits(SmokeMaterial)>10000 && Nits(ShellMaterial)>10000);
	World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	Sun->SetWorldRotation(FRotator(90,0,0)); Moon->SetWorldRotation(FRotator(-90,0,0));
	FTickableGameObject::TickObjects(World,LEVELTICK_All,false,0);
	TestTrue(TEXT("Ordinary play refreshes lighting without waiting for actor tick intervals"),Nits(SmokeMaterial)<1 && Nits(ShellMaterial)<5);
	// A frozen effect keeps its authored strength, including a zero-strength/off state.
	auto* Effect = UMaterialInstanceDynamic::Create(Parent,Rig);
	USimCopterEffectExposureSubsystem::ApplyEmissiveNits(Effect,World,true,.65f,2);
	Sun->SetWorldRotation(FRotator(-90,0,0));
	World->GetSubsystem<USimCopterEffectExposureSubsystem>()->RefreshLighting();
	TestNearlyEqual(TEXT("Registered effect retains its authored multiplier"),Nits(Effect),120000.f/UE_PI*.65f,1.f);
	USimCopterEffectExposureSubsystem::ApplyEmissiveNits(Effect,World,true,0);
	World->GetSubsystem<USimCopterEffectExposureSubsystem>()->RefreshLighting();
	TestEqual(TEXT("Lighting cannot reactivate an off effect"),Nits(Effect),0.f);
	Sun->SetVisibility(false);
	World->GetSubsystem<USimCopterEffectExposureSubsystem>()->RefreshLighting();
	TestTrue(TEXT("Invisible cached sun cannot light smoke"),Nits(SmokeMaterial)<1);
	Sun->SetVisibility(true); Sun->SetWorldRotation(FRotator(90,0,0));
	World->GetSubsystem<USimCopterEffectExposureSubsystem>()->RefreshLighting();
	if (!GUsingNullRHI)
	{
		World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
		bDeferredCleanup = true;
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,Instance,World,Rig,Hangar,Smoke,Sun,Moon]()
		{
			auto* Capture = NewObject<USceneCaptureComponent2D>(Rig);
			Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
			Capture->bAlwaysPersistRenderingState = true;
			Capture->CaptureSource = SCS_FinalColorLDR;
			Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
			Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
			Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
			Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
			Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
			Capture->PostProcessSettings.bOverride_AutoExposureBias = true; Capture->PostProcessSettings.AutoExposureBias = 0;
			auto* Target = NewObject<UTextureRenderTarget2D>(Rig); Target->InitAutoFormat(1280,720); Target->UpdateResourceImmediate(true);
			Capture->TextureTarget = Target; Capture->RegisterComponent(); Capture->FOVAngle = 70;
			const FVector Eye(1500,1100,850); Capture->SetWorldLocationAndRotation(Eye,(FVector(0,0,250)-Eye).Rotation());
			USimCopterSmokeStacksComponent::FSmokeMarker Puff; Puff.LocalOffset = FVector(500,-300,700); Puff.EffectClass = 1;
			Smoke->SetSmokeMarkers({Puff}); Smoke->SyncSmoke(0,Eye);
			World->GetSubsystem<USimCopterEffectExposureSubsystem>()->RefreshLighting();
			if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
			FAssetCompilingManager::Get().FinishAllCompilation();
			World->SendAllEndOfFrameUpdates(); FlushRenderingCommands(); Capture->CaptureScene(); FlushRenderingCommands();
			// Material uniforms, scene proxies and shadow history need independent engine frames.
			ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,Instance,World,Capture,Target]()
			{
			ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			int32 White = 0, Visible = 0;
			for (auto& Pixel : Pixels) { Pixel.A = 255; if (Pixel.R>250 && Pixel.G>250 && Pixel.B>250) ++White; if (Pixel.R>10 || Pixel.G>10 || Pixel.B>10) ++Visible; }
			TestTrue(TEXT("Paused night render contains the actual building"),Visible>10000);
			TestTrue(TEXT("Paused night warehouse cannot become a white glare sheet"),White<Pixels.Num()/100);
			TestTrue(TEXT("Night lighting capture saved"),FImageUtils::SaveImageByExtension(
				*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/photo-lighting-refresh/paused-night.png")),FImageView(Pixels.GetData(),1280,720)));
			},.5f));
		},.5f));
	}
	return true;
}

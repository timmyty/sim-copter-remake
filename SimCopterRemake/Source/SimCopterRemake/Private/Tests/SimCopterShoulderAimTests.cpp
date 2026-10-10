#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "ProceduralMeshComponent.h"
#include "AssetCompilingManager.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "ShaderCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterShoulderAimTest, "SimCopter.RescuePilot.ShoulderAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterShoulderAimTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	bool bDeferredCleanup = false;
	ON_SCOPE_EXIT { if (!bDeferredCleanup) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); } };
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>();
	Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay = false;
	Pilot->SetActorLocation(FVector(0, 0, 23));
	if (!TestTrue(TEXT("Original pilot art loads"), Pilot->LoadOriginalBodyFigure())) return false;
	Pilot->CameraBoom->bEnableCameraLag = false;
	auto Step = [Pilot](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Pilot->UpdateBodySprite(1.0f / 60);
			Pilot->UpdateCamera(1.0f / 60);
			Pilot->UpdateTaserPose(1.0f / 60);
			Pilot->CameraBoom->TickComponent(1.0f / 60, LEVELTICK_All, nullptr);
		}
	};
	Step(120);
	const FVector NormalCamera = Pilot->GetActorTransform().InverseTransformPosition(Pilot->CameraComponent->GetComponentLocation());
	const float NormalFov = Pilot->CameraComponent->FieldOfView;
	TestTrue(TEXT("Default camera stays behind the right shoulder"), NormalCamera.X < -80 && NormalCamera.Y > 12);
	const auto RestVertices = Pilot->OriginalBodySpriteComponent->GetProcMeshSection(0)->ProcVertexBuffer;
	Pilot->StartTaserAim();
	Step(5);
	TestTrue(TEXT("Aim raises the arm and moves the camera gradually"), Pilot->TaserAimAlpha > 0 && Pilot->TaserAimAlpha < 1 && Pilot->CameraBoom->TargetArmLength > 60 && Pilot->CameraBoom->TargetArmLength < 130);
	Step(115);
	TestTrue(TEXT("Aiming mesh is visible"), Pilot->TaserMesh && Pilot->TaserMesh->IsVisible());
	TestTrue(TEXT("Aim pose retains original figure"), Pilot->bFigureUsesAimPose && Pilot->bUsingOriginalFigure);
	const FVector AimCamera = Pilot->GetActorTransform().InverseTransformPosition(Pilot->CameraComponent->GetComponentLocation());
	TestTrue(TEXT("Aim closes in while staying right and behind"), AimCamera.X < -45 && AimCamera.X > -65 && AimCamera.Y > 15);
	auto ScreenPosition = [Pilot](const FVector& LocalPoint)
	{
		const FVector View = Pilot->CameraComponent->GetComponentTransform().InverseTransformPosition(Pilot->GetActorTransform().TransformPosition(LocalPoint));
		const double HalfWidth = View.X * FMath::Tan(FMath::DegreesToRadians(Pilot->CameraComponent->FieldOfView * 0.5));
		return FVector2D(0.5 + View.Y / (2 * HalfWidth), 0.5 - View.Z / (2 * HalfWidth * 9.0 / 16.0));
	};
	const FVector2D HeadOnScreen = ScreenPosition(FVector(0, 0, 23));
	TestTrue(TEXT("Upper-torso view keeps the pilot's head on screen, left of center"), HeadOnScreen.X > 0.1 && HeadOnScreen.X < 0.45 && HeadOnScreen.Y > 0.1 && HeadOnScreen.Y < 0.35);
	const FVector PalmWorld = Pilot->OriginalBodySpriteComponent->GetComponentTransform().TransformPosition(Pilot->TaserHandLocal);
	TestTrue(TEXT("Taser grip follows the animated hand"), Pilot->TaserMesh->GetComponentTransform().TransformPosition(FVector(-4,0,-5)).Equals(PalmWorld, 0.01));
	TestTrue(TEXT("Right hand is raised ahead of the chest"), Pilot->TaserHandLocal.X > 9 && Pilot->TaserHandLocal.Y > 4 && Pilot->TaserHandLocal.Z > 27);
	TestTrue(TEXT("Taser barrel follows camera pitch"), FVector::DotProduct(Pilot->TaserMesh->GetForwardVector(), Pilot->CameraComponent->GetForwardVector()) > 0.999);
	const auto& AimVertices = Pilot->OriginalBodySpriteComponent->GetProcMeshSection(0)->ProcVertexBuffer;
	int32 Changed = 0, Unchanged = 0;
	if (TestEqual(TEXT("Pose retains pilot mesh topology"), AimVertices.Num(), RestVertices.Num()))
	{
		for (int32 I = 0; I < RestVertices.Num(); ++I)
		{
			if (AimVertices[I].Position.Equals(RestVertices[I].Position, 0.001)) ++Unchanged;
			else ++Changed;
		}
		TestTrue(TEXT("Arm geometry actually moves"), Changed > 50);
		TestTrue(TEXT("Torso, head and legs retain their original geometry"), Unchanged > Changed * 3);
	}

	// Optional offscreen evidence with the actual pilot/taser materials and gameplay camera.
	// The same test remains usable with -NullRHI for logic/collision checks.
	if (!GUsingNullRHI)
	{
		bDeferredCleanup = true;
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		// Material compilation must be applied by an editor tick before capture. Otherwise
		// freshly loaded procedural components still show the default checker material.
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, World, Pilot, Step]()
		{
		ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		Pilot->PilotHealth = 100; Pilot->CameraPitchDeg = -12;
		Pilot->StartTaserAim(); Step(120);
		auto* Sun = NewObject<UDirectionalLightComponent>(Pilot);
		Sun->SetIntensity(120000); Sun->SetWorldRotation(FRotator(-35, 30, 0)); Sun->RegisterComponent();
		auto* Capture = NewObject<USceneCaptureComponent2D>(Pilot);
		Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true; Capture->CaptureSource = SCS_FinalColorLDR;
		Capture->UnlitViewmode = ESceneCaptureUnlitViewmode::Disabled;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = true;
		Capture->PostProcessSettings.bOverride_CameraISO = true; Capture->PostProcessSettings.CameraISO = 100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed = true; Capture->PostProcessSettings.CameraShutterSpeed = 125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop = true; Capture->PostProcessSettings.DepthOfFieldFstop = 8;
		Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetBloom(false);
		auto* Target = NewObject<UTextureRenderTarget2D>(Pilot);
		Target->RenderTargetFormat = RTF_RGBA8; Target->InitAutoFormat(1280, 720); Target->UpdateResourceImmediate(true);
		Capture->TextureTarget = Target; Capture->RegisterComponent();
		auto Render = [&](const TCHAR* Name)
		{
			Capture->SetWorldLocationAndRotation(Pilot->CameraComponent->GetComponentLocation(), Pilot->CameraComponent->GetComponentRotation());
			Capture->FOVAngle = Pilot->CameraComponent->FieldOfView;
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			for (auto& Pixel : Pixels) Pixel.A = 255;
			FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir() / TEXT("../Docs/scratchpad/shoulder-aim") / Name), FImageView(Pixels.GetData(), 1280, 720));
			return Pixels;
		};
		const auto WithTaser = Render(TEXT("aim.png"));
		Pilot->TaserMesh->SetVisibility(false);
		const auto WithoutTaser = Render(TEXT("aim-without-taser.png"));
		int32 VisiblePixels = 0;
		for (int32 I = 0; I < WithTaser.Num(); ++I)
		{
			const auto& A = WithTaser[I]; const auto& B = WithoutTaser[I];
			if (FMath::Abs(int32(A.R)-B.R) + FMath::Abs(int32(A.G)-B.G) + FMath::Abs(int32(A.B)-B.B) > 30) ++VisiblePixels;
		}
		TestTrue(TEXT("Taser is visible past the body in the rendered aim view"), VisiblePixels > 150);
		AddInfo(FString::Printf(TEXT("Visible taser pixels: %d"), VisiblePixels));
		Pilot->TaserMesh->SetVisibility(true);
		Pilot->StopTaserAim(); Step(120); Render(TEXT("normal.png"));
		Pilot->StartTaserAim(); Step(120);
		}, 0.5f));
	}

	for (const float Pitch : {-62.0f, 14.0f})
	{
		Pilot->CameraPitchDeg = Pitch; Step(2);
		TestTrue(TEXT("Arm and barrel follow extreme aim pitches"), FVector::DotProduct(Pilot->TaserMesh->GetForwardVector(), Pilot->CameraComponent->GetForwardVector()) > 0.999);
		TestTrue(TEXT("Grip remains in hand at extreme pitch"), Pilot->TaserMesh->GetComponentTransform().TransformPosition(FVector(-4,0,-5)).Equals(Pilot->OriginalBodySpriteComponent->GetComponentTransform().TransformPosition(Pilot->TaserHandLocal), 0.01));
	}
	Pilot->CameraPitchDeg = -12;
	Pilot->GetCharacterMovement()->Velocity = FVector(150, 0, 0);
	Step(3); const int32 FirstFrame = Pilot->FigureAnim.CurrentFrame;
	Step(8);
	TestTrue(TEXT("Walking continues under the raised arm pose"), Pilot->FigureAnim.Mnemonic == TEXT("1Wal") && Pilot->FigureAnim.CurrentFrame != FirstFrame && Pilot->bFigureUsesAimPose);
	Pilot->WaterVisualOffsetCm = -12; Step(2);
	TestTrue(TEXT("Taser follows submerged body"), Pilot->TaserMesh->GetComponentTransform().TransformPosition(FVector(-4,0,-5)).Equals(Pilot->OriginalBodySpriteComponent->GetComponentTransform().TransformPosition(Pilot->TaserHandLocal), 0.01));
	Pilot->WaterVisualOffsetCm = 0;
	Pilot->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	Pilot->SetActorRotation(FRotator(0, 90, 0)); Step(120);
	TestTrue(TEXT("Shoulder follows pilot turning"), Pilot->GetActorTransform().InverseTransformPosition(Pilot->CameraComponent->GetComponentLocation()).Y > 10);
	Pilot->SetActorRotation(FRotator::ZeroRotator);
	// Camera collision must include the shoulder offset rather than sweeping a centered boom.
	auto* Obstacle = World->SpawnActor<AActor>();
	auto* Wall = NewObject<UBoxComponent>(Obstacle); Obstacle->SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(2, 80, 80)); Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Wall->SetCollisionResponseToAllChannels(ECR_Ignore); Wall->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	Wall->RegisterComponent(); Obstacle->SetActorLocation(FVector(-20, 0, 35)); Step(2);
	TestTrue(TEXT("Wall pushes shoulder camera forward"), Pilot->CameraComponent->GetComponentLocation().X > -20);
	Obstacle->Destroy();
	Pilot->StopTaserAim(); Step(120);
	TestFalse(TEXT("Release stows taser"), Pilot->TaserMesh->IsVisible());
	TestFalse(TEXT("Release restores normal animation sections"), Pilot->bFigureUsesAimPose);
	TestTrue(TEXT("Release restores shoulder camera and user FOV"), FMath::IsNearlyEqual(Pilot->CameraBoom->TargetArmLength, 130.0f, 0.01f) && Pilot->CameraBoom->SocketOffset.Y > 17 && FMath::IsNearlyEqual(Pilot->CameraComponent->FieldOfView, NormalFov, 0.01f));
	Pilot->StartTaserAim(); Step(20); Pilot->SetParachuteDeployed(true); Step(120);
	TestFalse(TEXT("Parachute cancels aiming"), Pilot->IsTaserAiming());
	TestTrue(TEXT("Parachute retains wide view"), Pilot->CameraBoom->TargetArmLength > 230 && Pilot->CameraBoom->SocketOffset.Y > 17);
	Pilot->SetParachuteDeployed(false);
	Pilot->StartTaserAim(); Step(20); Pilot->UnPossessed(); Step(20);
	TestFalse(TEXT("Losing possession cancels held aim"), Pilot->IsTaserAiming());
	Pilot->PilotHealth = 0; Pilot->StartTaserAim(); TestFalse(TEXT("Incapacitated pilot cannot aim"), Pilot->IsTaserAiming());
	return true;
}
#endif

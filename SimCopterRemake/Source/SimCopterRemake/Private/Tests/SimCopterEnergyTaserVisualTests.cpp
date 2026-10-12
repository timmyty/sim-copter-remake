#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterEnergyTaser.h"
#include "City/SimCopterDayNight.h"
#include "ProceduralMeshComponent.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterEnergyTaserVisualTest,"SimCopter.EnergyTaser.Visuals",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterEnergyTaserVisualTest::RunTest(const FString&)
{
	if (GUsingNullRHI) return true;
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0); World->UpdateParameterCollectionInstances(true,true);
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,Instance,World]()
	{
		ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		// Create physical lighting before the weapon queries this frame's cached exposure.
		auto* Rig=World->SpawnActor<AActor>();
		auto* Sun=NewObject<UDirectionalLightComponent>(Rig); Sun->SetIntensity(120000); Sun->SetWorldRotation(FRotator(-55,-35,0)); Sun->RegisterComponent();
		auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>(); Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay=false;
		Pilot->SetActorLocation(FVector(0,0,24)); Pilot->LoadOriginalBodyFigure(); Pilot->StartTaserAim();
		Pilot->CameraBoom->bEnableCameraLag=false;
		for (int32 I=0;I<120;++I) { Pilot->UpdateBodySprite(1.f/60); Pilot->UpdateCamera(1.f/60); Pilot->UpdateTaserPose(1.f/60); Pilot->CameraBoom->TickComponent(1.f/60,LEVELTICK_All,nullptr); }
		auto* Capture=NewObject<USceneCaptureComponent2D>(Rig); Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
		Capture->bAlwaysPersistRenderingState=true; Capture->CaptureSource=SCS_FinalColorLDR; Capture->FOVAngle=45;
		auto& P=Capture->PostProcessSettings; P.bOverride_AutoExposureMethod=true; P.AutoExposureMethod=AEM_Manual;
		P.bOverride_AutoExposureApplyPhysicalCameraExposure=true; P.AutoExposureApplyPhysicalCameraExposure=true;
		P.bOverride_CameraISO=true; P.CameraISO=100; P.bOverride_CameraShutterSpeed=true; P.CameraShutterSpeed=125;
		P.bOverride_DepthOfFieldFstop=true; P.DepthOfFieldFstop=8;
		Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
		auto* Target=NewObject<UTextureRenderTarget2D>(Rig); Target->RenderTargetFormat=RTF_RGBA8;
		Target->InitAutoFormat(1280,720); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target; Capture->RegisterComponent();
		Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList; Capture->ShowOnlyComponent(Pilot->TaserMesh);
		const FTransform Weapon=Pilot->TaserMesh->GetComponentTransform();
		Capture->SetWorldLocationAndRotation(Weapon.TransformPosition(FVector(39,-65,30)),
			(Weapon.TransformPosition(FVector(1,0,0))-Weapon.TransformPosition(FVector(39,-65,30))).Rotation());
		const auto Render=[&](const TCHAR* Name)
		{
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels); for (auto& Pixel:Pixels) Pixel.A=255;
			if (Name) FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/energy-taser")/Name),FImageView(Pixels.GetData(),1280,720));
			return Pixels;
		};
		Render(nullptr); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); FAssetCompilingManager::Get().FinishAllCompilation();
		const auto Idle=Render(TEXT("weapon-idle.png"));
		auto* SightTarget=World->SpawnActor<ASimCopterGroundAgent>(); SightTarget->InitialPersonState=13;
		SightTarget->BehaviorContext.ResetToState(13);
		SightTarget->SetActorLocation(Pilot->CameraComponent->GetComponentLocation()+Pilot->CameraComponent->GetForwardVector()*350);
		Pilot->UpdateEnergyTaser(.7f);
		Render(nullptr); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); FAssetCompilingManager::Get().FinishAllCompilation();
		const auto Charged=Render(TEXT("weapon-charged.png"));
		int32 Changed=0,Teal=0;
		for (int32 I=0;I<Charged.Num();++I)
		{
			if (FMath::Abs(int32(Charged[I].R)-Idle[I].R)+FMath::Abs(int32(Charged[I].G)-Idle[I].G)+FMath::Abs(int32(Charged[I].B)-Idle[I].B)>30) ++Changed;
			if (Charged[I].G>100 && Charged[I].B>80 && Charged[I].G>Charged[I].R*1.3f) ++Teal;
		}
		TestTrue(TEXT("Actual materials render a visible weapon transformation"),Changed>1500);
		TestTrue(TEXT("Charged weapon visibly emits blue-green light"),Teal>300);
		SightTarget->SetActorHiddenInGame(true); Pilot->UpdateEnergyTaser(0);
		const auto Expired=Render(TEXT("weapon-expired.png"));
		int64 Difference=0; for (int32 I=0;I<Expired.Num();++I)
			Difference+=FMath::Abs(int32(Expired[I].R)-Idle[I].R)+FMath::Abs(int32(Expired[I].G)-Idle[I].G)+FMath::Abs(int32(Expired[I].B)-Idle[I].B);
		// Shadow sampling jitters edges between captures. Compare mean channel error,
		// with the actual orb section and deployment state checked independently.
		TestTrue(TEXT("Expired weapon returns to its closed appearance"),double(Difference)/(Expired.Num()*3)<1.0);
		TestTrue(TEXT("Retraction removes the orb and closes all emitters"),Pilot->TaserCharge.Deployment==0 &&
			(!Pilot->TaserMesh->GetProcMeshSection(2) || Pilot->TaserMesh->GetProcMeshSection(2)->ProcVertexBuffer.IsEmpty()));
		SightTarget->SetActorHiddenInGame(false); Pilot->UpdateEnergyTaser(.7f);
		Capture->ClearShowOnlyComponents(); Capture->ShowOnlyActorComponents(Pilot);
		Capture->SetWorldLocationAndRotation(Pilot->CameraComponent->GetComponentLocation(),Pilot->CameraComponent->GetComponentRotation());
		Capture->FOVAngle=Pilot->CameraComponent->FieldOfView;
		const auto Shoulder=Render(TEXT("shoulder-charged.png"));
		Pilot->TaserMesh->SetVisibility(false); const auto NoWeapon=Render(nullptr);
		int32 Visible=0; for (int32 I=0;I<Shoulder.Num();++I)
			if (FMath::Abs(int32(Shoulder[I].R)-NoWeapon[I].R)+FMath::Abs(int32(Shoulder[I].G)-NoWeapon[I].G)+FMath::Abs(int32(Shoulder[I].B)-NoWeapon[I].B)>30) ++Visible;
		TestTrue(TEXT("Redesigned weapon and charge remain visible from actual shoulder aim"),Visible>150);
		AddInfo(FString::Printf(TEXT("Transformation pixels: %d; teal pixels: %d; shoulder weapon pixels: %d"),Changed,Teal,Visible));
		// Render the actual traveling actor and its contact effect as well as the held charge.
		auto* Person=World->SpawnActor<ASimCopterGroundAgent>();
		Person->SetInitialBehaviorClass(15); Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT(""),TEXT(""),0);
		Person->InitialPersonState=13; Person->BehaviorContext.ResetToState(13); Person->SetActorLocation(FVector(100,0,24));
		auto* Orb=World->SpawnActor<ASimCopterTaserOrb>(); Orb->Launch(Pilot,Person,FVector(50,0,24),FVector::ForwardVector);
		Capture->ClearShowOnlyComponents(); Capture->ShowOnlyActorComponents(Person); Capture->ShowOnlyActorComponents(Orb);
		Capture->SetWorldLocationAndRotation(FVector(80,-125,65),(FVector(80,0,24)-FVector(80,-125,65)).Rotation()); Capture->FOVAngle=45;
		Render(nullptr); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); FAssetCompilingManager::Get().FinishAllCompilation();
		Render(TEXT("orb-flight.png"));
		for (int32 I=0;I<40 && !Orb->HasImpacted();++I) Orb->Tick(.01f);
		TestTrue(TEXT("Rendered orb reaches and stuns the actual suspect"),Person->IsTaserStunned());
		Render(TEXT("orb-impact.png"));
		Orb->Tick(.71f); TestTrue(TEXT("Impact actor expires after its electrical effect"),Orb->IsActorBeingDestroyed());
	},.5f));
	return true;
}
#endif

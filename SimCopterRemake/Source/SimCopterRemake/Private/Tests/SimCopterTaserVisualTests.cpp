#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Game/SimCopterPlayerController.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "City/SimCopterDayNight.h"
#include "ProceduralMeshComponent.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterTaserVisualTest,"SimCopter.AirOperations.TaserVisuals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterTaserVisualTest::RunTest(const FString&)
{
	if (GUsingNullRHI) return true; // The runtime firing test covers headless state/audio/lifetimes.
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);
	World->UpdateParameterCollectionInstances(true,true);
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,Instance,World]()
	{
		ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>(); Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RemoteTaser, true); Heli->SetActorHiddenInGame(true);
		auto* Car=World->SpawnActor<ASimCopterGroundAgent>();
		Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT("AUTO"),SimCopterOriginalGame::ResolveRoot(),0);
		Car->SetActorLocationAndRotation(FVector(650,0,55),FRotator(0,25,0));
		auto* Rig=World->SpawnActor<AActor>();
		auto* Sun=NewObject<UDirectionalLightComponent>(Rig); Sun->SetIntensity(120000);
		Sun->SetWorldRotation(FRotator(-55,-35,0)); Sun->RegisterComponent();
		auto* Capture=NewObject<USceneCaptureComponent2D>(Rig);
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->bAlwaysPersistRenderingState=true;
		Capture->CaptureSource=SCS_FinalColorLDR; Capture->FOVAngle=62;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->UnlitViewmode=ESceneCaptureUnlitViewmode::Disabled;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
		Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=0;
		Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
		auto* Target=NewObject<UTextureRenderTarget2D>(Rig); Target->RenderTargetFormat=RTF_RGBA8;
		Target->InitAutoFormat(1280,720); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target;
		Capture->RegisterComponent(); Capture->SetWorldLocationAndRotation(FVector(480,-650,400),
			(FVector(460,0,85)-FVector(480,-650,400)).Rotation());
		auto Render=[&]()
		{
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			for (auto& Pixel:Pixels) Pixel.A=255;
			return Pixels;
		};
		Render(); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FAssetCompilingManager::Get().FinishAllCompilation();
		const auto Before=Render();
		auto* Ops=Heli->GetAirOperations();
		Ops->DrawRemoteTaserDischarge(FVector(0,0,250),Car->GetActorLocation(),true);
		// Keep the owner visible; the capture hides its body, not its separately owned effects.
		Heli->SetActorHiddenInGame(false);
		Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyActorComponents(Car); Capture->ShowOnlyComponent(Ops->RemoteTaserTrail);
		Ops->TaserImpactTarget=Car; Ops->TaserImpactSeconds=.7f; Ops->UpdateTaserImpact(0);
		Render(); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FAssetCompilingManager::Get().FinishAllCompilation();
		const auto Discharge=Render();
		int32 White=0;
		for (int32 I=0; I<Discharge.Num(); ++I)
			if (Discharge[I].R>190 && Discharge[I].G>190 && Discharge[I].B>190 && Discharge[I]!=Before[I]) ++White;
		TestTrue(TEXT("White discharge lines are visible in a rendered view"),White>200);
		const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/taser-and-lot");
		FImageUtils::SaveImageByExtension(*(Folder/TEXT("taser-impact.png")),FImageView(Discharge.GetData(),1280,720));
		Ops->UpdateRemoteTaserDischarge(.38f); const auto Arcs=Render();
		FImageUtils::SaveImageByExtension(*(Folder/TEXT("taser-car-arcs.png")),FImageView(Arcs.GetData(),1280,720));
		Ops->UpdateRemoteTaserDischarge(.4f); const auto After=Render();
		int32 ArcPixels=0;
		for (int32 I=0; I<Arcs.Num(); ++I) if (Arcs[I].R>190 && Arcs[I].G>190 && Arcs[I].B>190 && Arcs[I]!=After[I]) ++ArcPixels;
		TestTrue(TEXT("Electricity remains visible around the car after the shot trail fades"),ArcPixels>100);

		Car->SetActorHiddenInGame(true);
		auto* Person=World->SpawnActor<ASimCopterGroundAgent>();
		Person->SetInitialBehaviorClass(1);
		Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT(""),SimCopterOriginalGame::ResolveRoot(),0);
		Person->SetActorLocationAndRotation(FVector(650,0,25),FRotator(0,-25,0));
		Capture->ClearShowOnlyComponents();
		Capture->ShowOnlyActorComponents(Person); Capture->ShowOnlyComponent(Ops->RemoteTaserTrail);
		Capture->SetWorldLocationAndRotation(FVector(460,-240,145),
			(FVector(650,0,25)-FVector(460,-240,145)).Rotation());
		for (const FString Pose : {FString(TEXT("NoMo")),FString(TEXT("Inju"))})
		{
			TestTrue(TEXT("Original pedestrian pose loads"),Person->SetForcedPedestrianFigureClip(Pose));
			Ops->TaserImpactTarget=Person; Ops->TaserImpactSeconds=.7f;
			Ops->UpdateRemoteTaserDischarge(.35f);
			Render(); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
			FAssetCompilingManager::Get().FinishAllCompilation();
			const auto Shock=Render();
			Ops->UpdateRemoteTaserDischarge(.36f); const auto NoShock=Render();
			int32 ShockPixels=0;
			for (int32 I=0; I<Shock.Num(); ++I)
				if (Shock[I].R>190 && Shock[I].G>190 && Shock[I].B>190 && Shock[I]!=NoShock[I]) ++ShockPixels;
			TestTrue(*FString::Printf(TEXT("White body shock is visible on the %s pedestrian"),*Pose),ShockPixels>100);
			TestTrue(TEXT("Pedestrian shock preview saved"),FImageUtils::SaveImageByExtension(
				*(Folder/FString::Printf(TEXT("pedestrian-%s.png"),*Pose)),FImageView(Shock.GetData(),1280,720)));
		}

		// The previous side-on preview never entered the real remote sight. Its hidden-actor
		// list used to remove the helicopter-owned trail and impact mesh from the player's view.
		Person->SetActorHiddenInGame(true); Car->SetActorHiddenInGame(false);
		Heli->SetActorLocation(FVector(0,0,1300));
		Car->SetActorLocation(FVector(2200,0,55));
		auto* PC=World->SpawnActor<ASimCopterPlayerController>();
		PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC); PC->Possess(Heli);
		Ops->ToggleRemoteTaser();
		TestTrue(TEXT("Preview uses active remote sight with the airframe hidden"),
			Ops->IsRemoteTaserActive() && PC->HiddenActors.Contains(Heli));
		Ops->RemoteTaserAim=(Car->GetActorLocation()-Ops->RemoteTaserMount->GetComponentLocation()).Rotation();
		Ops->UpdateRemoteTaserMount();
		Capture->ClearShowOnlyComponents(); Capture->ShowOnlyActorComponents(Car);
		Capture->ShowOnlyComponent(Ops->RemoteTaserTrail);
		for (AActor* HiddenActor:PC->HiddenActors) Capture->HideActorComponents(HiddenActor);
		Capture->FOVAngle=45;
		Capture->SetWorldLocationAndRotation(Ops->GetRemoteTaserMuzzle(),Ops->GetRemoteTaserDirection().Rotation());
		const auto SightBefore=Render();
		Ops->FireRemoteTaser();
		TestTrue(TEXT("Actual remote sight shot hits the vehicle"),Ops->TaserImpactTarget.Get()==Car);
		// Count each effect separately from this muzzle-aligned view, at gameplay distance.
		Ops->RemoteTaserTrail->SetMeshSectionVisible(1,false);
		const auto SightTrail=Render();
		Ops->RemoteTaserTrail->SetMeshSectionVisible(1,true);
		FImageUtils::SaveImageByExtension(*(Folder/TEXT("remote-sight-discharge.png")),FImageView(SightTrail.GetData(),1280,720));
		Ops->UpdateRemoteTaserDischarge(.35f); const auto SightShock=Render();
		FImageUtils::SaveImageByExtension(*(Folder/TEXT("remote-sight-car-shock.png")),FImageView(SightShock.GetData(),1280,720));
		int32 SightTrailPixels=0, SightShockPixels=0;
		for (int32 I=0; I<SightBefore.Num(); ++I)
		{
			const auto IsWhiteChange=[&](const FColor& Pixel)
			{ return Pixel.R>190 && Pixel.G>190 && Pixel.B>190 && Pixel!=SightBefore[I]; };
			SightTrailPixels+=IsWhiteChange(SightTrail[I]); SightShockPixels+=IsWhiteChange(SightShock[I]);
		}
		AddInfo(FString::Printf(TEXT("Remote sight white pixels: discharge=%d, vehicle shock=%d"),SightTrailPixels,SightShockPixels));
		TestTrue(TEXT("Remote sight displays the muzzle discharge"),SightTrailPixels>100);
		TestTrue(TEXT("Discharge remains thin lines rather than an opaque muzzle block"),SightTrailPixels<20000);
		TestTrue(TEXT("Remote sight displays electricity on the hit vehicle"),SightShockPixels>50);
		Ops->EndRemoteTaser();
	},1.0f));
	return true;
}
#endif

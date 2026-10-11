#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "City/SimCopterDayNight.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Missions/SimCopterMissionSystem.h"
#include "UI/SSimCopterToolFlaps.h"
#include "UI/SimCopterHangarArt.h"
#include "ProceduralMeshComponent.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCaptureCageTest,"SimCopter.CaptureCage.ControlsMountsAndMissionWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCaptureCageTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	bool bDeferredCleanup=false;
	ON_SCOPE_EXIT { if (!bDeferredCleanup) { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); } };
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Ops=Heli->GetAirOperations();
	auto Flaps=SNew(SSimCopterToolFlaps).Pawn(Heli);
	using namespace SimCopterFlapLayout;
	const int32 CageMask=SimCopterHelicopterRegistry::GetToolCareerBit(ESimCopterHelicopterTool::CaptureCage);
	TestTrue(TEXT("Unpurchased cage panel is collapsed"),Flaps->GetFlapVisibility(CageMask)==EVisibility::Collapsed);
	Flaps->PressAction(EAction::CageLower);
	TestFalse(TEXT("Unpurchased lower cannot deploy"),Ops->IsDeployed());
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,true);
	TestTrue(TEXT("Purchase immediately reveals panel"),Flaps->GetFlapVisibility(CageMask)==EVisibility::SelfHitTestInvisible);
	TestFalse(TEXT("Cage panel preserves flight keyboard focus"),Flaps->SupportsKeyboardFocus());
	for (int32 Model=0; Model<9; ++Model)
	{
		TestTrue(FString::Printf(TEXT("Helicopter %d loads"),Model),Heli->SwitchHelicopterModel(Model));
		Ops->Update(0);
		FBox Bounds;
		TestTrue(TEXT("Airframe bounds available"),Heli->TryGetAirframeLocalBoundsCm(Bounds));
		TestTrue(TEXT("Stowed skid hardware exists before deployment"),Ops->CageMountMesh && Ops->CageMountMesh->IsVisible());
		TestTrue(TEXT("Mount follows current airframe"),Ops->CageMountMesh->GetComponentTransform().Equals(Heli->GetCaptureCageMountTransform(),0.01f));
	}
	Flaps->PressAction(EAction::CageLower);
	TestTrue(TEXT("LOWER directly deploys cage"),Ops->IsDeployed() && Ops->IsCage());
	TestEqual(TEXT("Mouse hold persists separately from keyboard/controller input"),Ops->GetCagePanelInput(),-1.0f);
	Ops->SetCableInput(Ops->GetCagePanelInput()); Ops->Update(0.2f);
	TestTrue(TEXT("Held LOWER pays out"),Ops->GetCableLength()>140);
	TestTrue(TEXT("Formation is animated"),Ops->CageFormation>0 && Ops->CageFormation<1);
	Flaps->ReleaseAction(EAction::CageLower);
	TestEqual(TEXT("Mouse release clears hold"),Ops->GetCagePanelInput(),0.0f);
	Ops->SetCableInput(0); Ops->Update(0.3f);
	TestFalse(TEXT("Empty automatic search keeps floor open"),Ops->TryGrabCargo());
	TestTrue(TEXT("Empty floor remains open"),Ops->bCageOpen);
	Flaps->PressAction(EAction::CageRaise);
	Heli->SetSelectedTool(ESimCopterHelicopterTool::Megaphone);
	TestEqual(TEXT("Tool switch cancels mouse hold"),Ops->GetCagePanelInput(),0.0f);
	Ops->CableLength=140; Ops->SetCableInput(1); Ops->Update(0.01f);
	TestFalse(TEXT("Empty cage retracts at top"),Ops->IsDeployed());
	Ops->SetCableInput(0); Ops->Update(0.5f);
	TestTrue(TEXT("Skid hardware remains after stow"),Ops->CageMountMesh->IsVisible());
	TestFalse(TEXT("Extended bars disappear after stow animation"),Ops->CargoMesh->IsVisible());
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,false); Ops->Update(0);
	TestFalse(TEXT("Selling removes skid hardware"),Ops->CageMountMesh->IsVisible());
	TestTrue(TEXT("Selling collapses the panel"),Flaps->GetFlapVisibility(CageMask)==EVisibility::Collapsed);

	SimCopterMissions::FSimCopterMissionSystem Missions;
	Missions.Initialize(nullptr,42);
	SimCopterMissions::FSimCopterCareerCity City;
	for (float& Weight:City.Weights) Weight=10;
	Missions.SetCareerCity(City);
	const auto CrimeShare=[&] { const int32* Weights=Missions.GetCumulativeWeightTable(); return Weights[2]-Weights[1]; };
	const int32 Before=CrimeShare();
	Missions.SetCaptureCageOwned(true);
	TestTrue(TEXT("Buying cage increases crime share substantially"),CrimeShare()>Before*2);
	TestEqual(TEXT("Purchase preserves saved city weights"),Missions.GetCareerCity().Weights[1],10.0f);
	const int32 After=CrimeShare();
	Missions.SetCaptureCageOwned(true);
	TestEqual(TEXT("Repeated ownership refresh cannot compound the boost"),CrimeShare(),After);
	Missions.SetCaptureCageOwned(false);
	TestEqual(TEXT("Selling restores original crime share"),CrimeShare(),Before);
	City.Weights[1]=0; Missions.SetCareerCity(City); Missions.SetCaptureCageOwned(true);
	TestTrue(TEXT("Early cities gain crime jobs after cage purchase"),CrimeShare()>0);
	for (float& Weight:City.Weights) Weight=0;
	Missions.SetCareerCity(City);
	TestEqual(TEXT("Free roam/all-zero city remains quiet"),Missions.GetCumulativeWeightTable()[7],0);

	if (!FParse::Param(FCommandLine::Get(),TEXT("SimCaptureCagePreview"))) return true;
	TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
	Art->SetOriginalGameRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame")));
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,true);
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::Megaphone,true);
	for (float HudScale : {0.75f,1.0f,1.25f})
	{
		auto Panel=SNew(SSimCopterToolFlaps).Pawn(Heli).Art(Art.Get()).Scale(1.768292f*HudScale).HudScale(HudScale);
		FWidgetRenderer Renderer(true);
		auto* Target=Renderer.DrawWidget(Panel,FVector2D(450*HudScale,900*HudScale));
		if (!TestNotNull(TEXT("Cage controls render with original panel art"),Target)) return false;
		const auto& Panels=Panel->MissionMarkerAvoidancePanels;
		TestEqual(TEXT("Cage and remote taser join marker avoidance column"),Panels.Num(),8);
		TestTrue(TEXT("Cage lies below megaphone without overlap"),
			Panels[5]->GetCachedGeometry().GetAbsolutePosition().Y >= Panels[4]->GetCachedGeometry().GetAbsolutePosition().Y + Panels[4]->GetCachedGeometry().GetAbsoluteSize().Y);
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,Png);
		TestTrue(TEXT("Panel preview saved"),FFileHelper::SaveArrayToFile(Png,*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/capture-cage")/FString::Printf(TEXT("controls-%.2f.png"),HudScale))));
	}
	// Render the actual procedural fittings, deployed bars and closing grille without
	// opening a foreground game or synthesizing player input.
	World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);
	World->UpdateParameterCollectionInstances(true,true);
	bDeferredCleanup=true;
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,Instance,World,Heli,Ops]()
	{
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	Heli->SwitchHelicopterModel(4);
	Heli->SetActorLocation(FVector(0,0,500));
	Heli->SetSelectedTool(ESimCopterHelicopterTool::CaptureCage);
	Ops->UpdateCageMount();
	auto* Rig=World->SpawnActor<AActor>();
	auto* Sun=NewObject<UDirectionalLightComponent>(Rig);
	Sun->SetIntensity(120000); Sun->SetWorldRotation(FRotator(-35,145,0)); Sun->RegisterComponent();
	auto* Capture=NewObject<USceneCaptureComponent2D>(Rig);
	Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
	// Base color isolates geometry/fit from an empty preview world's missing skylight.
	Capture->bAlwaysPersistRenderingState=true; Capture->CaptureSource=SCS_BaseColor;
	Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
	Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
	Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
	Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
	Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetBloom(false);
	Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActorComponents(Heli);
	auto* Target=NewObject<UTextureRenderTarget2D>(Rig);
	Target->RenderTargetFormat=RTF_RGBA8; Target->InitAutoFormat(800,800); Target->UpdateResourceImmediate(true);
	Capture->TextureTarget=Target; Capture->RegisterComponent();
	const FVector Mount=Heli->GetCaptureCageMountTransform().GetLocation();
	const FVector Eye=Mount+FVector(500,600,180);
	Capture->SetWorldLocationAndRotation(Eye,(Mount-FVector(0,0,100)-Eye).Rotation()); Capture->FOVAngle=48;
	World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	FAssetCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
	for (int32 Stage=0; Stage<4; ++Stage)
	{
		Ops->bCage=true; Ops->bDeployed=Stage!=0;
		Ops->CageFormation=Stage==0?0.0f:Stage==1?0.4f:1.0f;
		Ops->CageSeal=Stage==3?1.0f:0.0f; Ops->bCageOpen=Stage!=3;
		Ops->CargoLocation=Mount-FVector(0,0,280);
		Ops->DrawCargo();
		World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
		TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		int32 Colored=0;
		for (auto& Pixel:Pixels)
		{
			Pixel.A=255;
			if (FMath::Max3(Pixel.R,Pixel.G,Pixel.B)-FMath::Min3(Pixel.R,Pixel.G,Pixel.B)>25) ++Colored;
		}
		TestTrue(TEXT("Actual aircraft and cage render in color"),Colored>100);
		const FString Output=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/capture-cage")/FString::Printf(TEXT("cage-stage-%d.png"),Stage);
		TestTrue(TEXT("Cage preview saved"),FImageUtils::SaveImageByExtension(*Output,FImageView(Pixels.GetData(),800,800)));
	}
	},1.0f));
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "City/SimCopterHangar.h"
#include "City/SimCopterDayNight.h"
#include "City/SimCopterEffectExposure.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ProceduralMeshComponent.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "../City/SimCopterHangarInterior.h"
#include "Formats/SimCopterOriginalGamePaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterHangarInteriorTest,"SimCopter.City.HangarInterior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterHangarInteriorTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone();
	auto* World=Instance->GetWorld();
	World->InitializeActorsForPlay(FURL());
	// Install the key light before the hangar first samples exposure. The exposure subsystem
	// intentionally caches missing-light scans for two seconds in menu/test worlds.
	auto* Rig=World->SpawnActor<AActor>();
	auto* Sun=NewObject<UDirectionalLightComponent>(Rig);
	Sun->SetIntensity(40000); Sun->SetWorldRotation(FRotator(-42,-65,0)); Sun->RegisterComponent();
	bool bDeferredCleanup=false;
	ON_SCOPE_EXIT
	{
		if (!bDeferredCleanup) { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
	};
	auto* Hangar=World->SpawnActor<ASimCopterHangar>();
	Hangar->DispatchBeginPlay();
	UProceduralMeshComponent* Props=nullptr;
	TArray<UProceduralMeshComponent*> Meshes;
	Hangar->GetComponents(Meshes);
	for (auto* Mesh : Meshes) if (Mesh->GetFName()==TEXT("InteriorMesh")) Props=Mesh;
	if (!TestNotNull(TEXT("The world hangar owns its interior"),Props)) return false;
	TestEqual(TEXT("Furniture and both aircraft are present"),Props->GetNumSections(),8);
	TestEqual(TEXT("Scenery cannot block the entry trigger or aircraft traffic"),Props->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Looking through the doorway does not open the menu"),Hangar->IsInsideEntryVolume(FVector(405,0,85)));
	TestTrue(TEXT("Walking inside still reaches the menu trigger"),Hangar->IsInsideEntryVolume(FVector(240,0,85)));
	// Rebuilding for a city's tile size must replace the geometry and keep every prop within
	// the shell, resting on its floor. This also catches rogue rotor/headlight blur cards.
	for (float TileSize : {400.0f,600.0f,400.0f})
	{
		SimCopterHangarInterior::Build(*Props,Props->GetMaterial(0),SimCopterOriginalGame::ResolveRoot(),TileSize);
		TestEqual(TEXT("Rebuild does not duplicate props"),Props->GetNumSections(),8);
		for (int32 I=0; I<Props->GetNumSections(); ++I)
		{
			const auto* Section=Props->GetProcMeshSection(I);
			const FBox Bounds=Section->SectionLocalBox;
			const float HalfSize=0.95f*TileSize;
			TestTrue(*FString::Printf(TEXT("Prop %d stays inside the warehouse"),I),
				Bounds.Min.X>-HalfSize && Bounds.Max.X<HalfSize && Bounds.Min.Y>-HalfSize && Bounds.Max.Y<HalfSize &&
				Bounds.Min.Z>=SimCopterHangarPlacement::FloorLiftCm-0.01f && Bounds.Max.Z<TileSize*0.5f);
			TestFalse(TEXT("No prop section has collision"),Section->bEnableCollision);
		}
	}
	TestTrue(TEXT("Tool cabinet is human sized, below a standing pilot's head"),
		Props->GetProcMeshSection(1)->SectionLocalBox.GetSize().Z<46);
	TestTrue(TEXT("Desk is at a pilot's waist, not taller than the pilot"),
		Props->GetProcMeshSection(2)->SectionLocalBox.GetSize().Z<35);
	UProceduralMeshComponent* Shell=nullptr; UProceduralMeshComponent* Glass=nullptr;
	for (auto* Mesh : Meshes)
	{
		if (Mesh->GetFName()==TEXT("ShellMesh")) Shell=Mesh;
		if (Mesh->GetFName()==TEXT("GlazingMesh")) Glass=Mesh;
	}
	TestTrue(TEXT("Architecture includes windows, roof steel, workshop detail and caution signs"),Shell && Shell->GetNumSections()==6 && Glass && Glass->GetNumSections()==1);
	TestTrue(TEXT("Tall ridge matches the spacious warehouse reference"),Shell && Shell->Bounds.BoxExtent.Z*2>450);
	if (GUsingNullRHI) return true;
	World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);
	World->UpdateParameterCollectionInstances(true,true);
	bDeferredCleanup=true;
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,Instance,World,Hangar,Props,Rig]()
	{
		ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		Hangar->Tick(0);
		TestTrue(TEXT("Interior fill samples the registered daylight"),
			USimCopterEffectExposureSubsystem::GetEffectEmissiveNitsForWorld(World)>1000);
		AddInfo(FString::Printf(TEXT("Daylight fill: %.1f nits, prop material: %.1f nits"),
			USimCopterEffectExposureSubsystem::GetEffectEmissiveNitsForWorld(World),
			Cast<UMaterialInstanceDynamic>(Props->GetMaterial(0))->K2_GetScalarParameterValue(TEXT("EmissiveNits"))));
		auto* Capture=NewObject<USceneCaptureComponent2D>(Rig);
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
		Capture->bAlwaysPersistRenderingState=true; Capture->CaptureSource=SCS_BaseColor;
		Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetBloom(false);
		Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyActorComponents(Hangar);
		auto* Target=NewObject<UTextureRenderTarget2D>(Rig);
		Target->RenderTargetFormat=RTF_RGBA8; Target->InitAutoFormat(1280,720); Target->UpdateResourceImmediate(true);
		Capture->TextureTarget=Target; Capture->RegisterComponent(); Capture->FOVAngle=88;
		auto Render=[&]()
		{
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			for (auto& Pixel:Pixels) Pixel.A=255;
			return Pixels;
		};
		auto Difference=[](const TArray<FColor>& A,const TArray<FColor>& B)
		{
			int32 Changed=0;
			for (int32 I=0; I<A.Num(); ++I)
				if (FMath::Abs(int32(A[I].R)-B[I].R)+FMath::Abs(int32(A[I].G)-B[I].G)+FMath::Abs(int32(A[I].B)-B[I].B)>20) ++Changed;
			return Changed;
		};
		const FVector Views[]={FVector(420,0,50),FVector(520,180,65),FVector(520,-180,65),FVector(-800,0,105)};
		for (int32 View=0; View<4; ++View)
		{
			Capture->SetWorldLocationAndRotation(Views[View],(FVector(-200,0,110)-Views[View]).Rotation());
			Render();
			if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
			FAssetCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
			const auto Furnished=Render();
			Props->SetVisibility(false); const auto Empty=Render(); Props->SetVisibility(true);
			const int32 Visible=Difference(Furnished,Empty);
			AddInfo(FString::Printf(TEXT("View %d: %d visible interior pixels"),View,Visible));
			if (View==3) TestEqual(TEXT("Opaque rear wall hides the interior"),Visible,0);
			else TestTrue(TEXT("Interior is visible from outside the doorway"),Visible>1000);
			const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/hangar-reference-match");
			TestTrue(TEXT("Exterior view saved"),FImageUtils::SaveImageByExtension(*(Folder/FString::Printf(TEXT("outside-%d.png"),View)),FImageView(Furnished.GetData(),1280,720)));
			if (View==0)
			{
				FImageUtils::SaveImageByExtension(*(Folder/TEXT("outside-before.png")),FImageView(Empty.GetData(),1280,720));
				for (int32 Section : {0,1,2,3,4,6,7})
				{
					Props->SetMeshSectionVisible(Section,false); const auto Without=Render(); Props->SetMeshSectionVisible(Section,true);
					const int32 Pixels=Difference(Furnished,Without);
					AddInfo(FString::Printf(TEXT("Prop %d: %d visible pixels through the doorway"),Section,Pixels));
					TestTrue(*FString::Printf(TEXT("Prop %d is visible through the real shell"),Section),Pixels>200);
				}
			}
		}
		Capture->SetWorldLocationAndRotation(Views[0],(FVector(-200,0,110)-Views[0]).Rotation());
		Capture->CaptureSource=SCS_Normal;
		const auto NormalPixels=Render();
		TestTrue(TEXT("The visible rear wall is lit from inside the warehouse"),NormalPixels[250*1280+600].R>240);
		TestTrue(TEXT("The visible floor faces the ceiling lights"),NormalPixels[660*1280+640].B>240);
		FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/hangar-reference-match/normals.png")),FImageView(NormalPixels.GetData(),1280,720));
		Capture->CaptureSource=SCS_FinalColorLDR;
		Target->InitAutoFormat(2560,1440); Target->UpdateResourceImmediate(true);
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;
		Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->UnlitViewmode=ESceneCaptureUnlitViewmode::Disabled;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
		Capture->PostProcessSettings.bOverride_AutoExposureBias=true;
		Capture->PostProcessSettings.AutoExposureBias=0;
		Capture->SetWorldLocationAndRotation(Views[0],(FVector(-200,0,110)-Views[0]).Rotation());
		Render(); const auto Lit=Render();
		for (int32 Section : {0,1,2,3,4,6,7})
		{
			Props->SetMeshSectionVisible(Section,false); const auto Without=Render(); Props->SetMeshSectionVisible(Section,true);
			TestTrue(*FString::Printf(TEXT("Prop %d remains readable with real lighting and exposure"),Section),Difference(Lit,Without)>150);
		}
		TestTrue(TEXT("Lit exterior view saved"),FImageUtils::SaveImageByExtension(
			*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/hangar-reference-match/outside-lit.png")),FImageView(Lit.GetData(),2560,1440)));
		Capture->SetWorldLocationAndRotation(FVector(1200,820,490),(FVector(0,0,180)-FVector(1200,820,490)).Rotation());
		Render(); const auto Exterior=Render();
		FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/hangar-reference-match/exterior.png")),FImageView(Exterior.GetData(),2560,1440));
	},1.0f));
	return true;
}
#endif

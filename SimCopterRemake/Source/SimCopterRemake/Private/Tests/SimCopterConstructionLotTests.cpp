#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "StaticMeshResources.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterDayNight.h"
#include "Materials/MaterialInterface.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterConstructionLotTest,"SimCopter.City.ConstructionLot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterConstructionLotTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bRenderProceduralMapExtension=false;
	City->bRenderStreetLightSpotLights=false;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	City->CityFile.FilePath=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame/cities/career/city0.sc2"));
	City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	TArray<UInstancedStaticMeshComponent*> Parts; City->GetBuildingModelComponents(0x89,Parts);
	if (!TestTrue(TEXT("City0 contains the pictured CA137 construction lot"),Parts.Num()>0))
	{
		Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false;
	}
	UStaticMesh* Mesh=Parts[0]->GetStaticMesh();
	const auto& LOD=Mesh->GetRenderData()->LODResources[0];
	const auto& Positions=LOD.VertexBuffers.PositionVertexBuffer;
	int32 PadTriangles=0, BaseTriangles=0;
	for (const auto& Section:LOD.Sections)
	{
		const auto* Material=Mesh->GetMaterial(Section.MaterialIndex);
		const bool bSolid=Material && Material->GetName().Contains(TEXT("CitySolidSurface"));
		for (uint32 T=0; T<Section.NumTriangles; ++T)
		{
			FVector P[3];
			for (int32 C=0; C<3; ++C) P[C]=FVector(Positions.VertexPosition(LOD.IndexBuffer.GetIndex(Section.FirstIndex+T*3+C)));
			// Large, nearly level triangles at the bottom are the authored pad or zone base;
			// the unchanged posts, walls and raised block tops cannot satisfy this predicate.
			const double MinZ=FMath::Min3(P[0].Z,P[1].Z,P[2].Z), MaxZ=FMath::Max3(P[0].Z,P[1].Z,P[2].Z);
			if (MaxZ-MinZ>0.5 || MaxZ>4 || FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).Size()<100) continue;
			if (bSolid)
			{
				++PadTriangles;
				TestTrue(TEXT("Every gray floor triangle stays clear of the textured base"),MinZ>=1.9);
			}
			else
			{
				++BaseTriangles;
				TestTrue(TEXT("The textured lot base retains its authored elevation"),FMath::Abs(MinZ)<0.01 && FMath::Abs(MaxZ)<0.01);
			}
		}
	}
	TestEqual(TEXT("The complete two-sided gray pad survives"),PadTriangles,4);
	TestEqual(TEXT("The complete two-sided textured base survives"),BaseTriangles,4);
	if (GUsingNullRHI)
	{
		Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true;
	}
	World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);
	World->UpdateParameterCollectionInstances(true,true);
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,Instance,World,City,Mesh]()
	{
		ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		City->SetActorHiddenInGame(true);
		auto* Rig=World->SpawnActor<AActor>();
		auto* Sun=NewObject<UDirectionalLightComponent>(Rig); Sun->SetIntensity(120000);
		Sun->SetWorldRotation(FRotator(-55,-35,0)); Sun->RegisterComponent();
		auto* Capture=NewObject<USceneCaptureComponent2D>(Rig);
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->bAlwaysPersistRenderingState=true;
		Capture->CaptureSource=SCS_FinalColorLDR; Capture->FOVAngle=50;
		Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
		Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=0;
		Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
		for (int32 I=0; I<2; ++I)
		{
			auto* Lot=World->SpawnActor<AActor>(); auto* Component=NewObject<UStaticMeshComponent>(Lot);
			Lot->SetRootComponent(Component); Component->SetStaticMesh(Mesh); Component->RegisterComponent();
			Lot->SetActorLocation(FVector(I*400,0,0)); Capture->ShowOnlyActorComponents(Lot);
		}
		auto* Target=NewObject<UTextureRenderTarget2D>(Rig); Target->RenderTargetFormat=RTF_RGBA8;
		Target->InitAutoFormat(1280,720); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target;
		Capture->RegisterComponent();
		auto Render=[&]()
		{
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			for (auto& Pixel:Pixels) Pixel.A=255;
			return Pixels;
		};
		for (int32 View=0; View<2; ++View)
		{
			const FVector Eye(950+View*20,950,780);
			Capture->SetWorldLocationAndRotation(Eye,(FVector(200,0,0)-Eye).Rotation());
			Render(); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
			FAssetCompilingManager::Get().FinishAllCompilation();
			const auto Pixels=Render();
			TestTrue(TEXT("Construction lot preview saved"),FImageUtils::SaveImageByExtension(
				*(FPaths::ProjectDir()/FString::Printf(TEXT("../Docs/scratchpad/taser-and-lot/construction-lot-%d.png"),View)),FImageView(Pixels.GetData(),1280,720)));
		}
	},1.0f));
	return true;
}
#endif

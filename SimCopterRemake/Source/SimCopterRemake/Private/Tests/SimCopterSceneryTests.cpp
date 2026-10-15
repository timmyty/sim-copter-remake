#if WITH_DEV_AUTOMATION_TESTS
#include "City/SimCopterScenery.h"
#include "City/SimCopterMoonDisc.h"
#include "City/SimCopterDayNight.h"
#include "ComponentRecreateRenderStateContext.h"
#include "City/SimCity2000CityActor.h"
#include "CelestialVaultDaySequenceActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSceneryVariantsTest,"SimCopter.Scenery.Variants",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterSceneryVariantsTest::RunTest(const FString& Parameters)
{
	FString Error;
	auto Library=FMaxisMeshLibrary::GetShared(SimCopterOriginalGame::ResolveRoot(),Error);
	if (!TestTrue(TEXT("Original library loads"),Library.IsValid())) { AddError(Error); return false; }
	const auto* Original=Library->FindObjectByObjectId(0x143);
	if (!TestNotNull(TEXT("Original small park loads"),Original)) return false;
	for (int32 Variant=0;Variant<SimCopterScenery::ParkVariantCount;++Variant)
	{
		const auto Mesh=SimCopterScenery::MakeParkVariant(*Original,Variant);
		int32 Pond=0, Trees=0, Paths=0;
		for (const auto& Face:Mesh.Faces)
		{
			Pond+=SimCopterScenery::IsParkPond(Face); Trees+=Face.FaceType==2; Paths+=Face.FaceType==15;
			for (const uint16 Index:Face.VertexIndices)
				TestTrue(TEXT("All variant face references stay valid"),Mesh.Vertices.IsValidIndex(Index));
		}
		TestEqual(TEXT("Only the pond layout contains water"),Pond>0,Variant==0);
		TestTrue(TEXT("Every layout retains trees"),Trees>=2);
		TestEqual(TEXT("Path geometry appears only on the three trail layouts"),Paths>0,Variant>=3);
	}
	TSet<int32> Variants;
	int32 Ponds=0;
	for(int32 Y=0;Y<128;++Y) for(int32 X=0;X<128;++X)
	{
		const int32 V=SimCopterScenery::ParkVariantForTile(X,Y); Variants.Add(V); Ponds+=V==0;
		TestEqual(TEXT("Regeneration uses the same layout"),SimCopterScenery::ParkVariantForTile(X,Y),V);
	}
	TestEqual(TEXT("All six layouts are distributed over city tiles"),Variants.Num(),6);
	TestTrue(TEXT("Ponds are occasional, not every tile"),Ponds>2000 && Ponds<3500);
	TestEqual(TEXT("Original pond mesh remains immutable"),Original->Faces.Num(),10);
	for (const int32 Id:{0x169,0x16a,0x16b}) TestTrue(TEXT("Commercial grid foundations are replaced"),SimCopterScenery::IsBuildingBase(Id));
	for (const int32 Id:{0x1d,0x3b,0x143,0x16,0x185}) TestFalse(TEXT("Roads, parks, hospitals and signals keep their own materials"),SimCopterScenery::IsBuildingBase(Id));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSkyDiscsTest,"SimCopter.Scenery.SkyDiscs",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterSkyDiscsTest::RunTest(const FString& Parameters)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Sky=World->SpawnActor<ACelestialVaultDaySequenceActor>();
	auto* Settings=NewObject<USimCopterMoonDiscComponent>(Sky); Settings->RegisterComponent();
	const float SunIntensity=Sky->SunLightComponent->Intensity;
	const float MoonIntensity=Sky->MoonLightComponent->Intensity;
	Settings->ApplyBrightness();
	const FVector Scale=Sky->MoonDiscComponent->GetRelativeScale3D();
	auto* Material=Cast<UMaterialInstanceDynamic>(Sky->MoonDiscComponent->GetMaterial(0));
	if (!TestNotNull(TEXT("Detailed moon material is assigned"),Material)) return false;
	TestNotNull(TEXT("Project-owned moon asset is cooked/loadable"),Settings->MoonMaterial.Get());
	Material->SetScalarParameterValue(TEXT("MoonAge"),0.27f);
	for(int32 I=0;I<60;++I) Settings->ApplyBrightness();
	TestEqual(TEXT("Size does not grow every tick"),Sky->MoonDiscComponent->GetRelativeScale3D(),Scale);
	TestTrue(TEXT("Sun is visibly larger"),Sky->SunLightComponent->LightSourceAngle>2.0f);
	TestTrue(TEXT("Moon is larger than the old map setting"),Sky->MoonScale>7.7f);
	TestEqual(TEXT("Sun illumination is unchanged"),Sky->SunLightComponent->Intensity,SunIntensity);
	TestEqual(TEXT("Moon illumination is unchanged"),Sky->MoonLightComponent->Intensity,MoonIntensity);
	TestTrue(TEXT("Shadow source size is preserved"),FMath::IsNearlyEqual(Sky->SunLightComponent->LightSourceAngle*Sky->SunLightComponent->ShadowSourceAngleFactor,0.5357f));
	float Age=0; Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("MoonAge")),Age);
	TestEqual(TEXT("Repeated appearance updates retain the live phase"),Age,0.27f);
	Sky->MoonDiscComponent->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/CelestialVault/Materials/MI_Moon.MI_Moon")));
	Settings->ApplyBrightness();
	Material=Cast<UMaterialInstanceDynamic>(Sky->MoonDiscComponent->GetMaterial(0));
	TestTrue(TEXT("Reconstructed material regains the detailed parent"),Material && Material->Parent==Settings->MoonMaterial);
	TestTrue(TEXT("Photo Mode can update the discs while paused"),Settings->PrimaryComponentTick.bTickEvenWhenPaused);
	if(!GUsingNullRHI && Material)
	{
		// Close-up of the actual disc mesh/material: fixed exposure is only a test-camera setting.
		// The shader samples atmospheric moonlight colour. Put the fixture moon overhead,
		// otherwise the default actor's below-horizon moon gives the preview an eclipse-red tint.
		Sky->MoonLightComponent->SetWorldRotation(FRotator(-90,0,0));
		Sky->SunLightComponent->SetIntensity(0);
		auto* Disc=NewObject<UStaticMeshComponent>(Sky);
		Disc->SetStaticMesh(Sky->MoonDiscComponent->GetStaticMesh()); Disc->SetMaterial(0,Material);
		Disc->SetWorldLocation(FVector(1000,0,0)); Disc->SetWorldRotation(FRotator(0,180,0));
		Disc->SetWorldScale3D(FVector(300.0 / Disc->GetStaticMesh()->GetBounds().BoxExtent.GetMax())); Disc->RegisterComponent();
		auto* Capture=NewObject<USceneCaptureComponent2D>(Sky);
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->bAlwaysPersistRenderingState=true;
		Capture->CaptureSource=SCS_FinalColorLDR; Capture->UnlitViewmode=ESceneCaptureUnlitViewmode::Disabled; Capture->FOVAngle=45;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
		Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=-1;
		Capture->ShowFlags.SetBloom(false); Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
		Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList; Capture->ShowOnlyComponent(Disc);
		auto* Target=NewObject<UTextureRenderTarget2D>(Sky); Target->RenderTargetFormat=RTF_RGBA8; Target->InitAutoFormat(1024,1024); Target->UpdateResourceImmediate(true);
		Capture->TextureTarget=Target; Capture->RegisterComponent();
		Material->SetScalarParameterValue(TEXT("MoonAge"),0.5f);
		World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
		if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); FAssetCompilingManager::Get().FinishAllCompilation();
		const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/celestial-presentation");
		IFileManager::Get().MakeDirectory(*Folder,true);
		for(int32 Phase=0;Phase<3;++Phase)
		{
			Material->SetScalarParameterValue(TEXT("MoonAge"),Phase==0 ? 0.5f : Phase==1 ? 0.25f : 0.1f);
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			int32 Visible=0,Neutral=0; TSet<uint8> Tones;
			for(auto& Pixel:Pixels) { Pixel.A=255; if(Pixel.R>30) { ++Visible; Tones.Add(Pixel.R); Neutral+=FMath::Abs(int32(Pixel.R)-Pixel.G)<30; } }
			TestTrue(TEXT("Lunar surface renders with visible tonal detail"),Visible>5000 && Tones.Num()>30);
			TestTrue(TEXT("Overhead lunar surface is neutral rather than a red horizon tint"),Neutral>Visible/2);
			const TCHAR* Names[]={TEXT("moon-full.png"),TEXT("moon-quarter.png"),TEXT("moon-crescent.png")};
			FImageUtils::SaveImageByExtension(*(Folder/Names[Phase]),FImageView(Pixels.GetData(),1024,1024));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSceneryCityTest,"SimCopter.Scenery.CityIntegration",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterSceneryCityTest::RunTest(const FString& Parameters)
{
	const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/celestial-presentation");
	IFileManager::Get().MakeDirectory(*Folder,true);
	TArray<uint8> Body; Body.Append(reinterpret_cast<const uint8*>("SCDH"),4);
	auto BE=[](TArray<uint8>& Bytes,uint32 Value) { for(int32 S=24;S>=0;S-=8) Bytes.Add(Value>>S); };
	auto Chunk=[&](const ANSICHAR* Name,const TArray<uint8>& Data,bool Raw=false)
	{
		TArray<uint8> Stored;
		if(Raw) Stored=Data;
		else for(int32 I=0;I<Data.Num();I+=127) { const int32 N=FMath::Min(127,Data.Num()-I); Stored.Add(N); Stored.Append(Data.GetData()+I,N); }
		Body.Append(reinterpret_cast<const uint8*>(Name),4); BE(Body,Stored.Num()); Body.Append(Stored);
	};
	TArray<uint8> Heights; Heights.Init(0,128*128*2); Chunk("ALTM",Heights,true);
	TArray<uint8> Grid; Grid.Init(0,128*128); Chunk("XTER",Grid);
	for(int32 Y=60;Y<=66;++Y) for(int32 X=60;X<=66;++X) Grid[Y*128+X]=0x0d;
	for(int32 Y=60;Y<62;++Y) for(int32 X=68;X<70;++X) Grid[Y*128+X]=0x8a;
	Chunk("XBLD",Grid); Grid.Init(0,128*128);
	for(int32 Y=60;Y<62;++Y) for(int32 X=68;X<70;++X) Grid[Y*128+X]=3;
	Chunk("XZON",Grid); Grid.Init(0,128*128);
	for(const ANSICHAR* Name:{"XUND","XTXT","XBIT"}) Chunk(Name,Grid);
	TArray<uint8> Bytes; Bytes.Append(reinterpret_cast<const uint8*>("FORM"),4); BE(Bytes,Body.Num()); Bytes.Append(Body);
	const FString File=Folder/TEXT("scenery-fixture.sc2"); FFileHelper::SaveArrayToFile(Bytes,*File);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->CityFile.FilePath=File;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	int32 Paths=0,Paving=0,ParkInstances=0;
	TInlineComponentArray<UInstancedStaticMeshComponent*> Meshes(City);
	for(const auto* Mesh:Meshes) for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
	{
		const auto* Material=Mesh->GetMaterial(Slot); if(!Material) continue;
		if(Material->GetName()==TEXT("M_ParkPath")) { ++Paths; ParkInstances+=Mesh->GetInstanceCount(); }
		if(Material->GetName()==TEXT("M_SceneryPaving")) ++Paving;
	}
	TestTrue(TEXT("Actual city instancing uses several path layouts"),Paths>=3 && ParkInstances>10);
	TestTrue(TEXT("Actual commercial buildings use the paving asset"),Paving>0);
	if(!GUsingNullRHI)
	{
		auto* Sun=NewObject<UDirectionalLightComponent>(City); Sun->SetIntensity(120000.0f); Sun->SetWorldRotation(FRotator(-65,-35,0)); Sun->RegisterComponent();
		auto* Fill=NewObject<UDirectionalLightComponent>(City); Fill->SetIntensity(30000.0f); Fill->SetWorldRotation(FRotator(-40,145,0)); Fill->SetCastShadows(false); Fill->RegisterComponent();
		auto* Capture=NewObject<USceneCaptureComponent2D>(City); Capture->RegisterComponent();
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->CaptureSource=SCS_FinalColorLDR;
		Capture->bAlwaysPersistRenderingState=true;
		Capture->UnlitViewmode=ESceneCaptureUnlitViewmode::Disabled;
		Capture->FOVAngle=70; Capture->SetWorldLocation(FVector(0,300,3600)); Capture->SetWorldRotation(FRotator(-75,0,0));
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
		Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=0;
		Capture->ShowFlags.SetBloom(false); Capture->ShowFlags.SetTemporalAA(false);
		auto* Target=NewObject<UTextureRenderTarget2D>(City); Target->RenderTargetFormat=RTF_RGBA8; Target->InitAutoFormat(1600,1200); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target;
		FAssetCompilingManager::Get().FinishAllCompilation(); if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);
		World->UpdateParameterCollectionInstances(true,true);
		{ FGlobalComponentRecreateRenderStateContext Recreate; }
		World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
		// The first draw queues on-demand material shaders; warm the persistent exposure state too.
		if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FAssetCompilingManager::Get().FinishAllCompilation();
		World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
		TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		for(auto& Pixel:Pixels) Pixel.A=255;
		int32 Visible=0,Clipped=0,Colored=0;
		for(const auto& Pixel:Pixels)
		{
			Visible+=FMath::Max3(Pixel.R,Pixel.G,Pixel.B)>40;
			Clipped+=FMath::Min3(Pixel.R,Pixel.G,Pixel.B)>250;
			Colored+=FMath::Max3(Pixel.R,Pixel.G,Pixel.B)-FMath::Min3(Pixel.R,Pixel.G,Pixel.B)>20;
		}
		TestTrue(TEXT("Capture has visible, illuminated scenery"),Visible>Pixels.Num()/3);
		TestTrue(TEXT("Capture is not overexposed or shader fallbacks"),Clipped<Pixels.Num()/10 && Colored>Pixels.Num()/10);
		TestTrue(TEXT("Scenery render exports"),FImageUtils::SaveImageByExtension(*(Folder/TEXT("parks-and-paving.png")),FImageView(Pixels.GetData(),1600,1200)));
	}
	return true;
}
#endif

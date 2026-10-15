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

#include "City/SimCopterWaterTowers.h"
#include "Game/SimCopterCareerProgression.h"
#include "StaticMeshResources.h"
#include "RHICommandList.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaterTowerContractTest,"SimCopter.WaterTowers.GeometryAndNames",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWaterTowerContractTest::RunTest(const FString& Parameters)
{
 FString Error;
 auto Library=FMaxisMeshLibrary::GetShared(SimCopterOriginalGame::ResolveRoot(),Error);
 if(!TestTrue(TEXT("Original library loads"),Library.IsValid())) return false;
 const auto* Original=Library->FindObjectByObjectId(SimCopterWaterTowers::ObjectId);
 if(!TestNotNull(TEXT("Water tower loads"),Original)) return false;
 TestEqual(TEXT("Only WT235 is the tower"),Original->Header.TableName,FString(TEXT("WT235")));
 const auto Prepared=SimCopterWaterTowers::PrepareModel(*Original);
 TestTrue(TEXT("Closed lid and frame detail are present"),Prepared.Vertices.Num()>Original->Vertices.Num());
 TestTrue(TEXT("Added geometry remains bounded"),Prepared.Vertices.Num()<1500 && Prepared.Faces.Num()<350);
 for(int32 I=0;I<Original->Vertices.Num();++I)
 {
  TestEqual(TEXT("Collision X unchanged"),Prepared.Vertices[I].X,Original->Vertices[I].X);
  TestEqual(TEXT("Collision Y unchanged"),Prepared.Vertices[I].Y,Original->Vertices[I].Y);
  TestEqual(TEXT("Collision Z unchanged"),Prepared.Vertices[I].Z,Original->Vertices[I].Z);
 }
 int32 Walls=0,Roof=0,Support=0;
 for(int32 I=0;I<Original->Faces.Num();++I)
 {
  const auto& Face=Prepared.Faces[I];
  TestTrue(TEXT("Triangulation unchanged"),Face.VertexIndices==Original->Faces[I].VertexIndices);
  TestEqual(TEXT("Every corner has a UV"),Face.RawUVs.Num(),Face.VertexIndices.Num());
  if(Face.MaterialIndex==0)
  {
   ++Walls; float MinU=10,MaxU=-10;
   for(const auto& Raw:Face.RawUVs) { auto UV=FMaxisMeshReader::ConvertMaxisUVToUnreal(Raw); MinU=FMath::Min(MinU,float(UV.X)); MaxU=FMath::Max(MaxU,float(UV.X)); TestTrue(TEXT("Tank V is in range"),UV.Y>=0 && UV.Y<=1); }
   TestTrue(TEXT("Cylinder seam never stretches the text across the whole tank"),MaxU-MinU<.101f);
  }
  else if(Face.MaterialIndex==1) ++Roof;
  else ++Support;
 }
 TestEqual(TEXT("All ten tank sides receive paint and lettering"),Walls,10);
 TestEqual(TEXT("Cap receives roof finish"),Roof,1);
 TestEqual(TEXT("Frame receives metal finish"),Support,44);
 TestEqual(TEXT("Shared original remains immutable"),Original->Faces[0].MaterialIndex,uint8(16));
 const FString Career=SimCopterOriginalGame::ResolveDirectory(TEXT("cities"))/TEXT("career");
 for(int32 I=0;I<SimCopterCareerProgression::CityCount;++I)
 {
  const FString File=Career/(SimCopterCareerProgression::GetMapBaseName(I)+TEXT(".sc2"));
  TestEqual(TEXT("Every original career city uses its displayed name"),SimCopterWaterTowers::ResolveCityName(File,TEXT("wrong CNAM")),FString(SimCopterCareerProgression::GetCityName(I)).ToUpper());
 }
 TestEqual(TEXT("Imported city0 is not renamed Sea Cliff"),SimCopterWaterTowers::ResolveCityName(FPaths::ProjectSavedDir()/TEXT("city0.sc2"),TEXT(" My Town ")),FString(TEXT("MY TOWN")));
 TestEqual(TEXT("Blank metadata has a useful fallback"),SimCopterWaterTowers::ResolveCityName(TEXT(""),TEXT(" ")),FString(TEXT("WATER WORKS")));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaterTowerCityTest,"SimCopter.WaterTowers.CityIntegration",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWaterTowerCityTest::RunTest(const FString& Parameters)
{
	const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/water-towers");
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
	TArray<uint8> NameBytes; NameBytes.Init(0,32); const ANSICHAR* CityName="Sea Cliff"; for(int32 I=0;CityName[I];++I) NameBytes[I+1]=CityName[I]; Chunk("CNAM",NameBytes,true);
	TArray<uint8> Heights; Heights.Init(0,128*128*2); Chunk("ALTM",Heights,true);
	TArray<uint8> Grid; Grid.Init(0,128*128); Chunk("XTER",Grid);
	// Sixteen separate 2x2 towers exercise batching, sharing, rebuild and removal.
	for(int32 TY=0;TY<4;++TY) for(int32 TX=0;TX<4;++TX)
		for(int32 Y=0;Y<2;++Y) for(int32 X=0;X<2;++X) Grid[(60+TY*3+Y)*128+60+TX*3+X]=235;
	Chunk("XBLD",Grid); Grid.Init(0,128*128);
	Chunk("XZON",Grid); Grid.Init(0,128*128);
	for(const ANSICHAR* Name:{"XUND","XTXT","XBIT"}) Chunk(Name,Grid);
	TArray<uint8> Bytes; Bytes.Append(reinterpret_cast<const uint8*>("FORM"),4); BE(Bytes,Body.Num()); Bytes.Append(Body);
	const FString File=Folder/TEXT("water-tower-fixture.sc2"); FFileHelper::SaveArrayToFile(Bytes,*File);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->CityFile.FilePath=File;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	City->FinishSpawning(FTransform::Identity);
	const double Begin=FPlatformTime::Seconds(); City->RebuildCity();
	AddInfo(FString::Printf(TEXT("16-tower city construction: %.3f ms"),(FPlatformTime::Seconds()-Begin)*1000));
	TArray<UInstancedStaticMeshComponent*> Towers; City->GetBuildingModelComponents(235,Towers);
	if(!TestEqual(TEXT("All towers share one instanced component"),Towers.Num(),1)) return false;
	TestEqual(TEXT("Sixteen towers are instances, not separate actors"),Towers[0]->GetInstanceCount(),16);
	TestEqual(TEXT("Tower retains one finish slot plus its existing foundation slot"),Towers[0]->GetNumMaterials(),2);
 TestTrue(TEXT("Detailed tower remains below 1500 triangles including its foundation"),Towers[0]->GetStaticMesh()->GetRenderData()->LODResources[0].GetNumTriangles()<1500);
	auto* Paint=Cast<UMaterialInstanceDynamic>(Towers[0]->GetMaterial(0));
	if(!TestNotNull(TEXT("City owns a shared paint instance"),Paint)) return false;
	TestEqual(TEXT("Dedicated material is actually bound"),Paint->Parent->GetName(),FString(TEXT("M_WaterTower")));
	TestFalse(TEXT("No new tower ticking"),Towers[0]->PrimaryComponentTick.bCanEverTick);
	if(!GUsingNullRHI)
	{
		auto* Sun=NewObject<UDirectionalLightComponent>(City); Sun->SetIntensity(120000.0f); Sun->SetWorldRotation(FRotator(-65,-35,0)); Sun->RegisterComponent();
		auto* Fill=NewObject<UDirectionalLightComponent>(City); Fill->SetIntensity(30000.0f); Fill->SetWorldRotation(FRotator(-40,145,0)); Fill->SetCastShadows(false); Fill->RegisterComponent();
		auto* Capture=NewObject<USceneCaptureComponent2D>(City); Capture->RegisterComponent();
		Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->CaptureSource=SCS_FinalColorLDR;
		Capture->bAlwaysPersistRenderingState=true;
		Capture->UnlitViewmode=ESceneCaptureUnlitViewmode::Disabled;
		Capture->FOVAngle=70; FBox Bounds; City->TryGetBuildingBoundsAtTile(60,60,Bounds);
		const FVector Aim=Bounds.GetCenter()+FVector(0,0,180);
		Capture->SetWorldLocation(Aim+FVector(1800,2400,900)); Capture->SetWorldRotation((Aim-Capture->GetComponentLocation()).Rotation());
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
		TestTrue(TEXT("Tower render exports"),FImageUtils::SaveImageByExtension(*(Folder/TEXT("water-towers.png")),FImageView(Pixels.GetData(),1600,1200)));
		auto* Label=Cast<UTextureRenderTarget2D>(Paint->K2_GetTextureParameterValue(TEXT("CityLettering")));
		if(TestNotNull(TEXT("One shared city label render target"),Label))
		{
			TestEqual(TEXT("Label allocation remains bounded"),Label->SizeX*Label->SizeY,1024*256);
			TestTrue(TEXT("Distant lettering has mipmaps"),Label->bAutoGenerateMips);
			TArray<FColor> Mask; Label->GameThread_GetRenderTargetResource()->ReadPixels(Mask);
			int32 Ink=0; for(auto& P:Mask) { Ink+=P.R>64; P.A=255; }
			TestTrue(TEXT("City lettering was rendered into the texture"),Ink>3000 && Ink<Mask.Num()/2);
			FImageUtils::SaveImageByExtension(*(Folder/TEXT("city-lettering.png")),FImageView(Mask.GetData(),1024,256));
		}
		// Compare the same mesh, instances, resolution, lights and camera. Synchronize the GPU
		// so this measures completed captures rather than merely queuing render commands.
		auto Draw = [&]()
		{
			World->SendAllEndOfFrameUpdates(); Capture->CaptureScene();
			ENQUEUE_RENDER_COMMAND(WaterTowerBenchmarkSync)([](FRHICommandListImmediate& Commands) { Commands.SubmitAndBlockUntilGPUIdle(); });
			FlushRenderingCommands();
		};
		TArray<double> OldTimes, NewTimes;
		for(int32 Pass=0;Pass<4;++Pass)
		{
			const bool bNew=Pass==1 || Pass==2; // ABBA order limits warm-up/thermal bias.
			Towers[0]->SetMaterial(0,bNew ? static_cast<UMaterialInterface*>(Paint) : City->VertexColorMaterial.Get());
			for(int32 Warm=0;Warm<4;++Warm) Draw();
			for(int32 Frame=0;Frame<12;++Frame)
			{
				const double Start=FPlatformTime::Seconds(); Draw();
				(bNew ? NewTimes : OldTimes).Add((FPlatformTime::Seconds()-Start)*1000);
			}
		}
		OldTimes.Sort(); NewTimes.Sort();
		const double OldMedian=OldTimes[OldTimes.Num()/2], NewMedian=NewTimes[NewTimes.Num()/2];
		AddInfo(FString::Printf(TEXT("Completed 1600x1200 captures, 16 towers, 24 samples each: old median %.3f ms / p95 %.3f ms; new median %.3f ms / p95 %.3f ms"),OldMedian,OldTimes[22],NewMedian,NewTimes[22]));
		TestTrue(TEXT("Tower material stays within the controlled render budget"),NewMedian<=OldMedian*1.25+0.5);
		Towers[0]->SetMaterial(0,Paint);
		Capture->SetWorldLocation(Aim+FVector(-1800,-2400,900)); Capture->SetWorldRotation((Aim-Capture->GetComponentLocation()).Rotation());
		Draw(); Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels); for(auto& P:Pixels) P.A=255;
		FImageUtils::SaveImageByExtension(*(Folder/TEXT("water-towers-reverse.png")),FImageView(Pixels.GetData(),1600,1200));

	}
	TArray<FIntPoint> Cleared;
	TestTrue(TEXT("Tower demolition still works"),City->DemolishBuildingAtTile(60,60,Cleared,false));
	TestEqual(TEXT("Demolition removes the painted tower and its lettering together"),Towers[0]->GetInstanceCount(),15);
	City->RebuildCity(); City->GetBuildingModelComponents(235,Towers);
	TestEqual(TEXT("Rebuild does not accumulate tower components"),Towers.Num(),1);
	TestEqual(TEXT("Rebuild restores all instances"),Towers[0]->GetInstanceCount(),16);
	return true;
}
#endif

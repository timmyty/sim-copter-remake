#if WITH_DEV_AUTOMATION_TESTS
#include "City/SimCopterMarina.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterDayNight.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ComponentRecreateRenderStateContext.h"
#include "ShaderCompiler.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMarinaShoreTest,"SimCopter.Marina.ShorelineAndGeometry",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterMarinaShoreTest::RunTest(const FString&)
{
	using namespace SimCopterMarina;
	FSimCity2000City City; City.Tiles.SetNum(FSimCity2000City::TileCount);
	constexpr int32 X=60,Y=60,N=128,Origin=Y*N+X;
	for (int32 V=0;V<3;++V) for (int32 U=0;U<3;++U) City.Tiles[(Y+V)*N+X+U].Building=BuildingId;
	TArray<uint8> Types; Types.Init(0, N*N);
	// Every combination of dry/wet cardinal edges, including all-land and all-water sites.
	for (int32 Mask=0;Mask<16;++Mask)
	{
		Types.Init(0,N*N);
		for (int32 Q=0;Q<3;++Q)
		{
			Types[(Y+Q)*N+X-1]=(Mask&1)?0x30:0;
			Types[(Y+Q)*N+X+3]=(Mask&2)?0x30:0;
			Types[(Y-1)*N+X+Q]=(Mask&4)?0x30:0;
			Types[(Y+3)*N+X+Q]=(Mask&8)?0x30:0;
		}
		const TArray<uint8> Before=Types;
		const auto Layouts=PrepareTerrain(City,Types);
		if (!TestEqual(TEXT("One complete marina is claimed"),Layouts.Num(),1)) return false;
		const int32 EdgeCells[]={3,5,1,7};
		for (int32 Side=0;Side<4;++Side)
			TestEqual(TEXT("Each side meets its actual neighbor"),bool(Layouts[Origin].LandMask&(1<<EdgeCells[Side])),bool(Mask&(1<<Side)));
		for (int32 I=0;I<N*N;++I)
			if (City.Tiles[I].Building!=BuildingId && Types[I]!=Before[I]) { AddError(TEXT("Changed terrain outside the marina")); return false; }
		if (Mask==0) TestEqual(TEXT("Open-water marina has no land slab"),Layouts[Origin].LandMask,uint16(0));
		if (Mask==15) TestEqual(TEXT("Inland marina is not flooded"),Layouts[Origin].LandMask,uint16(511));
	}
	Types.Init(0,N*N);
	City.Tiles[(Y+1)*N+X-1].Building=0x1d;
	const auto Road=PrepareTerrain(City,Types);
	TestEqual(TEXT("One road approach produces one paving cell"),Road[Origin].RoadMask,uint16(1<<3));
	TestTrue(TEXT("Road landing is dry even when saved as water"),Types[(Y+1)*N+X]>=10);
	TestEqual(TEXT("Opposite dock remains over water"),Types[(Y+1)*N+X+2],uint8(0));
	City.Tiles[(Y+1)*N+X-1].Building=0;
	for(int32 V=0;V<3;++V) for(int32 U=3;U<6;++U) City.Tiles[(Y+V)*N+X+U].Building=BuildingId;
	Types.Init(0,N*N);
	for(int32 I=0;I<N*N;++I) if(City.Tiles[I].Building==BuildingId) Types[I]=0x30;
	const auto Adjacent=PrepareTerrain(City,Types);
	TestEqual(TEXT("Two adjoining marinas remain distinct placements"),Adjacent.Num(),2);
	for(const auto& Entry:Adjacent) TestEqual(TEXT("Neighbor marina cannot create a false island"),Entry.Value.LandMask,uint16(0));
	City.Tiles[Origin].Building=0;
	for(int32 V=0;V<3;++V) for(int32 U=3;U<6;++U) City.Tiles[(Y+V)*N+X+U].Building=0;
	Types.Init(0x30,N*N);
	PrepareTerrain(City,Types);
	TestEqual(TEXT("Incomplete legacy footprint is left alone"),Types[Origin+1],uint8(0x30));

	FString Error; const auto Library=FMaxisMeshLibrary::GetShared(SimCopterOriginalGame::ResolveRoot(),Error);
	if (!TestTrue(TEXT("Original models load"),Library.IsValid())) return false;
	const auto* Original=Library->FindObjectByObjectId(ObjectId);
	if (!TestNotNull(TEXT("Actual screenshot model MA248 loads"),Original)) return false;
	const auto Model=MakeModel(*Original,Road[Origin]);
	int32 Textured=0,OriginalTextured=0,Red=0,Aprons=0;
	for(const auto& F:Original->Faces) OriginalTextured+=F.FaceType==18;
	FBox Bounds(ForceInit),ApronBounds(ForceInit);
	for(const auto& V:Model.Vertices) Bounds+=FVector(V.X,V.Y,V.Z)/65536.;
	for(const auto& F:Model.Faces)
	{
		Textured+=F.FaceType==18; Red+=F.FaceType==15 && F.MaterialIndex==16;
		TestEqual(TEXT("All faces retain UV coordinates"),F.RawUVs.Num(),F.VertexIndices.Num());
		if(F.FaceType==15 && F.MaterialIndex==RoadApronFinish)
		{
			++Aprons;
			for(auto I:F.VertexIndices) { const auto& V=Model.Vertices[I]; ApronBounds+=FVector(V.X,V.Y,V.Z)/65536.; }
		}
		for(auto I:F.VertexIndices) TestTrue(TEXT("Every mesh index is valid"),Model.Vertices.IsValidIndex(I));
	}
	TestEqual(TEXT("Original wall and porch textures preserved"),Textured,OriginalTextured);
	TestEqual(TEXT("No red walking surfaces remain"),Red,0);
	TestEqual(TEXT("Only the road-connected cell gets an apron"),Aprons,6);
	TestTrue(TEXT("Road apron maps to the correct world side"),ApronBounds.Min.Z>=32 && ApronBounds.Max.Z<=96 && ApronBounds.Min.X>=-32 && ApronBounds.Max.X<=32);
	TestTrue(TEXT("Model stays within original three-tile site"),Bounds.Min.X>=-96 && Bounds.Max.X<=96 && Bounds.Min.Z>=-96 && Bounds.Max.Z<=96);
	TestTrue(TEXT("Piers have pilings below the water line"),Bounds.Min.Y<0);
	TestTrue(TEXT("Dock geometry has a bounded cost"),Model.Vertices.Num()<16000);
	AddInfo(FString::Printf(TEXT("MA248: %d vertices / %d faces"),Model.Vertices.Num(),Model.Faces.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMarinaCityTest,"SimCopter.Marina.CityIntegrationAndPreview",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterMarinaCityTest::RunTest(const FString&)
{
	const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/marina-docks-2026-10-18");
	IFileManager::Get().MakeDirectory(*Folder,true);
	TArray<uint8> Body; Body.Append(reinterpret_cast<const uint8*>("SCDH"),4);
	auto BE=[](TArray<uint8>& Bytes,uint32 Value) { for(int32 S=24;S>=0;S-=8) Bytes.Add(Value>>S); };
	auto Chunk=[&](const ANSICHAR* Name,const TArray<uint8>& Data,bool Raw=false)
	{
		TArray<uint8> Stored;
		if(Raw) Stored=Data;
		else for(int32 I=0;I<Data.Num();I+=127) { int32 N=FMath::Min(127,Data.Num()-I); Stored.Add(N); Stored.Append(Data.GetData()+I,N); }
		Body.Append(reinterpret_cast<const uint8*>(Name),4); BE(Body,Stored.Num()); Body.Append(Stored);
	};
	TArray<uint8> Heights; Heights.Init(0,128*128*2); Chunk("ALTM",Heights,true);
	TArray<uint8> Grid; Grid.Init(0x30,128*128);
	for(int32 Y=0;Y<128;++Y) for(int32 X=0;X<60;++X) Grid[Y*128+X]=0;
	// Legacy marina footprints may have dry fill even where all neighbors are water.
	for(int32 Y=60;Y<63;++Y) for(int32 X=60;X<63;++X) Grid[Y*128+X]=0;
	for(int32 Y=60;Y<63;++Y) for(int32 X=68;X<71;++X) Grid[Y*128+X]=0;
	Chunk("XTER",Grid); Grid.Init(0,128*128);
	for(int32 Y=60;Y<63;++Y) for(int32 X=60;X<63;++X) Grid[Y*128+X]=248;
	for(int32 Y=60;Y<63;++Y) for(int32 X=68;X<71;++X) Grid[Y*128+X]=248;
	for(int32 X=58;X<64;++X) Grid[59*128+X]=0x1d;
	for(int32 X=61;X<67;++X) Grid[67*128+X]=223;
	Chunk("XBLD",Grid); Grid.Init(0,128*128);
	for(const ANSICHAR* Name:{"XZON","XUND","XTXT","XBIT"}) Chunk(Name,Grid);
	TArray<uint8> Bytes; Bytes.Append(reinterpret_cast<const uint8*>("FORM"),4); BE(Bytes,Body.Num()); Bytes.Append(Body);
	const FString File=Folder/TEXT("marina-fixture.sc2"); FFileHelper::SaveArrayToFile(Bytes,*File);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->CityFile.FilePath=File;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static); City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	TArray<UInstancedStaticMeshComponent*> Marinas,Piers;
	City->GetBuildingModelComponents(248,Marinas); City->GetBuildingModelComponents(223,Piers);
	if(!TestEqual(TEXT("Road and open-water marinas get separate cached models"),Marinas.Num(),2)) return false;
	for(auto* Component:Marinas)
	{
		bool Timber=false;
		for(int32 I=0;I<Component->GetNumMaterials();++I)
		{
			const FString Name=Component->GetMaterial(I)->GetName(); Timber |= Name==TEXT("M_DockTimber");
			TestFalse(TEXT("No cobblestone foundation under the marina"),Name==TEXT("M_SceneryPaving"));
		}
		TestTrue(TEXT("Timber is bound in the real city mesh"),Timber);
	}
	auto Sample=[&](int32 X,int32 Y,bool Water)
	{
		float Z=0; uint8 Type=255;
		const FVector P((X+.5)*400-25600,25600-(Y+.5)*400,500);
		TestTrue(TEXT("Terrain can be sampled"),City->TryGetWaterGameplaySurface(P,Z,Type));
		TestEqual(TEXT("Gameplay shore agrees with rendered terrain"),Type<10,Water);
	};
	Sample(60,61,false); Sample(61,60,false); Sample(62,61,true); Sample(61,62,true);
	Sample(69,60,true); Sample(70,61,true); Sample(68,61,true);
	// Trace through a boat slip: the old residential slab must no longer intercept it.
	const FVector Slip((62+.5)*400-25600,25600-(62+.5)*400,1000);
	FHitResult Hit;
	if(TestTrue(TEXT("Slip still has a terrain collision surface"),World->LineTraceSingleByChannel(Hit,Slip,Slip-FVector(0,0,2000),ECC_Visibility)))
		TestTrue(TEXT("Slip collision belongs to the terrain, not a full foundation"),!Marinas.Contains(Cast<UInstancedStaticMeshComponent>(Hit.GetComponent())));
	if(!GUsingNullRHI)
	{
		auto* Sun=NewObject<UDirectionalLightComponent>(City); Sun->SetIntensity(120000); Sun->SetWorldRotation(FRotator(-50,-35,0)); Sun->RegisterComponent();
		auto* Fill=NewObject<UDirectionalLightComponent>(City); Fill->SetIntensity(35000); Fill->SetWorldRotation(FRotator(-40,145,0)); Fill->SetCastShadows(false); Fill->RegisterComponent();
		auto* Capture=NewObject<USceneCaptureComponent2D>(City); Capture->RegisterComponent(); Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
		Capture->CaptureSource=SCS_FinalColorLDR; Capture->bAlwaysPersistRenderingState=true; Capture->FOVAngle=55;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
		Capture->ShowFlags.SetBloom(false); Capture->ShowFlags.SetTemporalAA(false);
		auto* Target=NewObject<UTextureRenderTarget2D>(City); Target->RenderTargetFormat=RTF_RGBA8; Target->InitAutoFormat(1600,1200); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target;
		FAssetCompilingManager::Get().FinishAllCompilation(); if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0); World->UpdateParameterCollectionInstances(true,true);
		{ FGlobalComponentRecreateRenderStateContext Recreate; }
		auto Shot=[&](int32 X,int32 Y,FVector Offset,const TCHAR* Name)
		{
			const FVector Aim((X+1.5)*400-25600,25600-(Y+1.5)*400,260);
			Capture->SetWorldLocation(Aim+Offset); Capture->SetWorldRotation((Aim-Capture->GetComponentLocation()).Rotation());
			for(int32 I=0;I<3;++I) { World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands(); if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); }
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels); for(auto& P:Pixels) P.A=255;
			TestTrue(TEXT("Review image exports"),FImageUtils::SaveImageByExtension(*(Folder/Name),FImageView(Pixels.GetData(),1600,1200)));
		};
		Shot(60,60,{1400,-1600,1600},TEXT("marina-shore.png"));
		Shot(68,60,{1400,-1600,1600},TEXT("marina-water.png"));
		Shot(62,66,{2300,-2400,1500},TEXT("pier.png"));
	}
	return true;
}
#endif

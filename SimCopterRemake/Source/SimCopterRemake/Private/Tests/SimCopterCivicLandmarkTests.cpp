#if WITH_DEV_AUTOMATION_TESTS
#include "City/SimCopterCivicLandmarks.h"
#include "City/SimCopterWaterTowers.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterDayNight.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
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
#include "StaticMeshResources.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCivicGeometryTest,"SimCopter.Civic.GeometryAndPierHeight",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterCivicGeometryTest::RunTest(const FString&)
{
	FString Error; auto Library=FMaxisMeshLibrary::GetShared(SimCopterOriginalGame::ResolveRoot(),Error);
	if (!TestTrue(TEXT("Original model library loads"),Library.IsValid())) return false;
	const auto* Source=Library->FindObjectByObjectId(SimCopterCivicLandmarks::CityHallObjectId);
	if (!TestNotNull(TEXT("CH208 loads"),Source)) return false;
	const auto Hall=SimCopterCivicLandmarks::MakeCityHall(*Source);
	TestEqual(TEXT("Original city hall remains immutable"),Source->Vertices.Num(),57);
	TestEqual(TEXT("Replacement keeps dispatch identity"),Hall.Header.Id,Source->Header.Id);
	FBox Bounds(ForceInit); int32 Triangles=0; TSet<uint8> Finishes;
	for (const auto& V:Hall.Vertices) Bounds+=FVector(V.X,V.Y,V.Z)/65536.;
	TestTrue(TEXT("City hall fits the original three-tile site"),Bounds.Min.X>=-96 && Bounds.Max.X<=96 && Bounds.Min.Z>=-96 && Bounds.Max.Z<=96);
	TestTrue(TEXT("Narrow lantern rises above the shallow crown"),Bounds.Max.Y>=108 && Bounds.Max.Y<=114);
	int32 EquipmentFaces=0,HedgeFaces=0; FBox LanternBounds(ForceInit);
	for (const auto& F:Hall.Faces)
	{
		Finishes.Add(F.MaterialIndex); Triangles+=F.VertexIndices.Num()-2;
		TestEqual(TEXT("All architectural surfaces have UVs"),F.RawUVs.Num(),F.VertexIndices.Num());
		for (const auto V:F.VertexIndices) TestTrue(TEXT("No invalid vertex index"),Hall.Vertices.IsValidIndex(V));
		if(F.MaterialIndex==7) ++EquipmentFaces;
		if(F.MaterialIndex==4 && F.RawUVs.Num() && F.RawUVs[0].X>64*65536) ++HedgeFaces;
		if(F.MaterialIndex==3) for(const auto I:F.VertexIndices)
		{
			const auto& V=Hall.Vertices[I];
			if(V.Y>92*65536) LanternBounds+=FVector(V.X,V.Y,V.Z)/65536.;
		}
	}
	TestEqual(TEXT("Eight finishes include textured facades and rooftop equipment"),Finishes.Num(),8);
	TestTrue(TEXT("Texture details reduce geometry by at least sixty percent"),Triangles<2200 && Hall.Vertices.Num()<5000);
	TestEqual(TEXT("Six simple rooftop AC housings are present"),EquipmentFaces,36);
	TestTrue(TEXT("Entryway has raised foliage rather than only flat grass"),HedgeFaces>=20);
	TestTrue(TEXT("Upper lantern is narrow and has textured openings"),LanternBounds.IsValid && LanternBounds.GetSize().X<=16 && LanternBounds.GetSize().Z<=16);
	AddInfo(FString::Printf(TEXT("City Hall: %d vertices, %d front triangles, %d finishes"),Hall.Vertices.Num(),Triangles,Finishes.Num()));
	for (int32 Id:{0x08c,0x10c}) TestTrue(TEXT("Both pier rotations are handled"),SimCopterCivicLandmarks::IsPier(Id));
	TestFalse(TEXT("Road bridges are not lowered"),SimCopterCivicLandmarks::IsPier(0x66));
	for (float Water:{-400.f,0.f,1200.f}) for (float Wave:{0.f,28.f,45.f})
	{
		const float Origin=SimCopterCivicLandmarks::PierOriginZ(Water,2621.44f,.25f,400,Wave);
		const float Deck=Origin+33*65536.f/2621.44f*.25f;
		TestTrue(TEXT("Deck is close to the local water and clears its crest"),Deck-Water>Wave && Deck-Water<=57.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCivicCityTest,"SimCopter.Civic.CityIntegration",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterCivicCityTest::RunTest(const FString&)
{
	const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/cityhall-reference-2026-10-15");
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
	TArray<uint8> Name; Name.Init(0,32); const ANSICHAR* Label="SEA CLIFF"; for(int32 I=0;Label[I];++I) Name[I+1]=Label[I]; Chunk("CNAM",Name,true);
	TArray<uint8> Heights; Heights.Init(0,128*128*2); Chunk("ALTM",Heights,true);
	TArray<uint8> Grid; Grid.Init(0,128*128);
	for(int32 Y=0;Y<128;++Y) for(int32 X=69;X<128;++X) Grid[Y*128+X]=0x30;
	Chunk("XTER",Grid); Grid.Init(0,128*128);
	for(int32 Y=60;Y<63;++Y) for(int32 X=60;X<63;++X) Grid[Y*128+X]=208;
	for(int32 Y=60;Y<62;++Y) for(int32 X=65;X<67;++X) Grid[Y*128+X]=235;
	for(int32 X=69;X<75;++X) Grid[61*128+X]=223;
	Grid[64*128+69]=223; // The opposite orientation below.
	Chunk("XBLD",Grid); Grid.Init(0,128*128); Chunk("XZON",Grid);
	for(const ANSICHAR* N:{"XUND","XTXT"}) Chunk(N,Grid);
	Grid[64*128+69]=2; Chunk("XBIT",Grid);
	TArray<uint8> Bytes; Bytes.Append(reinterpret_cast<const uint8*>("FORM"),4); BE(Bytes,Body.Num()); Bytes.Append(Body);
	const FString File=Folder/TEXT("civic-fixture.sc2"); FFileHelper::SaveArrayToFile(Bytes,*File);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->CityFile.FilePath=File;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static); City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	TArray<UInstancedStaticMeshComponent*> Halls,Towers,Piers;
	City->GetBuildingModelComponents(208,Halls); City->GetBuildingModelComponents(235,Towers); City->GetBuildingModelComponents(223,Piers);
	if (!TestEqual(TEXT("City hall is instanced"),Halls.Num(),1) || !TestEqual(TEXT("Water tower is instanced"),Towers.Num(),1)) return false;
	TestEqual(TEXT("City hall has one finish plus the existing foundation"),Halls[0]->GetNumMaterials(),2);
	TestEqual(TEXT("Civic finish bound in actual city builder"),Halls[0]->GetMaterial(0)->GetName(),FString(TEXT("M_CityHall")));
	const auto& LOD=Halls[0]->GetStaticMesh()->GetRenderData()->LODResources[0];
	const auto& Section=LOD.Sections[0]; TSet<int32> PackedFinishes;
	for (uint32 V=Section.MinVertexIndex; V<=Section.MaxVertexIndex; ++V)
	{
		const FColor C=LOD.VertexBuffers.ColorVertexBuffer.VertexColor(V);
		TestTrue(TEXT("Finish bits survive static-mesh sRGB packing"),(C.R==0 || C.R==255) && (C.G==0 || C.G==255) && (C.B==0 || C.B==255));
		PackedFinishes.Add((C.R?1:0)+(C.G?2:0)+(C.B?4:0));
	}
	TestEqual(TEXT("All eight finishes reach the GPU unchanged"),PackedFinishes.Num(),8);
	TestEqual(TEXT("Both pier orientations build"),Piers.Num(),2);
	for (auto* Pier:Piers)
	{
		FTransform T; Pier->GetInstanceTransform(0,T);
		// ALTM 0 maps to sample (0+1)*32, i.e. 200 cm, in the rendered height map.
		TestTrue(TEXT("Actual pier deck sits 40 cm above rendered water"),FMath::IsNearlyEqual(float(T.GetLocation().Z)+206.25f,240.f,.1f));
	}
	if (!GUsingNullRHI)
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
		auto Shot=[&](FIntPoint Tile,FVector Offset,const TCHAR* Name)
		{
			FBox Bounds; if (!TestTrue(TEXT("Building bounds exist"),City->TryGetBuildingBoundsAtTile(Tile.X,Tile.Y,Bounds))) return 0;
			const FVector Aim=Bounds.GetCenter(); Capture->SetWorldLocation(Aim+Offset); Capture->SetWorldRotation((Aim-Capture->GetComponentLocation()).Rotation());
			for(int32 I=0;I<3;++I) { World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands(); if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); }
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels); for(auto& P:Pixels) P.A=255;
			TestTrue(TEXT("Offscreen review image exports"),FImageUtils::SaveImageByExtension(*(Folder/Name),FImageView(Pixels.GetData(),1600,1200)));
			int32 Bright=0; for(const auto& Pixel:Pixels) Bright+=Pixel.R>80;
			return Bright;
		};
		Shot({60,60},{1900,1700,1300},TEXT("city-hall.png"));
		Shot({60,60},{1250,1500,1100},TEXT("city-hall-detail.png"));
		Shot({65,60},{1700,2100,1300},TEXT("closed-water-tower.png"));
		Shot({71,61},{1500,2100,1000},TEXT("low-piers.png"));
		// Facade windows are now texture masks. Verify their real emission still obeys
		// both shared lighting controls, and capture the mask for visual inspection.
		Sun->SetIntensity(0); Fill->SetIntensity(0);
		Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyComponent(Halls[0]);
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
		Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=-2;
		auto* Collection=LoadObject<UMaterialParameterCollection>(nullptr,TEXT("/Game/Materials/MPC_SimCopterDayNight.MPC_SimCopterDayNight"));
		auto* Values=World->GetParameterCollectionInstance(Collection);
		Values->SetScalarParameterValue(TEXT("NightBlend"),0); Values->SetScalarParameterValue(TEXT("WindowGlowNits"),25);
		const int32 Off=Shot({60,60},{1250,1500,1100},TEXT("city-hall-windows-off.png"));
		Values->SetScalarParameterValue(TEXT("NightBlend"),1);
		const int32 On=Shot({60,60},{1250,1500,1100},TEXT("city-hall-windows-night.png"));
		Values->SetScalarParameterValue(TEXT("WindowGlowNits"),0);
		const int32 Disabled=Shot({60,60},{1250,1500,1100},TEXT("city-hall-windows-disabled.png"));
		AddInfo(FString::Printf(TEXT("City Hall window emission pixels: off=%d night=%d disabled=%d"),Off,On,Disabled));
		TestTrue(TEXT("Textured windows emit at night, including the lantern"),On>200 && Off<On/20);
		TestTrue(TEXT("Shared brightness control disables facade emission"),Disabled<On/20);
	}
	return true;
}
#endif

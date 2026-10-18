#if WITH_DEV_AUTOMATION_TESTS
#include "City/SimCopterEmergencyBuildings.h"
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
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterEmergencyGeometryTest,"SimCopter.EmergencyBuildings.RoofsHeightAndGeometry",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterEmergencyGeometryTest::RunTest(const FString&)
{
	using namespace SimCopterEmergencyBuildings;
	FString Error; auto Library=FMaxisMeshLibrary::GetShared(SimCopterOriginalGame::ResolveRoot(),Error);
	if(!TestTrue(TEXT("Original library loads"),Library.IsValid())) return false;
	for(int32 Id:{HospitalId,PoliceId,FireId})
	{
		const auto* Source=Library->FindObjectByObjectId(Id); if(!TestNotNull(TEXT("Emergency model exists"),Source)) return false;
		const auto Model=MakeModel(*Source); int32 Details=0,Roofs=0; FBox Bounds(ForceInit);
		const auto Legacy=MakeModel(*Source,false);
		TestEqual(TEXT("Previous mode retains the original face count"),Legacy.Faces.Num(),Source->Faces.Num());
		TestEqual(TEXT("Previous mode retains the original vertex count"),Legacy.Vertices.Num(),Source->Vertices.Num());
		int32 LegacyTop=0;
		for(const auto& V:Legacy.Vertices) LegacyTop=FMath::Max(LegacyTop,V.Y);
		if(Id==FireId) TestEqual(TEXT("Previous fire station has its pre-update height"),LegacyTop,13950477);
		for(int32 I=0;I<Legacy.Faces.Num();++I)
		{
			TestTrue(TEXT("Previous face winding unchanged"),Legacy.Faces[I].VertexIndices==Source->Faces[I].VertexIndices);
			TestTrue(TEXT("Previous roof and wall UVs unchanged"),Legacy.Faces[I].RawUVs==Source->Faces[I].RawUVs);
			TestFalse(TEXT("Previous mode has no new detail faces"),IsDetail(Legacy,Legacy.Faces[I]));
		}
		for(const auto& V:Model.Vertices) Bounds+=FVector(V.X,V.Y,V.Z)/65536.;
		TestTrue(TEXT("Details stay within the existing three-tile lot"),Bounds.Min.X>=-96 && Bounds.Max.X<=96 && Bounds.Min.Z>=-96 && Bounds.Max.Z<=96);
		TestTrue(TEXT("One reusable model has bounded geometry"),Model.Vertices.Num()<12000);
		for(const auto& F:Model.Faces)
		{
			Details+=IsDetail(Model,F);
			for(auto I:F.VertexIndices) if(!TestTrue(TEXT("Valid vertex reference"),Model.Vertices.IsValidIndex(I))) return false;
			TestEqual(TEXT("UVs match the face"),F.RawUVs.Num(),F.VertexIndices.Num());
		}
		TestTrue(TEXT("Reference details exist"),Details>100);
		// Every original horizontal roof polygon and helipad retains exact vertices,
		// atlas/material and vertex-to-UV mapping, including after winding normalization.
		for(const auto& F:Source->Faces)
		{
			if(F.VertexIndices.Num()<3) continue;
			int32 Lo=MAX_int32,Hi=MIN_int32;
			for(auto I:F.VertexIndices){Lo=FMath::Min(Lo,Source->Vertices[I].Y);Hi=FMath::Max(Hi,Source->Vertices[I].Y);}
			if(Hi-Lo>8 || Lo<40*65536) continue;
			const auto* Kept=Model.Faces.FindByPredicate([&](const FMaxisMeshFace& N){return N.Offset==F.Offset;});
			if(!TestNotNull(TEXT("Original roof face survives"),Kept)) continue;
			++Roofs; TestEqual(TEXT("Roof atlas unchanged"),Kept->TextureAtlasIndex,F.TextureAtlasIndex);
			TestEqual(TEXT("Roof material unchanged"),Kept->MaterialIndex,F.MaterialIndex);
			for(int32 C=0;C<F.VertexIndices.Num();++C)
			{
				const uint16 I=F.VertexIndices[C];const int32 At=Kept->VertexIndices.Find(I);
				if(!TestTrue(TEXT("Roof corner survives"),At!=INDEX_NONE)) continue;
				const auto& A=Source->Vertices[I];const auto& D=Model.Vertices[I];
				TestTrue(TEXT("Roof position unchanged"),A.X==D.X && A.Y==D.Y && A.Z==D.Z);
				TestEqual(TEXT("Roof texture coordinates unchanged"),Kept->RawUVs[At],F.RawUVs[C]);
			}
		}
		TestTrue(TEXT("Roof preservation was exercised"),Roofs>=5);
		if(Id==FireId) TestTrue(TEXT("Fire station is back to the shipped height, including roof equipment"),Bounds.Max.Y<140 && Bounds.Max.Y>136);
		AddInfo(FString::Printf(TEXT("Model %x: %d vertices, %d detail faces, %d preserved roofs"),Id,Model.Vertices.Num(),Details,Roofs));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterEmergencyCityTest,"SimCopter.EmergencyBuildings.CityCollisionAndPreview",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterEmergencyCityTest::RunTest(const FString&)
{
	const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/release-v1.0.20/previews");
	IFileManager::Get().MakeDirectory(*Folder,true);
	TArray<uint8> Body; Body.Append(reinterpret_cast<const uint8*>("SCDH"),4);
	auto BE=[](TArray<uint8>& Bytes,uint32 Value){for(int32 S=24;S>=0;S-=8) Bytes.Add(Value>>S);};
	auto Chunk=[&](const ANSICHAR* Name,const TArray<uint8>& Data,bool Raw=false)
	{
		TArray<uint8> Stored;
		if(Raw) Stored=Data;
		else for(int32 I=0;I<Data.Num();I+=127){int32 N=FMath::Min(127,Data.Num()-I);Stored.Add(N);Stored.Append(Data.GetData()+I,N);}
		Body.Append(reinterpret_cast<const uint8*>(Name),4);BE(Body,Stored.Num());Body.Append(Stored);
	};
	TArray<uint8> Heights;Heights.Init(0,128*128*2);Chunk("ALTM",Heights,true);
	TArray<uint8> Grid;Grid.Init(0,128*128);Chunk("XTER",Grid);
	for(int32 Id=209;Id<=211;++Id) for(int32 Y=60;Y<63;++Y) for(int32 X=60+(Id-209)*7;X<63+(Id-209)*7;++X) Grid[Y*128+X]=Id;
	Chunk("XBLD",Grid);Grid.Init(0,128*128);
	for(const ANSICHAR* N:{"XZON","XUND","XTXT","XBIT"}) Chunk(N,Grid);
	TArray<uint8> Bytes;Bytes.Append(reinterpret_cast<const uint8*>("FORM"),4);BE(Bytes,Body.Num());Bytes.Append(Body);
	const FString File=Folder/TEXT("emergency-fixture.sc2");FFileHelper::SaveArrayToFile(Bytes,*File);
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT {GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false;City->bLoadOnBeginPlay=false;City->CityFile.FilePath=File;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);City->FinishSpawning(FTransform::Identity);City->RebuildCity();
	TArray<UInstancedStaticMeshComponent*> Models;
	for(int32 Id=209;Id<=211;++Id)
	{
		TArray<UInstancedStaticMeshComponent*> Parts;City->GetBuildingModelComponents(Id,Parts);
		if(!TestEqual(TEXT("Emergency building is instanced"),Parts.Num(),1)) return false;
		Models.Add(Parts[0]);bool Bound=false;
		for(int32 I=0;I<Parts[0]->GetNumMaterials();++I) Bound|=Parts[0]->GetMaterial(I)->GetName()==TEXT("M_EmergencyDetails");
		TestTrue(TEXT("Detail material bound in the real city"),Bound);
	}
	auto LocalPoint=[&](int32 Building,FVector GEO)
	{
		FTransform T;Models[Building]->GetInstanceTransform(0,T,true);
		return T.TransformPosition(FVector(-GEO.Z,-GEO.X,GEO.Y)*6.25);
	};
	FHitResult Hit;
	for(double Side:{-1.,1.})
		TestTrue(TEXT("Each hospital canopy has a solid top"),World->LineTraceSingleByChannel(Hit,LocalPoint(0,{92,50,Side*30}),LocalPoint(0,{92,20,Side*30}),ECC_Visibility));
	TestFalse(TEXT("Two overhead sections have an open gap"),World->LineTraceSingleByChannel(Hit,LocalPoint(0,{92,50,0}),LocalPoint(0,{92,20,0}),ECC_Visibility));
	TestTrue(TEXT("Lower roof has an orange top rail"),World->LineTraceSingleByChannel(Hit,LocalPoint(0,{95,117.35,0}),LocalPoint(0,{80,117.35,0}),ECC_Visibility));
	TestFalse(TEXT("Rear corner no longer has a balcony"),World->LineTraceSingleByChannel(Hit,LocalPoint(0,{-77,75,-57.8}),LocalPoint(0,{-77,55,-57.8}),ECC_Visibility));
	TestFalse(TEXT("Covered hospital entrance remains open below the canopy"),World->LineTraceSingleByChannel(Hit,LocalPoint(0,{95,18,0}),LocalPoint(0,{67,18,0}),ECC_Visibility));
	TestFalse(TEXT("Centre fire engine bay is recessed"),World->LineTraceSingleByChannel(Hit,LocalPoint(2,{95,20,0}),LocalPoint(2,{70,20,0}),ECC_Visibility));
	if(!GUsingNullRHI)
	{
		auto* Sun=NewObject<UDirectionalLightComponent>(City);Sun->SetIntensity(120000);Sun->SetWorldRotation(FRotator(-50,-35,0));Sun->RegisterComponent();
		auto* Fill=NewObject<UDirectionalLightComponent>(City);Fill->SetIntensity(35000);Fill->SetWorldRotation(FRotator(-40,145,0));Fill->SetCastShadows(false);Fill->RegisterComponent();
		auto* Capture=NewObject<USceneCaptureComponent2D>(City);Capture->RegisterComponent();Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
		Capture->CaptureSource=SCS_FinalColorLDR;Capture->bAlwaysPersistRenderingState=true;Capture->FOVAngle=55;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
		Capture->PostProcessSettings.bOverride_CameraISO=true;Capture->PostProcessSettings.CameraISO=100;
		Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true;Capture->PostProcessSettings.CameraShutterSpeed=125;
		Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true;Capture->PostProcessSettings.DepthOfFieldFstop=8;
		Capture->ShowFlags.SetBloom(false);Capture->ShowFlags.SetTemporalAA(false);
		auto* Target=NewObject<UTextureRenderTarget2D>(City);Target->RenderTargetFormat=RTF_RGBA8;Target->InitAutoFormat(1600,1200);Target->UpdateResourceImmediate(true);Capture->TextureTarget=Target;
		FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
		World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);World->UpdateParameterCollectionInstances(true,true);
		{FGlobalComponentRecreateRenderStateContext Recreate;}
		auto Shot=[&](int32 I,FVector Offset,const TCHAR* Name)
		{
			FBox Bounds;City->TryGetBuildingBoundsAtTile(60+I*7,60,Bounds);
			const FVector Aim=Bounds.GetCenter();Capture->SetWorldLocation(Aim+Offset);Capture->SetWorldRotation((Aim-Capture->GetComponentLocation()).Rotation());
			for(int32 Frame=0;Frame<3;++Frame){World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();}
			TArray<FColor> Pixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);for(auto& P:Pixels)P.A=255;
			TestTrue(TEXT("Review image exports"),FImageUtils::SaveImageByExtension(*(Folder/Name),FImageView(Pixels.GetData(),1600,1200)));
			int32 Bright=0;for(const auto& P:Pixels)Bright+=P.R>80;return Bright;
		};
		Shot(0,{1900,-2300,1500},TEXT("hospital.png"));Shot(1,{2200,-1900,1400},TEXT("police.png"));Shot(2,{1700,-2100,1350},TEXT("fire-station.png"));
		Sun->SetIntensity(0);Fill->SetIntensity(0);Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=-2;
		auto* Collection=LoadObject<UMaterialParameterCollection>(nullptr,TEXT("/Game/Materials/MPC_SimCopterDayNight.MPC_SimCopterDayNight"));
		auto* Values=World->GetParameterCollectionInstance(Collection);
		Values->SetScalarParameterValue(TEXT("WindowLitFraction"),1);Values->SetScalarParameterValue(TEXT("WindowRowLitFraction"),1);Values->SetScalarParameterValue(TEXT("WindowGlowNits"),25);
		Capture->ShowOnlyComponent(Models[1]);Values->SetScalarParameterValue(TEXT("NightBlend"),0);
		const int32 Off=Shot(1,{2200,-1900,1400},TEXT("police-windows-off.png"));Values->SetScalarParameterValue(TEXT("NightBlend"),1);
		const int32 On=Shot(1,{2200,-1900,1400},TEXT("police-windows-night.png"));Values->SetScalarParameterValue(TEXT("WindowGlowNits"),0);
		const int32 Disabled=Shot(1,{2200,-1900,1400},TEXT("police-windows-disabled.png"));
		TestTrue(TEXT("New windows respond to night fade"),On>200 && Off<On/20);TestTrue(TEXT("Brightness control disables new windows"),Disabled<On/20);
	}
	for(bool Updated:{false,true})
	{
		City->bUpdatedEmergencyArtwork=Updated;City->RebuildCity();
		for(int32 Id=209;Id<=211;++Id)
		{
			TArray<UInstancedStaticMeshComponent*> Parts;City->GetBuildingModelComponents(Id,Parts);
			if(!TestEqual(TEXT("Switch keeps each building instanced"),Parts.Num(),1)) return false;
			bool HasDetails=false,HasLegacyFacade=false;
			for(int32 I=0;I<Parts[0]->GetNumMaterials();++I)
			{
				HasDetails|=Parts[0]->GetMaterial(I)->GetName()==TEXT("M_EmergencyDetails");
				HasLegacyFacade|=Parts[0]->GetMaterial(I)->GetName()==TEXT("M_HospitalFacadeLegacy");
			}
			TestEqual(TEXT("Selected artwork controls actual city materials"),HasDetails,Updated);
			if(Id==209) TestEqual(TEXT("Previous hospital facade restored"),HasLegacyFacade,!Updated);
			if(Id==211)
			{
				FBox BuildingBounds;City->TryGetBuildingBoundsAtTile(74,60,BuildingBounds);
				TestTrue(TEXT("Fire station collision bounds follow artwork choice"),Updated ? BuildingBounds.GetSize().Z<900 : BuildingBounds.GetSize().Z>1300);
			}
		}
	}
	return true;
}
#endif

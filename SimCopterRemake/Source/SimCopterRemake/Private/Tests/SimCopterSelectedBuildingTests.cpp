#if WITH_DEV_AUTOMATION_TESTS
#include "City/SimCopterSelectedBuildings.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Formats/SimCopterPeopleCityRules.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSelectedGeometryTest,"SimCopter.SelectedBuildings.GeometryAndScope",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterSelectedGeometryTest::RunTest(const FString&)
{
 using namespace SimCopterSelectedBuildings;
 FString Error;auto Library=FMaxisMeshLibrary::GetShared(SimCopterOriginalGame::ResolveRoot(),Error);
 if(!TestTrue(TEXT("Original GEO library loads"),Library.IsValid()))return false;
 for(int32 Id:{10,20,21,23,24,27,141,143,151,152,153,213,214,217,218,220,221,222,223,224,225,226,228,229,230,231,241,242,244,245,246,324,326,367})
 {
  const auto* Source=Library->FindObjectByObjectId(Id);if(!TestNotNull(TEXT("Selected source exists"),Source))continue;
  auto Model=MakeModel(*Source);FBox Old(ForceInit),Bounds(ForceInit);
  int32 OldBeacons=0,NewBeacons=0;for(const auto& F:Source->Faces)OldBeacons+=F.FaceType==25;
  for(const auto& F:Model.Faces){NewBeacons+=F.FaceType==25;if(F.FaceType==26)TestEqual(TEXT("Authored chimney retains the smoke effect class"),int32(F.MaterialIndex),1);}
  TestEqual(TEXT("Original warning beacons are retained"),NewBeacons,OldBeacons);
  for(const auto& V:Source->Vertices)Old+=FVector(V.X,V.Y,V.Z)/65536.;
  for(const auto& V:Model.Vertices)Bounds+=FVector(V.X,V.Y,V.Z)/65536.;
  const FVector Ratio=Bounds.GetSize()/Old.GetSize();
  TestTrue(*FString::Printf(TEXT("%s footprint stays near existing size: %s"),*Source->Header.TableName,*Ratio.ToString()),Ratio.X>.7 && Ratio.X<1.2 && Ratio.Z>.7 && Ratio.Z<1.2);
  TestTrue(*FString::Printf(TEXT("%s height stays near existing height: %.3f"),*Source->Header.TableName,Ratio.Y),Ratio.Y>.8 && Ratio.Y<1.16);
  TestTrue(TEXT("Authored geometry uses consistent winding"),Model.Header.bConsistentWinding);
  TestTrue(TEXT("Geometry is reusable and within 16-bit index budget"),Model.Vertices.Num()<60000 && Model.Faces.Num()>20);
  int32 Glass=0;
  for(const auto& F:Model.Faces)
  {
   TestEqual(TEXT("UV count matches corners"),F.RawUVs.Num(),F.VertexIndices.Num());
   for(auto I:F.VertexIndices)if(!TestTrue(TEXT("Every corner references a vertex"),Model.Vertices.IsValidIndex(I)))return false;
   if(F.FaceType!=15)continue;
   TestTrue(TEXT("Every authored surface is material tagged"),IsDetail(F));Glass+=F.MaterialIndex==16;
   if(F.VertexIndices.Num()>=3)
   {
    auto V=[&](int32 I){const auto& P=Model.Vertices[F.VertexIndices[I]];return FVector(P.X,P.Y,P.Z)/65536.;};
    TestTrue(*FString::Printf(TEXT("%s has no degenerate first triangle"),*Source->Header.TableName),FVector::CrossProduct(V(1)-V(0),V(2)-V(0)).SizeSquared()>1e-12);
   }
  }
  if(Id==152)TestTrue(TEXT("Launch has actual glazing"),Glass>500);
  AddInfo(FString::Printf(TEXT("%s: %d vertices, %d faces, scale %s"),*Source->Header.TableName,Model.Vertices.Num(),Model.Faces.Num(),*Ratio.ToString()));
 }
 for(int32 Id:{13,22,28,130,219,243,323,325,142,154,155})
 {
  TestFalse(TEXT("Excluded building is outside the allowlist"),IsSelected(Id));
  const auto* Source=Library->FindObjectByObjectId(Id);if(!Source)continue;const auto Model=MakeModel(*Source);
  TestEqual(TEXT("Excluded mesh retains vertices"),Model.Vertices.Num(),Source->Vertices.Num());
  TestEqual(TEXT("Excluded mesh retains faces"),Model.Faces.Num(),Source->Faces.Num());
 }
 for(int32 Id:{247,248,249,250})
 {
  const auto* Source=Library->FindObjectByObjectId(Id);if(!Source)return false;
  const auto Model=MakeCauseway(*Source,true);
  for(int32 I=0;I<Source->Faces.Num();++I)
  {
   TestTrue(TEXT("Causeway keeps every road polygon and texture coordinate"),Source->Faces[I].VertexIndices==Model.Faces[I].VertexIndices && Source->Faces[I].RawUVs==Model.Faces[I].RawUVs);
  }
  for(int32 I=0;I<Source->Vertices.Num();++I)TestTrue(TEXT("Road/slope profile positions are unchanged"),Source->Vertices[I].X==Model.Vertices[I].X && Source->Vertices[I].Y==Model.Vertices[I].Y && Source->Vertices[I].Z==Model.Vertices[I].Z);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSelectedBoatTest,"SimCopter.SelectedBuildings.CausewayBoatClearance",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterSelectedBoatTest::RunTest(const FString&)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 ON_SCOPE_EXIT {GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
 auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();Traffic->ActiveTileSize=400;
 Traffic->WaterTileFlags.Init(1,128*128);Traffic->XbldTileIds.Init(0,128*128);Traffic->TileCenterWorldZ.Init(0,128*128);
 Traffic->PeopleTileClasses.Init(4,128*128);
 auto* Ambient=World->SpawnActor<ASimCopterAmbientVehiclesActor>();
 for(int32 Y=20;Y<100;++Y)Traffic->XbldTileIds[Y*128+64]=0x49;
 auto P=[&](int32 X,int32 Y){FVector V;Traffic->TryGetTileCenterWorldLocation(X,Y,V);return V;};
 TestFalse(TEXT("Boat cannot spawn beneath causeway"),Ambient->IsBoatWaterTile({64,64}));
 TestTrue(TEXT("Open water beside bridge remains navigable"),Ambient->IsBoatWaterTile({63,64}));
 TestFalse(TEXT("Large displacement cannot jump over excluded strip"),Ambient->CanMoveBoat(P(62,64),P(66,64),false));
 TestTrue(TEXT("Parallel route stays open"),Ambient->CanMoveBoat(P(62,62),P(62,68),false));
 TestFalse(TEXT("Capsized rescue requires clearance from the bridge"),Ambient->CanMoveBoat(P(63,64),P(63,64),true));
 const FVector Toward=(P(64,64)-P(63,64)).GetSafeNormal();
 TestFalse(TEXT("Hull edge is excluded before its centre enters bridge tile"),Ambient->CanMoveBoat(P(63,64),P(63,64)+Toward*100,false));
 FSimCopterAmbientBoat Boat;Boat.ObjectId=SimCopterAmbientVehicles::CapsizedBoatObjectId;
 TestTrue(TEXT("Placement near causeway finds open water"),Ambient->PlaceBoatNearTile(Boat,{64,64},10));
 TestTrue(TEXT("Relocated hull and rescue area clear causeway"),Ambient->CanMoveBoat(Boat.World,Boat.World,true));
 Boat.World=P(64,64);Boat.Tile={64,64};Boat.bVisible=true;Boat.EventId=123;Boat.TowEventId=456;Boat.MissionTimerSeconds=47;
 Ambient->UpdateBoat(Boat,0);
 TestTrue(TEXT("Saved hull trapped under bridge is relocated"),Boat.Tile!=FIntPoint(64,64) && Ambient->CanMoveBoat(Boat.World,Boat.World,true));
 TestEqual(TEXT("Relocation preserves rescue identity"),Boat.EventId,123);
 TestEqual(TEXT("Relocation preserves tow identity"),Boat.TowEventId,456);
 TestEqual(TEXT("Relocation preserves rescue timer"),Boat.MissionTimerSeconds,47.f);
 const FVector Safe=Boat.World;Traffic->WaterTileFlags.Init(0,128*128);Ambient->UpdateBoat(Boat,0);
 TestTrue(TEXT("Failed relocation retains the recoverable hull"),Boat.bVisible && Boat.World.Equals(Safe) && Boat.TowEventId==456);
 Traffic->WaterTileFlags.Init(1,128*128);
 for(int32 Kind=0x49;Kind<=0x50;++Kind){Traffic->XbldTileIds[64*128+64]=Kind;TestFalse(TEXT("All causeway orientations exclude boats"),Ambient->IsBoatWaterTile({64,64}));}
 Traffic->XbldTileIds[64*128+64]=0;TestTrue(TEXT("Removing the causeway restores navigation"),Ambient->IsBoatWaterTile({64,64}));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSelectedCityTest,"SimCopter.SelectedBuildings.CityGalleryAndCollision",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterSelectedCityTest::RunTest(const FString&)
{
 const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/selected-buildings-2026-10-18/previews");IFileManager::Get().MakeDirectory(*Folder,true);
 const TArray<int32> Ids={187,247,250,245,212,214,224,226,251,252,253,254,255,194,195,198,199,201,202,203,204,205,206,207,216,217,219,220,243,244,249,213,218,210,211};
 TArray<uint8> Body;Body.Append(reinterpret_cast<const uint8*>("SCDH"),4);
 auto BE=[](TArray<uint8>& B,uint32 V){for(int32 S=24;S>=0;S-=8)B.Add(V>>S);};
 auto Chunk=[&](const ANSICHAR* N,const TArray<uint8>& Data,bool Raw=false){TArray<uint8> Stored;if(Raw)Stored=Data;else for(int32 I=0;I<Data.Num();I+=127){int32 Count=FMath::Min(127,Data.Num()-I);Stored.Add(Count);Stored.Append(Data.GetData()+I,Count);}Body.Append(reinterpret_cast<const uint8*>(N),4);BE(Body,Stored.Num());Body.Append(Stored);};
 TArray<uint8> Grid;Grid.Init(0,128*128*2);Chunk("ALTM",Grid,true);Grid.Init(0,128*128);Chunk("XTER",Grid);
 for(int32 I=0;I<Ids.Num();++I){int32 X=8+(I%6)*18,Y=8+(I/6)*18,N=FSimCopterPeopleCityRules::GetFootprintSizeForBuildingId(Ids[I]);for(int32 DY=0;DY<N;++DY)for(int32 DX=0;DX<N;++DX)Grid[(Y+DY)*128+X+DX]=Ids[I];}
 for(int32 X=48;X<80;++X)Grid[120*128+X]=0x49;
 Chunk("XBLD",Grid);Grid.Init(0,128*128);for(const ANSICHAR* N:{"XZON","XUND","XTXT","XBIT"})Chunk(N,Grid);
 TArray<uint8> Bytes;Bytes.Append(reinterpret_cast<const uint8*>("FORM"),4);BE(Bytes,Body.Num());Bytes.Append(Body);
 const FString File=Folder/TEXT("selected-buildings.sc2");FFileHelper::SaveArrayToFile(Bytes,*File);
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 ON_SCOPE_EXIT {GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
 auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
 City->bLoadOnConstruction=false;City->bLoadOnBeginPlay=false;City->CityFile.FilePath=File;City->GetRootComponent()->SetMobility(EComponentMobility::Static);City->FinishSpawning(FTransform::Identity);City->RebuildCity();
 TArray<UInstancedStaticMeshComponent*> Models;TArray<FBox> Bounds;
 for(int32 I=0;I<Ids.Num();++I)
 {
  TArray<UInstancedStaticMeshComponent*> Parts;City->GetBuildingModelComponents(Ids[I],Parts);
  if(!TestEqual(*FString::Printf(TEXT("XBLD %d uses one instanced model"),Ids[I]),Parts.Num(),1))return false;
  bool Bound=false;for(int32 J=0;J<Parts[0]->GetNumMaterials();++J)Bound|=Parts[0]->GetMaterial(J)->GetName()==TEXT("M_SelectedBuildings") || Parts[0]->GetMaterial(J)->GetName()==TEXT("M_EmergencyDetails");
  TestTrue(TEXT("Real city binds authored material"),Bound);Models.Add(Parts[0]);FBox B;TestTrue(TEXT("Building has collision bounds"),City->TryGetBuildingBoundsAtTile(8+(I%6)*18,8+(I/6)*18,B));Bounds.Add(B);
 }
 // The middle of an eight-tile span must remain empty below the original deck.
 FHitResult Hit,DeckHit;
 const FVector BridgeXY((68+.5-64)*400,-(120+.5-64)*400,0);
 if(!TestTrue(TEXT("Causeway has its original raised road deck"),World->LineTraceSingleByChannel(DeckHit,BridgeXY+FVector(0,0,1000),BridgeXY-FVector(0,0,1000),ECC_Visibility)))return false;
 TestTrue(TEXT("Deck is above the fixture's 200 cm terrain"),DeckHit.ImpactPoint.Z>350 && DeckHit.ImpactPoint.Z<420);
 // Use the measured deck altitude. GEO road heights are relative to terrain, not world zero.
 auto Point=[&](int32 X){return FVector((X+.5-64)*400,-(120+.5-64)*400,DeckHit.ImpactPoint.Z-105);};
 TestFalse(TEXT("Helicopter path beneath midspan is open"),World->LineTraceSingleByChannel(Hit,Point(68)+FVector(0,-220,0),Point(68)+FVector(0,220,0),ECC_Visibility));
 TestTrue(TEXT("Support is collidable at its actual pier"),World->LineTraceSingleByChannel(Hit,Point(64)+FVector(0,-220,0),Point(64)+FVector(0,220,0),ECC_Visibility));
 // A conservative 4.4 m rotor disc and 1 m tall airframe envelope, not just a ray.
 const auto Aircraft=FCollisionShape::MakeBox(FVector(220,220,50));
 TestFalse(TEXT("Full rotor/airframe envelope clears midspan"),World->SweepSingleByChannel(Hit,Point(68)+FVector(0,-500,0),Point(68)+FVector(0,500,0),FQuat::Identity,ECC_Visibility,Aircraft));
 TestTrue(TEXT("Airframe envelope detects the pier"),World->SweepSingleByChannel(Hit,Point(64)+FVector(0,-500,0),Point(64)+FVector(0,500,0),FQuat::Identity,ECC_Visibility,Aircraft));
 auto CheckMerged=[&]()
 {
  City->bInstanceBuildingMeshes=false;City->RebuildCity();bool Bound=false,Glass=false;
  for(int32 I=0;I<City->OriginalMeshComponent->GetNumMaterials();++I)
  {
   auto* Material=City->OriginalMeshComponent->GetMaterial(I);if(!Material)continue;
   Bound|=Material->GetName()==TEXT("M_SelectedBuildings");Glass|=Material->GetName()==TEXT("M_SelectedBuildingGlass");
  }
  TestTrue(TEXT("Merged rendering keeps selected building surfaces"),Bound);
  TestTrue(TEXT("Merged rendering keeps biosphere glazing"),Glass);
 };
 if(GUsingNullRHI){CheckMerged();return true;}
 auto* Sun=NewObject<UDirectionalLightComponent>(City);Sun->SetIntensity(120000);Sun->SetWorldRotation(FRotator(-50,-35,0));Sun->RegisterComponent();
 auto* Fill=NewObject<UDirectionalLightComponent>(City);Fill->SetIntensity(35000);Fill->SetWorldRotation(FRotator(-40,145,0));Fill->SetCastShadows(false);Fill->RegisterComponent();
 auto* Capture=NewObject<USceneCaptureComponent2D>(City);Capture->RegisterComponent();Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->CaptureSource=SCS_FinalColorLDR;Capture->bAlwaysPersistRenderingState=true;Capture->FOVAngle=45;
 Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
 Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
 Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
 Capture->PostProcessSettings.bOverride_CameraISO=true;Capture->PostProcessSettings.CameraISO=100;Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true;Capture->PostProcessSettings.CameraShutterSpeed=125;Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true;Capture->PostProcessSettings.DepthOfFieldFstop=8;
 Capture->ShowFlags.SetBloom(false);Capture->ShowFlags.SetTemporalAA(false);
 auto* Target=NewObject<UTextureRenderTarget2D>(City);Target->RenderTargetFormat=RTF_RGBA8;Target->InitAutoFormat(1200,1000);Target->UpdateResourceImmediate(true);Capture->TextureTarget=Target;
 FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();World->GetSubsystem<USimCopterDayNightSubsystem>()->Tick(0);World->UpdateParameterCollectionInstances(true,true);{FGlobalComponentRecreateRenderStateContext Recreate;}
 for(int32 I=0;I<Ids.Num();++I)
 {
  Capture->ClearShowOnlyComponents();Capture->ShowOnlyComponent(Models[I]);const FVector Aim=Bounds[I].GetCenter();const double D=Bounds[I].GetSize().GetMax();
  Capture->SetWorldLocation(Aim+FVector(1.3,-1.6,1.05)*D);Capture->SetWorldRotation((Aim-Capture->GetComponentLocation()).Rotation());
  for(int32 Frame=0;Frame<4;++Frame){World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();}
  TArray<FColor> Pixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);int32 Visible=0;for(auto& P:Pixels){P.A=255;Visible+=P.R>30 || P.G>30 || P.B>30;}
  TestTrue(TEXT("Offscreen image contains visible geometry"),Visible>1000);
  TestTrue(TEXT("Render exported"),FImageUtils::SaveImageByExtension(*(Folder/FString::Printf(TEXT("building-%d.png"),Ids[I])),FImageView(Pixels.GetData(),1200,1000)));
 }
 Capture->ClearShowOnlyComponents();Capture->ShowOnlyComponent(City->OriginalMeshComponent);
 const FVector BridgeAim=Point(68);Capture->SetWorldLocation(BridgeAim+FVector(1500,-1900,650));Capture->SetWorldRotation((BridgeAim-Capture->GetComponentLocation()).Rotation());
 for(int32 Frame=0;Frame<4;++Frame){World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();}
 TArray<FColor> BridgePixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(BridgePixels);for(auto& P:BridgePixels)P.A=255;
 FImageUtils::SaveImageByExtension(*(Folder/TEXT("causeway.png")),FImageView(BridgePixels.GetData(),1200,1000));
 Sun->SetIntensity(0);Fill->SetIntensity(0);Capture->ClearShowOnlyComponents();Capture->ShowOnlyComponent(Models[0]);
 const FVector Aim=Bounds[0].GetCenter();Capture->SetWorldLocation(Aim+FVector(1.3,-1.6,1.05)*Bounds[0].GetSize().GetMax());Capture->SetWorldRotation((Aim-Capture->GetComponentLocation()).Rotation());
 Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=-2;
 auto* Collection=LoadObject<UMaterialParameterCollection>(nullptr,TEXT("/Game/Materials/MPC_SimCopterDayNight.MPC_SimCopterDayNight"));auto* Values=World->GetParameterCollectionInstance(Collection);
 Values->SetScalarParameterValue(TEXT("WindowLitFraction"),1);Values->SetScalarParameterValue(TEXT("WindowRowLitFraction"),1);Values->SetScalarParameterValue(TEXT("WindowGlowNits"),25);
 auto Night=[&](float Blend,float Glow,const TCHAR* Name)
 {
  Values->SetScalarParameterValue(TEXT("NightBlend"),Blend);Values->SetScalarParameterValue(TEXT("WindowGlowNits"),Glow);
  for(int32 Frame=0;Frame<4;++Frame){World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();}
  TArray<FColor> Pixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);int32 Lit=0;for(auto& P:Pixels){P.A=255;Lit+=P.R>80;}
  FImageUtils::SaveImageByExtension(*(Folder/Name),FImageView(Pixels.GetData(),1200,1000));return Lit;
 };
 const int32 Off=Night(0,25,TEXT("corporate-windows-off.png")),On=Night(1,25,TEXT("corporate-night.png")),Disabled=Night(1,0,TEXT("corporate-windows-disabled.png"));
 TestTrue(TEXT("Selected windows follow night fade"),On>1000 && Off<On/20);
 TestTrue(TEXT("Window brightness control disables selected windows"),Disabled<On/20);
 Values->SetScalarParameterValue(TEXT("WindowLitFraction"),.25f);Values->SetScalarParameterValue(TEXT("WindowRowLitFraction"),.05f);
 const int32 Partial=Night(1,25,TEXT("corporate-night-selective.png"));
 TestTrue(TEXT("Window density controls select a subset of complete panes"),Partial>On/20 && Partial<On*3/4);
 CheckMerged();return true;
}
#endif

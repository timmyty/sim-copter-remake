#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Components/BoxComponent.h"
#include "Ground/SimCopterBaseballGame.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterGroundAgent.h"
#include "City/SimCity2000CityActor.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "City/SimCopterDayNight.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterBaseballGameTest,"SimCopter.BaseballGames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FSimCopterBaseballGameTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
	for (const TCHAR* Name : {TEXT("CountsAndWalks"),TEXT("HitsAndBattingOrder"),TEXT("InningsAndWalkoffs"),
		TEXT("ActivePlayers"),TEXT("Distance"),TEXT("InterruptionAndSave"),TEXT("CompleteGame"),TEXT("Canyon"),TEXT("Capture"),TEXT("CapacityAndLegacy"),TEXT("BlockedPlay"),TEXT("TeamUniforms"),TEXT("Scoreboard")})
	{ Names.Add(Name); Commands.Add(Name); }
}
bool FSimCopterBaseballGameTest::RunTest(const FString& Case)
{
	using E=ESimCopterBaseballPlay;
	if (Case==TEXT("Capture") && GUsingNullRHI) { AddInfo(TEXT("Visual capture requires RenderOffscreen; not exercised with NullRHI.")); return true; }
	if (Case==TEXT("CountsAndWalks"))
	{
		FSimCopterBaseballScore S;
		S.Apply(E::Foul); S.Apply(E::Foul); S.Apply(E::Foul);
		TestEqual(TEXT("Repeated fouls retain two strikes"),S.Strikes,2); TestEqual(TEXT("Fouls are not outs"),S.Outs,0);
		S.Apply(E::Strike); TestEqual(TEXT("Third strike is out"),S.Outs,1); TestEqual(TEXT("Next batter"),S.Batter(),1);
		for (int32 I=0; I<16; ++I) S.Apply(E::Ball);
		TestEqual(TEXT("Four consecutive walks force exactly one run"),S.Runs[0],1);
		TestEqual(TEXT("Walk resets count"),S.Balls,0); TestEqual(TEXT("Runner forced to third"),S.Bases[2],2);
		S=FSimCopterBaseballScore(); S.Bases[2]=8;
		for (int32 I=0; I<4; ++I) S.Apply(E::Ball);
		TestEqual(TEXT("Unforced runner stays on third"),S.Bases[2],8); TestEqual(TEXT("No run on unforced walk"),S.Runs[0],0);
		return true;
	}
	if (Case==TEXT("HitsAndBattingOrder"))
	{
		FSimCopterBaseballScore S;
		S.Apply(E::Single); S.Apply(E::Double); S.Apply(E::Triple);
		TestEqual(TEXT("Triple scores both occupied bases"),S.Runs[0],2); TestEqual(TEXT("Tripler remains on third"),S.Bases[2],2);
		S.Apply(E::HomeRun); TestEqual(TEXT("Home run scores runner and batter"),S.Runs[0],4);
		for (int32 I=0; I<5; ++I) S.Apply(E::HomeRun);
		TestEqual(TEXT("Nine-person batting order wraps"),S.Batter(),0);
		for (int32 B : S.Bases) TestEqual(TEXT("Home run clears bases"),B,INDEX_NONE);
		S.Apply(E::Single); S.Apply(E::GroundOut); S.Apply(E::FlyOut);
		TestEqual(TEXT("Held runner is not removed by batter outs"),S.Bases[0],0);
		TestEqual(TEXT("Two fielded outs"),S.Outs,2);
		return true;
	}
	if (Case==TEXT("InningsAndWalkoffs"))
	{
		FSimCopterBaseballScore S;
		for (int32 I=0; I<54; ++I) S.Apply(E::GroundOut);
		TestEqual(TEXT("Tie after nine enters tenth"),S.Inning,10); TestFalse(TEXT("Tie is not final"),S.bFinal);
		S.Apply(E::HomeRun); for (int32 I=0; I<6; ++I) S.Apply(E::FlyOut);
		TestTrue(TEXT("Visitors win after home gets its last turn"),S.bFinal); TestEqual(TEXT("Extra inning score"),S.Runs[0],1);
		const auto Final=S; S.Apply(E::HomeRun); TestEqual(TEXT("Final score is immutable"),S.Runs[1],Final.Runs[1]);
		S=FSimCopterBaseballScore(); S.Inning=9; S.Runs[1]=1;
		for (int32 I=0; I<3; ++I) S.Apply(E::Strike);
		for (int32 I=0; I<2; ++I) S.Apply(E::GroundOut);
		TestTrue(TEXT("Home lead skips unnecessary bottom ninth"),S.bFinal);
		S=FSimCopterBaseballScore(); S.Inning=9; S.BattingTeam=1; S.Bases[2]=17;
		S.Apply(E::Single); TestTrue(TEXT("Walk-off ends before three outs"),S.bFinal); TestEqual(TEXT("Walk-off run"),S.Runs[1],1);
		return true;
	}
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	if (Case==TEXT("Canyon") || Case==TEXT("Capture"))
	{
		AActor* CaptureRig=nullptr;
		UDirectionalLightComponent* CaptureSun=nullptr;
		if (Case==TEXT("Capture"))
		{
			// Install lighting before players/scoreboard query the per-frame exposure cache.
			CaptureRig=World->SpawnActor<AActor>();
			CaptureSun=NewObject<UDirectionalLightComponent>(CaptureRig); CaptureSun->SetIntensity(120000);
			CaptureSun->SetWorldRotation(FRotator(-45,120,0)); CaptureSun->RegisterComponent();
		}
		auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
		City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false;
		City->bRenderProceduralMapExtension=false; City->bRenderStreetLightSpotLights=false;
		City->GetRootComponent()->SetMobility(EComponentMobility::Static);
		City->CityFile.FilePath=SimCopterOriginalGame::ResolveRoot()/TEXT("cities/career/city20.sc2");
		City->FinishSpawning(FTransform::Identity); City->RebuildCity();
		if (!TestTrue(TEXT("Canyon loads"),Traffic->RebuildSpawnData())) return false;
		for (int32 N=0; N<Traffic->PedestrianNodes.Num(); ++N)
		{
			const auto& Node=Traffic->PedestrianNodes[N]; if (Node.BuildingId!=0xd7) continue;
			Traffic->ClearBaseballGames(); Traffic->SetActorLocation(Node.Location);
			int32 Attempts=1; Traffic->TrySpawnSpecialBuildingPeople(Node.FileX,Node.FileY,Attempts);
			auto* Game=Traffic->BaseballGames.FindRef(N).Get();
			if (!TestNotNull(TEXT("Each actual field starts a full game"),Game)) continue;
			TestEqual(TEXT("Nine fielders and batter fit Canyon geometry"),Traffic->PedestrianAgents.Num(),10);
			if (Case==TEXT("Capture"))
			{
				for (auto* Prop : {Game->Ball.Get(),Game->Bat.Get(),Game->BoardBacking.Get()})
				{
					const auto* Mesh=Prop->GetProcMeshSection(0);
					TestNotNull(TEXT("Prop/scoreboard has generated geometry"),Mesh);
					if (!Mesh) continue;
					for (int32 I=0; I<Mesh->ProcIndexBuffer.Num(); I+=3)
					{
						const auto& A=Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I]];
						const auto& B=Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I+1]];
						const auto& C=Mesh->ProcVertexBuffer[Mesh->ProcIndexBuffer[I+2]];
						TestTrue(TEXT("Prop winding exposes the lit outer face"),FVector::DotProduct(
							FVector::CrossProduct(B.Position-A.Position,C.Position-A.Position),A.Normal)<=.001);
					}
				}
				auto* Rig=CaptureRig;
				if (auto* Day=World->GetSubsystem<USimCopterDayNightSubsystem>()) Day->Tick(0);
				World->UpdateParameterCollectionInstances(true,true);
				auto* Capture=NewObject<USceneCaptureComponent2D>(Rig);
				Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
				Capture->bAlwaysPersistRenderingState=true; Capture->CaptureSource=SCS_FinalColorLDR;
				Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
				Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
				Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
				Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
				Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
				Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetBloom(false);
				auto* Target=NewObject<UTextureRenderTarget2D>(Rig); Target->RenderTargetFormat=RTF_RGBA8;
				Target->InitAutoFormat(1600,1000); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target;
				Capture->RegisterComponent(); Capture->FOVAngle=60;
				const FVector Camera=Game->Point({180,-180},1350);
				Capture->SetWorldLocationAndRotation(Camera,(Node.Location-Camera).Rotation());
				Traffic->SetActorLocation(Camera);
				for (int32 Frame=0; Frame<50; ++Frame)
				{
					Game->Tick(1.f/15);
					for (auto P : Traffic->PedestrianAgents) if (P.IsValid()) P->Tick(1.f/15);
				}
				auto SaveCapture=[&](const TCHAR* Name)
				{
					FAssetCompilingManager::Get().FinishAllCompilation();
					for (int32 Pass=0; Pass<3; ++Pass)
					{
						World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
						if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
						FAssetCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
					}
					TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
					for (auto& Pixel : Pixels) Pixel.A=255;
					TestTrue(TEXT("Captured actual field"),FImageUtils::SaveImageByExtension(
						*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/baseball-stadium-board")/Name),FImageView(Pixels.GetData(),1600,1000)));
				};
				SaveCapture(TEXT("canyon-live.png"));
				Game->Score.Inning=9; Game->Score.Runs[0]=7; Game->Score.Runs[1]=4;
				Game->Score.bFinal=true; Game->SetPhase(ASimCopterBaseballGame::EPhase::Final); Game->UpdateBoard();
				Game->Ball->SetVisibility(false); Game->Bat->SetVisibility(false);
				SaveCapture(TEXT("canyon-final.png"));
				const FTransform BoardTransform=Game->BoardRoot->GetComponentTransform();
				const FVector BoardCenter=BoardTransform.TransformPosition(FVector(0,0,310));
				Capture->FOVAngle=55;
				for (const auto& View : {TPair<FString,FVector>(TEXT("stadium-side.png"),FVector(1100,650,380)),
					TPair<FString,FVector>(TEXT("stadium-back.png"),FVector(-1100,650,380))})
				{
					// Illuminate the rear for a geometry inspection; the fixture has no sky fill.
					if (View.Value.X<0) CaptureSun->SetWorldRotation(FRotator(-45,-60,0));
					const FVector Eye=BoardCenter+BoardTransform.TransformVector(View.Value);
					Traffic->SetActorLocation(Eye); Game->Tick(1.f/15);
					TestTrue(TEXT("Cabinet stays fixed for side/rear views"),BoardTransform.Equals(Game->BoardRoot->GetComponentTransform()));
					Capture->SetWorldLocationAndRotation(Eye,(BoardCenter-FVector(0,0,90)-Eye).Rotation());
					SaveCapture(*View.Key);
				}

				Traffic->ClearBaseballGames(); return true;
			}
			Game->Random.Initialize(719);
			for (int32 Frame=0; Frame<100000 && !Game->Score.bFinal; ++Frame)
			{
				Game->Tick(1.f/15);
				for (auto P : Traffic->PedestrianAgents) if (P.IsValid()) P->Tick(1.f/15);
			}
			TestTrue(TEXT("Actual field completes a full nine-inning game"),Game->Score.bFinal);
			AddInfo(FString::Printf(TEXT("Canyon (%d,%d): inning %d, score %d-%d, phase %d, batter %d, %s"),Node.FileX,Node.FileY,Game->Score.Inning,Game->Score.Runs[0],Game->Score.Runs[1],int32(Game->Phase),Game->Score.Batter(),*Game->Announcement));
		}
		Traffic->ClearBaseballGames(); return true;
	}
	Traffic->ActiveTileSize=400; Traffic->MaxVehicleAgents=0;
	Traffic->PeopleTileClasses.Init(13,FSimCity2000City::TileCount); Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount); Traffic->PeopleTerrainTypes.Init(0,FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0,FSimCity2000City::TileCount);
	Traffic->ActiveOriginalGameRootPath=SimCopterOriginalGame::ResolveRoot(); Traffic->PedestrianMeshNames.Add(TEXT("5man"));
	FSimCopterGroundRouteNode Node; Node.FileX=62; Node.FileY=62; Node.BuildingId=0xd7;
	Node.PeopleFootprintSize=3; Node.PeopleTileClass=13; Node.Location=FVector(-200,200,10);
	Traffic->PedestrianNodes.Add(Node);
	for (int32 Y=62; Y<65; ++Y) for (int32 X=62; X<65; ++X)
	{ Traffic->XbldTileIds[Y*128+X]=0xd7; Traffic->PedestrianNodeIndexByTile.Add(FIntPoint(X,Y),0); }
	auto* Floor=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10000,10000,10)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-10));
	if (Case==TEXT("Distance"))
	{
		auto* H=World->SpawnActor<ASimCopterHelicopterPawn>(); auto* PC=World->SpawnActor<APlayerController>();
		PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC); PC->Possess(H);
		ON_SCOPE_EXIT { PC->UnPossess(); Traffic->ClearBaseballGames(); };
		H->SetActorLocation(Node.Location+FVector(6000,0,1000)); Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("No distant simulation"),Traffic->BaseballGames.Num(),0);
		H->SetActorLocation(Node.Location+FVector(500,0,1000)); Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Helicopter arrival activates game"),Traffic->BaseballGames.Num(),1);
		TestEqual(TEXT("Arrival starts the roster without a full-team spawn burst"),Traffic->PedestrianAgents.Num(),1);
		for (int32 I=0; I<12; ++I)
		{
			const int32 Before = Traffic->PedestrianAgents.Num();
			Traffic->UpdateAgentPool(1.f/60);
			for (auto Pair : Traffic->BaseballGames) if (auto* Game=Pair.Value.Get()) Game->Tick(1.f/60);
			TestTrue(TEXT("Traffic and baseball ticks admit at most one initial player per frame"),Traffic->PedestrianAgents.Num()-Before<=1);
		}
		TestEqual(TEXT("Only ten active actors at first pitch"),Traffic->PedestrianAgents.Num(),10);
		for (int32 I=0; I<10; ++I) Traffic->UpdateAgentPool(.1f);
		TestEqual(TEXT("No duplicates during stationary observation"),Traffic->PedestrianAgents.Num(),10);
		H->SetActorLocation(Node.Location+FVector(3900,0,1000)); Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Hysteresis retains game"),Traffic->BaseballGames.Num(),1);
		H->SetActorLocation(Node.Location+FVector(8000,0,1000)); Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Departure removes coordinator"),Traffic->BaseballGames.Num(),0);
		TestEqual(TEXT("Departure removes all players"),Traffic->PedestrianAgents.Num(),0);
		H->SetActorLocation(Node.Location+FVector(500,0,1000)); Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Return creates one game"),Traffic->BaseballGames.Num(),1);
		return true;
	}
	if (Case==TEXT("CapacityAndLegacy"))
	{
		Traffic->MaxPedestrianAgents=12; int32 Budget=1;
		TestEqual(TEXT("Hard cap defers game before spawning a partial defense"),Traffic->TrySpawnSpecialBuildingPeople(62,62,Budget),0);
		Traffic->MaxPedestrianAgents=13;
		Traffic->bPlayFullBaseballGames=false; Budget=1; Traffic->TrySpawnSpecialBuildingPeople(62,62,Budget);
		TestEqual(TEXT("Old-save fixture has eight retail actors"),Traffic->PedestrianAgents.Num(),8);
		Traffic->bPlayFullBaseballGames=true; Budget=1; Traffic->TrySpawnSpecialBuildingPeople(64,64,Budget);
		TestEqual(TEXT("Old roster migrates without duplicates"),Traffic->PedestrianAgents.Num(),10);
		TestEqual(TEXT("Every footprint tile shares one game"),Traffic->BaseballGames.Num(),1);
		auto* G=Traffic->BaseballGames.FindRef(0).Get();
		Traffic->MaxPedestrianAgents=12;
		for (int32 I=0; I<3; ++I) { G->Score.Apply(E::Single); G->SyncPlayers(); }
		TestEqual(TEXT("Runners cannot bypass hard actor cap"),Traffic->PedestrianAgents.Num(),12);
		Traffic->MaxPedestrianAgents=13; TestTrue(TEXT("Available capacity completes roster"),G->SyncPlayers());
		TestEqual(TEXT("Exactly one missing player repaired"),Traffic->PedestrianAgents.Num(),13);
		Traffic->XbldTileIds[62*128+62]=0; Traffic->PruneBaseballGames();
		TestEqual(TEXT("Removed field loses its coordinator"),Traffic->BaseballGames.Num(),0);
		TestEqual(TEXT("Removed field loses its healthy actors"),Traffic->PedestrianAgents.Num(),0);
		return true;
	}
	int32 Attempts=1; Traffic->TrySpawnSpecialBuildingPeople(62,62,Attempts);
	auto* Game=Traffic->BaseballGames.FindRef(0).Get(); if (!TestNotNull(TEXT("Game created"),Game)) return false;
	ON_SCOPE_EXIT { Traffic->ClearBaseballGames(); };
	if (Case==TEXT("TeamUniforms"))
	{
		auto CheckUniforms=[&]()
		{
			FColor Jerseys[2]={FColor(0,0,0,0),FColor(0,0,0,0)};
			for (int32 I=0; I<18; ++I)
			{
				auto* P=Game->Players[I].Get(); if (!P) continue;
				if (!TestTrue(TEXT("Original player figure loaded"),P->FigureShared.IsValid() &&
					P->FigureShared->Model.Figures.IsValidIndex(P->FigureIndex))) continue;
				const auto& Figure=P->FigureShared->Model.Figures[P->FigureIndex];
				const auto* Jersey=Figure.Parts.FindByPredicate([](const FPrivAnimPart& Part) { return Part.ColorIndex==1 && !Part.FixedColor; });
				if (!TestNotNull(TEXT("Original figure contains a recolorable jersey"),Jersey)) continue;
				const FColor Color=FSimCopterPopulationFigure::ResolvePartColor(P->FigureShared->Palette,*Jersey,P->GetPedestrianFigureClothesOffset()).ToFColor(false);
				TestTrue(TEXT("Visitor jersey is red and home jersey is blue"),I<9 ? Color.R>Color.B*2 : Color.B>Color.R*2);
				if (Jerseys[I/9].A) TestEqual(TEXT("Every active teammate wears the same jersey"),Color,Jerseys[I/9]);
				Jerseys[I/9]=Color;
				bool bRendered=false;
				for (int32 Section=0; Section<P->OriginalMeshComponent->GetNumSections(); ++Section)
					if (auto* Mesh=P->OriginalMeshComponent->GetProcMeshSection(Section); Mesh && Mesh->bSectionVisible)
						bRendered|=Mesh->ProcVertexBuffer.ContainsByPredicate([Color](const FProcMeshVertex& Vertex) { return Vertex.Color==Color; });
				TestTrue(TEXT("Team jersey color reaches the visible mesh"),bRendered);
			}
		};
		CheckUniforms();
		for (int32 I=0; I<3; ++I) Game->Score.Apply(E::Single);
		Game->SyncPlayers(); CheckUniforms(); // runners retain the batting team's colors
		for (int32 I=0; I<3; ++I) Game->Score.Apply(E::GroundOut);
		Game->SyncPlayers(); Game->SetFormation();
		for (auto P : Traffic->PedestrianAgents) if (P.IsValid()) P->Tick(1.f/15);
		CheckUniforms(); // roles change, team identities and uniforms do not
		return true;
	}
	if (Case==TEXT("Scoreboard"))
	{
		const FTransform Mounted=Game->BoardRoot->GetComponentTransform();
		TestTrue(TEXT("Cabinet stands upright"),Game->BoardRoot->GetUpVector().Equals(FVector::UpVector,.001));
		TestTrue(TEXT("Cabinet faces home plate"),FVector::DotProduct(Game->BoardRoot->GetForwardVector(),
			(Game->Base(0)-Mounted.GetLocation()).GetSafeNormal2D())>.999);
		Traffic->SetActorLocation(Node.Location+FVector(-3000,2000,2000)); Game->Tick(.1f);
		TestTrue(TEXT("Flying around cannot rotate or move the scoreboard"),Mounted.Equals(Game->BoardRoot->GetComponentTransform()));
		const auto* Cabinet=Game->BoardBacking->GetProcMeshSection(0);
		if (TestNotNull(TEXT("Physical cabinet and supports exist"),Cabinet))
		{
			TestTrue(TEXT("Cabinet has real depth and a rear face"),Cabinet->SectionLocalBox.GetSize().X>50);
			TestTrue(TEXT("Support footings reach ground level"),Cabinet->SectionLocalBox.Min.Z<=0);
		}
		TestEqual(TEXT("New game opens at zero"),Game->BoardReadout[0],FString(TEXT("00")));
		const auto Before=Game->BoardLamps->GetProcMeshSection(0)->ProcVertexBuffer;
		Game->Score.Apply(E::HomeRun); Game->Score.Apply(E::Ball); Game->Score.Apply(E::Strike); Game->UpdateBoard();
		TestEqual(TEXT("Visitor run updates its score only"),Game->BoardReadout[0],FString(TEXT("01")));
		TestEqual(TEXT("Home score stays zero"),Game->BoardReadout[1],FString(TEXT("00")));
		TestTrue(TEXT("Score change changes the actual bulb geometry"),Before.Num()!=Game->BoardLamps->GetProcMeshSection(0)->ProcVertexBuffer.Num());
		TestEqual(TEXT("Ball count"),Game->BoardReadout[3],FString(TEXT("1")));
		TestEqual(TEXT("Strike count"),Game->BoardReadout[4],FString(TEXT("1")));
		for (int32 I=0; I<3; ++I) Game->Score.Apply(E::GroundOut);
		Game->UpdateBoard();
		TestEqual(TEXT("Board follows side change"),Game->BoardReadout[6],FString(TEXT("BOT")));
		Game->Score.Inning=9;
		for (int32 I=0; I<3; ++I) { Game->Play=E::GroundOut; Game->FinishPlay(); }
		TestEqual(TEXT("Final inning shown"),Game->BoardReadout[2],FString(TEXT("09")));
		TestEqual(TEXT("Visitor winner shown"),Game->BoardReadout[7],FString(TEXT("FINAL - VISITORS WIN")));
		Game->Tick(.25f);
		TestEqual(TEXT("Final score stays visible"),Game->BoardReadout[0],FString(TEXT("01")));
		Game->Score=FSimCopterBaseballScore(); Game->Score.Inning=9; Game->Score.BattingTeam=1;
		Game->Play=E::HomeRun; Game->FinishPlay();
		TestEqual(TEXT("Walk-off identifies the home winner"),Game->BoardReadout[7],FString(TEXT("FINAL - HOME WIN")));
		TestEqual(TEXT("Walk-off run shown"),Game->BoardReadout[1],FString(TEXT("01")));
		Game->PhaseAge=15; Game->Tick(.25f);
		TestEqual(TEXT("New game clears both scores"),Game->BoardReadout[1],FString(TEXT("00")));
		TestEqual(TEXT("New game clears winner"),Game->BoardReadout[7],FString(TEXT("NEW GAME")));
		TestEqual(TEXT("New game restores first inning"),Game->BoardReadout[2],FString(TEXT("01")));
		return true;
	}
	if (Case==TEXT("BlockedPlay"))
	{
		// Immobilize an in-play runner; unresolved movement cannot silently award a hit/run.
		Game->Play=E::HomeRun; Game->StartRunners(); Game->SetPhase(ASimCopterBaseballGame::EPhase::InPlay);
		Game->BallStart=Game->Base(0); Game->BallEnd=Game->Point({-85,75},5);
		for (int32 I=0; I<240; ++I) Game->Tick(1.f/15); // no agent ticks: runner cannot arrive
		TestEqual(TEXT("Blocked player has not scored"),Game->Score.Runs[0],0);
		TestEqual(TEXT("Blocked at-bat is replayed"),Game->Score.Batter(),0);
		TestTrue(TEXT("Only ten actors during interruption"),Traffic->PedestrianAgents.Num()==10);
		return true;
	}
	if (Case==TEXT("ActivePlayers"))
	{
		TestEqual(TEXT("Opening active count"),Traffic->PedestrianAgents.Num(),10);
		for (int32 I=0; I<3; ++I) { Game->Score.Apply(E::Single); Game->SyncPlayers(); }
		TestEqual(TEXT("Loaded bases renders maximum thirteen"),Traffic->PedestrianAgents.Num(),13);
		for (int32 I=0; I<3; ++I) Game->Score.Apply(E::GroundOut);
		Game->SyncPlayers(); TestEqual(TEXT("Change sides frees benched actors"),Traffic->PedestrianAgents.Num(),10);
		TestEqual(TEXT("Home batting roster"),Game->Score.Batter(),9);
		for (int32 I=10; I<18; ++I) TestFalse(TEXT("Eight benched players have no actors"),Game->Players[I].IsValid());
		return true;
	}
	if (Case==TEXT("InterruptionAndSave"))
	{
		auto* Patient=Game->Players[9].Get(); Patient->SetMissionInjuredPose(); Game->Tick(.1f);
		TestFalse(TEXT("Injury immediately releases baseball control"),Patient->IsPlayingBaseball());
		TestEqual(TEXT("Interrupted game does not advance"),Game->Score.Batter(),0);
		TestTrue(TEXT("Interruption visible on board"),Game->Announcement.Contains(TEXT("INTERRUPTED")));
		TArray<uint8> Data; TestTrue(TEXT("Traffic save works with an interrupted game"),Traffic->CaptureRuntimeSaveState(Data));
		Traffic->ClearBaseballGames();
		TestFalse(TEXT("Cleanup preserves injured actor"),Patient->IsActorBeingDestroyed());
		TestTrue(TEXT("Traffic restore works"),Traffic->RestoreRuntimeSaveState(Data,nullptr));
		TestEqual(TEXT("Only injured player is serialized; healthy players are transient"),Traffic->PedestrianAgents.Num(),1);
		return true;
	}
	Game->Random.Initialize(719);
	int32 MaxPlayers=0; bool bSawRunner=false, bSawHome=false;
	for (int32 I=0; I<100000 && !Game->Score.bFinal; ++I)
	{
		Game->Tick(1.f/15);
		for (auto P : Traffic->PedestrianAgents) if (P.IsValid()) P->Tick(1.f/15);
		MaxPlayers=FMath::Max(MaxPlayers,Traffic->PedestrianAgents.Num());
		bSawRunner|=Traffic->PedestrianAgents.Num()>10; bSawHome|=Game->Score.BattingTeam==1;
	}
	TestTrue(TEXT("Real actor movement completes a nine-inning game"),Game->Score.bFinal);
	TestTrue(TEXT("Both batting sides play"),bSawHome); TestTrue(TEXT("Hits produce active runners"),bSawRunner);
	TestTrue(TEXT("Never more than thirteen rendered players"),MaxPlayers<=13);
	AddInfo(FString::Printf(TEXT("Final %d-%d, inning %d, phase %d, batter %d, max actors %d, %s"),Game->Score.Runs[0],Game->Score.Runs[1],Game->Score.Inning,int32(Game->Phase),Game->Score.Batter(),MaxPlayers,*Game->Announcement));
	return true;
}
#endif

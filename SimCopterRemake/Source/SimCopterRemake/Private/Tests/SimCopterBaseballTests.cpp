#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Components/BoxComponent.h"
#include "City/SimCity2000CityActor.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterOriginalGamePaths.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterBaseballTest, "SimCopter.Behavior.Baseball",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FSimCopterBaseballTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* Name : {TEXT("SpawnBudget"), TEXT("Passerby"), TEXT("PartialTeam"),
		TEXT("WarpArrival"), TEXT("Distance"), TEXT("Capacity"), TEXT("OriginalPrograms"), TEXT("Canyon")})
	{
		Names.Add(Name); Commands.Add(Name);
	}
}

bool FSimCopterBaseballTest::RunTest(const FString& Case)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	// Preserve retail restoration coverage independently of the requested full-game extension.
	Traffic->bPlayFullBaseballGames = false;
	if (Case == TEXT("Canyon"))
	{
		auto* City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
		City->bLoadOnConstruction = false; City->bLoadOnBeginPlay = false;
		City->bRenderProceduralMapExtension = false; City->bRenderStreetLightSpotLights = false;
		City->GetRootComponent()->SetMobility(EComponentMobility::Static);
		City->CityFile.FilePath = SimCopterOriginalGame::ResolveRoot() / TEXT("cities/career/city20.sc2");
		City->FinishSpawning(FTransform::Identity); City->RebuildCity();
		if (!TestTrue(TEXT("Canyon spawn data loads"), Traffic->RebuildSpawnData())) return false;
		int32 Fields = 0;
		for (const auto& Node : Traffic->PedestrianNodes)
		{
			if (Node.BuildingId != 0xd7) continue;
			++Fields;
			Traffic->SetActorLocation(Node.Location);
			int32 Budget = 1;
			TestEqual(TEXT("Actual Canyon field spawns all eight players"), Traffic->TrySpawnSpecialBuildingPeople(Node.FileX,Node.FileY,Budget),8);
			AddInfo(FString::Printf(TEXT("Canyon baseball field (%d,%d), centre %s"),Node.FileX,Node.FileY,*Node.Location.ToString()));
		}
		TestTrue(TEXT("Canyon has baseball fields"),Fields>0);
		return true;
	}
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(13,FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount);
	Traffic->PeopleTerrainTypes.Init(0,FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0,FSimCity2000City::TileCount);
	Traffic->ActiveOriginalGameRootPath = SimCopterOriginalGame::ResolveRoot();
	Traffic->PedestrianMeshNames.Add(TEXT("5man"));
	Traffic->MaxVehicleAgents = 0;
	FSimCopterGroundRouteNode Node;
	Node.FileX=62; Node.FileY=62; Node.BuildingId=0xd7;
	Node.PeopleFootprintSize=3; Node.PeopleTileClass=13; Node.Location=FVector(-200,200,10);
	Traffic->PedestrianNodes.Add(Node);
	for (int32 Y=62;Y<65;++Y) for (int32 X=62;X<65;++X)
	{
		Traffic->XbldTileIds[Y*128+X]=0xd7;
		Traffic->PedestrianNodeIndexByTile.Add(FIntPoint(X,Y),0);
	}
	auto* Floor=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(10000,10000,10));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-10));
	const auto CountTeam = [&]()
	{
		int32 N=0; for (const auto& P : Traffic->PedestrianAgents)
			if (P.IsValid() && (P->InitialBehaviorProgramId==1205 || P->InitialBehaviorProgramId==1206)) ++N;
		return N;
	};
	if (Case==TEXT("Distance"))
	{
		auto* Helicopter=World->SpawnActor<ASimCopterHelicopterPawn>();
		auto* PC=World->SpawnActor<APlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine);
		World->AddController(PC); PC->Possess(Helicopter);
		ON_SCOPE_EXIT { PC->UnPossess(); };
		Traffic->SetActorLocation(Node.Location);
		const float Radius=Traffic->OriginalAmbientScanRadiusTiles*Traffic->ActiveTileSize;
		Helicopter->SetActorLocation(Node.Location+FVector(Radius+400,0,1000));
		Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Distant field has no team even with traffic actor at the field"),CountTeam(),0);
		int32 Attempts=8;
		TestEqual(TEXT("Leading-edge path also respects helicopter distance"),Traffic->TrySpawnSpecialBuildingPeople(62,62,Attempts),0);
		Helicopter->SetActorLocation(Node.Location+FVector(Radius-400,0,1000));
		Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Approaching helicopter activates eight players"),CountTeam(),8);
		for (int32 I=0;I<30;++I) Traffic->UpdateAgentPool(0.1f);
		TestEqual(TEXT("Repeated nearby updates keep population bounded"),CountTeam(),8);
		Helicopter->SetActorLocation(Node.Location+FVector(8000,0,1000));
		for (int32 I=0;I<30;++I) Traffic->UpdateAgentPool(0.1f);
		TestEqual(TEXT("Flying away removes all eight players"),CountTeam(),0);
		Helicopter->SetActorLocation(Node.Location+FVector(0,0,1000));
		Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Returning recreates exactly one complete team"),CountTeam(),8);
		return true;
	}
	if (Case==TEXT("Passerby") || Case==TEXT("WarpArrival"))
	{
		const int32 Count=Case==TEXT("WarpArrival") ? Traffic->OriginalAmbientRandomCap : 1;
		for (int32 I=0;I<Count;++I)
		{
			auto* P=World->SpawnActor<ASimCopterGroundAgent>(); P->SetOwner(Traffic);
			P->SetActorLocation(Node.Location+FVector(I*2,1000,92)); Traffic->PedestrianAgents.Add(P);
		}
	}
	if (Case==TEXT("WarpArrival"))
	{
		Traffic->SetActorLocation(Node.Location);
		Traffic->LastAmbientScanTileX=63; Traffic->LastAmbientScanTileY=63;
		Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Stationary arrival inside scan perimeter works at civilian cap"),CountTeam(),8);
		Traffic->UpdateAgentPool(1);
		TestEqual(TEXT("Repeated population think never duplicates the team"),CountTeam(),8);
		return true;
	}
	int32 Budget=Case==TEXT("SpawnBudget") ? 1 : 8;
	if (Case==TEXT("Capacity"))
	{
		Traffic->MaxPedestrianAgents=7;
		TestEqual(TEXT("Hard cap defers the team without truncating it"),Traffic->TrySpawnSpecialBuildingPeople(62,62,Budget),0);
		TestEqual(TEXT("No partial team created at hard cap"),CountTeam(),0);
		Traffic->MaxPedestrianAgents=8;
	}
	Traffic->TrySpawnSpecialBuildingPeople(62,62,Budget);
	TestEqual(TEXT("Full team: one batter and seven fielders"),CountTeam(),8);
	if (CountTeam()!=8) return false;
	if (Case==TEXT("PartialTeam"))
	{
		// Simulate an old save/culled team containing a non-prefix subset of the fielders.
		for (int32 I=6;I>=2;--I) { Traffic->PedestrianAgents[I]->Destroy(); Traffic->PedestrianAgents.RemoveAt(I); }
		Budget=8; Traffic->TrySpawnSpecialBuildingPeople(64,64,Budget);
		TestEqual(TEXT("Another tile of the same field repairs missing slots"),CountTeam(),8);
		for (int32 I=0;I<Traffic->PedestrianAgents.Num();++I) for (int32 J=I+1;J<Traffic->PedestrianAgents.Num();++J)
			TestTrue(TEXT("Repaired players occupy distinct positions"),FVector::Dist2D(Traffic->PedestrianAgents[I]->GetActorLocation(),Traffic->PedestrianAgents[J]->GetActorLocation())>80);
	}
	int32 Batters=0,Fielders=0,TeamColor=INDEX_NONE;
	ASimCopterGroundAgent* Batter=nullptr; ASimCopterGroundAgent* Fielder=nullptr;
	for (const auto& Weak : Traffic->PedestrianAgents)
	{
		auto* P=Weak.Get(); if (!P || (P->InitialBehaviorProgramId!=1205 && P->InitialBehaviorProgramId!=1206)) continue;
		TestEqual(TEXT("Original baseball figure is class 4"),P->InitialBehaviorClass,4);
		TestTrue(TEXT("Original figure loaded"),P->IsUsingOriginalMesh());
		TestFalse(TEXT("Helicopter curiosity cannot steal players"),P->IsEligibleForAmbientCuriosity());
		if (P->InitialBehaviorProgramId==1205) { ++Batters; Batter=P; }
		else { ++Fielders; Fielder=P; if (TeamColor==INDEX_NONE) TeamColor=P->FigureClothesOffset;
			TestEqual(TEXT("Fielders share their original team color"),P->FigureClothesOffset,TeamColor); }
	}
	TestEqual(TEXT("Exactly one batter"),Batters,1); TestEqual(TEXT("Exactly seven fielders"),Fielders,7);
	Budget=8; Traffic->TrySpawnSpecialBuildingPeople(63,63,Budget);
	TestEqual(TEXT("Rescanning footprint cannot duplicate players"),CountTeam(),8);
	if (Case==TEXT("OriginalPrograms") && Batter && Fielder)
	{
		TestEqual(TEXT("Original state-zero baseball movement does not auto-turn"),Batter->GetBehaviorAttribute(EBhavAttr::AutoTurn),0);
		TSet<FString> Clips; TSet<int32> BaseCalls; bool bMoved=false;
		const FVector Start=Batter->GetActorLocation();
		for (int32 Tick=0;Tick<2400;++Tick)
		{
			for (const auto& P : Traffic->PedestrianAgents) if (P.IsValid()) P->Tick(1.f/30);
			Clips.Add(Fielder->FigureMnemonic);
			if (Batter->BehaviorContext.Stack.ContainsByPredicate([](const auto& Frame){return Frame.ProgramId==1040;}))
				BaseCalls.Add(Batter->BehaviorContext.Stack[0].RecordIndex);
			bMoved |= FVector::Dist2D(Start,Batter->GetActorLocation())>100;
		}
		TestTrue(TEXT("Batter physically runs"),bMoved);
		TestTrue(TEXT("Original batter calls all four base-running legs"),
			BaseCalls.Contains(6) && BaseCalls.Contains(7) && BaseCalls.Contains(8) && BaseCalls.Contains(9));
		TestTrue(TEXT("Fielders use original hands-on-hips, waving and idle animations"),
			Clips.Contains(TEXT("HipH")) && Clips.Contains(TEXT("WvNo")) && Clips.Contains(TEXT("NoMo")));
		// Old saves contain the erroneous turn flag. Repair it without restarting the VM.
		Batter->BehaviorContext.Attributes[EBhavAttr::AutoTurn]=1;
		const auto Stack=Batter->BehaviorContext.Stack;
		TArray<uint8> Data;
		TestTrue(TEXT("Capture existing baseball save"),Batter->CaptureRuntimeSaveState(Data));
		TestTrue(TEXT("Restore existing baseball save"),Batter->RestoreRuntimeSaveState(Data));
		TestEqual(TEXT("Old baseball save restores original turn policy"),Batter->GetBehaviorAttribute(EBhavAttr::AutoTurn),0);
		TestEqual(TEXT("Save preserves baseball stack depth"),Batter->BehaviorContext.Stack.Num(),Stack.Num());
		for (int32 I=0;I<FMath::Min(Stack.Num(),Batter->BehaviorContext.Stack.Num());++I)
		{
			TestEqual(TEXT("Save preserves program"),Batter->BehaviorContext.Stack[I].ProgramId,Stack[I].ProgramId);
			TestEqual(TEXT("Save preserves program counter"),Batter->BehaviorContext.Stack[I].RecordIndex,Stack[I].RecordIndex);
		}
	}
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Game/SimCopterSessionSubsystem.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterUserCityPopulationTest, "SimCopter.Population.UserCities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FSimCopterUserCityPopulationTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	// Use the same discovery as Start User Game, so newly added maps join this regression
	// automatically. The optional directory checks a packaged installation's city browser too.
	TArray<FString> Paths;
	USimCopterSessionSubsystem::GetUserCityFilePaths(Paths);
	TMap<FString, FString> ByName;
	for (const auto& Path : Paths) ByName.Add(FPaths::GetCleanFilename(Path).ToLower(), Path);
	FString Directory;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimPopulationCities="), Directory))
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*.sc2")), true, false);
		for (const auto& File : Files) ByName.Add(File.ToLower(), Directory / File);
	}
	ByName.GetKeys(Paths); Paths.Sort();
	for (const auto& Name : Paths) { Names.Add(FPaths::GetBaseFilename(Name)); Commands.Add(ByName[Name]); }
}

bool FSimCopterUserCityPopulationTest::RunTest(const FString& Path)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->CityFile.FilePath = Path;
	if (!TestTrue(TEXT("User city loads through the normal spawn-data path"), Traffic->RebuildSpawnData())) return false;
	Traffic->bSimulateWholeMap = false; Traffic->MaxVehicleAgents = 0;
	for (int32 I = 0; I < Traffic->OriginalAmbientRandomCap; ++I)
	{
		auto* Crew = World->SpawnActor<ASimCopterGroundAgent>();
		Crew->InitialPersonState = I % 2 ? 7 : 5; // Posted police and medical crews.
		Traffic->PedestrianAgents.Add(Crew);
	}
	TestEqual(TEXT("Service crews leave the entire civilian budget available"), Traffic->CountAmbientPedestrians(), 0);
	TSet<FString> Figures;
	int32 EligibleTiles = 0;
	FVector Focus = FVector::ZeroVector;
	// Sample across the map; row-major sampling can fill all 55 slots in one rural
	// corner before ever reaching the city's different land-use/appearance classes.
	TArray<int32> Nodes;
	for (int32 I = 0; I < Traffic->PedestrianNodes.Num(); ++I) Nodes.Add(I);
	FRandomStream Sampling(314159);
	for (int32 I = Nodes.Num()-1; I > 0; --I) Nodes.Swap(I, Sampling.RandRange(0,I));
	for (int32 NodeIndex : Nodes)
	{
		const auto& Node = Traffic->PedestrianNodes[NodeIndex];
		if (!Traffic->IsOriginalAmbientTileGateOpen(Node.FileX, Node.FileY)) continue;
		++EligibleTiles; Focus = Node.Location;
		const int32 Before = Traffic->PedestrianAgents.Num();
		int32 Budget = 1; Traffic->TryRunAmbientTileSpawn(Node.FileX, Node.FileY, 1, Budget);
		for (int32 I = Before; I < Traffic->PedestrianAgents.Num(); ++I)
		{
			const auto* Person = Traffic->PedestrianAgents[I].Get();
			if (Person && Person->InitialPersonState == 0 && Person->InitialBehaviorProgramId == INDEX_NONE)
				Figures.Add(Person->GetPedestrianFigureName());
		}
		if (Traffic->CountAmbientPedestrians() >= Traffic->OriginalAmbientRandomCap) break;
	}
	if (EligibleTiles == 0)
	{
		AddInfo(TEXT("This map has no eligible civilian ground; no artificial population is required."));
		TestEqual(TEXT("No civilians are placed on unsuitable terrain"), Traffic->CountAmbientPedestrians(), 0);
		return true;
	}
	// Tiny/rural maps need not contain every urban class, but a civilian population
	// must not turn into one repeated police figure. Report the actual variety below.
	TestTrue(TEXT("Civilians have varied appearances alongside posted police and medics"), Figures.Num() >= FMath::Min(2, EligibleTiles));
	// The city-wide sample is intentionally spread over the entire map. Clear that
	// sample before testing a stationary LOCAL pool, or normal distance despawning
	// would confound refill with the removal of the distant samples.
	for (auto Person : Traffic->PedestrianAgents) if (Person.IsValid()) Person->Destroy();
	Traffic->PedestrianAgents.Empty();
	for (int32 I = 0; I < Traffic->OriginalAmbientRandomCap; ++I)
	{
		auto* Crew = World->SpawnActor<ASimCopterGroundAgent>(); Crew->InitialPersonState = I % 2 ? 7 : 5;
		Crew->SetActorLocation(Focus); Traffic->PedestrianAgents.Add(Crew);
	}
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>(); Heli->SetActorLocation(Focus + FVector(0,0,1000));
	auto* Controller = World->SpawnActor<APlayerController>(); Controller->Player = NewObject<ULocalPlayer>(GEngine);
	World->AddController(Controller); Controller->Possess(Heli);
	Traffic->UpdateAgentPool(1.f);
	const int32 BeforeRefill = Traffic->CountAmbientPedestrians();
	for (int32 I = 0; I < 8; ++I) Traffic->UpdateAgentPool(1.f);
	TestTrue(TEXT("Stationary user-city play replenishes ordinary pedestrians"), Traffic->CountAmbientPedestrians() > BeforeRefill || BeforeRefill >= Traffic->OriginalAmbientRandomCap);
	TestTrue(TEXT("Refill respects civilian population cap"), Traffic->CountAmbientPedestrians() <= Traffic->OriginalAmbientRandomCap);
	TestTrue(TEXT("Refill respects total actor cap"), Traffic->PedestrianAgents.Num() <= Traffic->MaxPedestrianAgents);
	const int32 RefilledPopulation = Traffic->CountAmbientPedestrians();
	Traffic->PruneAgentArray(Traffic->PedestrianAgents, Focus);
	TestEqual(TEXT("Refilled civilians remain inside the local population radius"), Traffic->CountAmbientPedestrians(), RefilledPopulation);
	AddInfo(FString::Printf(TEXT("%s: %d civilian appearances; civilian population %d -> %d with stationary refill"), *Path, Figures.Num(), BeforeRefill, Traffic->CountAmbientPedestrians()));
	Controller->UnPossess();
	return true;
}
#endif

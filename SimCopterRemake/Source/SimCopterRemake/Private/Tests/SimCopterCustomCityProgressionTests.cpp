#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Missions/SimCopterCustomCityProgression.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Game/SimCopterCursorIdle.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "UI/SimCopterControllerHelp.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "City/SimCity2000CityActor.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Engine/LocalPlayer.h"
#include "UObject/UnrealType.h"
#include "Serialization/MemoryReader.h"

using namespace SimCopterMissions;

namespace
{
struct FCapacityWorld : ISimCopterMissionWorld
{
	int32 Seats = 2;
	bool bCustom = false;
	SimCopterCustomCityProgression::FCapabilities Capabilities;
	virtual int32 GetXbldTileId(int32, int32) const override { return 0x81; }
	virtual int32 GetBuildingFootprintSize(int32, int32) const override { return 1; }
	virtual bool GetCameraTile(int32& X, int32& Y) const override { X = Y = 1; return true; }
	virtual bool GetPlayerTile(int32& X, int32& Y) const override { return GetCameraTile(X, Y); }
	virtual bool TrySpawnMissionPerson(int32, int32, int32, int32, int32) override { return true; }
	virtual int32 GetMissionPassengerLimit(bool bMedical) const override { return SimCopterCustomCityProgression::PassengerLimit(Seats, bMedical); }
	virtual bool UsesCitywideMissionPlacement() const override { return bCustom; }
	virtual bool IsMissionTypeAvailable(int32 Type) const override { return !bCustom || SimCopterCustomCityProgression::Allows(Type, Capabilities); }
	virtual bool TryPickCityMissionTile(int32, int32 Random, int32& X, int32& Y) override { X = 100 + Random % 20; Y = 100 + Random / 20 % 20; return true; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCustomCityRulesTest, "SimCopter.CustomCities.ProgressionAndSeatLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCustomCityRulesTest::RunTest(const FString&)
{
	using namespace SimCopterCustomCityProgression;
	for (int32 Score : {0, 499, 500, 1499, 1500, 2999, 3000})
	{
		FCapabilities C; C.Tier = TierForScore(Score);
		TestFalse(TEXT("Starter never receives riots even with a high score"), Allows(TYPE_Riot, C));
		C.bAdvancedAircraft = true; C.bTearGas = true; C.Seats = 4;
		TestEqual(TEXT("Riots need the final score stage as well as aircraft and gas"), Allows(TYPE_Riot, C), Score >= 3000);
		C.bTearGas = false; TestFalse(TEXT("Missing/empty gas prevents riots"), Allows(TYPE_Riot, C));
		C.bHarness = true;
		TestEqual(TEXT("Boat SOS unlocks after training"), Allows(TYPE_BoatRescue, C), Score >= 500);
		C.bHarness = false; TestFalse(TEXT("A missing harness prevents boat rescues"), Allows(TYPE_BoatRescue, C));
	}
	for (int32 Seats : {0, 1, 2, 4, 6, 14}) for (int32 Tier = 1; Tier <= 4; ++Tier) for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		FCapacityWorld World; World.Seats = Seats;
		FSimCopterMissionSystem System; System.Initialize(&World, Seed);
		FSimCopterCareerCity City; City.Difficulty = Tier - 1; System.SetCareerCity(City);
		const int32 Medical = System.CreateEventAt(20, 20, TYPE_Medevac);
		if (Seats <= 1) TestEqual(TEXT("No medical mission without patient plus medic space"), Medical, INDEX_NONE);
		else if (TestNotNull(TEXT("Medical mission creates"), System.FindRecord(Medical)))
			TestTrue(TEXT("All patients and one medic fit"), System.FindRecord(Medical)->MedevacVictims <= Seats - 1);
		const int32 Incident = System.CreateIncidentMedevacAt(21, 21);
		const int32 PlayerInjury = System.CreatePlayerCausedMedevacAt(22, 22);
		TestEqual(TEXT("Incident-created mission also needs a medic seat"), Incident != INDEX_NONE, Seats > 1);
		TestEqual(TEXT("Player-caused mission also needs a medic seat"), PlayerInjury != INDEX_NONE, Seats > 1);
		const int32 Roof = System.CreateEventAt(25, 25, TYPE_RooftopRescue);
		if (!Seats) TestEqual(TEXT("No zero-seat rescue"), Roof, INDEX_NONE);
		else if (TestNotNull(TEXT("Roof rescue creates"), System.FindRecord(Roof)))
			TestTrue(TEXT("Healthy victims use all seats"), System.FindRecord(Roof)->RescueVictims <= Seats);
		const int32 Transport = System.CreateEventAt(30, 30, TYPE_Transport);
		if (!Seats) TestEqual(TEXT("No zero-seat transport"), Transport, INDEX_NONE);
		else if (TestNotNull(TEXT("Transport creates"), System.FindRecord(Transport)))
			TestTrue(TEXT("Transport party fits"), System.FindRecord(Transport)->TransportPassengers <= Seats);
	}
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
		auto* ActualWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(ActualWorld);
		ON_SCOPE_EXIT { GEngine->DestroyWorldContext(ActualWorld); ActualWorld->DestroyWorld(false); };
		auto* PC = ActualWorld->SpawnActor<ASimCopterPlayerController>(); ActualWorld->AddController(PC);
		PC->Player = NewObject<ULocalPlayer>(GEngine); PC->ChangeState(NAME_Playing);
		auto* Aircraft = ActualWorld->SpawnActor<ASimCopterHelicopterPawn>(); PC->Possess(Aircraft);
		auto* Parked = ActualWorld->SpawnActor<ASimCopterHelicopterPawn>(); Parked->SwitchHelicopterModel(5);
		auto* Missions = ActualWorld->SpawnActor<ASimCopterMissionSystemActor>();
		for (int32 Type : {4, 0, 5})
		{
			TestTrue(TEXT("Real helicopter model loads"), Aircraft->SwitchHelicopterModel(Type));
			TestEqual(TEXT("Medical cap follows current helicopter, not another fleet member"),
				Missions->GetMissionPassengerLimit(true), FMath::Max(0, Aircraft->GetPassengerSeatCount() - 1));
		}
		auto* Pilot = ActualWorld->SpawnActor<ASimCopterOnFootPawn>(); Pilot->SetParkedHelicopter(Aircraft); PC->Possess(Pilot);
		TestEqual(TEXT("On-foot missions retain the current helicopter's medic reservation"),
			Missions->GetMissionPassengerLimit(true), Aircraft->GetPassengerSeatCount() - 1);
	}
	FCapacityWorld World; World.bCustom = true;
	FSimCopterMissionSystem System; System.Initialize(&World, 42);
	System.SetCareerCity(FSimCopterMissionSystem::MakeUserCityDefaults({}));
	System.SetCaptureCageOwned(true);
	const int32* Weights = System.GetCumulativeWeightTable();
	TestEqual(TEXT("Cage cannot bypass beginner crime lock"), Weights[2], Weights[1]);
	TestEqual(TEXT("Starting riot weight is actually excluded"), Weights[4], Weights[3]);
	TestEqual(TEXT("A directly scheduled locked riot is rejected"), System.CreateEventOfType(TYPE_Riot), INDEX_NONE);
	const int32 Medical = System.CreateEventOfType(TYPE_Medevac);
	if (TestNotNull(TEXT("A far-side citywide mission creates"), System.FindRecord(Medical)))
		TestTrue(TEXT("No camera/hangar range restriction"), System.FindRecord(Medical)->TileX >= 100 && System.FindRecord(Medical)->TileY >= 100);
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterCustomCitySitesTest, "SimCopter.CustomCities.InitialScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FSimCopterCustomCitySitesTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* City : {TEXT("arcocity"), TEXT("lakeland"), TEXT("la-isla")}) { Names.Add(City); Commands.Add(City); }
}
bool FSimCopterCustomCitySitesTest::RunTest(const FString& Name)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
	for (const TCHAR* Flag : {TEXT("bLoadOnConstruction"), TEXT("bLoadOnBeginPlay"), TEXT("bRenderProceduralMapExtension"), TEXT("bRenderStreetLightSpotLights")})
		FindFProperty<FBoolProperty>(City->GetClass(), Flag)->SetPropertyValue_InContainer(City, false);
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	const auto* CityFile = FindFProperty<FStructProperty>(City->GetClass(), TEXT("CityFile"));
	CityFile->ContainerPtrToValuePtr<FFilePath>(City)->FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame/cities") / (Name + TEXT(".sc2")));
	City->FinishSpawning(FTransform::Identity);
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(Missions, 47);
	Missions->SessionMode = ESimCopterMissionSessionMode::UserCityJobs;
	Missions->ScanCustomCityMissionSites();
	TestFalse(TEXT("Scan waits for city data rather than caching an empty city"), Missions->bCustomCitySitesScanned);
	City->RebuildCity();
	TestTrue(TEXT("Traffic placement data loads"), Traffic->RebuildSpawnData());
	Missions->ScanCustomCityMissionSites();
	TestTrue(TEXT("Whole city scan completes"), Missions->bCustomCitySitesScanned);
	TestFalse(TEXT("Mission buildings found"), Missions->CustomBuildingSites.IsEmpty());
	for (const FIntPoint& Tile : Missions->CustomWaterSites)
		for (int32 Y = -2; Y <= 2; ++Y) for (int32 X = -2; X <= 2; ++X)
			if (!Traffic->IsWaterTile(Tile.X + X, Tile.Y + Y)) { AddError(TEXT("Boat site overlaps a shore or small pond")); return false; }
	for (const FIntPoint& Tile : Missions->CustomRoofSites)
	{
		FVector Roof, Ground; float Extent;
		TestTrue(TEXT("Roof has a real rescue post"), Traffic->TryGetBuildingRoofPost(Tile.X, Tile.Y, Roof, Extent));
		Traffic->TryGetTileCenterWorldLocation(Tile.X, Tile.Y, Ground);
		TestTrue(TEXT("Roof is high above local ground"), Roof.Z - Ground.Z >= City->GetTileSize() * 1.5f);
	}
	AddInfo(FString::Printf(TEXT("%s: %d buildings, %d tall roofs, %d open-water tiles"), *Name,
		Missions->CustomBuildingSites.Num(), Missions->CustomRoofSites.Num(), Missions->CustomWaterSites.Num()));
	Missions->CustomCityBestScore = 3000;
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	auto* Aircraft = World->SpawnActor<ASimCopterHelicopterPawn>(); PC->Possess(Aircraft);
	Aircraft->SwitchHelicopterModel(4);
	Aircraft->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TearGas, true);
	TestFalse(TEXT("Actual starter remains ineligible for riots with gas"), Missions->IsMissionTypeAvailable(TYPE_Riot));
	Aircraft->SwitchHelicopterModel(0); // First catalog upgrade is registry type zero, not the starter.
	Aircraft->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TearGas, true);
	TestTrue(TEXT("Actual Jet Ranger with gas can receive late-stage riots"), Missions->IsMissionTypeAvailable(TYPE_Riot));
	Aircraft->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TearGas, false);
	TestFalse(TEXT("Removing actual gas equipment relocks riots"), Missions->IsMissionTypeAvailable(TYPE_Riot));
	TArray<uint8> Saved;
	TestTrue(TEXT("Version 2 mission save writes"), Missions->CaptureRuntimeSaveState(Saved));
	Missions->CustomCityBestScore = 0;
	TestTrue(TEXT("Version 2 mission save restores"), Missions->RestoreRuntimeSaveState(Saved));
	TestEqual(TEXT("Progress never relocks after save/load"), Missions->GetCustomCityProgressionTier(), 4);
	// Version 1 is the same byte layout without the appended best-score field.
	Saved.SetNum(Saved.Num() - sizeof(int32));
	int32 LegacyVersion = 1; FMemory::Memcpy(Saved.GetData() + sizeof(uint32), &LegacyVersion, sizeof(int32));
	TestTrue(TEXT("Existing v1 saves still load"), Missions->RestoreRuntimeSaveState(Saved));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPhotoHelpIdleRulesTest, "SimCopter.CustomCities.HelpAndCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPhotoHelpIdleRulesTest::RunTest(const FString&)
{
	TestFalse(TEXT("Cursor is visible before ten real seconds"), SimCopterCursorIdle::HasTimedOut(109.99, 100));
	TestTrue(TEXT("Cursor hides at ten real seconds"), SimCopterCursorIdle::HasTimedOut(110, 100));
	TestFalse(TEXT("Mouse activity resets timeout"), SimCopterCursorIdle::HasTimedOut(110, 110));
	SimCopterControllerHelp::FState State; State.bKeyboard = true;
	TestEqual(TEXT("Collapsed help stays in the minimap header without a floating shortcut row"), SimCopterControllerHelp::BuildPrompts(State).Num(), 0);
	State.bExpanded = true;
	TestTrue(TEXT("Shortcut expands keyboard instructions"), SimCopterControllerHelp::BuildPrompts(State).Num() > 1);
	return true;
}
#endif

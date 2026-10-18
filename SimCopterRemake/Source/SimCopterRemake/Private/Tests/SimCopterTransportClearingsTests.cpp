#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UnrealType.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "City/SimCity2000CityActor.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Game/SimCopterSessionSubsystem.h"
#include "Missions/SimCopterMissionSystemActor.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterTransportCityClearingsTest,
	"SimCopter.Transport.CityClearings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FSimCopterTransportCityClearingsTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame/cities"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files,*Root,TEXT("*.sc2"),true,false);
	Files.Sort();
	for (const FString& File : Files)
	{
		FString Relative=File;
		FPaths::MakePathRelativeTo(Relative,*(Root+TEXT("/")));
		Names.Add(FPaths::ChangeExtension(Relative,TEXT("")).Replace(TEXT("/"),TEXT(".")).Replace(TEXT(" "),TEXT("_")));
		Commands.Add(File);
	}
}

bool FSimCopterTransportCityClearingsTest::RunTest(const FString& Parameters)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); CollectGarbage(RF_NoFlags); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	// Configure the same editor-exposed properties a map uses, before construction.
	for (const TCHAR* Name : {TEXT("bLoadOnConstruction"),TEXT("bLoadOnBeginPlay"),
		TEXT("bRenderProceduralMapExtension"),TEXT("bRenderStreetLightSpotLights")})
	{
		auto* Property=FindFProperty<FBoolProperty>(City->GetClass(),Name);
		if (!TestNotNull(Name,Property)) return false;
		Property->SetPropertyValue_InContainer(City,false);
	}
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	auto* CityFile=FindFProperty<FStructProperty>(City->GetClass(),TEXT("CityFile"));
	if (!TestNotNull(TEXT("CityFile editor property"),CityFile)) return false;
	CityFile->ContainerPtrToValuePtr<FFilePath>(City)->FilePath=Parameters;
	City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	if (!TestTrue(TEXT("City placement data loads"),Traffic->RebuildSpawnData())) return false;
	TArray<FBox> Buildings;
	TArray<FVector> Anchors;
	for (int32 Y=0;Y<128;++Y) for (int32 X=0;X<128;++X)
	{
		FBox Bounds(ForceInit);
		if (City->TryGetBuildingBoundsAtTile(X,Y,Bounds)) Buildings.Add(Bounds);
		const int32 Id=Traffic->GetXbldTileId(X,Y);
		FVector Center;
		if (Id>0x6f && Id<0xdc && Traffic->TryGetTileCenterWorldLocation(X,Y,Center)) Anchors.Add(Center);
	}
	int32 Checked=0,Resolved=0;
	if (FPaths::GetBaseFilename(Parameters)==TEXT("city0"))
	{
		auto* Aircraft=World->SpawnActor<ASimCopterHelicopterPawn>();
		Aircraft->SwitchHelicopterModel(0);
		const FBox Bounds=Aircraft->GetParkingWorldBounds();
		AddInfo(FString::Printf(TEXT("Helper parking bounds: %s"),*Bounds.ToString()));
		const float X=FMath::Max(FMath::Abs(Bounds.Min.X),FMath::Abs(Bounds.Max.X));
		const float Y=FMath::Max(FMath::Abs(Bounds.Min.Y),FMath::Abs(Bounds.Max.Y));
		TestTrue(TEXT("Clearing fits helper rotor and tail at every heading, with margin"),FMath::Sqrt(X*X+Y*Y)+24<=275);
		Aircraft->Destroy();
	}
	const auto CheckAnchor = [&](const FVector& Anchor)
	{
		++Checked; FVector Clearing;
		if (!Traffic->TryFindTransportClearing(Anchor,Clearing,275,3200)) return;
		++Resolved;
		TestTrue(TEXT("Original neighbourhood retained"),FVector::Dist2D(Clearing,Anchor)<=3200.01f);
		float GroundZ=0; uint8 TerrainClass=0;
		TestTrue(TEXT("Actual terrain surface exists"),City->TryGetWaterGameplaySurface(Clearing,GroundZ,TerrainClass));
		TestTrue(TEXT("Drop point is dry ground, not a roof"),TerrainClass>=10 && FMath::Abs(Clearing.Z-GroundZ)<1);
		bool OverlapsBuilding=false;
		for (const FBox& Bounds : Buildings)
			if (Clearing.X+275>=Bounds.Min.X && Clearing.X-275<=Bounds.Max.X &&
				Clearing.Y+275>=Bounds.Min.Y && Clearing.Y-275<=Bounds.Max.Y) { OverlapsBuilding=true; break; }
		TestFalse(TEXT("Whole helicopter footprint lies outside every rendered building"),OverlapsBuilding);
		FVector Again;
		TestTrue(TEXT("Saved destination does not drift on reload"),Traffic->TryFindTransportClearing(Clearing,Again,275,3200) && Again.Equals(Clearing,0.1));
	};
	// Distribute probes through every city's original mission-building pool. If the
	// sample misses every clearing, exhaust the pool before declaring the city unusable.
	const int32 Stride=FMath::Max(1,Anchors.Num()/24);
	for (int32 Index=0;Index<Anchors.Num();Index+=Stride) CheckAnchor(Anchors[Index]);
	if (Resolved==0)
	{
		for (int32 Index=0;Index<Anchors.Num() && Resolved==0;++Index)
			if (Index%Stride!=0) CheckAnchor(Anchors[Index]);
	}
	AddInfo(FString::Printf(TEXT("%s: %d/%d sampled original mission anchors have a nearby clearing"),*FPaths::GetBaseFilename(Parameters),Resolved,Checked));
	TestTrue(TEXT("City still has playable transport destinations"),Resolved>0);
	return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterUserCityTransportCreationTest,
	"SimCopter.Transport.UserCityCreation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
// Observe the production placement boundary without replacing city geometry or passenger spawning.
struct FUserTransportProbe : SimCopterMissions::ISimCopterMissionWorld
{
	ASimCopterMissionSystemActor* Missions = nullptr;
	int32 ClearingCalls = 0, ClearingFailures = 0, SpawnCalls = 0, SpawnFailures = 0;
	virtual bool UsesCitywideMissionPlacement() const override { return true; }
	virtual bool IsMissionTypeAvailable(int32 Type) const override { return Missions->IsMissionTypeAvailable(Type); }
	virtual int32 GetMissionPassengerLimit(bool bMedical) const override { return Missions->GetMissionPassengerLimit(bMedical); }
	virtual int32 GetXbldTileId(int32 X, int32 Y) const override { return Missions->GetXbldTileId(X, Y); }
	virtual int32 GetBuildingFootprintSize(int32 X, int32 Y) const override { return Missions->GetBuildingFootprintSize(X, Y); }
	virtual bool GetCameraTile(int32& X, int32& Y) const override { return Missions->GetCameraTile(X, Y); }
	virtual bool GetPlayerTile(int32& X, int32& Y) const override { return Missions->GetPlayerTile(X, Y); }
	virtual bool TryPickCityMissionTile(int32 Type, int32 Random, int32& X, int32& Y) override
	{ return Missions->TryPickCityMissionTile(Type, Random, X, Y); }
	virtual bool TryResolveTransportSpawnTile(int32 X, int32 Y, int32& OutX, int32& OutY) override
	{
		++ClearingCalls;
		const bool bResolved = Missions->TryResolveTransportSpawnTile(X, Y, OutX, OutY);
		if (!bResolved) ++ClearingFailures;
		return bResolved;
	}
	virtual bool TrySpawnMissionPerson(int32 State, int32 Behavior, int32 X, int32 Y, int32 Event) override
	{
		++SpawnCalls;
		const bool bSpawned = Missions->TrySpawnMissionPerson(State, Behavior, X, Y, Event);
		if (!bSpawned) ++SpawnFailures;
		return bSpawned;
	}
};
}

void FSimCopterUserCityTransportCreationTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	TArray<FString> Paths;
	USimCopterSessionSubsystem::GetUserCityFilePaths(Paths);
	TMap<FString, FString> ByName;
	for (const auto& Path : Paths) ByName.Add(FPaths::GetCleanFilename(Path).ToLower(), Path);
	FString Directory;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimTransportCities="), Directory))
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*.sc2")), true, false);
		for (const auto& File : Files) ByName.Add(File.ToLower(), Directory / File);
	}
	ByName.GetKeys(Paths); Paths.Sort();
	for (const auto& Name : Paths) { Names.Add(FPaths::GetBaseFilename(Name)); Commands.Add(ByName[Name]); }
}

bool FSimCopterUserCityTransportCreationTest::RunTest(const FString& Path)
{
	using namespace SimCopterMissions;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); CollectGarbage(RF_NoFlags); };
	auto* City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
	for (const TCHAR* Flag : {TEXT("bLoadOnConstruction"), TEXT("bLoadOnBeginPlay"),
		TEXT("bRenderProceduralMapExtension"), TEXT("bRenderStreetLightSpotLights")})
		FindFProperty<FBoolProperty>(City->GetClass(), Flag)->SetPropertyValue_InContainer(City, false);
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	FindFProperty<FStructProperty>(City->GetClass(), TEXT("CityFile"))->ContainerPtrToValuePtr<FFilePath>(City)->FilePath = Path;
	City->FinishSpawning(FTransform::Identity);
	City->RebuildCity();
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	if (!TestTrue(TEXT("Real city placement data loads"), Traffic->RebuildSpawnData())) return false;
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->SessionMode = ESimCopterMissionSessionMode::UserCityJobs;
	FUserTransportProbe Probe;
	Probe.Missions = Missions;
	Missions->MissionSystem.Initialize(&Probe, 47);
	const double ScanStart = FPlatformTime::Seconds();
	Missions->ScanCustomCityMissionSites();
	AddInfo(FString::Printf(TEXT("City scan: %.2f ms, %d transport clearings"),
		(FPlatformTime::Seconds() - ScanStart) * 1000, Missions->CustomTransportSites.Num()));
	TestTrue(TEXT("Beginner transport is available"), Missions->IsMissionTypeAvailable(TYPE_Transport));
	// Sample twelve placements while respecting the three-event workload limit. Retire the
	// oldest fare before requesting another; keep real city geometry and passenger spawning.
	int32 Created = 0;
	TArray<int32> Pending;
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		if (Pending.Num()==3)
		{
			TestEqual(TEXT("Fourth simultaneous fare is refused on this map"),Missions->MissionSystem.CreateEventOfType(TYPE_Transport),INDEX_NONE);
			const int32 Retired=Pending[0];Pending.RemoveAt(0);
			Missions->MissionSystem.PostEvent(EVT_SetCategory,Retired,CAT_ExpireSilently);
			Missions->MissionSystem.Tick(0);
			for (TActorIterator<ASimCopterGroundAgent> It(World); It; ++It)
				if (It->MissionEventId==Retired) It->Destroy();
		}
		const int32 Event = Missions->MissionSystem.CreateEventOfType(TYPE_Transport);
		const auto* Record = Missions->MissionSystem.FindRecord(Event);
		if (!Record) continue;
		Pending.Add(Event);
		TestTrue(TEXT("User map never exceeds three ongoing jobs"),Missions->MissionSystem.GetOngoingEventCount()<=3);
		++Created;
		TestTrue(TEXT("A passenger actually spawned"), Record->TransportPassengers > 0);
		TestTrue(TEXT("Starter cabin capacity is respected"), Record->TransportPassengers <= 2);
		TestTrue(TEXT("Pickup and drop-off differ"), Record->TileX != Record->TertiaryX || Record->TileY != Record->TertiaryY);
		int32 People = 0;
		for (TActorIterator<ASimCopterGroundAgent> It(World); It; ++It)
			if (It->MissionEventId == Event && It->InitialPersonState == 4) ++People;
		TestEqual(TEXT("Record count matches real passenger actors"), People, Record->TransportPassengers);
	}
	AddInfo(FString::Printf(TEXT("User transport %s: %d/12 creation requests succeeded"), *FPaths::GetBaseFilename(Path), Created));
	AddInfo(FString::Printf(TEXT("Placement boundaries: %d/%d clearings failed; %d/%d passenger spawns failed"),
		Probe.ClearingFailures, Probe.ClearingCalls, Probe.SpawnFailures, Probe.SpawnCalls));
	TestEqual(TEXT("Every sampled transport request creates on this User map"), Created, 12);
	return true;
}
#endif


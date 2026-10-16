#include "Missions/SimCopterCustomCityProgression.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Formats/SimCopterPeopleCityRules.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "City/SimCity2000CityActor.h"
#include "Game/SimCopterCareerSubsystem.h"

using namespace SimCopterMissions;

bool ASimCopterMissionSystemActor::UsesCitywideMissionPlacement() const
{
	return SessionMode == ESimCopterMissionSessionMode::UserCityJobs;
}

int32 ASimCopterMissionSystemActor::GetMissionPassengerLimit(bool bMedical) const
{
	const auto* Aircraft = SimCopterHelicopterParking::ResolveCurrentAircraft(this);
	// Until possession/airport setup is complete, use the starter cabin rather than an unlimited party.
	return SimCopterCustomCityProgression::PassengerLimit(Aircraft ? Aircraft->GetPassengerSeatCount() : 2, bMedical);
}

int32 ASimCopterMissionSystemActor::GetCustomCityProgressionTier() const
{
	return SimCopterCustomCityProgression::TierForScore(CustomCityBestScore);
}

FString ASimCopterMissionSystemActor::GetCustomCityProgressionText() const
{
	const int32 Tier = GetCustomCityProgressionTier();
	const TCHAR* Names[] = {TEXT("Local response"), TEXT("Rescue response"), TEXT("Advanced response"), TEXT("Major incidents")};
	const int32 Goals[] = {500, 1500, 3000};
	FString Text = FString::Printf(TEXT("Custom city: %s (%d/4)."), Names[Tier - 1], Tier);
	if (Tier < 4) Text += FString::Printf(TEXT(" Next stage: %d points."), Goals[Tier - 1]);
	return Text + TEXT(" Roof/boat rescues need a harness; fires need water equipment; riots need an upgraded helicopter and tear gas.");
}

bool ASimCopterMissionSystemActor::IsMissionTypeAvailable(int32 TypeMask) const
{
	if (!UsesCitywideMissionPlacement()) return true;
	if (!bCustomCitySitesScanned) return false;
	SimCopterCustomCityProgression::FCapabilities C;
	C.Tier = GetCustomCityProgressionTier();
	if (const auto* Aircraft = SimCopterHelicopterParking::ResolveCurrentAircraft(this))
	{
		C.Seats = Aircraft->GetPassengerSeatCount();
		C.bAdvancedAircraft = Aircraft->GetHelicopterTypeIndex() != USimCopterCareerSubsystem::StartingHelicopterTypeIndex &&
			!Aircraft->IsFixedWingAircraft() && C.Seats >= 3;
		C.bHarness = Aircraft->IsToolAvailable(ESimCopterHelicopterTool::RescueHarness);
		C.bFireEquipment = Aircraft->IsToolAvailable(ESimCopterHelicopterTool::WaterBucket) || Aircraft->IsToolAvailable(ESimCopterHelicopterTool::WaterCannon);
		C.bTearGas = Aircraft->IsToolAvailable(ESimCopterHelicopterTool::TearGas) && Aircraft->GetEquipmentState().CareerTearGasRounds > 0;
	}
	if (!SimCopterCustomCityProgression::Allows(TypeMask, C)) return false;
	if (TypeMask == TYPE_RooftopRescue) return !CustomRoofSites.IsEmpty();
	if (TypeMask == TYPE_BoatRescue) return !CustomWaterSites.IsEmpty();
	if (TypeMask == TYPE_Riot || TypeMask == TYPE_Burglar || TypeMask == TYPE_TrafficJam) return !CustomRoadSites.IsEmpty();
	if (TypeMask == TYPE_BuildingFire) return !CustomFireSites.IsEmpty();
	if (TypeMask == TYPE_Transport) return CustomTransportSites.Num() >= 2;
	if (TypeMask == TYPE_Medevac || TypeMask == TYPE_Robber || TypeMask == TYPE_Mugger || TypeMask == TYPE_Arsonist)
		return !CustomBuildingSites.IsEmpty();
	return true;
}

void ASimCopterMissionSystemActor::ScanCustomCityMissionSites()
{
	if (!UsesCitywideMissionPlacement() || bCustomCitySitesScanned) return;
	auto* Traffic = ResolveTrafficSystem();
	FVector Probe;
	if (!Traffic || !Traffic->HasMissionPlacementData() || !Traffic->GetCityActor() || !Traffic->TryGetTileCenterWorldLocation(0, 0, Probe)) return;
	CustomBuildingSites.Reset(); CustomFireSites.Reset(); CustomRoadSites.Reset(); CustomRoofSites.Reset(); CustomWaterSites.Reset();
	CustomTransportSites.Reset();
	TSet<FIntPoint> RoofCenters;
	// One initial scan of all 128x128 tiles. No camera, airport or player-distance test.
	for (int32 Y = 0; Y < 128; ++Y) for (int32 X = 0; X < 128; ++X)
	{
		const FIntPoint Tile(X, Y);
		const int32 Id = Traffic->GetXbldTileId(X, Y);
		if (FSimCopterMissionSystem::IsFireSuitableTile(Id)) CustomFireSites.Add(Tile);
		if (FSimCopterPeopleCityRules::IsRoadTileClass(FSimCopterPeopleCityRules::GetTileClassForBuildingId(static_cast<uint8>(Id))))
			CustomRoadSites.Add(Tile);
		if (FSimCopterMissionSystem::IsMissionBuildingTile(Id))
		{
			CustomBuildingSites.Add(Tile);
			if ((FSimCopterMissionSystem::GetXbldPropertyFlags(Id) & 4) != 0)
			{
				FVector Roof, Ground; float Extent = 0;
				if (Traffic->TryGetBuildingRoofPost(X, Y, Roof, Extent) && Traffic->TryGetTileCenterWorldLocation(X, Y, Ground) &&
					Roof.Z - Ground.Z >= Traffic->GetCityActor()->GetTileSize() * 1.5f && Extent >= 100)
				{
					const FIntPoint Center(FMath::RoundToInt(Roof.X), FMath::RoundToInt(Roof.Y));
					if (!RoofCenters.Contains(Center)) { RoofCenters.Add(Center); CustomRoofSites.Add(Tile); }
				}
			}
		}
		// A five-tile-wide body, with a two-tile shore margin: ponds, thin canals and edges fail.
		if (X < 2 || X > 125 || Y < 2 || Y > 125 || !Traffic->IsWaterTile(X, Y)) continue;
		bool bOpenWater = true;
		for (int32 DY = -2; DY <= 2 && bOpenWater; ++DY) for (int32 DX = -2; DX <= 2; ++DX)
			if (!Traffic->IsWaterTile(X + DX, Y + DY)) { bOpenWater = false; break; }
		if (bOpenWater) CustomWaterSites.Add(Tile);
	}
	// User-city adaptation of FUN_004a7a10's two building endpoints: the remake requires
	// helicopter clearings near both. Drawing two unfiltered buildings made success the
	// product of two small probabilities on dense/island maps (including Heven and Hawaii).
	// Test each nearby ground tile once at load, rather than repeatedly searching the same
	// blocked neighbourhoods during play. Keep the existing 275 cm footprint / 3200 cm range.
	constexpr float TransportRangeCm = 3200;
	const float TileSize = Traffic->GetCityActor()->GetTileSize();
	const int32 Range = FMath::CeilToInt(TransportRangeCm / TileSize);
	TBitArray<> NearBuildings(false, 128 * 128);
	for (const FIntPoint& Building : CustomBuildingSites)
		for (int32 DY = -Range; DY <= Range; ++DY) for (int32 DX = -Range; DX <= Range; ++DX)
		{
			const FIntPoint Tile = Building + FIntPoint(DX, DY);
			if (Tile.X >= 0 && Tile.X < 128 && Tile.Y >= 0 && Tile.Y < 128 &&
				(DX * DX + DY * DY) * TileSize * TileSize <= TransportRangeCm * TransportRangeCm)
				NearBuildings[Tile.Y * 128 + Tile.X] = true;
		}
	for (int32 Y = 0; Y < 128; ++Y) for (int32 X = 0; X < 128; ++X)
	{
		if (!NearBuildings[Y * 128 + X]) continue;
		FVector Surface; uint8 TerrainClass; float SurfaceZ;
		if (Traffic->TryGetTileCenterWorldLocation(X, Y, Surface) &&
			Traffic->GetCityActor()->TryGetWaterGameplaySurface(Surface, SurfaceZ, TerrainClass))
		{
			Surface.Z = SurfaceZ;
			if (Traffic->IsTransportClearing(Surface, 275)) CustomTransportSites.Add(FIntPoint(X, Y));
		}
	}
	bCustomCitySitesScanned = true;
	UE_LOG(LogTemp, Display, TEXT("Custom city mission scan: %d buildings, %d roofs, %d open-water sites, %d road tiles, %d transport clearings."),
		CustomBuildingSites.Num(), CustomRoofSites.Num(), CustomWaterSites.Num(), CustomRoadSites.Num(), CustomTransportSites.Num());
}

bool ASimCopterMissionSystemActor::TryPickCityMissionTile(int32 TypeMask, int32 Random, int32& X, int32& Y)
{
	ScanCustomCityMissionSites();
	TArray<FIntPoint>* Sites = &CustomBuildingSites;
	if (TypeMask == TYPE_RooftopRescue) Sites = &CustomRoofSites;
	else if (TypeMask == TYPE_BoatRescue) Sites = &CustomWaterSites;
	else if (TypeMask == TYPE_BuildingFire) Sites = &CustomFireSites;
	else if (TypeMask == TYPE_Riot || TypeMask == TYPE_Burglar) Sites = &CustomRoadSites;
	else if (TypeMask == TYPE_Transport) Sites = &CustomTransportSites;
	while (!Sites->IsEmpty())
	{
		const int32 Index = static_cast<uint32>(Random) % Sites->Num();
		const FIntPoint Tile = (*Sites)[Index];
		const int32 Id = GetXbldTileId(Tile.X, Tile.Y);
		bool bValid = true;
		if (Sites == &CustomBuildingSites || Sites == &CustomRoofSites) bValid = FSimCopterMissionSystem::IsMissionBuildingTile(Id);
		if (Sites == &CustomRoofSites) bValid &= (FSimCopterMissionSystem::GetXbldPropertyFlags(Id) & 4) != 0;
		if (Sites == &CustomFireSites) bValid = FSimCopterMissionSystem::IsFireSuitableTile(Id);
		if (Sites == &CustomRoadSites) bValid = FSimCopterPeopleCityRules::IsRoadTileClass(FSimCopterPeopleCityRules::GetTileClassForBuildingId(static_cast<uint8>(Id)));
		if (Sites == &CustomTransportSites)
		{
			auto* Traffic = ResolveTrafficSystem();
			FVector Surface; uint8 TerrainClass; float SurfaceZ;
			bValid = Traffic && Traffic->GetCityActor() && Traffic->TryGetTileCenterWorldLocation(Tile.X, Tile.Y, Surface) &&
				Traffic->GetCityActor()->TryGetWaterGameplaySurface(Surface, SurfaceZ, TerrainClass);
			if (bValid) { Surface.Z = SurfaceZ; bValid = Traffic->IsTransportClearing(Surface, 275); }
		}
		if (!bValid) { Sites->RemoveAtSwap(Index); continue; } // demolition since load
		// Keep separate jobs off an occupied rescue site.
		if (Sites == &CustomRoofSites || Sites == &CustomWaterSites)
			for (const auto& Record : MissionSystem.GetRecords())
				if (Record.bActive && Record.TypeMask == TypeMask && FMath::Abs(Record.TileX - Tile.X) <= 2 && FMath::Abs(Record.TileY - Tile.Y) <= 2)
					return false;
		X = Tile.X; Y = Tile.Y;
		return true;
	}
	return false;
}

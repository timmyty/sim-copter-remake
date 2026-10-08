#pragma once

#include "CoreMinimal.h"

class ASimCopterHangar;
class ASimCopterHelicopterPawn;
class ASimCopterTrafficSystemActor;
class USimCopterCareerSubsystem;

namespace SimCopterHelicopterParking
{
SIMCOPTERREMAKE_API ASimCopterHelicopterPawn* ResolveCurrentAircraft(const UObject* WorldContext);
// SCHOOK: FUN_0047c0c0 case 0xe7, first F-15 tile in scene order.
// The five late-game maps (City25..29) retain their original hidden spawn points.
constexpr int32 FirstApacheEncounterCityIndex = 25;
constexpr int32 LastApacheEncounterCityIndex = 29;
SIMCOPTERREMAKE_API bool IsApacheEncounterCity(int32 CityIndex);
SIMCOPTERREMAKE_API FIntPoint FindApacheSpawnTile(TFunctionRef<int32(int32, int32)> GetTile);
SIMCOPTERREMAKE_API bool TryGetApacheSpawnSurface(const ASimCopterTrafficSystemActor* Traffic, FVector& OutSurface);
SIMCOPTERREMAKE_API void EnsureApacheEncounter(ASimCopterTrafficSystemActor* Traffic,
	ASimCopterHelicopterPawn* Existing, USimCopterCareerSubsystem* Career);
struct FPad
{
	int32 Index = INDEX_NONE;
	FVector Surface = FVector::ZeroVector;
};

// The eight edge-adjacent tiles around the 2x2 hangar, excluding the four airport corners.
SIMCOPTERREMAKE_API bool IsHangarPad(int32 PadIndex);
SIMCOPTERREMAKE_API void SortByDoorDistance(TArray<FPad>& Pads, const FVector& Door);
SIMCOPTERREMAKE_API bool OverlapsParkedAircraft(const FBox& Candidate, const TArray<FBox>& Occupants);

// Creates a separate, unpossessed airframe. No money/ownership changes on failure.
SIMCOPTERREMAKE_API ASimCopterHelicopterPawn* SpawnOnFreePad(
	ASimCopterHangar* Hangar, ASimCopterHelicopterPawn* Existing, int32 TypeIndex, FString& OutError);
}

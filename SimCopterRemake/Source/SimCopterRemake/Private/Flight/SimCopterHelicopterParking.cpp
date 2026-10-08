#include "Flight/SimCopterHelicopterParking.h"

#include "City/SimCopterAirport.h"
#include "City/SimCopterHangar.h"
#include "Engine/World.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Formats/SimCity2000Reader.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Kismet/GameplayStatics.h"

namespace SimCopterHelicopterParking
{
ASimCopterHelicopterPawn* ResolveCurrentAircraft(const UObject* WorldContext)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(WorldContext, 0);
	if (ASimCopterHelicopterPawn* Possessed = Cast<ASimCopterHelicopterPawn>(Player)) return Possessed;
	// ExitHelicopter makes the on-foot pawn owned by the aircraft just exited.
	if (Player != nullptr)
	{
		if (ASimCopterHelicopterPawn* Previous = Cast<ASimCopterHelicopterPawn>(Player->GetOwner()); IsValid(Previous)) return Previous;
	}
	TArray<AActor*> Aircraft;
	UGameplayStatics::GetAllActorsOfClass(WorldContext, ASimCopterHelicopterPawn::StaticClass(), Aircraft);
	Aircraft.RemoveAll([](AActor* Actor) { return CastChecked<ASimCopterHelicopterPawn>(Actor)->IsSupportAircraft(); });
	Aircraft.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	return Aircraft.IsEmpty() ? nullptr : CastChecked<ASimCopterHelicopterPawn>(Aircraft[0]);
}

bool IsApacheEncounterCity(int32 CityIndex)
{
	return CityIndex >= FirstApacheEncounterCityIndex && CityIndex <= LastApacheEncounterCityIndex;
}

FIntPoint FindApacheSpawnTile(TFunctionRef<int32(int32, int32)> GetTile)
{
	for (int32 Y = 0; Y < FSimCity2000City::MapSize; ++Y)
		for (int32 X = 0; X < FSimCity2000City::MapSize; ++X)
			if (GetTile(X, Y) == 0xe7) return FIntPoint(X, Y);
	return FIntPoint(INDEX_NONE, INDEX_NONE);
}

bool TryGetApacheSpawnSurface(const ASimCopterTrafficSystemActor* Traffic, FVector& OutSurface)
{
	if (!Traffic) return false;
	const auto* Missions = Cast<ASimCopterMissionSystemActor>(
		UGameplayStatics::GetActorOfClass(Traffic, ASimCopterMissionSystemActor::StaticClass()));
	if (!Missions || Missions->GetSessionMode() == ESimCopterMissionSessionMode::UserCityJobs ||
		!IsApacheEncounterCity(Missions->GetSessionCareerCityIndex())) return false;
	const FIntPoint Tile = FindApacheSpawnTile([Traffic](int32 X, int32 Y) { return Traffic->GetXbldTileId(X, Y); });
	return Tile.X != INDEX_NONE && Traffic->TryGetTileCenterWorldLocation(Tile.X, Tile.Y, OutSurface);
}

void EnsureApacheEncounter(ASimCopterTrafficSystemActor* Traffic,
	ASimCopterHelicopterPawn* Existing, USimCopterCareerSubsystem* Career)
{
	FVector Surface;
	if (!Career || Career->HasSpawnedApacheEncounter() || !TryGetApacheSpawnSurface(Traffic, Surface)) return;
	UWorld* World = Traffic->GetWorld();
	if (!World) return;
	TArray<AActor*> Aircraft;
	UGameplayStatics::GetAllActorsOfClass(World, ASimCopterHelicopterPawn::StaticClass(), Aircraft);
	for (AActor* Actor : Aircraft)
		if (CastChecked<ASimCopterHelicopterPawn>(Actor)->IsApacheHelicopter() &&
			!CastChecked<ASimCopterHelicopterPawn>(Actor)->IsSupportAircraft())
		{
			Career->SetApacheEncounterSpawned(true);
			return; // Includes old saves that already contain an Apache.
		}
	// SCHOOK: FUN_0047a240 places runtime type 2 at DAT_005d91d8/dc.
	// Discovery keeps that free world aircraft; ownership is claimed when boarding,
	// so the requested paid hangar delivery remains an alternative to finding it.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	auto* Apache = World->SpawnActor<ASimCopterHelicopterPawn>(
		Existing ? Existing->GetClass() : ASimCopterHelicopterPawn::StaticClass(), Surface, FRotator::ZeroRotator, Params);
	if (!Apache) return;
	Apache->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	Apache->AutoPossessAI = EAutoPossessAI::Disabled;
	Apache->FinishSpawning(Apache->GetActorTransform());
	if (!Apache->SwitchHelicopterModel(2)) { Apache->Destroy(); return; }
	Apache->ResetAircraft();
	Apache->PlaceOnHelipad(Surface, 0.0f);
	Career->SetApacheEncounterSpawned(true);
}

bool IsHangarPad(const int32 PadIndex)
{
	const FIntPoint Tile = SimCopterAirport::GetPadTile(FIntPoint::ZeroValue, PadIndex);
	return ((Tile.X == 0 || Tile.X == 3) && (Tile.Y == 1 || Tile.Y == 2)) ||
		((Tile.Y == 0 || Tile.Y == 3) && (Tile.X == 1 || Tile.X == 2));
}

void SortByDoorDistance(TArray<FPad>& Pads, const FVector& Door)
{
	Pads.Sort([&Door](const FPad& A, const FPad& B)
	{
		const double DA = FVector::DistSquared2D(A.Surface, Door);
		const double DB = FVector::DistSquared2D(B.Surface, Door);
		return DA == DB ? A.Index < B.Index : DA < DB;
	});
}

bool OverlapsParkedAircraft(const FBox& Candidate, const TArray<FBox>& Occupants)
{
	if (!Candidate.IsValid) return true;
	for (const FBox& Occupant : Occupants)
	{
		// Leave a small gap, and project onto the apron: different rotor heights must not
		// allow one parked helicopter's blades to extend through another's footprint.
		if (!Occupant.IsValid || Candidate.ExpandBy(10.0f).IntersectXY(Occupant)) return true;
	}
	return false;
}

ASimCopterHelicopterPawn* SpawnOnFreePad(
	ASimCopterHangar* Hangar, ASimCopterHelicopterPawn* Existing, const int32 TypeIndex, FString& OutError)
{
	UWorld* World = Hangar != nullptr ? Hangar->GetWorld() : nullptr;
	ASimCopterTrafficSystemActor* Traffic = World != nullptr ? Cast<ASimCopterTrafficSystemActor>(
		UGameplayStatics::GetActorOfClass(World, ASimCopterTrafficSystemActor::StaticClass())) : nullptr;
	if (Traffic == nullptr)
	{
		OutError = TEXT("The airport pads are unavailable.");
		return nullptr;
	}

	TArray<FPad> Pads;
	for (int32 Index = 0; Index < SimCopterAirport::PadCount; ++Index)
	{
		FVector Surface;
		if (IsHangarPad(Index) && Traffic->TryGetAirportPadWorldLocation(Index, Surface))
		{
			Pads.Add({Index, Surface});
		}
	}
	SortByDoorDistance(Pads, Hangar->GetDoorWorldLocation());
	double TileSize = TNumericLimits<double>::Max();
	for (int32 A = 0; A < Pads.Num(); ++A)
	{
		for (int32 B = A + 1; B < Pads.Num(); ++B)
		{
			TileSize = FMath::Min(TileSize, FVector::Dist2D(Pads[A].Surface, Pads[B].Surface));
		}
	}

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, ASimCopterHelicopterPawn::StaticClass(), Actors);
	Actors.RemoveAll([](AActor* Actor) { return CastChecked<ASimCopterHelicopterPawn>(Actor)->IsSupportAircraft(); });
	TArray<FBox> Occupants;
	for (AActor* Actor : Actors)
	{
		const ASimCopterHelicopterPawn* Aircraft = CastChecked<ASimCopterHelicopterPawn>(Actor);
		Occupants.Add(Aircraft->GetParkingWorldBounds());
	}

	// SCHOOK: BuyHelicopter 0x0048b1a0 / PlaceHelicopter 0x00484790. The original creates
	// a separate airframe on a free pad. Remake preference: keep possession and prefer the door.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ASimCopterHelicopterPawn* NewAircraft = World->SpawnActor<ASimCopterHelicopterPawn>(
		Existing != nullptr ? Existing->GetClass() : ASimCopterHelicopterPawn::StaticClass(),
		Hangar->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (NewAircraft == nullptr)
	{
		OutError = TEXT("Could not create the helicopter.");
		return nullptr;
	}
	NewAircraft->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	NewAircraft->AutoPossessAI = EAutoPossessAI::Disabled;
	NewAircraft->SetActorHiddenInGame(true);
	NewAircraft->SetActorEnableCollision(false);
	NewAircraft->FinishSpawning(NewAircraft->GetActorTransform());
	if (!NewAircraft->SwitchHelicopterModel(TypeIndex))
	{
		OutError = NewAircraft->GetLastModelSwitchStatus();
		NewAircraft->Destroy();
		return nullptr;
	}
	NewAircraft->ResetAircraft();
	for (const FPad& Pad : Pads)
	{
		bool bTileOccupied = false;
		for (const AActor* Actor : Actors)
		{
			const FVector Delta = Actor->GetActorLocation() - Pad.Surface;
			if (FMath::Abs(FVector::DotProduct(Delta, Hangar->GetActorForwardVector())) <= TileSize * 0.5 &&
				FMath::Abs(FVector::DotProduct(Delta, Hangar->GetActorRightVector())) <= TileSize * 0.5)
			{
				bTileOccupied = true;
				break;
			}
		}
		if (bTileOccupied) continue;
		NewAircraft->PlaceOnHelipad(Pad.Surface, 0.0f);
		if (!OverlapsParkedAircraft(NewAircraft->GetParkingWorldBounds(), Occupants))
		{
			NewAircraft->SetActorEnableCollision(true);
			NewAircraft->SetActorHiddenInGame(false);
			return NewAircraft;
		}
	}
	NewAircraft->Destroy();
	OutError = TEXT("No clear helicopter pad is available near the hangar. Move a parked helicopter first.");
	return nullptr;
}
}

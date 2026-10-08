#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

// Requested remake service points. Both floors retain the original state-5/state-7 programs.
bool ASimCopterTrafficSystemActor::IsResponderSpawnClear(const FVector& Surface, const ASimCopterGroundAgent* Ignore) const
{
	for (const auto Weak : PedestrianAgents)
	{
		const auto* Person = Weak.Get();
		if (!Person || Person == Ignore || Person->IsActorBeingDestroyed() || Person->IsHidden() ||
			Person->GetBehaviorCarrier() || Person->IsMissionCarried()) continue;
		const FVector Feet = Person->GetActorLocation() - FVector(0, 0, Person->GetCapsuleHalfHeightCm());
		if (FMath::Abs(Feet.Z - Surface.Z) < 150 &&
			FVector::DistSquared2D(Feet, Surface) < FMath::Square(FMath::Max(90.0f, Person->GetCollisionRadiusCm() + 55.0f)))
			return false;
	}
	if (GetWorld()) for (TActorIterator<ASimCopterHelicopterPawn> It(GetWorld()); It; ++It)
		if (It->GetDistanceToAirframeCm(Surface + FVector(0, 0, 50), false) < 65) return false;
	return true;
}

bool ASimCopterTrafficSystemActor::TryGetBuildingEntrancePost(int32 TileX, int32 TileY, FVector& OutSurface)
{
	int32 NodeIndex;
	if (!GetWorld() || !TryResolvePedestrianNodeForTile(TileX, TileY, NodeIndex)) return false;
	const auto& Node = PedestrianNodes[NodeIndex];
	if (Node.BuildingId != 0xD1 && Node.BuildingId != 0xD2) return false;
	if (uint8(GetXbldTileId(Node.FileX, Node.FileY)) != Node.BuildingId) return false;
	const FIntPoint Key(Node.FileX, Node.FileY);
	if (const auto* Cached = BuildingEntrancePosts.Find(Key)) { OutSurface = *Cached; return true; }
	const float Extent = FMath::Max(1, Node.PeopleFootprintSize) * ActiveTileSize * 0.5f;
	// HO209's lower entrance wing faces -Y after the city's GEO axis conversion.
	// Try the front forecourt first, then other accessible sides for crowded imported cities.
	const FVector Sides[] = {FVector(0,-1,0), FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0)};
	for (const FVector Side : Sides) for (const float Lateral : {0.0f, -0.35f, 0.35f})
	{
		FVector Candidate = Node.Location + Side * (Extent + 85) + FVector(-Side.Y, Side.X, 0) * Extent * Lateral;
		float TerrainZ;
		int32 X, Y;
		if (!TryGetTerrainWorldZAtWorldLocation(Candidate, TerrainZ) ||
			!TryGetPeopleTileCoordinateAtWorldLocation(Candidate, X, Y) || IsWaterTile(X, Y)) continue;
		Candidate.Z = TerrainZ;
		if (!IsPedestrianSpawnLocationOpen(Candidate) || !IsMissionGroundSpawnValid(Candidate)) continue;
		OutSurface = Candidate;
		BuildingEntrancePosts.Add(Key, Candidate);
		auto* Sign = NewObject<UTextRenderComponent>(this);
		Sign->SetText(FText::FromString(Node.BuildingId == 0xD1 ? TEXT("HOSPITAL\nFRONT ENTRANCE") : TEXT("POLICE\nPUBLIC ENTRANCE")));
		Sign->SetHorizontalAlignment(EHTA_Center);
		Sign->SetWorldSize(28);
		Sign->SetTextRenderColor(FColor(235, 244, 237));
		Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sign->ComponentTags.Add(TEXT("ServiceEntranceSign"));
		Sign->RegisterComponent();
		Sign->SetWorldLocationAndRotation(Candidate - Side * 46 + FVector(0,0,237), Side.Rotation());
		AddInstanceComponent(Sign);
		// A physical roadside plaque, with no added collision on the landing or walking surfaces.
		for (bool bPanel : {false, true})
		{
			auto* Part = NewObject<UStaticMeshComponent>(this);
			Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
			Part->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Generated/CityAtlas/M_HospitalRoof.M_HospitalRoof")));
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->ComponentTags.Add(TEXT("ServiceEntranceSign"));
			Part->RegisterComponent(); AddInstanceComponent(Part);
			Part->SetWorldLocationAndRotation(Candidate - Side*50 + FVector(0,0,bPanel ? 215 : 100),Side.Rotation());
			Part->SetWorldScale3D(bPanel ? FVector(.06,2.7,.8) : FVector(.07,.07,2));
		}
		return true;
	}
	return false;
}

void ASimCopterTrafficSystemActor::ClearServiceEntrancePosts()
{
	TArray<UActorComponent*> Parts;
	GetComponents(Parts);
	for (auto* Part : Parts) if (Part->ComponentHasTag(TEXT("ServiceEntranceSign"))) Part->DestroyComponent();
	BuildingEntrancePosts.Reset(); EntranceCrewLastSeenSeconds.Reset();
	ServiceEntranceUpdateSeconds = 0;
}

ASimCopterGroundAgent* ASimCopterTrafficSystemActor::EnsureBuildingEntranceCrew(int32 TileX, int32 TileY)
{
	FVector Entrance;
	if (!TryGetBuildingEntrancePost(TileX, TileY, Entrance)) return nullptr;
	const int32 State = uint8(GetXbldTileId(TileX, TileY)) == 0xD1 ? 5 : 7;
	for (auto Weak : PedestrianAgents)
	{
		auto* Person = Weak.Get();
		if (!Person || Person->IsActorBeingDestroyed() || Person->IsMissionPatientDead() || Person->GetBehaviorCarrier() ||
			Person->IsMissionCarried() || Person->GetBehaviorAttribute(EBhavAttr::State) != State ||
			!Person->IsPersistentHospitalRoofCrew() || Person->MissionEventId != INDEX_NONE) continue;
		const FVector Feet = Person->GetActorLocation() - FVector(0,0,Person->GetCapsuleHalfHeightCm());
		if (FMath::Abs(Feet.Z-Entrance.Z) < 100 && FVector::DistSquared2D(Feet,Entrance) < FMath::Square(300.0f))
		{
			EntranceCrewLastSeenSeconds.Add(FIntPoint(TileX,TileY), GetWorld()->GetTimeSeconds());
			return Person;
		}
	}
	const FIntPoint Key(TileX, TileY);
	const auto* Last = EntranceCrewLastSeenSeconds.Find(Key);
	if (!CanPostHospitalParamedic(Last != nullptr, Last ? *Last : 0, GetWorld()->GetTimeSeconds(), HospitalParamedicRespawnDelaySeconds)) return nullptr;
	for (float Offset : {0.0f, -100.0f, 100.0f})
	{
		const FVector Surface = Entrance + FVector(Offset,0,0);
		if (!IsResponderSpawnClear(Surface) || !IsMissionGroundSpawnValid(Surface)) continue;
		if (TrySpawnOriginalPersonAtTile(TileX, TileY, State == 5 ? 0x0c : 0x0e, State,
			INDEX_NONE, nullptr, INDEX_NONE, false, true, &Surface))
		{
			auto* Person = PedestrianAgents.Last().Get();
			Person->SetHospitalRoofPost(Entrance, 280);
			EntranceCrewLastSeenSeconds.Add(Key, GetWorld()->GetTimeSeconds());
			return Person;
		}
	}
	return nullptr;
}

bool ASimCopterTrafficSystemActor::IsAtHospitalEntrance(const FVector& Feet) const
{
	for (const auto& Entry : BuildingEntrancePosts)
		if (uint8(GetXbldTileId(Entry.Key.X, Entry.Key.Y)) == 0xD1 &&
			FMath::Abs(Feet.Z - Entry.Value.Z) < 100 && FVector::DistSquared2D(Feet, Entry.Value) < FMath::Square(280.0f)) return true;
	return false;
}

void ASimCopterTrafficSystemActor::UpdateServiceEntrances(float DeltaSeconds)
{
	ServiceEntranceUpdateSeconds -= DeltaSeconds;
	if (ServiceEntranceUpdateSeconds > 0) return;
	ServiceEntranceUpdateSeconds = 1.0f;
	auto* Missions = Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ASimCopterMissionSystemActor::StaticClass()));
	auto* Pilot = Cast<ASimCopterOnFootPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	for (const auto& Node : PedestrianNodes)
	{
		if (Node.BuildingId != 0xD1 && Node.BuildingId != 0xD2) continue;
		auto* Worker = EnsureBuildingEntranceCrew(Node.FileX, Node.FileY);
		if (!Worker || !Missions || Node.BuildingId != 0xD1) continue;
		const FVector WorkerFeet = Worker->GetActorLocation() - FVector(0,0,Worker->GetCapsuleHalfHeightCm());
		for (auto Weak : PedestrianAgents)
		{
			auto* Patient = Weak.Get();
			if (!Patient || Patient->IsActorBeingDestroyed() || !Patient->IsMedevacVictim() || Patient->HasMissionResolutionReported() ||
				Patient->HasClaimedPassengerSeat() || Patient->GetBehaviorCarrier()) continue;
			const bool bPilotCarry = Patient->IsMissionCarried() && Pilot &&
				Pilot->GetCarriedMissionEventId() == Patient->MissionEventId && Patient->GetAttachParentActor() == Pilot;
			if (Patient->IsMissionCarried() && !bPilotCarry) continue;
			const FVector Feet = bPilotCarry ? Pilot->GetActorLocation() - FVector(0,0,Pilot->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) :
				Patient->GetActorLocation() - FVector(0,0,Patient->GetCapsuleHalfHeightCm());
			if (!IsAtHospitalEntrance(Feet) || FMath::Abs(Feet.Z-WorkerFeet.Z)>100 ||
				FVector::DistSquared2D(Feet,WorkerFeet)>FMath::Square(180.0f)) continue;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(HospitalEntranceHandoff), false, Worker);
			Query.AddIgnoredActor(Patient); if (Pilot) Query.AddIgnoredActor(Pilot);
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit,WorkerFeet+FVector(0,0,45),
				Feet+FVector(0,0,45),ECC_Camera,Query)) continue;
			// A dropped/carried casualty uses the existing pickup and idempotent delivery accounting.
			if (!Missions->AcceptEntrancePatient(Patient)) continue;
			if (bPilotCarry) Pilot->ConsumeCarriedMissionPerson();
			Patient->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Patient->SetLifeSpan(3);
			Patient->SetActorHiddenInGame(true);
			Patient->SetActorEnableCollision(false);
			break;
		}
	}
}

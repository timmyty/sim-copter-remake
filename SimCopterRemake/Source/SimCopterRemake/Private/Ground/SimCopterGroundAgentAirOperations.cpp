#include "Ground/SimCopterGroundAgent.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterParticleFX.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

namespace
{
ASimCopterMissionSystemActor* MissionActor(const AActor* Actor)
{
	return Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(Actor->GetWorld(), ASimCopterMissionSystemActor::StaticClass()));
}
}

bool ASimCopterGroundAgent::IsArrestableSuspect() const
{
	return AgentKind == ESimCopterGroundAgentKind::Pedestrian && !bMissionPatientDead &&
		!bMissionResolutionReported && !bHandcuffed &&
		(SimCopterAirOperations::IsCriminalState(BehaviorContext.GetStateIndex()) ||
		 SimCopterAirOperations::IsCriminalState(InitialPersonState));
}

bool ASimCopterGroundAgent::StunForArrest()
{
	if (!IsArrestableSuspect() || bTaserStunned || BehaviorCarrier.IsValid() || bMissionCarried) return false;
	bTaserStunned = true;
	if (IsKnockedDown()) FinishKnockdown(true);
	bBehaviorActive = false;
	bMissionStationary = true;
	CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = BehaviorStepVelocityCmPerSec = FVector::ZeroVector;
	ClearMoveTarget();
	SetForcedPedestrianFigureClip(TEXT("Inju"));
	StopWalkingVoice();
	// Capture is credited only when a roof officer takes custody at the police station.
	return true;
}

void ASimCopterGroundAgent::CompletePoliceDelivery(const FVector& Roof)
{
	if (!bHandcuffed || bMissionResolutionReported) return;
	if (auto* Missions = MissionActor(this))
	{
		if (MissionEventId != INDEX_NONE)
		{
			if (InitialPersonState == 13) Missions->ReportBurglarCaught(MissionEventId);
			else Missions->PostMissionEvent(SimCopterMissions::EVT_CriminalCaught, MissionEventId, 1, false);
		}
	}
	bMissionResolutionReported = true;
	AlightFromCarrier(false);
	SetActorLocation(Roof + FVector(0, 0, GetCapsuleHalfHeightCm()));
	SetActorHiddenInGame(false);
	bBehaviorActive = false;
	bMissionStationary = true;
	bSnapToGround = false;
	SetForcedPedestrianFigureClip(TEXT("NoMo"));
	SetLifeSpan(5.0f);
}

void ASimCopterGroundAgent::EnsureTowMission(bool bPlayerCaused)
{
	if (!bVehicleStalled || TowMissionId != INDEX_NONE) return;
	auto* Missions = MissionActor(this);
	auto* Traffic = Cast<ASimCopterTrafficSystemActor>(GetOwner());
	int32 X = 0, Y = 0;
	if (!Missions || !Traffic || !Traffic->TryGetPeopleTileCoordinateAtWorldLocation(GetActorLocation(), X, Y)) return;
	TowMissionId = Missions->CreateMissionAt(X, Y, SimCopterMissions::TYPE_VehicleTow);
	if (TowMissionId != INDEX_NONE && bPlayerCaused)
	{
		// Do not make repeatedly wrecking and repairing the same car a money generator.
		Missions->SuppressMissionRewards(TowMissionId);
	}
}

void ASimCopterGroundAgent::SetVehicleStalled(bool bPlayerCaused)
{
	if (AgentKind != ESimCopterGroundAgentKind::Vehicle) return;
	bVehicleStalled = true;
	CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = FVector::ZeroVector;
	ClearMoveTarget();
	EnsureTowMission(bPlayerCaused);
}

void ASimCopterGroundAgent::ApplyHelicopterVehicleImpact(const FVector& WorldImpact)
{
	if (AgentKind != ESimCopterGroundAgentKind::Vehicle || bVehicleTowed ||
		bVehicleExploded || HelicopterImpactCount >= 4 || VehicleImpactCooldown > 0) return;
	VehicleImpactCooldown = 0.65f;
	++HelicopterImpactCount;
	if (auto* Audio = USimCopterAudioSubsystem::Get(this))
		if (auto* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Generated/CityAtlas/S_HelicopterVehicleImpact.S_HelicopterVehicleImpact")))
			UGameplayStatics::PlaySoundAtLocation(this, Sound, WorldImpact,
				USimCopterAudioSubsystem::VolumeIndexToGain(Audio->GetMasterVolume()), FMath::FRandRange(0.92f,1.08f), 0,
				LoadObject<USoundAttenuation>(nullptr,TEXT("/Game/Generated/CityAtlas/A_VehicleImpact.A_VehicleImpact")));
	if (auto* Missions = MissionActor(this))
		if (auto* FX = Missions->FindComponentByClass<USimCopterParticleFXComponent>())
		{
			const FVector Away = (WorldImpact - GetActorLocation()).GetSafeNormal();
			for (int32 Spark = 0; Spark < 14; ++Spark)
			{
				const FVector Direction = (Away * 0.7f + FMath::VRand() + FVector(0,0,0.6f)).GetSafeNormal();
				// Distinct origins preserve each slot's custom lifetime in the shared particle pool.
				FX->SpawnParticle(WorldImpact + Direction * (3.0f + Spark), Direction * FMath::FRandRange(180.0f,460.0f),
					3.5f, FLinearColor(1,0.65f,0.12f), FMath::FRandRange(0.18f,0.5f), -650);
			}
		}
	VehicleDents.Add(OriginalMeshComponent
		? OriginalMeshComponent->GetComponentTransform().InverseTransformPosition(WorldImpact)
		: GetActorTransform().InverseTransformPosition(WorldImpact));
	RebuildVehicleDents();
	if (HelicopterImpactCount >= 2) SetVehicleStalled(true);
	if (HelicopterImpactCount == 4)
	{
		VehicleExplosionSeconds = 4.0f;
		if (auto* Missions = MissionActor(this))
		{
			Missions->CreatePlayerCausedCarFireForVehicle(this);
			if (TowMissionId != INDEX_NONE)
				Missions->PostMissionEvent(SimCopterMissions::EVT_SetCategory, TowMissionId, SimCopterMissions::CAT_ExpireSilently, true);
		}
		TowMissionId = INDEX_NONE;
	}
}

void ASimCopterGroundAgent::RebuildVehicleDents()
{
	if (!OriginalMeshComponent || VehicleDents.IsEmpty() || AgentKind != ESimCopterGroundAgentKind::Vehicle) return;
	const int32 Count = OriginalMeshComponent->GetNumSections();
	if (UndamagedVehicleVertices.Num() != Count)
	{
		UndamagedVehicleVertices.SetNum(Count);
		for (int32 S = 0; S < Count; ++S)
			if (auto* Section = OriginalMeshComponent->GetProcMeshSection(S))
				for (const auto& Vertex : Section->ProcVertexBuffer) UndamagedVehicleVertices[S].Add(Vertex.Position);
	}
	for (int32 S = 0; S < Count; ++S)
	{
		auto* Section = OriginalMeshComponent->GetProcMeshSection(S);
		if (!Section || UndamagedVehicleVertices[S].Num() != Section->ProcVertexBuffer.Num()) continue;
		TArray<FVector> Vertices = UndamagedVehicleVertices[S], Normals;
		TArray<FVector2D> UV;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		const FVector Center = Section->SectionLocalBox.GetCenter();
		for (int32 V = 0; V < Vertices.Num(); ++V)
		{
			float Damage = 0;
			for (const FVector& Impact : VehicleDents)
			{
				const FVector Before = Vertices[V];
				Vertices[V] = SimCopterAirOperations::DentVertex(Before, Impact, Center);
				Damage += FVector::Distance(Before, Vertices[V]);
			}
			const auto& Old = Section->ProcVertexBuffer[V];
			Normals.Add(Old.Normal); UV.Add(Old.UV0); Tangents.Add(Old.Tangent);
			Colors.Add((FLinearColor(Old.Color) * FMath::Clamp(1.0f - Damage * 0.025f, 0.25f, 1.0f)).ToFColor(false));
		}
		OriginalMeshComponent->UpdateMeshSection(S, Vertices, Normals, UV, Colors, Tangents);
	}
}

void ASimCopterGroundAgent::SetVehicleTowed(bool bTowed)
{
	bVehicleTowed = bTowed;
	bSnapToGround = !bTowed;
	SetActorEnableCollision(!bTowed);
	CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = FVector::ZeroVector;
	ClearMoveTarget();
}

void ASimCopterGroundAgent::CompleteVehicleTow(bool bScrap)
{
	if (!bVehicleStalled || HelicopterImpactCount >= 4) return;
	if (auto* Missions = MissionActor(this); Missions && TowMissionId != INDEX_NONE)
		Missions->PostMissionEvent(SimCopterMissions::EVT_CarCleared, TowMissionId, 1, true);
	TowMissionId = INDEX_NONE;
	bMissionResolutionReported = true;
	SetVehicleTowed(false);
	// The shop takes the vehicle off the road pool; show it on the service lift before removal.
	bVehicleStalled = true;
	SetLifeSpan(bScrap ? 8.0f : 15.0f);
}

bool ASimCopterGroundAgent::TickAirOperations(float DeltaSeconds)
{
	VehicleImpactCooldown = FMath::Max(0.0f, VehicleImpactCooldown - DeltaSeconds);
	if (VehicleExplosionSeconds > 0)
	{
		VehicleExplosionSeconds -= DeltaSeconds;
		if (VehicleExplosionSeconds <= 0 && !bVehicleExploded)
		{
			bVehicleExploded = true;
			if (auto* Missions = MissionActor(this))
			{
				Missions->SpawnCrashBurningDebris(GetActorLocation(), MissionEventId);
				if (auto* FX = Missions->FindComponentByClass<USimCopterParticleFXComponent>())
					FX->SpawnHardLanding(GetActorLocation(), false);
			}
			SetActorHiddenInGame(true);
			SetActorEnableCollision(false);
			SetLifeSpan(0.2f);
		}
	}
	if (bVehicleStalled || bVehicleTowed)
	{
		CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = FVector::ZeroVector;
		if (!bVehicleTowed && !bVehicleExploded) UpdateGroundSnap(DeltaSeconds);
		return true;
	}
	if (IsInPoliceCustody() && !bPassengerFallActive && !IsMedevacVictim())
	{
		bBehaviorActive = false;
		CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = FVector::ZeroVector;
		if (!BehaviorCarrier.IsValid() && !bMissionCarried) UpdateGroundSnap(DeltaSeconds);
		UpdateJankyAnimation(DeltaSeconds);
		StopWalkingVoice();
		return true;
	}
	return false;
}

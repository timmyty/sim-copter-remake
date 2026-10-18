#include "Ground/SimCopterGroundAgent.h"
#include "Game/SimCopterSettings.h"
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
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Ground/SimCopterCriminalCar.h"

namespace
{
ASimCopterMissionSystemActor* MissionActor(const AActor* Actor)
{
	return Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(Actor->GetWorld(), ASimCopterMissionSystemActor::StaticClass()));
}
}

bool ASimCopterGroundAgent::UpdatePoliceRecovery()
{
	// Remake taser custody has no retail BHAV: 1053 selects only active criminals,
	// and 1060 immediately credits arrest and sends them to a police car. Recover
	// disabled people physically instead, retaining delivery credit for the station.
	// Street officers put down by a police car (states 8 and 0xe, BHAV 1150/1152) keep their
	// original programs, except that they now also deal with burglars: BHAV 1053's search only
	// sees criminals on foot, so beside a burglar's car it reported "no suspect in area".
	const int32 OfficerState = BehaviorContext.GetStateIndex();
	const bool bStreetOfficer = OfficerState == SimCopterCriminalCar::OfficerStateDefault ||
		OfficerState == SimCopterCriminalCar::OfficerStateAgainstFleeing;
	if ((OfficerState != 7 && !bStreetOfficer) || BehaviorCarrier.IsValid() ||
		bMissionCarried || bPassengerFallActive || bMissionPatientDead || IsInPoliceCustody() || IsKnockedDown()) return false;
	auto* Traffic = Cast<ASimCopterTrafficSystemActor>(GetOwner());
	auto* Heli = ResolvePlayerHelicopter();
	if (!Traffic) return false;
	if (bStreetOfficer)
	{
		ASimCopterGroundAgent* Burglary = Traffic->FindPoliceRecoveryTarget(*this);
		if (!Burglary || !(Burglary->IsCriminalCar() ||
			Burglary->InitialPersonState == SimCopterCriminalCar::BurglarPersonState)) return false;
	}
	else if (!bHasHospitalRoofPost && (!Heli || Heli->IsFixedWingAircraft())) return false;
	auto* Carried = Traffic->FindPersonCarriedBy(*this);
	if (Carried && (!Carried->IsInPoliceCustody() && !Carried->IsMedevacVictim())) return false;
	ASimCopterGroundAgent* Target = nullptr;
	if (bHasHospitalRoofPost)
	{
		// Direct cage drops never pass through a cabin seat. A station officer must
		// collect the released person on its own roof, after the fall has finished.
		if (Carried) return false;
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It)
		{
			if (*It == this || It->IsActorBeingDestroyed() || It->IsHidden() || It->HasMissionResolutionReported() ||
				It->GetBehaviorCarrier() || It->IsMissionCarried() || It->IsPassengerFalling() ||
				It->IsMedevacVictim() || It->IsMissionPatientDead() ||
				(!It->IsInPoliceCustody() && !It->IsArrestableSuspect())) continue;
			const FVector Feet = It->GetActorLocation() - FVector(0,0,It->GetCapsuleHalfHeightCm());
			const FVector Offset = Feet - HospitalRoofPostWorldLocation;
			const float Extent = HospitalRoofPostHalfExtentCm - It->GetCollisionRadiusCm();
			if (FMath::Abs(Offset.X) > Extent || FMath::Abs(Offset.Y) > Extent || FMath::Abs(Offset.Z) > 30) continue;
			const float Distance = FVector::DistSquared(GetActorLocation(),It->GetActorLocation());
			if (Distance < BestDistance) { BestDistance = Distance; Target = *It; }
		}
	}
	else if (!Carried)
	{
		Target = Traffic->FindPoliceRecoveryTarget(*this);
		// Requested remake addition: deployed police help with medical retrieval only
		// after criminal work. Keep the same patient claims, route and real-body carry.
		if (!Target && !bStreetOfficer && USimCopterSettings::RemakeFeaturesEnabled(this)) Target = Traffic->FindAndClaimGroundPatient(*this);
	}
	if (auto* Previous = Cast<ASimCopterGroundAgent>(BehaviorContext.SelectedObject.Get());
		Previous && Previous != Target && Previous != Carried && Previous->HospitalPatientClaim.Get() == this)
		Previous->HospitalPatientClaim.Reset();
	if (!Carried && !Target) return false;
	AActor* Goal = Carried ? static_cast<AActor*>(Heli) : Target;
	BehaviorContext.SelectedObject = Goal;
	BehaviorContext.SelectedLocation = Goal->GetActorLocation();
	BehaviorContext.bHasSelection = true;
	BehaviorContext.bSelectionIsHarness = false;
	// BHAV 1053 searches thirty tiles, but its repeated two-tile probes and random
	// Run-10/Run-30 bursts can lose a distant suspect. Keep a routed pursuit through
	// physical contact, at the shipped Run-10 speed, then resume its arrest/hold.
	BehaviorContext.Attributes[EBhavAttr::MoveSpeed] = Carried || (Target && Target->IsMedevacVictim()) ? 16 : 25;
	BehaviorContext.Attributes[EBhavAttr::AutoTurn] = 1;
	if (StepTowardSelectedObject(BehaviorContext) != ESimCopterBehaviorStepResult::Arrived) return true;
	BehaviorStepVelocityCmPerSec = FVector::ZeroVector;
	BehaviorStepTimeRemainingSeconds = 0;
	if (bHasHospitalRoofPost)
	{
		Target->bHandcuffed = true;
		Target->bTaserStunned = false;
		Target->TaserStunSeconds = 0;
		Target->CompletePoliceDelivery(Target->GetActorLocation() - FVector(0,0,Target->GetCapsuleHalfHeightCm()));
	}
	else if (Carried)
	{
		// Keep hold while hovering too high or full; never drop or teleport a prisoner.
		if (Heli->CanBoardMissionPassengers() && Heli->GetAvailablePassengerSeats() >= 1 &&
			Carried->BoardCarrier(Heli, false)) BoardCarrier(Heli, false);
	}
	else if (Target->IsCriminalCar())
	{
		// Electrical immobilization intentionally bypasses the retail spotlight-only
		// stop order. Physical officer contact completes the same burglar mission.
		if (auto* Missions = MissionActor(this); Missions && Missions->IsMissionEventActive(Target->GetCriminalEventId()))
		{
			Target->bMissionResolutionReported = true;
			Target->SetCriminalState(uint8(SimCopterCriminalCar::EState::Leaving));
			Missions->ReportBurglarCaught(Target->GetCriminalEventId());
			Target->Destroy();
		}
	}
	else if (Target->IsMedevacVictim())
	{
		if (!IsCarryingPerson() && Target->TryClaimHospitalPatient(this)) PutSelectedPersonOnMe(BehaviorContext);
	}
	else if (!Target->IsInPoliceCustody())
	{
		// A burglar caught on foot closes the burglary itself; the abandoned car then leaves
		// with its retired record (RunBurglarOutsidePhase).
		if (Target->InitialPersonState == SimCopterCriminalCar::BurglarPersonState && Target->MissionEventId != INDEX_NONE)
			if (auto* Missions = MissionActor(this); Missions && Missions->IsMissionEventActive(Target->MissionEventId))
				Missions->ReportBurglarCaught(Target->MissionEventId);
		// BHAV 1053 rec[17] / 1150 rec[6] push criminal-caught; 1053 rec[26]
		// holds the officer in the original 2Gab pose while the player remains nearby.
		if (PushReactionOnSelectedObject(BehaviorContext, 1060) && !bStreetOfficer)
		{
			// Street officers keep their own BHAV 1150/1152, which walks them back to the car.
			BehaviorContext.Stack.Reset();
			BehaviorContext.Stack.Add({1053, 26, {}});
		}
	}
	else if (!IsCarryingPerson() && !Target->GetBehaviorCarrier() && !Target->IsMissionCarried() &&
		PutSelectedPersonOnMe(BehaviorContext))
	{
		Target->bHandcuffed = true;
		Target->bTaserStunned = false;
		Target->TaserStunSeconds = 0;
	}
	return true;
}

bool ASimCopterGroundAgent::IsArrestableSuspect() const
{
	return AgentKind == ESimCopterGroundAgentKind::Pedestrian && !bMissionPatientDead &&
		!bMissionResolutionReported && !bHandcuffed &&
		(SimCopterAirOperations::IsCriminalState(BehaviorContext.GetStateIndex()) ||
		 SimCopterAirOperations::IsCriminalState(InitialPersonState));
}

bool ASimCopterGroundAgent::StunForArrest(float DurationSeconds)
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return false;
	if (!IsArrestableSuspect() || bTaserStunned || BehaviorCarrier.IsValid() || bMissionCarried) return false;
	bTaserStunned = true;
	if (IsKnockedDown()) FinishKnockdown(true);
	TaserStunSeconds=FMath::Clamp(DurationSeconds,0.f,12.f);
	bTaserResumeBehavior=bBehaviorActive; bTaserResumeStationary=bMissionStationary;
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
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return;
	if (!IsTowableVehicle() || TowMissionId != INDEX_NONE) return;
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

bool ASimCopterGroundAgent::DisableVehicleWithTaser()
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return false;
	if (AgentKind != ESimCopterGroundAgentKind::Vehicle || bVehicleStalled || bVehicleTowed ||
		bVehicleTipped || bVehicleExploded || bMissionResolutionReported || IsHidden() || IsActorBeingDestroyed()) return false;
	// Remake-only electrical stop: preserve the route and traffic state for recovery.
	// A fresh hit refreshes the duration without creating damage or a towing mission.
	VehicleTaserSeconds = SimCopterAirOperations::VehicleTaserDisableSeconds;
	CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = BehaviorStepVelocityCmPerSec = FVector::ZeroVector;
	return true;
}

void ASimCopterGroundAgent::ApplyHelicopterVehicleImpact(const FVector& WorldImpact)
{
	if (AgentKind != ESimCopterGroundAgentKind::Vehicle || bVehicleTowed ||
		bVehicleExploded || bVehicleTipped || VehicleImpactCooldown > 0) return;
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
	// SCHOOK: VehicleHelicopterCollision 0x0049f680, case 0xc: ignite immediately
	// and queue event 0x34 with event id -1. FUN_0049fd00 rolls the car 900
	// tenth-degrees. Keep this fork's four-second fuse (retail uses 120 seconds).
	bVehicleTipped = true;
	bVehicleStalled = true;
	VehicleTipRoll = GetActorTransform().InverseTransformPosition(WorldImpact).Y >= 0 ? -90.0f : 90.0f;
	CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = FVector::ZeroVector;
	ClearMoveTarget();
	VehicleExplosionSeconds = 4.0f;
	ApplyVehicleTipVisual();
	EvacuateVehicleOccupants();
	if (auto* Audio = USimCopterAudioSubsystem::Get(this))
		Audio->Play3D(SimCopterSound::SND_FIRESTAR, GetActorLocation());
	if (auto* Missions = MissionActor(this))
	{
		Missions->PostMissionEvent(SimCopterMissions::EVT_CrashPenaltyG, INDEX_NONE, 1, false);
		if (TowMissionId != INDEX_NONE)
			Missions->PostMissionEvent(SimCopterMissions::EVT_SetCategory, TowMissionId, SimCopterMissions::CAT_ExpireSilently, true);
	}
	TowMissionId = INDEX_NONE;
}

bool ASimCopterGroundAgent::ApplyWeaponVehicleHit(bool bMissile, const FVector& WorldImpact)
{
	if (AgentKind != ESimCopterGroundAgentKind::Vehicle || bVehicleExploded || IsActorBeingDestroyed()) return false;
	// FUN_0049f680 mode 3/7 dispatches the vehicle's fire/destruction path.
	// In this fork a missile destroys immediately; a gun strike starts a short
	// fuse and another strike finishes it. Mission allocation cannot make a car invulnerable.
	const bool bAlreadyBurning = VehicleExplosionSeconds > 0;
	if (!bAlreadyBurning)
	{
		if (auto* Missions = MissionActor(this))
		{
			Missions->CreatePlayerCausedCarFireForVehicle(this);
			if (TowMissionId != INDEX_NONE)
				Missions->PostMissionEvent(SimCopterMissions::EVT_SetCategory, TowMissionId, SimCopterMissions::CAT_ExpireSilently, true);
		}
		TowMissionId = INDEX_NONE;
		bVehicleStalled = bVehicleTipped = true;
		VehicleTipRoll = 20;
		CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = FVector::ZeroVector;
		ClearMoveTarget();
		VehicleExplosionSeconds = 2;
		ApplyVehicleTipVisual();
		EvacuateVehicleOccupants();
	}
	if (bMissile || bAlreadyBurning) ExplodeVehicle();
	return true;
}

void ASimCopterGroundAgent::ExplodeVehicle()
{
	if (AgentKind != ESimCopterGroundAgentKind::Vehicle || bVehicleExploded) return;
	if (auto* Traffic = Cast<ASimCopterTrafficSystemActor>(GetOwner())) Traffic->NotifyVehicleAccidentExploded(*this);
	bVehicleExploded = true;
	VehicleExplosionSeconds = 0;
	if (auto* Audio = USimCopterAudioSubsystem::Get(this)) Audio->Play3D(SimCopterSound::SND_BLDEXPL, GetActorLocation());
	if (auto* Missions = MissionActor(this))
	{
		Missions->SpawnCrashBurningDebris(GetActorLocation(), MissionEventId);
		if (auto* FX = Missions->FindComponentByClass<USimCopterParticleFXComponent>()) FX->SpawnHardLanding(GetActorLocation(), false);
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetLifeSpan(.2f);
}

void ASimCopterGroundAgent::ApplyVehicleTipVisual()
{
	if (!bVehicleTipped || !VisualRoot) return;
	VisualRoot->SetRelativeRotation(FRotator(0,0,VehicleTipRoll));
	// The capsule keeps its road height and routing identity. Only the rendered
	// body rolls; lift its lowest corner to the road so wide cars don't sink.
	const UPrimitiveComponent* Body = bUsingOriginalMesh ? static_cast<UPrimitiveComponent*>(OriginalMeshComponent) : static_cast<UPrimitiveComponent*>(ProxyMeshComponent);
	if (Body)
	{
		const FBox LocalBox = Body->CalcBounds(Body->GetComponentTransform()).GetBox();
		const float FeetZ = GetActorLocation().Z - GetCapsuleHalfHeightCm();
		FVector Offset = VisualRoot->GetRelativeLocation();
		Offset.Z += FeetZ + 2 - LocalBox.Min.Z;
		SetVisualRootRelativeLocation(Offset);
	}
}

void ASimCopterGroundAgent::DouseVehicleAccident()
{
	if (!IsVehicleAccidentBurning()) return;
	VehicleExplosionSeconds = 0;
	// A saved car can be recovered, but causing the accident cannot earn rewards.
	EnsureTowMission(true);
}

void ASimCopterGroundAgent::EvacuateVehicleOccupants()
{
	auto* Traffic = Cast<ASimCopterTrafficSystemActor>(GetOwner());
	if (!Traffic || !GetWorld()) return;
	bool bHasPeople = false;
	bool bPendingExit = false;
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It)
	{
		ASimCopterGroundAgent* Person = *It;
		if (Person->AgentKind != ESimCopterGroundAgentKind::Pedestrian) continue;
		if (Person->BehaviorStartingVehicle.Get() == this) bHasPeople = true;
		if (Person->BehaviorCarrier.Get() != this) continue;
		bHasPeople = true;
		FVector Exit;
		if (!Traffic->TryGetVehicleEscapeLocation(*this,Exit)) { bPendingExit = true; continue; }
		Person->AlightFromCarrier(false);
		Person->SetActorLocation(Exit-FVector(0,0,92-Person->GetCapsuleHalfHeightCm()));
		Person->BehaviorStartingVehicle.Reset();
		Person->BeginVehicleEscape(GetActorLocation());
	}
	if (bHasPeople) bVehicleDriverEvacuated = !bPendingExit;
	if (!bHasPeople && !bVehicleDriverEvacuated)
	{
		FVector Exit;
		if (Traffic->TryGetVehicleEscapeLocation(*this,Exit))
			if (auto* Driver = Traffic->SpawnVehicleDriver(Exit,this))
			{
				Driver->BeginVehicleEscape(GetActorLocation());
				bVehicleDriverEvacuated = true;
			}
	}
}

void ASimCopterGroundAgent::BeginVehicleEscape(const FVector& From)
{
	if (AgentKind != ESimCopterGroundAgentKind::Pedestrian || bMissionPatientDead || IsMedevacVictim() || IsInPoliceCustody()) return;
	VehicleEscapeOrigin = From;
	VehicleEscapeSeconds = 6.0f;
	ClearMoveTarget();
	SetForcedPedestrianFigureClip(TEXT("1Run"));
}

bool ASimCopterGroundAgent::TickVehicleEscape(float DeltaSeconds)
{
	if (VehicleEscapeSeconds <= 0) return false;
	if (BehaviorCarrier.IsValid() || bMissionCarried || IsInPoliceCustody() || IsMedevacVictim() || IsKnockedDown())
	{
		VehicleEscapeSeconds = 0;
		ClearForcedPedestrianFigureClip();
		return false;
	}
	VehicleEscapeSeconds = FMath::Max(0.0f,VehicleEscapeSeconds-DeltaSeconds);
	const FVector Away = (GetActorLocation()-VehicleEscapeOrigin).GetSafeNormal2D();
	const float Step = 260.0f * FMath::Min(DeltaSeconds,0.1f);
	const FVector Start = GetActorLocation();
	for (float Angle : {0.0f,45.0f,-45.0f,90.0f,-90.0f})
	{
		const FVector Direction = Away.RotateAngleAxis(Angle,FVector::UpVector);
		const FVector Target = Start+Direction*Step;
		if (auto* Traffic = Cast<ASimCopterTrafficSystemActor>(GetOwner()))
		{
			int32 X,Y;
			if (!Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Target,X,Y) ||
				(Traffic->IsWaterTile(X,Y) && !IsStandingOnRaisedDeck())) continue;
		}
		float Z;
		if (!TryGetWalkSurfaceZAt(Target,Z) || FMath::Abs(Z-(Start.Z-GetCapsuleHalfHeightCm()))>45 ||
			IsPedestrianStepBlockedByGeometry(Start,Target,Z)) continue;
		if (auto* Missions = MissionActor(this); Missions && Missions->ShouldAvoidFireStep(
			Start-FVector(0,0,GetCapsuleHalfHeightCm()),FVector(Target.X,Target.Y,Z))) continue;
		MoveAgainstCityGeometry(Direction*Step,Direction.Rotation().Quaternion());
		break;
	}
	CurrentVelocityCmPerSec = (GetActorLocation()-Start)/FMath::Max(DeltaSeconds,0.001f);
	ContainOutsideBuildingGeometry();
	UpdateGroundSnap(DeltaSeconds);
	UpdateWaterSubmersion(DeltaSeconds);
	UpdateJankyAnimation(DeltaSeconds);
	// Forced clips hold their frame in UpdateFigureAnimation; fleeing must animate.
	AdvanceForcedFigureFrames(DeltaSeconds);
	UpdateWalkingVoice(CurrentVelocityCmPerSec.IsNearlyZero() ? 0 : 12);
	if (VehicleEscapeSeconds<=0) { ClearForcedPedestrianFigureClip(); StopWalkingVoice(); }
	return true;
}

bool ASimCopterGroundAgent::IsCageRecoverableVehicle() const
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return false;
	if (AgentKind != ESimCopterGroundAgentKind::Vehicle || IsHidden() || IsActorBeingDestroyed() ||
		bVehicleExploded || IsVehicleAccidentBurning() || bMissionResolutionReported) return false;
	if (bVehicleStalled || bVehicleTowed || IsVehicleTaserDisabled()) return true;
	if (!bFleeing) return false;
	const auto* Missions = MissionActor(this);
	return !bCriminalCar || !Missions || Missions->IsMissionEventActive(CriminalEventId);
}

void ASimCopterGroundAgent::SetVehicleTowed(bool bTowed)
{
	bVehicleTowed = bTowed;
	bSnapToGround = !bTowed;
	SetActorEnableCollision(!bTowed);
	CurrentVelocityCmPerSec = ExternalVelocityCmPerSec = FVector::ZeroVector;
	// A released runner or temporarily disabled car keeps its road route.
}

void ASimCopterGroundAgent::CompleteVehicleTow(bool bScrap)
{
	if (!IsCageRecoverableVehicle()) return;
	if (bCriminalCar)
		if (auto* Missions = MissionActor(this); Missions && Missions->IsMissionEventActive(CriminalEventId))
			Missions->ReportBurglarCaught(CriminalEventId);
	if (auto* Missions = MissionActor(this); Missions && TowMissionId != INDEX_NONE)
		Missions->PostMissionEvent(SimCopterMissions::EVT_CarCleared, TowMissionId, 1, true);
	TowMissionId = INDEX_NONE;
	bMissionResolutionReported = true;
	SetVehicleTowed(false);
	// The shop takes the vehicle off the road pool; show it on the service lift before removal.
	bVehicleStalled = true;
	bFleeing = false;
	VehicleTaserSeconds = 0;
	ClearMoveTarget();
	SetLifeSpan(bScrap ? 8.0f : 15.0f);
}

bool ASimCopterGroundAgent::TickAirOperations(float DeltaSeconds)
{
	if (bTaserStunned && TaserStunSeconds>0 && !bHandcuffed && !bMissionCarried && !BehaviorCarrier.IsValid())
	{
		TaserStunSeconds=FMath::Max(0.f,TaserStunSeconds-FMath::Max(0.f,DeltaSeconds));
		if (TaserStunSeconds<=0)
		{
			bTaserStunned=false;
			if (!bMissionPatientDead && !IsMedevacVictim() && !bMissionResolutionReported)
			{
				bBehaviorActive=bTaserResumeBehavior; bMissionStationary=bTaserResumeStationary;
				ClearForcedPedestrianFigureClip();
			}
		}
	}
	const bool bTaserHeldThisFrame = IsVehicleTaserDisabled();
	VehicleTaserSeconds = FMath::Max(0.0f, VehicleTaserSeconds - FMath::Max(0.0f, DeltaSeconds));
	if (TickVehicleEscape(DeltaSeconds)) return true;
	VehicleImpactCooldown = FMath::Max(0.0f, VehicleImpactCooldown - DeltaSeconds);
	if (bVehicleTipped && !bVehicleExploded && !bVehicleDriverEvacuated)
	{
		VehicleEvacuationRetry -= DeltaSeconds;
		if (VehicleEvacuationRetry <= 0) { EvacuateVehicleOccupants(); VehicleEvacuationRetry=0.2f; }
	}
	if (VehicleExplosionSeconds > 0)
	{
		VehicleExplosionSeconds -= DeltaSeconds;
		if (VehicleExplosionSeconds <= 0 && !bVehicleExploded)
		{
			ExplodeVehicle();
		}
	}
	if (bVehicleStalled || bVehicleTowed || bTaserHeldThisFrame)
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

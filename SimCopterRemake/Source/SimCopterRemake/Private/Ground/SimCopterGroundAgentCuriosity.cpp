#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterBandNavigation.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr int32 MaxVisitors = 3;
constexpr float VisitorSpacing = 110;
constexpr float WalkClearance = 110;
constexpr float VisitSeconds = 8;
constexpr float ApproachSeconds = 14;
}

bool ASimCopterGroundAgent::IsEligibleForAmbientCuriosity() const
{
	// This extension borrows idle civilians, never a patient, criminal, responder, mission
	// passenger, animal, statue, mechanic, band member, or an original reaction/gawk program.
	if (AgentKind != ESimCopterGroundAgentKind::Pedestrian || !bBehaviorActive || !BehaviorModel.IsValid() ||
		IsActorBeingDestroyed() || IsHidden() || IsCow() || InitialPersonState != 0 ||
		InitialBehaviorProgramId != INDEX_NONE || MissionEventId != INDEX_NONE ||
		BehaviorContext.GetStateIndex() != 0 || bMissionPatientDead || bMissionCarried ||
		bMissionStationary || bPassengerFallActive || bBehaviorMoveSuspended || BehaviorCarrier.IsValid() ||
		bHasHospitalRoofPost || IsKnockedDown() || IsAvoidanceMoveActive() || IsAvoidancePathOffsetActive() ||
		IsGuidanceMoveTargetActive() || BehaviorContext.Attributes[EBhavAttr::WrittenOff] != 0 ||
		BehaviorContext.Attributes[EBhavAttr::Visible] == 0 ||
		BehaviorContext.Attributes[EBhavAttr::AmbientFlag] == 0 ||
		BehaviorContext.Attributes[EBhavAttr::RecentlySpooked] != 0 ||
		BehaviorContext.Attributes[EBhavAttr::ReactionDepth] != 0) return false;
	for (const auto& Frame : BehaviorContext.Stack)
		if (Frame.ProgramId == 273 || Frame.ProgramId == 274 || (Frame.ProgramId >= 900 && Frame.ProgramId < 1000)) return false;
	return true;
}

void ASimCopterGroundAgent::StopAmbientCuriosity()
{
	if (AmbientCuriositySlot != INDEX_NONE)
	{
		BehaviorStepVelocityCmPerSec = FVector::ZeroVector;
		BehaviorStepTimeRemainingSeconds = 0;
		if (IsEligibleForAmbientCuriosity()) BehaviorContext.PendingAnimMnemonic = TEXT("NoMo");
	}
	AmbientCuriosityAircraft.Reset();
	AmbientCuriositySlot = INDEX_NONE;
	bAmbientCuriosityArrived = false;
	AmbientCuriosityTimeLeft = AmbientCuriositySpeakingTime = 0;
	// Stable per-person staggering, without altering the original VM's random stream on probes.
	AmbientCuriosityCooldown = 25 + (GetTypeHash(GetFName()) % 21);
}

bool ASimCopterGroundAgent::UpdateAmbientCuriosity(float DeltaSeconds)
{
	// Requested extension, not an invented retail BHAV: nearby people admire the current
	// parked aircraft, then return to their untouched original program. Real reaction programs
	// and medical/fire activity always win. No extra Sims or passenger seats are created.
	DeltaSeconds = FMath::Max(0.f, DeltaSeconds);
	const bool bWasVisiting = AmbientCuriositySlot != INDEX_NONE;
	if (!bWasVisiting)
	{
		AmbientCuriosityCooldown = FMath::Max(0.f, AmbientCuriosityCooldown - DeltaSeconds);
		if (AmbientCuriosityCooldown > 0) return false;
		AmbientCuriosityCooldown = 2 + (GetTypeHash(GetFName()) % 4);
	}
	if (!IsEligibleForAmbientCuriosity())
	{
		if (bWasVisiting) StopAmbientCuriosity();
		return false;
	}
	auto* Traffic = Cast<ASimCopterTrafficSystemActor>(GetOwner());
	auto* Aircraft = ResolvePlayerHelicopter();
	const FVector Here = GetActorLocation();
	if (!Traffic || !Aircraft || Aircraft->IsFixedWingAircraft() || Aircraft->IsSupportAircraft() ||
		Aircraft->GetFlightModel().State != ESimCopterFlightState::Parked ||
		!Aircraft->CanTransferMissionPassengers() || Aircraft->GetVelocityCmPerSec().Size() > 5 ||
		FVector::DistSquared2D(Here, Aircraft->GetActorLocation()) > FMath::Square((Traffic->GetPeopleWorldCmPerOriginalUnit() * 64.f) * 3.f) ||
		(bWasVisiting && (AmbientCuriosityAircraft.Get() != Aircraft ||
			FVector::DistSquared(AmbientCuriosityAircraftLocation, Aircraft->GetActorLocation()) > FMath::Square(20.f) ||
			FMath::Abs(FMath::FindDeltaAngleDegrees(AmbientCuriosityAircraftYaw, Aircraft->GetActorRotation().Yaw)) > 5)))
	{
		if (bWasVisiting) StopAmbientCuriosity();
		return false;
	}
	const float OldTimeLeft = AmbientCuriosityTimeLeft;
	AmbientCuriosityTimeLeft -= DeltaSeconds;
	if (!bWasVisiting || FMath::FloorToInt(OldTimeLeft * 2) != FMath::FloorToInt(AmbientCuriosityTimeLeft * 2))
	{
		int32 X, Y;
		ASimCopterMissionSystemActor::FServiceFireTarget Fire;
		const auto* Casualty = Traffic->FindNearestGawkCasualty(*this);
		auto* Missions = GetFireSafetyMissionSystem();
		if ((Casualty && FVector::DistSquared(Here, Casualty->GetActorLocation()) < FMath::Square((Traffic->GetPeopleWorldCmPerOriginalUnit() * 64.f) * 3.f)) ||
			(Missions && TryGetCurrentTileCoordinate(X,Y) && Missions->TryAcquireServiceFireTarget(FIntPoint(X,Y),12,Fire)))
		{
			if (bWasVisiting) StopAmbientCuriosity();
			return false;
		}
	}

	TArray<ASimCopterGroundAgent*> Visitors;
	TArray<ASimCopterGroundAgent*> Neighbors;
	bool UsedSlots[MaxVisitors] = {};
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It)
	{
		auto* Other = *It;
		if (Other == this || Other->IsHidden() || Other->IsActorBeingDestroyed() || Other->GetBehaviorCarrier() || Other->IsMissionCarried()) continue;
		if (Other->AmbientCuriositySlot != INDEX_NONE && Other->IsEligibleForAmbientCuriosity())
		{
			Visitors.Add(Other);
			if (Other->AmbientCuriositySlot < MaxVisitors) UsedSlots[Other->AmbientCuriositySlot] = true;
		}
		if (FMath::Abs(Other->GetActorLocation().Z - Here.Z) < 100) Neighbors.Add(Other);
	}
	const auto* Pilot = Cast<ASimCopterOnFootPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	const auto ClearOfPeople = [&](const FVector& From, const FVector& To)
	{
		if (Pilot && FMath::Abs(Pilot->GetActorLocation().Z - Here.Z) < 150 &&
			!SimCopterBandNavigation::KeepsDistance(From, To, Pilot->GetActorLocation(), WalkClearance)) return false;
		for (const auto* Other : Neighbors)
		{
			// Responders and moving people retain a broad walking lane. Visitors themselves
			// stand 110 cm apart, more than four scaled pedestrian body widths.
			const float Clearance = Other->AgentKind == ESimCopterGroundAgentKind::Vehicle ?
				Other->GetCollisionRadiusCm() + 80 : 65;
			if (!SimCopterBandNavigation::KeepsDistance(From,To,Other->GetActorLocation(),Clearance)) return false;
		}
		return true;
	};
	if (!bWasVisiting)
	{
		if (Visitors.Num() >= MaxVisitors) return false;
		const FBox Bounds = Aircraft->GetParkingWorldBounds();
		if (!Bounds.IsValid) return false;
		const FVector Forward = Aircraft->GetActorForwardVector().GetSafeNormal2D();
		const FVector Right(-Forward.Y,Forward.X,0);
		const FVector Extent = Bounds.GetExtent();
		const FVector Sides[] = {Forward, Right, -Right};
		for (int32 Side = 0; Side < 3 && AmbientCuriositySlot == INDEX_NONE; ++Side)
		{
			if (!Visitors.IsEmpty() && Side != Visitors[0]->AmbientCuriositySide) continue;
			const FVector Outward = Sides[Side];
			const FVector Along(-Outward.Y, Outward.X, 0);
			const float Radius = FMath::Abs(Outward.X)*Extent.X + FMath::Abs(Outward.Y)*Extent.Y;
			for (int32 Slot = 0; Slot < MaxVisitors; ++Slot)
			{
				if (UsedSlots[Slot]) continue;
				// One group beyond the rotor footprint. Prefer the nose, then a flank if the
				// base building blocks it; keep the tail/camera and both door lanes clear.
				FVector Goal = Bounds.GetCenter() + Outward*(Radius + 140) + Along*((Slot-1)*VisitorSpacing);
				Goal.Z = Here.Z;
				if (!ClearOfPeople(Here,Goal) || !CanWalkGroundSegment(Here,Goal,nullptr,&Goal) ||
					FMath::Abs(Goal.Z - GetCapsuleHalfHeightCm() - Aircraft->GetPassengerDropWorldLocation(0).Z) > 80) continue;
				bool bDoorsClear = true;
				for (int32 Door = 0; Door < 2; ++Door)
					bDoorsClear &= SimCopterBandNavigation::KeepsDistance(Here,Goal,Aircraft->GetPassengerDropWorldLocation(Door),WalkClearance);
				if (!bDoorsClear) continue;
				AmbientCuriosityAircraft = Aircraft;
				AmbientCuriosityAircraftLocation = Aircraft->GetActorLocation();
				AmbientCuriosityAircraftYaw = Aircraft->GetActorRotation().Yaw;
				AmbientCuriosityGoal = Goal;
				AmbientCuriositySlot = Slot;
				AmbientCuriositySide = Side;
				AmbientCuriosityTimeLeft = ApproachSeconds;
				AmbientCuriosityChatWait = Slot * 1.1f;
				AmbientCuriosityRemarks = 0;
				break;
			}
		}
		if (AmbientCuriositySlot == INDEX_NONE) return false;
	}

	// A pilot or responder stepping into the group breaks up that visitor's stop.
	// The short outward step is ground-checked; the normal car/hazard avoidance still wins.
	const AActor* YieldTo = Pilot && FMath::Abs(Pilot->GetActorLocation().Z - Here.Z) < 150 && FVector::DistSquared2D(Here,Pilot->GetActorLocation()) < FMath::Square(WalkClearance)
		? static_cast<const AActor*>(Pilot) : nullptr;
	for (const auto* Other : Neighbors)
		if (Other->IsEmergencyCrewMember() && FVector::DistSquared2D(Here,Other->GetActorLocation()) < FMath::Square(WalkClearance)) YieldTo = Other;
	if (YieldTo || AmbientCuriosityTimeLeft <= 0)
	{
		const FVector Away = (Here - (YieldTo ? YieldTo->GetActorLocation() : Aircraft->GetActorLocation())).GetSafeNormal2D();
		FVector Leave = Here + Away * 180;
		StopAmbientCuriosity();
		if (ClearOfPeople(Here,Leave) && CanWalkGroundSegment(Here,Leave,nullptr,&Leave))
			SetAvoidanceMoveTarget(Leave,2.f,0.6f);
		return false;
	}
	BehaviorStepTimeRemainingSeconds = 0;
	BehaviorStepVelocityCmPerSec = FVector::ZeroVector;
	const FVector ToGoal = AmbientCuriosityGoal - Here;
	if (ToGoal.Size2D() > 20)
	{
		const FVector Step = ToGoal.GetSafeNormal2D() * FMath::Min(float(ToGoal.Size2D()), 85.f * FMath::Min(DeltaSeconds,0.1f));
		if (!ClearOfPeople(Here,Here+Step) || !CanWalkGroundSegment(Here,Here+Step)) { StopAmbientCuriosity(); return false; }
		int32 Facing;
		if (TryGetBehaviorFacingOctantToward(AmbientCuriosityGoal,Facing)) BehaviorContext.Attributes[EBhavAttr::Facing] = uint16(Facing);
		BehaviorContext.PendingAnimMnemonic = TEXT("Walk");
		BehaviorStepVelocityCmPerSec = Step / FMath::Max(DeltaSeconds,UE_SMALL_NUMBER);
		BehaviorStepTimeRemainingSeconds = FMath::Max(DeltaSeconds,1.f/BehaviorTickRate) * 1.25f;
		return true;
	}
	if (!bAmbientCuriosityArrived)
	{
		bAmbientCuriosityArrived = true;
		AmbientCuriosityTimeLeft = VisitSeconds;
		BehaviorContext.PendingAnimMnemonic = TEXT("WvNo");
		int32 Facing;
		if (TryGetBehaviorFacingOctantToward(Aircraft->GetActorLocation(),Facing)) BehaviorContext.Attributes[EBhavAttr::Facing] = uint16(Facing);
	}
	AmbientCuriositySpeakingTime = FMath::Max(0.f,AmbientCuriositySpeakingTime - DeltaSeconds);
	AmbientCuriosityChatWait -= DeltaSeconds;
	if (AmbientCuriosityChatWait <= 0 && AmbientCuriosityRemarks < 2)
	{
		ASimCopterGroundAgent* Partner = nullptr;
		bool bSomeoneSpeaking = false;
		for (auto* Other : Visitors)
			if (Other->bAmbientCuriosityArrived && Other->AmbientCuriosityAircraft.Get() == Aircraft)
			{
				Partner = Other;
				bSomeoneSpeaking |= Other->AmbientCuriositySpeakingTime > 0;
			}
		if (!bSomeoneSpeaking)
		{
			// FUN_004c6970's actual 2Gab/HipH and nine original positional voice events.
			PlayCivilianConversation(BehaviorContext, AmbientCuriosityRemarks > 0 && Partner ? static_cast<AActor*>(Partner) : Aircraft);
			AmbientCuriositySpeakingTime = 2;
			AmbientCuriosityChatWait = 3.5f;
			++AmbientCuriosityRemarks;
		}
	}
	return true;
}


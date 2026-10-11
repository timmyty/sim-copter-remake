#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "City/SimCity2000CityActor.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

// User-requested remake autopilot. Never feed stale pilot controls into the fixed-point model.
void USimCopterAirOperationsComponent::BeginPilotRecovery(ASimCopterOnFootPawn* Pilot)
{
	auto* Heli=Helicopter();
	if (!Heli || !Pilot || Heli->IsFixedWingAircraft() || Heli->FlightModel.State==ESimCopterFlightState::Dying) return;
	EndRemoteTaser();
	Heli->ResetTransientInputState();
	RecoveryPilot=Pilot; RecoveryHoldLocation=Heli->GetActorLocation(); RecoveryHoldSeconds=2.0f;
	bHasRecoveryLandingGoal=false; RecoveryLandingSearchSeconds=0;
	RecoveryCableLength=0; bRecoveryRaising=false; bRecoveryPilotAttached=Pilot->IsAutoRescueAttached();
	RecoveryPassenger.Reset();
	bRecoveryHarnessActive=bRecoveryPilotAttached;
	if (bRecoveryPilotAttached)
	{
		bRecoveryRaising=true;
		RecoveryCableLength=FVector::Distance(Heli->GetCargoAnchorWorldLocation(),Pilot->GetActorLocation());
	}
	else if (Pilot->GetCharacterMovement()->MovementMode==MOVE_None) Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		if (It->GetBehaviorCarrier()==Heli && It->IsRidingHarness())
		{
			RecoveryPassenger=*It; bRecoveryRaising=bRecoveryHarnessActive=true;
			RecoveryCableLength=FVector::Distance(Heli->GetCargoAnchorWorldLocation(),It->GetActorLocation()); break;
		}
	Heli->SetAutopilotTransform(RecoveryHoldLocation,Heli->GetActorRotation(),.01f);
	Status=TEXT("AUTO HOVER: holding, then checking for a safe landing.");
	Heli->SetAirOperationsStatus(Status);
}

void USimCopterAirOperationsComponent::EndPilotRecovery()
{
	if (auto* Pilot=RecoveryPilot.Get()) Pilot->SetAutoRescueAttached(false);
	// An interrupted lift keeps the real passenger on the regular harness for manual retrieval.
	if (auto* Heli=Helicopter(); Heli && bRecoveryHarnessActive && !RecoveryPassenger.IsValid())
	{
		Heli->WinchState=SimCopterWinch::FWinchState();
		Heli->bRopeDeployed=false; Heli->bHarnessRiderAttached=false;
		Heli->RopeFirstActiveNode=SimCopterWinch::StowedNode;
		Heli->UpdateRopeVisuals(0);
	}
	RecoveryPilot.Reset(); RecoveryPassenger.Reset();
	RecoveryHoldSeconds=RecoveryCableLength=0;
	bHasRecoveryLandingGoal=false; RecoveryLandingSearchSeconds=0;
	bRecoveryRaising=bRecoveryPilotAttached=false;
	bRecoveryHarnessActive=false;
}

bool USimCopterAirOperationsComponent::FindRecoverySurface(FVector& OutGoal, bool& bOutCanLand) const
{
	return FindRecoverySurfaceAt(Helicopter()->GetActorLocation(),OutGoal,bOutCanLand);
}

bool USimCopterAirOperationsComponent::FindRecoverySurfaceAt(const FVector& Position, FVector& OutGoal, bool& bOutCanLand) const
{
	auto* Heli=Helicopter();
	const FVector From=Position;
	const FBox Body=Heli->GetParkingWorldBounds().ShiftBy(Position-Heli->GetActorLocation());
	if (!Body.IsValid) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(PilotRecoverySurface),true,Heli);
	if (auto* Pilot=RecoveryPilot.Get()) Query.AddIgnoredActor(Pilot);
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
	float Low=BIG_NUMBER, High=-BIG_NUMBER;
	bOutCanLand=true;
	int32 Hits=0;
	for (int32 X=-1; X<=1; ++X) for (int32 Y=-1; Y<=1; ++Y)
	{
		const FVector Probe(Body.GetCenter().X+X*Body.GetExtent().X,Body.GetCenter().Y+Y*Body.GetExtent().Y,Body.Min.Z+10);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit,Probe,Probe-FVector(0,0,100000),ECC_Camera,Query))
		{ bOutCanLand=false; continue; }
		++Hits; Low=FMath::Min(Low,float(Hit.ImpactPoint.Z)); High=FMath::Max(High,float(Hit.ImpactPoint.Z));
		bOutCanLand &= Hit.ImpactNormal.Z>=.98f;
		if (auto* City=Heli->ResolveCityActor())
		{
			float WaterZ; uint8 Class;
			if (City->TryGetWaterGameplaySurface(Hit.ImpactPoint,WaterZ,Class) && Class<10 && Hit.ImpactPoint.Z<WaterZ+15) bOutCanLand=false;
		}
	}
	if (!Hits) return false;
	bOutCanLand &= Hits==9 && High-Low<=16;
	// Keep the body above the highest supported point; a low hover is within jump/boarding reach.
	OutGoal=From; OutGoal.Z=High+(From.Z-Body.Min.Z)+(bOutCanLand?10.f:40.f);
	if (bOutCanLand)
	{
		const FBox Landing=Body.ShiftBy(OutGoal-From).ExpandBy(12);
		if (auto* Pilot=RecoveryPilot.Get(); Pilot && Landing.Intersect(Pilot->GetCapsuleComponent()->Bounds.GetBox())) bOutCanLand=false;
		for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		{
			if (It->IsHidden() || It->GetBehaviorCarrier()) continue;
			FBox Occupant(ForceInit);
			if (It->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle && It->TryGetBodyLocalBoundsCm(Occupant))
				Occupant=Occupant.TransformBy(It->GetActorTransform());
			else
			{
				const FVector Extent(It->GetCollisionRadiusCm(),It->GetCollisionRadiusCm(),It->GetCapsuleHalfHeightCm());
				Occupant=FBox(It->GetActorLocation()-Extent,It->GetActorLocation()+Extent);
			}
			if (Landing.Intersect(Occupant)) bOutCanLand=false;
		}
		// A wall, overhang or small obstacle can fit between the nine ground samples.
		bOutCanLand &= !GetWorld()->OverlapBlockingTestByChannel(Body.GetCenter()+OutGoal-From,FQuat::Identity,
			ECC_Visibility,FCollisionShape::MakeBox(Body.GetExtent()+FVector(2)),Query);
		if (!bOutCanLand) OutGoal.Z+=30;
	}
	return true;
}

bool USimCopterAirOperationsComponent::FindNearbyRecoveryLanding(FVector& OutGoal) const
{
	auto* Heli=Helicopter();
	const FVector From=Heli->GetActorLocation();
	const auto TryLanding=[&](FVector Position)
	{
		Position.Z=From.Z;
		FVector Goal; bool CanLand=false;
		if (!FindRecoverySurfaceAt(Position,Goal,CanLand) || !CanLand) return false;
		const double CruiseZ=FMath::Max(From.Z,Goal.Z);
		const FVector AboveFrom(From.X,From.Y,CruiseZ), AboveGoal(Goal.X,Goal.Y,CruiseZ);
		// Use the same full-airframe corridor as movement: climb, translate, then descend.
		if (!IsRecoveryRouteClear(From,AboveFrom) || !IsRecoveryRouteClear(AboveFrom,AboveGoal) ||
			!IsRecoveryRouteClear(AboveGoal,Goal)) return false;
		OutGoal=Goal; return true;
	};
	if (TryLanding(From)) return true;
	// Bounded search around the exit location, nearest rings first. Never chase a walking pilot.
	for (int32 Ring=1; Ring<=6; ++Ring)
		for (int32 Direction=0; Direction<16; ++Direction)
			if (TryLanding(RecoveryHoldLocation+FVector(300.f*Ring,0,0).RotateAngleAxis(Direction*22.5f,FVector::UpVector))) return true;
	return false;
}

bool USimCopterAirOperationsComponent::IsRecoveryRouteClear(const FVector& From, const FVector& To) const
{
	auto* Heli=Helicopter();
	const FBox Body=Heli->GetParkingWorldBounds();
	if (!Body.IsValid) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(PilotRecoveryClearance),true,Heli);
	if (auto* Pilot=RecoveryPilot.Get()) Query.AddIgnoredActor(Pilot);
	if (auto* Patient=RecoveryPassenger.Get()) Query.AddIgnoredActor(Patient);
	if (auto* Pilot=RecoveryPilot.Get(); Pilot && Pilot->GetCarriedMissionPerson()) Query.AddIgnoredActor(Pilot->GetCarriedMissionPerson());
	FHitResult Hit;
	const FVector Offset=Body.GetCenter()-Heli->GetActorLocation();
	const FCollisionShape Shape=FCollisionShape::MakeBox(Body.GetExtent()+FVector(2));
	bool Blocked=From.Equals(To,.01f)
		? GetWorld()->OverlapBlockingTestByChannel(To+Offset,FQuat::Identity,ECC_Visibility,Shape,Query)
		: GetWorld()->SweepSingleByChannel(Hit,From+Offset,To+Offset,FQuat::Identity,ECC_Visibility,Shape,Query);
	for (TActorIterator<ASimCopterHelicopterPawn> It(GetWorld()); !Blocked && It; ++It)
	{
		if (*It==Heli || It->IsHidden()) continue;
		const FBox Other=It->GetParkingWorldBounds();
		Blocked=Other.IsValid && FMath::LineBoxIntersection(Other.ExpandBy(Shape.GetExtent()),From+Offset,To+Offset,To-From);
	}
	return !Blocked;
}

bool USimCopterAirOperationsComponent::MoveRecoveryAircraft(const FVector& Goal, float DeltaSeconds)
{
	auto* Heli=Helicopter();
	const FVector From=Heli->GetActorLocation();
	FVector Route=Goal;
	if (FVector::Dist2D(From,Goal)>2)
	{
		Route.Z=FMath::Max(From.Z,Goal.Z);
		if (Goal.Z>From.Z+2) { Route.X=From.X; Route.Y=From.Y; }
	}
	const FVector Next=FMath::VInterpConstantTo(From,Route,FMath::Min(DeltaSeconds,.1f),100.f);
	const bool Blocked=!IsRecoveryRouteClear(From,Next);
	Heli->SetAutopilotTransform(Blocked?From:Next,Heli->GetActorRotation(),DeltaSeconds);
	return !Blocked && Next.Equals(Goal,2);
}

void USimCopterAirOperationsComponent::UpdatePilotRecovery(float DeltaSeconds)
{
	auto* Heli=Helicopter(); auto* Pilot=RecoveryPilot.Get();
	if (!Pilot || Pilot->IsActorBeingDestroyed() || Cast<APlayerController>(Heli->GetController()) || Pilot->GetParkedHelicopter()!=Heli ||
		Heli->FlightModel.State==ESimCopterFlightState::Dying || Heli->FlightModel.HitPoints<=0)
	{ EndPilotRecovery(); return; }
	if (RecoveryHoldSeconds>0)
	{
		RecoveryHoldSeconds=FMath::Max(0.f,RecoveryHoldSeconds-DeltaSeconds);
		Heli->SetAutopilotTransform(RecoveryHoldLocation,Heli->GetActorRotation(),DeltaSeconds);
		return;
	}
	if (bRecoveryHarnessActive) { UpdateRecoveryHarness(DeltaSeconds); return; }
	FVector Goal; bool CanLand=false;
	const bool Harness=(Heli->GetEquipmentState().CareerEquipmentMask & SimCopterHelicopterRegistry::GetToolCareerBit(ESimCopterHelicopterTool::RescueHarness))!=0;
	if (!Harness)
	{
		RecoveryLandingSearchSeconds=FMath::Max(0.f,RecoveryLandingSearchSeconds-DeltaSeconds);
		if (bHasRecoveryLandingGoal)
		{
			FVector Probe=RecoveryLandingGoal; Probe.Z=Heli->GetActorLocation().Z;
			if (!FindRecoverySurfaceAt(Probe,Goal,CanLand) || !CanLand || FMath::Abs(Goal.Z-RecoveryLandingGoal.Z)>16)
			{ bHasRecoveryLandingGoal=false; RecoveryLandingSearchSeconds=0; }
		}
		if (!bHasRecoveryLandingGoal && RecoveryLandingSearchSeconds<=0)
		{
			bHasRecoveryLandingGoal=FindNearbyRecoveryLanding(RecoveryLandingGoal);
			RecoveryLandingSearchSeconds=2.f;
		}
		if (!bHasRecoveryLandingGoal)
		{
			Status=TEXT("AUTO HOVER: searching for a clear nearby landing spot."); Heli->SetAirOperationsStatus(Status);
			Heli->SetAutopilotTransform(Heli->GetActorLocation(),Heli->GetActorRotation(),DeltaSeconds); return;
		}
		Goal=RecoveryLandingGoal; CanLand=true;
	}
	else if (!FindRecoverySurface(Goal,CanLand))
	{ Heli->SetAutopilotTransform(Heli->GetActorLocation(),Heli->GetActorRotation(),DeltaSeconds); return; }
	if (!CanLand && Harness && !Pilot->GetCharacterMovement()->IsFalling() && FVector::Dist2D(Pilot->GetActorLocation(),Heli->GetActorLocation())<1800)
	{
		bRecoveryHarnessActive=true;
		UpdateRecoveryHarness(DeltaSeconds); return;
	}
	Status=CanLand?TEXT("AUTO LAND: moving to clear ground."):TEXT("AUTO HOVER: uneven ground - jump toward the cabin to board.");
	Heli->SetAirOperationsStatus(Status);
	const FVector Before=Heli->GetActorLocation();
	if (MoveRecoveryAircraft(Goal,DeltaSeconds) && CanLand)
	{
		EndPilotRecovery();
		Heli->bEngineRunning=false; Heli->bIsLanded=true;
		Heli->FlightModel.State=ESimCopterFlightState::Parked;
		Heli->VelocityCmPerSec=FVector::ZeroVector;
		Heli->FlightModel.VelX=Heli->FlightModel.VelZ=0;
	}
	else if (!Harness && DeltaSeconds>0 && Heli->GetActorLocation().Equals(Before,.01f))
	{
		// A new moving obstruction invalidates the cached approach; choose another safe spot.
		bHasRecoveryLandingGoal=false; RecoveryLandingSearchSeconds=0;
	}
}

void USimCopterAirOperationsComponent::UpdateRecoveryHarness(float DeltaSeconds)
{
	auto* Heli=Helicopter(); auto* Pilot=RecoveryPilot.Get();
	if (!Pilot) return;
	if (bRecoveryRaising || Pilot->GetCharacterMovement()->IsFalling())
		Heli->SetAutopilotTransform(Heli->GetActorLocation(),Heli->GetActorRotation(),DeltaSeconds);
	// Align the actual bracket, not the aircraft centre, over the person being recovered.
	Heli->bHarnessRopeEndSelected=true;
	FVector Anchor=Heli->GetCargoAnchorWorldLocation();
	FVector Target=RecoveryPassenger.IsValid()?RecoveryPassenger->GetActorLocation():Pilot->GetActorLocation();
	if (!bRecoveryRaising)
	{
		if (auto* Carried=Pilot->GetCarriedMissionPerson()) Target=Carried->GetActorLocation();
		if (Pilot->GetCharacterMovement()->IsFalling()) return;
		FVector Goal=Heli->GetActorLocation()+FVector(Target.X-Anchor.X,Target.Y-Anchor.Y,0);
		FVector SurfaceGoal; bool CanLand;
		if (FindRecoverySurface(SurfaceGoal,CanLand)) Goal.Z=FMath::Max(SurfaceGoal.Z+160.f,Target.Z+240.f);
		if (!MoveRecoveryAircraft(Goal,DeltaSeconds)) return;
		Anchor=Heli->GetCargoAnchorWorldLocation();
	}
	const float Wanted=bRecoveryRaising?0.f:FMath::Clamp(float(Anchor.Z-Target.Z),0.f,SimCopterAirOperations::MaxCableCm);
	const float NextLength=FMath::FInterpConstantTo(RecoveryCableLength,Wanted,DeltaSeconds,85.f);
	const FVector End=Anchor-FVector(0,0,NextLength);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AutoRescueCable),true,Heli);
	Query.AddIgnoredActor(Pilot);
	if (auto* Carried=Pilot->GetCarriedMissionPerson()) Query.AddIgnoredActor(Carried);
	if (auto* Patient=RecoveryPassenger.Get()) Query.AddIgnoredActor(Patient);
	FHitResult Hit;
	// Check the whole cable and a body-sized lift corridor before moving either person.
	if (GetWorld()->SweepSingleByChannel(Hit,Anchor,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(14),Query))
	{ Status=TEXT("AUTO RESCUE: cable path obstructed - holding."); Heli->SetAirOperationsStatus(Status); return; }
	RecoveryCableLength=NextLength;
	Heli->InitializeRopeState();
	Heli->bRopeDeployed=true; Heli->WinchState.bHarnessStowed=false; Heli->WinchState.bBucketStowed=true;
	Heli->WinchState.RopeEnd=SimCopterWinch::ERopeEnd::Harness;
	Heli->RopeFirstActiveNode=Heli->WinchState.NodeCursor=SimCopterWinch::LoweredNode;
	const int32 First=Heli->RopeFirstActiveNode, Last=Heli->RopeNodeWorldPositions.Num()-1;
	for (int32 I=0; I<=Last; ++I)
		Heli->RopeNodeWorldPositions[I]=FMath::Lerp(Anchor,End,FMath::Clamp(float(I-First)/float(Last-First),0.f,1.f));
	// The automatic winch is kinematic and swept above. Use the real rope and harness artwork.
	Heli->SmoothedRopeOffsets.Reset(); Heli->UpdateRopeVisuals(0);
	if (!bRecoveryRaising && FVector::Distance(End,Target)<24)
	{
		if (auto* Carried=Pilot->GetCarriedMissionPerson())
		{
			if (!Carried->BoardCarrier(Heli,true,true))
			{ Status=TEXT("AUTO RESCUE: free a passenger seat for the carried person."); Heli->SetAirOperationsStatus(Status); return; }
			Pilot->ConsumeCarriedMissionPerson(); RecoveryPassenger=Carried;
		}
		else { Pilot->SetAutoRescueAttached(true); bRecoveryPilotAttached=true; }
		bRecoveryRaising=true; Heli->bHarnessRiderAttached=true;
	}
	if (bRecoveryPilotAttached) Pilot->SetActorLocation(End,false,nullptr,ETeleportType::TeleportPhysics);
	if (RecoveryPassenger.IsValid()) RecoveryPassenger->SetActorLocation(End,false,nullptr,ETeleportType::TeleportPhysics);
	Status=RecoveryPassenger.IsValid()?TEXT("AUTO RESCUE: lifting passenger first."):
		bRecoveryPilotAttached?TEXT("AUTO RESCUE: lifting pilot to cockpit."):TEXT("AUTO RESCUE: lowering harness.");
	Heli->SetAirOperationsStatus(Status);
	if (bRecoveryRaising && RecoveryCableLength<=2)
	{
		if (auto* Patient=RecoveryPassenger.Get())
		{
			if (!Patient->BoardCarrier(Heli,false,true,Patient->IsMissionPatientDead())) return;
			RecoveryPassenger.Reset(); bRecoveryRaising=false; Heli->bHarnessRiderAttached=false;
		}
		else if (auto* PC=Cast<APlayerController>(Pilot->GetController()))
		{
			EndPilotRecovery(); Heli->EnterHelicopter(PC);
		}
	}
}

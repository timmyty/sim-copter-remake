#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Components/CapsuleComponent.h"
#include "Missions/SimCopterMissionSystemActor.h"

bool ASimCopterHelicopterPawn::IsSupportAircraft() const { return AirOperations && AirOperations->IsSupportAircraft(); }

void ASimCopterHelicopterPawn::SetAutopilotTransform(const FVector& Location,const FRotator& Rotation,float DeltaSeconds)
{
	const FVector Previous=GetActorLocation();
	SetActorLocationAndRotation(Location,Rotation,false,nullptr,ETeleportType::TeleportPhysics);
	VelocityCmPerSec=(Location-Previous)/FMath::Max(DeltaSeconds,0.001f);
	const float Unit=FMath::Max(OriginalUnitToCm,0.01f);
	const float HalfHeight=CollisionComponent?CollisionComponent->GetScaledCapsuleHalfHeight():0;
	FlightModel.PosX=SimCopterFixed::FromFloat(Location.Y/Unit);
	FlightModel.PosZ=SimCopterFixed::FromFloat(Location.X/Unit);
	FlightModel.Altitude=SimCopterFixed::FromFloat((Location.Z-HalfHeight)/Unit);
	FlightModel.Heading=SimCopterFixed::WrapAngle(SimCopterFixed::FromFloat(FRotator::ClampAxis(Rotation.Yaw)*10));
	FlightModel.State=ESimCopterFlightState::Flying;
	FlightModel.RotorSpeed=FSimCopterFlightModel::RotorTopSpeed;
	FlightModel.bRotorBlurDisc=true;
	FlightModel.MainRotorAngle=SimCopterFixed::WrapAngle(FlightModel.MainRotorAngle+SimCopterFixed::FromFloat(9000*DeltaSeconds));
	FlightModel.TailRotorAngle=FlightModel.MainRotorAngle;
	FlightModel.PitchTarget=FlightModel.PitchSmoothed=0;
	FlightModel.BankTarget=FlightModel.BankSmoothed=0;
	FlightModel.SlideTarget=FlightModel.SlideSmoothed=0;
	FlightModel.YawRateTarget=FlightModel.YawRateSmoothed=0;
	FlightModel.VelX=SimCopterFixed::FromFloat(VelocityCmPerSec.Y/Unit);
	FlightModel.VelZ=SimCopterFixed::FromFloat(VelocityCmPerSec.X/Unit);
	FlightModel.ForwardSpeed=0;
	bFlightModelSeeded=true; bEngineRunning=true; bIsLanded=false;
	CurrentPitchDeg=CurrentRollDeg=0;
	UpdateGroundProbe();
}

void ASimCopterHelicopterPawn::ControllerLeftBumperPressed()
{
	bBumperLeftHeld=true;
	if(bBumperRightHeld && !bBumperChordLatched)
	{
		bBumperChordLatched=true;
		ControllerToolWheelPressed();
	}
	else if(!bBumperChordLatched && ControllerMode==ESimCopterControllerMode::None) RememberExitSide(-1);
}
void ASimCopterHelicopterPawn::ControllerLeftBumperReleased()
{
	if(bBumperChordLatched && bBumperLeftHeld && bBumperRightHeld)
	{
		if(ControllerMode==ESimCopterControllerMode::ToolWheel) ControllerToolWheelReleased();
		else if(ControllerMode==ESimCopterControllerMode::DispatchWheel) ControllerDispatchWheelReleased();
	}
	bBumperLeftHeld=false;
	if(!bBumperRightHeld) bBumperChordLatched=false;
}
void ASimCopterHelicopterPawn::ControllerRightBumperPressed()
{
	bBumperRightHeld=true;
	if(bBumperLeftHeld && !bBumperChordLatched)
	{
		bBumperChordLatched=true;
		ControllerToolWheelPressed();
	}
	else if(!bBumperChordLatched && ControllerMode==ESimCopterControllerMode::None) RememberExitSide(1);
}
void ASimCopterHelicopterPawn::ControllerRightBumperReleased()
{
	if(bBumperChordLatched && bBumperLeftHeld && bBumperRightHeld)
	{
		if(ControllerMode==ESimCopterControllerMode::ToolWheel) ControllerToolWheelReleased();
		else if(ControllerMode==ESimCopterControllerMode::DispatchWheel) ControllerDispatchWheelReleased();
	}
	bBumperRightHeld=false;
	if(!bBumperLeftHeld) bBumperChordLatched=false;
}
void ASimCopterHelicopterPawn::RequestAirSupport(bool bAutomatic)
{
	if (IsFixedWingAircraft()) { LastToolStatus=TEXT("Transport only - board a helicopter for emergency response."); return; }
	auto* Ops=GetWorld()?GetWorld()->GetSubsystem<USimCopterAirOperationsSubsystem>():nullptr;
	auto* Missions=ResolveMissionSystem();
	int32 EventId=INDEX_NONE;
	if(Missions)
	{
		const auto& Records=Missions->GetMissionRecords();
		const int32 Selected=Missions->GetMapFocusRecordIndex();
		if(Records.IsValidIndex(Selected)) EventId=Records[Selected].EventId;
	}
	if(Ops && Ops->OrderSupport(EventId,bAutomatic))
		LastToolStatus=bAutomatic?TEXT("Air Support: automatic mission response enabled."):TEXT("Air Support assigned to the mission selected on the map.");
	else LastToolStatus=Ops && !Ops->IsSupportUnlocked()?TEXT("Pass a level to unlock Air Support."):TEXT("Select an active map mission. Air Support must finish its current passenger/cargo delivery first.");
	if(Missions) Missions->ShowAirOperationsMessage(LastToolStatus);
}
void ASimCopterHelicopterPawn::TogglePoliceTaser()
{
	if (IsFixedWingAircraft()) return;
	StopPrimaryToolUse(); // Clear held fire AND a queued projectile press before changing roles.
	AirOperations->TogglePoliceTaser();
	RefreshCrosshairVisibility();
}

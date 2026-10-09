#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"

bool ASimCopterHelicopterPawn::CanAcceptPassenger(ESimCopterMissionPassengerKind Kind, const ASimCopterGroundAgent* Person) const
{
	if (Person && Person->IsCow()) return false;
	if (!IsFixedWingAircraft()) return true;
	return Kind == ESimCopterMissionPassengerKind::Transport &&
		(!Person || (Person->GetBehaviorAttribute(EBhavAttr::State) == 4 && Person->MissionEventId != INDEX_NONE &&
		!Person->IsMissionPatientDead() && !Person->IsMedevacVictim() && !Person->IsInPoliceCustody()));
}

void ASimCopterHelicopterPawn::StepFixedWing(float Dt, const FSimCopterFlightInputs& Inputs,
	const FSimCopterFlightEnvironment& Environment)
{
	using namespace SimCopterFixed;
	// Deliberate arcade plane model. Shared fixed-point state retains save, collision,
	// fuel, damage, camera and passenger integration without granting rotor lift/hover.
	if (FlightModel.State == ESimCopterFlightState::Dying || FlightModel.State == ESimCopterFlightState::Dead || FlightModel.HitPoints <= 0)
	{
		FlightModel.Step(Dt, Inputs, Environment, LastFlightEvents);
		return;
	}
	LastFlightEvents = FSimCopterFlightEvents();
	const float Unit = FMath::Max(OriginalUnitToCm, 0.01f);
	constexpr float PositionScale = 40000.0f / 65536.0f;
	const bool bOnGround = FlightModel.State == ESimCopterFlightState::Parked;
	const float Pitch = FMath::Clamp(Inputs.PitchAxis / 100.0f +
		(Inputs.bPitchForwardKey ? 1.0f : 0.0f) - (Inputs.bPitchBackKey ? 1.0f : 0.0f), -1.0f, 1.0f);
	const float Turn = FMath::Clamp(Inputs.TurnAxis / 100.0f + Inputs.SlideAxis / 100.0f +
		(Inputs.bTurnRightKey || Inputs.bSlideRightKey ? 1.0f : 0.0f) -
		(Inputs.bTurnLeftKey || Inputs.bSlideLeftKey ? 1.0f : 0.0f), -1.0f, 1.0f);
	const bool bPowered = bEngineRunning && FlightModel.Fuel > 0;
	const bool bBraking = Pitch < -0.15f || (bOnGround && Inputs.ClimbCommand < 0);
	const float TargetSpeed = !bPowered ? (bOnGround ? 0 : 650) : bBraking ? (bOnGround ? 0 : 650) :
		Pitch > 0.15f || (bOnGround && Inputs.ClimbCommand > 0) ? 3200 : bOnGround ? 0 : 1800;
	float Speed = FMath::FInterpConstantTo(ToFloat(FlightModel.ForwardSpeed) * PositionScale * Unit,
		TargetSpeed, Dt, bBraking ? 1300 : 1100);
	float Heading = ToFloat(FlightModel.Heading) / 10;
	Heading = FRotator::ClampAxis(Heading + Turn * (bOnGround ? 70 : 105) * Dt);
	FlightModel.Heading = FromFloat(Heading * 10);
	const float Bank = bOnGround ? 0 : -Turn * 42;
	FlightModel.BankSmoothed = FromFloat(Bank * 10);
	FlightModel.BankTarget = FlightModel.BankSmoothed;
	FlightModel.SlideTarget = FlightModel.SlideSmoothed = 0;
	float Climb = bPowered ? Inputs.ClimbCommand * Speed * 0.38f : -260;
	if (bOnGround) Climb = bPowered && Speed >= 600 && Inputs.ClimbCommand > 0 ? Speed * 0.38f : 0;
	FlightModel.PitchTarget = FlightModel.PitchSmoothed = FromFloat(-FMath::RadiansToDegrees(FMath::Atan2(Climb, FMath::Max(Speed,1.0f))) * 10);
	FlightModel.BounceTimer = FMath::Max(0, FlightModel.BounceTimer - FromFloat(Dt));
	if (FlightModel.BounceTimer > 0)
	{
		Speed = FMath::Min(Speed, 450.0f);
		Climb = 180;
		FlightModel.Heading = WrapAngle(FlightModel.Heading + FromFloat(1200 * Dt));
		Heading = ToFloat(FlightModel.Heading) / 10;
	}
	float Altitude = ToFloat(FlightModel.Altitude) * Unit + Climb * Dt;
	const float Surface = ToFloat(Environment.SurfaceHeight) * Unit + 1.2f * Unit;
	if (Altitude <= Surface && Climb <= 0)
	{
		Altitude = Surface;
		if (!bOnGround)
		{
			LastFlightEvents.bTouchedDown = true;
			if ((Speed > 1300 || Climb < -500) && !FlightModel.bCheatInvulnerable)
			{
				LastFlightEvents.DamageTaken = FMath::Min(FlightModel.HitPoints, 70);
				FlightModel.HitPoints -= LastFlightEvents.DamageTaken;
				LastFlightEvents.bGroundBounce = true;
			}
		}
		FlightModel.State = ESimCopterFlightState::Parked;
		Climb = 0;
		if (Environment.bHostileSurface && !FlightModel.bCheatInvulnerable)
		{
			LastFlightEvents.bTouchedDown=false;
			LastFlightEvents.bSplashBounce=true;
			LastFlightEvents.DamageTaken=FlightModel.HitPoints;
			FlightModel.HitPoints=0;
			FlightModel.State=ESimCopterFlightState::Dying;
			LastFlightEvents.bStartedDying=true;
		}
	}
	else if (Climb > 0 || !bOnGround)
	{
		LastFlightEvents.bLiftedOff = bOnGround;
		FlightModel.State = ESimCopterFlightState::Flying;
	}
	const FVector Direction = FRotator(0, Heading, 0).Vector();
	FlightModel.PosZ += FromFloat(Direction.X * Speed * Dt / Unit);
	FlightModel.PosX += FromFloat(Direction.Y * Speed * Dt / Unit);
	FlightModel.Altitude = FromFloat(Altitude / Unit);
	FlightModel.AboveGround = FlightModel.Altitude - Environment.TerrainHeight;
	FlightModel.ForwardSpeed = FlightModel.HorizontalSpeed = FromFloat(Speed / (PositionScale * Unit));
	FlightModel.VelZ = FromFloat(Direction.X * Speed / (PositionScale * Unit));
	FlightModel.VelX = FromFloat(Direction.Y * Speed / (PositionScale * Unit));
	FlightModel.ClimbSpeed = FromFloat(Climb / Unit);
	FlightModel.RotorSpeed = bPowered ? FromFloat(350) : 0; // engine audio/HUD proxy; no rotor meshes
	FlightModel.FlightSeconds += FromFloat(Dt);
	if (bPowered && !FlightModel.bCheatInfiniteFuel) FlightModel.Fuel = FMath::Max(0, FlightModel.Fuel - FromFloat(HelicopterTuning.FuelRateGallonsPerHour * Dt / 3600));
}

void ASimCopterHelicopterPawn::EnsureTransportPlane(UWorld* World)
{
	if (!World) return;
	for (TActorIterator<ASimCopterHelicopterPawn> It(World); It; ++It)
		if (It->IsFixedWingAircraft() && !It->IsActorBeingDestroyed()) return;
	TActorIterator<ASimCopterTrafficSystemActor> Traffic(World);
	if (!Traffic) return;
	for (int32 Pad = SimCopterAirport::PadCount - 1; Pad >= 0; --Pad)
	{
		FVector Position;
		if (!Traffic->TryGetAirportPadWorldLocation(Pad, Position)) continue;
		bool bOccupied = false;
		for (TActorIterator<ASimCopterHelicopterPawn> It(World); It; ++It)
			if (FVector::Dist2D(It->GetActorLocation(), Position) < 240) { bOccupied = true; break; }
		if (bOccupied) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Plane = World->SpawnActor<ASimCopterHelicopterPawn>(StaticClass(), Position, FRotator::ZeroRotator, Params);
		if (!Plane) return;
		if (!Plane->SwitchHelicopterModel(SimCopterHelicopterRegistry::PlaneTypeIndex)) { Plane->Destroy(); return; }
		Plane->PlaceOnHelipad(Position, 0);
		Plane->SetAirOperationsStatus(TEXT("Transport plane: one passenger. Accelerate, climb to take off; brake and descend to land."));
		return;
	}
}

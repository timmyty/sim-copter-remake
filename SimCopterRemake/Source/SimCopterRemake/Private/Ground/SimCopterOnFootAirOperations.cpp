#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "City/SimCity2000CityActor.h"
#include "Ground/SimCopterParticleFX.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Ground/SimCopterPilotEquipment.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "City/SimCopterEffectExposure.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void ASimCopterOnFootPawn::SetAutoRescueAttached(bool bAttached)
{
	if (bAutoRescueAttached==bAttached) return;
	bAutoRescueAttached=bAttached;
	SetParachuteDeployed(false); StopTaserAim(); StopJumping();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(bAttached?MOVE_None:MOVE_Falling);
	FallPeakZ=GetActorLocation().Z;
}

void ASimCopterOnFootPawn::SetParachuteDeployed(bool bDeployed)
{
	bParachuteDeployed = bDeployed;
	GetCharacterMovement()->GravityScale = bDeployed ? 0.1f : GravityScale;
	if (bDeployed)
	{
		StopTaserAim();
		if (!ParachuteMesh)
		{
			ParachuteMesh = NewObject<UProceduralMeshComponent>(this,TEXT("Parachute"));
			AddInstanceComponent(ParachuteMesh); ParachuteMesh->SetupAttachment(RootComponent); ParachuteMesh->RegisterComponent();
			FMaxisMeshSection Shape; SimCopterPilotEquipment::BuildParachute(Shape);
			ParachuteMesh->CreateMeshSection_LinearColor(0,Shape.Vertices,Shape.Triangles,Shape.Normals,Shape.UVs,Shape.VertexColors,Shape.Tangents,false);
			ParachuteMesh->SetMaterial(0,BodyVertexColorMaterial);
			ParachuteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (ParachuteMesh) ParachuteMesh->SetVisibility(bDeployed);
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = bDeployed ? 235.0f : 130.0f;
		CameraBoom->TargetOffset.Z = bDeployed ? 50.0f : 10.0f;
	}
}

void ASimCopterOnFootPawn::ToggleParachute()
{
	if (bAutoRescueAttached || PilotHealth <= 0 || !GetCharacterMovement()->IsFalling()) return;
	SetParachuteDeployed(!bParachuteDeployed);
}

void ASimCopterOnFootPawn::StartTaserAim()
{
	if (bAutoRescueAttached || PilotHealth <= 0 || IsCarryingMissionPerson() || bParachuteDeployed) return;
	bTaserAiming=true;
	if(!TaserMesh && CameraComponent)
	{
		TaserMesh=NewObject<UProceduralMeshComponent>(this,TEXT("Taser"));
		AddInstanceComponent(TaserMesh); TaserMesh->SetupAttachment(RootComponent); TaserMesh->RegisterComponent();
		FMaxisMeshSection Shape; SimCopterPilotEquipment::BuildTaser(Shape);
		TaserMesh->CreateMeshSection_LinearColor(0,Shape.Vertices,Shape.Triangles,Shape.Normals,Shape.UVs,Shape.VertexColors,Shape.Tangents,false);
		TaserMesh->SetMaterial(0,BodyVertexColorMaterial);
		TaserGlow=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_SimCopterParticleFX.M_SimCopterParticleFX")),this);
		TaserMesh->SetMaterial(1,TaserGlow); TaserMesh->SetMaterial(2,TaserGlow);
		TaserMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TaserMesh->SetRelativeLocation(FVector(8,5,7));
		TaserMesh->SetRelativeScale3D(FVector(0.25));
	}
	if(TaserMesh) TaserMesh->SetVisibility(true);
	UpdateTaserPose(0.0f);
	UpdateEnergyTaser(0.0f);
}
void ASimCopterOnFootPawn::StopTaserAim()
{
	bTaserAiming=false;
	TaserCharge.Update(0,false);
	StopTaserHum();
	if(TaserMesh) TaserMesh->SetVisibility(false);
}
void ASimCopterOnFootPawn::FireTaser()
{
	if(!bTaserAiming || TaserCooldown>0 || !CameraComponent || !TaserCharge.CanFire() || PilotHealth<=0 || IsCarryingMissionPerson()) return;
	// Re-evaluate on the click as well as the tick: turning off target cannot release a shot.
	auto* Target=SimCopterEnergyTaser::FindTarget(GetWorld(),this,CameraComponent->GetComponentLocation(),CameraComponent->GetForwardVector());
	if (!Target) { UpdateEnergyTaser(0); return; }
	const FVector Muzzle=TaserMesh->GetComponentTransform().TransformPosition(FVector(14,0,2));
	auto* Orb=GetWorld()->SpawnActor<ASimCopterTaserOrb>();
	if (!Orb) return;
	TaserCooldown=1.5f;
	Orb->Launch(this,Target,Muzzle,CameraComponent->GetForwardVector());
	TaserCharge.Consume(); StopTaserHum(); UpdateEnergyTaser(0);
}

void ASimCopterOnFootPawn::StopTaserHum()
{
	if (auto* Audio=USimCopterAudioSubsystem::Get(this)) Audio->StopAttachedVoiceLoop(TaserHum.Get());
	TaserHum.Reset();
}

void ASimCopterOnFootPawn::UpdateEnergyTaser(float DeltaSeconds)
{
	const bool bCanSight=bTaserAiming && CameraComponent && PilotHealth>0 && !bParachuteDeployed && !bAutoRescueAttached && !IsCarryingMissionPerson();
	const bool bSighted=bCanSight && SimCopterEnergyTaser::FindTarget(GetWorld(),this,
		CameraComponent->GetComponentLocation(),CameraComponent->GetForwardVector());
	TaserCharge.Update(DeltaSeconds,bSighted);
	TaserEffectPhase+=DeltaSeconds;
	if (!TaserMesh) return;
	if (auto* Audio=USimCopterAudioSubsystem::Get(this))
	{
		if (bCanSight && TaserCharge.Energy>.001f)
		{
			if (!TaserHum.IsValid()) TaserHum=Audio->PlayTaserSound(ESimCopterTaserSound::Charge,TaserMesh);
			if (TaserHum.IsValid())
			{
				TaserHum->SetPitchMultiplier(.7f+TaserCharge.Energy*.5f);
				TaserHum->SetVolumeMultiplier((.08f+.22f*TaserCharge.Energy)*TaserCharge.Energy*USimCopterAudioSubsystem::VolumeIndexToGain(Audio->GetMasterVolume()));
			}
		}
		else StopTaserHum();
	}
	if (!bTaserAiming) return;
	FMaxisMeshSection Body,Circuits,Ball;
	SimCopterPilotEquipment::BuildTaser(Body,TaserCharge.Deployment);
	SimCopterPilotEquipment::BuildTaserCircuits(Circuits,TaserCharge.Deployment,TaserCharge.Energy,TaserEffectPhase);
	SimCopterPilotEquipment::BuildTaserOrb(Ball,4.f*TaserCharge.Energy*(.97f+.03f*FMath::Sin(TaserEffectPhase*9)),TaserEffectPhase,TaserCharge.Energy);
	for (auto& V:Ball.Vertices) V+=FVector(14,0,2);
	const auto Apply=[&](int32 Section,const FMaxisMeshSection& Shape)
	{
		if (Shape.Vertices.IsEmpty()) TaserMesh->ClearMeshSection(Section);
		else TaserMesh->CreateMeshSection_LinearColor(Section,Shape.Vertices,Shape.Triangles,Shape.Normals,Shape.UVs,Shape.VertexColors,Shape.Tangents,false);
	};
	Apply(0,Body); Apply(1,Circuits); Apply(2,Ball);
	USimCopterEffectExposureSubsystem::ApplyEmissiveNits(TaserGlow,GetWorld(),true,.08f+TaserCharge.Energy*1.3f);
}
bool ASimCopterOnFootPawn::TryCarryDownedPerson()
{
	// Putting someone down is deliberate. Require walking away and returning before the
	// proximity pickup can grab that same person, even after the short drop cooldown ends.
	if (LastPutDownPerson.IsValid() && FVector::Dist2D(GetActorLocation(),LastPutDownPerson->GetActorLocation())>140)
		LastPutDownPerson.Reset();
	if(bAutoRescueAttached || IsCarryingMissionPerson() || !CanPickUpMissionPersonNow() || PilotHealth<=0 || GetCharacterMovement()->IsFalling()) return false;
	ASimCopterGroundAgent* Best=nullptr; float Distance=90;
	for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
	{
		const bool bDowned=It->IsInPoliceCustody() || It->IsMedevacVictim() || It->IsMissionPatientDead();
		const float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation());
		if(*It!=LastPutDownPerson.Get() && It->GetAgentKind()==ESimCopterGroundAgentKind::Pedestrian && bDowned &&
			!It->IsActorBeingDestroyed() && !It->IsHidden() &&
			(!It->HasMissionResolutionReported() || It->IsMissionPatientDead()) &&
			!It->IsMissionCarried() && !It->GetBehaviorCarrier() && D<Distance &&
			FMath::Abs(GetActorLocation().Z-It->GetActorLocation().Z)<90)
		{
			FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(CarrySuspect),true,this); Query.AddIgnoredActor(*It);
			if(!GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation(),It->GetActorLocation(),ECC_Visibility,Query)) { Best=*It; Distance=D; }
		}
	}
	if(Best) { StopTaserAim(); return PickUpMissionPerson(Best); }
	return false;
}
void ASimCopterOnFootPawn::BeginAirborneExit(const FVector& Location,const FVector& Velocity)
{
	SetParachuteDeployed(false);
	SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);
	FallPeakZ=Location.Z;
	MissionPickupCooldownSeconds=2;
	if(auto* Move=GetCharacterMovement()) { Move->SetMovementMode(MOVE_Falling); Move->Velocity=Velocity; }
}
void ASimCopterOnFootPawn::TeleportForCheat(const FVector& Location)
{
	SetParachuteDeployed(false);
	StopJumping();
	GetCharacterMovement()->StopMovementImmediately();
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	FallPeakZ = Location.Z;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}
void ASimCopterOnFootPawn::Landed(const FHitResult& Hit)
{
	const float Distance=FMath::Max(0.0f,FallPeakZ-float(GetActorLocation().Z));
	bool Water=false;
	if(auto* City=ResolveCityActor())
	{
		float Z=0; uint8 Class=255;
		Water=City->TryGetWaterGameplaySurface(GetActorLocation(),Z,Class) && Class<10 && GetActorLocation().Z-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()<Z+40;
	}
	// Opening just above the ground is not instant immunity: the canopy must slow the descent.
	const bool bSoftParachuteLanding = bParachuteDeployed && GetCharacterMovement()->Velocity.Z >= -250.0f;
	if(!bSoftParachuteLanding && SimCopterAirOperations::FallCausesInjury(Distance,Water))
	{
		if (PilotHealth>0) if (auto* Audio=USimCopterAudioSubsystem::Get(this)) Audio->PlayPilotFallInjury(Distance>=1800.f,GetActorLocation());
		// Falls have a per-impact outcome; minor falls never accumulate hidden health damage.
		PilotHealth = Distance >= 1800.f ? 0.0f : 100.0f;
		InjurySeconds=PilotHealth<=0?5.0f:2.0f;
		if(auto* Missions=Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(GetWorld(),ASimCopterMissionSystemActor::StaticClass())))
			Missions->ShowAirOperationsMessage(PilotHealth <= 0 ? TEXT("Fatal fall. Recovering at your helicopter.") : TEXT("Hard landing!"));
	}
	FallPeakZ=GetActorLocation().Z;
	SetParachuteDeployed(false);
	Super::Landed(Hit);
}
void ASimCopterOnFootPawn::UpdateAirOperations(float DeltaSeconds)
{
	if (PilotHealth <= 0)
	{
		// Fatal accidents respawn the player in the cockpit. Keep this independent of
		// contact boarding and autopilot pickup, including a saved/expired countdown.
		SetParachuteDeployed(false);
		StopTaserAim(); StopJumping();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->MaxWalkSpeed = 0;
		InjurySeconds = FMath::Max(0.0f, InjurySeconds - DeltaSeconds);
		if (InjurySeconds > 0) return;
		auto* PlayerController = Cast<APlayerController>(GetController());
		if (!PlayerController || !IsValid(ParkedHelicopter)) return;

		if (CarriedMissionPerson.IsValid()) DropCarriedMissionPerson();
		ParkedHelicopter->GetAirOperations()->EndPilotRecovery();
		const auto& Flight = ParkedHelicopter->GetFlightModel();
		if (Flight.HitPoints <= 0 || Flight.State == ESimCopterFlightState::Dying || Flight.State == ESimCopterFlightState::Dead)
		{
			// Reuse the existing crash/airport path (FUN_0048a8b0). A surviving
			// helicopter keeps its location, fuel, damage and running engine.
			ParkedHelicopter->WriteOffPassengersInDestroyedHelicopter();
			ParkedHelicopter->ResetAircraft();
			ParkedHelicopter->ReturnToAirportAfterCrash();
		}
		PilotHealth = 100;
		RemoveControllerOverlayWidget();
		ParkedHelicopter->EnterHelicopter(PlayerController, /*bBlendView=*/false);
		return;
	}
	if (bAutoRescueAttached) { FallPeakZ=GetActorLocation().Z; return; }
	TaserCooldown=FMath::Max(0.0f,TaserCooldown-DeltaSeconds);
	if (bParachuteDeployed)
	{
		auto* Move = GetCharacterMovement();
		if (!Move->IsFalling()) SetParachuteDeployed(false);
		else
		{
			Move->Velocity.Z = FMath::FInterpConstantTo(Move->Velocity.Z, -150.0f, DeltaSeconds, 1600.0f);
			// Once the fall is arrested, closing the canopy begins a new fall from this height.
			if (Move->Velocity.Z >= -250.0f) FallPeakZ = GetActorLocation().Z;
		}
	}
	// Remake control change: reaching a downed person is the pickup action, before auto-boarding.
	TryCarryDownedPerson();
	if(GetCharacterMovement()->IsFalling()) FallPeakZ=FMath::Max(FallPeakZ,float(GetActorLocation().Z));
	else FallPeakZ=GetActorLocation().Z;
	if(InjurySeconds>0)
	{
		InjurySeconds=FMath::Max(0.0f,InjurySeconds-DeltaSeconds);
		GetCharacterMovement()->MaxWalkSpeed=WalkSpeedCmPerSec*0.4f;
		if(InjurySeconds==0)
		{
			GetCharacterMovement()->MaxWalkSpeed=WalkSpeedCmPerSec;
		}
	}
}

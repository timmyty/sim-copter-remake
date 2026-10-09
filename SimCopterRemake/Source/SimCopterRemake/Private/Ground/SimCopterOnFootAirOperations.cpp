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
#include "Ground/SimCopterPilotEquipment.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

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
		CameraBoom->TargetOffset.Z = bDeployed ? 50.0f : 20.5f;
	}
}

void ASimCopterOnFootPawn::ToggleParachute()
{
	if (PilotHealth <= 0 || !GetCharacterMovement()->IsFalling()) return;
	SetParachuteDeployed(!bParachuteDeployed);
}

void ASimCopterOnFootPawn::StartTaserAim()
{
	if (PilotHealth <= 0 || IsCarryingMissionPerson() || bParachuteDeployed) return;
	bTaserAiming=true;
	if(CameraComponent) CameraComponent->SetFieldOfView(55);
	if(!TaserMesh && CameraComponent)
	{
		TaserMesh=NewObject<UProceduralMeshComponent>(this,TEXT("Taser"));
		AddInstanceComponent(TaserMesh); TaserMesh->SetupAttachment(RootComponent); TaserMesh->RegisterComponent();
		FMaxisMeshSection Shape; SimCopterPilotEquipment::BuildTaser(Shape);
		TaserMesh->CreateMeshSection_LinearColor(0,Shape.Vertices,Shape.Triangles,Shape.Normals,Shape.UVs,Shape.VertexColors,Shape.Tangents,false);
		TaserMesh->SetMaterial(0,BodyVertexColorMaterial);
		TaserMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TaserMesh->SetRelativeLocation(FVector(8,5,7));
		TaserMesh->SetRelativeScale3D(FVector(0.25));
	}
	if(TaserMesh) TaserMesh->SetVisibility(true);
}
void ASimCopterOnFootPawn::StopTaserAim()
{
	bTaserAiming=false;
	if(TaserMesh) TaserMesh->SetVisibility(false);
}
void ASimCopterOnFootPawn::FireTaser()
{
	if(!bTaserAiming || TaserCooldown>0 || !CameraComponent || IsCarryingMissionPerson()) return;
	TaserCooldown=1.5f;
	FVector End;
	ASimCopterGroundAgent* Target=SimCopterAirOperations::TracePerson(GetWorld(),this,CameraComponent->GetComponentLocation(),CameraComponent->GetForwardVector(),650,End);
	const bool Hit=Target && Target->StunForArrest();
	TaserStatus=Hit?TEXT("Suspect stunned. Walk up to carry automatically."):TEXT("No suspect hit. Taser recharging.");
	if(auto* Missions=Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(GetWorld(),ASimCopterMissionSystemActor::StaticClass())))
	{
		Missions->ShowAirOperationsMessage(TaserStatus);
		if(auto* FX=Missions->FindComponentByClass<USimCopterParticleFXComponent>())
		{
			const FVector Muzzle = TaserMesh ? TaserMesh->GetComponentTransform().TransformPosition(FVector(15,0,3)) : CameraComponent->GetComponentLocation();
			SimCopterAirOperations::DrawTaserDischarge(FX, Muzzle, End);
		}
	}
}
bool ASimCopterOnFootPawn::TryCarryDownedPerson()
{
	if(IsCarryingMissionPerson() || !CanPickUpMissionPersonNow() || PilotHealth<=0 || GetCharacterMovement()->IsFalling()) return false;
	ASimCopterGroundAgent* Best=nullptr; float Distance=90;
	for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
	{
		const bool bDowned=It->IsInPoliceCustody() || It->IsMedevacVictim() || It->IsMissionPatientDead();
		const float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation());
		if(It->GetAgentKind()==ESimCopterGroundAgentKind::Pedestrian && bDowned &&
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
		PilotHealth=FMath::Max(0.0f,PilotHealth-FMath::Clamp((Distance-SimCopterAirOperations::SafeFallCm)*0.08f+15.0f,15.0f,100.0f));
		InjurySeconds=PilotHealth<=0?5.0f:2.0f;
		if(auto* Missions=Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(GetWorld(),ASimCopterMissionSystemActor::StaticClass())))
			Missions->ShowAirOperationsMessage(FString::Printf(TEXT("Fall injury - pilot health %.0f%%"),PilotHealth));
	}
	FallPeakZ=GetActorLocation().Z;
	SetParachuteDeployed(false);
	Super::Landed(Hit);
}
void ASimCopterOnFootPawn::UpdateAirOperations(float DeltaSeconds)
{
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
		GetCharacterMovement()->MaxWalkSpeed=PilotHealth<=0?0:WalkSpeedCmPerSec*0.4f;
		if(InjurySeconds==0)
		{
			if(PilotHealth<=0 && ParkedHelicopter)
			{
				if(CarriedMissionPerson.IsValid()) DropCarriedMissionPerson();
				ParkedHelicopter->ResetAircraft();
				SetActorLocation(ParkedHelicopter->GetPassengerDropWorldLocation()+FVector(100,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
				PilotHealth=100;
			}
			GetCharacterMovement()->MaxWalkSpeed=WalkSpeedCmPerSec;
		}
	}
}

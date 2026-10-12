#include "Ground/SimCopterEnergyTaser.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterPilotEquipment.h"
#include "Flight/SimCopterAirOperations.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "City/SimCopterEffectExposure.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace SimCopterEnergyTaser
{
bool IsTarget(const ASimCopterGroundAgent* Agent)
{
	if (!IsValid(Agent) || Agent->IsActorBeingDestroyed() || Agent->IsHidden() || Agent->HasMissionResolutionReported() ||
		Agent->IsMissionCarried() || Agent->GetBehaviorCarrier() || Agent->IsInPoliceCustody() || Agent->IsMedevacVictim() || Agent->IsMissionPatientDead()) return false;
	if (Agent->GetAgentKind()!=ESimCopterGroundAgentKind::Vehicle) return Agent->IsArrestableSuspect();
	if (Agent->HasVehicleExploded() || Agent->IsVehicleTowed() || Agent->IsVehicleTipped() || Agent->IsTowableVehicle()) return false;
	if (Agent->IsSpeeder()) return Agent->IsFleeing();
	if (!Agent->IsCriminalCar() || Agent->GetCriminalEventId()==INDEX_NONE) return false;
	// Pooled getaway cars can retain their class/flags after their event has finished.
	const auto* Missions=Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(Agent,ASimCopterMissionSystemActor::StaticClass()));
	return !Missions || Missions->IsMissionEventActive(Agent->GetCriminalEventId());
}

FVector TargetPoint(const ASimCopterGroundAgent* Agent)
{
	FBox Bounds(ForceInit);
	if (Agent->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle && Agent->TryGetBodyLocalBoundsCm(Bounds))
		return Agent->GetActorTransform().TransformPosition(Bounds.GetCenter());
	return Agent->GetActorLocation();
}

ASimCopterGroundAgent* FindTarget(UWorld* World, AActor* Source, const FVector& Start, const FVector& Direction)
{
	if (!World) return nullptr;
	ASimCopterGroundAgent* Best=nullptr;
	float BestScore=TNumericLimits<float>::Max();
	const FVector Forward=Direction.GetSafeNormal();
	for (TActorIterator<ASimCopterGroundAgent> It(World); It; ++It)
	{
		if (!IsTarget(*It)) continue;
		const FVector Delta=TargetPoint(*It)-Start;
		const float Distance=Delta.Size();
		const float Dot=FVector::DotProduct(Forward,Delta.GetSafeNormal());
		if (Distance>RangeCm || Dot<FMath::Cos(FMath::DegreesToRadians(SightConeDegrees))) continue;
		FVector End;
		// Use the real body/civilian/world trace, not just an angular test through buildings.
		if (SimCopterAirOperations::TraceRemoteTaserTarget(World,Source,Start,Delta,Distance+10,End)!=*It) continue;
		const float Score=(1-Dot)*10000+Distance*.002f;
		if (Score<BestScore) { Best=*It; BestScore=Score; }
	}
	return Best;
}

void FCharge::Update(float DeltaSeconds, bool bSighted)
{
	const float Dt=FMath::Max(0.f,DeltaSeconds);
	// The powered appearance communicates a current criminal sighting. Civilians,
	// ordinary traffic, cover, and holstering immediately return it to standby.
	if (!bSighted) { Energy=0; Deployment=0; return; }
	Energy=FMath::Min(1.f,Energy+Dt/ChargeSeconds);
	Deployment=FMath::FInterpConstantTo(Deployment,1.f,Dt,2.5f);
}
}

ASimCopterTaserOrb::ASimCopterTaserOrb()
{
	PrimaryActorTick.bCanEverTick=true;
	Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EnergyOrb")); SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false); Mesh->SetCanEverAffectNavigation(false);
}

void ASimCopterTaserOrb::Launch(AActor* Source, ASimCopterGroundAgent* Target, const FVector& Location, const FVector& Aim)
{
	Shooter=Source; HomingTarget=Target; Direction=Aim.GetSafeNormal(); SetActorLocation(Location);
	Glow=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_SimCopterParticleFX.M_SimCopterParticleFX")),this);
	Mesh->SetMaterial(0,Glow); UpdateVisual();
	if (auto* Audio=USimCopterAudioSubsystem::Get(this))
	{
		Audio->PlayTaserSound(ESimCopterTaserSound::Release,Mesh);
		SeekSound=Audio->PlayTaserSound(ESimCopterTaserSound::Seek,Mesh);
	}
}

void ASimCopterTaserOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt=FMath::Max(0.f,DeltaSeconds); Age+=Dt;
	if (bImpacted)
	{
		ImpactAge+=Dt;
		if (ImpactAge>=.7f) { Destroy(); return; }
		if (auto* Target=ImpactTarget.Get(); IsValid(Target) && !Target->IsHidden()) SetActorLocation(SimCopterEnergyTaser::TargetPoint(Target));
	}
	else
	{
		if (!SimCopterEnergyTaser::IsTarget(HomingTarget.Get()) || Age>2.5f) { Impact(nullptr); return; }
		// Small swept steps prevent a fast shot from skipping a person, car or thin wall.
		for (float Remaining=FMath::Min(Dt,.2f); Remaining>0 && !bImpacted;)
		{
			const float Step=FMath::Min(Remaining,1.f/120); Remaining-=Step;
			const FVector From=GetActorLocation();
			const FVector Desired=(SimCopterEnergyTaser::TargetPoint(HomingTarget.Get())-From).GetSafeNormal();
			const float Angle=FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction,Desired),-1.,1.));
			const FQuat Turn=FQuat::FindBetweenNormals(Direction,Desired);
			Direction=FQuat::Slerp(FQuat::Identity,Turn,FMath::Min(1.f,FMath::DegreesToRadians(540.f)*Step/FMath::Max(Angle,.0001f))).RotateVector(Direction).GetSafeNormal();
			FVector End;
			const float Travel=1400.f*Step;
			auto* Contact=SimCopterAirOperations::TraceRemoteTaserTarget(GetWorld(),Shooter.Get(),From,Direction,Travel,End);
			SetActorLocation(End);
			if (Contact || FVector::DistSquared(From,End)<FMath::Square(Travel-.01f)) Impact(Contact);
		}
	}
	if (SeekSound.IsValid()) if (auto* Audio=USimCopterAudioSubsystem::Get(this))
		SeekSound->SetVolumeMultiplier(.42f*USimCopterAudioSubsystem::VolumeIndexToGain(Audio->GetMasterVolume()));
	UpdateVisual();
}

void ASimCopterTaserOrb::Impact(ASimCopterGroundAgent* Contact)
{
	bImpacted=true; ImpactAge=0;
	if (auto* Audio=USimCopterAudioSubsystem::Get(this))
	{
		Audio->StopAttachedVoiceLoop(SeekSound.Get()); SeekSound.Reset();
		if (Contact) Audio->PlayTaserSound(ESimCopterTaserSound::Impact,Mesh);
	}
	if (Contact==HomingTarget.Get() && SimCopterEnergyTaser::IsTarget(Contact))
	{
		if (Contact->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle) Contact->DisableVehicleWithTaser();
		else Contact->StunForArrest(SimCopterEnergyTaser::StunSeconds);
		ImpactTarget=Contact;
	}
	UpdateVisual();
}

void ASimCopterTaserOrb::UpdateVisual()
{
	FMaxisMeshSection Shape;
	float Radius=4.f, Energy=1.f;
	if (bImpacted)
	{
		Energy=FMath::Clamp(1.f-ImpactAge/.7f,0.f,1.f);
		Radius=4+ImpactAge*38;
		if (auto* Target=ImpactTarget.Get()) Radius=Target->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle ? 32.f : 13.f;
	}
	SimCopterPilotEquipment::BuildTaserOrb(Shape,Radius,Age,Energy,!bImpacted);
	if (bImpacted) if (auto* Target=ImpactTarget.Get())
	{
		FBox Bounds(ForceInit);
		const FVector Half=Target->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle && Target->TryGetBodyLocalBoundsCm(Bounds)
			? Bounds.GetExtent()+FVector(3) : FVector(10,10,Target->GetCapsuleHalfHeightCm());
		for (auto& V:Shape.Vertices) V=Target->GetActorRotation().RotateVector(V*Half/FMath::Max(1.f,Radius));
	}
	Mesh->CreateMeshSection_LinearColor(0,Shape.Vertices,Shape.Triangles,Shape.Normals,Shape.UVs,Shape.VertexColors,Shape.Tangents,false);
	if (Glow) Glow->SetScalarParameterValue(TEXT("EmissiveNits"),USimCopterEffectExposureSubsystem::GetEffectEmissiveNitsForWorld(GetWorld())*Energy*1.5f);
}

void ASimCopterTaserOrb::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* Audio=USimCopterAudioSubsystem::Get(this)) Audio->StopAttachedVoiceLoop(SeekSound.Get());
	Super::EndPlay(Reason);
}

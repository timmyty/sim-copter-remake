#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimCopterEnergyTaser.generated.h"

class ASimCopterGroundAgent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;

namespace SimCopterEnergyTaser
{
constexpr float RangeCm=1800.f;
constexpr float SightConeDegrees=7.f;
constexpr float ChargeSeconds=.65f;
constexpr float StunSeconds=12.f;

// Only a visible, unresolved criminal is eligible. Fleeing by itself is not a crime.
SIMCOPTERREMAKE_API bool IsTarget(const ASimCopterGroundAgent* Agent);
SIMCOPTERREMAKE_API FVector TargetPoint(const ASimCopterGroundAgent* Agent);
SIMCOPTERREMAKE_API ASimCopterGroundAgent* FindTarget(UWorld* World, AActor* Source,
	const FVector& Start, const FVector& Direction);

struct FCharge
{
	float Energy=0;
	float Deployment=0;
	void Update(float DeltaSeconds, bool bSighted);
	bool CanFire() const { return Energy>=.99f; }
	void Consume() { Energy=0; }
};
}

// The remake's nonlethal orb owns its flight, contact and sound even after the pilot holsters.
UCLASS(Transient, NotBlueprintable)
class SIMCOPTERREMAKE_API ASimCopterTaserOrb : public AActor
{
	GENERATED_BODY()
public:
	ASimCopterTaserOrb();
	void Launch(AActor* Source, ASimCopterGroundAgent* Target, const FVector& Location, const FVector& Direction);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool HasImpacted() const { return bImpacted; }
private:
	friend class FSimCopterEnergyTaserTest;
	friend class FSimCopterEnergyTaserVisualTest;
	void Impact(ASimCopterGroundAgent* Contact);
	void UpdateVisual();
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
	TWeakObjectPtr<AActor> Shooter;
	TWeakObjectPtr<ASimCopterGroundAgent> HomingTarget;
	TWeakObjectPtr<ASimCopterGroundAgent> ImpactTarget;
	TWeakObjectPtr<UAudioComponent> SeekSound;
	FVector Direction=FVector::ForwardVector;
	float Age=0;
	float ImpactAge=0;
	bool bImpacted=false;
};

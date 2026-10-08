#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "SimCopterAirOperations.generated.h"

class ASimCopterHelicopterPawn;
class ASimCopterGroundAgent;
class ASimCopterTrafficSystemActor;
class ASimCopterMissionSystemActor;
class UProceduralMeshComponent;
class UMaterialInterface;
class ACameraActor;
class USimCopterParticleFXComponent;

// User-requested remake additions. These are independent of the original fixed-point winch.
namespace SimCopterAirOperations
{
constexpr float SafeFallCm = 900.0f;
constexpr float CageBoardHoldSeconds = 1.25f;
constexpr float MaxCableCm = 1800.0f;
constexpr int32 CageCapacity = 4;
// Fictional remake tasers: a stun query and cosmetic electrical discharge, never weapon projectiles.
SIMCOPTERREMAKE_API void DrawTaserDischarge(USimCopterParticleFXComponent* FX, const FVector& Start, const FVector& End);
SIMCOPTERREMAKE_API bool IsCriminalState(int32 State);
SIMCOPTERREMAKE_API bool FallCausesInjury(float DistanceCm, bool bInWater);
SIMCOPTERREMAKE_API FVector DentVertex(const FVector& Vertex, const FVector& Impact, const FVector& BodyCenter);
SIMCOPTERREMAKE_API ASimCopterGroundAgent* TracePerson(UWorld* World, AActor* Source, const FVector& Start, const FVector& Direction, float Range, FVector& OutEnd);
}

UENUM()
enum class ESimCopterRecoverySite : uint8 { AutoRepair, Junkyard, Harbor };

UCLASS()
class SIMCOPTERREMAKE_API ASimCopterRecoverySite : public AActor
{
	GENERATED_BODY()
public:
	ASimCopterRecoverySite();
	void Configure(ESimCopterRecoverySite Kind, const FVector& Location);
	ESimCopterRecoverySite GetKind() const { return SiteKind; }
private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
	ESimCopterRecoverySite SiteKind = ESimCopterRecoverySite::AutoRepair;
};

UCLASS()
class SIMCOPTERREMAKE_API USimCopterAirOperationsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USimCopterAirOperationsComponent();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void Update(float DeltaSeconds);
	bool IsCargoToolSelected() const;
	void UseCargoTool();
	void SetCableInput(float Direction) { CableInput = FMath::Clamp(Direction, -1.0f, 1.0f); }
	void OpenCargo();
	bool TryGrabCargo();
	bool TryDeliverCargo();
	void ReleaseCargo(bool bOpenCage = true);
	bool IsDeployed() const { return bDeployed; }
	bool HasCargo() const;
	bool HasBoat() const { return BoatIndex != INDEX_NONE; }
	bool IsCage() const { return bCage; }
	FVector GetCargoLocation() const { return CargoLocation; }
	float GetCableLength() const { return CableLength; }
	int32 GetCageCount() const { return CagePeople.Num(); }
	ASimCopterGroundAgent* GetTowVehicle() const { return TowVehicle.Get(); }
	void SerializeState(FArchive& Ar);
	void ResolveSavedCargo();

	void TogglePoliceTaser();
	void FirePoliceTaser();
	void AimPoliceTaser(float Yaw, float Pitch, float DeltaSeconds);
	bool IsPoliceTaserActive() const { return bPoliceTaserActive; }
	bool HasPolicePassenger() const;
	bool AdvanceAutopilot(const FVector& Goal, float DeltaSeconds, float Speed = 1400.0f);
	bool IsAIPiloted() const { return bSupportAircraft || bPoliceTaserActive; }
	bool IsSupportAircraft() const { return bSupportAircraft; }
	void SetSupportAircraft(bool bValue) { bSupportAircraft = bValue; }
	void SetSupportGoal(const FVector& Goal) { AutopilotGoal = Goal; }
	FString GetStatus() const { return Status; }
	void SetStatus(const FString& Value) { Status = Value; }
	void FinishPoliceHandoffs(float DeltaSeconds);
private:
	friend class FSimCopterAirOperationsTest;
	ASimCopterHelicopterPawn* Helicopter() const;
	void EnsureMesh();
	void DrawCargo();
	void UpdatePoliceTaser(float DeltaSeconds);
	void EndPoliceTaser();
	bool HasClearLine(const FVector& From, const FVector& To, AActor* Target = nullptr) const;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CargoMesh;
	UPROPERTY() TObjectPtr<USceneComponent> CageAnchor;
	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
	UPROPERTY() TObjectPtr<ACameraActor> PoliceTaserCamera;
	TWeakObjectPtr<ASimCopterGroundAgent> TowVehicle;
	TArray<TWeakObjectPtr<ASimCopterGroundAgent>> CagePeople;
	TArray<FName> SavedCargoNames;
	TSet<TWeakObjectPtr<ASimCopterGroundAgent>> VehicleContacts;
	bool bDeployed = false;
	bool bCage = false;
	bool bCageOpen = false;
	bool bPoliceTaserActive = false;
	bool bSupportAircraft = false;
	bool bSavedCargoPending = false;
	int32 BoatIndex = INDEX_NONE;
	float CableLength = 140.0f;
	float CableInput = 0.0f;
	float BoardHoldSeconds = 0.0f;
	float ShotCooldown = 0.0f;
	float PoliceHandoffSeconds = 0.0f;
	FVector CargoLocation = FVector::ZeroVector;
	FVector CargoVelocity = FVector::ZeroVector;
	FVector AutopilotGoal = FVector::ZeroVector;
	FVector PoliceTaserFocus = FVector::ZeroVector;
	FRotator PoliceTaserAim = FRotator::ZeroRotator;
	FString Status;
};

// City-wide recovery destinations and the single dispatchable helper aircraft.
UCLASS()
class SIMCOPTERREMAKE_API USimCopterAirOperationsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Deinitialize() override;
	void EnsureSites();
	ASimCopterRecoverySite* FindRecoverySite(const FVector& From, bool bBoat) const;
	const TArray<TWeakObjectPtr<ASimCopterRecoverySite>>& GetSites() const { return Sites; }
	bool OrderSupport(int32 EventId, bool bAutomatic);
	void RecallSupport();
	void SerializeState(FArchive& Ar);
	FString GetSupportStatus() const;
	bool IsSupportUnlocked() const;
	ASimCopterHelicopterPawn* GetSupportHelicopter() const { return SupportHelicopter.Get(); }
	ASimCopterMissionSystemActor* Missions() const;
	ASimCopterTrafficSystemActor* Traffic() const;
	bool FindServiceRoof(uint8 BuildingId, const FVector& From, FVector& OutRoof) const;
private:
	friend class FSimCopterAirOperationsTest;
	void UpdateSupport(float DeltaSeconds);
	void EnsureSupport();
	bool CanPlayerTow() const;
	void UpdateStalledCars(float DeltaSeconds);
	void StartStalledCar(bool bCanTow);
	TArray<TWeakObjectPtr<ASimCopterRecoverySite>> Sites;
	mutable TMap<uint8,TArray<FVector>> ServiceRoofs;
	TWeakObjectPtr<ASimCopterHelicopterPawn> SupportHelicopter;
	bool bSitesReady = false;
	bool bAutomatic = true;
	bool bReturning = false;
	int32 AssignedEvent = INDEX_NONE;
	float ThinkSeconds = 0.0f;
	// Stored in equipped-rate seconds; the countdown runs six times slower without a tow clamp.
	float StalledCarSeconds = 150.0f;
	float WorkSeconds = 0.0f;
	float MissionAttemptSeconds = 0.0f;
	FVector SupportHome = FVector::ZeroVector;
};

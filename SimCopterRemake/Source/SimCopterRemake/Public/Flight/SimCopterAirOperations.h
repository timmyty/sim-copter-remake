#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "SimCopterAirOperations.generated.h"

class ASimCopterHelicopterPawn;
class ASimCopterGroundAgent;
class ASimCopterOnFootPawn;
class ASimCopterTrafficSystemActor;
class ASimCopterMissionSystemActor;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ACameraActor;
class USimCopterParticleFXComponent;

// User-requested remake additions. These are independent of the original fixed-point winch.
namespace SimCopterAirOperations
{
constexpr float SafeFallCm = 900.0f;
constexpr float CageBoardHoldSeconds = 1.25f;
constexpr float VehicleTaserDisableSeconds = 16.0f;
constexpr float MaxCableCm = 1800.0f;
constexpr int32 CageCapacity = 4;
constexpr float RecoveryRadiusCm = 180.0f;
// Fictional remake tasers: a stun query and cosmetic electrical discharge, never weapon projectiles.
SIMCOPTERREMAKE_API void DrawTaserDischarge(USimCopterParticleFXComponent* FX, const FVector& Start, const FVector& End);
SIMCOPTERREMAKE_API bool IsCriminalState(int32 State);
SIMCOPTERREMAKE_API bool FallCausesInjury(float DistanceCm, bool bInWater);
SIMCOPTERREMAKE_API FVector DentVertex(const FVector& Vertex, const FVector& Impact, const FVector& BodyCenter);
SIMCOPTERREMAKE_API ASimCopterGroundAgent* TracePerson(UWorld* World, AActor* Source, const FVector& Start, const FVector& Direction, float Range, FVector& OutEnd);
SIMCOPTERREMAKE_API ASimCopterGroundAgent* TraceRemoteTaserTarget(UWorld* World, AActor* Source, const FVector& Start, const FVector& Direction, float Range, FVector& OutEnd);
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
	void SetWaypointActive(bool bActive);
	bool IsWaypointActive() const { return bWaypointActive; }
	virtual void Tick(float DeltaSeconds) override;
	ESimCopterRecoverySite GetKind() const { return SiteKind; }
private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> WaypointRing;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
	bool bWaypointActive = false;
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
	void SetCagePanelInput(float Direction);
	float GetCagePanelInput() const { return CagePanelInput; }
	FString GetCageReadout() const;
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

	void ToggleRemoteTaser();
	void FireRemoteTaser();
	void AimRemoteTaser(float Yaw, float Pitch, float DeltaSeconds);
	void ZoomRemoteTaser(float Steps);
	float GetRemoteTaserZoom() const { return RemoteScopeZoom; }
	void UpdateRemoteTaserControls(float MouseYaw, float MousePitch, float StickYaw, float StickPitch, float DeltaSeconds);
	bool IsRemoteTaserActive() const { return bRemoteTaserActive; }
	FVector GetRemoteTaserMuzzle() const;
	FVector GetRemoteTaserDirection() const { return RemoteTaserAim.Vector(); }
	FString GetRemoteTaserReadout() const;
	void BeginPilotRecovery(ASimCopterOnFootPawn* Pilot);
	void EndPilotRecovery();
	bool TryLoadRecoveryPassenger(ASimCopterOnFootPawn* Pilot);
	bool IsRecoveringPilot() const { return RecoveryPilot.IsValid(); }
	bool AdvanceAutopilot(const FVector& Goal, float DeltaSeconds, float Speed = 1400.0f, bool bAllowApproachDescent = false);
	bool IsAIPiloted() const { return bSupportAircraft || bRemoteTaserActive || IsRecoveringPilot(); }
	bool IsSupportAircraft() const { return bSupportAircraft; }
	void SetSupportAircraft(bool bValue) { bSupportAircraft = bValue; }
	void SetSupportGoal(const FVector& Goal) { AutopilotGoal = Goal; }
	FString GetStatus() const { return Status; }
	void SetStatus(const FString& Value) { Status = Value; }
	void FinishPoliceHandoffs(float DeltaSeconds);
private:
	friend class FSimCopterAirOperationsTest;
	friend class FSimCopterCaptureCageTest;
	friend class FSimCopterCageRecoveryTest;
	friend class FSimCopterPoliceRecoveryTest;
	friend class FSimCopterRemoteRecoveryTest;
	friend class FSimCopterVehicleTaserTest;
	ASimCopterHelicopterPawn* Helicopter() const;
	void EnsureMesh();
	void DrawCargo();
	FBox GetCageCargoBounds() const;
	void UpdateCageMount();
	void UpdateRemoteTaser(float DeltaSeconds);
	void DrawRemoteTaserDischarge(const FVector& Start, const FVector& End, bool bHit);
	void UpdateRemoteTaserDischarge(float DeltaSeconds);
	friend class FSimCopterTaserVisualTest;
	void UpdateTaserImpact(float DeltaSeconds);
	void EndRemoteTaser();
	void UpdateRemoteTaserMount();
	void SetRemoteTaserCursor(bool bVisible);
	void OffsetRemoteTaserAim(float YawDegrees, float PitchDegrees);
	bool CanControlRemoteTaser() const;
	void SelectRemoteTaserTarget();
	FVector FindRemoteTaserPursuitOffset(const FVector& Velocity) const;
	void UpdatePilotRecovery(float DeltaSeconds);
	bool FindRecoverySurface(FVector& OutGoal, bool& bOutCanLand) const;
	bool FindRecoverySurfaceAt(const FVector& Position, FVector& OutGoal, bool& bOutCanLand) const;
	bool FindNearbyRecoveryLanding(FVector& OutGoal) const;
	bool IsRecoveryRouteClear(const FVector& From, const FVector& To) const;
	bool MoveRecoveryAircraft(const FVector& Goal, float DeltaSeconds);
	void UpdateRecoveryHarness(float DeltaSeconds);
	bool HasClearLine(const FVector& From, const FVector& To, AActor* Target = nullptr) const;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CargoMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CageMountMesh;
	UPROPERTY() TObjectPtr<USceneComponent> CageAnchor;
	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
	UPROPERTY() TObjectPtr<ACameraActor> RemoteTaserCamera;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RemoteTaserMount;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RemoteTaserWeapon;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RemoteTaserTrail;
	UPROPERTY() TObjectPtr<UMaterialInterface> TaserTrailMaterial;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TaserTrailMaterialInstance;
	float TaserTrailSeconds = 0.0f;
	TWeakObjectPtr<ASimCopterGroundAgent> TaserImpactTarget;
	float TaserImpactSeconds = 0.0f;
	TWeakObjectPtr<ASimCopterGroundAgent> RemoteTaserTarget;
	TWeakObjectPtr<ASimCopterGroundAgent> LastRemoteTaserTarget;
	TWeakObjectPtr<ASimCopterOnFootPawn> RecoveryPilot;
	TWeakObjectPtr<ASimCopterGroundAgent> RecoveryPassenger;
	FVector RecoveryHoldLocation = FVector::ZeroVector;
	FVector RecoveryLandingGoal = FVector::ZeroVector;
	float RecoveryLandingSearchSeconds = 0;
	bool bHasRecoveryLandingGoal = false;
	float RecoveryHoldSeconds = 0;
	float RecoveryCableLength = 0;
	bool bRecoveryRaising = false;
	bool bRecoveryHarnessActive = false;
	bool bRecoveryPilotAttached = false;
	float RemotePursuitSpeed = 0;
	FVector RemotePursuitOffset = FVector::ZeroVector;
	float RemoteTargetSearchSeconds = 0;
	float RemotePursuitPlanSeconds = 0;
	FVector RemotePursuitGoal = FVector::ZeroVector;
	float RemoteObstructedSeconds = 0;
	bool bRemoteHolding = false;
	float RemoteScopeZoom = 1;
	float RemoteScopeZoomTarget = 1;
	TWeakObjectPtr<ASimCopterGroundAgent> TowVehicle;
	TArray<TWeakObjectPtr<ASimCopterGroundAgent>> CagePeople;
	TArray<FName> SavedCargoNames;
	TSet<TWeakObjectPtr<ASimCopterGroundAgent>> VehicleContacts;
	bool bDeployed = false;
	bool bCage = false;
	bool bCageOpen = false;
	bool bRemoteTaserActive = false;
	bool bSupportAircraft = false;
	bool bSavedCargoPending = false;
	int32 BoatIndex = INDEX_NONE;
	float CableLength = 140.0f;
	float CableInput = 0.0f;
	float CagePanelInput = 0.0f;
	float CageFormation = 0.0f;
	float CageSeal = 0.0f;
	float CageCaptureDelay = 0.0f;
	float BoardHoldSeconds = 0.0f;
	float ShotCooldown = 0.0f;
	float PoliceHandoffSeconds = 0.0f;
	FVector CargoLocation = FVector::ZeroVector;
	FVector CargoVelocity = FVector::ZeroVector;
	FVector AutopilotGoal = FVector::ZeroVector;
	FVector RemoteTaserFocus = FVector::ZeroVector;
	FRotator RemoteTaserAim = FRotator::ZeroRotator;
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
	void RefreshRecoveryWaypoints();
	ASimCopterRecoverySite* FindRecoverySite(const FVector& From, bool bBoat) const;
	const TArray<TWeakObjectPtr<ASimCopterRecoverySite>>& GetSites() const { return Sites; }
	bool OrderSupport(int32 EventId);
	void RecallSupport();
	void SerializeState(FArchive& Ar);
	FString GetSupportStatus() const;
	bool IsSupportUnlocked() const;
	bool IsRescuingPerson(const ASimCopterGroundAgent* Person) const;
	ASimCopterHelicopterPawn* GetSupportHelicopter() const { return SupportHelicopter.Get(); }
	ASimCopterMissionSystemActor* Missions() const;
	ASimCopterTrafficSystemActor* Traffic() const;
	bool FindServiceRoof(uint8 BuildingId, const FVector& From, FVector& OutRoof) const;
private:
	friend class FSimCopterAirOperationsTest;
	friend class FSimCopterCageRecoveryTest;
	void UpdateSupport(float DeltaSeconds);
	friend class FSimCopterPoliceDestinationTest;
	void EnsureSupport();
	bool CanPlayerTow() const;
	void UpdateStalledCars(float DeltaSeconds);
	void StartStalledCar(bool bCanTow);
	TArray<TWeakObjectPtr<ASimCopterRecoverySite>> Sites;
	mutable TMap<uint8,TArray<FVector>> ServiceRoofs;
	TWeakObjectPtr<ASimCopterHelicopterPawn> SupportHelicopter;
	bool bSitesReady = false;
	bool bReturning = false;
	int32 AssignedEvent = INDEX_NONE;
	float ThinkSeconds = 0.0f;
	// Legacy 150-second units: now advances at 1/4 with a cage and 1/6 without one.
	float StalledCarSeconds = 150.0f;
	float WorkSeconds = 0.0f;
	float MissionAttemptSeconds = 0.0f;
	FVector SupportHome = FVector::ZeroVector;
	TWeakObjectPtr<ASimCopterGroundAgent> LandingPerson;
	int32 LandingEvent = INDEX_NONE;
	FVector LandingSurface = FVector::ZeroVector;
	bool bHasLandingSurface = false;
};

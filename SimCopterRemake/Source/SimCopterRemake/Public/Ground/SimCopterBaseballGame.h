#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimCopterBaseballGame.generated.h"

class ASimCopterGroundAgent;
class ASimCopterTrafficSystemActor;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

// Requested remake extension. Retail BHAVs 1205/1206 are individual pantomimes,
// not a rules engine; keep this independent of their saved VM state.
enum class ESimCopterBaseballPlay : uint8
{
	Ball, Strike, Foul, Single, Double, Triple, HomeRun, GroundOut, FlyOut
};

struct SIMCOPTERREMAKE_API FSimCopterBaseballScore
{
	int32 Inning = 1;
	int32 BattingTeam = 0; // visitors first, home second
	int32 Runs[2] = {};
	int32 NextBatter[2] = {};
	int32 Bases[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
	int32 Balls = 0, Strikes = 0, Outs = 0;
	bool bFinal = false;
	int32 Batter() const { return BattingTeam * 9 + NextBatter[BattingTeam]; }
	void Apply(ESimCopterBaseballPlay Play);
};

UCLASS(NotBlueprintable, Transient)
class SIMCOPTERREMAKE_API ASimCopterBaseballGame : public AActor
{
	GENERATED_BODY()
	friend class FSimCopterBaseballGameTest;
	friend class ASimCopterTrafficSystemActor;
public:
	ASimCopterBaseballGame();
	void Initialize(ASimCopterTrafficSystemActor* InTraffic, int32 InNode);
	void Stop();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static bool CanPlay(const ASimCopterGroundAgent* Agent);
	const FSimCopterBaseballScore& GetScore() const { return Score; }
private:
	enum class EPhase : uint8 { Ready, Windup, Pitch, InPlay, Return, Result, Final };
	TWeakObjectPtr<ASimCopterTrafficSystemActor> Traffic;
	TWeakObjectPtr<ASimCopterGroundAgent> Players[18];
	FSimCopterBaseballScore Score;
	FRandomStream Random;
	int32 NodeIndex = INDEX_NONE;
	EPhase Phase = EPhase::Ready;
	ESimCopterBaseballPlay Play = ESimCopterBaseballPlay::Strike;
	float Age = 0, PhaseAge = 0, RetrySeconds = 0;
	int32 Fielder = INDEX_NONE;
	int32 RunNext[18] = {}, RunEnd[18] = {};
	FVector BallStart = FVector::ZeroVector, BallEnd = FVector::ZeroVector;
	bool bBallCaught = false;
	// Ambient arrival assembles the initial roster through the population refill budget.
	bool bPopulationRosterPending = false;
	FString Announcement;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Ball;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Bat;
	UPROPERTY() TObjectPtr<USceneComponent> BoardRoot;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BoardBacking;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BoardLamps;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BoardLampMaterial;
	TArray<FString> BoardReadout;
	FVector Point(FVector2D Offset, float Height = 0) const;
	FVector Base(int32 Index) const;
	FVector Defense(int32 Slot) const;
	bool SyncPlayers();
	bool SetFormation();
	void SetGoal(int32 Player, const FVector& Location, const FVector& Facing, const TCHAR* Clip = TEXT("NoMo"), float Speed = 180);
	void SetPhase(EPhase Next);
	void StartRunners();
	bool MoveRunners();
	void FinishPlay();
	void InitializeScoreboard();
	void UpdateScoreboardLighting();
	void UpdateBoard();
	void Retire(int32 Player);
};

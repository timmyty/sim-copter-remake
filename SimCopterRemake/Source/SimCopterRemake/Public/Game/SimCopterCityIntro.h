#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCopterCityIntro.generated.h"

class ACameraActor;
class ASimCopterPlayerController;
class ASimCopterTrafficSystemActor;
class SWidget;

struct FSimCopterCityIntroShot
{
	FVector Focus = FVector::ZeroVector;
	float Radius = 4000;
	float CameraZ = 4000;
	FString Caption;
	FTransform Evaluate(float Alpha) const;
};

// FUN_0044ce50 presents the selected city's cityride before play. The remake uses
// the loaded city's geometry so imported cities and restored saves get a tour too.
UCLASS()
class SIMCOPTERREMAKE_API USimCopterCityIntro : public UActorComponent
{
	GENERATED_BODY()
public:
	USimCopterCityIntro();
	bool Start();
	void Finish();
	bool IsPlaying() const { return bPlaying; }
	void RequestSkip() { if (Elapsed >= 0.4f) bSkipRequested = true; }
	static TArray<FSimCopterCityIntroShot> BuildShots(const ASimCopterTrafficSystemActor& Traffic);
	static constexpr float ShotSeconds = 5.0f;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class FSimCopterCityIntroTest;
	void UpdateCamera();
	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<ASimCopterPlayerController> Controller;
	TArray<FSimCopterCityIntroShot> Shots;
	TSharedPtr<SWidget> Overlay;
	float Elapsed = 0;
	double LastTickSeconds = 0;
	bool bPlaying = false;
	bool bSkipRequested = false;
	bool bPreviousCursor = false;
	bool bPreviousFullCameraUpdate = false;
};

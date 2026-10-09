#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCopterCityIntro.generated.h"

class ACameraActor;
class ASimCopterPlayerController;
class ASimCopterTrafficSystemActor;
class ASimCopterAmbientVehiclesActor;
class FSimCopterTourViewExtension;
class SWidget;

struct FSimCopterCityIntroRoute
{
	FBox Bounds = FBox(ForceInit);
	float Distance = 0;
	static constexpr float FieldOfView = 55;
	static constexpr float Elevation = 38;
	static constexpr float Duration = 24;
	// Fit every corner for the entire revolution, including the title/skip safe area.
	void Fit(float AspectRatio);
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
	static FSimCopterCityIntroRoute BuildRoute(const ASimCopterTrafficSystemActor& Traffic);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class FSimCopterCityIntroTest;
	friend class FSimCopterCityTourRenderTest;
	void UpdateCamera();
	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
	TWeakObjectPtr<ASimCopterPlayerController> Controller;
	FSimCopterCityIntroRoute Route;
	TWeakObjectPtr<ASimCopterAmbientVehiclesActor> Ambient;
	TSharedPtr<FSimCopterTourViewExtension, ESPMode::ThreadSafe> PresentationClock;
	TSharedPtr<SWidget> Overlay;
	float ViewportAspect = 0;
	float Elapsed = 0;
	double LastTickSeconds = 0;
	bool bPlaying = false;
	bool bSkipRequested = false;
	bool bPreviousCursor = false;
	bool bPreviousFullCameraUpdate = false;
};

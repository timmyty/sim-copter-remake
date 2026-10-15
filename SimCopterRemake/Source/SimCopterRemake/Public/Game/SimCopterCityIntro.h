#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCopterCityIntro.generated.h"

class ACameraActor;
class UAudioComponent;
class ASimCopterPlayerController;
class ASimCopterTrafficSystemActor;
class ASimCopterAmbientVehiclesActor;
class FSimCopterTourViewExtension;
class SWidget;

struct FSimCopterCityIntroFrame
{
	FVector Position = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	float OverviewWeight = 0;
};

struct FSimCopterCityIntroRoute
{
	FBox Bounds = FBox(ForceInit);
	float Distance = 0;
	static constexpr float FieldOfView = 55;
	static constexpr float Elevation = 38;
	// SCHOOK: CityRide 0x0044ce50 -> CITY<n>_B.SMK. All 30 CD movies have
	// 451 frames and a -6666 Smacker delay (66.66 ms); not the CITY*_S previews.
	static constexpr float FrameSeconds = 0.06666f;
	static constexpr float Duration = 451 * FrameSeconds;
	int32 CityIndex = INDEX_NONE;
	float TileSize = 400;
	float ReferenceDistance = 0;
	TArray<FSimCopterCityIntroFrame> Frames;
	FBox FinaleBounds = FBox(ForceInit);
	FVector AircraftStart = FVector::ZeroVector;
	FVector AircraftDirection = FVector::ForwardVector;
	bool bHelicopterSubject = false;
	float AircraftPassSeconds = 14;
	// Only the establishing view fits the whole map. The rest follows the
	// authored descent, low passes and landmark finale in the original movie.
	void Fit(float AspectRatio);
	FTransform Evaluate(float Alpha) const;
	FTransform EvaluateAircraft(float Seconds, int32 Index) const;
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
	friend class FSimCopterLoadingEffectsTest;
	friend class FSimCopterCityIntroLightingTest;
	friend class FSimCopterCityTourRenderTest;
	friend class FSimCopterCityIntroAudioTest;
	friend class FSimCopterCityIntroMixerCommand;
	void StartMusic();
	void UpdateMusic(bool bActive);
	void StopMusic();
	void UpdateCamera();
	void BeginTourLighting();
	void RestoreLighting();
	// Light cloud cover for the flyover (user request, v1.0.19), skipped on desert-looking maps.
	void BeginTourClouds();
	void ApplyTourClouds();
	void RestoreTourClouds();
	bool bCloudOverride = false;
	bool bLoggedTourClouds = false;
	// Whether TourCloudMaterial is our own instance (restored away afterwards) or the day
	// sequence's live one (only its coverage is put back).
	bool bCreatedTourCloudMaterial = false;

	float PreviousCloudCoverage = -1;
	int32 PreviousVolumetricCloudCVar = -1;
	TWeakObjectPtr<class UVolumetricCloudComponent> TourCloud;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInterface> PreviousCloudMaterial;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> TourCloudMaterial;
	bool bLightingOverride = false;
	uint8 PreviousTimeMode = 0;
	float PreviousStaticHour = 12;
	float PreviousLiveHour = -1;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Music;
	bool bRadioSuspended = false;
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

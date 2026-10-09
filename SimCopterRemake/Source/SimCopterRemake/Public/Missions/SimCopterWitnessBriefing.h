#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateBrush.h"
#include "SimCopterWitnessBriefing.generated.h"

class ASimCopterGroundAgent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class SWidget;

// Remake addition: a witness's one-time scene photograph, never a live tracking camera.
UCLASS()
class SIMCOPTERREMAKE_API USimCopterWitnessBriefing : public UActorComponent
{
	GENERATED_BODY()
public:
	USimCopterWitnessBriefing();
	void Report(ASimCopterGroundAgent* Suspect);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class FSimCopterWitnessPhotoRenderingTest;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Photo;
	TArray<TWeakObjectPtr<ASimCopterGroundAgent>> Pending;
	FSlateBrush PhotoBrush;
	TSharedPtr<SWidget> Panel;
	FText Caption;
	float DisplayRemaining = 0;
	float CaptureDelay = 0;
	bool bVoicePending = false;
	bool bPhotoPending = false;
	bool bSuspectWearsShades = false;
	void TakePhoto(ASimCopterGroundAgent& Suspect);
	void EnsurePanel();
	TSharedRef<SWidget> CreatePhotoWidget() const;
};

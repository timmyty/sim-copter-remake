// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCopterMoonDisc.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Overrides the brightness of the celestial vault's moon DISC - the textured card in the sky, not
 * the moonlight it casts.
 *
 * The stock `MI_Moon` ships at `Brightness = 2000`, which blows the disc out to a flat white blob:
 * there is no exposure that shows the lunar surface while still working for night visibility and
 * daylight, because the disc is clipping long before the rest of the frame is exposed correctly.
 * Lowering this pulls the surface detail back without touching illumination - `MoonLightIntensity`
 * on the day sequence actor is a separate control and is deliberately left alone.
 *
 * Add it to the `CelestialVaultDaySequenceActor`; it finds that actor's own Moon Disk component.
 *
 * **Why a component rather than just editing the material instance:** the actor swaps the disc's
 * material for a `UMaterialInstanceDynamic` at construction (to drive `MoonAge`), and rebuilds that
 * MID whenever it reconstructs. Any value typed into `MI_Moon` by hand is either shared with every
 * other project using the engine plugin or thrown away on the next rebuild. This re-asserts the
 * override on tick, so it survives.
 */
UCLASS(ClassGroup = (SimCopter), meta = (BlueprintSpawnableComponent, DisplayName = "SimCopter Moon Disc"))
class SIMCOPTERREMAKE_API USimCopterMoonDiscComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimCopterMoonDiscComponent();

	/** Larger sky discs, independent of moonlight/sunlight intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky Discs", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float SunDiscAngle = 3.0f;

	/** Multiplier of the astronomical moon diameter (the shipped map previously used 7.69). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky Discs", meta = (ClampMin = "0.1", ClampMax = "30.0"))
	float MoonSize = 14.0f;

	/** Project-owned material retains the plugin's phase shader and detailed lunar maps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon Disc")
	TObjectPtr<UMaterialInterface> MoonMaterial;

	/** Off leaves the disc on whatever `MI_Moon` authored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon Disc")
	bool bOverrideBrightness = true;

	/**
	 * Emissive brightness of the moon disc. Ten matches the city's night exposure while retaining
	 * the maria and crater detail; the stock plugin's 2000 clips them away.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon Disc", meta = (EditCondition = "bOverrideBrightness", ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0"))
	float Brightness = 10.0f;

	/** The value last pushed at the disc's material, for reading off the details panel. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Moon Disc")
	float AppliedBrightness = 0.0f;

	/** Pushes Brightness at the moon disc's material instance. */
	UFUNCTION(BlueprintCallable, Category = "Moon Disc")
	void ApplyBrightness();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void ApplyDiscSizes();
	/** The owning celestial actor's moon disc material, creating the MID if it is not one yet. */
	UMaterialInstanceDynamic* ResolveMoonDiscMaterial();

	/** Set once the owner has been checked and found not to be a celestial vault actor. */
	bool bWarnedMissingActor = false;

	/** Set once the disc's material has been checked and found to have no Brightness parameter. */
	bool bWarnedMissingParameter = false;
};

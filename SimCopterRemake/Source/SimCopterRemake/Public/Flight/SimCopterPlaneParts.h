#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Formats/MaxisMeshReader.h"
#include "SimCopterPlaneParts.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

// PLANE1's propeller and undercarriage share its GEO object. Keep their authored
// geometry, but give the moving parts independent transforms in every plane view.
UCLASS()
class SIMCOPTERREMAKE_API USimCopterPlanePartsComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	USimCopterPlanePartsComponent();
	static bool IsPropellerFace(const FMaxisMeshObject& Object, const FMaxisMeshFace& Face);
	static bool IsGearFace(const FMaxisMeshObject& Object, const FMaxisMeshFace& Face);
	static void RemoveMovingFaces(FMaxisMeshObject& Body);
	void Build(const FMaxisMeshObject& Source, const TArray<FColor>* Palette, float UnitsPerCm,
		float Scale, UMaterialInterface* Paint, UMaterialInterface* Blur);
	void SetPresentationTime(float Seconds, bool bSpinning, bool bGearDown);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	float GetPropellerAngle() const { return PropellerAngle; }
	bool IsGearVisible() const;
private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Propeller;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Gear;
	FVector PreviousWorld = FVector::ZeroVector;
	float RunningSeconds = 0;
	float PropellerAngle = 0;
};

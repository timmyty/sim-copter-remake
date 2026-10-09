#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimCopterDriveIn.generated.h"

class ASimCity2000CityActor;
class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class UAudioComponent;
class USoundSourceBus;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;

struct FSimCopterDriveInSurface
{
	FIntPoint Tile;
	// Original movie quad, in world space: top left, top right, bottom right, bottom left.
	TArray<FVector> Corners;
};

namespace SimCopterDriveIn
{
	SIMCOPTERREMAKE_API FString VideoDirectory();
	SIMCOPTERREMAKE_API bool ResolveVideo(const FString& FileName, FString& OutPath, FString& OutError);
}

UCLASS()
class SIMCOPTERREMAKE_API ASimCopterDriveInPlayer : public AActor
{
	GENERATED_BODY()
public:
	ASimCopterDriveInPlayer();
	static ASimCopterDriveInPlayer* Get(UWorld* World);
	FString PlayVideo(const FString& FileName, bool bToggle = false);
	void StopVideo();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool IsVideoActive() const { return !CurrentFile.IsEmpty(); }
	int32 GetScreenCount() const { return Screens.Num(); }
private:
	friend class FSimCopterDriveInPlaybackCommand;
	friend class FSimCopterDriveInAudioCommand;
	UPROPERTY(Transient) TObjectPtr<UMediaPlayer> Player;
	UPROPERTY(Transient) TObjectPtr<UMediaTexture> Texture;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ScreenParent;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Screens;
	UPROPERTY(Transient) TObjectPtr<UMediaSoundComponent> DecodedSound;
	UPROPERTY(Transient) TObjectPtr<USoundSourceBus> SoundBus;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Sounds;
	TArray<TWeakObjectPtr<ASimCity2000CityActor>> Cities;
	TArray<FSimCopterDriveInSurface> Surfaces;
	FString CurrentFile;
	bool bWasPaused = false;
	void ClearScreens();
	void BuildScreen(int32 Index);
	UFUNCTION() void MediaOpened(FString Url);
	UFUNCTION() void MediaFailed(FString Url);
};

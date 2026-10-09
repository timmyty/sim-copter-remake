#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Misc/AutomationTest.h"
#include "City/SimCopterDriveIn.h"
#include "City/SimCity2000CityActor.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioMixerDevice.h"
#include "AudioMixer.h"
#include "Modules/ModuleManager.h"
#include "ISubmixBufferListener.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "Components/AudioComponent.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"

class FDriveInTestMixerModule : public IAudioDeviceModule
{
public:
	virtual FAudioDevice* CreateAudioDevice() override
	{
		auto& Platform = FModuleManager::LoadModuleChecked<IAudioDeviceModule>(TEXT("AudioMixerXAudio2"));
		return new Audio::FMixerDevice(Platform.CreateAudioMixerPlatformInterface());
	}
};

// Captures the real Windows post-spatialization mix on a private device, then
// silences it before hardware output. The offline renderer cannot validate bus mixing.
class FDriveInAudioMeter : public ISubmixBufferListener
{
public:
	virtual void OnNewSubmixBuffer(const USoundSubmix*, float* Audio, int32 Count, int32, int32, double) override
	{
		FScopeLock Lock(&Mutex);
		for (int32 I = 0; I < Count; ++I) Peak = FMath::Max(Peak, FMath::Abs(Audio[I]));
		FMemory::Memzero(Audio, Count * sizeof(float));
	}
	float TakePeak() { FScopeLock Lock(&Mutex); const float Result = Peak; Peak = 0; return Result; }
	virtual bool IsRenderingAudio() const override { return true; }
private:
	FCriticalSection Mutex;
	float Peak = 0;
};

class FSimCopterDriveInAudioCommand : public IAutomationLatentCommand
{
public:
	explicit FSimCopterDriveInAudioCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (!World)
		{
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			World->bAllowAudioPlayback = true;
			FAudioDeviceParams Params; Params.AssociatedWorld = World;
			Params.Scope = EAudioDeviceScope::Unique;
			Params.AudioModule = &MixerModule;
			Device = GEngine->GetAudioDeviceManager()->RequestAudioDevice(Params);
			if (!Test->TestTrue(TEXT("Private Windows mixer exists (run without -NoSound)"), Device.IsValid())) return Finish();
			World->SetAudioDevice(Device);
			// An unfocused editor sets application volume to zero. This muted fixture
			// bypasses that volume while measuring and restores it on completion.
			AppVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume"));
			OldAppVolume = AppVolume->GetInt(); AppVolume->SetWithCurrentPriority(1);
			Meter = MakeShared<FDriveInAudioMeter, ESPMode::ThreadSafe>();
			Device->RegisterSubmixBufferListener(Meter.ToSharedRef(), Device->GetMainSubmixObject());
			World->InitializeActorsForPlay(FURL());
			Controller = World->SpawnActor<ASimCopterPlayerController>();
			Controller->Possess(World->SpawnActor<APawn>());
			City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
			City->bLoadOnConstruction = false;
			City->bRenderProceduralMapExtension = false;
			City->bRenderStreetLightSpotLights = false;
			City->CityFile.FilePath = SimCopterOriginalGame::ResolveRoot() / TEXT("cities/career/city29.sc2");
			City->FinishSpawning(FTransform::Identity); City->RebuildCity();
			DriveIn = ASimCopterDriveInPlayer::Get(World);
			Test->AddInfo(Controller->ExecuteCheatCodes(TEXT("hsi")));
			if (!Test->TestEqual(TEXT("Real city has eight theater speakers"), DriveIn->Sounds.Num(), 8)) return Finish();
			Started = StepStarted = FPlatformTime::Seconds();
		}
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 90) { Test->AddError(TEXT("Timed out testing drive-in audio")); return Finish(); }
		DriveIn->Tick(.05f);
		// This standalone fixture has no game loop; explicitly service media components.
		TArray<UMediaSoundComponent*> MediaSounds;
		DriveIn->GetComponents(MediaSounds);
		for (auto* Media : MediaSounds)
		{
			Media->bIsUISound = true;
			if (auto* Audio = Media->GetAudioComponent()) Audio->SetUISound(true);
			Media->UpdatePlayer();
		}
		// The editor otherwise pauses game sounds between this fixture's updates.
		for (auto Sound : DriveIn->Sounds)
			if (auto* Audio = Cast<UAudioComponent>(Sound.Get())) Audio->SetUISound(true);
		if (bAwaitingMovie && (!DriveIn->Player->IsPlaying() || DriveIn->Player->GetTime().GetTotalSeconds() < 1))
		{
			Device->SetDeviceMuted(false); Device->Update(true); return false;
		}
		if (bAwaitingMovie)
		{
			bAwaitingMovie = false; StepStarted = Now; Meter->TakePeak(); return false;
		}
		const FVector Centre = (DriveIn->Surfaces[Theater].Corners[0] + DriveIn->Surfaces[Theater].Corners[2]) * .5;
		const FVector Listener = Centre + (Phase == 1 ? FVector(0, 1000000, 0) : FVector(100, 0, 0));
		Device->SetListener(World, 0, FTransform(FQuat::Identity, Listener), .05f);
		// Isolate each actual emitter in turn so another nearby theater cannot mask a failure.
		for (int32 I = 0; I < DriveIn->Sounds.Num(); ++I)
			if (I != Theater) DriveIn->Sounds[I]->SetVolumeMultiplier(0.f);
		Device->SetDeviceMuted(false); Device->Update(true);
		const float Peak = Meter->TakePeak();
		if (Now - StepStarted < .5) return false; // Drain the previous listener position.
		StepPeak = FMath::Max(StepPeak, Peak);
		if (Now - StepStarted < 2) return false;
		const TCHAR* Names[] = {TEXT("near"), TEXT("far"), TEXT("returned"), TEXT("paused"), TEXT("resumed"), TEXT("demolished"), TEXT("surviving"), TEXT("silent_movie")};
		Test->AddInfo(FString::Printf(TEXT("Movie %d theater %d %s output peak %.6f"), Movie, Theater, Names[Phase], StepPeak));
		const FString Check = FString::Printf(TEXT("Movie %d theater %d %s audio"), Movie, Theater, Names[Phase]);
		if (Phase == 1 || Phase == 3 || Phase == 5 || Phase == 7) Test->TestTrue(*Check, StepPeak < .00001f);
		else Test->TestTrue(*Check, StepPeak > .0001f && StepPeak <= 1.f);
		StepPeak = 0; StepStarted = Now;
		if (Phase == 0 && ++Theater < DriveIn->Sounds.Num()) return false;
		if (Phase == 0) { Phase = 1; Theater = 0; return false; }
		if (Phase == 1) { Phase = 2; return false; }
		if (Phase == 2 && Movie == 0)
		{
			Test->AddInfo(Controller->ExecuteCheatCodes(TEXT("Lights, Camera, Action!")));
			Test->TestEqual(TEXT("Switching movies replaces eight speakers without duplicates"), DriveIn->Sounds.Num(), 8);
			// This bundled movie contains a silent AAC track. Its silence must not
			// retain the previous soundtrack, and returning to HSI must restore audio.
			Phase = 7; bAwaitingMovie = true; return false;
		}
		if (Phase == 7)
		{
			Test->AddInfo(Controller->ExecuteCheatCodes(TEXT("hsi")));
			++Movie; Theater = 0; Phase = 0; bAwaitingMovie = true; return false;
		}
		if (Phase == 2)
		{
			World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
			Phase = 3; return false;
		}
		if (Phase == 3)
		{
			Test->TestTrue(TEXT("Pausing stops the shared movie clock"), DriveIn->Player->IsPaused());
			World->GetWorldSettings()->SetPauserPlayerState(nullptr);
			Phase = 4; return false;
		}
		if (Phase == 4)
		{
			TArray<FIntPoint> Cleared;
			const FIntPoint Tile = DriveIn->Surfaces[0].Tile;
			City->DemolishBuildingAtTile(Tile.X, Tile.Y, Cleared, true);
			Phase = 5; return false;
		}
		if (Phase == 5)
		{
			Test->TestTrue(TEXT("Other standing theaters keep their movie after demolition"), DriveIn->IsVideoActive());
			Theater = 1; Phase = 6; return false;
		}
		Controller->ExecuteCheatCodes(TEXT("hsi"));
		Test->TestEqual(TEXT("Stopping releases all theater speakers"), DriveIn->Sounds.Num(), 0);
		TArray<UMediaSoundComponent*> RemainingMedia;
		DriveIn->GetComponents(RemainingMedia);
		Test->TestTrue(TEXT("Stopping releases all media sound components"), RemainingMedia.IsEmpty());
		return Finish();
	}
private:
	FAutomationTestBase* Test;
	UGameInstance* Instance = nullptr;
	UWorld* World = nullptr;
	ASimCopterPlayerController* Controller = nullptr;
	ASimCity2000CityActor* City = nullptr;
	ASimCopterDriveInPlayer* DriveIn = nullptr;
	FAudioDeviceHandle Device;
	FDriveInTestMixerModule MixerModule;
	TSharedPtr<FDriveInAudioMeter, ESPMode::ThreadSafe> Meter;
	IConsoleVariable* AppVolume = nullptr;
	int32 OldAppVolume = 0;
	double Started = 0, StepStarted = 0;
	float StepPeak = 0;
	int32 Theater = 0, Movie = 0, Phase = 0;
	bool bAwaitingMovie = true;
	bool Finish()
	{
		if (World) World->GetWorldSettings()->SetPauserPlayerState(nullptr);
		if (DriveIn) DriveIn->StopVideo();
		if (Device && Meter) Device->UnregisterSubmixBufferListener(Meter.ToSharedRef(), Device->GetMainSubmixObject());
		Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
		Device.Reset();
		if (AppVolume) AppVolume->SetWithCurrentPriority(OldAppVolume);
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDriveInAudioTest, "SimCopter.DriveIn.LocalizedAudio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FSimCopterDriveInAudioTest::RunTest(const FString&)
{
	if (!GEngine->GetAudioDeviceManager()) { AddError(TEXT("Run with -DeterministicAudio, without -NoSound")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FSimCopterDriveInAudioCommand(this));
	return true;
}
#endif

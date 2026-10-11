#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Audio/SimCopterRadio.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterCityIntro.h"
#include "Sound/SoundWaveProcedural.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCityIntroAudioTest, "SimCopter.CityIntro.OriginalAudioAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCityIntroAudioTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Audio = World->GetSubsystem<USimCopterAudioSubsystem>();
	auto* Radio = World->GetSubsystem<USimCopterRadioSubsystem>();
	auto* Owner = World->SpawnActor<AActor>();
	auto* Tour = NewObject<USimCopterCityIntro>(Owner);
	Tour->RegisterComponent();
	Tour->BeginPlay();
	TSet<FString> Recordings;
	TArray<FString> Hashes;
	for (int32 City = 0; City < 30; ++City)
	{
		const FString Path = SimCopterOriginalGame::ResolveFile(*FString::Printf(TEXT("sound/cityride/city%d.wav"), City));
		FSimCopterPcmClip Clip;
		if (!TestTrue(*FString::Printf(TEXT("city%d original soundtrack decodes"), City), Audio->DecodeWav(Path, Clip))) return false;
		TestEqual(TEXT("Original sample rate"), Clip.SampleRate, 11025);
		TestEqual(TEXT("Original stereo"), Clip.Channels, 2);
		TestEqual(TEXT("Original full movie sample count"), Clip.Pcm16.Num(), 331450 * 4);
		TestTrue(TEXT("Audio and camera end together within one sample pair"), FMath::Abs(Clip.Duration - FSimCopterCityIntroRoute::Duration) < .0002f);
		Hashes.Add(FMD5::HashBytes(Clip.Pcm16.GetData(), Clip.Pcm16.Num())); Recordings.Add(Hashes.Last());
		Tour->Route.CityIndex = City; Tour->StartMusic();
		if (!TestNotNull(TEXT("Each city creates its own soundtrack"), Tour->Music.Get())) return false;
		auto* Wave = Cast<USoundWaveProcedural>(Tour->Music->Sound);
		TestNotNull(TEXT("PCM source created"), Wave);
		if (Wave) TestEqual(TEXT("Complete original stream is queued"), Wave->GetAvailableAudioByteCount(), Clip.Pcm16.Num());
		TestTrue(TEXT("Music is audible through gameplay pause, without spatial attenuation"), Tour->Music->bIsUISound && !Tour->Music->bAllowSpatialization);
		Tour->Finish();
		TestNull(TEXT("Cleanup works even before the tour's playing flag is set"), Tour->Music.Get());
	}
	TestEqual(TEXT("CD has twelve distinct recordings"), Recordings.Num(), 12);
	TestEqual(TEXT("City16 shares the original city12 track"), Hashes[16], Hashes[12]);
	TestTrue(TEXT("Myrtle Dam uses a different original track from City16"), Hashes[18] != Hashes[16]);
	// Exercise suspension with a classic station without touching the user's sequential-radio save.
	for (int32 I = 0; I < Radio->Stations.Num(); ++I) if (!Radio->Stations[I].bSequential) { Radio->StationIndex = I; break; }
	Radio->bPlayerInHelicopter = true; Radio->bPowered = true;
	const int32 Station = Radio->GetStationIndex(), Cursor = Radio->PatternCursor;
	const FString RadioFile = SimCopterOriginalGame::ResolveFile(TEXT("sound/cityride/city0.wav"));
	Audio->PlayRadioFile(RadioFile);
	Tour->Route.CityIndex = 16; Tour->bPlaying = true; Tour->StartMusic();
	TestFalse(TEXT("Existing radio cannot overlap soundtrack"), Audio->IsRadioPlaying());
	Radio->Tick(1);
	TestEqual(TEXT("Suspended radio does not consume its playlist"), Radio->PatternCursor, Cursor);
	TestTrue(TEXT("Power and possession remain selected"), Radio->IsPowered() && Radio->IsPlayerInHelicopter());
	Tour->UpdateMusic(false);
	TestTrue(TEXT("Switching applications pauses PCM consumption"), Tour->Music->bIsPaused);
	Audio->SetMasterVolume(6000); Tour->UpdateMusic(true);
	TestFalse(TEXT("Focus return resumes the same component"), Tour->Music->bIsPaused);
	TestEqual(TEXT("Music follows original master gain"), Tour->Music->VolumeMultiplier, Audio->VolumeIndexToGain(6000));
	Tour->Elapsed = 1; Tour->RequestSkip();
	Tour->TickComponent(0, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Skip releases intro state"), Tour->IsPlaying());
	TestNull(TEXT("Skip destroys music source"), Tour->Music.Get());
	TestFalse(TEXT("Skip releases radio suspension"), Radio->bCityIntroSuspended);
	TestEqual(TEXT("Radio tuning preserved"), Radio->GetStationIndex(), Station);
	Tour->Route.CityIndex = INDEX_NONE; Tour->StartMusic();
	TestNotNull(TEXT("Imported cities use city0's soundtrack"), Tour->Music.Get());
	Tour->EndPlay(EEndPlayReason::LevelTransition);
	TestNull(TEXT("Level transition destroys soundtrack"), Tour->Music.Get());
	TestFalse(TEXT("Level transition releases radio"), Radio->bCityIntroSuspended);
	Tour->Finish();
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioMixerDevice.h"
#include "AudioMixer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "ISubmixBufferListener.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

class FCityIntroMixerModule : public IAudioDeviceModule
{
public:
	virtual FAudioDevice* CreateAudioDevice() override
	{
		auto& Platform = FModuleManager::LoadModuleChecked<IAudioDeviceModule>(TEXT("AudioMixerXAudio2"));
		return new Audio::FMixerDevice(Platform.CreateAudioMixerPlatformInterface());
	}
};

// Measure the real Windows mix, then clear it before hardware output.
class FCityIntroAudioMeter : public ISubmixBufferListener
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

class FSimCopterCityIntroMixerCommand : public IAutomationLatentCommand
{
public:
	explicit FSimCopterCityIntroMixerCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (!World)
		{
			Instance = NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
			World = Instance->GetWorld(); World->bAllowAudioPlayback = true;
			FAudioDeviceParams Params; Params.AssociatedWorld = World; Params.Scope = EAudioDeviceScope::Unique; Params.AudioModule = &MixerModule;
			Device = GEngine->GetAudioDeviceManager()->RequestAudioDevice(Params);
			if (!Test->TestTrue(TEXT("Private Windows mixer available"), Device.IsValid())) return Finish();
			World->SetAudioDevice(Device);
			AppVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableAppVolume"));
			OldAppVolume = AppVolume->GetInt(); AppVolume->SetWithCurrentPriority(1);
			Meter = MakeShared<FCityIntroAudioMeter, ESPMode::ThreadSafe>();
			Device->RegisterSubmixBufferListener(Meter.ToSharedRef(), Device->GetMainSubmixObject());
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
			Tour = NewObject<USimCopterCityIntro>(World->SpawnActor<AActor>()); Tour->RegisterComponent();
			if (!Tour->HasBegunPlay()) Tour->BeginPlay();
			Tour->Route.CityIndex = 16; Tour->StartMusic(); Tour->UpdateMusic(true);
			Started = StepStarted = FPlatformTime::Seconds();
		}
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 45) { Test->AddError(TEXT("Timed out measuring city intro audio")); return Finish(); }
		Device->SetDeviceMuted(false); Device->Update(false); // Gameplay remains paused.
		const float Peak = Meter->TakePeak();
		if (Now - StepStarted < .5) return false; // Drain buffers from the previous state.
		StepPeak = FMath::Max(StepPeak, Peak);
		if (Now - StepStarted < 2) return false;
		const TCHAR* Names[] = {TEXT("city16_playing"), TEXT("focus_paused"), TEXT("focus_resumed"), TEXT("skipped"),
			TEXT("myrtle_restarted"), TEXT("natural_finish"), TEXT("restarted_again"), TEXT("level_teardown")};
		Test->AddInfo(FString::Printf(TEXT("%s output peak %.6f"), Names[Phase], StepPeak));
		const bool bSilent = Phase == 1 || Phase == 3 || Phase == 5 || Phase == 7;
		Test->TestTrue(Names[Phase], bSilent ? StepPeak < .00001f : StepPeak > .0001f && StepPeak <= 1.f);
		StepPeak = 0; StepStarted = Now;
		switch (Phase++)
		{
		case 0: Tour->UpdateMusic(false); break;
		case 1: Tour->UpdateMusic(true); break;
		case 2:
			Tour->bPlaying = true; Tour->Elapsed = 1; Tour->RequestSkip(); Tour->TickComponent(0, LEVELTICK_All, nullptr); break;
		case 3:
			Tour->Route.CityIndex = 18; Tour->StartMusic(); Tour->UpdateMusic(true); break;
		case 4:
			Tour->bPlaying = true; Tour->bSkipRequested = false; Tour->Elapsed = FSimCopterCityIntroRoute::Duration;
			Tour->TickComponent(0, LEVELTICK_All, nullptr); break;
		case 5: Tour->StartMusic(); Tour->UpdateMusic(true); break;
		case 6: Tour->EndPlay(EEndPlayReason::LevelTransition); break;
		default: return Finish();
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	UGameInstance* Instance = nullptr;
	UWorld* World = nullptr;
	USimCopterCityIntro* Tour = nullptr;
	FAudioDeviceHandle Device;
	FCityIntroMixerModule MixerModule;
	TSharedPtr<FCityIntroAudioMeter, ESPMode::ThreadSafe> Meter;
	IConsoleVariable* AppVolume = nullptr;
	int32 OldAppVolume = 0, Phase = 0;
	double Started = 0, StepStarted = 0;
	float StepPeak = 0;
	bool Finish()
	{
		if (Tour) Tour->Finish();
		if (World) World->GetWorldSettings()->SetPauserPlayerState(nullptr);
		if (Device && Meter) Device->UnregisterSubmixBufferListener(Meter.ToSharedRef(), Device->GetMainSubmixObject());
		if (Instance) Instance->Shutdown();
		if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
		Device.Reset();
		if (AppVolume) AppVolume->SetWithCurrentPriority(OldAppVolume);
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCityIntroMixerTest, "SimCopter.CityIntro.OriginalAudioMixer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FSimCopterCityIntroMixerTest::RunTest(const FString&)
{
	if (!GEngine->GetAudioDeviceManager()) { AddError(TEXT("Run with -DeterministicAudio, without -NoSound")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FSimCopterCityIntroMixerCommand(this));
	return true;
}
#endif

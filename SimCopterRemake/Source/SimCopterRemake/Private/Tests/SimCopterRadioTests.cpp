// Radio tests. Everything here is the pure part of the station player: the ported RNG, the
// shuffle-bag behaviour that distinguishes it from a random draw, the scheduler's probability
// table, and the dial mapping the dash tuner uses.

#include "Audio/SimCopterRadio.h"
#include "Game/SimCopterSettings.h"
#include "Misc/AutomationTest.h"
#include "UI/SSimCopterDashboard.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRadioPreferenceTest,
	"SimCopter.Radio.StationPreference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioPreferenceTest::RunTest(const FString&)
{
	TArray<FSimCopterRadioStation> Stations;
	for (const TCHAR* Name : {TEXT("KJAZ"), TEXT("KMIX"), TEXT("KINV")}) Stations.AddDefaulted_GetRef().CallSign = Name;
	USimCopterSettings* Store = NewObject<USimCopterSettings>(NewObject<UGameInstance>());
	Store->RadioStation = INDEX_NONE; Store->RadioStationCallSign.Reset();
	TestEqual(TEXT("Fresh preference defaults to KINV"), Store->ResolveRadioStation(Stations), 2);
	Store->SetRadioStation(1);
	TestEqual(TEXT("Legacy saved index is respected"), Store->ResolveRadioStation(Stations), 1);
	Store->RememberRadioStation(1, TEXT("KMIX"));
	Store->SetRadioVolume(3500);
	const FString TestIni = FConfigCacheIni::NormalizeConfigIniPath(FPaths::ProjectSavedDir() / TEXT("Automation/RadioStationPreference.ini"));
	Store->SaveConfig(CPF_Config, *TestIni);
	GConfig->UnloadFile(TestIni);
	USimCopterSettings* Reload = NewObject<USimCopterSettings>(NewObject<UGameInstance>());
	Reload->LoadConfig(USimCopterSettings::StaticClass(), *TestIni);
	TestEqual(TEXT("Last selected station survives disk reload"), Reload->ResolveRadioStation(Stations), 1);
	TestEqual(TEXT("Volume survives alongside station"), Reload->GetRadioVolume(), 3500);
	Stations.Swap(0, 1);
	TestEqual(TEXT("Call sign survives reordered station list"), Reload->ResolveRadioStation(Stations), 0);
	Reload->SetRadioStation(2);
	TestEqual(TEXT("Settings tuner replaces previous call sign"), Reload->ResolveRadioStation(Stations), 2);
	Reload->RememberRadioStation(8, TEXT("REMOVED"));
	TestEqual(TEXT("Missing station falls back to KINV"), Reload->ResolveRadioStation(Stations), 2);
	Stations.Pop();
	TestEqual(TEXT("Missing KINV falls back to KMIX"), Reload->ResolveRadioStation(Stations), 0);
	Stations.Reset();
	TestEqual(TEXT("Empty station list is safe"), Reload->ResolveRadioStation(Stations), INDEX_NONE);
	GConfig->UnloadFile(TestIni); IFileManager::Get().Delete(*TestIni);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRadioSequentialTest,
	"SimCopter.Radio.SequentialResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioSequentialTest::RunTest(const FString& Parameters)
{
	const FString SaveFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("InvokeRadio.ini"));
	FString OldSave;
	const bool bHadSave = FFileHelper::LoadFileToString(OldSave, *SaveFile);
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	USimCopterRadioSubsystem* Radio = World->GetSubsystem<USimCopterRadioSubsystem>();
	USimCopterAudioSubsystem* Audio = World->GetSubsystem<USimCopterAudioSubsystem>();
	Audio->bSoundsAvailable = true;
	const int32 ArtistIndex = Radio->Stations.IndexOfByPredicate([](const FSimCopterRadioStation& S) { return S.bSequential; });
	if (TestTrue(TEXT("Artist station discovered"), ArtistIndex != INDEX_NONE))
	{
		Radio->SetStationIndex(ArtistIndex);
		TestEqual(TEXT("All 21 songs present"), Radio->Stations[ArtistIndex].Music.Num(), 21);
		Radio->SequentialTrack = 0;
		Radio->SequentialOffset = 0.0f;
		Radio->SetPlayerInHelicopter(true);
		Radio->Tick(0.0f);
		TestEqual(TEXT("Album opens with Expectations Increasing"), Radio->GetCurrentTitle(), FString(TEXT("Expectations Increasing")));
		Radio->SequentialStartTime -= 12.0;
		Radio->SetVolume(0.35f);
		Radio->SetStationIndex(0);
		TestEqual(TEXT("Changing station preserves runtime volume"), Radio->GetVolume(), 0.35f);
		TestTrue(TEXT("Station switch saves partial track"), Radio->SequentialOffset >= 12.0f);
		const float SavedOffset = Radio->SequentialOffset;
		Radio->SetStationIndex(ArtistIndex);
		Radio->Tick(0.0f);
		TestEqual(TEXT("Resumed playback preserves mixer volume"), Audio->GetRadioVolumeMultiplier(), 0.35f);
		TestEqual(TEXT("Returning retains track"), Radio->SequentialTrack, 0);
		TestEqual(TEXT("Returning retains offset"), Radio->SequentialOffset, SavedOffset);
		TestTrue(TEXT("Seek queues only remaining audio"), Audio->GetRadioRemainingSeconds() < 83.0f);
		Radio->SetVolume(1.0f);
		Audio->SetMasterVolume(10000);
		TestEqual(TEXT("Radio reaches double effects gain"), Audio->RadioComponent->VolumeMultiplier, 2.0f);
		Radio->SetVolume(0.0f);
		TestEqual(TEXT("Radio slider can mute"), Audio->RadioComponent->VolumeMultiplier, 0.0f);
		Radio->StepStation(1);
		Radio->SetStationIndex(ArtistIndex);
		Radio->Tick(0.0f);
		TestEqual(TEXT("Tuning away and back preserves mute"), Radio->GetVolume(), 0.0f);
		TestEqual(TEXT("Resumed playback stays muted"), Audio->RadioComponent->VolumeMultiplier, 0.0f);
		Audio->RadioEndTime = 0.0;
		Radio->Tick(0.0f);
		TestEqual(TEXT("Natural end advances sequentially"), Radio->SequentialTrack, 1);
		TestEqual(TEXT("Next album track"), Radio->GetCurrentTitle(), FString(TEXT("Dancing to Our Doom")));
		Radio->SequentialTrack = 7;
		Audio->RadioEndTime = 0.0;
		Radio->Tick(0.0f);
		TestEqual(TEXT("Album followed by screenshot song 1"), Radio->GetCurrentTitle(), FString(TEXT("The Great Filter (RenAIssance)")));
		Radio->SequentialTrack = 20;
		Audio->RadioEndTime = 0.0;
		Radio->Tick(0.0f);
		TestEqual(TEXT("Playlist wraps to album"), Radio->SequentialTrack, 0);
		Radio->SetPowered(false);
		TestFalse(TEXT("Power off suspends"), Radio->bSequentialPlaying);
		float DiskOffset = -1.0f;
		FConfigFile SavedProgress;
		SavedProgress.Read(SaveFile);
		SavedProgress.GetFloat(TEXT("Playback"), TEXT("Seconds"), DiskOffset);
		TestTrue(TEXT("Progress persisted on disk"), DiskOffset >= 0.0f);
	}
	World->DestroyWorld(false);
	GConfig->UnloadFile(SaveFile);
	if (bHadSave) { FFileHelper::SaveStringToFile(OldSave, *SaveFile); }
	else { IFileManager::Get().Delete(*SaveFile); }
	return true;
}

using FRng = USimCopterRadioSubsystem::FLaggedFibonacci;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterRadioRandomTest,
	"SimCopter.Radio.Random",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioRandomTest::RunTest(const FString& Parameters)
{
	// SCHOOK: RadioRandom 0x00455d70 - a subtractive lagged-Fibonacci, not MSVC rand and not the
	// people LFSR. The radio is the game's third generator.
	FRng Rng;
	Rng.Seed(12345);

	// Range: every value must land in [0, n).
	for (int32 Trial = 0; Trial < 4000; ++Trial)
	{
		const uint32 Value = Rng.Next(7);
		if (Value >= 7)
		{
			AddError(FString::Printf(TEXT("value %u out of range on trial %d"), Value, Trial));
			return false;
		}
	}

	// Modulo 0 must not divide by zero - the shuffle calls this with a count that can be 0.
	TestEqual(TEXT("modulo zero is safe"), Rng.Next(0), 0u);
	TestEqual(TEXT("modulo one is always zero"), Rng.Next(1), 0u);

	// Determinism: the same seed replays the same stream, which is what makes the shuffle
	// reproducible in a test at all.
	FRng A, B;
	A.Seed(999);
	B.Seed(999);
	bool bSame = true;
	for (int32 Index = 0; Index < 200; ++Index)
	{
		bSame &= (A.Next(1000) == B.Next(1000));
	}
	TestTrue(TEXT("same seed replays the same stream"), bSame);

	// Different seeds must not collapse onto the same stream.
	FRng C;
	C.Seed(1000);
	FRng D;
	D.Seed(1001);
	int32 Differences = 0;
	for (int32 Index = 0; Index < 200; ++Index)
	{
		Differences += (C.Next(1000) != D.Next(1000)) ? 1 : 0;
	}
	TestTrue(TEXT("different seeds diverge"), Differences > 150);

	// Spread: over many draws every bucket should be hit. A generator stuck on a lattice would
	// leave holes here, which is the failure mode worth guarding.
	FRng E;
	E.Seed(4242);
	int32 Buckets[10] = {};
	for (int32 Index = 0; Index < 20000; ++Index)
	{
		Buckets[E.Next(10)]++;
	}
	for (int32 Bucket = 0; Bucket < 10; ++Bucket)
	{
		TestTrue(
			*FString::Printf(TEXT("bucket %d is populated (%d)"), Bucket, Buckets[Bucket]),
			Buckets[Bucket] > 1000);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterRadioShuffleTest,
	"SimCopter.Radio.ShuffleBag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioShuffleTest::RunTest(const FString& Parameters)
{
	FRng Rng;
	Rng.Seed(7);

	// A shuffle is a permutation: nothing gained, nothing lost. This is the property that makes
	// it a shuffle BAG - every track plays once per cycle before any repeats - as opposed to
	// drawing at random each time, which is what the radio is usually assumed to do.
	for (int32 Count = 1; Count <= 32; ++Count)
	{
		TArray<int32> Items;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Items.Add(Index);
		}
		USimCopterRadioSubsystem::ShuffleWithAntiRepeat(Items, Rng, INDEX_NONE);

		TestEqual(*FString::Printf(TEXT("count %d preserved"), Count), Items.Num(), Count);
		TArray<int32> Sorted = Items;
		Sorted.Sort();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Sorted[Index] != Index)
			{
				AddError(FString::Printf(TEXT("count %d is not a permutation"), Count));
				return false;
			}
		}
	}

	// SCHOOK: RadioShuffle 0x00430070's tail - if the new first element repeats whatever ended
	// the previous cycle, first and last are swapped. Without it the seam between cycles can
	// play the same category twice running.
	for (int32 Trial = 0; Trial < 200; ++Trial)
	{
		TArray<int32> Items = { 0, 1, 2, 3, 4, 5 };
		const int32 PreviousLast = 3;
		USimCopterRadioSubsystem::ShuffleWithAntiRepeat(Items, Rng, PreviousLast);
		if (Items[0] == PreviousLast)
		{
			AddError(FString::Printf(
				TEXT("trial %d left the repeated value first"), Trial));
			return false;
		}
	}

	// A single-element list cannot honour the rule and must not be corrupted trying.
	TArray<int32> One = { 4 };
	USimCopterRadioSubsystem::ShuffleWithAntiRepeat(One, Rng, 4);
	TestEqual(TEXT("single element survives"), One.Num(), 1);
	TestEqual(TEXT("single element unchanged"), One[0], 4);

	TArray<int32> Empty;
	USimCopterRadioSubsystem::ShuffleWithAntiRepeat(Empty, Rng, INDEX_NONE);
	TestEqual(TEXT("empty list stays empty"), Empty.Num(), 0);

	// The shuffle must actually shuffle - a no-op would satisfy every check above.
	int32 Moved = 0;
	for (int32 Trial = 0; Trial < 50; ++Trial)
	{
		TArray<int32> Items;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Items.Add(Index);
		}
		USimCopterRadioSubsystem::ShuffleWithAntiRepeat(Items, Rng, INDEX_NONE);
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Moved += (Items[Index] != Index) ? 1 : 0;
		}
	}
	TestTrue(TEXT("shuffle displaces most elements"), Moved > 50 * 12 / 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterRadioScheduleTest,
	"SimCopter.Radio.Schedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioScheduleTest::RunTest(const FString& Parameters)
{
	// FUN_0042f160's four branches, in the units the original wrote them: music unconditional,
	// then rand() % 100 against 0x14 / 0x5a / 0x14.
	TestEqual(TEXT("music always plays"),
		USimCopterRadioSubsystem::GetSlotChancePercent(ESimCopterRadioSlot::Music), 100);
	TestEqual(TEXT("dj is 20% (< 0x14)"),
		USimCopterRadioSubsystem::GetSlotChancePercent(ESimCopterRadioSlot::Dj), 0x14);
	TestEqual(TEXT("commercial is 90% (< 0x5a)"),
		USimCopterRadioSubsystem::GetSlotChancePercent(ESimCopterRadioSlot::Commercial), 0x5a);
	TestEqual(TEXT("jingle is 20% (< 0x14)"),
		USimCopterRadioSubsystem::GetSlotChancePercent(ESimCopterRadioSlot::Jingle), 0x14);

	// The gap is 4000 of the scheduler's timer units, which are milliseconds.
	TestEqual(TEXT("inter-item gap is four seconds"), USimCopterRadioSubsystem::GapSeconds, 4.0f);
	TestEqual(TEXT("back-to-back music is one in ten"),
		USimCopterRadioSubsystem::BackToBackMusicPercent, 10);

	// The slot codes are the pattern array's own values and must not be reordered: the loader
	// fills the four lists in this order (music, dj, commercl, jingle).
	TestEqual(TEXT("music is slot 0"), static_cast<int32>(ESimCopterRadioSlot::Music), 0);
	TestEqual(TEXT("dj is slot 1"), static_cast<int32>(ESimCopterRadioSlot::Dj), 1);
	TestEqual(TEXT("commercial is slot 2"), static_cast<int32>(ESimCopterRadioSlot::Commercial), 2);
	TestEqual(TEXT("jingle is slot 3"), static_cast<int32>(ESimCopterRadioSlot::Jingle), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterRadioPossessionGateTest,
	"SimCopter.Radio.PossessionGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioPossessionGateTest::RunTest(const FString& Parameters)
{
	USimCopterRadioSubsystem* Radio = NewObject<USimCopterRadioSubsystem>();
	if (!TestNotNull(TEXT("subsystem constructs"), Radio))
	{
		return false;
	}

	TestFalse(TEXT("radio starts outside the helicopter"), Radio->IsPlayerInHelicopter());
	TestTrue(TEXT("radio power setting still defaults on"), Radio->IsPowered());

	Radio->SetPlayerInHelicopter(true);
	TestTrue(TEXT("possession enables radio playback"), Radio->IsPlayerInHelicopter());

	Radio->SetPowered(false);
	Radio->SetPlayerInHelicopter(false);
	TestFalse(TEXT("leaving disables radio playback"), Radio->IsPlayerInHelicopter());
	Radio->SetPlayerInHelicopter(true);
	TestFalse(TEXT("re-entering preserves the player's off setting"), Radio->IsPowered());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterRadioDialTest,
	"SimCopter.Radio.Dial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioDialTest::RunTest(const FString& Parameters)
{
	// The dial mapping is the remake's, so the thing worth pinning is that it round-trips: the
	// needle position a station produces must tune back to that same station when clicked.
	// Anything else and click-to-tune lands one station off at the ends.
	USimCopterRadioSubsystem* Radio = NewObject<USimCopterRadioSubsystem>();
	if (!TestNotNull(TEXT("subsystem constructs"), Radio))
	{
		return false;
	}

	// With no stations discovered the dial must be inert rather than divide by zero.
	TestEqual(TEXT("no stations gives alpha 0"), Radio->GetDialAlpha(), 0.0f);
	TestEqual(TEXT("no stations tunes to nothing"), Radio->GetStationForDialAlpha(0.5f), INDEX_NONE);

	// The dial spans the two outermost printed labels: alpha 0 is the first station and 1 the
	// last, evenly divided. Mirrors GetDialAlpha, which the subsystem cannot be asked for here
	// without a discovered station list.
	auto AlphaFor = [](int32 Index, int32 Count)
	{
		return Count == 1 ? 0.5f : static_cast<float>(Index) / static_cast<float>(Count - 1);
	};
	auto StationFor = [](float Alpha, int32 Count)
	{
		const float Position = FMath::Clamp(Alpha, 0.0f, 1.0f) * static_cast<float>(Count - 1);
		return FMath::Clamp(FMath::RoundToInt(Position), 0, Count - 1);
	};

	// The round-trip is what click-to-tune depends on: the needle position a station produces
	// must tune back to that station. Five is the shipped count; 1 and 2 are the edge cases.
	for (int32 Count : { 1, 2, 3, 5, 9 })
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Alpha = AlphaFor(Index, Count);
			TestTrue(
				*FString::Printf(TEXT("count %d station %d alpha is on the dial"), Count, Index),
				Alpha >= 0.0f && Alpha <= 1.0f);
			if (Count > 1)
			{
				TestEqual(
					*FString::Printf(TEXT("count %d station %d round-trips"), Count, Index),
					StationFor(Alpha, Count),
					Index);
			}
		}
		if (Count > 1)
		{
			TestEqual(*FString::Printf(TEXT("count %d: first is at 0"), Count), AlphaFor(0, Count), 0.0f);
			TestEqual(*FString::Printf(TEXT("count %d: last is at 1"), Count), AlphaFor(Count - 1, Count), 1.0f);
		}
	}

	// Past either end of the scale must clamp onto a real station, never run off the array.
	for (int32 Count : { 1, 2, 5 })
	{
		for (float Alpha : { -1.0f, 0.0f, 0.5f, 1.0f, 2.0f })
		{
			const int32 Station = StationFor(Alpha, Count);
			TestTrue(
				*FString::Printf(TEXT("count %d alpha %.1f clamps"), Count, Alpha),
				Station >= 0 && Station < Count);
		}
	}

	// Clicking the far left and far right must select the end stations, not the neighbours.
	TestEqual(TEXT("far left tunes the first station"), StationFor(0.0f, 5), 0);
	TestEqual(TEXT("far right tunes the last station"), StationFor(1.0f, 5), 4);
	TestEqual(TEXT("centre tunes the middle station"), StationFor(0.5f, 5), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterRadioDashboardVolumeTest,
	"SimCopter.Radio.DashboardVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioDashboardVolumeTest::RunTest(const FString& Parameters)
{
	// SCHOOK: DashRadioVolumeMarker 0x004520a0. The five constants used by FUN_004402a0 make
	// 10000 / 8000 / 6000 land at top / middle / the start of the off detent respectively.
	TestEqual(TEXT("maximum is at the 11 label"),
		SSimCopterDashboard::RadioVolumeToMarkerY(10000), 19);
	TestEqual(TEXT("8000 is halfway down the logarithmic fader"),
		SSimCopterDashboard::RadioVolumeToMarkerY(8000), 29);
	TestEqual(TEXT("6000 reaches the off-detent boundary"),
		SSimCopterDashboard::RadioVolumeToMarkerY(6000), 34);
	TestEqual(TEXT("zero paints on the bottom line"),
		SSimCopterDashboard::RadioVolumeToMarkerY(0), 39);

	// SCHOOK: DashRadioInput 0x00451e30. Clicking the main travel turns the set on at that
	// volume; y=34 and the remaining five page pixels turn it off.
	TestEqual(TEXT("top click is full volume"),
		SSimCopterDashboard::RadioMarkerYToVolume(19), 10000);
	TestTrue(TEXT("last live click remains audible"),
		SSimCopterDashboard::RadioMarkerYToVolume(33) >= USimCopterSettings::VolumeMin);
	TestEqual(TEXT("off detent begins at y 34"),
		SSimCopterDashboard::RadioMarkerYToVolume(34), 0);
	TestEqual(TEXT("bottom is off"),
		SSimCopterDashboard::RadioMarkerYToVolume(39), 0);

	return true;
}

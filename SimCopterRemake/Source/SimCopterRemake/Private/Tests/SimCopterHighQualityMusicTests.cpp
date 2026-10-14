#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Audio/SimCopterHighQualityMusic.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Audio/SimCopterRadio.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWaveProcedural.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterFlacDecodeTest,
	"SimCopter.Sound.HighQualityMusic.Decode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterFlacDecodeTest::RunTest(const FString&)
{
	FString Json;
	TSharedPtr<FJsonObject> Document;
	if (!TestTrue(TEXT("Verified soundtrack manifest present"), FFileHelper::LoadFileToString(Json,
		*(FPaths::ProjectConfigDir() / TEXT("HighQualityMusic.json"))))) return false;
	if (!TestTrue(TEXT("Manifest parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Document))) return false;
	const auto& Tracks = Document->GetArrayField(TEXT("tracks"));
	TestEqual(TEXT("Only the fifteen verified masters"), Tracks.Num(), 15);
	const FString Root = SimCopterOriginalGame::ResolveDirectory(TEXT("sound"));
	for (const auto& Value : Tracks)
	{
		const auto Track = Value->AsObject();
		const FString Path = Root / Track->GetStringField(TEXT("file"));
		FSimCopterPcmClip Clip;
		const double Start = FPlatformTime::Seconds();
		if (!TestTrue(*Path, SimCopterHighQualityMusic::DecodeFlac(Path, Clip))) continue;
		AddInfo(FString::Printf(TEXT("%s decoded in %.3f seconds"), *FPaths::GetCleanFilename(Path), FPlatformTime::Seconds() - Start));
		TestEqual(TEXT("Full stereo master preserved"), Clip.Channels, 2);
		TestEqual(TEXT("Original master sample rate preserved"), Clip.SampleRate, 44100);
		TestEqual(TEXT("Complete track, including longer edits"), Clip.Pcm16.Num() / 4, Track->GetIntegerField(TEXT("frames")));
		TestEqual(TEXT("Bit-exact match to independent FFmpeg PCM decode"), FMD5::HashBytes(Clip.Pcm16.GetData(), Clip.Pcm16.Num()), Track->GetStringField(TEXT("pcm_md5")));
	}
	const FString Bad = FPaths::ProjectSavedDir() / TEXT("Automation/TruncatedMusic.flac");
	TArray<uint8> Encoded;
	FFileHelper::LoadFileToArray(Encoded, *(Root / Tracks[0]->AsObject()->GetStringField(TEXT("file"))));
	Encoded.SetNum(2048);
	FFileHelper::SaveArrayToFile(Encoded, *Bad);
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*Bad); };
	FSimCopterPcmClip Clip;
	TestFalse(TEXT("Truncated master rejected"), SimCopterHighQualityMusic::DecodeFlac(Bad, Clip));
	TestFalse(TEXT("Failure never exposes partial samples"), Clip.IsValid());
	Encoded.Init(0, 64); FFileHelper::SaveArrayToFile(Encoded, *Bad);
	TestFalse(TEXT("Invalid header rejected"), SimCopterHighQualityMusic::DecodeFlac(Bad, Clip));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterFlacMappingTest,
	"SimCopter.Sound.HighQualityMusic.Mapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterFlacMappingTest::RunTest(const FString&)
{
	const FString Root = SimCopterOriginalGame::ResolveDirectory(TEXT("sound"));
	const auto& Entries = SimCopterHighQualityMusic::GetReplacements();
	TestEqual(TEXT("29 originals map to 15 shared files"), Entries.Num(), 29);
	TSet<FString> Distinct;
	for (const auto& Entry : Entries)
	{
		const FString Path = SimCopterHighQualityMusic::Resolve(Root, Root / Entry.Original);
		TestFalse(*Entry.Original, Path.IsEmpty()); Distinct.Add(Path);
	}
	TestEqual(TEXT("Shared physical paths"), Distinct.Num(), 15);
	TestTrue(TEXT("Unmatched jazz preserved"), SimCopterHighQualityMusic::Resolve(Root, Root / TEXT("radio/stations/jazz/music/jazz3mx1.wav")).IsEmpty());
	TestTrue(TEXT("Menu music preserved"), SimCopterHighQualityMusic::Resolve(Root, Root / TEXT("menuback.wav")).IsEmpty());
	if (Entries.IsEmpty()) return false;
	const auto& Entry = Entries[0];
	const FString Temp = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString();
	const FString Original = Temp / Entry.Original, Replacement = Temp / Entry.Flac;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Original), true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Replacement), true);
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Temp, false, true); };
	TestTrue(TEXT("Unavailable upgrade leaves fallback"), SimCopterHighQualityMusic::Resolve(Temp, Original).IsEmpty());
	FFileHelper::SaveStringToFile(TEXT("Presence marker for path-resolution fixture"), *Replacement);
	TArray<FString> Playlist;
	SimCopterHighQualityMusic::UpgradePlaylist(Temp, FPaths::GetPath(Original), Playlist);
	SimCopterHighQualityMusic::UpgradePlaylist(Temp, FPaths::GetPath(Original), Playlist);
	TestEqual(TEXT("Removed original restored exactly once in packaged playlist"), Playlist.Num(), 1);
	FFileHelper::SaveStringToFile(TEXT("User recording with a retail filename"), *Original);
	TestTrue(TEXT("Different recording is never replaced by filename alone"), SimCopterHighQualityMusic::Resolve(Temp, Original).IsEmpty());
	Playlist = { Original };
	SimCopterHighQualityMusic::UpgradePlaylist(Temp, FPaths::GetPath(Original), Playlist);
	TestEqual(TEXT("Custom WAV remains the sole entry"), Playlist.Num(), 1);
	TestEqual(TEXT("Custom recording kept"), Playlist[0], Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterFlacPlaybackTest,
	"SimCopter.Sound.HighQualityMusic.Playback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterFlacPlaybackTest::RunTest(const FString&)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Audio = World->GetSubsystem<USimCopterAudioSubsystem>();
	auto* Radio = World->GetSubsystem<USimCopterRadioSubsystem>();
	Audio->bSoundsAvailable = true;
	const FString Root = Audio->GetSoundRoot();
	const FString Hangar = Audio->ResolveWavPath(TEXT("HANGAR"), SimCopterSound::ESoundDir::Root);
	TestTrue(TEXT("Original hangar call resolves shared FLAC"), Hangar.EndsWith(TEXT(".flac")));
	TestTrue(TEXT("Hangar music playback starts"), Audio->PlayFile2D(TEXT("HANGAR"), SimCopterSound::ESoundDir::Root));
	for (const auto& Station : Radio->Stations)
	{
		const FString Sign = Station.CallSign.ToUpper();
		if (Sign == TEXT("KJAZ") || Sign == TEXT("KROC") || Sign == TEXT("KTEC"))
			TestEqual(*FString::Printf(TEXT("%s retains six songs"), *Sign), Station.Music.Num(), 6);
		if (Sign == TEXT("KMIX")) TestEqual(TEXT("Mix retains every original song"), Station.Music.Num(), 25);
	}
	const FString Original = Root / TEXT("radio/stations/rock/music/rock1mx2.wav");
	const FString Rock = SimCopterHighQualityMusic::Resolve(Root, Original);
	TestEqual(TEXT("Genre and mix play the identical physical file"), Rock,
		SimCopterHighQualityMusic::Resolve(Root, Root / TEXT("radio/stations/mix/music/rock1mx2.wav")));
	if (TestTrue(TEXT("Lossless radio starts at requested offset"), Audio->PlayRadioFile(Rock, .35f, 12.0f)))
	{
		TestTrue(TEXT("FLAC decode runs asynchronously"), Audio->PendingRadioClip.IsValid());
		TestTrue(TEXT("Scheduler treats loading as active"), Audio->IsRadioPlaying());
		Audio->SetRadioVolumeMultiplier(0);
		Audio->SetRadioPaused(true);
		const double Deadline = FPlatformTime::Seconds() + 10;
		while (Audio->PendingRadioClip.IsValid() && FPlatformTime::Seconds() < Deadline)
		{
			Audio->UpdateRadioPlayback(); FPlatformProcess::Sleep(.005f);
		}
		if (!TestNotNull(TEXT("Asynchronous decode creates radio component"), Audio->RadioComponent.Get())) return false;
		TestEqual(TEXT("Changing volume while loading is respected"), Audio->RadioComponent->VolumeMultiplier, 0.0f);
		auto* Wave = Cast<USoundWaveProcedural>(Audio->RadioComponent->Sound);
		if (TestNotNull(TEXT("Radio owns a playable PCM wave"), Wave))
		{
			TestEqual(TEXT("Radio preserves stereo"), Wave->NumChannels, 2);
			TestTrue(TEXT("Only remaining frames queued"), Wave->GetAvailableAudioByteCount() > 50 * 1024 * 1024);
		}
		TestTrue(TEXT("Seek reduces duration"), Audio->GetRadioRemainingSeconds() > 299 && Audio->GetRadioRemainingSeconds() < 302);
		TestTrue(TEXT("Pause while decoding is retained on completion"),Audio->IsRadioPaused());
		TestTrue(TEXT("Decoded paused FLAC reports requested offset"),FMath::IsNearlyEqual(Audio->GetRadioPositionSeconds(),12.f,.02f));
		TestTrue(TEXT("FLAC transport can seek while paused"),Audio->SeekRadio(60.f));
		TestTrue(TEXT("Seek retains pause during a replacement decode"),Audio->IsRadioPaused() && Audio->PendingRadioClip.IsValid());
		const double SeekDeadline=FPlatformTime::Seconds()+10;
		while (Audio->PendingRadioClip.IsValid() && FPlatformTime::Seconds()<SeekDeadline)
		{ Audio->UpdateRadioPlayback(); FPlatformProcess::Sleep(.005f); }
		TestFalse(TEXT("Seek decode completes"),Audio->PendingRadioClip.IsValid());
		TestTrue(TEXT("Paused seek resumes at correct offset"),Audio->IsRadioPaused() && FMath::IsNearlyEqual(Audio->GetRadioPositionSeconds(),60.f,.02f));
		Audio->SetRadioPaused(false); TestFalse(TEXT("Decoded FLAC resumes"),Audio->IsRadioPaused());
		Audio->SetRadioVolumeMultiplier(0);
		TestEqual(TEXT("Mute still works"), Audio->RadioComponent->VolumeMultiplier, 0.0f);
		Audio->StopRadio(); TestFalse(TEXT("Stop retires FLAC playback"), Audio->IsRadioPlaying());
		Audio->PlayRadioFile(Rock);
		const auto Cancelled = Audio->RadioDecodeCancelled;
		Audio->StopRadio();
		TestTrue(TEXT("Tuning/power-off cancels worker"), Cancelled && Cancelled->Load());
		TestFalse(TEXT("Cancelled completion cannot resurrect playback"), Audio->PendingRadioClip.IsValid());
		Audio->PlayRadioFile(Rock);
		const auto Superseded = Audio->RadioDecodeCancelled;
		Audio->PlayRadioFile(Root / TEXT("radio/stations/classic/music/op13.wav"));
		TestTrue(TEXT("Switching to WAV cancels prior FLAC"), Superseded && Superseded->Load());
		TestFalse(TEXT("WAV station has no stale request"), Audio->PendingRadioClip.IsValid());
	}
	return true;
}
#endif

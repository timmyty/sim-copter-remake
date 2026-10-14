#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/SimCopterRadio.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Audio/SimCopterCompressedMusic.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterOptionalMusicTest,
	"SimCopter.Radio.OptionalMusic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterOptionalMusicTest::RunTest(const FString&)
{
	const FString Temp = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString();
	const FString SaveFile = FPaths::ProjectSavedDir() / TEXT("InvokeRadio.ini");
	FString OldSave;
	const bool HadSave = FFileHelper::LoadFileToString(OldSave, *SaveFile);
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		IFileManager::Get().DeleteDirectory(*Temp, false, true);
		if (HadSave) FFileHelper::SaveStringToFile(OldSave, *SaveFile);
		else IFileManager::Get().Delete(*SaveFile);
	};
	auto* Radio = World->GetSubsystem<USimCopterRadioSubsystem>();
	auto* Audio = World->GetSubsystem<USimCopterAudioSubsystem>();
	Audio->bSoundsAvailable = true;
	const int32 BaseCount = Radio->Stations.Num();
	const int32 KinvTrack = Radio->SequentialTrack;
	const auto FinishDecode = [&]
	{
		const double Deadline = FPlatformTime::Seconds() + 15;
		while (Audio->PendingRadioClip.IsValid() && FPlatformTime::Seconds() < Deadline)
		{ Audio->UpdateRadioPlayback(); FPlatformProcess::Sleep(.005f); }
		TestFalse(TEXT("Optional decode completes"), Audio->PendingRadioClip.IsValid());
	};
	Radio->ScanMusicCollections(Temp);
	TestEqual(TEXT("Absent optional directory preserves baseline"), Radio->Stations.Num(), BaseCount);
	IFileManager::Get().MakeDirectory(*(Temp / TEXT("Empty Game")), true);
	for (const TCHAR* Game : { TEXT("Test Game A"), TEXT("Test Game B") })
	{
		const FString Folder = Temp / Game;
		IFileManager::Get().MakeDirectory(*Folder, true);
		const FString Song = FPaths::ProjectContentDir() / TEXT("Radio/InvokeTheRevoked/01 Expectations Increasing.ogg");
		TestEqual(TEXT("Fixture copies real compressed recording"), IFileManager::Get().Copy(*(Folder / TEXT("001 Opening.ogg")), *Song), COPY_OK);
		TestEqual(TEXT("Second recording copied"), IFileManager::Get().Copy(*(Folder / TEXT("002 Ending.ogg")), *Song), COPY_OK);
		FFileHelper::SaveStringToFile(TEXT("Ignored format and duplicate"), *(Folder / TEXT("001 Opening.wav")));
		FFileHelper::SaveStringToFile(TEXT("Not playable"), *(Folder / TEXT("003 Notes.txt")));
	}
	Radio->ScanMusicCollections(Temp);
	TestEqual(TEXT("Only nonempty games become stations"), Radio->Stations.Num(), BaseCount + 2);
	Radio->ScanMusicCollections(Temp);
	TestEqual(TEXT("Repeated discovery does not duplicate collections"), Radio->Stations.Num(), BaseCount + 2);
	const int32 First = BaseCount, Second = BaseCount + 1;
	if (!Radio->Stations.IsValidIndex(Second)) return false;
	TestEqual(TEXT("Games sorted without moving original stations"), Radio->Stations[First].CallSign, FString(TEXT("Test Game A")));
	TestEqual(TEXT("No ads, unsupported files or duplicate encodings"), Radio->GetStationPlaylist(First).Num(), 2);
	TestTrue(TEXT("Ogg preferred to legacy WAV"), Radio->Stations[First].Music[0].EndsWith(TEXT(".ogg")));
	Radio->SetPlayerInHelicopter(true);
	Radio->SetPowered(true);
	Radio->SetStationIndex(First);
	Radio->Tick(0); FinishDecode();
	TestEqual(TEXT("Tuning starts collection at its first title"), Radio->GetCurrentTitle(), FString(TEXT("001 Opening")));
	TestTrue(TEXT("Full recording decoded"), Radio->GetPlaybackDuration() > 80.f);
	Radio->TogglePlaybackPaused();
	TestTrue(TEXT("Paused collection can seek"), Radio->SeekPlayback(12.f)); FinishDecode();
	TestTrue(TEXT("Paused seek retains state"), Radio->IsPlaybackPaused() && FMath::IsNearlyEqual(Radio->GetPlaybackPosition(), 12.f, .02f));
	Radio->SetVolume(0.f);
	TestTrue(TEXT("Exact second song is selectable"), Radio->PlayStationTrack(First, 1)); FinishDecode();
	TestEqual(TEXT("Selection preserves mute"), Audio->GetRadioVolumeMultiplier(), 0.f);
	Audio->RadioEndTime = 0;
	Radio->Tick(0); FinishDecode();
	TestEqual(TEXT("Natural completion wraps in displayed order"), Radio->GetCurrentTitle(), FString(TEXT("001 Opening")));
	Radio->SetStationIndex(Second); Radio->Tick(0);
	Radio->SetPowered(false); FinishDecode();
	TestFalse(TEXT("Power off cancels pending collection playback"), Audio->IsRadioPlaying());
	Radio->SetPowered(true); Radio->Tick(0); FinishDecode();
	TestEqual(TEXT("Second collection continues its own queue after power cycle"), Radio->GetCurrentTitle(), FString(TEXT("002 Ending")));
	TestEqual(TEXT("Collections never consume KINV progress"), Radio->SequentialTrack, KinvTrack);
	Radio->SetStationIndex(0);
	TestTrue(TEXT("Original station still includes commercials"), Radio->GetStationPlaylist(0).Num() > Radio->Stations[0].Music.Num());

	FString PackRoot;
	if (FParse::Value(FCommandLine::Get(), TEXT("OptionalMusicRoot="), PackRoot))
	{
		TArray<FString> Manifests;
		IFileManager::Get().FindFilesRecursive(Manifests, *PackRoot, TEXT("collection.json"), true, false);
		TestTrue(TEXT("Release verification finds collection manifests"), !Manifests.IsEmpty());
		int32 Verified = 0;
		FString Mp3Fixture;
		for (const FString& Manifest : Manifests)
		{
			FString Json; TSharedPtr<FJsonObject> Object;
			if (!FFileHelper::LoadFileToString(Json, *Manifest) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object))
			{ AddError(TEXT("Invalid release manifest: ") + Manifest); continue; }
			for (const auto& Value : Object->GetArrayField(TEXT("tracks")))
			{
				const auto Track = Value->AsObject();
				const FString Path = FPaths::GetPath(Manifest) / Track->GetStringField(TEXT("file"));
				if (Mp3Fixture.IsEmpty() && Path.EndsWith(TEXT(".mp3")) && Track->GetNumberField(TEXT("seconds")) > 30 &&
					Track->GetNumberField(TEXT("seconds")) < 300) Mp3Fixture = Path;
				FSimCopterPcmClip Clip;
				if (!TestTrue(*Path, Audio->DecodeAudio(Path, Clip))) continue;
				TestEqual(TEXT("Channels preserved"), Clip.Channels, Track->GetIntegerField(TEXT("channels")));
				TestEqual(TEXT("Sample rate preserved"), Clip.SampleRate, Track->GetIntegerField(TEXT("sample_rate")));
				TestTrue(*(TEXT("Complete decoded duration matches independent FFmpeg: ") + Path),
					FMath::Abs(Clip.Pcm16.Num() / (2 * Clip.Channels) - Track->GetIntegerField(TEXT("frames"))) <= 2048);
				++Verified;
			}
		}
		AddInfo(FString::Printf(TEXT("Verified %d complete optional recordings across %d games"), Verified, Manifests.Num()));
		if (TestFalse(TEXT("Release contains an MP3 integration fixture"), Mp3Fixture.IsEmpty()))
		{
			Audio->StopRadio();
			TestTrue(TEXT("MP3 starts asynchronously"), Audio->PlayRadioFile(Mp3Fixture, 0.f)); FinishDecode();
			TestTrue(TEXT("MP3 retains full playback duration"), Audio->GetRadioDurationSeconds() > 30.f);
			Audio->SetRadioPaused(true);
			TestTrue(TEXT("MP3 supports paused seeking"), Audio->SeekRadio(12.f)); FinishDecode();
			TestTrue(TEXT("MP3 seek retains pause and position"), Audio->IsRadioPaused() &&
				FMath::IsNearlyEqual(Audio->GetRadioPositionSeconds(), 12.f, .02f));
			TestEqual(TEXT("MP3 seeking preserves mute"), Audio->GetRadioVolumeMultiplier(), 0.f);
			Audio->PlayRadioFile(Mp3Fixture); Audio->StopRadio(); FinishDecode();
			TestFalse(TEXT("Cancelled MP3 does not restart"), Audio->IsRadioPlaying());
			FSimCopterPcmClip Clip;
			TAtomic<bool> Cancelled(true);
			TestFalse(TEXT("Cancelled decode produces no PCM"), SimCopterCompressedMusic::DecodeMp3(Mp3Fixture, Clip, &Cancelled));
			TestTrue(TEXT("Cancelled decode leaves empty clip"), Clip.Pcm16.IsEmpty());
			const FString BadMp3 = Temp / TEXT("Invalid.mp3");
			FFileHelper::SaveStringToFile(TEXT("This is not an MP3 recording"), *BadMp3);
			TestFalse(TEXT("Invalid MP3 is rejected"), SimCopterCompressedMusic::DecodeMp3(BadMp3, Clip));
			TArray<uint8> Bytes;
			FFileHelper::LoadFileToArray(Bytes, *Mp3Fixture);
			Bytes.SetNum(Bytes.Num() / 2);
			FFileHelper::SaveArrayToFile(Bytes, *BadMp3);
			TestFalse(TEXT("Half of a declared MP3 is rejected"), SimCopterCompressedMusic::DecodeMp3(BadMp3, Clip));
		}
	}
	return true;
}
#endif

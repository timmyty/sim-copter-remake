#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/SimCopterCompressedMusic.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterOggDecodeTest,
	"SimCopter.Sound.CompressedKinv.Decode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterOggDecodeTest::RunTest(const FString&)
{
	FString Json;
	TSharedPtr<FJsonObject> Manifest;
	if (!TestTrue(TEXT("KINV manifest exists"), FFileHelper::LoadFileToString(Json,
		*(FPaths::ProjectConfigDir() / TEXT("KinvMusic.json"))))) return false;
	if (!TestTrue(TEXT("KINV manifest parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Manifest))) return false;
	const auto& Tracks = Manifest->GetArrayField(TEXT("tracks"));
	TestEqual(TEXT("All songs retained"), Tracks.Num(), 21);
	const FString Root = FPaths::ProjectContentDir() / TEXT("Radio/InvokeTheRevoked");
	int64 TotalBytes = 0;
	for (const auto& Value : Tracks)
	{
		const auto Track = Value->AsObject();
		const FString Path = Root / Track->GetStringField(TEXT("file"));
		TotalBytes += IFileManager::Get().FileSize(*Path);
		FSimCopterPcmClip Clip;
		const double Start = FPlatformTime::Seconds();
		if (!TestTrue(*Path, SimCopterCompressedMusic::DecodeOgg(Path, Clip))) continue;
		AddInfo(FString::Printf(TEXT("%s decoded in %.3fs"), *FPaths::GetCleanFilename(Path), FPlatformTime::Seconds() - Start));
		TestEqual(TEXT("Stereo retained"), Clip.Channels, 2);
		TestEqual(TEXT("Sample rate retained"), Clip.SampleRate, 44100);
		TestTrue(TEXT("Complete song retained within codec padding"),
			FMath::Abs(Clip.Pcm16.Num() / 4 - Track->GetIntegerField(TEXT("frames"))) <= 1024);
		TestTrue(TEXT("Decoded track is audible"), Clip.Pcm16.ContainsByPredicate([](uint8 Byte) { return Byte != 0; }));
	}
	TestTrue(TEXT("All 21 songs fit below 100 MiB"), TotalBytes > 0 && TotalBytes < 100 * 1024 * 1024);
	if (Tracks.IsEmpty()) return false;
	const FString First = Root / Tracks[0]->AsObject()->GetStringField(TEXT("file"));
	const FString Temp = FPaths::ProjectSavedDir() / TEXT("Automation/TruncatedKinv.ogg");
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*Temp); };
	TArray<uint8> Encoded;
	FFileHelper::LoadFileToArray(Encoded, *First);
	Encoded.SetNum(Encoded.Num() - 128); FFileHelper::SaveArrayToFile(Encoded, *Temp);
	FSimCopterPcmClip Clip;
	TestFalse(TEXT("Truncated song rejected"), SimCopterCompressedMusic::DecodeOgg(Temp, Clip));
	TestFalse(TEXT("Failure leaves no partial samples"), Clip.IsValid());
	Encoded.Init(0, 64); FFileHelper::SaveArrayToFile(Encoded, *Temp);
	TestFalse(TEXT("Invalid file rejected"), SimCopterCompressedMusic::DecodeOgg(Temp, Clip));
	TAtomic<bool> Cancelled(true);
	TestFalse(TEXT("Cancelled worker produces no clip"), SimCopterCompressedMusic::DecodeOgg(First, Clip, &Cancelled));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterOggDiscoveryTest,
	"SimCopter.Radio.CompressedKinvDiscovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterOggDiscoveryTest::RunTest(const FString&)
{
	const FString Temp = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString();
	IFileManager::Get().MakeDirectory(*Temp, true);
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Temp, false, true); };
	for (const TCHAR* Name : { TEXT("01 Song.wav"), TEXT("01 Song.ogg"), TEXT("02 Legacy.wav") })
		FFileHelper::SaveStringToFile(TEXT("Discovery fixture"), *(Temp / Name));
	const auto Tracks = SimCopterCompressedMusic::FindTracks(Temp);
	if (!TestEqual(TEXT("Upgrade does not duplicate songs"), Tracks.Num(), 2)) return false;
	TestTrue(TEXT("Compressed song preferred"), Tracks[0].EndsWith(TEXT("01 Song.ogg")));
	TestTrue(TEXT("WAV fallback retained"), Tracks[1].EndsWith(TEXT("02 Legacy.wav")));
	return true;
}
#endif

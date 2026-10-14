#include "Audio/SimCopterCompressedMusic.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Interfaces/IAudioFormat.h"
#include "Decoders/VorbisAudioInfo.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SimCopterCompressedMusic
{
	bool DecodeOgg(const FString& Path, FSimCopterPcmClip& OutClip, const TAtomic<bool>* Cancelled)
	{
		OutClip = FSimCopterPcmClip();
		TArray<uint8> Encoded;
		if ((Cancelled && Cancelled->Load()) || !FFileHelper::LoadFileToArray(Encoded, *Path)) return false;
		// Require complete Ogg pages and an end-of-stream marker. The engine decoder otherwise
		// accepts a truncated file's last complete page as a shorter, apparently valid song.
		int32 Offset = 0;
		bool EndOfStream = false;
		while (Offset < Encoded.Num())
		{
			if (Cancelled && Cancelled->Load()) return false;
			if (Encoded.Num() - Offset < 27 || EndOfStream) return false;
			const uint8* Page = Encoded.GetData() + Offset;
			if (FMemory::Memcmp(Page, "OggS", 4) != 0 || Page[4] != 0) return false;
			const int32 HeaderSize = 27 + Page[26];
			if (Encoded.Num() - Offset < HeaderSize) return false;
			int32 PageSize = HeaderSize;
			for (int32 I = 27; I < HeaderSize; ++I) PageSize += Page[I];
			if (Encoded.Num() - Offset < PageSize) return false;
			EndOfStream = (Page[5] & 4) != 0;
			Offset += PageSize;
		}
		if (!EndOfStream) return false;
		LoadVorbisLibraries();
		FVorbisAudioInfo Decoder;
		FSoundQualityInfo Quality{};
		if (!Decoder.ReadCompressedInfo(Encoded.GetData(), Encoded.Num(), &Quality) ||
			Quality.NumChannels < 1 || Quality.NumChannels > 2 || Quality.SampleRate < 8000 ||
			Quality.SampleRate > 96000 || Quality.SampleDataSize == 0 ||
			Quality.SampleDataSize > 256 * 1024 * 1024 ||
			Quality.SampleDataSize % (2 * Quality.NumChannels) != 0) return false;
		FSimCopterPcmClip Clip;
		Clip.Pcm16.SetNumUninitialized(Quality.SampleDataSize);
		for (int32 Byte = 0; Byte < Clip.Pcm16.Num();)
		{
			if (Cancelled && Cancelled->Load()) return false;
			const int32 Count = FMath::Min(64 * 1024, Clip.Pcm16.Num() - Byte);
			// Reaching EOF before the declared PCM count means incomplete audio.
			if (Decoder.ReadCompressedData(Clip.Pcm16.GetData() + Byte, false, Count)) return false;
			Byte += Count;
		}
		if (Cancelled && Cancelled->Load()) return false;
		Clip.Channels = Quality.NumChannels;
		Clip.SampleRate = Quality.SampleRate;
		Clip.Duration = static_cast<float>(Clip.Pcm16.Num() / (2 * Clip.Channels)) / Clip.SampleRate;
		OutClip = MoveTemp(Clip);
		return true;
	}

	TArray<FString> FindTracks(const FString& Directory)
	{
		TMap<FString, FString> ByName;
		// Prefer the compact release file when an upgrade is extracted over legacy WAVs.
		for (const TCHAR* Extension : { TEXT("*.wav"), TEXT("*.ogg") })
		{
			TArray<FString> Files;
			IFileManager::Get().FindFiles(Files, *(Directory / Extension), true, false);
			for (const FString& File : Files) ByName.Add(FPaths::GetBaseFilename(File).ToLower(), Directory / File);
		}
		TArray<FString> Tracks;
		ByName.GenerateValueArray(Tracks);
		Tracks.Sort();
		return Tracks;
	}
}

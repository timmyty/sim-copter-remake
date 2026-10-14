#include "Audio/SimCopterCompressedMusic.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Interfaces/IAudioFormat.h"
#include "Decoders/VorbisAudioInfo.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

THIRD_PARTY_INCLUDES_START
#define DR_MP3_NO_STDIO
#define DRMP3_ASSERT(expression) check(expression)
#define DR_MP3_IMPLEMENTATION
#include "ThirdParty/dr_mp3.h"
THIRD_PARTY_INCLUDES_END

namespace SimCopterCompressedMusic
{
	bool DecodeMp3(const FString& Path, FSimCopterPcmClip& OutClip, const TAtomic<bool>* Cancelled)
	{
		OutClip = FSimCopterPcmClip();
		TArray<uint8> Encoded;
		if ((Cancelled && Cancelled->Load()) || IFileManager::Get().FileSize(*Path) > 128 * 1024 * 1024 ||
			!FFileHelper::LoadFileToArray(Encoded, *Path)) return false;
		drmp3 Decoder{};
		if (!drmp3_init_memory(&Decoder, Encoded.GetData(), Encoded.Num(), nullptr)) return false;
		ON_SCOPE_EXIT { drmp3_uninit(&Decoder); };
		if (Decoder.channels < 1 || Decoder.channels > 2 || Decoder.sampleRate < 8000 || Decoder.sampleRate > 96000) return false;
		const drmp3_uint64 Frames = drmp3_get_pcm_frame_count(&Decoder);
		// The preservation collections include uninterrupted 38-minute theme-park suites.
		// Bound allocations while retaining their original rate, stereo channels and full length.
		if (Frames == 0 || Frames > 512ull * 1024 * 1024 / (2 * Decoder.channels) ||
			(Cancelled && Cancelled->Load())) return false;
		FSimCopterPcmClip Clip;
		Clip.Pcm16.SetNumUninitialized(static_cast<int32>(Frames * 2 * Decoder.channels));
		drmp3_uint64 Offset = 0;
		while (Offset < Frames)
		{
			if (Cancelled && Cancelled->Load()) return false;
			const drmp3_uint64 Count = FMath::Min<drmp3_uint64>(16384, Frames - Offset);
			const drmp3_uint64 Read = drmp3_read_pcm_frames_s16(&Decoder, Count,
				reinterpret_cast<int16*>(Clip.Pcm16.GetData()) + Offset * Decoder.channels);
			Offset += Read;
			if (Read != Count)
			{
				// Some older LAME tags overstate the final frame's encoder padding. Accept at
				// most one MP3 frame of padding discrepancy, never a truncated song or zero fill.
				if (Offset == 0 || Frames - Offset > 1152) return false;
				Clip.Pcm16.SetNum(static_cast<int32>(Offset * 2 * Decoder.channels), EAllowShrinking::No);
				break;
			}
		}
		if (Cancelled && Cancelled->Load()) return false;
		Clip.Channels = Decoder.channels;
		Clip.SampleRate = Decoder.sampleRate;
		Clip.Duration = static_cast<float>(Offset) / Clip.SampleRate;
		OutClip = MoveTemp(Clip);
		return true;
	}

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
		for (const TCHAR* Extension : { TEXT("*.wav"), TEXT("*.mp3"), TEXT("*.ogg"), TEXT("*.flac") })
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

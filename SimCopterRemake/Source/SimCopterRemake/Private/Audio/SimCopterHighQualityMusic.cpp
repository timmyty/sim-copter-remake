#include "Audio/SimCopterHighQualityMusic.h"

#include "Audio/SimCopterAudioSubsystem.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
THIRD_PARTY_INCLUDES_START
#include "stream_decoder.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace SimCopterHighQualityMusic
{
	const TArray<FReplacement>& GetReplacements()
	{
		static const TArray<FReplacement> Replacements = []
		{
			TArray<FReplacement> Result;
			FString Text;
			TSharedPtr<FJsonObject> Document;
			if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectConfigDir() / TEXT("HighQualityMusic.json"))) ||
				!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Document) || !Document.IsValid()) return Result;
			const TArray<TSharedPtr<FJsonValue>>* Tracks = nullptr;
			if (!Document->TryGetArrayField(TEXT("tracks"), Tracks)) return Result;
			for (const auto& Track : *Tracks)
			{
				const auto Object = Track->AsObject();
				FString Flac;
				const TArray<TSharedPtr<FJsonValue>>* Originals = nullptr;
				if (!Object.IsValid() || !Object->TryGetStringField(TEXT("file"), Flac) ||
					!Flac.StartsWith(TEXT("music-hq/")) || Flac.Contains(TEXT("..")) || Flac.Contains(TEXT("\\")) ||
					!Object->TryGetArrayField(TEXT("originals"), Originals)) continue;
				for (const auto& Original : *Originals)
				{
					const auto Entry = Original->AsObject();
					FString Path, Md5;
					if (Entry.IsValid() && Entry->TryGetStringField(TEXT("path"), Path) &&
						Entry->TryGetStringField(TEXT("md5"), Md5) && Md5.Len() == 32 && FPaths::IsRelative(Path) &&
						!Path.Contains(TEXT("..")) && !Path.Contains(TEXT("\\"))) Result.Add({Path, Flac, Md5});
				}
			}
			return Result;
		}();
		return Replacements;
	}

	FString Resolve(const FString& SoundRoot, const FString& OriginalPath)
	{
#if PLATFORM_WINDOWS
		const FString Absolute = FPaths::ConvertRelativePathToFull(OriginalPath);
		for (const auto& Entry : GetReplacements())
		{
			if (!FPaths::IsSamePath(Absolute, FPaths::ConvertRelativePathToFull(SoundRoot / Entry.Original))) continue;
			const FString Flac = SoundRoot / Entry.Flac;
			if (!FPaths::FileExists(Flac)) return FString();
			// The manifest only authorizes the verified retail recording. Preserve custom WAVs
			// even if the user gives them the same filename as a soundtrack track.
			if (FPaths::FileExists(Absolute) && !LexToString(FMD5Hash::HashFile(*Absolute)).Equals(Entry.OriginalMd5, ESearchCase::IgnoreCase)) return FString();
			return FPaths::ConvertRelativePathToFull(Flac);
		}
#endif
		return FString();
	}

	void UpgradePlaylist(const FString& SoundRoot, const FString& MusicDirectory, TArray<FString>& Tracks)
	{
		for (const auto& Entry : GetReplacements())
		{
			const FString Original = SoundRoot / Entry.Original;
			if (!FPaths::IsSamePath(FPaths::GetPath(Original), MusicDirectory)) continue;
			const FString Flac = Resolve(SoundRoot, Original);
			if (Flac.IsEmpty()) continue;
			Tracks.RemoveAll([&](const FString& Path) { return FPaths::IsSamePath(Path, Original); });
			Tracks.AddUnique(Flac);
		}
	}

#if PLATFORM_WINDOWS
	namespace
	{
		struct FDecodeState
		{
			TArray<uint8> Encoded;
			FSimCopterPcmClip Clip;
			size_t Offset = 0;
			uint64 Frames = 0;
			uint64 Written = 0;
			bool bFailed = false;
			const TAtomic<bool>* Cancelled = nullptr;
		};

		FLAC__StreamDecoderReadStatus Read(const FLAC__StreamDecoder*, FLAC__byte Buffer[], size_t* Bytes, void* Context)
		{
			auto& State = *static_cast<FDecodeState*>(Context);
			if (*Bytes == 0 || (State.Cancelled && State.Cancelled->Load())) return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
			*Bytes = FMath::Min(*Bytes, static_cast<size_t>(State.Encoded.Num()) - State.Offset);
			if (*Bytes == 0) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
			FMemory::Memcpy(Buffer, State.Encoded.GetData() + State.Offset, *Bytes);
			State.Offset += *Bytes;
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}

		void Metadata(const FLAC__StreamDecoder*, const FLAC__StreamMetadata* Data, void* Context)
		{
			if (Data->type != FLAC__METADATA_TYPE_STREAMINFO) return;
			auto& State = *static_cast<FDecodeState*>(Context);
			const auto& Info = Data->data.stream_info;
			// Bound allocation before accepting untrusted lengths. These soundtrack masters are
			// 44.1 kHz/16-bit stereo; keep other normal mono/stereo 16-bit FLAC usable as well.
			if (Info.bits_per_sample != 16 || Info.channels < 1 || Info.channels > 2 ||
				Info.sample_rate == 0 || Info.sample_rate > 192000 || Info.total_samples == 0 ||
				Info.total_samples > 512ull * 1024 * 1024 / (2 * Info.channels))
			{
				State.bFailed = true;
				return;
			}
			State.Frames = Info.total_samples;
			State.Clip.SampleRate = Info.sample_rate;
			State.Clip.Channels = Info.channels;
			State.Clip.Pcm16.SetNumUninitialized(static_cast<int32>(State.Frames * 2 * Info.channels));
		}

		FLAC__StreamDecoderWriteStatus Write(const FLAC__StreamDecoder*, const FLAC__Frame* Frame, const FLAC__int32* const Channels[], void* Context)
		{
			auto& State = *static_cast<FDecodeState*>(Context);
			if (State.bFailed || (State.Cancelled && State.Cancelled->Load()) || State.Frames == 0 || Frame->header.bits_per_sample != 16 ||
				Frame->header.channels != static_cast<uint32>(State.Clip.Channels) ||
				Frame->header.sample_rate != static_cast<uint32>(State.Clip.SampleRate) ||
				State.Written + Frame->header.blocksize > State.Frames) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
			int16* Destination = reinterpret_cast<int16*>(State.Clip.Pcm16.GetData()) + State.Written * State.Clip.Channels;
			for (uint32 Index = 0; Index < Frame->header.blocksize; ++Index)
				for (int32 Channel = 0; Channel < State.Clip.Channels; ++Channel)
					*Destination++ = static_cast<int16>(Channels[Channel][Index]);
			State.Written += Frame->header.blocksize;
			return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
		}

		void Error(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus, void* Context)
		{
			static_cast<FDecodeState*>(Context)->bFailed = true;
		}
	}
#endif

	bool DecodeFlac(const FString& Path, FSimCopterPcmClip& OutClip, const TAtomic<bool>* Cancelled)
	{
		OutClip = FSimCopterPcmClip();
#if PLATFORM_WINDOWS
		FDecodeState State;
		State.Cancelled = Cancelled;
		if (Cancelled && Cancelled->Load()) return false;
		if (!FFileHelper::LoadFileToArray(State.Encoded, *Path) || State.Encoded.Num() < 42 ||
			FMemory::Memcmp(State.Encoded.GetData(), "fLaC", 4) != 0) return false;
		FLAC__StreamDecoder* Decoder = FLAC__stream_decoder_new();
		if (!Decoder) return false;
		FLAC__stream_decoder_set_md5_checking(Decoder, true);
		const bool bInitialized = FLAC__stream_decoder_init_stream(Decoder, Read, nullptr, nullptr, nullptr, nullptr,
			Write, Metadata, Error, &State) == FLAC__STREAM_DECODER_INIT_STATUS_OK;
		const bool bDecoded = bInitialized && FLAC__stream_decoder_process_until_end_of_stream(Decoder);
		const bool bChecksum = FLAC__stream_decoder_finish(Decoder) != 0;
		FLAC__stream_decoder_delete(Decoder);
		if (!bDecoded || !bChecksum || State.bFailed || State.Frames == 0 || State.Written != State.Frames) return false;
		State.Clip.Duration = static_cast<float>(State.Frames) / State.Clip.SampleRate;
		OutClip = MoveTemp(State.Clip);
		return true;
#else
		return false;
#endif
	}
}

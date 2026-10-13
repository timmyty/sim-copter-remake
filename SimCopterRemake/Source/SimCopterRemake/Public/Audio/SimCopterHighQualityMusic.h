#pragma once

#include "CoreMinimal.h"

struct FSimCopterPcmClip;

/** Verified soundtrack upgrades. Paths are relative to the selected original sound root. */
namespace SimCopterHighQualityMusic
{
	struct FReplacement
	{
		FString Original;
		FString Flac;
		FString OriginalMd5;
	};

	const TArray<FReplacement>& GetReplacements();
	/** Returns empty if unavailable, unsupported, or a user has changed the original recording. */
	FString Resolve(const FString& SoundRoot, const FString& OriginalPath);
	/** Restores playlist entries whose original files were omitted from the package. */
	void UpgradePlaylist(const FString& SoundRoot, const FString& MusicDirectory, TArray<FString>& Tracks);
	/** Lossless 16-bit mono/stereo decode; verifies the complete stream and its PCM checksum. */
	bool DecodeFlac(const FString& Path, FSimCopterPcmClip& OutClip, const TAtomic<bool>* Cancelled = nullptr);
}

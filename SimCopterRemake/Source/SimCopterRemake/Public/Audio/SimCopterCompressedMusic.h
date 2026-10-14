#pragma once

#include "CoreMinimal.h"

struct FSimCopterPcmClip;

/** Compact authored radio recordings; original-game soundtrack mapping stays independent. */
namespace SimCopterCompressedMusic
{
	bool DecodeOgg(const FString& Path, FSimCopterPcmClip& OutClip, const TAtomic<bool>* Cancelled = nullptr);
	TArray<FString> FindTracks(const FString& Directory);
}

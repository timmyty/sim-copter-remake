#pragma once

#include "CoreMinimal.h"

class UObject;
struct FKey;

namespace SimCopterCheats
{
enum class ECode : uint8
{
	Superpower, Shields, Fuel, CEO, Home, Helicopter, Complete, Map,
	Warp, Money, Radioactivity, Pam, Megaphone, SundayDrive, Directions, Gort, Movies,
	HSI, PlayVideo, StopVideo,
};

struct FCommand
{
	ECode Code;
	int32 Value = 0;
	FString FileName;
};

// Process-local switches in FUN_00435680 (DAT_0051ac58..78). The remake keeps them
// with the career so travel preserves them and a new game/load starts clean.
struct FState
{
	bool bSuperpower = false;
	bool bShields = false;
	bool bFuel = false;
	bool bCEO = false;
	bool bMap = false;
	bool bMegaphone = false;
	bool bSundayDrive = false;
	bool bPortraits = false;
};

SIMCOPTERREMAKE_API FState* Get(const UObject* Context);
SIMCOPTERREMAKE_API TArray<FCommand> Parse(const FString& Text);
// FUN_00435680 accepts 1..49999; the random roll is in 0..49999.
SIMCOPTERREMAKE_API bool WinsMoneyGamble(int32 Amount, int32 Roll);
SIMCOPTERREMAKE_API int32 AircraftTypeForKey(const FKey& Key);
SIMCOPTERREMAKE_API bool CanNuclearBlastDemolish(uint8 XbldId, int32 Roll);
SIMCOPTERREMAKE_API bool ShouldSpawnPostBlastFire(int32 Roll);
}

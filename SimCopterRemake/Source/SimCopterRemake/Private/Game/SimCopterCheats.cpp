#include "Game/SimCopterCheats.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

namespace SimCopterCheats
{
FState* Get(const UObject* Context)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	auto* Career = Instance ? Instance->GetSubsystem<USimCopterCareerSubsystem>() : nullptr;
	return Career ? &Career->Cheats : nullptr;
}

// SCHOOK: CheatCommand 0x00435680. Case-sensitive substring searches are independent:
// one line can contain several codes. Numeric suffixes use the original bounded atoi.
TArray<FCommand> Parse(const FString& Text)
{
	// Remake movie commands consume the whole line: a filename containing an original
	// cheat phrase must never also trigger that cheat (especially Radioactivity).
	const FString Trimmed = Text.TrimStartAndEnd();
	if (Trimmed == TEXT("HSI")) return {{ECode::HSI}};
	if (Trimmed == TEXT("Stop video")) return {{ECode::StopVideo}};
	if (Trimmed.StartsWith(TEXT("Play video:"), ESearchCase::CaseSensitive))
		return {{ECode::PlayVideo, 0, Trimmed.Mid(11).TrimStartAndEnd()}};
	static const TCHAR* Phrases[] = {
		TEXT("superpowermultiply"), TEXT("Shields up"), TEXT("Gas does grow on trees"),
		TEXT("I'm the CEO of McDonnell Douglas"), TEXT("There's no place like home"),
		TEXT("I love my helicopter"), TEXT("Been there, done that"), TEXT("The map, please"),
		TEXT("Warp me to career: "), TEXT("Give me bucks or give me death: "),
		TEXT("Radioactivity"), TEXT("PAMCAREYGOLDMAN"),
		TEXT("A megaphone in the hand is worth two in the bush"),
		TEXT("Out on a Sunday drive"), TEXT("Stop and ask for directions"),
		TEXT("Gort"), TEXT("Lights, Camera, Action!")
	};
	TArray<FCommand> Commands;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Phrases); ++Index)
	{
		const int32 Position = Text.Find(Phrases[Index], ESearchCase::CaseSensitive);
		if (Position == INDEX_NONE) continue;
		FCommand Command{ static_cast<ECode>(Index) };
		if (Command.Code == ECode::Warp || Command.Code == ECode::Money)
		{
			const bool bWarp = Command.Code == ECode::Warp;
			Command.Value = FCString::Atoi(*Text.Mid(Position + FCString::Strlen(Phrases[Index]), bWarp ? 2 : 5));
			if (Command.Value < 1 || Command.Value > (bWarp ? 30 : 49999)) continue;
		}
		Commands.Add(Command);
	}
	return Commands;
}

bool WinsMoneyGamble(int32 Amount, int32 Roll)
{
	return Amount > 0 && Amount < 50000 && Roll >= 0 && Roll < 50000 &&
		Roll >= (Amount - 50000) / 2 + 50000;
}

int32 AircraftTypeForKey(const FKey& Key)
{
	static const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
		if (Keys[Index] == Key) return Index;
	return INDEX_NONE;
}

// SCHOOK: NuclearCityDestruction 0x004a6940. Preserve roads, airport, hospital,
// police and fire services; other structures have a one-in-four chance to survive.
bool CanNuclearBlastDemolish(uint8 Id, int32 Roll)
{
	return Id >= 5 && !(Id > 0x1c && Id < 0x6c) && Id != 0xde && Id != 0xf6 &&
		Id != 0xd1 && Id != 0xd2 && Id != 0xd3 && (Roll & 3) != 0;
}

bool ShouldSpawnPostBlastFire(int32 Roll)
{
	// SCHOOK: NuclearCityDestruction 0x004a6940 -> TilePuff 0x004af220, class 1.
	return (Roll & 0x1f) == 0;
}
}

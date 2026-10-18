// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/SimCopterSessionSubsystem.h"
#include "Game/SimCopterSettings.h"
#include "Game/SimCopterCursorIdle.h"

#include "Game/SimCopterInputDebug.h"
#include "Game/SimCopterMacWindowFix.h"

#include "Formats/SimCopterOriginalGamePaths.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void USimCopterSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CursorIdleGuard = SimCopterCursorIdle::Register(GetGameInstance());
	SimCopterInputDebug::RegisterIfRequested();
	SimCopterMacWindowFix::Register();

	int32 CommandLineCareerCity = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCopterCareerCity="), CommandLineCareerCity))
	{
		RequestCareerCity(CommandLineCareerCity);
	}

	// `-SimCopterBenchCmds="@8:SimBoardHelicopter;@12:csvprofile frames=900;@40:HighResShot 1"`: console
	// commands run through the player controller that many seconds after launch, so a benchmark can
	// fly to a fixed view before it starts measuring. -ExecCmds runs everything on the first frame,
	// before the city, the pawn or the airport placement exist.
	FString BenchCommands;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimCopterBenchCmds="), BenchCommands, /*bShouldStopOnSeparator=*/false))
	{
		TArray<FString> Entries;
		BenchCommands.ParseIntoArray(Entries, TEXT(";"));
		const double LaunchSeconds = FPlatformTime::Seconds();
		for (const FString& Entry : Entries)
		{
			FString Delay;
			FString Command;
			if (!Entry.TrimStart().Split(TEXT(":"), &Delay, &Command) || !Delay.StartsWith(TEXT("@")))
			{
				continue;
			}
			const double RunAtSeconds = LaunchSeconds + FCString::Atod(*Delay.RightChop(1));
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
				[this, RunAtSeconds, Command](float)
				{
					if (FPlatformTime::Seconds() < RunAtSeconds)
					{
						return true;
					}
					UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
					if (APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr)
					{
						Controller->ConsoleCommand(Command);
					}
					return false;
				}));
		}
	}
}

void USimCopterSessionSubsystem::Deinitialize()
{
	SimCopterMacWindowFix::Unregister();
	SimCopterCursorIdle::Unregister(CursorIdleGuard);
	Super::Deinitialize();
}

void USimCopterSessionSubsystem::RequestCareerCity(int32 InCareerCityIndex)
{
	Kind = ESimCopterSessionKind::Career;
	CareerCityIndex = FMath::Clamp(InCareerCityIndex, 0, 29);
	CityFilePath = ResolveCareerCityFilePath(CareerCityIndex);
}

void USimCopterSessionSubsystem::RequestUserCity(const FString& InCityFilePath)
{
	Kind = ESimCopterSessionKind::User;
	if (auto* Settings = USimCopterSettings::Get(this)) Settings->SetTimeOfDayMode(ESimCopterTimeOfDayMode::Dynamic);
	// Career City0 supplies only the shared base record; BeginSession replaces its settings fields
	// with FUN_004080c0's separate mode-1 defaults.
	CareerCityIndex = 0;
	CityFilePath = InCityFilePath;
}

void USimCopterSessionSubsystem::ClearPendingSession()
{
	Kind = ESimCopterSessionKind::None;
	CareerCityIndex = 0;
	CityFilePath.Reset();
	PendingMissionTypeMask = 0;
	bStartFirstMissionImmediately = false;
}

FString USimCopterSessionSubsystem::ResolveCitiesDir()
{
	return SimCopterOriginalGame::ResolveDirectory(TEXT("cities"));
}

FString USimCopterSessionSubsystem::ResolveCareerCityFilePath(int32 CareerCityIndex)
{
	const FString CitiesDir = ResolveCitiesDir();
	if (CitiesDir.IsEmpty())
	{
		return FString();
	}

	return FPaths::Combine(CitiesDir, TEXT("career"), FString::Printf(TEXT("city%d.sc2"), FMath::Clamp(CareerCityIndex, 0, 29)));
}

void USimCopterSessionSubsystem::GetUserCityFilePaths(TArray<FString>& OutPaths)
{
	OutPaths.Reset();

	const FString CitiesDir = ResolveCitiesDir();
	if (CitiesDir.IsEmpty())
	{
		return;
	}

	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *(CitiesDir / TEXT("*.sc2")), true, false);
	FileNames.Sort();

	for (const FString& FileName : FileNames)
	{
		OutPaths.Add(FPaths::Combine(CitiesDir, FileName));
	}
}

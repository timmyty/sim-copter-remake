#include "Game/SimCopterPlayerController.h"
#include "Game/SimCopterCheats.h"
#include "City/SimCopterDriveIn.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Game/SimCopterSessionSubsystem.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "City/SimCopterHangar.h"
#include "City/SimCity2000CityActor.h"
#include "UI/SimCopterHangarArt.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "UI/SSimCopterMapPanel.h"
#include "Widgets/SOverlay.h"

bool ASimCopterPlayerController::HandleCheatKey(const FKeyEvent& Event)
{
	// This preprocessor runs before gameplay: Ctrl+Alt+X must never also fire the tool on X.
	if (!IsLocalController() || !GetPawn() || !GEngine || !GEngine->GameViewport) return false;
	FString Focused;
	const auto Focus = GatherKeyboardFocusState(Focused);
	if (!Focus.bGameViewportHasFocus && !Focus.bFocusIsReclaimable) return false;
	if (Event.GetKey() == EKeys::X && Event.IsControlDown() && Event.IsAltDown())
	{
		if (!Event.IsRepeat()) OpenCheatEntry();
		return true;
	}
	if (IsPaused() || IsSettingsOpen() || GEngine->GameViewport->IgnoreInput()) return false;
	const auto* Cheats = SimCopterCheats::Get(this);
	if (Cheats && Cheats->bMegaphone && Cast<ASimCopterOnFootPawn>(GetPawn()))
	{
		const FKey Keys[] = { EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10 };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
		{
			if (Keys[Index] != Event.GetKey()) continue;
			if (!Event.IsRepeat())
				if (auto* Heli = SimCopterHelicopterParking::ResolveCurrentAircraft(this))
					Heli->SendOnFootMegaphoneMessage(GetPawn(), Index);
			return true;
		}
	}
	return false;
}

void ASimCopterPlayerController::OpenCheatEntry()
{
	if (IsSettingsOpen() || IsReplayPanelOpen() || IsPaused() ||
		ASimCopterHangar::IsAnyShellOpen(GetWorld()) || !GEngine || !GEngine->GameViewport) return;
	FlushPressedKeys();
	PushPause();
	EnterScreen(ESimCopterSettingsScreen::CheatEntry);
}

FString ASimCopterPlayerController::ExecuteCheatCodes(const FString& Text)
{
	using namespace SimCopterCheats;
	FState* State = Get(this);
	if (!State || !GetPawn()) return TEXT("Start a game before entering a cheat code.");
	const TArray<FCommand> Commands = Parse(Text);
	if (Commands.IsEmpty()) return TEXT("Code not recognized. Check spelling, case, punctuation and number range.");
	auto* Heli = SimCopterHelicopterParking::ResolveCurrentAircraft(this);
	auto* Pilot = Cast<ASimCopterOnFootPawn>(GetPawn());
	auto* Missions = ResolveMissionSystem();
	auto* Session = GetGameInstance()->GetSubsystem<USimCopterSessionSubsystem>();
	auto* Career = GetGameInstance()->GetSubsystem<USimCopterCareerSubsystem>();
	TArray<FString> Results;
	bool bApplied = false;
	bool bEnabled = true;
	const auto Toggle = [&](bool& Flag, const TCHAR* Name)
	{
		Flag = !Flag;
		bEnabled = Flag;
		bApplied = true;
		Results.Add(FString::Printf(TEXT("%s %s."), Name, Flag ? TEXT("enabled") : TEXT("disabled")));
	};
	for (const FCommand& Command : Commands)
	{
		switch (Command.Code)
		{
		case ECode::Superpower: Toggle(State->bSuperpower, TEXT("Superpower (hold Shift)")); break;
		case ECode::Shields: Toggle(State->bShields, TEXT("Shields")); break;
		case ECode::Fuel:
			Toggle(State->bFuel, TEXT("Unlimited fuel"));
			if (State->bFuel && Heli) Heli->RefillFuelForCheat();
			break;
		case ECode::CEO:
			Toggle(State->bCEO, TEXT("CEO aircraft delivery"));
			if (State->bCEO) Results.Add(TEXT("In the helicopter catalog, press 1–9 to take free delivery; 3 is the Apache."));
			break;
		case ECode::Map: Toggle(State->bMap, TEXT("On-foot map")); break;
		case ECode::Megaphone: Toggle(State->bMegaphone, TEXT("On-foot megaphone (F6–F10)")); break;
		case ECode::SundayDrive: Toggle(State->bSundayDrive, TEXT("Car camera")); break;
		case ECode::Home:
			// FUN_00435680 permits this only in the on-foot view. Leave the aircraft where it is.
			if (Pilot)
			{
				if (TActorIterator<ASimCopterHangar> Hangar(GetWorld()); Hangar)
				{
					FVector Position = Hangar->GetDoorWorldLocation() + Hangar->GetActorForwardVector() * 90;
					Position.Z += Pilot->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					Pilot->TeleportForCheat(Position);
					bApplied = true;
					Results.Add(TEXT("Returned to the hangar."));
				}
			}
			if (!Pilot) Results.Add(TEXT("Leave the helicopter first."));
			break;
		case ECode::Helicopter:
			if (Pilot && Heli)
			{
				CloseScreen();
				Heli->EnterHelicopter(this);
				Pilot = Cast<ASimCopterOnFootPawn>(GetPawn());
				bApplied = GetPawn() == Heli;
				Results.Add(bApplied ? TEXT("Returned to the helicopter.") : TEXT("The helicopter is unavailable."));
			}
			else Results.Add(TEXT("Leave the helicopter first."));
			break;
		case ECode::Complete:
			if (Session && Session->GetSessionKind() == ESimCopterSessionKind::Career && Missions)
			{
				// FUN_00407b30 -> FUN_00407ae0 sets the score to the level's goal.
				Missions->RestoreSavedSessionState(FMath::Max(1, Missions->GetSessionCareerCity().PointsNeeded),
					Missions->GetSessionCash(), Missions->GetSessionCareerCity(), Missions->GetSessionElapsedSeconds());
				Results.Add(TEXT("Level complete. Return to the hangar to advance."));
				bApplied = true;
			}
			else Results.Add(TEXT("This code requires a career game."));
			break;
		case ECode::Warp:
			if (Session && Session->GetSessionKind() == ESimCopterSessionKind::Career && Missions && Career)
			{
				FSimCopterCareerCityTransfer Transfer;
				Transfer.Cash = Missions->GetSessionCash();
				if (Heli)
				{
					Transfer.ActiveHelicopterTypeIndex = Heli->GetHelicopterTypeIndex();
					Transfer.CareerEquipmentMask = Heli->GetEquipmentState().CareerEquipmentMask;
					Transfer.CareerTearGasRounds = Heli->GetEquipmentState().CareerTearGasRounds;
				}
				Career->SetPendingCityTransfer(Transfer);
				Session->ClearCompletedCareerCity();
				Session->RequestCareerCity(Command.Value - 1);
				CloseScreen();
				UGameplayStatics::OpenLevel(this, FName(USimCopterSessionSubsystem::GetCityLevelName()));
				return FString::Printf(TEXT("Travelling to career level %d."), Command.Value);
			}
			Results.Add(TEXT("This code requires a career game (levels 1–30)."));
			break;
		case ECode::Money:
			if (!Missions) break;
			if (WinsMoneyGamble(Command.Value, FMath::RandRange(0, 49999)))
			{
				Missions->AddSessionCash(Command.Value);
				bApplied = true;
				Results.Add(FString::Printf(TEXT("You won %d Bucks."), Command.Value));
			}
			else
			{
				// The original gamble ends the current game on failure. Existing saves remain intact.
				CloseScreen();
				Session->ClearPendingSession();
				Session->ClearCompletedCareerCity();
				Career->ClearPendingCityTransfer();
				UGameplayStatics::OpenLevel(this, FName(USimCopterSessionSubsystem::GetMainMenuLevelName()));
				return TEXT("Give me death: the money gamble ended this game.");
			}
			break;
		case ECode::Directions:
			if (TActorIterator<ASimCopterTrafficSystemActor> Traffic(GetWorld()); Traffic)
			{
				FString Map = TEXT("SimCopter city building map — one hexadecimal XBLD id per tile\n");
				for (int32 Y = 0; Y < 128; ++Y)
				{
					for (int32 X = 0; X < 128; ++X) Map += FString::Printf(TEXT("%02X "), Traffic->GetXbldTileId(X, Y) & 255);
					Map += TEXT("\n");
				}
				const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("dump_bm.txt"));
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
				bApplied = FFileHelper::SaveStringToFile(Map, *Path);
				Results.Add(bApplied ? FString::Printf(TEXT("City map saved to %s"), *Path) : TEXT("Could not save the city map."));
			}
			break;
		case ECode::Pam:
			State->bPortraits = true;
			for (TActorIterator<ASimCity2000CityActor> City(GetWorld()); City; ++City) City->ShowCheatPortraits();
			bApplied = true;
			Results.Add(TEXT("Pam's portrait billboards revealed."));
			break;
		case ECode::Gort:
			// FUN_004455e0 uses the original martian.bmp ending panel.
			if (!ScreenWidget.IsValid()) PushPause();
			EnterScreen(ESimCopterSettingsScreen::CheatEnding);
			return TEXT("Gort");
		case ECode::Movies:
		case ECode::HSI:
		case ECode::PlayVideo:
		{
			// FUN_004477b0 / 0x00435680 movie toggle, extended to HD MP4 files.
			auto* DriveIn = ASimCopterDriveInPlayer::Get(GetWorld());
			const FString File = Command.Code == ECode::Movies ? TEXT("LightsCameraActionSimCopter.mp4") :
				Command.Code == ECode::HSI ? TEXT("HSI.mp4") : Command.FileName;
			Results.Add(DriveIn->PlayVideo(File, Command.Code != ECode::PlayVideo));
			break;
		}
		case ECode::StopVideo:
			ASimCopterDriveInPlayer::Get(GetWorld())->StopVideo();
			Results.Add(TEXT("Drive-in movie stopped."));
			break;
		case ECode::Radioactivity:
		{
			// FUN_004515d0: blast sound and white flash, then FUN_004a6940 destroys the city.
			int32 Count = 0;
			for (TActorIterator<ASimCity2000CityActor> City(GetWorld()); City; ++City)
			{
				TArray<FIntPoint> Cleared;
				Count += City->ApplyNuclearCheat(Cleared);
				for (TActorIterator<ASimCopterTrafficSystemActor> Traffic(GetWorld()); Traffic; ++Traffic)
					Traffic->ClearXbldTiles(Cleared);
			}
			if (auto* Audio = USimCopterAudioSubsystem::Get(this))
				Audio->PlayFile2D(TEXT("blast"), SimCopterSound::ESoundDir::Root);
			if (PlayerCameraManager) PlayerCameraManager->StartCameraFade(1, 0, 3.2f, FLinearColor::White, false, false);
			Results.Add(FString::Printf(TEXT("Nuclear blast: %d buildings destroyed."), Count));
			bApplied = true;
			break;
		}
		}
	}
	if (bApplied)
	{
		// Original success/off cues; use a dedicated slot so other speech is not interrupted.
		if (auto* Audio = USimCopterAudioSubsystem::Get(this))
		{
			Audio->PlayFile2D(bEnabled ? TEXT("MBoxCht") : TEXT("MBoxCht1"), SimCopterSound::ESoundDir::Root);
		}
	}
	UpdateCheatViews();
	return FString::Join(Results, TEXT("\n"));
}

void ASimCopterPlayerController::PlayerTick(float DeltaSeconds)
{
	Super::PlayerTick(DeltaSeconds);
	UpdateCheatViews();
}

void ASimCopterPlayerController::RemoveCheatViews()
{
	if (OnFootCheatMap && GEngine && GEngine->GameViewport)
		GEngine->GameViewport->RemoveViewportWidgetContent(OnFootCheatMap.ToSharedRef());
	OnFootCheatMap.Reset();
	if (CheatCarCamera.IsValid()) CheatCarCamera->Destroy();
	CheatCarCamera.Reset();
	CheatCar.Reset();
}

void ASimCopterPlayerController::UpdateCheatViews()
{
	const auto* State = SimCopterCheats::Get(this);
	if (!State || !IsLocalController() || !GetPawn()) return;
	auto* Heli = SimCopterHelicopterParking::ResolveCurrentAircraft(this);
	const bool bShowMap = State->bMap && Cast<ASimCopterOnFootPawn>(GetPawn()) && Heli;
	if (bShowMap && !OnFootCheatMap && GEngine && GEngine->GameViewport && Art)
	{
		OnFootCheatMap = SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
			[SNew(SSimCopterMapPanel).Pawn(Heli).Art(Art).Scale(1.5f)];
		GEngine->GameViewport->AddViewportWidgetContent(OnFootCheatMap.ToSharedRef(), 25);
	}
	else if (!bShowMap && OnFootCheatMap)
	{
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(OnFootCheatMap.ToSharedRef());
		OnFootCheatMap.Reset();
	}
	if (State->bSundayDrive && !IsReplayPanelOpen())
	{
		if (!CheatCar.IsValid() || CheatCar->IsActorBeingDestroyed())
			CheatCar.Reset();
		if (!CheatCar.IsValid())
		{
			for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It)
				if (It->GetAgentKind() == ESimCopterGroundAgentKind::Vehicle && !It->IsActorBeingDestroyed())
				{ CheatCar = *It; break; }
		}
		if (!CheatCar.IsValid() && CheatCarCamera.IsValid())
		{
			if (GetViewTarget() == CheatCarCamera.Get()) SetViewTarget(GetPawn());
			CheatCarCamera->Destroy();
			CheatCarCamera.Reset();
		}
		if (CheatCar.IsValid())
		{
			if (!CheatCarCamera.IsValid()) CheatCarCamera = GetWorld()->SpawnActor<ACameraActor>();
			if (auto* Camera = CheatCarCamera.Get())
			{
				Camera->SetActorLocationAndRotation(CheatCar->GetActorLocation() + FVector(0, 0, 45), CheatCar->GetActorRotation());
				SetViewTarget(Camera);
			}
		}
	}
	else if (CheatCarCamera.IsValid())
	{
		if (GetViewTarget() == CheatCarCamera.Get()) SetViewTarget(GetPawn());
		CheatCarCamera->Destroy();
		CheatCarCamera.Reset();
		CheatCar.Reset();
	}
}

#include "Game/SimCopterPlayerController.h"
#include "Game/SimCopterSettings.h"
#include "City/SimCopterDayNight.h"
#include "Replay/SimCopterReplayFreeCamera.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void ASimCopterPlayerController::SimPhotoMode()
{
	if (IsPhotoModeActive()) { ClosePhotoMode(); return; }
	if (!GetPawn() || IsSettingsOpen() || IsReplayPanelOpen() || IsPaused() || !GEngine || !GEngine->GameViewport ||
		GEngine->GameViewport->IgnoreInput()) return;
	auto* Replay = ResolveReplay();
	if (!Replay || Replay->GetState() != ESimCopterReplayState::Idle) return;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Camera = GetWorld()->SpawnActor<ASimCopterReplayFreeCamera>(Params);
	if (!Camera) return;
	FVector Location; FRotator Rotation;
	GetPlayerViewPoint(Location, Rotation);
	Camera->SnapTo(Location, Rotation);
	if (PlayerCameraManager) Camera->SetFieldOfView(PlayerCameraManager->GetFOVAngle());
	PhotoCamera = Camera;
	PhotoPreviousView = GetViewTarget();
	bPhotoPreviousCursor = bShowMouseCursor;
	bPhotoPreviousHudHidden = Replay->IsHudHidden();
	bPhotoPreviousPaused = IsPaused();
	bPhotoPreviousFullTick = bShouldPerformFullTickWhenPaused;
	bShouldPerformFullTickWhenPaused = true;
	if (auto* Settings = USimCopterSettings::Get(this))
	{
		PhotoPreviousTimeMode = static_cast<uint8>(Settings->GetTimeOfDayMode());
		PhotoPreviousStaticHour = Settings->GetStaticTimeOfDayHours();
	}
	PhotoPreviousLiveHour = -1;
	if (auto* Day = USimCopterDayNightSubsystem::Get(this))
	{
		float LiveHour;
		if (Day->TryGetLiveTimeOfDayHours(LiveHour)) PhotoPreviousLiveHour = LiveHour;
	}
	PushPause();
	Replay->SetHudHidden(true);
	RemoveCheatViews();
	SetViewTarget(Camera);
	UpdateFreeCameraInputSuppression();
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	PhotoStatus = TEXT("F12: Save screenshot | H: Hide controls | Ctrl+P / Esc: Exit");
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	Rows->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("PHOTO MODE")))
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))];
	Rows->AddSlot().AutoHeight().Padding(0, 8)[SNew(STextBlock)
		.Text(FText::FromString(TEXT("WASD: move | Space / Ctrl: up / down | Shift: fast\nHold RMB + mouse: look | Mouse wheel: lens zoom")))];
	Rows->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]
	{
		const auto* Day = USimCopterDayNightSubsystem::Get(this);
		return FText::Format(NSLOCTEXT("Photo", "Time", "Day / Night: {0}"), USimCopterSettings::FormatTimeOfDay(Day ? Day->GetTimeOfDayHours() : 12));
	})];
	Rows->AddSlot().AutoHeight().Padding(0, 6)
	[SNew(SSlider).IsFocusable(false).Value_Lambda([this]
	{
		const auto* Day = USimCopterDayNightSubsystem::Get(this);
		return Day ? Day->GetTimeOfDayHours() / 24.0f : 0.5f;
	}).OnValueChanged_Lambda([this](float Value)
	{
		if (auto* Settings = USimCopterSettings::Get(this))
		{
			Settings->SetTimeOfDayMode(ESimCopterTimeOfDayMode::Static);
			Settings->SetStaticTimeOfDayHours(FMath::Clamp(Value, 0.0f, 1.0f) * 24.0f);
			if (auto* Day = USimCopterDayNightSubsystem::Get(this)) Day->ApplyTimeOfDaySettings();
		}
	})];
	Rows->AddSlot().AutoHeight().Padding(0, 6)
	[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("Take screenshot")))
		.OnClicked_Lambda([this] { CapturePhoto(); return FReply::Handled(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0)[SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("Exit Photo Mode")))
		.OnClicked_Lambda([this] { ClosePhotoMode(); return FReply::Handled(); })]];
	Rows->AddSlot().AutoHeight().Padding(0, 6)[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(PhotoStatus); }).WrapTextAt(560)];
	PhotoPanel = SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24)
	[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
	.BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.045f, 0.9f)).Padding(18)[Rows]];
	GEngine->GameViewport->AddViewportWidgetContent(PhotoPanel.ToSharedRef(), 200);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void ASimCopterPlayerController::UpdatePhotoMode()
{
	if (!PhotoCamera.IsValid()) return;
	PhotoCamera->SetBoostActive(IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift));
	UpdateFreeCameraInputSuppression();
}

void ASimCopterPlayerController::CapturePhoto()
{
	if (!IsPhotoModeActive() || FScreenshotRequest::IsScreenshotRequested()) return;
	const FString Filename = FPaths::ScreenShotDir() / TEXT("SimCopter_Photo.png");
	// Exclude Slate, cursor and the photo controls even when the user clicks the button.
	FScreenshotRequest::RequestScreenshot(Filename, false, true);
	PhotoStatus = TEXT("Screenshot requested: ") + FPaths::ConvertRelativePathToFull(FScreenshotRequest::GetFilename());
}

void ASimCopterPlayerController::ClosePhotoMode()
{
	if (!PhotoCamera.IsValid()) return;
	if (GEngine && GEngine->GameViewport && PhotoPanel)
		GEngine->GameViewport->RemoveViewportWidgetContent(PhotoPanel.ToSharedRef());
	PhotoPanel.Reset();
	SetViewTarget(PhotoPreviousView.IsValid() ? PhotoPreviousView.Get() : GetPawn());
	PhotoPreviousView.Reset();
	PhotoCamera->Destroy(); PhotoCamera.Reset();
	bFreeCameraLookHeld = false;
	if (auto* Settings = USimCopterSettings::Get(this))
	{
		Settings->SetTimeOfDayMode(static_cast<ESimCopterTimeOfDayMode>(PhotoPreviousTimeMode));
		Settings->SetStaticTimeOfDayHours(PhotoPreviousStaticHour);
		if (auto* Day = USimCopterDayNightSubsystem::Get(this))
		{
			Day->ApplyTimeOfDaySettings();
			if (PhotoPreviousLiveHour >= 0) Day->RestoreSavedTimeOfDay(PhotoPreviousLiveHour);
		}
	}
	if (auto* Replay = ResolveReplay()) Replay->SetHudHidden(bPhotoPreviousHudHidden);
	UpdateFreeCameraInputSuppression();
	PopPause();
	if (bPhotoPreviousPaused) SetPause(true);
	bShouldPerformFullTickWhenPaused = bPhotoPreviousFullTick;
	RestoreGameInput();
	bShowMouseCursor = bPhotoPreviousCursor;
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

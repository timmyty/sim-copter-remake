#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerState.h"
#include "Game/SimCopterPlayerController.h"
#include "Game/SimCopterSettings.h"
#include "City/SimCopterDayNight.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Replay/SimCopterReplaySubsystem.h"
#include "Replay/SimCopterReplayFreeCamera.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Widgets/SOverlay.h"
#include "UnrealClient.h"
#include "UI/SSimCopterSettingsMenu.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPhotoModeTest, "SimCopter.CustomCities.PhotoModeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPhotoModeTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World = Instance->GetWorld(); World->SetGameMode(FURL()); World->InitializeActorsForPlay(FURL());
	auto* PreviousViewport = GEngine->GameViewport.Get();
	auto* Viewport = NewObject<UGameViewportClient>(GEngine);
	auto Widget = SNew(SViewport);
	auto SceneViewport = Viewport->CreateViewport(Widget);
	Viewport->SetViewport(&SceneViewport.Get());
	Viewport->SetViewportOverlayWidget(nullptr, SNew(SOverlay));
	GEngine->GetWorldContextFromWorldChecked(World).GameViewport = Viewport;
	GEngine->GameViewport = Viewport;
	ON_SCOPE_EXIT
	{
		FScreenshotRequest::Reset();
		Instance->Shutdown();
		GEngine->GameViewport = PreviousViewport;
		Viewport->SetViewport(nullptr);
		GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	};
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->ViewportClient = Viewport; PC->Player = LocalPlayer;
	if (!PC->PlayerState) PC->PlayerState = World->SpawnActor<APlayerState>();
	PC->PlayerInput = NewObject<UPlayerInput>(PC);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	PC->Possess(Heli); PC->SetViewTarget(Heli);
	auto* Settings = USimCopterSettings::Get(PC);
	auto* Replay = PC->ResolveReplay();
	if (!TestNotNull(TEXT("Photo settings available"), Settings) || !TestNotNull(TEXT("HUD state available"), Replay)) return false;
	Settings->SetTimeOfDayMode(ESimCopterTimeOfDayMode::Dynamic);
	Settings->SetStaticTimeOfDayHours(8.0f);
	Replay->SetHudHidden(false);
	PC->bShowMouseCursor = false;
	const FVector AircraftPosition = Heli->GetActorLocation();
	PC->SimPhotoMode();
	if (!TestTrue(TEXT("Photo Mode opens"), PC->IsPhotoModeActive())) return false;
	TestTrue(TEXT("Photo Mode pauses live missions"), PC->IsPaused());
	TestTrue(TEXT("Photo Mode hides the HUD"), Replay->IsHudHidden());
	TestTrue(TEXT("Photo Mode suppresses pawn input"), PC->PawnWithSuppressedInput.Get() == Heli);
	TestTrue(TEXT("Photo Mode uses a separate view without changing possession"), PC->GetViewTarget() == PC->PhotoCamera.Get() && PC->GetPawn() == Heli);
	TestTrue(TEXT("Photo controls tick through pause"), PC->bShouldPerformFullTickWhenPaused);
	Settings->SetTimeOfDayMode(ESimCopterTimeOfDayMode::Static);
	Settings->SetStaticTimeOfDayHours(23.0f);
	PC->CapturePhoto();
	TestTrue(TEXT("Capture queues an image"), FScreenshotRequest::IsScreenshotRequested());
	TestFalse(TEXT("Capture excludes all Slate controls"), FScreenshotRequest::ShouldShowUI());
	TestTrue(TEXT("Capture has a named PNG destination"), FScreenshotRequest::GetFilename().Contains(TEXT("SimCopter_Photo")) && FScreenshotRequest::GetFilename().EndsWith(TEXT(".png")));
	FScreenshotRequest::Reset();
	PC->SimSettings(); // The same Esc route used during play.
	TestFalse(TEXT("Escape closes Photo Mode"), PC->IsPhotoModeActive());
	TestFalse(TEXT("Escape resumes live missions"), PC->IsPaused());
	TestFalse(TEXT("Escape restores HUD visibility"), Replay->IsHudHidden());
	TestFalse(TEXT("Pawn input is restored"), PC->PawnWithSuppressedInput.IsValid());
	TestFalse(TEXT("Prior cursor visibility is restored"), PC->bShowMouseCursor);
	TestTrue(TEXT("Camera restores to aircraft"), PC->GetViewTarget() == Heli);
	TestEqual(TEXT("Photo Mode never moves the aircraft"), Heli->GetActorLocation(), AircraftPosition);
	TestEqual(TEXT("Photo lighting does not replace gameplay time mode"), Settings->GetTimeOfDayMode(), ESimCopterTimeOfDayMode::Dynamic);
	TestEqual(TEXT("Photo lighting restores the saved static hour"), Settings->GetStaticTimeOfDayHours(), 8.0f);
	PC->SimSettings();
	TestTrue(TEXT("Escape opens menu normally after photo exit"), PC->IsSettingsOpen());
	PC->HandleSettingsItem(ESimCopterSettingsItem::PhotoMode);
	TestTrue(TEXT("Esc menu hands off directly to Photo Mode"), PC->IsPhotoModeActive());
	TestFalse(TEXT("Esc menu is closed during Photo Mode"), PC->IsSettingsOpen());
	PC->SimPhotoMode();
	TestFalse(TEXT("Shortcut toggles Photo Mode off"), PC->IsPhotoModeActive());
	TestFalse(TEXT("Esc handoff leaves no extra pause reference"), PC->IsPaused());
	return true;
}
#endif

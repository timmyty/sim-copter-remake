#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Audio/SimCopterRadio.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "UI/SSimCopterRadioMenu.h"
#include "UI/SSimCopterDashboard.h"
#include "UI/SimCopterHangarArt.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerState.h"
#include "Components/InputComponent.h"
#include "Slate/WidgetRenderer.h"
#include "Slate/SceneViewport.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Types/PaintArgs.h"
#include "Widgets/SVirtualWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRadioMenuPlaybackTest, "SimCopter.Radio.MenuSequentialPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioMenuPlaybackTest::RunTest(const FString&)
{
	const FString SaveFile = FPaths::ProjectSavedDir() / TEXT("InvokeRadio.ini");
	FString OldSave;
	const bool HadSave = FFileHelper::LoadFileToString(OldSave, *SaveFile);
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		if (HadSave) FFileHelper::SaveStringToFile(OldSave, *SaveFile);
		else IFileManager::Get().Delete(*SaveFile);
	};
	auto* Radio = World->GetSubsystem<USimCopterRadioSubsystem>();
	auto* Audio = World->GetSubsystem<USimCopterAudioSubsystem>();
	Audio->bSoundsAvailable = true;
	TestTrue(TEXT("Invalid station has empty playlist"), Radio->GetStationPlaylist(-1).IsEmpty());
	const int32 Artist = Radio->Stations.IndexOfByPredicate([](const auto& S) { return S.bSequential; });
	if (!TestTrue(TEXT("KINV discovered"), Artist != INDEX_NONE) ||
		!TestTrue(TEXT("Original commercials discovered"), Radio->Commercials.Num() > 1)) return false;
	TestEqual(TEXT("KINV has only 21 songs, no shared ads"), Radio->GetStationPlaylist(Artist).Num(), 21);
	const auto Original = Radio->GetStationPlaylist(0);
	const int32 FirstAd = Original.IndexOfByPredicate([](const auto& T) { return T.Slot == ESimCopterRadioSlot::Commercial; });
	TestEqual(TEXT("Commercials follow songs"), FirstAd, Radio->Stations[0].Music.Num());
	TestFalse(TEXT("On-foot playback rejected"), Radio->PlayStationTrack(0, FirstAd));
	Radio->SetPlayerInHelicopter(true);
	Radio->SetCityIntroSuspended(true);
	TestFalse(TEXT("Intro cannot be interrupted"), Radio->PlayStationTrack(0, FirstAd));
	Radio->SetCityIntroSuspended(false);
	Radio->SetPowered(false);
	Radio->SetVolume(0.35f);
	Radio->SetSlotEnabled(ESimCopterRadioSlot::Commercial, false);
	TestTrue(TEXT("Explicit ad selection bypasses automatic-ad setting"), Radio->PlayStationTrack(0, FirstAd));
	TestTrue(TEXT("Selection powers on"), Radio->IsPowered());
	TestEqual(TEXT("Exact selected ad starts"), Radio->GetCurrentTitle(), Original[FirstAd].Title);
	const float Duration=Radio->GetPlaybackDuration();
	TestTrue(TEXT("Playback reports decoded duration"),Duration>0);
	Radio->TogglePlaybackPaused(); const float PausedPosition=Radio->GetPlaybackPosition();
	Radio->Tick(1);
	TestTrue(TEXT("Pause retains a live item without advancing its queue"),Radio->IsPlaybackPaused() && Audio->IsRadioPlaying() && Radio->ManualPlaylistIndex==FirstAd);
	TestEqual(TEXT("Paused playhead remains still"),Radio->GetPlaybackPosition(),PausedPosition);
	TestTrue(TEXT("Paused item can seek"),Radio->SeekPlayback(Duration*.5f));
	TestTrue(TEXT("Seek retains pause and jumps to requested time"),Radio->IsPlaybackPaused() && FMath::IsNearlyEqual(Radio->GetPlaybackPosition(),Duration*.5f,.02f));
	TestTrue(TEXT("Seek past end clamps to playable tail"),Radio->SeekPlayback(Duration+100));
	TestTrue(TEXT("End seek stays inside track"),Radio->GetPlaybackPosition()<Duration && Radio->GetPlaybackPosition()>Duration-.1f);
	TestTrue(TEXT("Rewind before start clamps to zero"),Radio->SeekPlayback(-100));
	TestTrue(TEXT("Rewind starts at zero"),FMath::IsNearlyZero(Radio->GetPlaybackPosition(),.01f));
	Radio->TogglePlaybackPaused(); TestFalse(TEXT("Play resumes paused audio"),Radio->IsPlaybackPaused());
	TestTrue(TEXT("Next button follows displayed ordering"),Radio->StepTrack(1));
	TestEqual(TEXT("Next selects next entry"),Radio->GetCurrentTitle(),Original[FirstAd+1].Title);
	TestTrue(TEXT("Previous returns to current test entry"),Radio->StepTrack(-1));
	TestEqual(TEXT("Previous selects preceding entry"),Radio->GetCurrentTitle(),Original[FirstAd].Title);
	TestEqual(TEXT("Existing volume retained"), Audio->GetRadioVolumeMultiplier(), 0.35f);
	TestFalse(TEXT("Automatic-ad preference retained"), Radio->IsSlotEnabled(ESimCopterRadioSlot::Commercial));
	Radio->Tick(0);
	TestEqual(TEXT("Playing selection is not immediately skipped"), Radio->ManualPlaylistIndex, FirstAd);
	Audio->StopRadio(); Radio->Tick(0);
	TestEqual(TEXT("Next displayed ad follows, no random selection"), Radio->GetCurrentTitle(), Original[FirstAd + 1].Title);
	TestFalse(TEXT("Out-of-range choice rejected"), Radio->PlayStationTrack(0, Original.Num()));
	TestEqual(TEXT("Bad choice does not retune"), Radio->GetCurrentTitle(), Original[FirstAd + 1].Title);
	TestTrue(TEXT("First item selectable for reverse wrap"),Radio->PlayStationTrack(0,0));
	Radio->StepTrack(-1); TestEqual(TEXT("Previous wraps to last displayed item"),Radio->GetCurrentTitle(),Original.Last().Title);
	Radio->SetVolume(0);
	TestTrue(TEXT("Last playlist item selectable"), Radio->PlayStationTrack(0, Original.Num() - 1));
	Audio->StopRadio(); Radio->Tick(0);
	TestEqual(TEXT("End wraps to first displayed song"), Radio->GetCurrentTitle(), Original[0].Title);
	TestEqual(TEXT("Wrap preserves mute"), Audio->GetRadioVolumeMultiplier(), 0.0f);
	Radio->SetPowered(false);
	TestFalse(TEXT("Power off cancels any pending FLAC"), Audio->IsRadioPlaying());
	TestTrue(TEXT("KINV exact song selectable"), Radio->PlayStationTrack(Artist, 4));
	TestEqual(TEXT("KINV starts at selected index"), Radio->SequentialTrack, 4);
	TestEqual(TEXT("KINV selection starts at beginning"), Radio->SequentialOffset, 0.0f);
	TestEqual(TEXT("KINV selected title"), Radio->GetCurrentTitle(), Radio->GetStationPlaylist(Artist)[4].Title);
	Audio->StopRadio(); Radio->Tick(0);
	TestEqual(TEXT("KINV advances sequentially"), Radio->SequentialTrack, 5);
	Radio->SequentialStartTime -= 10;
	TestTrue(TEXT("Reselecting same KINV song restarts it"), Radio->PlayStationTrack(Artist, 5));
	TestEqual(TEXT("Old saved offset discarded on explicit choice"), Radio->SequentialOffset, 0.0f);
	Radio->SetPlayerInHelicopter(false);
	TestFalse(TEXT("Leaving aircraft stops playback"), Audio->IsRadioPlaying());
	Radio->SetStationIndex(0);
	TestEqual(TEXT("Dial change clears manual queue"), Radio->ManualPlaylistIndex, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRadioMenuWidgetTest, "SimCopter.Radio.MenuWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioMenuWidgetTest::RunTest(const FString&)
{
	const FString SaveFile=FPaths::ProjectSavedDir()/TEXT("InvokeRadio.ini");
	FString OldSave; const bool HadSave=FFileHelper::LoadFileToString(OldSave,*SaveFile);
	ON_SCOPE_EXIT { if (HadSave) FFileHelper::SaveStringToFile(OldSave,*SaveFile); else IFileManager::Get().Delete(*SaveFile); };
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Radio = World->GetSubsystem<USimCopterRadioSubsystem>();
	int32 Closed = 0;
	auto Menu = SNew(SSimCopterRadioMenu).Radio(Radio).OnClosed(FSimpleDelegate::CreateLambda([&] { ++Closed; }));
	TestEqual(TEXT("All stations shown"), Menu->Stations.Num(), Radio->GetStationCount());
	TestTrue(TEXT("Current station is initially browsed"), Menu->SelectedStation == Radio->GetStationIndex());
	// Route input through a real Slate focus path, including the lists' native row navigation.
	// No OS window or physical controller is needed.
	{
		auto& Slate=FSlateApplication::Get();
		const auto PreviousFocus=Slate.GetUserFocusedWidget(0);
		auto Window=SNew(SVirtualWindow).Size(FVector2D(640,480));
		Window->SetContent(Menu); Slate.RegisterVirtualWindow(Window);
		ON_SCOPE_EXIT
		{
			if (PreviousFocus) Slate.SetUserFocus(0,PreviousFocus,EFocusCause::SetDirectly);
			else Slate.ClearUserFocus(0,EFocusCause::SetDirectly);
			Slate.UnregisterVirtualWindow(Window);
		};
		const auto Arrange=[&]
		{
			Window->SlatePrepass();
			FHittestGrid Grid; Grid.SetHittestArea(FVector2D::ZeroVector,FVector2D(640,480));
			FSlateWindowElementList Elements(Window);
			FPaintArgs Args(nullptr,Grid,FVector2D::ZeroVector,0,0);
			Window->Paint(Args,FGeometry::MakeRoot(FVector2D(640,480),FSlateLayoutTransform()),
				FSlateRect(0,0,640,480),Elements,0,FWidgetStyle(),true);
		};
		const auto Press=[&](FKey Key)
		{
			Slate.ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
			Slate.ProcessKeyUpEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
			Arrange();
		};
		Menu->StationList->SetSelection(Menu->Stations[0]); Arrange(); Arrange();
		Slate.SetUserFocus(0,Menu,EFocusCause::SetDirectly);
		Press(EKeys::Gamepad_FaceButton_Bottom);
		TestTrue(TEXT("Controller A enters station browsing"),Slate.GetUserFocusedWidget(0)==Menu->StationList);
		Press(EKeys::Gamepad_DPad_Down);
		TestEqual(TEXT("Controller down browses another station"),Menu->SelectedStation,1);
		Press(EKeys::Gamepad_FaceButton_Right);
		TestEqual(TEXT("B leaves station browsing without closing radio"),Closed,0);
		Press(EKeys::Gamepad_DPad_Right); Press(EKeys::Gamepad_FaceButton_Bottom);
		TestTrue(TEXT("Controller can move from stations into tracks"),Slate.GetUserFocusedWidget(0)==Menu->TrackList);
		Press(EKeys::Gamepad_DPad_Down);
		TestTrue(TEXT("Controller down browses the next track"),Menu->Tracks.Num()>1 && Menu->TrackList->GetSelectedItems().Contains(Menu->Tracks[1]));
		Radio->SetPlayerInHelicopter(true); World->GetSubsystem<USimCopterAudioSubsystem>()->bSoundsAvailable=true;
		Press(EKeys::Gamepad_FaceButton_Bottom);
		TestEqual(TEXT("A plays the controller-selected track"),Radio->GetCurrentTitle(),Menu->Tracks[1]->Title);
		Press(EKeys::Gamepad_FaceButton_Right);
		TestEqual(TEXT("B leaves track browsing without closing radio"),Closed,0);
		// Use a synchronous WAV for transport assertions; music may still be decoding FLAC.
		const auto Playlist=Radio->GetStationPlaylist(0);
		const int32 Ad=Playlist.IndexOfByPredicate([](const auto& Track){return Track.Slot==ESimCopterRadioSlot::Commercial;});
		TestTrue(TEXT("Transport fixture starts a WAV with a real duration"),Ad!=INDEX_NONE && Radio->PlayStationTrack(0,Ad) && Radio->GetPlaybackDuration()>0);
		Arrange();
		// The two transport sliders must participate in the same focus/edit loop as the lists.
		Press(EKeys::Gamepad_DPad_Up);
		for (int32 I=0;I<8 && Menu->GetControllerSelection()->GetTag()!=TEXT("RadioVolume");++I)
			Press(EKeys::Gamepad_DPad_Right);
		TestEqual(TEXT("Controller navigation reaches radio volume"),Menu->GetControllerSelection()->GetTag(),FName(TEXT("RadioVolume")));
		Radio->SetVolume(.35f); Press(EKeys::Gamepad_FaceButton_Bottom);
		Press(EKeys::Gamepad_DPad_Right);
		TestEqual(TEXT("A and right adjust radio volume"),Radio->GetVolume(),.4f);
		Press(EKeys::Gamepad_FaceButton_Right);
		Press(EKeys::Gamepad_DPad_Up);
		TestEqual(TEXT("Controller navigation reaches seek bar"),Menu->GetControllerSelection()->GetTag(),FName(TEXT("RadioSeekBar")));
		Radio->SeekPlayback(Radio->GetPlaybackDuration()*.25f);
		Press(EKeys::Gamepad_FaceButton_Bottom); Press(EKeys::Gamepad_DPad_Right);
		TestTrue(TEXT("A and right seek forward five percent"),FMath::IsNearlyEqual(Radio->GetPlaybackPosition(),Radio->GetPlaybackDuration()*.3f,.02f));
		Press(EKeys::Gamepad_FaceButton_Right);
		// Left stick moves between panels too, through Slate's analog routing.
		Menu->SetControllerSelection(Menu->StationList);
		Slate.ProcessAnalogInputEvent(FAnalogInputEvent(EKeys::Gamepad_LeftX,FModifierKeysState(),0,false,0,0,1.f));
		TestTrue(TEXT("Left stick moves from stations to tracks"),Menu->GetControllerSelection()==Menu->TrackList);
		Slate.ProcessAnalogInputEvent(FAnalogInputEvent(EKeys::Gamepad_LeftX,FModifierKeysState(),0,false,0,0,0.f));
		Press(EKeys::Gamepad_FaceButton_Right);
		TestEqual(TEXT("B closes radio after leaving the list"),Closed,1);
		Radio->SetPlayerInHelicopter(false); Radio->SetStationIndex(0); Closed=0;
	}
	const auto Capture = [&](const TCHAR* Name, FVector2D Size)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("SimRadioMenuPreview"))) return;
		FWidgetRenderer Renderer(false);
		Renderer.DrawWidget(Menu, Size);
		auto* Target = Renderer.DrawWidget(Menu, Size);
		if (!TestNotNull(TEXT("Menu rendered"), Target)) return;
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags);
		FString PreviewDir=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/remote-taser-autopilot");
		FParse::Value(FCommandLine::Get(),TEXT("SimRadioMenuPreviewDir="),PreviewDir);
		TestTrue(TEXT("Menu preview saved"), FImageUtils::SaveImageByExtension(
			*(PreviewDir / (FString(Name) + TEXT(".png"))),
			FImageView(Pixels.GetData(), Target->SizeX, Target->SizeY)));
	};
	Capture(TEXT("radio-menu"), FVector2D(1280, 960));
	const int32 Artist = Radio->GetStations().IndexOfByPredicate([](const auto& S) { return S.bSequential; });
	Menu->StationList->SetSelection(Menu->Stations[Artist]);
	TestEqual(TEXT("Station change replaces playlist"), Menu->Tracks.Num(), 21);
	Menu->TrackList->SetSelection(Menu->Tracks[17]);
	Menu->TrackList->RequestScrollIntoView(Menu->Tracks[17]);
	Capture(TEXT("radio-menu-kinv"), FVector2D(1280, 720));
	const int32 Before = Radio->GetStationIndex();
	Menu->PlaySelection();
	TestEqual(TEXT("Failed playback keeps menu open"), Closed, 0);
	TestFalse(TEXT("Failure explains why"), Menu->Status.IsEmpty());
	Menu->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
	TestEqual(TEXT("Escape closes once"), Closed, 1);
	TestEqual(TEXT("Browsing and cancelling do not retune"), Radio->GetStationIndex(), Before);
	Radio->SetPlayerInHelicopter(true);
	World->GetSubsystem<USimCopterAudioSubsystem>()->bSoundsAvailable=true;
	Menu->PlaySelection();
	TestEqual(TEXT("Successful playback keeps transport panel open"),Closed,1);
	TestEqual(TEXT("Selected track starts in open panel"),Radio->GetCurrentTitle(),Menu->Tracks[17]->Title);
	Menu->OnPreviewKeyDown(FGeometry(),FKeyEvent(EKeys::Gamepad_FaceButton_Top,FModifierKeysState(),0,false,0,0));
	TestTrue(TEXT("Controller Y pauses in radio"),Radio->IsPlaybackPaused());
	Menu->SeekToFraction(.5f); TestTrue(TEXT("Seek bar controls playback"),FMath::IsNearlyEqual(Radio->GetPlaybackPosition(),Radio->GetPlaybackDuration()*.5f,.02f));
	Capture(TEXT("radio-menu-paused"),FVector2D(1280,960));
	Menu->OnPreviewKeyDown(FGeometry(),FKeyEvent(EKeys::Gamepad_RightShoulder,FModifierKeysState(),0,false,0,0));
	TestEqual(TEXT("Controller RB advances track"),Radio->GetCurrentTitle(),Radio->GetStationPlaylist(Artist)[18].Title);
	TestEqual(TEXT("Next-track control keeps selected row synchronized"),Menu->TrackList->GetSelectedItems()[0]->Title,Radio->GetCurrentTitle());
	TestEqual(TEXT("Transport actions do not close radio"),Closed,1);
	Menu->Radio.Reset(); Menu->SelectStation(INDEX_NONE);
	TestTrue(TEXT("Unavailable radio produces empty list"), Menu->Tracks.IsEmpty());
	Menu->PlaySelection(); Menu->TuneStation();
	TestEqual(TEXT("Empty playback is safe and stays open"), Closed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRadioMenuLifecycleTest, "SimCopter.Radio.MenuLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRadioMenuLifecycleTest::RunTest(const FString&)
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
		Instance->Shutdown(); GEngine->GameViewport = PreviousViewport; Viewport->SetViewport(nullptr);
		GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	};
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine); LocalPlayer->ViewportClient = Viewport; PC->Player = LocalPlayer;
	if (!PC->PlayerState) PC->PlayerState = World->SpawnActor<APlayerState>();
	PC->PlayerInput = NewObject<UPlayerInput>(PC);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>(); PC->Possess(Heli); PC->SetViewTarget(Heli);
	PC->SetupInputComponent();
	bool Bound = false;
	for (const auto& Key : PC->InputComponent->KeyBindings)
		if (Key.Chord.Key == EKeys::R && Key.Chord.bCtrl)
		{
			Bound = true;
			TestTrue(TEXT("Ctrl+R consumes R so rope does not also toggle"), Key.bConsumeInput);
			TestTrue(TEXT("Radio shortcut can close while paused"), Key.bExecuteWhenPaused);
		}
	TestTrue(TEXT("Ctrl+R bound"), Bound);
	PC->SimRadioMenu();
	TestEqual(TEXT("Shortcut opens radio page"), PC->Screen, ESimCopterSettingsScreen::Radio);
	TestTrue(TEXT("Radio menu pauses flight"), PC->IsPaused());
	TestTrue(TEXT("UI owns input"), Viewport->IgnoreInput());
	PC->SimPhotoMode(); TestFalse(TEXT("Photo mode cannot overlap"), PC->IsPhotoModeActive());
	PC->SimRadioMenu();
	TestFalse(TEXT("Shortcut closes menu"), PC->IsSettingsOpen());
	TestFalse(TEXT("Closing resumes game"), PC->IsPaused());
	TestFalse(TEXT("Closing restores game input"), Viewport->IgnoreInput());
	auto* HeliInput=NewObject<UInputComponent>(Heli); Heli->SetupPlayerInputComponent(HeliInput);
	const auto ControllerKey=[&](FKey Key,EInputEvent Event=IE_Pressed)
	{ for (auto& Binding:HeliInput->KeyBindings) if (Binding.Chord.Key==Key && Binding.KeyEvent==Event) Binding.KeyDelegate.Execute(Key); };
	ControllerKey(EKeys::Gamepad_RightThumbstick); ControllerKey(EKeys::Gamepad_Special_Left);
	TestEqual(TEXT("R3 plus View opens Radio directly"),PC->Screen,ESimCopterSettingsScreen::Radio);
	TestTrue(TEXT("Controller radio shortcut pauses flight"),PC->IsPaused());
	PC->SimRadioMenu(); ControllerKey(EKeys::Gamepad_RightThumbstick,IE_Released);
	TestFalse(TEXT("Controller-opened menu restores flight input on close"),PC->IsPaused() || Viewport->IgnoreInput());
	// Exercise the actual cockpit tuner and housing's Slate double-click handlers.
	TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
	Art->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	auto Dashboard = SNew(SSimCopterDashboard).Pawn(Heli).Art(Art.Get());
	int32 OpenedByDoubleClick = 0;
	TFunction<void(TSharedRef<SWidget>)> Visit = [&](TSharedRef<SWidget> W)
	{
		const FPointerEvent Event(0, FVector2D::ZeroVector, FVector2D::ZeroVector,
			TSet<FKey>{ EKeys::LeftMouseButton }, EKeys::LeftMouseButton, 0, FModifierKeysState());
		W->OnMouseButtonDoubleClick(FGeometry(), Event);
		if (PC->Screen == ESimCopterSettingsScreen::Radio)
		{
			++OpenedByDoubleClick; PC->SimRadioMenu();
		}
		auto* Children = W->GetChildren();
		for (int32 I = 0; I < Children->Num(); ++I) Visit(Children->GetChildAt(I));
	};
	Visit(Dashboard);
	TestEqual(TEXT("Both radio housing and dial open menu"), OpenedByDoubleClick, 2);
	PC->SimSettings(); PC->SimRadioMenu();
	TestEqual(TEXT("Shortcut does not replace another menu"), PC->Screen, ESimCopterSettingsScreen::Menu);
	PC->SimSettings();
	PC->Possess(World->SpawnActor<ASimCopterOnFootPawn>()); PC->SimRadioMenu();
	TestFalse(TEXT("Cockpit radio unavailable on foot"), PC->IsSettingsOpen());
	return true;
}
#endif

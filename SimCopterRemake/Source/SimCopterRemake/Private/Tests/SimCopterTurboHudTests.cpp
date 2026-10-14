#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SSimCopterMapPanel.h"
#include "UI/SSimCopterControllerHelp.h"
#include "UI/SimCopterHangarArt.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Game/SimCopterPlayerController.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterTurboHudTest, "SimCopter.UI.TurboHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterTurboHudTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	PC->Possess(Heli);
	PC->NoteInputDevice(false);
	TestFalse(TEXT("Lamp starts off"), SSimCopterControllerHelp::ReadState(Heli).bTurboEnabled);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, true);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, false);
	TestTrue(TEXT("Shift reaches the live lamp"), SSimCopterControllerHelp::ReadState(Heli).bTurboEnabled);
	PC->NoteInputDevice(true);
	TestTrue(TEXT("Device change preserves Turbo state"), SSimCopterControllerHelp::ReadState(Heli).bTurboEnabled);
	TestFalse(TEXT("Device change updates the plate shortcut"), SSimCopterControllerHelp::ReadState(Heli).bKeyboard);
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("L3 turns off the same lamp"), SSimCopterControllerHelp::ReadState(Heli).bTurboEnabled);
	for (float Scale : {1.0f, 1.768292f, 2.5f})
	{
		auto Map = SNew(SSimCopterMapPanel).Pawn(Heli).Scale(Scale);
		Map->SlatePrepass(1);
		TestTrue(TEXT("Silver help extension adds height without stretching map"), Map->GetDesiredSize().Equals(FVector2D(185, 170) * Scale, 0.01));
		TestFalse(TEXT("Map never takes keyboard focus"), Map->SupportsKeyboardFocus());
		const FGeometry Geometry = FGeometry::MakeRoot(Map->GetDesiredSize(), FSlateLayoutTransform());
		const FVector2D Position(34 * Scale, (22 + 60) * Scale);
		const FPointerEvent Click(0, Position, Position, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState());
		TestTrue(TEXT("Zoom button still receives clicks below the new header"), Map->OnMouseButtonDown(Geometry, Click).IsEventHandled());
		TestTrue(TEXT("Zoom button release stays aligned"), Map->OnMouseButtonUp(Geometry, Click).IsEventHandled());
	}
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>();
	PC->Possess(Pilot);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, true); PC->HandleKeyboardTurboKey(EKeys::LeftShift, false);
	PC->ToggleControllerHelp();
	for (bool bGamepad : {false, true})
	{
		PC->NoteInputDevice(bGamepad);
		const auto State = SSimCopterControllerHelp::ReadState(Pilot);
		TestTrue(TEXT("On-foot help omits Turbo even while enabled"), !SimCopterControllerHelp::BuildPrompts(State).ContainsByPredicate(
			[](const SimCopterControllerHelp::FPrompt& Prompt) { return Prompt.Label.Contains(TEXT("Turbo")); }));
	}
	PC->ToggleControllerHelp();
	PC->Possess(Heli);
	if (!FParse::Param(FCommandLine::Get(), TEXT("SimTurboHudPreview"))) return true;
	TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
	Art->SetOriginalGameRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame")));
	for (float Scale : {1.5f, 2.5f})
	{
		auto Map = SNew(SSimCopterMapPanel).Pawn(Heli).Art(Art.Get()).Scale(Scale);
		const int32 W = FMath::CeilToInt(185 * Scale), H = FMath::CeilToInt(170 * Scale);
		TArray<FColor> KeyboardPixels;
		for (bool bOn : {false, true})
		{
			if (PC->IsControllerTurboEnabled() != bOn) { PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased(); }
			for (bool bGamepad : {false, true})
			{
				PC->NoteInputDevice(bGamepad);
				FWidgetRenderer Renderer(true);
				auto* Target = Renderer.DrawWidget(Map, FVector2D(W, H));
				if (!TestNotNull(TEXT("Turbo HUD renders offscreen"), Target)) return false;
				TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
				Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags);
				const FColor Lamp = Pixels[FMath::RoundToInt(39 * Scale) * W + FMath::RoundToInt(38.5f * Scale)];
				TestTrue(TEXT("Lamp is bright red only when Turbo is on"), bOn ? Lamp.R > 200 && Lamp.G < 100 : Lamp.R < 160);
				if (!bGamepad) KeyboardPixels = Pixels;
				else
				{
					int32 HardwareDifferences = 0, ShortcutDifferences = 0;
					for (int32 Y = 0; Y < H; ++Y) for (int32 X = 0; X < W; ++X)
						if (Pixels[Y * W + X] != KeyboardPixels[Y * W + X])
							(Y < 22 * Scale ? ShortcutDifferences : HardwareDifferences)++;
					TestEqual(TEXT("Input device leaves lamp, switch and map identical"), HardwareDifferences, 0);
					TestTrue(TEXT("Input device changes the header shortcut"), ShortcutDifferences > 40);
				}
				const FString Path = FPaths::ProjectDir() / TEXT("../Docs/scratchpad/turbo-hud") /
					FString::Printf(TEXT("map-%s-%s-%.1f.png"), bGamepad ? TEXT("controller") : TEXT("keyboard"), bOn ? TEXT("on") : TEXT("off"), Scale);
				TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(W, H, Pixels, Png);
				TestTrue(TEXT("Turbo HUD preview saved"), FFileHelper::SaveArrayToFile(Png, *Path));
			}
		}
	}
	return true;
}
#endif

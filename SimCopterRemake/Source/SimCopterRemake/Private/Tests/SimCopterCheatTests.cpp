#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Game/SimCopterCheats.h"
#include "City/SimCopterDriveIn.h"
#include "Ground/SimCopterParticleFX.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Formats/MaxisWindowsBitmapReader.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Game/SimCopterPlayerController.h"
#include "Components/AudioComponent.h"
#include "Engine/LocalPlayer.h"
#include "Flight/SimCopterFlightModel.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "City/SimCopterHangar.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "UI/SimCopterHangarShop.h"
#include "UI/SSimCopterCheatDialog.h"
#include "UI/SSimCopterGortSequence.h"
#include "UI/SimCopterGortSequence.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Misc/ScopeExit.h"
#include "Formats/SimCopterPrivAnimReader.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "City/SimCity2000CityActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/Paths.h"
#include "UI/SimCopterHangarArt.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatParserTest, "SimCopter.Cheats.OriginalParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatParserTest::RunTest(const FString&)
{
	using namespace SimCopterCheats;
	const auto Codes = Parse(TEXT("superpowermultiply Shields up Gas does grow on trees I'm the CEO of McDonnell Douglas There's no place like home I love my helicopter Been there, done that The map, please Warp me to career: 30 Give me bucks or give me death: 1000 Radioactivity PAMCAREYGOLDMAN A megaphone in the hand is worth two in the bush Out on a Sunday drive Stop and ask for directions Gort Lights, Camera, Action!"));
	TestEqual(TEXT("All original phrases recognized independently"), Codes.Num(), 17);
	TestTrue(TEXT("Case-sensitive, no invented aliases"), Parse(TEXT("shields up godmode money")).IsEmpty());
	TestEqual(TEXT("Substring match, once per code"), Parse(TEXT("xxShields upxx Shields up")).Num(), 1);
	for (const TCHAR* Invalid : {TEXT("Warp me to career: 0"), TEXT("Warp me to career: 31"), TEXT("Warp me to career: -1"), TEXT("Give me bucks or give me death: 0"), TEXT("Give me bucks or give me death: 50000"), TEXT("Give me bucks or give me death: none")})
		TestTrue(Invalid, Parse(Invalid).IsEmpty());
	const auto Warp = Parse(TEXT("Warp me to career: 123"));
	TestTrue(TEXT("Original two-character atoi bound"), Warp.Num() == 1 && Warp[0].Value == 12);
	const auto Money = Parse(TEXT("Give me bucks or give me death: 123456"));
	TestTrue(TEXT("Original five-character atoi bound"), Money.Num() == 1 && Money[0].Value == 12345);
	TestFalse(TEXT("Gamble below threshold loses"), WinsMoneyGamble(1000, 25499));
	TestTrue(TEXT("Gamble at threshold wins"), WinsMoneyGamble(1000, 25500));
	TestFalse(TEXT("Invalid amount cannot win"), WinsMoneyGamble(50000, 49999));
	TestEqual(TEXT("3 maps to Apache runtime type"), AircraftTypeForKey(EKeys::Three), 2);
	TestEqual(TEXT("9 maps to MD520 runtime type"), AircraftTypeForKey(EKeys::Nine), 8);
	TestEqual(TEXT("Zero is not an aircraft shortcut"), AircraftTypeForKey(EKeys::Zero), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatFlightTest, "SimCopter.Cheats.FuelShieldsAndTurbo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatFlightTest::RunTest(const FString&)
{
	FSimCopterFlightModel Normal;
	Normal.ResetOnSurface(0, 0, 0);
	Normal.State = ESimCopterFlightState::Flying;
	Normal.Altitude = SimCopterFixed::FromFloat(500);
	Normal.RotorSpeed = FSimCopterFlightModel::RotorLiftGate;
	FSimCopterFlightModel Protected = Normal;
	Protected.bCheatInfiniteFuel = Protected.bCheatInvulnerable = true;
	const int32 Fuel = Normal.Fuel, Health = Normal.HitPoints;
	FSimCopterFlightInputs Inputs;
	FSimCopterFlightEnvironment Env;
	Env.bTerrainFlat = true;
	FSimCopterFlightEvents Events;
	Normal.Step(0.05f, Inputs, Env, Events);
	Protected.Step(0.05f, Inputs, Env, Events);
	TestTrue(TEXT("Normal fuel still burns"), Normal.Fuel < Fuel);
	TestEqual(TEXT("Cheat preserves fuel"), Protected.Fuel, Fuel);
	Normal.NotifyObjectCollision(Events);
	TestTrue(TEXT("Normal collision still damages"), Normal.HitPoints < Health);
	Protected.NotifyObjectCollision(Events);
	TestEqual(TEXT("Shields prevent collision damage"), Protected.HitPoints, Health);
	Protected.bCheatInvulnerable = false;
	Protected.NotifyObjectCollision(Events);
	TestTrue(TEXT("Disabling shields restores damage"), Protected.HitPoints < Health);
	Protected.bCheatInfiniteFuel = false;
	Protected.Step(0.05f, Inputs, Env, Events);
	TestTrue(TEXT("Disabling fuel cheat restores burn"), Protected.Fuel < Fuel);
	FSimCopterFlightModel Turbo = Normal, Standard = Normal;
	Turbo.Altitude = Standard.Altitude = SimCopterFixed::FromFloat(50);
	Turbo.ClimbSpeed = Standard.ClimbSpeed = SimCopterFixed::FromFloat(5);
	Inputs.ClimbCommand = 1;
	Standard.Step(0.05f, Inputs, Env, Events);
	Inputs.bTurbo = true;
	Turbo.Step(0.05f, Inputs, Env, Events);
	TestTrue(TEXT("Shift superpower boosts climb"), Turbo.Altitude > Standard.Altitude);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatDestructionTest, "SimCopter.Cheats.NuclearPreservesServices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatDestructionTest::RunTest(const FString&)
{
	using namespace SimCopterCheats;
	for (int32 Id : {0, 4, 0x1d, 0x6b, 0xd1, 0xd2, 0xd3, 0xde, 0xf6})
		for (int32 Roll = 0; Roll < 4; ++Roll)
			TestFalse(TEXT("Essential service/road/airport survives"), CanNuclearBlastDemolish(Id, Roll));
	TestFalse(TEXT("One in four buildings survives"), CanNuclearBlastDemolish(0x70, 0));
	for (int32 Roll = 1; Roll < 4; ++Roll)
		TestTrue(TEXT("Three in four buildings demolished"), CanNuclearBlastDemolish(0x70, Roll));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatDeliveryTest, "SimCopter.Cheats.CEOFreeDeliveryAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatDeliveryTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Instance = NewObject<UGameInstance>(GEngine);
	auto* Career = NewObject<USimCopterCareerSubsystem>(Instance);
	Career->BeginCareer();
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->AddSessionCash(1234 - Missions->GetSessionCash());
	auto* Current = World->SpawnActor<ASimCopterHelicopterPawn>();
	Current->SwitchHelicopterModel(4);
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	if (!TestTrue(TEXT("Airport data loads"), Traffic->RebuildSpawnData())) return false;
	FVector Pad;
	if (!TestTrue(TEXT("Pad exists"), Traffic->TryGetAirportPadWorldLocation(0, Pad))) return false;
	Current->PlaceOnHelipad(Pad, 0);
	auto* Hangar = World->SpawnActor<ASimCopterHangar>();
	if (!TestTrue(TEXT("Hangar placed"), Hangar->PlaceAtAirport(Traffic, Pad))) return false;
	SimCopterHangarShop::FContext Shop;
	Shop.Career = Career; Shop.Missions = Missions; Shop.Helicopter = Current; Shop.Hangar = Hangar;
	FString Message;
	TestFalse(TEXT("Digits require CEO code"), SimCopterHangarShop::DeliverCheatHelicopter(Shop, 2, Message));
	Career->Cheats.bCEO = true;
	const FTransform Before = Current->GetActorTransform();
	if (!TestTrue(TEXT("CEO delivers Apache"), SimCopterHangarShop::DeliverCheatHelicopter(Shop, 2, Message))) { AddError(Message); return false; }
	TestEqual(TEXT("No money charged"), Missions->GetSessionCash(), 1234);
	TestTrue(TEXT("Delivered aircraft owned"), Career->OwnsHelicopter(2));
	TestTrue(TEXT("Current aircraft stays parked"), Before.Equals(Current->GetActorTransform()));
	TestEqual(TEXT("Current aircraft type unchanged"), Current->GetHelicopterTypeIndex(), 4);
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>();
	Pilot->BeginAirborneExit(FVector(0, 0, 10000), FVector(0, 0, -1000));
	Pilot->ToggleParachute();
	TestTrue(TEXT("Pilot has open parachute before teleport"), Pilot->IsParachuteDeployed());
	Pilot->TeleportForCheat(FVector(0, 0, 100));
	TestFalse(TEXT("Home teleport folds parachute"), Pilot->IsParachuteDeployed());
	TestTrue(TEXT("Home teleport clears falling velocity"), Pilot->GetCharacterMovement()->Velocity.IsNearlyZero());
	TestFalse(TEXT("Repeated key cannot duplicate owned model"), SimCopterHangarShop::DeliverCheatHelicopter(Shop, 2, Message));
	Career->ContinueCareerIntoNextCity();
	TestTrue(TEXT("Cheat carries across cities"), Career->Cheats.bCEO);
	Career->BeginCareer();
	TestFalse(TEXT("New career clears cheats"), Career->Cheats.bCEO);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatDogAssetsTest, "SimCopter.Cheats.OriginalDogAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatDogAssetsTest::RunTest(const FString&)
{
	FPrivAnimModel Model; FString Error;
	if (!TestTrue(TEXT("Original animation data loads"), FSimCopterPrivAnimReader::LoadFromFile(
		FSimCopterPrivAnimReader::ResolvePrivAnimPath(SimCopterOriginalGame::ResolveRoot()), Model, Error))) return false;
	const int32 Index = Model.FindFigureIndex(TEXT("2DOGG"));
	if (!TestTrue(TEXT("Original dog exists"), Model.Figures.IsValidIndex(Index))) return false;
	TestNotNull(TEXT("Dog standing animation exists"), Model.FindClip(Model.Figures[Index], TEXT("DgSt")));
	TestNotNull(TEXT("Dog running animation exists"), Model.FindClip(Model.Figures[Index], TEXT("DgRn")));
	TestTrue(TEXT("Pilot figure can be restored"), Model.FindFigureIndex(TEXT("pilot")) != INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatDialogTest, "SimCopter.Cheats.DialogEscape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatDialogTest::RunTest(const FString&)
{
	int32 Closed = 0;
	auto Dialog = SNew(SSimCopterCheatDialog).OnClosed(FSimpleDelegate::CreateLambda([&] { ++Closed; }));
	TestTrue(TEXT("Code field receives initial focus"), Dialog->GetInitialFocusWidget().IsValid());
	FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("Escape intercepted before text field"), Dialog->OnPreviewKeyDown(FGeometry(), Escape).IsEventHandled());
	TestEqual(TEXT("Escape closes once"), Closed, 1);
	int32 Executed = 0;
	FString Entered;
	auto Accept = SNew(SSimCopterCheatDialog)
		.OnClosed(FSimpleDelegate::CreateLambda([&] { ++Closed; }))
		.OnEntered(FOnSimCopterCheatEntered::CreateLambda([&](const FString& Text)
		{
			TestEqual(TEXT("Original page closes before applying code"), Closed, 2);
			Entered = Text; ++Executed; return FString();
		}));
	for (TCHAR Character : FString(TEXT("HSIx")))
		Accept->OnKeyChar(FGeometry(), FCharacterEvent(Character, FModifierKeysState(), 0, false));
	Accept->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::BackSpace, FModifierKeysState(), 0, false, 0, 0));
	Accept->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0));
	TestEqual(TEXT("Original append/backspace entry delivers code without caret"), Entered, FString(TEXT("HSI")));
	Accept->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, true, 0, 0));
	TestEqual(TEXT("Held Enter cannot toggle a cheat twice"), Executed, 1);
	auto Limit = SNew(SSimCopterCheatDialog)
		.OnEntered(FOnSimCopterCheatEntered::CreateLambda([&](const FString& Text) { Entered = Text; return FString(); }));
	for (int32 Index = 0; Index < 150; ++Index)
		Limit->OnKeyChar(FGeometry(), FCharacterEvent(TEXT('x'), FModifierKeysState(), 0, false));
	Limit->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0));
	TestEqual(TEXT("Original 128-byte buffer includes underscore caret"), Entered.Len(), 127);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatDialogArtTest, "SimCopter.Cheats.OriginalDialogArt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatDialogArtTest::RunTest(const FString&)
{
	TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
	Art->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	for (const TCHAR* Name : {TEXT("MBox.bmp"), TEXT("MBoxCht.bmp"), TEXT("MBoxl.bmp"), TEXT("button.bmp")})
		if (!TestNotNull(Name, Art->GetBitmap(Name))) return false;
	TestEqual(TEXT("Original page size"), FVector2D(Art->GetBitmap(TEXT("MBox.bmp"))->ImageSize), FVector2D(465, 353));
	TestEqual(TEXT("Original input-well size"), FVector2D(Art->GetBitmap(TEXT("MBoxCht.bmp"))->ImageSize), FVector2D(270, 100));
	TestTrue(TEXT("Opaque inspection cannot poison the keyed page cache"),
		Art->GetBitmap(TEXT("MBox.bmp"))->GetResourceObject() != Art->GetBitmap(TEXT("MBOX.BMP"), true)->GetResourceObject());
	if (!FParse::Param(FCommandLine::Get(), TEXT("SimCheatDialogPreview"))) return true;
	for (int32 Example = 0; Example < 2; ++Example)
	{
		auto Dialog = SNew(SSimCopterCheatDialog).Art(Art.Get());
		const FString Code = Example == 0 ? TEXT("Lights, Camera, Action!") : TEXT("A megaphone in the hand is worth two in the bush");
		for (TCHAR Character : Code)
			Dialog->OnKeyChar(FGeometry(), FCharacterEvent(Character, FModifierKeysState(), 0, false));
		FWidgetRenderer Renderer(true);
		const FVector2D Size(1280, 960);
		Renderer.DrawWidget(Dialog, Size);
		auto* Target = Renderer.DrawWidget(Dialog, Size);
		if (!TestNotNull(TEXT("Original cheat dialog rendered"), Target)) return false;
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags);
		TestTrue(TEXT("Cyan page corners are transparent"), Pixels[128 * 1280 + 176].A < 10);
		TestTrue(TEXT("Original dialog preview saved"), FImageUtils::SaveImageByExtension(
			*(FPaths::ProjectDir() / FString::Printf(TEXT("../Docs/scratchpad/drive-in-ufo/cheat-dialog-%d.png"), Example)),
			FImageView(Pixels.GetData(), 1280, 960)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheatCityEffectsTest, "SimCopter.Cheats.CityEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheatCityEffectsTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
	City->bLoadOnConstruction = false;
	City->bRenderProceduralMapExtension = false;
	City->bRenderStreetLightSpotLights = false;
	City->CityFile.FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame/cities/career/city1.sc2"));
	City->FinishSpawning(FTransform::Identity);
	City->RebuildCity();
	TestTrue(TEXT("Original portrait models retained"), City->CheatPortraitComponents.Num() > 0);
	for (int32 Index : City->CheatPortraitComponents)
		TestFalse(TEXT("Portrait hidden normally"), City->BuildingInstanceComponents[Index]->IsVisible());
	City->ShowCheatPortraits();
	for (int32 Index : City->CheatPortraitComponents)
		TestTrue(TEXT("PAM reveals original portrait geometry"), City->BuildingInstanceComponents[Index]->IsVisible());
	TArray<FIntPoint> Services;
	for (const auto& Building : City->Buildings)
		if (!Building.bDemolished && !SimCopterCheats::CanNuclearBlastDemolish(Building.XbldId, 1)) Services.Add(Building.OriginTile);
	TArray<FIntPoint> Cleared;
	FMath::RandInit(901);
	TestTrue(TEXT("Blast demolishes real instances"), City->ApplyNuclearCheat(Cleared) > 0);
	TestNotNull(TEXT("Blast creates scattered original tile effects"), City->FindComponentByClass<USimCopterParticleFXComponent>());
	TSet<FIntPoint> Unique;
	for (auto Tile : Cleared)
	{
		TestFalse(TEXT("Footprints cleared only once"), Unique.Contains(Tile)); Unique.Add(Tile);
		TestFalse(TEXT("Destroyed building no longer standing"), City->HasStandingBuildingAtTile(Tile.X, Tile.Y));
	}
	for (auto Tile : Services)
		TestTrue(TEXT("Essential building survives"), City->HasStandingBuildingAtTile(Tile.X, Tile.Y));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDriveInCommandsTest, "SimCopter.Cheats.DriveInCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterDriveInCommandsTest::RunTest(const FString&)
{
	using namespace SimCopterCheats;
	TestEqual(TEXT("HSI exact command"), Parse(TEXT("HSI"))[0].Code, ECode::HSI);
	TestEqual(TEXT("HSI not a substring"), Parse(TEXT("THISIS" )).Num(), 0);
	TestEqual(TEXT("Stop movie command"), Parse(TEXT("Stop video"))[0].Code, ECode::StopVideo);
	auto Commands = Parse(TEXT("Play video: Radioactivity.mp4"));
	TestEqual(TEXT("Filename cannot trigger other cheats"), Commands.Num(), 1);
	TestEqual(TEXT("Filename preserved"), Commands[0].FileName, FString(TEXT("Radioactivity.mp4")));
	FString Path, Error;
	for (const FString& Bad : {TEXT("../HSI.mp4"), TEXT("C:\\HSI.mp4"), TEXT("https://a/HSI.mp4"), TEXT("HSI.exe"), TEXT("")})
		TestFalse(TEXT("Unsafe or unsupported filename rejected"), SimCopterDriveIn::ResolveVideo(Bad, Path, Error));
	int32 Fires = 0;
	for (int32 Roll = 0; Roll < 32768; ++Roll) Fires += ShouldSpawnPostBlastFire(Roll) ? 1 : 0;
	TestEqual(TEXT("Original one-in-32 post-blast scatter"), Fires, 1024);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGortAssetsTest, "SimCopter.Cheats.GortOriginalAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGortAssetsTest::RunTest(const FString&)
{
	FSimCopterPcmClip Clip;
	const FString SoundDir = SimCopterOriginalGame::ResolveRoot() / TEXT("sound");
	for (int32 Voice = 1; Voice <= 20; ++Voice)
	{
		FSimCopterPcmClip Mono;
		if (!TestTrue(TEXT("Original voice decodes"), USimCopterAudioSubsystem::DecodeWav(
			SoundDir / FString::Printf(TEXT("al%02d.wav"), Voice), Mono))) return false;
		for (int32 Speaker = 0; Speaker < 2; ++Speaker)
		{
			if (!TestTrue(TEXT("Panned voice builds"), USimCopterAudioSubsystem::BuildGortVoice(SoundDir, Voice, Speaker, Clip))) return false;
			TestEqual(TEXT("Original speech sample rate"), Clip.SampleRate, 22050);
			TestEqual(TEXT("Stereo presentation"), Clip.Channels, 2);
			TestEqual(TEXT("No resampling or time stretching"), Clip.Duration, Mono.Duration);
			TestTrue(TEXT("Every clip fits inside its original subtitle hold"), Clip.Duration > 0 && Clip.Duration < 4);
			bool bSamplesMatch = true;
			for (int32 Sample = 0; Sample < Mono.Pcm16.Num() / 2; ++Sample)
			{
				for (int32 Byte = 0; Byte < 2; ++Byte)
				{
					bSamplesMatch &= Clip.Pcm16[Sample * 4 + Speaker * 2 + Byte] == Mono.Pcm16[Sample * 2 + Byte];
					bSamplesMatch &= Clip.Pcm16[Sample * 4 + (1 - Speaker) * 2 + Byte] == 0;
				}
			}
			TestTrue(TEXT("Original samples only in the speaking alien's channel"), bSamplesMatch);
		}
	}
	TestFalse(TEXT("Missing voice fails safely"), USimCopterAudioSubsystem::BuildGortVoice(SoundDir / TEXT("missing"), 1, 0, Clip));
	TestFalse(TEXT("Invalid voice rejected"), USimCopterAudioSubsystem::BuildGortVoice(SoundDir, 21, 0, Clip));
	FMaxisTextureImage Image; FString Error;
	TestTrue(TEXT("Original image decodes"), FMaxisWindowsBitmapReader::LoadPalettedBitmapFromFile(
		SimCopterOriginalGame::ResolveRoot() / TEXT("bmp/martian.bmp"), Image, Error));
	TestEqual(TEXT("Original ending image width"), Image.Width, 640);
	TestEqual(TEXT("Original ending image height"), Image.Height, 480);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGortTimelineTest, "SimCopter.Cheats.GortTimeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGortTimelineTest::RunTest(const FString&)
{
	using namespace SimCopterGort;
	const auto Cues = GetCues();
	TestEqual(TEXT("All original strings 964 through 999"), Cues.Num(), 36);
	TestEqual(TEXT("Intro holds before first line"), FindCue(3.999), INDEX_NONE);
	TestEqual(TEXT("First voice and subtitle begin at four seconds"), FindCue(4), 0);
	TestEqual(TEXT("Speaker change at eight seconds"), FindCue(8), 1);
	TestEqual(TEXT("Original unusual voice order, string 977"), Cues[13].Voice, 15);
	TestEqual(TEXT("Original unusual voice order, string 978"), Cues[14].Voice, 17);
	TestEqual(TEXT("Original unusual voice order, string 979"), Cues[15].Voice, 19);
	TestEqual(TEXT("Same alien speaks four consecutive lines"), Cues[15].Speaker, 0);
	TestEqual(TEXT("Ten-second bonus pause holds Trust me"), FindCue(89.999), 19);
	TestEqual(TEXT("Bonus conversation begins at 90 seconds"), FindCue(90), 20);
	TestEqual(TEXT("Last line remains readable for six seconds"), FindCue(185.999), 35);
	TestEqual(TEXT("Finite ending releases final subtitle"), FindCue(186), INDEX_NONE);
	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		TestEqual(TEXT("Cue boundary selects exactly one line"), FindCue(Cues[Index].StartSeconds), Index);
		TestEqual(TEXT("No early subtitle change"), FindCue(Cues[Index].StartSeconds - .001), Index - 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGortPlaybackTest, "SimCopter.Cheats.GortPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGortPlaybackTest::RunTest(const FString&)
{
	// No audio device, image, world tick or loaded city is needed to progress or exit.
	int32 Closed = 0;
	auto Sequence = SNew(SSimCopterGortSequence).OnClosed(FSimpleDelegate::CreateLambda([&] { ++Closed; }));
	Sequence->Tick(FGeometry(), 0, 4);
	TestEqual(TEXT("First subtitle"), Sequence->GetSubtitle(0).ToString(), FString(TEXT("That human contains excessive flying abilities.")));
	TestTrue(TEXT("Other speaker cleared"), Sequence->GetSubtitle(1).IsEmpty());
	Sequence->Tick(FGeometry(), 0, 4);
	TestTrue(TEXT("Previous subtitle removed when speaker changes"), Sequence->GetSubtitle(0).IsEmpty());
	TestEqual(TEXT("Second subtitle"), Sequence->GetSubtitle(1).ToString(), FString(TEXT("Yes, it is very well.")));
	Sequence->Tick(FGeometry(), 0, 172);
	TestTrue(TEXT("A delayed frame selects current bonus line, not a backlog"), Sequence->GetSubtitle(1).ToString().StartsWith(TEXT("An installer")));
	Sequence->Tick(FGeometry(), 0, 5);
	TestEqual(TEXT("Final line allowed to finish"), Closed, 0);
	Sequence->Tick(FGeometry(), 0, 1);
	TestEqual(TEXT("Completion closes automatically"), Closed, 1);
	Sequence->Tick(FGeometry(), 0, 100);
	Sequence->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
	TestEqual(TEXT("Completion and escape cannot unpause twice"), Closed, 1);
	for (float SkipAt : {0.f, .25f, 4.f, 85.f, 185.f})
	{
		for (FKey Key : {EKeys::Escape, EKeys::Gamepad_FaceButton_Right})
		{
			int32 Skipped = 0;
			auto Skip = SNew(SSimCopterGortSequence).OnClosed(FSimpleDelegate::CreateLambda([&] { ++Skipped; }));
			Skip->Tick(FGeometry(), 0, SkipAt);
			TestTrue(TEXT("Escape/B always intercepted"), Skip->OnPreviewKeyDown(FGeometry(), FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0)).IsEventHandled());
			Skip->Tick(FGeometry(), 0, 400);
			TestEqual(TEXT("Skip returns once at any point"), Skipped, 1);
			TestTrue(TEXT("Closed subtitle cleared"), Skip->GetSubtitle(0).IsEmpty() && Skip->GetSubtitle(1).IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGortRenderTest, "SimCopter.Cheats.GortRender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGortRenderTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("SimGortPreview"))) return true;
	TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
	Art->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	const auto* Brush = Art->GetBitmap(TEXT("martian.bmp"));
	if (!TestNotNull(TEXT("Original alien image"), Brush)) return false;
	for (float Time : {0.f, .25f, 4.f, 8.f, 180.f})
	{
		auto Sequence = SNew(SSimCopterGortSequence).Art(Brush);
		Sequence->Tick(FGeometry(), 0, Time);
		FWidgetRenderer Renderer(true);
		const FVector2D Size(1280, 960);
		Renderer.DrawWidget(Sequence, Size);
		auto* Target = Renderer.DrawWidget(Sequence, Size);
		if (!TestNotNull(TEXT("Gort frame rendered"), Target)) return false;
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags);
		TestTrue(TEXT("Preview saved"), FImageUtils::SaveImageByExtension(
			*(FPaths::ProjectDir() / FString::Printf(TEXT("../Docs/scratchpad/gort-sequence/gort-%06.2f.png"), Time)),
			FImageView(Pixels.GetData(), 1280, 960)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGortReturnTest, "SimCopter.Cheats.GortReturnToGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGortReturnTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	auto* Audio = World->GetSubsystem<USimCopterAudioSubsystem>();
	if (!TestNotNull(TEXT("World audio subsystem"), Audio)) return false;
	Audio->bSoundsAvailable = true;
	auto* UnrelatedVoice = Audio->PlayGortVoice(2, 1);
	if (!TestNotNull(TEXT("Fixture standalone voice"), UnrelatedVoice)) return false;
	const int32 BaselineVoices = Audio->LooseComponents.Num();
	for (bool bOnFoot : {false, true})
	{
		APawn* Pawn = bOnFoot ? static_cast<APawn*>(World->SpawnActor<ASimCopterOnFootPawn>())
			: static_cast<APawn*>(World->SpawnActor<ASimCopterHelicopterPawn>());
		Controller->Possess(Pawn);
		for (bool bFinishNaturally : {false, true})
		{
			// Exercise the real BuildScreen -> CloseScreen binding without taking
			// over the desktop viewport. An independent focus-loss pause must survive.
			Controller->PushPause();
			Controller->PushPause();
			Controller->Screen = ESimCopterSettingsScreen::CheatEnding;
			auto Sequence = StaticCastSharedRef<SSimCopterGortSequence>(Controller->BuildScreen(Controller->Screen));
			Controller->ScreenWidget = Sequence;
			TestTrue(TEXT("Ending receives keyboard focus"), Controller->InitialFocusWidget == Sequence);
			TestEqual(TEXT("No sound before first subtitle"), Audio->LooseComponents.Num(), BaselineVoices);
			Sequence->Tick(FGeometry(), 0, 4);
			TestEqual(TEXT("First subtitle starts exactly one voice"), Audio->LooseComponents.Num(), BaselineVoices + 1);
			auto* FirstVoice = Audio->LooseComponents.Last().Get();
			TestTrue(TEXT("Gort remains audible while world is paused"), FirstVoice->bIsUISound);
			Sequence->Tick(FGeometry(), 0, .1f);
			TestTrue(TEXT("Holding a subtitle does not restart its recording"), Audio->LooseComponents.Last() == FirstVoice);
			Sequence->Tick(FGeometry(), 0, 4);
			TestFalse(TEXT("Next speaker releases previous voice"), FirstVoice->IsRegistered());
			TestEqual(TEXT("Only one Gort voice at a time"), Audio->LooseComponents.Num(), BaselineVoices + 1);
			auto* LastVoice = Audio->LooseComponents.Last().Get();
			if (bFinishNaturally) Sequence->Tick(FGeometry(), 0, 186);
			else Sequence->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
			TestFalse(TEXT("Voice stopped before the retained widget is destroyed"), LastVoice->IsRegistered());
			TestEqual(TEXT("Unrelated audio retained"), Audio->LooseComponents.Num(), BaselineVoices);
			TestFalse(TEXT("Ending screen closed"), Controller->IsSettingsOpen());
			TestFalse(TEXT("UI focus owner released"), Controller->InitialFocusWidget.IsValid());
			TestEqual(TEXT("Only ending's pause released"), Controller->PauseDepth, 1);
			TestTrue(TEXT("Same pawn after Gort"), Controller->GetPawn() == Pawn);
			TestEqual(TEXT("Correct cursor mode restored"), bool(Controller->bShowMouseCursor), !bOnFoot);
			Controller->PopPause();
			TestEqual(TEXT("Game resumes when final pause owner releases"), Controller->PauseDepth, 0);
		}
		Controller->UnPossess();
		Pawn->Destroy();
	}
	Audio->StopStandaloneSound(UnrelatedVoice);
	return true;
}
#endif

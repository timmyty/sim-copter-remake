#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/SimCopterSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SSimCopterSaveGamePicker.h"
#include "UI/SimCopterHangarArt.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterExternalSaveTest, "SimCopter.SaveGame.ExternalFiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterExternalSaveTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World = Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Docs/scratchpad/open-save-browser/files") / FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*Folder, true);
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Saves = Instance->GetSubsystem<USimCopterSaveSubsystem>();
	auto* Session = Instance->GetSubsystem<USimCopterSessionSubsystem>();
	auto* Save = NewObject<USimCopterSaveGame>();
	Save->DisplayName = TEXT("An external save"); Save->CityWeights.Init(1.0f, 7); Save->bHasTimeOfDayState = true;
	Save->Kind = ESimCopterSessionKind::Career;
	const FString CareerPath = Folder / TEXT("Renamed career checkpoint.sav");
	const auto WriteSave = [&](const FString& Path)
	{
		TArray<uint8> Bytes;
		return UGameplayStatics::SaveGameToMemory(Save, Bytes) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	};
	FString Error;
	if (!TestTrue(TEXT("Write external career fixture"), WriteSave(CareerPath))) return false;
	TestTrue(TEXT("Career opens from arbitrary path and filename"), Saves->LoadGame(CareerPath, ESimCopterSessionKind::Career, Error));
	TestEqual(TEXT("Career session requested"), Session->GetSessionKind(), ESimCopterSessionKind::Career);
	TestEqual(TEXT("Career city resolves normally"), Session->GetCityFilePath(), USimCopterSessionSubsystem::ResolveCareerCityFilePath(0));
	const FString PreviousCity = Session->GetCityFilePath();
	TestFalse(TEXT("User opener rejects career file"), Saves->LoadGame(CareerPath, ESimCopterSessionKind::User, Error));
	TestTrue(TEXT("Wrong-kind rejection explains itself"), Error.Contains(TEXT("not a user-game")));
	TestEqual(TEXT("Failed open preserves active load"), Saves->GetCurrentSlotName(), CareerPath);
	TestEqual(TEXT("Failed open preserves city selection"), Session->GetCityFilePath(), PreviousCity);

	Save->Kind = ESimCopterSessionKind::User;
	Save->UserCityFileName = TEXT("A custom city.SC2");
	const FString MapFolder = Folder / TEXT("Elsewhere"); IFileManager::Get().MakeDirectory(*MapFolder, true);
	Save->UserCitySourcePath = MapFolder / Save->UserCityFileName;
	TestEqual(TEXT("Copy real SC2 fixture outside game folders"), IFileManager::Get().Copy(*Save->UserCitySourcePath, *PreviousCity), COPY_OK);
	const FString UserPath = Folder / TEXT("My custom flight.sav");
	TestTrue(TEXT("Write external user fixture"), WriteSave(UserPath));
	TestFalse(TEXT("Career opener rejects user file"), Saves->LoadGame(UserPath, ESimCopterSessionKind::Career, Error));
	TestTrue(TEXT("User save remembers city in a different folder"), Saves->LoadGame(UserPath, ESimCopterSessionKind::User, Error));
	TestEqual(TEXT("Serialized absolute SC2 path survives"), Session->GetCityFilePath(), Save->UserCitySourcePath);

	// An old save has only a basename. A portable save + map pair must still work.
	Save->FormatVersion = 4; Save->UserCitySourcePath.Reset();
	const FString NearbyMap = Folder / Save->UserCityFileName;
	TestEqual(TEXT("Place city beside relocated save"), IFileManager::Get().Copy(*NearbyMap, *PreviousCity), COPY_OK);
	TestTrue(TEXT("Write legacy external user fixture"), WriteSave(UserPath));
	TestTrue(TEXT("Legacy user save finds adjacent city"), Saves->LoadGame(UserPath, ESimCopterSessionKind::User, Error));
	TestEqual(TEXT("Adjacent city selected"), Session->GetCityFilePath(), NearbyMap);
	Save->UserCitySourcePath = Folder / TEXT("missing/original/location.SC2");
	TestTrue(TEXT("Write moved-city fixture"), WriteSave(UserPath));
	TestTrue(TEXT("Stale absolute path falls back to neighboring city"), Saves->LoadGame(UserPath, ESimCopterSessionKind::User, Error));
	Save->UserCityFileName = TEXT("Missing completely.SC2");
	TestTrue(TEXT("Write missing-city fixture"), WriteSave(UserPath));
	TestFalse(TEXT("Missing SC2 is rejected clearly"), Saves->LoadGame(UserPath, ESimCopterSessionKind::User, Error));
	TestEqual(TEXT("Missing-city failure preserves prior city"), Session->GetCityFilePath(), NearbyMap);
	TestTrue(TEXT("Error explains where to place the city"), Error.Contains(TEXT("beside this saved game")));
	const FString BrokenPath = Folder / TEXT("Invalid.sav");
	FFileHelper::SaveStringToFile(TEXT("Not a save"), *BrokenPath);
	TestFalse(TEXT("Non-save files fail before Unreal deserialization"), Saves->LoadGame(BrokenPath, ESimCopterSessionKind::User, Error));
	TestFalse(TEXT("Missing save file is rejected"), Saves->LoadGame(Folder / TEXT("Missing.sav"), ESimCopterSessionKind::User, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSaveBrowseMenuTest, "SimCopter.FrontEnd.SaveBrowseMenus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterSaveBrowseMenuTest::RunTest(const FString&)
{
	TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
	Art->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	for (ESimCopterSessionKind Kind : {ESimCopterSessionKind::Career, ESimCopterSessionKind::User})
	{
		int32 Cancelled = 0, Opened = 0;
		auto Menu = SNew(SSimCopterSaveGamePicker).Art(Art.Get()).Kind(Kind)
			.OnAccepted(FOnSimCopterSaveGameChosen::CreateLambda([&](const FString&) { ++Opened; }))
			.OnCancelled(FSimpleDelegate::CreateLambda([&] { ++Cancelled; }));
		int32 Buttons = 0, BrowseLabels = 0;
		TFunction<void(TSharedRef<SWidget>)> Visit = [&](TSharedRef<SWidget> Widget)
		{
			if (Widget->GetTypeAsString() == TEXT("SButton")) ++Buttons;
			if (Widget->GetTypeAsString() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == TEXT("Browse...")) ++BrowseLabels;
			for (int32 Index = 0; Index < Widget->GetChildren()->Num(); ++Index) Visit(Widget->GetChildren()->GetChildAt(Index));
		};
		Visit(Menu);
		TestEqual(TEXT("Both menus have Open, Cancel and Browse"), Buttons, 3);
		TestEqual(TEXT("Browse is available even with no local saves"), BrowseLabels, 1);
		Menu->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0));
		TestEqual(TEXT("Empty Open leaves selection intact"), Opened, 0);
		Menu->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
		TestEqual(TEXT("Escape still cancels"), Cancelled, 1);
		if (FParse::Param(FCommandLine::Get(), TEXT("SimCheatMenuPreview")) && FApp::CanEverRender())
		{
			FWidgetRenderer Renderer(true); const FVector2D Size(1280, 960);
			Renderer.DrawWidget(Menu, Size); auto* Target = Renderer.DrawWidget(Menu, Size);
			if (!TestNotNull(TEXT("Save picker renders"), Target)) return false;
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags);
			const FString Name = Kind == ESimCopterSessionKind::Career ? TEXT("open-career.png") : TEXT("open-user.png");
			TestTrue(TEXT("Save picker image saved"), FImageUtils::SaveImageByExtension(
				*(FPaths::ProjectDir() / TEXT("../Docs/scratchpad/open-save-browser") / Name), FImageView(Pixels.GetData(), 1280, 960)));
		}
	}
	return true;
}
#endif

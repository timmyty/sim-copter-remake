#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "UI/SSimCopterHangarMenu.h"
#include "UI/SSimCopterCitySettings.h"
#include "UI/SimCopterHangarArt.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SVirtualWindow.h"
#include "Input/HittestGrid.h"
#include "Types/PaintArgs.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPlaytestMenuTest,"SimCopter.PlaytestOctober.Menus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPlaytestMenuTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	SimCopterHangarShop::FContext Shop; Shop.Career=Instance->GetSubsystem<USimCopterCareerSubsystem>();
	for(int32 I=0;I<80;++I) Shop.Career->AddLogEntry(ESimCopterCareerLogKind::MissionEnded,
		FString::Printf(TEXT("Entry %02d: Harbor accepted the boat. Recovery complete."),I),1,I*60);
	auto* Art=NewObject<USimCopterHangarArt>(Instance); Art->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	auto Menu=SNew(SSimCopterHangarMenu).Shop(Shop).Art(Art);
	Menu->ShowPage(SSimCopterHangarMenu::EPage::MissionLog);
	auto Window=SNew(SVirtualWindow).Size(FVector2D(640,480)); Window->SetContent(Menu);
	auto Arrange=[&]()
	{
		Window->SlatePrepass(); FHittestGrid Grid; Grid.SetHittestArea(FVector2D::ZeroVector,FVector2D(640,480));
		FSlateWindowElementList Elements(Window); FPaintArgs Args(nullptr,Grid,FVector2D::ZeroVector,0,0);
		Window->Paint(Args,FGeometry::MakeRoot(FVector2D(640,480),FSlateLayoutTransform()),FSlateRect(0,0,640,480),Elements,0,FWidgetStyle(),true);
	};
	Arrange(); Arrange();
	if(!TestTrue(TEXT("Long mission history is scrollable"),Menu->LogScroll.IsValid() && Menu->LogScroll->GetScrollOffsetOfEnd()>500)) return false;
	Menu->SetControllerSelection(Menu->LogPanel); Menu->LogScroll->SetScrollOffset(0);
	Menu->OnPreviewKeyDown(Menu->GetCachedGeometry(),FKeyEvent(EKeys::Gamepad_DPad_Down,FModifierKeysState(),0,false,0,0));
	TestTrue(TEXT("Controller scrolls the mission log"),Menu->LogScroll->GetScrollOffset()>0);
	FSimCopterCitySettingsValues Values; Values.Difficulty=3; Values.MissionFrequency=2;
	for(float& Weight:Values.Weights) Weight=50;
	auto Settings=SNew(SSimCopterCitySettings).Art(Art).Values(Values).bProgressive(true).ProgressionText(TEXT("Select any stage."));
	int32 EditableSliders=0;
	TFunction<void(TSharedRef<SWidget>)> Visit=[&](TSharedRef<SWidget> Widget)
	{
		if((Widget->GetTypeAsString()==TEXT("SSimCopterCheckupSlider") || Widget->GetTypeAsString()==TEXT("SSlider")) && Widget->IsEnabled()) ++EditableSliders;
		for(int32 I=0;I<Widget->GetChildren()->Num();++I) Visit(Widget->GetChildren()->GetChildAt(I));
	};
	Visit(Settings); TestEqual(TEXT("User-city progress, seven weights, and frequency are editable"),EditableSliders,9);
	if(!GUsingNullRHI)
	{
		auto Capture=[&](TSharedRef<SWidget> Widget,const TCHAR* Name)
		{
			FWidgetRenderer Renderer(false); Renderer.DrawWidget(Widget,FVector2D(1280,960));
			auto* Target=Renderer.DrawWidget(Widget,FVector2D(1280,960)); FlushRenderingCommands();
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
			TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1280,960,Pixels,Png);
			TestTrue(TEXT("UI review image saved"),FFileHelper::SaveArrayToFile(Png,*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/playtest-2026-10-18")/Name)));
		};
		Capture(Menu,TEXT("mission-log.png")); Capture(Settings,TEXT("city-settings.png"));
	}
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SSimCopterCockpitFrame.h"
#include "UI/SSimCopterDashboard.h"
#include "UI/SSimCopterMapPanel.h"
#include "UI/SSimCopterToolFlaps.h"
#include "UI/SimCopterHangarArt.h"
#include "Engine/World.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCockpitFrameTest,"SimCopter.UI.CockpitFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCockpitFrameTest::RunTest(const FString&)
{
	for (float Scale : {0.5f,1.0f,1.768292f,2.5f})
	{
		const FVector2D Size = SSimCopterCockpitFrame::WiringSize(Scale,FVector2D(640,480)*Scale);
		TestTrue(TEXT("Wiring occupies half the original scaled area"),FMath::IsNearlyEqual(Size.X*Size.Y,84.0*291*Scale*Scale/2,0.02));
		TestTrue(TEXT("Wiring reaches behind the current dashboard"),Size.Y>480*Scale-SSimCopterDashboard::GetPanelScreenHeight(Scale));
	}
	auto Frame = SNew(SSimCopterCockpitFrame);
	TestFalse(TEXT("Wiring starts extended"),Frame->IsCollapsed());
	TestTrue(TEXT("First arrow click is consumed"),Frame->ToggleWiring().IsEventHandled());
	TestTrue(TEXT("First arrow click retracts wiring"),Frame->IsCollapsed());
	Frame->ToggleWiring();
	TestFalse(TEXT("Second arrow click restores reduced panel"),Frame->IsCollapsed());
	TestFalse(TEXT("Frame never takes flight keyboard focus"),Frame->SupportsKeyboardFocus());
	if (!FParse::Param(FCommandLine::Get(),TEXT("SimCockpitPreview"))) return true;
	TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
	Art->SetOriginalGameRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame")));
	TestNotNull(TEXT("Original wiring loaded"),Art->GetBitmap(TEXT("DASH2.BMP")));
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SwitchHelicopterModel(4);
	for (const FVector2D Size : {FVector2D(1920,1080),FVector2D(1280,720),FVector2D(2560,1080)})
	{
		const float Scale=1.768292f;
		auto Wiring=SNew(SSimCopterCockpitFrame).Art(Art.Get()).Scale(Scale);
		auto Dash=SNew(SSimCopterDashboard).Art(Art.Get()).Pawn(Heli).Scale(Scale);
		auto Map=SNew(SSimCopterMapPanel).Art(Art.Get()).Pawn(Heli).Scale(Scale);
		auto Canvas=SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(0.13f,0.21f,0.28f))]
			+ SOverlay::Slot()[Wiring]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0,12,0,0)
			[SNew(SSimCopterToolFlaps).Art(Art.Get()).Pawn(Heli).Scale(Scale)]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)[Map]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)[Dash];
		FWidgetRenderer Renderer(true);
		FVector2f DashboardPosition, MapPosition;
		for (int32 Variant=0;Variant<2;++Variant)
		{
			if (Variant) Wiring->ToggleWiring();
			auto* Target=Renderer.DrawWidget(Canvas,Size);
			if (!TestNotNull(TEXT("Cockpit rendered offscreen"),Target)) return false;
			if (!Variant) { DashboardPosition=Dash->GetCachedGeometry().GetAbsolutePosition(); MapPosition=Map->GetCachedGeometry().GetAbsolutePosition(); }
			else
			{
				TestTrue(TEXT("Collapse leaves instrument position unchanged"),DashboardPosition.Equals(Dash->GetCachedGeometry().GetAbsolutePosition(),0.01f));
				TestTrue(TEXT("Collapse leaves map position unchanged"),MapPosition.Equals(Map->GetCachedGeometry().GetAbsolutePosition(),0.01f));
			}
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
			TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(int32(Size.X),int32(Size.Y),Pixels,Png);
			TestTrue(TEXT("Cockpit preview saved"),FFileHelper::SaveArrayToFile(Png,*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/cockpit-support")/
				FString::Printf(TEXT("cockpit-%dx%d-%s.png"),int32(Size.X),int32(Size.Y),Variant?TEXT("collapsed"):TEXT("expanded")))));
		}
	}
	return true;
}
#endif

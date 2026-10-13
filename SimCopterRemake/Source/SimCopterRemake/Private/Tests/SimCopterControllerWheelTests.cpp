#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/SSimCopterRadialWheel.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterControllerWheelPresentationTest,
	"SimCopter.Controller.WheelPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterControllerWheelPresentationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Wheel starts fully visible"), SSimCopterRadialWheel::ReleaseOpacity(0, false), 1.0f);
	TestEqual(TEXT("Wheel is gone after a quarter second"), SSimCopterRadialWheel::ReleaseOpacity(0.25, false), 0.0f);
	TestEqual(TEXT("Confirmed sector is half visible at 0.375 seconds"), SSimCopterRadialWheel::ReleaseOpacity(0.375, true), 0.5f);
	TestEqual(TEXT("Confirmed sector is gone at 0.75 seconds"), SSimCopterRadialWheel::ReleaseOpacity(0.75, true), 0.0f);
	for (bool bTools : {false, true})
	{
		const TArray<FString> Labels = bTools
			? TArray<FString>{TEXT("Water Cannon"), TEXT("Water Bucket"), TEXT("Rescue Harness"), TEXT("Megaphone"), TEXT("Tear Gas"), TEXT("Capture Cage"), TEXT("Machine Gun"), TEXT("Missile"), TEXT("Remote Taser"), TEXT("Passengers")}
			: TArray<FString>{TEXT("Fire Truck"), TEXT("Police"), TEXT("Ambulance"), TEXT("Police (Chase)"), TEXT("Air: Map Mission"), TEXT("Recall All")};
		TSharedRef<SSimCopterRadialWheel> Widget = SNew(SSimCopterRadialWheel)
			.Labels(Labels).Title(FText::FromString(bTools ? TEXT("SELECT TOOL") : TEXT("DISPATCH")))
			.Instructions(FText::FromString(bTools ? TEXT("A: SELECT   X: DISPATCH\nB: CANCEL")
				: TEXT("A: CONFIRM   X: TOOLS\nB: CANCEL")))
			.SelectedIndex(1);
		Widget->SlatePrepass();
		TestTrue(TEXT("Wheel cannot steal flight focus"), !Widget->SupportsKeyboardFocus());
		TestTrue(TEXT("Wheel has a bounded, scalable canvas"), Widget->GetDesiredSize().Equals(FVector2f(560, 600)));
		if (FParse::Param(FCommandLine::Get(), TEXT("SimControllerWheelPreview")))
		{
			// Explicit offscreen visual QA; never takes the user's foreground or drives gameplay.
			FWidgetRenderer Renderer(true);
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(Widget, FVector2D(560, 600));
			if (!TestNotNull(TEXT("Offscreen wheel rendered"), Target)) return false;
			TArray<FColor> Pixels;
			FReadSurfaceDataFlags ReadFlags;
			ReadFlags.SetLinearToGamma(false);
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags);
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(560, 600, Pixels, Png);
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Docs/scratchpad") /
				(bTools ? TEXT("controller-tools.png") : TEXT("controller-dispatch.png")));
			TestTrue(TEXT("Preview saved"), FFileHelper::SaveArrayToFile(Png, *Path));
		}
	}
	return true;
}
#endif

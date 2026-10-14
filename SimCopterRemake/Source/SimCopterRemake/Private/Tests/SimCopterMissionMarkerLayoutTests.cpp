// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "UI/SimCopterMissionMarkerLayout.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
float UiBoundaryMeasure(
	const FVector2D& Point,
	const FBox2D& Bounds,
	const FVector2D& MarkerSize,
	const float Padding)
{
	const FVector2D HalfExtents =
		Bounds.GetExtent() + MarkerSize * 0.5f + FVector2D(Padding, Padding);
	const FVector2D Local = Point - Bounds.GetCenter();
	return FMath::Max(
		FMath::Abs(Local.X) / HalfExtents.X,
		FMath::Abs(Local.Y) / HalfExtents.Y);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterMissionMarkerLabelFitsTest,
	"SimCopter.Missions.MarkerLabelFits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterMissionMarkerLabelFitsTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterMissionMarkerLayout;
	for (const TCHAR* Label : { TEXT("STALLED VEHICLE / 999 M"), TEXT("STALLED VEHICLE / 10.0 KM"), TEXT("BOAT RECOVERY / 999 M") })
	{
		TSharedRef<STextBlock> Text = SNew(STextBlock)
			.Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8));
		Text->SlatePrepass();
		const FVector2D Size = MeasureMarkerSize(Label, FVector2D(110, 63));
		TestTrue(TEXT("Full label and distance fit inside the padded plate"), Size.X >= Text->GetDesiredSize().X + 12.0);
		TestTrue(TEXT("Long labels expand beyond the old fixed width"), Size.X > 110.0);
		for (const FVector2D Desired : { FVector2D(-100, 200), FVector2D(2000, 200) })
		{
			bool bAdjusted = false;
			const FVector2D Center = ResolveMarkerCenter(Desired, Size, FVector2D(1280, 720), 10,
				TConstArrayView<FUiObstacle>(), TConstArrayView<FPlacedMarker>(), 0.5f, bAdjusted);
			TestTrue(TEXT("Full plate stays inside both viewport edges"),
				Center.X - Size.X * 0.5 >= 10.0 && Center.X + Size.X * 0.5 <= 1270.0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterMissionMarkerUiAvoidanceTest,
	"SimCopter.Missions.MarkerUiAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterMissionMarkerUiAvoidanceTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterMissionMarkerLayout;

	const FVector2D Viewport(1000.0f, 700.0f);
	const FVector2D MarkerSize(100.0f, 60.0f);
	constexpr float EdgePadding = 10.0f;
	constexpr float UiPadding = 12.0f;
	constexpr float AllowedOverlap = 0.5f;
	const FBox2D CenterPanelBounds(FVector2D(400.0f, 250.0f), FVector2D(600.0f, 450.0f));
	const TArray<FUiObstacle> CenterPanel = { FUiObstacle(CenterPanelBounds, UiPadding) };
	const FVector2D DesiredCenter(540.0f, 370.0f);

	bool bAdjusted = false;
	const FVector2D Result = ResolveMarkerCenter(
		DesiredCenter, MarkerSize, Viewport, EdgePadding,
		CenterPanel, TConstArrayView<FPlacedMarker>(), AllowedOverlap, bAdjusted);
	TestTrue(TEXT("A marker covered by UI is adjusted"), bAdjusted);
	TestTrue(TEXT("The marker reaches the exact padded UI outline"),
		FMath::IsNearlyEqual(
			UiBoundaryMeasure(Result, CenterPanelBounds, MarkerSize, UiPadding), 1.0f, 0.001f));
	TestTrue(TEXT("UI avoidance moves along one screen axis without radial projection"),
		FMath::IsNearlyEqual(Result.X, DesiredCenter.X, 0.001f) ||
		FMath::IsNearlyEqual(Result.Y, DesiredCenter.Y, 0.001f));

	const FBox2D BottomPanelBounds(FVector2D(600.0f, 500.0f), FVector2D(1000.0f, 700.0f));
	const TArray<FUiObstacle> BottomPanel = { FUiObstacle(BottomPanelBounds, UiPadding) };
	const FVector2D BottomDesired(700.0f, 650.0f);
	const FVector2D BottomResult = ResolveMarkerCenter(
		BottomDesired, MarkerSize, Viewport, EdgePadding,
		BottomPanel, TConstArrayView<FPlacedMarker>(), AllowedOverlap, bAdjusted);
	TestTrue(TEXT("A screen-edge panel sends the marker to a visible outline edge"),
		UiBoundaryMeasure(BottomResult, BottomPanelBounds, MarkerSize, UiPadding) >= 1.0f - 0.001f);
	TestTrue(TEXT("Screen-edge avoidance also moves along one axis"),
		FMath::IsNearlyEqual(BottomResult.X, BottomDesired.X, 0.001f) ||
		FMath::IsNearlyEqual(BottomResult.Y, BottomDesired.Y, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterMissionMarkerOverlapTest,
	"SimCopter.Missions.MarkerOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterMissionMarkerOverlapTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterMissionMarkerLayout;

	const FVector2D Viewport(1000.0f, 700.0f);
	const FVector2D MarkerSize(100.0f, 60.0f);
	constexpr float EdgePadding = 10.0f;
	constexpr float AllowedOverlap = 0.5f;
	const FVector2D ExistingCenter(500.0f, 350.0f);
	const FBox2D ExistingBounds(
		ExistingCenter - MarkerSize * 0.5f,
		ExistingCenter + MarkerSize * 0.5f);
	const TArray<FPlacedMarker> PlacedMarkers = { FPlacedMarker(ExistingBounds) };

	bool bAdjusted = false;
	FVector2D Result = ResolveMarkerCenter(
		FVector2D(400.0f, 300.0f), MarkerSize, Viewport,
		EdgePadding, TConstArrayView<FUiObstacle>(), TConstArrayView<FPlacedMarker>(),
		AllowedOverlap, bAdjusted);
	TestEqual(TEXT("A marker with no neighbours is unchanged"), Result, FVector2D(400.0f, 300.0f));
	TestFalse(TEXT("An unchanged marker is not marked adjusted"), bAdjusted);

	Result = ResolveMarkerCenter(
		ExistingCenter, MarkerSize, Viewport, EdgePadding,
		TConstArrayView<FUiObstacle>(), PlacedMarkers, AllowedOverlap, bAdjusted);
	const FVector2D Delta = Result - ExistingCenter;
	const bool bHasHorizontalClearance = FMath::IsNearlyEqual(FMath::Abs(Delta.X), MarkerSize.X * 0.5f, 0.001f);
	const bool bHasVerticalClearance = FMath::IsNearlyEqual(FMath::Abs(Delta.Y), MarkerSize.Y * 0.5f, 0.001f);
	TestTrue(TEXT("Coincident markers are separated"), bAdjusted);
	TestTrue(TEXT("Markers may retain exactly fifty-percent overlap on one axis"),
		bHasHorizontalClearance || bHasVerticalClearance);
	TestTrue(TEXT("Separation moves along one screen axis instead of a radial projection"),
		FMath::IsNearlyZero(Delta.X, 0.001f) || FMath::IsNearlyZero(Delta.Y, 0.001f));

	const FVector2D FiftyPercentHorizontalOverlap(
		ExistingCenter.X + MarkerSize.X * 0.5f,
		ExistingCenter.Y);
	Result = ResolveMarkerCenter(
		FiftyPercentHorizontalOverlap, MarkerSize, Viewport, EdgePadding,
		TConstArrayView<FUiObstacle>(), PlacedMarkers, AllowedOverlap, bAdjusted);
	TestEqual(TEXT("An existing fifty-percent overlap is allowed"), Result, FiftyPercentHorizontalOverlap);
	TestFalse(TEXT("An allowed overlap is not adjusted"), bAdjusted);

	Result = ResolveMarkerCenter(
		FVector2D(-100.0f, 900.0f), MarkerSize, Viewport,
		EdgePadding, TConstArrayView<FUiObstacle>(), TConstArrayView<FPlacedMarker>(),
		AllowedOverlap, bAdjusted);
	TestEqual(TEXT("The viewport edge remains padded"), Result, FVector2D(60.0f, 660.0f));
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCopterMissionMessageVisibilityTest,
	"SimCopter.Missions.MessageLogVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterMissionMessageVisibilityTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Init;
	Init.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	UGameViewportClient* PreviousViewport = GEngine->GameViewport.Get();
	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	Viewport->SetViewportOverlayWidget(nullptr, Overlay);
	GEngine->GameViewport = Viewport;
	ASimCopterMissionSystemActor* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	ON_SCOPE_EXIT
	{
		Missions->RemoveMessageLogWidget();
		World->DestroyWorld(false);
		GEngine->GameViewport = PreviousViewport;
	};

	Missions->RefreshMessageLogWidget();
	if (!TestTrue(TEXT("Real message widget is created"), Missions->MessageLogWidget.IsValid())) return false;
	TestTrue(TEXT("Empty log starts collapsed"), Missions->MessageLogWidget->GetVisibility() == EVisibility::Collapsed);
	for (int32 Cycle = 0; Cycle < 3; ++Cycle)
	{
		// Photo Mode, replay and the city intro all restore through this shared entry point.
		Missions->SetHudHiddenForReplay(true);
		Missions->SetHudHiddenForReplay(false);
		TestTrue(TEXT("Restoring an empty HUD leaves no background square"), Missions->MessageLogWidget->GetVisibility() == EVisibility::Collapsed);
	}

	Missions->PushMissionLogMessage(TEXT("Visibility regression message"), FLinearColor::White);
	TestTrue(TEXT("An active message is visible without intercepting input"), Missions->MessageLogWidget->GetVisibility() == EVisibility::SelfHitTestInvisible);
	Missions->SetHudHiddenForReplay(true);
	TestTrue(TEXT("Hide HUD suppresses an active message"), Missions->MessageLogWidget->GetVisibility() == EVisibility::Collapsed);
	Missions->PushMissionLogMessage(TEXT("Message received while HUD is hidden"), FLinearColor::White);
	TestTrue(TEXT("New messages do not unhide the HUD"), Missions->MessageLogWidget->GetVisibility() == EVisibility::Collapsed);
	Missions->SetHudHiddenForReplay(false);
	TestTrue(TEXT("Restoring the HUD shows retained messages without intercepting input"), Missions->MessageLogWidget->GetVisibility() == EVisibility::SelfHitTestInvisible);
	Missions->ClearMissionLogMessage(TEXT("Visibility regression message"));
	TestTrue(TEXT("One remaining message keeps the panel visible"), Missions->MessageLogWidget->GetVisibility().IsVisible());
	Missions->ClearMissionLogMessage(TEXT("Message received while HUD is hidden"));
	TestTrue(TEXT("Removing the final message collapses the panel"), Missions->MessageLogWidget->GetVisibility() == EVisibility::Collapsed);
	Missions->SetHudHiddenForReplay(false);
	TestTrue(TEXT("A later HUD restore cannot resurrect an empty panel"), Missions->MessageLogWidget->GetVisibility() == EVisibility::Collapsed);

	Missions->SetHudHiddenForReplay(true);
	Missions->RemoveMessageLogWidget();
	Missions->PushMissionLogMessage(TEXT("Message after widget recreation"), FLinearColor::White);
	TestTrue(TEXT("Recreating the panel preserves hidden HUD state"), Missions->MessageLogWidget->GetVisibility() == EVisibility::Collapsed);
	Missions->SetHudHiddenForReplay(false);
	TestTrue(TEXT("Recreated panel shows its message when restored"), Missions->MessageLogWidget->GetVisibility() == EVisibility::SelfHitTestInvisible);
	return true;
}
#endif

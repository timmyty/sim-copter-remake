#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Crc.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "UI/SSimCopterHangarMenu.h"
#include "UI/SimCopterEquipmentPreview.h"
#include "UI/SimCopterHangarArt.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Input/HittestGrid.h"
#include "Types/PaintArgs.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Input/SButton.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterEquipmentPageTest,"SimCopter.Hangar.EquipmentPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterEquipmentPageTest::RunTest(const FString&)
{
	using namespace SimCopterEquipmentPreview;
	for (int32 Type=0; Type<9; ++Type)
	{
		const auto Bare=Build(Type,0,INDEX_NONE);
		if (!TestTrue(TEXT("Every catalog helicopter has original geometry"),!Bare.Mesh.IsEmpty())) return false;
		TArray<uint32> Shapes;
		for (int32 Row=0; Row<SimCopterHangarShop::UpgradeRowCount; ++Row)
		{
			const auto Fitted=Build(Type,0,Row);
			TestTrue(TEXT("Unpurchased selection adds visible fitting geometry"),Fitted.Mesh.Triangles.Num()>Bare.Mesh.Triangles.Num() && Fitted.HighlightTriangleCount>0);
			TestTrue(TEXT("Preview has finite bounds"),Fitted.Bounds.IsValid && !Fitted.Bounds.Min.ContainsNaN() && !Fitted.Bounds.Max.ContainsNaN());
			Shapes.AddUnique(FCrc::MemCrc32(Fitted.Mesh.Vertices.GetData(),Fitted.Mesh.Vertices.Num()*sizeof(FVector)));
			if (Row==4) TestTrue(TEXT("Harness hangs below the helicopter ready to deploy"),Fitted.Bounds.Min.Z<Bare.Bounds.Min.Z);
		}
		TestEqual(TEXT("All attachments have distinct geometry"),Shapes.Num(),SimCopterHangarShop::UpgradeRowCount);
	}
	TestTrue(TEXT("Missing model fails gracefully"),!Build(INDEX_NONE,0,0).Error.IsEmpty());
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	SimCopterHangarShop::FContext Shop; Shop.Helicopter=Heli; Shop.Career=Instance->GetSubsystem<USimCopterCareerSubsystem>();
	auto* Art=NewObject<USimCopterHangarArt>(Instance);
	Art->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	auto Menu=SNew(SSimCopterHangarMenu).Shop(Shop).Art(Art);
	Menu->ShowPage(SSimCopterHangarMenu::EPage::Catalog); Menu->HandleSelectCatalogRow(INDEX_NONE);
	const int32 Active=Heli->GetHelicopterTypeIndex(), Mask=Menu->GetOwnedEquipmentMask();
	TestEqual(TEXT("Equipment starts with the active aircraft"),Menu->EquipmentPreviewType,Active);
	Menu->EquipmentPreviewType=4;
	auto Key=[&](FKey K){Menu->OnPreviewKeyDown(FGeometry(),FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
	Key(EKeys::Left); TestEqual(TEXT("Left wraps to last revealed helicopter"),Menu->EquipmentPreviewType,7);
	Key(EKeys::Right); TestEqual(TEXT("Right wraps to first helicopter"),Menu->EquipmentPreviewType,4);
	Key(EKeys::Right); TestEqual(TEXT("Right follows catalog order"),Menu->EquipmentPreviewType,0);
	Key(EKeys::Gamepad_LeftShoulder); TestEqual(TEXT("Controller bumper browses helicopters"),Menu->EquipmentPreviewType,4);
	Key(EKeys::Up); TestEqual(TEXT("Equipment selection wraps upward"),Menu->UpgradeRow,SimCopterHangarShop::UpgradeRowCount-1);
	Key(EKeys::Down); TestEqual(TEXT("Equipment selection wraps downward"),Menu->UpgradeRow,0);
	TestEqual(TEXT("Browsing never changes the active aircraft"),Heli->GetHelicopterTypeIndex(),Active);
	TestEqual(TEXT("Preview never grants equipment"),Menu->GetOwnedEquipmentMask(),Mask);
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,true);
	TestTrue(TEXT("Ownership driving icon checks updates immediately"),(Menu->GetOwnedEquipmentMask() & 4)!=0);
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,false);
	TestTrue(TEXT("Selling removes the icon check"),(Menu->GetOwnedEquipmentMask() & 4)==0);
	if (!GUsingNullRHI)
	{
		Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::WaterBucket,true);
		Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,true);
		for (int32 Row=0; Row<SimCopterHangarShop::UpgradeRowCount; ++Row)
		{
			Menu->HandleSelectUpgradeRow(Row);
			FWidgetRenderer Renderer(false); const FVector2D Size(1280,960);
			Renderer.DrawWidget(Menu,Size); auto* Target=Renderer.DrawWidget(Menu,Size); FlushRenderingCommands();
			if (!TestNotNull(TEXT("Equipment page renders offscreen"),Target)) return false;
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
			TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1280,960,Pixels,Png);
			TestTrue(TEXT("Save equipment page evidence"),FFileHelper::SaveArrayToFile(Png,*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/equipment-preview")/FString::Printf(TEXT("equipment-%d.png"),Row))));
		}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterEquipmentInteractionTest,"SimCopter.Hangar.EquipmentInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterEquipmentInteractionTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->AddSessionCash(10000-Missions->GetSessionCash());
	SimCopterHangarShop::FContext Shop; Shop.Helicopter=Heli; Shop.Missions=Missions;
	Shop.Career=Instance->GetSubsystem<USimCopterCareerSubsystem>();
	auto Menu=SNew(SSimCopterHangarMenu).Shop(Shop);
	Menu->ShowPage(SSimCopterHangarMenu::EPage::Catalog); Menu->HandleSelectCatalogRow(INDEX_NONE);
	auto Window=SNew(SVirtualWindow).Size(FVector2D(640,480)); Window->SetContent(Menu);
	auto Arrange=[&]()
	{
		Window->SlatePrepass(); FHittestGrid Grid; Grid.SetHittestArea(FVector2D::ZeroVector,FVector2D(640,480));
		FSlateWindowElementList Elements(Window); FPaintArgs Args(nullptr,Grid,FVector2D::ZeroVector,0,0);
		Window->Paint(Args,FGeometry::MakeRoot(FVector2D(640,480),FSlateLayoutTransform()),
			FSlateRect(0,0,640,480),Elements,0,FWidgetStyle(),true);
	};
	auto Key=[&](FKey K,bool Repeat=false)
	{ Menu->OnPreviewKeyDown(Menu->GetCachedGeometry(),FKeyEvent(K,FModifierKeysState(),0,Repeat,0,0)); };
	Arrange();
	StaticCastSharedPtr<SButton>(Menu->ControllerFocusableWidgets[5])->SimulateClick(); // Cannon row.
	Menu->ControllerFocusableWidgets[6]->OnMouseEnter(FGeometry(),FPointerEvent()); // Cross harness en route to Buy.
	TestEqual(TEXT("Crossing another item cannot replace the clicked Water Cannon"),Menu->UpgradeRow,3);
	Menu->HandleSelectUpgradeRow(3);
	Menu->SelectedControl=Menu->ControllerFocusableWidgets[5];
	Key(EKeys::Gamepad_FaceButton_Bottom);
	TestTrue(TEXT("A on equipment moves to Buy"),Menu->SelectedControl.Pin()==Menu->ControllerFocusableWidgets[2+SimCopterHangarShop::UpgradeRowCount]);
	TestEqual(TEXT("Selecting the purchase controls does not spend funds"),Missions->GetSessionCash(),10000);
	Key(EKeys::Gamepad_FaceButton_Bottom,true);
	TestEqual(TEXT("A repeat cannot purchase"),Missions->GetSessionCash(),10000);
	Key(EKeys::Gamepad_FaceButton_Bottom); Arrange();
	TestTrue(TEXT("A second press buys the selected cannon"),Heli->IsToolAvailable(ESimCopterHelicopterTool::WaterCannon));
	TestFalse(TEXT("Crossed harness was never bought"),Heli->GetEquipmentState().HasCareerBit(4));
	TestEqual(TEXT("Purchase charges exactly once"),Missions->GetSessionCash(),8500);
	Menu->SelectedControl=Menu->ControllerFocusableWidgets[5];
	Key(EKeys::Gamepad_FaceButton_Bottom);
	TestTrue(TEXT("A on owned equipment moves to Sell"),Menu->SelectedControl.Pin()==Menu->ControllerFocusableWidgets[3+SimCopterHangarShop::UpgradeRowCount]);
	Key(EKeys::Gamepad_FaceButton_Right);
	if (!TestTrue(TEXT("B from the action row stays in equipment"),Menu->Page==SSimCopterHangarMenu::EPage::Catalog && Menu->CatalogRow==INDEX_NONE)) return false;
	TestTrue(TEXT("B returns to the selected equipment item"),Menu->SelectedControl.Pin()==Menu->ControllerFocusableWidgets[5]);
	Missions->AddSessionCash(-Missions->GetSessionCash());
	Menu->HandleSelectUpgradeRow(4); Menu->SelectedControl=Menu->ControllerFocusableWidgets[6];
	Key(EKeys::Gamepad_FaceButton_Bottom);
	TestTrue(TEXT("A with no available transaction moves to Done"),Menu->SelectedControl.Pin()==Menu->ControllerFocusableWidgets[4+SimCopterHangarShop::UpgradeRowCount]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCatalogNavigationTest,"SimCopter.Hangar.CatalogControllerSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCatalogNavigationTest::RunTest(const FString&)
{
	auto Menu = SNew(SSimCopterHangarMenu);
	Menu->ShowPage(SSimCopterHangarMenu::EPage::Catalog);
	auto Window = SNew(SVirtualWindow).Size(FVector2D(640,480)); Window->SetContent(Menu);
	auto Arrange = [&]()
	{
		Window->SlatePrepass(); FHittestGrid Grid; Grid.SetHittestArea(FVector2D::ZeroVector,FVector2D(640,480));
		FSlateWindowElementList Elements(Window); FPaintArgs Args(nullptr,Grid,FVector2D::ZeroVector,0,0);
		Window->Paint(Args,FGeometry::MakeRoot(FVector2D(640,480),FSlateLayoutTransform()),
			FSlateRect(0,0,640,480),Elements,0,FWidgetStyle(),true);
	};
	auto Key = [&](FKey K)
	{ Menu->OnPreviewKeyDown(Menu->GetCachedGeometry(),FKeyEvent(K,FModifierKeysState(),0,false,0,0)); };
	auto Tab = [&](int32 Row)
	{
		TArray<TSharedPtr<SWidget>> Buttons;
		TFunction<void(TSharedRef<SWidget>)> Visit = [&](TSharedRef<SWidget> Widget)
		{
			if (Widget->GetTypeAsString() == TEXT("SButton")) Buttons.Add(Widget);
			FChildren* Children = Widget->GetChildren();
			for (int32 I=0; I<Children->Num(); ++I) Visit(Children->GetChildAt(I));
		};
		Visit(Menu);
		return Buttons[Buttons.Num()-10+Row]; // Nine aircraft tabs plus Upgrades, after transactions.
	};
	Arrange();
	Menu->SetControllerSelection(Tab(2));
	Key(EKeys::Gamepad_FaceButton_Bottom); Arrange();
	TestEqual(TEXT("Controller opens selected aircraft"),Menu->CatalogRow,2);
	TestTrue(TEXT("Rebuilt card retains controller focus on its aircraft tab"),Menu->GetControllerSelection()==Tab(2));
	Key(EKeys::Gamepad_DPad_Right); Key(EKeys::Gamepad_FaceButton_Bottom); Arrange();
	TestEqual(TEXT("Next aircraft is one controller step away"),Menu->CatalogRow,3);
	Menu->OnNavigation(Menu->GetCachedGeometry(),FNavigationEvent(FModifierKeysState(),0,EUINavigation::Left,ENavigationGenesis::Controller));
	Key(EKeys::Gamepad_FaceButton_Bottom); Arrange();
	TestEqual(TEXT("Stick navigation can return directly to previous aircraft"),Menu->CatalogRow,2);
	for (const FKey K : {EKeys::Gamepad_DPad_Left,EKeys::Gamepad_DPad_Right,EKeys::Left,EKeys::Right})
	{
		for (int32 Row=0;Row<8;++Row)
		{
			Menu->SetControllerSelection(Tab(Row));
			for (int32 Step=0;Step<12;++Step)
			{
				Key(K);
				const int32 Selected=Menu->CatalogTabWidgets.IndexOfByKey(Menu->GetControllerSelection());
				TestTrue(TEXT("Horizontal browsing stays in the eight-aircraft row, including its edges"),Selected>=0 && Selected<8);
			}
		}
	}
	for (EUINavigation Direction : {EUINavigation::Left,EUINavigation::Right})
	{
		Menu->SetControllerSelection(Tab(Direction==EUINavigation::Left ? 0 : 7));
		for (int32 I=0;I<12;++I)
			Menu->OnNavigation(Menu->GetCachedGeometry(),FNavigationEvent(FModifierKeysState(),0,Direction,ENavigationGenesis::Controller));
		TestTrue(TEXT("Analog navigation does not fall into lower links at row ends"),Menu->CatalogTabWidgets.IndexOfByKey(Menu->GetControllerSelection())<8);
	}
	for (const int32 Row : {1,6})
	{
		Menu->SetControllerSelection(Tab(Row)); Key(EKeys::Gamepad_DPad_Down);
		const int32 Lower=Menu->CatalogTabWidgets.IndexOfByKey(Menu->GetControllerSelection());
		TestEqual(TEXT("Down deliberately reaches the appropriate lower link"),Lower,Row==1?9:8);
		Key(EKeys::Gamepad_DPad_Up);
		const int32 Upper=Menu->CatalogTabWidgets.IndexOfByKey(Menu->GetControllerSelection());
		TestTrue(TEXT("Up returns to aircraft tabs"),Upper>=0 && Upper<8);
	}
	return true;
}
#endif

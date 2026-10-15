#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "City/SimCopterDayNight.h"
#include "DaySequenceActor.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Components/DirectionalLightComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Game/SimCopterSettings.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Game/SimCopterCheats.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "UI/SimCopterHangarShop.h"
#include "UI/SSimCopterCockpitFrame.h"
#include "Components/InputComponent.h"
#include "Flight/SimCopterControllerInput.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGameplayOptionsTest, "SimCopter.GameplayOptions.Integration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGameplayOptionsTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	TestFalse(TEXT("New aircraft has no remote taser"), Heli->IsToolAvailable(ESimCopterHelicopterTool::RemoteTaser));
	Heli->ToggleRemoteTaser();
	TestFalse(TEXT("Direct command cannot bypass purchase"), Heli->GetAirOperations()->IsRemoteTaserActive());
	TestEqual(TEXT("Unpurchased taser absent from wheel"), Heli->GetControllerRemoteTaserSlot(), INDEX_NONE);
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(Missions, 47);
	auto* Career = NewObject<USimCopterCareerSubsystem>(NewObject<UGameInstance>());
	Career->BeginCareer(); Heli->ApplyCareerCityTransfer(4, SimCopterHelicopterRegistry::StartingCareerEquipmentBits, 0); Career->EnsurePricesLoaded(SimCopterOriginalGame::ResolveRoot());
	SimCopterHangarShop::FContext Shop; Shop.Helicopter = Heli; Shop.Missions = Missions; Shop.Career = Career;
	const int32 Price = SimCopterHelicopterRegistry::GetEquipmentPrice(ESimCopterHelicopterTool::RemoteTaser);
	Missions->AddSessionCash(Price - 1 - Missions->GetSessionCash());
	FString Message;
	TestFalse(TEXT("Remote taser requires full price"), SimCopterHangarShop::BuyUpgrade(Shop, 6, Message));
	Missions->AddSessionCash(1);
	TestTrue(TEXT("Remote taser purchases through hangar"), SimCopterHangarShop::BuyUpgrade(Shop, 6, Message));
	TestEqual(TEXT("Purchase charged once"), Missions->GetSessionCash(), 0);
	TestTrue(TEXT("Purchase enables remote taser"), Heli->IsToolAvailable(ESimCopterHelicopterTool::RemoteTaser));
	TArray<uint8> SavedAircraft;
	TestTrue(TEXT("Equipment saves"), Heli->CaptureRuntimeSaveState(SavedAircraft));
	auto* Restored = World->SpawnActor<ASimCopterHelicopterPawn>();
	TestTrue(TEXT("Equipment restores"), Restored->RestoreRuntimeSaveState(SavedAircraft));
	TestTrue(TEXT("Purchased taser survives save/load"), Restored->IsToolAvailable(ESimCopterHelicopterTool::RemoteTaser));

	auto* Input = NewObject<UInputComponent>(Heli); Heli->SetupPlayerInputComponent(Input);
	const auto Key = [&](FKey Button, EInputEvent Event) { for (auto& Binding : Input->KeyBindings)
		if (Binding.Chord.Key == Button && Binding.KeyEvent == Event) Binding.KeyDelegate.Execute(Button); };
	Key(EKeys::Gamepad_FaceButton_Left, IE_Pressed);
	TestEqual(TEXT("X does not open radial"), Heli->GetControllerMode(), ESimCopterControllerMode::None);
	TestTrue(TEXT("X preserves camera adjustments"), Heli->IsControllerCameraAdjustHeld());
	Key(EKeys::Gamepad_FaceButton_Left, IE_Released);
	Key(EKeys::Gamepad_RightThumbstick, IE_Pressed);
	const int32 Slot = Heli->ControllerToolWheelTools.Find(ESimCopterHelicopterTool::Megaphone);
	const FVector2D Direction = SimCopterControllerInput::GetRadialSlotDirection(Slot, Heli->GetControllerToolWheelSlotCount());
	Heli->ControllerRightX(Direction.X); Heli->ControllerRightY(Direction.Y); Heli->UpdateControllerRadialSelection();
	Heli->ControllerRightX(0); Heli->ControllerRightY(0);
	Key(EKeys::Gamepad_RightThumbstick, IE_Released);
	TestEqual(TEXT("Release preserves the tool wheel"), Heli->GetControllerMode(), ESimCopterControllerMode::ToolWheel);
	TestEqual(TEXT("Release does not equip the highlighted tool"), Heli->GetActiveTool(), Heli->ControllerToolWheelOriginal);
	Key(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed);
	TestEqual(TEXT("A equips last highlight after recentering"), Heli->GetActiveTool(), ESimCopterHelicopterTool::Megaphone);
	TestEqual(TEXT("A closes the confirmed tool wheel"), Heli->GetControllerMode(), ESimCopterControllerMode::None);
	Key(EKeys::Gamepad_FaceButton_Bottom, IE_Released);
	Key(EKeys::Gamepad_RightThumbstick, IE_Pressed); Heli->CancelControllerRadialForShortcut();
	Key(EKeys::Gamepad_RightThumbstick, IE_Released);
	TestEqual(TEXT("Shortcut cancellation never equips something else"), Heli->GetActiveTool(), ESimCopterHelicopterTool::Megaphone);

	auto* Settings = NewObject<USimCopterSettings>(NewObject<UGameInstance>());
	auto Frame = SNew(SSimCopterCockpitFrame).Settings(Settings);
	Settings->SetCockpitDecorationsVisible(false); Frame->UpdateAllAttributes(); Frame->SlatePrepass();
	TestTrue(TEXT("Preference removes bars and their controls"), Frame->GetVisibility() == EVisibility::Collapsed);
	Settings->SetCockpitDecorationsVisible(true); Frame->UpdateAllAttributes(); Frame->SlatePrepass();
	TestTrue(TEXT("Preference restores decorations"), Frame->GetVisibility() == EVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Daylight windows off"), SimCopterDayNight::NightAlphaForSunElevation(30), 0.f);
	TestTrue(TEXT("Windows turn on at sunset"), SimCopterDayNight::NightAlphaForSunElevation(0) > .5f);
	TestEqual(TEXT("Night windows fully on"), SimCopterDayNight::NightAlphaForSunElevation(-10), 1.f);
	TestTrue(TEXT("Lighting continues to refresh during Photo Mode pause"), World->GetSubsystem<USimCopterDayNightSubsystem>()->IsTickableWhenPaused());
	// Exercise evaluated sun direction, including a frozen world/Photo Mode time seek.
	auto* Sky = World->SpawnActor<ADaySequenceActor>();
	auto* Sun = NewObject<UDirectionalLightComponent>(Sky);
	Sky->AddInstanceComponent(Sun); Sun->RegisterComponent();
	Sun->bAtmosphereSunLight = true; Sun->AtmosphereSunLightIndex = 0;
	auto* Lighting = World->GetSubsystem<USimCopterDayNightSubsystem>();
	Lighting->CachedDaySequenceActor = Sky;
	Sun->SetWorldRotation(FRotator(-30,0,0)); Lighting->Tick(0.f);
	TestEqual(TEXT("Evaluated daytime sun disables building lights"), Lighting->GetNightAlpha(), 0.f);
	World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
	TestTrue(TEXT("Photo lighting fixture is paused"), World->IsPaused());
	Sun->SetWorldRotation(FRotator(10,0,0)); Lighting->Tick(0.f);
	TestEqual(TEXT("Night sun enables building lights while paused"), Lighting->GetNightAlpha(), 1.f);
	const float NightSeed = UKismetMaterialLibrary::GetScalarParameterValue(World, Lighting->CachedParameterCollection, SimCopterDayNight::WindowSeedParameterName);
	Lighting->Tick(0.f);
	TestEqual(TEXT("Paused night does not reroll windows every frame"), UKismetMaterialLibrary::GetScalarParameterValue(World, Lighting->CachedParameterCollection, SimCopterDayNight::WindowSeedParameterName), NightSeed);
	Sun->SetWorldRotation(FRotator(-30,0,0)); Lighting->Tick(0.f);
	TestEqual(TEXT("Seeking back to day disables building lights while paused"), Lighting->GetNightAlpha(), 0.f);
	World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	Missions->SessionMode = ESimCopterMissionSessionMode::UserCityJobs;
	Missions->CustomCityBestScore = 2999;
	TestTrue(TEXT("Apache locked before last user-city stage"), SimCopterHangarShop::GetHelicopterRowState(Shop, 8).bMystery);
	Missions->CustomCityBestScore = 3000;
	TestFalse(TEXT("Apache unlocked by playing own city"), SimCopterHangarShop::GetHelicopterRowState(Shop, 8).bMystery);
	TestEqual(TEXT("Dedicated unlock cheat is parsed"), SimCopterCheats::Parse(TEXT("Unlock Apache"))[0].Code, SimCopterCheats::ECode::UnlockApache);

	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	const FString Heven = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../../SimCopter Remake v1.0.1/Windows/SimCopter/cities/heven (new).sc2"));
	if (IFileManager::Get().FileExists(*Heven)) Traffic->CityFile.FilePath = Heven;
	if (!TestTrue(TEXT("Actual city spawn data loads"), Traffic->RebuildSpawnData())) return false;
	// Police/medics stationed across a user city must not consume its civilian budget.
	for (int32 I = 0; I < Traffic->OriginalAmbientRandomCap; ++I)
	{
		auto* Officer = World->SpawnActor<ASimCopterGroundAgent>(); Officer->InitialPersonState = 7;
		Officer->SetInitialBehaviorClass(14); Traffic->PedestrianAgents.Add(Officer);
	}
	TestEqual(TEXT("Posted officers do not starve civilian population"), Traffic->CountAmbientPedestrians(), 0);
	TSet<FString> Figures;
	for (const auto& Node : Traffic->PedestrianNodes)
	{
		if (Figures.Num() >= 6) break;
		const int32 Before = Traffic->PedestrianAgents.Num();
		int32 Budget = 1; Traffic->TryRunAmbientTileSpawn(Node.FileX, Node.FileY, 1, Budget);
		for (int32 I = Before; I < Traffic->PedestrianAgents.Num(); ++I)
			if (Traffic->PedestrianAgents[I]->InitialPersonState == 0) Figures.Add(Traffic->PedestrianAgents[I]->GetPedestrianFigureName());
	}
	TestTrue(TEXT("Heven spawns a varied civilian crowd alongside police"), Figures.Num() >= 6);
	AddInfo(FString::Printf(TEXT("City %s: %d distinct civilian figures"), *Traffic->CityFile.FilePath, Figures.Num()));

	// The fire record owns debris on an ordinary road. Water must clear it and its marker.
	Missions->MissionSystem.Initialize(Missions, 47);
	// A synthetic active fire record isolates debris accounting from traffic's random car choice.
	SimCopterMissions::FSimCopterMissionRecord* Record = nullptr;
	for (const auto& Candidate : Missions->MissionSystem.GetRecords()) if (!Candidate.bActive)
	{ Record = const_cast<SimCopterMissions::FSimCopterMissionRecord*>(&Candidate); break; }
	if (!TestNotNull(TEXT("Fire record slot"), Record)) return false;
	Record->bActive = true; Record->EventId = 9001; Record->TypeMask = SimCopterMissions::TYPE_Debris;
	Record->Name = TEXT("Road debris"); Record->TileX = 20; Record->TileY = 20;
	FVector Road; Traffic->TryGetTileCenterWorldLocation(20, 20, Road);
	TestTrue(TEXT("Road debris starts"), Missions->AddBurningDebrisSlot(Road, FIntPoint(20,20), 9001));
	TArray<FSimCopterMissionWorldMarkerEntry> Markers;
	Missions->BuildMissionWorldMarkers(Markers);
	TestTrue(TEXT("Active debris has marker"), Markers.ContainsByPredicate([](const auto& M) { return M.Detail == TEXT("Road debris"); }));
	TestTrue(TEXT("Water reaches road debris"), Missions->ApplyWaterParticleImpact(Road, 65536) > 0);
	TestEqual(TEXT("Doused debris removed"), Missions->GetBurningDebrisCount(), 0);
	TestEqual(TEXT("Douse counter credited exactly once"), Record->DebrisDoused, 1);
	Missions->ApplyWaterParticleImpact(Road, 65536);
	TestEqual(TEXT("Repeated water cannot score cleared debris"), Record->DebrisDoused, 1);
	Missions->BuildMissionWorldMarkers(Markers);
	TestFalse(TEXT("No phantom marker after fire clears"), Markers.ContainsByPredicate([](const auto& M) { return M.Detail == TEXT("Road debris"); }));
	return true;
}
#endif

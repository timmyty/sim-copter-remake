#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Game/SimCopterSettings.h"
#include "Game/SimCopterSaveSubsystem.h"
#include "Game/SimCopterMainMenuGameMode.h"
#include "Game/SimCopterPlayerController.h"
#include "City/SimCopterHangar.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterControllerInput.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Missions/SimCopterWitnessBriefing.h"
#include "Ground/SimCopterGroundAgent.h"
#include "UI/SimCopterHangarArt.h"
#include "UI/SimCopterHangarShop.h"
#include "UI/SimCopterControllerHelp.h"
#include "UI/SSimCopterGraphicsSettings.h"
#include "UI/SSimCopterMainMenu.h"
#include "UI/SSimCopterUserCityPicker.h"
#include "UI/SSimCopterToolFlaps.h"
#include "UI/SSimCopterHangarMenu.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProceduralMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Widgets/SOverlay.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterRemakeModeTest,"SimCopter.RemakeMode",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FSimCopterRemakeModeTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(const TCHAR* Name:{TEXT("PreferenceLifecycle"),TEXT("DisabledSystems"),TEXT("SaveModes"),TEXT("CheatAndRepeat"),TEXT("MenusRendering")})
 { Names.Add(Name); Commands.Add(Name); }
}
bool FSimCopterRemakeModeTest::RunTest(const FString& Case)
{
 if(Case==TEXT("MenusRendering") && GUsingNullRHI) { AddInfo(TEXT("Menu pixels require the separate GPU run.")); return true; }
 auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
 auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
 auto* Settings=Instance->GetSubsystem<USimCopterSettings>();
 const bool OldPreference=Settings->bRemakeFeatures, OldActive=Settings->bActiveRemakeFeatures;
 auto* PreviousViewport=GEngine->GameViewport.Get();
 auto* Viewport=NewObject<UGameViewportClient>(GEngine);
 Viewport->Init(GEngine->GetWorldContextFromWorldChecked(World),Instance,false);
 Viewport->SetViewportOverlayWidget(nullptr,SNew(SOverlay));
 GEngine->GetWorldContextFromWorldChecked(World).GameViewport=Viewport; GEngine->GameViewport=Viewport;
 ON_SCOPE_EXIT {
  Settings->bRemakeFeatures=OldPreference; Settings->bActiveRemakeFeatures=OldActive;
  if(Case==TEXT("PreferenceLifecycle")) Settings->SaveConfig();
  GEngine->GameViewport=PreviousViewport;
  Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
 };
 Settings->bRemakeFeatures=Settings->bActiveRemakeFeatures=true;
 if(Case==TEXT("PreferenceLifecycle"))
 {
  Settings->SetRemakeFeaturesPreference(false); Settings->ApplyRemakeFeaturesForSession();
  TestFalse(TEXT("Front-end choice applies before choosing a city"),Settings->AreRemakeFeaturesEnabled());
  auto Panel=SNew(SSimCopterGraphicsSettings);
  Settings->SetRemakeFeaturesPreference(true);
  TestTrue(TEXT("In-city change waits for next session"),Settings->HasPendingRemakeFeaturesChange());
  TestFalse(TEXT("Running world keeps its active policy"),USimCopterSettings::RemakeFeaturesEnabled(World));
  Panel->Cancel();
  TestFalse(TEXT("Cancel restores the pending choice"),Settings->WantsRemakeFeatures());
  Settings->SetRemakeFeaturesPreference(true); Settings->SaveConfig();
  auto* Restart=NewObject<UGameInstance>(GEngine); Restart->InitializeStandalone();
  auto* RestartWorld=Restart->GetWorld();
  auto* Loaded=Restart->GetSubsystem<USimCopterSettings>();
  TestTrue(TEXT("Restart activates the saved choice"),Loaded->AreRemakeFeaturesEnabled());
  TestTrue(TEXT("Saved preference is shared by the next toggle screen"),Loaded->WantsRemakeFeatures());
  Restart->Shutdown(); GEngine->DestroyWorldContext(RestartWorld); RestartWorld->DestroyWorld(false);
  auto* Menu=World->SpawnActor<ASimCopterMainMenuGameMode>();
  const ESimCopterMainMenuItem Items[]={ESimCopterMainMenuItem::NewCareerGame,ESimCopterMainMenuItem::OpenCareerGame,ESimCopterMainMenuItem::NewUserGame,ESimCopterMainMenuItem::OpenUserGame};
  const ESimCopterFrontEndScreen Targets[]={ESimCopterFrontEndScreen::CareerSelect,ESimCopterFrontEndScreen::SavedGamePicker,ESimCopterFrontEndScreen::UserCityPicker,ESimCopterFrontEndScreen::SavedGamePicker};
  for (int32 Visit=0; Visit<8; ++Visit)
  {
   Menu->HandleMainMenuItem(Items[Visit%4]);
   TestEqual(TEXT("Every New/Open visit shows toggle, including returning players"),Menu->Screen,ESimCopterFrontEndScreen::RemakeFeatures);
   const bool Last=Settings->WantsRemakeFeatures();
   Menu->EnterScreen(ESimCopterFrontEndScreen::MainMenu);
   TestEqual(TEXT("Back preserves last toggle position"),Settings->WantsRemakeFeatures(),Last);
   Menu->HandleMainMenuItem(Items[Visit%4]);
   Settings->SetRemakeFeaturesPreference(Visit%2==0);
   Menu->ContinueGameSelection();
   TestEqual(TEXT("Continue reaches the selected game's picker"),Menu->Screen,Targets[Visit%4]);
   TestEqual(TEXT("Continue activates latest preference without restarting"),Settings->AreRemakeFeaturesEnabled(),Visit%2==0);
   if (Visit%4==1 || Visit%4==3) TestEqual(TEXT("Open keeps career/user save filter"),Menu->PendingSaveKind,Visit%4==1 ? ESimCopterSessionKind::Career : ESimCopterSessionKind::User);
  }
  Menu->CloseScreen();
  return true;
 }
 if(Case==TEXT("SaveModes"))
 {
  auto* Save=NewObject<USimCopterSaveGame>(); Save->Kind=ESimCopterSessionKind::Career;
  Save->DisplayName=TEXT("Mode regression"); Save->CareerCityIndex=0; Save->CityWeights.Init(1,7); Save->bHasTimeOfDayState=true;
  TestTrue(TEXT("Pre-choice saves default to the full fork"),Save->bRemakeFeatures);
  const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/remake-features-v121/mode-test.sav"));
  ON_SCOPE_EXIT { IFileManager::Get().Delete(*File); };
  auto* Saves=Instance->GetSubsystem<USimCopterSaveSubsystem>();
  for(bool Enabled:{false,true})
  {
   Save->bRemakeFeatures=Enabled; TArray<uint8> Bytes; UGameplayStatics::SaveGameToMemory(Save,Bytes); FFileHelper::SaveArrayToFile(Bytes,*File);
   Settings->bActiveRemakeFeatures=!Enabled; FString Error;
   TestFalse(TEXT("Opposite mode is rejected before session changes"),Saves->LoadGame(File,ESimCopterSessionKind::Career,Error));
   TestTrue(TEXT("Mismatch explains how to return to toggle"),Error.Contains(TEXT("Extra Remake Features")) && Error.Contains(TEXT("Main Menu")));
   TestFalse(TEXT("Rejected load leaves no pending world restore"),Saves->HasPendingLoad());
   Settings->bActiveRemakeFeatures=Enabled;
   TestTrue(TEXT("Matching mode loads"),Saves->LoadGame(File,ESimCopterSessionKind::Career,Error));
   Saves->BeginNewGame();
  }
  return true;
 }
 auto* Art=NewObject<USimCopterHangarArt>(Instance);
 Art->SetOriginalGameRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame")));
 if(Case==TEXT("MenusRendering"))
 {
  const FString Folder=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/session-toggle-clouds/previews"); IFileManager::Get().MakeDirectory(*Folder,true);
  auto Capture=[&](TSharedRef<SWidget> Widget,const TCHAR* Name)
  {
   FWidgetRenderer Renderer(true); UTextureRenderTarget2D* Target=nullptr;
   for(int32 Frame=0;Frame<3;++Frame) { Target=Renderer.DrawWidget(Widget,FVector2D(1280,960)); FlushRenderingCommands(); }
   if(!TestNotNull(TEXT("Menu render target"),Target)) return;
   TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false); Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
   TestTrue(TEXT("Actual menu screenshot exports"),FImageUtils::SaveImageByExtension(*(Folder/Name),FImageView(Pixels.GetData(),1280,960)));
  };
  auto* Menu=World->SpawnActor<ASimCopterMainMenuGameMode>(); Menu->Art=Art;
  Capture(Menu->BuildScreen(ESimCopterFrontEndScreen::RemakeFeatures),TEXT("session-options-on.png"));
  Settings->SetRemakeFeaturesPreference(false);
  Capture(Menu->BuildScreen(ESimCopterFrontEndScreen::RemakeFeatures),TEXT("session-options-off.png"));
  Capture(SNew(SSimCopterMainMenu).Art(Art),TEXT("main-menu.png"));
  Capture(SNew(SSimCopterGraphicsSettings).Art(Art),TEXT("gameplay-options-updated.png"));
  Settings->SetUpdatedEmergencyArtwork(false);
  Capture(SNew(SSimCopterGraphicsSettings).Art(Art),TEXT("gameplay-options-previous.png"));
  Settings->bActiveRemakeFeatures=false;
  auto* PreviewHeli=World->SpawnActor<ASimCopterHelicopterPawn>();
  SimCopterHangarShop::FContext Shop; Shop.Helicopter=PreviewHeli;
  auto Catalog=SNew(SSimCopterHangarMenu).Art(Art).Shop(Shop);
  Catalog->ShowPage(SSimCopterHangarMenu::EPage::Catalog); Catalog->HandleSelectCatalogRow(INDEX_NONE);
  Catalog->HandleSelectUpgradeRow(6);
  TestEqual(TEXT("Earlier catalog limits navigation to the original five items"),Catalog->UpgradeRow,4);
  Capture(Catalog,TEXT("original-style-equipment.png"));
  return true;
 }
 auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
 if(Case==TEXT("DisabledSystems"))
 {
  Settings->bActiveRemakeFeatures=false;
  auto CityPicker=SNew(SSimCopterUserCityPicker).Art(Art).AllowExtraControls(false);
  TestTrue(TEXT("No added city-download links"),CityPicker->WebsiteLinks.IsEmpty());
  CityPicker->RequestWebsite(0);
  TestEqual(TEXT("Disabled city links cannot open a website prompt"),CityPicker->PendingWebsiteIndex,INDEX_NONE);
  CityPicker->Browse(); // Must return before invoking a native dialog in unattended tests.
  auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
  Pilot->StartTaserAim(); TestFalse(TEXT("No handheld taser"),Pilot->IsTaserAiming());
  Pilot->SetParachuteDeployed(true); TestFalse(TEXT("No parachute"),Pilot->IsParachuteDeployed());
  TestFalse(TEXT("No expanded automatic carry"),Pilot->TryCarryDownedPerson());
  auto* Ops=Heli->GetAirOperations(); Ops->BeginPilotRecovery(Pilot); TestFalse(TEXT("No automatic pilot retrieval"),Ops->IsRecoveringPilot());
  for(auto Tool:{ESimCopterHelicopterTool::CaptureCage,ESimCopterHelicopterTool::RemoteTaser})
  { Heli->SetCareerEquipmentOwned(Tool,true); TestFalse(TEXT("Disabled tool cannot be selected even if its bit is owned"),Heli->IsToolAvailable(Tool)); }
  auto Flaps=SNew(SSimCopterToolFlaps).Pawn(Heli);
  TestTrue(TEXT("Disabled cage panel is absent"),Flaps->GetFlapVisibility(SimCopterHelicopterRegistry::GetToolCareerBit(ESimCopterHelicopterTool::CaptureCage))==EVisibility::Collapsed);
  TestEqual(TEXT("Classic dispatch retains Recall All"),SimCopterControllerInput::GetDispatchSelection(4,false).ServiceIndex,SimCopterControllerInput::RecallAllServiceIndex);
  auto* Air=World->GetSubsystem<USimCopterAirOperationsSubsystem>(); Air->EnsureSites(); Air->Tick(200);
  TestTrue(TEXT("No recovery-site props"),Air->GetSites().IsEmpty()); TestFalse(TEXT("No helper unlock"),Air->IsSupportUnlocked());
  TestFalse(TEXT("No helper order"),Air->OrderSupport(0)); TestNull(TEXT("No helper aircraft"),Air->GetSupportHelicopter());
  TestFalse(TEXT("Flyable plane cannot be selected"),Heli->SwitchHelicopterModel(SimCopterHelicopterRegistry::PlaneTypeIndex));
  auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>(); Missions->SessionMode=ESimCopterMissionSessionMode::UserCityJobs;
  TestTrue(TEXT("Full-map placement fixes remain"),Missions->UsesCitywideMissionPlacement());
  TestFalse(TEXT("Score/equipment progression is disabled"),Missions->UsesCustomCityProgression());
  TestTrue(TEXT("Original mission types are not locked by progression"),Missions->IsMissionTypeAvailable(SimCopterMissions::TYPE_Riot));
  Missions->MissionSystem.Initialize(Missions,42);
  for(int32 Type:{SimCopterMissions::TYPE_VehicleTow,SimCopterMissions::TYPE_BoatTow})
  {
   TestFalse(TEXT("Recovery types are unavailable"),Missions->IsMissionTypeAvailable(Type));
   TestEqual(TEXT("Direct incident creation cannot bypass disabled recovery"),Missions->CreateMissionAt(30,30,Type),INDEX_NONE);
  }
  Missions->SetCustomCityOptions(3,4); TestEqual(TEXT("Additional frequency control has no effect"),Missions->GetCustomMissionFrequency(),1.f);
  TestEqual(TEXT("Original party rules remain"),Missions->GetMissionPassengerLimit(true),MAX_int32);
  auto* Suspect=World->SpawnActor<ASimCopterGroundAgent>();
  auto* Witness=NewObject<USimCopterWitnessBriefing>(Missions); Witness->RegisterComponent(); Witness->Report(Suspect);
  TestTrue(TEXT("No photograph or spoken report is queued"),Witness->Pending.IsEmpty());
  TestFalse(TEXT("No extra incident rescue is generated"),Missions->CreateIncidentMedevacForVictim(Suspect));
  auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>(); int32 Attempts=20;
  TestEqual(TEXT("No full-game baseball roster"),Traffic->EnsureBaseballGame(60,60,Attempts),0);
  TestEqual(TEXT("Disabled baseball consumes no spawn budget"),Attempts,20);
  auto* Hangar=World->SpawnActor<ASimCopterHangar>(); Hangar->BuildShellMesh();
  TestEqual(TEXT("Earlier hangar has no reconstructed interior"),Hangar->InteriorMesh->GetNumSections(),0);
  TestEqual(TEXT("Earlier shell retains its four textured sections"),Hangar->ShellMesh->GetNumSections(),4);
  Settings->bActiveRemakeFeatures=true; Hangar->BuildShellMesh();
  TestTrue(TEXT("Yes reconstructs the hangar interior"),Hangar->InteriorMesh->GetNumSections()>0);
  SimCopterControllerHelp::FState Help; Help.bExpanded=true; Help.Context=SimCopterControllerHelp::EContext::OnFoot; Help.bRemakeFeatures=false;
  for(bool Keyboard:{false,true}) { Help.bKeyboard=Keyboard; for(const auto& Row:SimCopterControllerHelp::BuildPrompts(Help)) TestFalse(TEXT("Help omits disabled controls"),Row.Label.Contains(TEXT("taser")) || Row.Label.Contains(TEXT("parachute"))); }
  return true;
 }
 // Execute the actual parser/controller cheat, interrupting repeated recovery attempts.
 auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
 PC->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>(); PC->PlayerCameraManager->InitializeFor(PC);
 auto* Ground=World->SpawnActor<AActor>(); auto* Floor=NewObject<UBoxComponent>(Ground); Ground->SetRootComponent(Floor);
 Floor->SetBoxExtent(FVector(10000,10000,20)); Floor->SetCollisionResponseToAllChannels(ECR_Block); Floor->RegisterComponent(); Ground->SetActorLocation(FVector(0,0,-20));
 Heli->SwitchHelicopterModel(4); PC->Possess(Heli); auto* Ops=Heli->GetAirOperations();
 for(int32 Attempt=0;Attempt<3;++Attempt)
 {
  Heli->SetAutopilotTransform(FVector(800,500,1000),FRotator::ZeroRotator,.05f); Heli->ExitHelicopter();
  auto* Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn()); if(!TestNotNull(TEXT("Exit creates pilot"),Pilot)) return false;
  Pilot->SetActorLocation(FVector(1500,500,94)); Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  const FVector Before=Heli->GetActorLocation(); for(int32 I=0;I<60;++I) Ops->Update(.1f);
  TestTrue(TEXT("Retrieval moves on every exit after the cheat"),FVector::Distance(Before,Heli->GetActorLocation())>100);
  const FString Result=PC->ExecuteCheatCodes(TEXT("I love my helicopter"));
  TestEqual(*Result,PC->GetPawn().Get(),static_cast<APawn*>(Heli));
  TestFalse(TEXT("Cheat ends the previous recovery"),Ops->IsRecoveringPilot());
  TestTrue(TEXT("Cheat preserves the user's autopilot preference"),Ops->IsAutopilotEnabled());
 }
 return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterControllerInput.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterApachePool.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "City/SimCity2000CityActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterPeopleReader.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterAirOperationsTest,"SimCopter.AirOperations.RecoveryCustodyControlsAndPersistence",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterAirOperationsTest::RunTest(const FString& Parameters)
{
 using namespace SimCopterMissions;
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
 City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->FinishSpawning(FTransform::Identity);
 City->WaterGameplayCornerZ.Init(0,129*129); City->WaterGameplayTerrainClasses.Init(10,128*128);
 auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
 Traffic->SourceCityActor=City;
 Traffic->PeopleTileClasses.Init(7,FSimCity2000City::TileCount);
 Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
 Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount);
 Traffic->XbldTileIds.Init(0,FSimCity2000City::TileCount);
 Traffic->ActiveTileSize=400;
 auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
 Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
 auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
 TestTrue(TEXT("Schweizer mesh and two-seat cabin load"),Heli->SwitchHelicopterModel(4));
 Heli->bEngineRunning=true;
 auto* Ops=Heli->GetAirOperations();
 auto* System=World->GetSubsystem<USimCopterAirOperationsSubsystem>();
 TestNotNull(TEXT("Recovery world subsystem exists"),System);

 // An available helper must not claim work until the player orders a mission.
 const int32 UnorderedJob=Missions->MissionSystem.CreateIncidentMedevacAt(20,20);
 TestTrue(TEXT("Standby fixture has an active emergency"),UnorderedJob!=INDEX_NONE);
 System->SupportHelicopter=Heli;
 System->SupportHome=Heli->GetActorLocation();
 System->UpdateSupport(0.05f);
 TestEqual(TEXT("Available helper leaves emergencies unassigned without an order"),System->AssignedEvent,INDEX_NONE);

 // Read the pre-change byte layout: older saves enabled automatic work by default.
 TArray<uint8> LegacySupport;
 {
  FMemoryWriter Writer(LegacySupport,true);
  bool Automatic=true, Returning=false;
  int32 Event=UnorderedJob;
  float Work=1, Attempt=30, Stalled=75;
  FVector Home(3000,3000,2200);
  Writer << Automatic << Returning << Event << Work << Attempt << Stalled << Home;
 }
 {
  FMemoryReader Reader(LegacySupport,true); System->SerializeState(Reader);
  TestFalse(TEXT("Legacy automatic support save remains readable"),Reader.IsError());
  TestEqual(TEXT("Legacy support reader consumes the unchanged layout"),Reader.Tell(),Reader.TotalSize());
 }
 TestEqual(TEXT("Loading legacy automatic work discards its unrequested assignment"),System->AssignedEvent,INDEX_NONE);
 System->SupportHelicopter=Heli;
 Heli->SetActorLocation(System->SupportHome);
 System->UpdateSupport(0.05f); System->UpdateSupport(0.05f);
 TestEqual(TEXT("Legacy helper stays idle after reaching base"),System->AssignedEvent,INDEX_NONE);
 TestEqual(TEXT("Legacy migration preserves the unrelated breakdown countdown"),System->StalledCarSeconds,75.0f);
 System->SupportHelicopter.Reset(); System->AssignedEvent=INDEX_NONE; System->bReturning=false;
 System->WorkSeconds=0; System->MissionAttemptSeconds=0; System->StalledCarSeconds=150;
 Heli->SetActorLocation(FVector::ZeroVector);
 Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();

 auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine); PC->Possess(Heli);
 // This synthetic world never begins play, so explicitly register its player controller.
 World->AddController(PC);
 TestTrue(TEXT("Fixture controller possesses the test helicopter"),PC->GetPawn()==Heli);
 TestFalse(TEXT("Starter aircraft cannot tow"),System->CanPlayerTow());
 System->UpdateStalledCars(150.0f);
 TestEqual(TEXT("Without towing, 2.5 minutes consumes only one sixth of the interval"),System->StalledCarSeconds,125.0f);
 System->UpdateStalledCars(744.0f);
 TestTrue(TEXT("No ambient stalled car is due before 15 minutes"),FMath::IsNearlyEqual(System->StalledCarSeconds,1.0f));
 System->UpdateStalledCars(6.0f);
 TestEqual(TEXT("Rare stalled-car interval rearms after 15 minutes"),System->StalledCarSeconds,150.0f);
 Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,true);
 TestTrue(TEXT("Purchased cage is available on the helicopter"),Heli->IsToolAvailable(ESimCopterHelicopterTool::CaptureCage));
 TestTrue(TEXT("Buying the cage enables towing"),System->CanPlayerTow());
 System->UpdateStalledCars(596.0f);
 TestEqual(TEXT("Equipped stalls wait ten minutes instead of two and a half"),System->StalledCarSeconds,1.0f);
 auto* ParkedPilot=World->SpawnActor<ASimCopterOnFootPawn>(); ParkedPilot->SetParkedHelicopter(Heli); PC->Possess(ParkedPilot);
 TestTrue(TEXT("Walking away from an equipped helicopter retains towing capability"),System->CanPlayerTow());
 PC->Possess(Heli); ParkedPilot->Destroy();
 Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,false);
 System->UpdateStalledCars(3.0f);
 TestTrue(TEXT("Selling the cage slows an already pending mission"),FMath::IsNearlyEqual(System->StalledCarSeconds,0.5f));
 System->StalledCarSeconds=150.0f;

 Heli->ControllerLeftBumperPressed();
 TestTrue(TEXT("LB turns left without opening a wheel"),Heli->bBumperLeftHeld && Heli->ControllerMode==ESimCopterControllerMode::None);
 TestTrue(TEXT("LB reaches flight yaw controls"),Heli->BuildFlightInputs().bTurnLeftKey);
 Heli->ControllerRightBumperPressed();
 TestFalse(TEXT("Both bumpers suspend turning"),Heli->BuildFlightInputs().bTurnLeftKey || Heli->BuildFlightInputs().bTurnRightKey);
 TestTrue(TEXT("Both bumpers leave the tools wheel closed"),Heli->ControllerMode==ESimCopterControllerMode::None);
 Heli->ControllerLeftBumperReleased();
 TestTrue(TEXT("Remaining RB resumes turning without slowing or repressing"),Heli->BuildFlightInputs().bTurnRightKey);
 Heli->ControllerRightBumperReleased(); Heli->ControllerRightBumperPressed();
 TestTrue(TEXT("Fresh RB turns right"),Heli->BuildFlightInputs().bTurnRightKey);
 Heli->ControllerLeftBumperPressed();
 TestTrue(TEXT("Reverse press order also leaves wheels closed"),Heli->ControllerMode==ESimCopterControllerMode::None);
 Heli->ControllerRightBumperReleased();
 TestTrue(TEXT("Remaining LB resumes turning"),Heli->BuildFlightInputs().bTurnLeftKey);
 Heli->ControllerLeftBumperReleased(); Heli->ControllerOptionsPressed();
 TestTrue(TEXT("X opens Tools"),Heli->ControllerMode==ESimCopterControllerMode::ToolWheel);
 Heli->ControllerOptionsPressed();
 TestTrue(TEXT("X switches to Dispatch"),Heli->ControllerMode==ESimCopterControllerMode::DispatchWheel);
 Heli->ControllerCancelPressed();
 TestTrue(TEXT("B closes the wheel"),Heli->ControllerMode==ESimCopterControllerMode::None);
 TestEqual(TEXT("Map mission helper dispatch entry"),SimCopterControllerInput::GetDispatchSelection(4).ServiceIndex,100);
 TestFalse(TEXT("Helper locked before a passed level"),System->OrderSupport(1));

 const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
 auto Model=MakeShared<FPeopleBehaviorModel>(); FString Error;
 TestTrue(TEXT("People programs available"),FSimCopterPeopleReader::LoadFromFile(FSimCopterPeopleReader::ResolvePeoplePath(Root),*Model,Error));
 auto Person=[&](int32 State,FVector Location)
 {
  auto* P=World->SpawnActor<ASimCopterGroundAgent>(); P->SetOwner(Traffic); P->InitialPersonState=State;
  P->BehaviorModel=Model; P->BehaviorContext.ResetToState(State); P->bBehaviorActive=true;
  P->BehaviorContext.Attributes[EBhavAttr::MedevacHealth]=100;
  P->SetActorLocation(Location); Traffic->PedestrianAgents.Add(P); return P;
 };
 auto* Civilian=Person(0,FVector(300,300,24));
 TestFalse(TEXT("Taser cannot arrest an ordinary civilian"),Civilian->StunForArrest());
 auto* Criminal=Person(12,FVector(400,300,24));
 TestTrue(TEXT("Taser immobilizes an active mugger"),Criminal->StunForArrest());
 TestFalse(TEXT("Repeated taser shot cannot restart arrest"),Criminal->StunForArrest());
 TestFalse(TEXT("Stun does not award a completed arrest"),Criminal->HasMissionResolutionReported());
 TestTrue(TEXT("Stunned criminal boards actual helicopter"),Criminal->BoardCarrier(Heli,false,true));
 TestTrue(TEXT("Cabin passenger is handcuffed"),Criminal->IsHandcuffed());
 TestEqual(TEXT("One real custody occupant"),Heli->GetPassengerCount(),1);
 System->ServiceRoofs.Add(0xD2,TArray<FVector>{FVector(0,0,300)});
 auto* RoofOfficer=Person(7,FVector(0,0,300+24));
 Heli->SetActorLocation(FVector(0,0,300+Heli->GetPassengerDropHeightOffsetCm()+45));
 Ops->FinishPoliceHandoffs(1.6f);
 TestEqual(TEXT("Police handoff releases the seat"),Heli->GetPassengerCount(),0);
 TestTrue(TEXT("Police handoff resolves the person"),Criminal->HasMissionResolutionReported());
 Criminal->CompletePoliceDelivery(FVector(0,0,300));
 TestEqual(TEXT("Repeat handoff cannot remove another occupant"),Heli->GetPassengerCount(),0);

 Heli->SetDebugToolGrant(ESimCopterHelicopterTool::CaptureCage,true);
 Heli->SetSelectedTool(ESimCopterHelicopterTool::CaptureCage); Ops->UseCargoTool();
 Ops->Update(0.4f);
 TestTrue(TEXT("Purchased cage has visible skid fittings"),Ops->CageMountMesh && Ops->CageMountMesh->IsVisible());
 TestEqual(TEXT("Cage finishes forming from the skids"),Ops->CageFormation,1.0f);
 Heli->CargoKeyboardCableInput=1; Heli->bControllerDPadUpHeld=true;
 Heli->ControllerMode=ESimCopterControllerMode::ToolWheel; Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Menu navigation cannot reel a deployed cage"),Ops->CableInput,0.0f);
 Heli->ControllerMode=ESimCopterControllerMode::None; Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Cable resumes its manual raise input outside menus"),Ops->CableInput,1.0f);
 Heli->SetSelectedTool(ESimCopterHelicopterTool::WaterBucket); Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Changing away from cargo stops the cable"),Ops->CableInput,0.0f);
 Heli->bControllerDPadUpHeld=false; Heli->SetSelectedTool(ESimCopterHelicopterTool::CaptureCage);
 Ops->SetCagePanelInput(-1); Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Cockpit LOWER survives the controller's per-frame input refresh"),Ops->CableInput,-1.0f);
 Heli->ControllerMode=ESimCopterControllerMode::ToolWheel; Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Menus also suspend cockpit cage holds"),Ops->CableInput,0.0f);
 Heli->ControllerMode=ESimCopterControllerMode::None; Ops->SetCagePanelInput(0); Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Releasing the cockpit button stops the cable"),Ops->CableInput,0.0f);
 Ops->CargoLocation=FVector(1000,1000,50);
 Ops->CableLength=1600; Ops->CargoVelocity=FVector::ZeroVector; Ops->SetCableInput(0);
 TArray<ASimCopterGroundAgent*> Captives;
 for(int32 K=0;K<4;++K) Captives.Add(Person(K==0?10:0,Ops->CargoLocation+FVector(K*8,0,0)));
 Ops->Update(0.001f);
 TestFalse(TEXT("Cage automatically closes around people without a second use"),Ops->bCageOpen);
 TestTrue(TEXT("Floor starts open and animates after capture"),Ops->CageSeal<1);
 TestEqual(TEXT("Cage holds four"),Ops->GetCageCount(),4);
 TestTrue(TEXT("Captured suspect cannot escape via its behavior VM"),Captives[0]->IsInPoliceCustody());
 TArray<uint8> CargoSave; { FMemoryWriter W(CargoSave,true); Ops->SerializeState(W); }
 { FMemoryReader R(CargoSave,true); Ops->SerializeState(R); TestFalse(TEXT("Cage save reads"),R.IsError()); }
 Ops->ResolveSavedCargo();
 TestEqual(TEXT("Cage save relinks all people"),Ops->GetCageCount(),4);
 const int32 Capacity=Heli->GetAvailablePassengerSeats();
 Ops->CableLength=140; Ops->CargoLocation=Heli->GetCargoAnchorWorldLocation()-FVector(0,0,140);
 Ops->SetCableInput(1); Ops->Update(1.3f);
 TestEqual(TEXT("Top hold boards only available seats"),Heli->GetPassengerCount(),FMath::Min(4,Capacity));
 TestEqual(TEXT("Full cabin leaves excess in cage"),Ops->GetCageCount(),4-FMath::Min(4,Capacity));
 Ops->OpenCargo();
 TestEqual(TEXT("Opening cage releases remaining captives"),Ops->GetCageCount(),0);
 TestTrue(TEXT("Release gives people time to clear the capture volume"),Ops->CageCaptureDelay>0);
 for(const auto& Slot:TArray<FSimCopterMissionPassengerSlot>(Heli->GetMissionPassengerSlots())) if(Slot.Person.IsValid()) Slot.Person->AlightFromCarrier(false);

 auto* Car=World->SpawnActor<ASimCopterGroundAgent>(); Car->AgentKind=ESimCopterGroundAgentKind::Vehicle; Car->SetOwner(Traffic); Car->SetActorLocation(FVector(2000,2000,25));
 // Recovery covers an extinguished/player-caused breakdown. First-hit accidents
 // now have their own lifecycle and occupant regression test.
 Car->SetVehicleStalled(true);
 TestTrue(TEXT("Stalled car is immobilized"),Car->IsVehicleImmobilized());
 TestTrue(TEXT("Stalled car creates towing job"),Car->GetTowMissionId()!=INDEX_NONE);
 if(const auto* Tow=Missions->MissionSystem.FindRecord(Car->GetTowMissionId())) TestTrue(TEXT("Player-created recovery cannot farm rewards"),Tow->bSuppressCompletionRewards);
 const FVector Near(40,0,10),Far(-100,0,10);
 TestTrue(TEXT("Hit side geometry visibly dents"),!SimCopterAirOperations::DentVertex(Near,Near,FVector::ZeroVector).Equals(Near));
 TestTrue(TEXT("Far side retains body shape"),SimCopterAirOperations::DentVertex(Far,Near,FVector::ZeroVector).Equals(Far));
 auto* Yard=World->SpawnActor<ASimCopterRecoverySite>(); Yard->Configure(ESimCopterRecoverySite::Junkyard,FVector(2500,2500,0)); System->Sites.Add(Yard);
 auto* HealthyCar=World->SpawnActor<ASimCopterGroundAgent>(); HealthyCar->AgentKind=ESimCopterGroundAgentKind::Vehicle; HealthyCar->SetOwner(Traffic);
 System->StartStalledCar(false);
 TestFalse(TEXT("Unequipped player receives no second ambient stalled car"),HealthyCar->IsTowableVehicle());
 System->StartStalledCar(true);
 TestFalse(TEXT("Equipped player also receives at most one ambient stalled car"),HealthyCar->IsTowableVehicle());
 HealthyCar->Destroy();
 Ops->bCage=true; Ops->bCageOpen=true; Ops->CageCaptureDelay=0; Ops->bDeployed=true; Ops->CargoLocation=Car->GetActorLocation();
 TestTrue(TEXT("Cage grabs stalled vehicle"),Ops->TryGrabCargo());
 Ops->CageSeal=1;
 TestTrue(TEXT("Car suspends road movement while slung"),Car->IsVehicleTowed());
 Ops->CargoLocation=Yard->GetActorLocation()+FVector(0,0,70); Ops->CargoVelocity=FVector::ZeroVector;
 TestTrue(TEXT("Yard accepts gently delivered vehicle"),Ops->TryDeliverCargo());
 TestFalse(TEXT("Recovered car cannot be rewarded repeatedly"),Car->IsTowableVehicle());
 Missions->MissionSystem.UpdateLifecycle();
 TestEqual(TEXT("No crash-repair reward"),Missions->MissionSystem.GetScore(),0);

 auto* Swimmer=Person(0,FVector(100,100,24));
 City->WaterGameplayTerrainClasses.Init(0,128*128);
 Swimmer->BeginPassengerFall(INDEX_NONE,900); Swimmer->FinishPassengerFall(3000);
 TestTrue(TEXT("Water landing creates medevac for dropped civilian"),Swimmer->IsMedevacVictim() && Swimmer->MissionEventId!=INDEX_NONE);
 TestFalse(TEXT("Water landing exempts impact damage"),SimCopterAirOperations::FallCausesInjury(10000,true));
 TestTrue(TEXT("High dry landing injures"),SimCopterAirOperations::FallCausesInjury(1000,false));
 TestFalse(TEXT("Low dry landing is safe"),SimCopterAirOperations::FallCausesInjury(100,false));
 auto* Ambient=World->SpawnActor<ASimCopterAmbientVehiclesActor>();
 Ambient->Boats[0].bVisible=true; Ambient->Boats[0].ObjectId=SimCopterAmbientVehicles::CapsizedBoatObjectId;
 Ambient->Boats[0].World=FVector(8000,8000,0);
 Ambient->Boats[0].TowEventId=Missions->CreateMissionAt(70,70,TYPE_BoatTow);
 int32 BoatIndex=INDEX_NONE;
 TestTrue(TEXT("Capsized hull is cage eligible"),Ambient->FindTowableBoat(FVector(8000,8000,0),150,BoatIndex));
 Ambient->SetBoatTow(BoatIndex,true,FVector(8000,8000,1500));
 Ambient->UpdateBoat(Ambient->Boats[0],1);
 TestEqual(TEXT("Towed hull keeps sling height above water"),Ambient->Boats[0].World.Z,1500.0);
 TestTrue(TEXT("Harbor completes hull delivery"),Ambient->FinishBoatTow(BoatIndex));
 TestTrue(TEXT("Recovered hull remains displayed on repair platform"),Ambient->Boats[0].RepairDisplaySeconds>0 && Ambient->Boats[0].bVisible);
 TestFalse(TEXT("Repaired hull cannot be picked up again"),Ambient->FindTowableBoat(FVector(8000,8000,1500),150,BoatIndex));
 Missions->MissionSystem.UpdateLifecycle();
 TestEqual(TEXT("One recovered hull pays one award"),Missions->MissionSystem.GetScore(),100);

 // An assigned helper collects and delivers a real stalled car without player input.
 City->WaterGameplayTerrainClasses.Init(10,128*128);
 auto* ServiceCar=World->SpawnActor<ASimCopterGroundAgent>(); ServiceCar->SetOwner(Traffic);
 ServiceCar->AgentKind=ESimCopterGroundAgentKind::Vehicle; ServiceCar->SetActorLocation(FVector(3500,3500,25)); ServiceCar->SetVehicleStalled(false);
 Ops->ReleaseCargo(); Ops->bDeployed=false; Ops->bCage=false; Ops->SetSupportAircraft(true);
 Heli->SetDebugToolGrant(ESimCopterHelicopterTool::CaptureCage,true);
 System->SupportHelicopter=Heli; System->AssignedEvent=ServiceCar->GetTowMissionId();
 System->SupportHome=FVector(3500,3500,600); Heli->SetActorLocation(System->SupportHome);
 bool bLoadedCargoMigrated=false;
 for(int32 K=0;K<1200 && !ServiceCar->HasMissionResolutionReported();++K)
 {
  System->UpdateSupport(0.05f); Ops->Update(0.05f);
  if (!bLoadedCargoMigrated && Ops->HasCargo())
  {
   FMemoryReader Reader(LegacySupport,true); System->SerializeState(Reader);
   System->SupportHelicopter=Heli;
   TestTrue(TEXT("Retiring a legacy automatic assignment preserves its loaded vehicle"),Ops->GetTowVehicle()==ServiceCar);
   TestEqual(TEXT("Loaded legacy helper clears automatic assignment"),System->AssignedEvent,INDEX_NONE);
   bLoadedCargoMigrated=true;
  }
 }
 TestTrue(TEXT("Legacy cargo migration exercised after actual pickup"),bLoadedCargoMigrated);
 TestTrue(TEXT("AI helper independently delivers its assigned car"),ServiceCar->HasMissionResolutionReported());
 System->UpdateSupport(0.05f);
 TestEqual(TEXT("Legacy helper takes no new job after cargo delivery"),System->AssignedEvent,INDEX_NONE);

 // Helper cabin releases must use distinct door positions, not a shared destination point.
 FVector HelperPad; Traffic->TryGetAirportPadWorldLocation(0,HelperPad);
 const float HelperDoorOffset=Heli->GetActorLocation().Z-Heli->GetPassengerDropWorldLocation().Z;
 Heli->SetActorLocation(HelperPad+FVector(0,0,HelperDoorOffset+25)); Heli->VelocityCmPerSec=FVector::ZeroVector;
 Ops->ReleaseCargo(); Ops->bDeployed=false;
 System->AssignedEvent=INDEX_NONE;
 System->SupportHome=Heli->GetPassengerDropWorldLocation()+FVector(700,700,2200);
 auto* HelperRiderA=Person(1,Heli->GetActorLocation());
 auto* HelperRiderB=Person(1,Heli->GetActorLocation());
 TestTrue(TEXT("Helper first passenger boards"),HelperRiderA->BoardCarrier(Heli,false,true));
 TestTrue(TEXT("Helper second passenger boards"),HelperRiderB->BoardCarrier(Heli,false,true));
 System->UpdateSupport(0.05f);
 TestNull(TEXT("Helper releases first passenger"),HelperRiderA->GetBehaviorCarrier());
 TestNull(TEXT("Helper releases second passenger"),HelperRiderB->GetBehaviorCarrier());
 TestTrue(TEXT("Helper passengers get separate exit spaces"),FVector::Dist2D(HelperRiderA->GetActorLocation(),HelperRiderB->GetActorLocation())>=26);
 Ops->SetSupportAircraft(false); Ops->bDeployed=false;
 PC->Possess(Heli);
 Heli->SetActorLocation(FVector(10000,10000,1500)); Heli->GroundClearanceCm=1400; Heli->bIsLanded=false;
 auto* TaserTarget=Person(10,FVector(11000,10000,300));
 Heli->bPrimaryToolUseHeld=true; Heli->bPrimaryToolUsePressed=true;
 Heli->ToggleRemoteTaser();
 TestTrue(TEXT("Remote taser works without an officer and enables autopilot"),Ops->IsRemoteTaserActive() && Ops->IsAIPiloted());
 TestFalse(TEXT("Entering taser mode clears held and queued aircraft weapon input"),Heli->bPrimaryToolUseHeld || Heli->bPrimaryToolUsePressed);
 if(Ops->RemoteTaserCamera)
 {
  TestTrue(TEXT("Remote camera originates at the nose weapon"),Ops->RemoteTaserCamera->GetActorLocation().Equals(Ops->GetRemoteTaserMuzzle(),.1));
  TaserTarget->CurrentVelocityCmPerSec=FVector(120,80,0);
  TaserTarget->ExternalVelocityCmPerSec=FVector(90,30,0);
  Ops->RemoteTaserAim=(TaserTarget->GetActorLocation()-Ops->RemoteTaserMount->GetComponentLocation()).Rotation(); Ops->FireRemoteTaser();
  TestTrue(TEXT("Aimed taser hit incapacitates suspect"),TaserTarget->IsTaserStunned());
  TestFalse(TEXT("Taser leaves suspect alive"),TaserTarget->IsMissionPatientDead());
  TestEqual(TEXT("Taser does not subtract health"),TaserTarget->GetBehaviorAttribute(EBhavAttr::MedevacHealth),100);
  TestFalse(TEXT("Taser does not create a medical patient"),TaserTarget->IsMedevacVictim());
  TestFalse(TEXT("Taser does not complete the arrest before delivery"),TaserTarget->HasMissionResolutionReported());
  const FVector StunnedPosition=TaserTarget->GetActorLocation();
  TaserTarget->Tick(1.0f);
  // The floating synthetic actor can settle vertically onto the tile surface, but cannot walk or flee.
  TestTrue(TEXT("Stunned suspect cannot move across the ground during simulation"),FVector::DistSquared2D(TaserTarget->GetActorLocation(),StunnedPosition)<0.01f);
  TestTrue(TEXT("Taser clears walking and external movement"),TaserTarget->CurrentVelocityCmPerSec.IsNearlyZero() && TaserTarget->ExternalVelocityCmPerSec.IsNearlyZero());
  TestFalse(TEXT("Stunned suspect behavior stays suspended"),TaserTarget->bBehaviorActive);

  // Even a previously selected or directly requested Apache weapon cannot fire in this role.
  auto* ArmedHeli=World->SpawnActor<ASimCopterHelicopterPawn>();
  TestTrue(TEXT("Armed Apache fixture loads"),ArmedHeli->SwitchHelicopterModel(2));
  ArmedHeli->SetActorLocation(FVector(30000,30000,1500));
  ArmedHeli->SetDebugToolGrant(ESimCopterHelicopterTool::ApacheMachineGun,true);
  ArmedHeli->SetDebugToolGrant(ESimCopterHelicopterTool::ApacheMissile,true);
  ArmedHeli->SetSelectedTool(ESimCopterHelicopterTool::ApacheMachineGun);
  TestTrue(TEXT("Fixture has a selectable armed machine gun"),ArmedHeli->GetActiveTool()==ESimCopterHelicopterTool::ApacheMachineGun && ArmedHeli->IsToolAvailable(ESimCopterHelicopterTool::ApacheMachineGun));
  ArmedHeli->GetAirOperations()->bRemoteTaserActive=true;
  ArmedHeli->bPrimaryToolUseHeld=true;
  ArmedHeli->EmitApacheMachineGunFrame();
  TestFalse(TEXT("Passenger mode refuses a direct machine-gun command"),ArmedHeli->TryBeginToolUse(ESimCopterHelicopterTool::ApacheMachineGun));
  TestFalse(TEXT("Passenger mode refuses a direct missile command"),ArmedHeli->TryBeginToolUse(ESimCopterHelicopterTool::ApacheMissile));
  TestEqual(TEXT("Police mode emits no bullets from an armed Apache"),ArmedHeli->GetApachePool()->GetActiveBulletCount(),0);
  TestEqual(TEXT("Police mode emits no missiles from an armed Apache"),ArmedHeli->GetApachePool()->GetActiveMissileCount(),0);
  TestEqual(TEXT("Police taser emits no bullets"),Heli->GetApachePool()->GetActiveBulletCount(),0);
  TestEqual(TEXT("Police taser emits no missiles"),Heli->GetApachePool()->GetActiveMissileCount(),0);
  ArmedHeli->Destroy();

  // A civilian in the sightline must block the shot without injury or arrest.
  const FVector SightOrigin=Ops->RemoteTaserMount->GetComponentLocation();
  auto* Bystander=Person(0,SightOrigin+FVector(600,400,-900));
  auto* ShieldedSuspect=Person(11,SightOrigin+FVector(1200,800,-1800));
  Ops->RemoteTaserAim=(ShieldedSuspect->GetActorLocation()-SightOrigin).Rotation();
  Ops->ShotCooldown=0; Heli->StartPrimaryToolUse();
  TestFalse(TEXT("Civilian is not arrested"),Bystander->IsInPoliceCustody());
  TestFalse(TEXT("Civilian remains alive"),Bystander->IsMissionPatientDead());
  TestEqual(TEXT("Civilian health is preserved"),Bystander->GetBehaviorAttribute(EBhavAttr::MedevacHealth),100);
  TestFalse(TEXT("Taser cannot shoot through a civilian"),ShieldedSuspect->IsInPoliceCustody());
  Bystander->Destroy();
  Heli->ControllerRightTriggerInput=0; Heli->ControllerRightTrigger(1);
  TestFalse(TEXT("Taser cooldown blocks rapid trigger shot"),ShieldedSuspect->IsInPoliceCustody());
  Ops->ShotCooldown=0; Heli->ControllerRightTrigger(0); Heli->ControllerRightTrigger(1);
  TestTrue(TEXT("Right trigger stuns after recharge"),ShieldedSuspect->IsTaserStunned());
  TestEqual(TEXT("Right trigger does not damage suspect"),ShieldedSuspect->GetBehaviorAttribute(EBhavAttr::MedevacHealth),100);
  TestFalse(TEXT("Right trigger never arms aircraft primary weapon"),Heli->bPrimaryToolUseHeld || Heli->bPrimaryToolUsePressed);

  TestTrue(TEXT("Taser victim boards alive for arrest"),TaserTarget->BoardCarrier(Heli,false,true));
  TestTrue(TEXT("Taser victim is handcuffed in cabin"),TaserTarget->IsHandcuffed());
  TaserTarget->CompletePoliceDelivery(FVector(12000,10000,300));
  TestTrue(TEXT("Taser victim resolves as delivered prisoner"),TaserTarget->HasMissionResolutionReported());
  TestFalse(TEXT("Police delivery does not kill the suspect"),TaserTarget->IsMissionPatientDead());
  TestEqual(TEXT("Arrest preserves suspect health"),TaserTarget->GetBehaviorAttribute(EBhavAttr::MedevacHealth),100);
  TestTrue(TEXT("Delivery keeps handcuffed custody"),TaserTarget->IsHandcuffed());
 }
 Ops->ToggleRemoteTaser(); TestFalse(TEXT("Leaving taser restores manual piloting"),Ops->IsAIPiloted());
 auto* ArrestTarget=Person(12,FVector(6000,6000,24));
 auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
 Pilot->SetActorLocation(FVector(5500,6000,24));
 Pilot->CameraComponent->SetWorldLocationAndRotation(FVector(5500,6000,24),FRotator::ZeroRotator);
 Pilot->StartTaserAim(); Pilot->UpdateEnergyTaser(.7f); Pilot->FireTaser();
 TestFalse(TEXT("On-foot orb does not stun before arrival"),ArrestTarget->IsInPoliceCustody());
 for (TActorIterator<ASimCopterTaserOrb> It(World);It;++It)
  for (int32 Frame=0;Frame<100 && !It->HasImpacted();++Frame) It->Tick(.01f);
 TestTrue(TEXT("On-foot aimed taser hits suspect"),ArrestTarget->IsInPoliceCustody());
 TestFalse(TEXT("On-foot taser leaves suspect alive"),ArrestTarget->IsMissionPatientDead());
 TestEqual(TEXT("On-foot taser preserves health"),ArrestTarget->GetBehaviorAttribute(EBhavAttr::MedevacHealth),100);
 Pilot->StopTaserAim(); Pilot->SetActorLocation(FVector(5940,6000,24));
 auto* Wall=World->SpawnActor<AActor>();
 auto* Block=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Block); Wall->AddInstanceComponent(Block);
 Block->SetBoxExtent(FVector(8,60,100)); Block->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Block->SetCollisionResponseToAllChannels(ECR_Block); Block->RegisterComponent(); Wall->SetActorLocation(FVector(5970,6000,24));
 Pilot->UpdateAirOperations(0.01f);
 TestFalse(TEXT("Automatic pickup cannot reach through walls"),Pilot->IsCarryingMissionPerson());
 Wall->Destroy();
 ArrestTarget->SetActorLocation(FVector(6000,6000,224)); Pilot->UpdateAirOperations(0.01f);
 TestFalse(TEXT("Automatic pickup cannot reach another floor"),Pilot->IsCarryingMissionPerson());
 ArrestTarget->SetActorLocation(FVector(6000,6000,24));
 Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Falling); Pilot->UpdateAirOperations(0.01f);
 TestFalse(TEXT("Airborne pilot cannot scoop up a suspect"),Pilot->IsCarryingMissionPerson());
 Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Pilot->UpdateAirOperations(0.01f);
 TestTrue(TEXT("Walking up automatically carries the actual stunned suspect"),Pilot->CarriedMissionPerson.Get()==ArrestTarget);
 Pilot->DropCarriedMissionPerson();
 TestTrue(TEXT("Putting suspect down preserves arrest identity"),ArrestTarget->IsInPoliceCustody());
 TestNull(TEXT("Putting suspect down detaches their root from the pilot"),ArrestTarget->GetAttachParentActor());
 TestFalse(TEXT("Dropped suspect is no longer marked carried"),ArrestTarget->IsMissionCarried());
 TestTrue(TEXT("Dropped suspect restores collision and visible behavior"),ArrestTarget->GetActorEnableCollision() && ArrestTarget->GetBehaviorAttribute(EBhavAttr::Visible)==1);
 TestTrue(TEXT("Dropped suspect clears the shoulder-carry tilt"),FMath::Abs(ArrestTarget->GetActorRotation().Roll)<.1f);
 Pilot->UpdateAirOperations(0.1f);
 TestFalse(TEXT("Putting a person down does not instantly pick them up again"),Pilot->IsCarryingMissionPerson());
 Pilot->MissionPickupCooldownSeconds=0;
 ArrestTarget->FinishPassengerFall(0); Pilot->UpdateAirOperations(0.01f);
 TestFalse(TEXT("Standing next to a dropped suspect does not pick them up again after cooldown"),Pilot->IsCarryingMissionPerson());
 const FVector DroppedPosition=ArrestTarget->GetActorLocation();
 Pilot->SetActorLocation(FVector(5700,6000,24)); Pilot->SetActorRotation(FRotator(0,90,0));
 Pilot->UpdateAirOperations(.01f);
 TestTrue(TEXT("Moving and turning the pilot leaves the dropped suspect in place"),ArrestTarget->GetActorLocation().Equals(DroppedPosition));
 Pilot->SetActorLocation(DroppedPosition-FVector(60,0,0)); Pilot->UpdateAirOperations(.01f);
 TestTrue(TEXT("Walking away and returning can pick up the same suspect"),Pilot->CarriedMissionPerson.Get()==ArrestTarget);
 Pilot->DropCarriedMissionPerson(); Pilot->MissionPickupCooldownSeconds=0;
 Pilot->SetActorLocation(FVector(14000,14000,24));
 auto* WalkingCivilian=Person(0,FVector(14030,14000,24)); Pilot->UpdateAirOperations(0.01f);
 TestFalse(TEXT("Walking near a healthy civilian does not grab them"),Pilot->IsCarryingMissionPerson());
 WalkingCivilian->SetDroppedInjuredOnGround(FVector(14030,14000,24)); Pilot->UpdateAirOperations(0.01f);
 TestTrue(TEXT("Walking up automatically carries an injured civilian"),Pilot->CarriedMissionPerson.Get()==WalkingCivilian);
 Pilot->DropCarriedMissionPerson();
 Pilot->FallPeakZ=1600; Pilot->Landed(FHitResult());
 TestTrue(TEXT("Pilot suffers actual health loss after high exit"),Pilot->GetPilotHealth()<100);
 const float Health=Pilot->GetPilotHealth(); City->WaterGameplayTerrainClasses.Init(0,128*128);
 Pilot->FallPeakZ=9000; Pilot->Landed(FHitResult());
 TestEqual(TEXT("Pilot water landing preserves health"),Pilot->GetPilotHealth(),Health);

 auto* GI=NewObject<UGameInstance>(GEngine); auto* Career=NewObject<USimCopterCareerSubsystem>(GI);
 Career->BeginCareer(); TestFalse(TEXT("New career starts without helper"),Career->IsAirSupportUnlocked());
 Career->SetAirSupportUnlocked(true); Career->ContinueCareerIntoNextCity();
 TestFalse(TEXT("Entering the next city clears helper eligibility"),Career->IsAirSupportUnlocked());
 TestFalse(TEXT("An unfinished city cannot dispatch a helper"),System->IsSupportUnlocked());
 Career->SetAirSupportUnlocked(true);
 TestFalse(TEXT("Legacy carried unlock cannot enable an unfinished city"),System->IsSupportUnlocked());
 TestFalse(TEXT("Mission order cannot bypass graduation"),System->OrderSupport(INDEX_NONE));
 TestTrue(TEXT("Graduation fixture loads actual career thresholds"),Missions->MissionSystem.LoadCareerData(
  FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame/tweak/career.twk"))));
 Missions->MissionSystem.SelectCareerCity(0);
 Missions->MissionSystem.AddScore(399);
 Missions->MissionSystem.AdvanceCareerIfComplete();
 TestFalse(TEXT("One point below graduation keeps helper locked"),System->IsSupportUnlocked());
 Missions->MissionSystem.AddScore(1);
 Missions->MissionSystem.AdvanceCareerIfComplete();
 TestTrue(TEXT("Passing current city permits helper"),System->IsSupportUnlocked());
 // Exercise the public dispatch entry points and actual mission lifecycle.
 Ops->ReleaseCargo(); Heli->RemoveMissionPassengers(100); Ops->SetSupportAircraft(true);
 System->SupportHelicopter=Heli; System->AssignedEvent=INDEX_NONE;
 const int32 OrderedJob=Missions->MissionSystem.CreateIncidentMedevacAt(18,18);
 const int32 WaitingJob=Missions->MissionSystem.CreateIncidentMedevacAt(19,19);
 TestTrue(TEXT("Two dispatch jobs are active"),OrderedJob!=INDEX_NONE && WaitingJob!=INDEX_NONE);
 const int32 BaseEvent=Missions->MissionSystem.EnsureBaseLocationRecord(8,8);
 TestFalse(TEXT("Dispatch rejects the base marker"),System->OrderSupport(BaseEvent));
 TestFalse(TEXT("Dispatch rejects an empty selection"),System->OrderSupport(INDEX_NONE));
 System->UpdateSupport(0);
 TestEqual(TEXT("Graduation leaves active missions waiting for a player order"),System->AssignedEvent,INDEX_NONE);
 const int32 OrderedIndex=Missions->GetMissionRecords().IndexOfByPredicate([&](const auto& R){return R.EventId==OrderedJob;});
 Missions->MissionSystem.SetMapFocusRecordIndex(OrderedIndex,EMapFocusReason::CycledNext);
 Heli->RequestAirSupport();
 TestEqual(TEXT("Cockpit order assigns only the selected map mission"),System->AssignedEvent,OrderedJob);
 TArray<uint8> ManualSupport;
 { FMemoryWriter Writer(ManualSupport,true); System->SerializeState(Writer); }
 System->RecallSupport();
 {
  FMemoryReader Reader(ManualSupport,true); System->SerializeState(Reader);
  TestFalse(TEXT("Explicit mission save reads"),Reader.IsError());
  TestEqual(TEXT("Manual save retains the legacy field layout"),Reader.TotalSize(),int64(LegacySupport.Num()));
 }
 TestEqual(TEXT("A saved explicit mission order resumes"),System->AssignedEvent,OrderedJob);
 System->EnsureSupport();
 TestTrue(TEXT("Loading reconnects the existing helper"),System->GetSupportHelicopter()==Heli);
 Missions->MissionSystem.PostEvent(EVT_MedevacDelivered,OrderedJob,1);
 Missions->MissionSystem.UpdateLifecycle();
 const auto* CompletedOrder=Missions->MissionSystem.FindRecord(OrderedJob);
 TestTrue(TEXT("Assigned mission completes through its delivery event"),!CompletedOrder || !CompletedOrder->bActive || CompletedOrder->Category==CAT_CompleteNow);
 System->UpdateSupport(0.05f);
 TestEqual(TEXT("Completing an order does not claim the waiting job"),System->AssignedEvent,INDEX_NONE);
 TestFalse(TEXT("Completed jobs cannot be ordered again"),System->OrderSupport(OrderedJob));
 const int32 WaitingIndex=Missions->GetMissionRecords().IndexOfByPredicate([&](const auto& R){return R.EventId==WaitingJob;});
 Missions->MissionSystem.SetMapFocusRecordIndex(WaitingIndex,EMapFocusReason::CycledNext);
 Heli->ControllerRadialIndex=4; Heli->DispatchControllerSelection();
 TestEqual(TEXT("Controller can explicitly order the next mission"),System->AssignedEvent,WaitingJob);
 auto* BusyRider=Person(1,Heli->GetActorLocation());
 TestTrue(TEXT("Busy helper carries a real passenger"),BusyRider->BoardCarrier(Heli,false,true));
 TestFalse(TEXT("An order cannot interrupt an onboard passenger delivery"),System->OrderSupport(WaitingJob));
 TestEqual(TEXT("Rejected busy order preserves the current assignment"),System->AssignedEvent,WaitingJob);
 Heli->RemoveMissionPassengers(100); BusyRider->Destroy();
 Heli->ControllerRadialIndex=5; Heli->DispatchControllerSelection();
 TestEqual(TEXT("Controller Recall All cancels the helper assignment"),System->AssignedEvent,INDEX_NONE);
 Heli->SetActorLocation(System->SupportHome);
 System->UpdateSupport(0.05f); System->UpdateSupport(0.05f);
 TestEqual(TEXT("Recalled helper stays idle with jobs available"),System->AssignedEvent,INDEX_NONE);
 TestTrue(TEXT("Helper remains dispatchable after recall"),System->OrderSupport(WaitingJob));
 System->UpdateSupport(181.0f);
 TestEqual(TEXT("Unreachable order is abandoned without choosing another job"),System->AssignedEvent,INDEX_NONE);
 System->UpdateSupport(0.05f); System->UpdateSupport(0.05f);
 TestEqual(TEXT("Timeout returns to standby until another order"),System->AssignedEvent,INDEX_NONE);
 Missions->MissionSystem.SelectCareerCity(1);
 Missions->MissionSystem.ContinueSession(1000);
 TestFalse(TEXT("Fresh session after a transfer or warp requires graduation again"),System->IsSupportUnlocked());
 Ops->SetSupportAircraft(true);
 TestFalse(TEXT("Player cannot take over dispatch helper"),Heli->CanBeEnteredBy(Heli->GetActorLocation(),10000));
 const FVector Before=Heli->GetActorLocation(); Ops->AdvanceAutopilot(Before+FVector(0,0,1000),0.1f,500);
 TestTrue(TEXT("Helper autopilot moves real helicopter"),Heli->GetActorLocation().Z>Before.Z);
 TArray<uint8> State; {FMemoryWriter W(State,true); Ops->SerializeState(W);}
 Ops->SetSupportAircraft(false); {FMemoryReader R(State,true); Ops->SerializeState(R);}
 TestTrue(TEXT("Helper identity survives aircraft save"),Ops->IsSupportAircraft());
 Missions->MissionSystem=FSimCopterMissionSystem();
 Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
 Missions->MissionSystem.EnsureBaseLocationRecord(8,8);
 System->AssignedEvent=INDEX_NONE; System->bReturning=false;
 System->UpdateSupport(0.05f);
 TestEqual(TEXT("Idle helper never treats Base Location as a mission"),System->AssignedEvent,INDEX_NONE);

 const int32 NearbyRecovery=Missions->CreateMissionAt(64,64,TYPE_VehicleTow);
 const int32 OriginalJob=Missions->MissionSystem.CreateIncidentMedevacAt(20,20);
 TestTrue(TEXT("Original dispatch fixture creates a medical incident"),OriginalJob!=INDEX_NONE);
 System->UpdateSupport(0);
 TestEqual(TEXT("Idle helper does not claim an emergency or recovery"),System->AssignedEvent,INDEX_NONE);
 System->AssignedEvent=NearbyRecovery;
 System->UpdateSupport(0);
 TestEqual(TEXT("Manual recovery assignment remains respected"),System->AssignedEvent,NearbyRecovery);
 auto* WaitingCar=World->SpawnActor<ASimCopterGroundAgent>(); WaitingCar->AgentKind=ESimCopterGroundAgentKind::Vehicle; WaitingCar->SetOwner(Traffic);
 System->StartStalledCar(true);
 TestFalse(TEXT("Original active work defers ambient breakdowns"),WaitingCar->IsTowableVehicle());
 WaitingCar->Destroy();
 Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
 System->AssignedEvent=INDEX_NONE;

 // End-to-end helper flight: collect a fragile patient, clear a real blocking building,
 // descend onto a hospital roof, and deliver the same living actor without penalties.
 PC->UnPossess(); Heli->RemoveMissionPassengers(100); Ops->ReleaseCargo(); Ops->bDeployed=false;
	// The mission-system reset above reuses event IDs. Retire its old fixture people so
	// an earlier dropped swimmer cannot consume this patient's new one-person rescue.
	for (auto Existing : Traffic->PedestrianAgents) if (Existing.IsValid()) Existing->Destroy();
	Traffic->PedestrianAgents.Reset();
 Ops->SetSupportAircraft(true); Heli->SwitchHelicopterModel(0);
 System->SupportHelicopter=Heli; System->bReturning=false;
 City->WaterGameplayTerrainClasses.Init(10,128*128);
 const FVector Pickup(-20000,-18000,0), Hospital(-12000,-18000,300);
 auto Box=[&](const FVector& At,const FVector& Half)
 {
  auto* Actor=World->SpawnActor<AActor>(); auto* Shape=NewObject<UBoxComponent>(Actor);
  Actor->SetRootComponent(Shape); Actor->AddInstanceComponent(Shape); Shape->SetBoxExtent(Half);
  Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Shape->SetCollisionResponseToAllChannels(ECR_Block);
  Shape->RegisterComponent(); Actor->SetActorLocation(At); return Actor;
 };
 auto* Building=Box(FVector(-17000,-18000,550),FVector(150,900,550));
 auto* HospitalBlock=Box(Hospital-FVector(0,0,150),FVector(700,700,150));
 auto* PickupGround=Box(Pickup-FVector(0,0,40),FVector(1000,1000,40));
 int32 PX=0,PY=0,HX=0,HY=0;
 TestTrue(TEXT("Patient has a valid city tile"),Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Pickup,PX,PY));
 Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Hospital,HX,HY);
 Traffic->XbldTileIds[HY*128+HX]=0xD1;
 System->ServiceRoofs.Add(0xD1,TArray<FVector>{Hospital});
 const int32 RescueId=Missions->MissionSystem.CreateIncidentMedevacAt(PX,PY);
 auto* Patient=Person(6,Pickup+FVector(0,0,24)); Patient->MissionEventId=RescueId;
 Patient->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT("PED1"),Root,125);
 Patient->BehaviorContext.Attributes[EBhavAttr::MedevacHealth]=1;
 System->AssignedEvent=RescueId; System->MissionAttemptSeconds=0; System->WorkSeconds=0;
 Heli->SetAutopilotTransform(Pickup+FVector(0,0,1500),FRotator::ZeroRotator,0.05f);
 bool bHitBuilding=false,bBoarded=false,bRecalledWithPatient=false;
 for(int32 Step=0;Step<2400 && !Patient->HasMissionResolutionReported();++Step)
 {
  System->UpdateSupport(0.05f); Ops->Update(0.05f); Patient->UpdateOriginalBehavior(0.05f);
  bBoarded |= Patient->GetBehaviorCarrier()==Heli;
  if (bBoarded && !bRecalledWithPatient)
  {
   System->RecallSupport();
   TestTrue(TEXT("Recall keeps the rescued patient aboard for delivery"),Patient->GetBehaviorCarrier()==Heli);
   bRecalledWithPatient=true;
  }
  bHitBuilding |= Heli->GetParkingWorldBounds().Intersect(Building->GetComponentsBoundingBox());
 }
 TestTrue(TEXT("Helper boards the assigned real patient"),bBoarded);
 TestFalse(TEXT("Full airframe clears the building during the rescue"),bHitBuilding);
 AddInfo(FString::Printf(TEXT("Helper rescue result: position=%s patient=%s state=%d initial=%d event=%d resolved=%d health=%d"),
  *Heli->GetActorLocation().ToString(),*Patient->GetActorLocation().ToString(),Patient->BehaviorContext.GetStateIndex(),
  Patient->InitialPersonState,Patient->MissionEventId,Patient->HasMissionResolutionReported(),Patient->GetBehaviorAttribute(EBhavAttr::MedevacHealth)));
 TestTrue(TEXT("Helper completes roof delivery within two minutes"),Patient->HasMissionResolutionReported());
 TestFalse(TEXT("Critical patient remains alive during helper rescue"),Patient->IsMissionPatientDead());
 TestNull(TEXT("Delivered patient no longer occupies helper cabin"),Patient->GetBehaviorCarrier());
 const auto* Rescued=Missions->MissionSystem.FindRecord(RescueId);
 TestTrue(TEXT("One delivery and no casualties are recorded"),Rescued && Rescued->MedevacDelivered==1 && Rescued->Casualties==0);
 TestTrue(TEXT("Helper patient resumes original post-delivery program"),Patient->bBehaviorActive &&
  Patient->BehaviorContext.Stack.Num()>0 && Patient->BehaviorContext.Stack[0].ProgramId==282);
 bool bCheered=false;
 for(int32 Step=0;Step<120;++Step)
 {
  Patient->UpdateOriginalBehavior(0.05f);
  bCheered |= Patient->GetFigureMnemonic()==TEXT("Whoa");
 }
 TestTrue(TEXT("Delivered helper patient performs original Whoa celebration"),bCheered);
 TestFalse(TEXT("Celebration does not kill the patient"),Patient->IsMissionPatientDead());
 TestTrue(TEXT("Original celebration finishes, rather than looping"),!Patient->IsBehaviorActive());
 Building->Destroy(); HospitalBlock->Destroy(); PickupGround->Destroy();

 // A building destination and a passenger in a narrow strip beside another building.
 // The old helper aimed at the roof / person's feet, so a safe full-body sweep could
 // never reach either goal. Exercise the real flight and walking path with no teleport.
 struct FTransportWorld : ISimCopterMissionWorld
 {
  ASimCopterTrafficSystemActor* Traffic=nullptr;
  bool bResolve=false;
  virtual int32 GetXbldTileId(int32 X,int32 Y) const override { return 0x70; }
  virtual int32 GetBuildingFootprintSize(int32,int32) const override { return 1; }
  virtual bool GetCameraTile(int32& X,int32& Y) const override { X=64; Y=64; return true; }
  virtual bool GetPlayerTile(int32& X,int32& Y) const override { return GetCameraTile(X,Y); }
  virtual bool TrySpawnMissionPerson(int32,int32,int32,int32,int32) override { return true; }
  virtual bool TryResolveTransportSpawnTile(int32 X,int32 Y,int32& OutX,int32& OutY) override
  {
   OutX=X; OutY=Y;
   if (!bResolve) return true;
   FVector Origin,Clearing;
   return Traffic->TryGetTileCenterWorldLocation(X,Y,Origin) && Traffic->TryFindTransportClearing(Origin,Clearing,275,3200) &&
    Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Clearing,OutX,OutY);
  }
 } TransportWorld;
 TransportWorld.Traffic=Traffic;
 Missions->MissionSystem=FSimCopterMissionSystem();
 Missions->MissionSystem.Initialize(&TransportWorld,1); Missions->MissionSystem.BeginSession();
 const int32 TransportId=Missions->MissionSystem.CreateEventAt(76,64,TYPE_Transport);
 TestTrue(TEXT("Transport fixture created"),TransportId>=0);
 FVector DropCenter; Traffic->TryGetTileCenterWorldLocation(76,64,DropCenter);
 FVector WaitCenter; Traffic->TryGetTileCenterWorldLocation(64,64,WaitCenter);
 auto* Ground=Box(FVector(0,0,-40),FVector(24000,24000,40));
 auto* DropBuilding=Box(DropCenter+FVector(0,0,600),FVector(520,520,600));
 auto* WaitBuilding=Box(WaitCenter+FVector(0,600,600),FVector(900,400,600));
 TestFalse(TEXT("Roof-centre marker is not a clearing"),Traffic->IsTransportClearing(DropCenter));
 TestFalse(TEXT("Pedestrian-size gap is too small for rotor footprint"),Traffic->IsTransportClearing(WaitCenter));
 FVector Clearing;
 TestTrue(TEXT("Nearby ground clearing exists"),Traffic->TryFindTransportClearing(DropCenter,Clearing));
 TestTrue(TEXT("Clearing remains within four original tiles"),FVector::Dist2D(Clearing,DropCenter)<=1600);
 TestTrue(TEXT("Dropoff is ground-level"),FMath::Abs(Clearing.Z)<1);
 TestFalse(TEXT("Bounded resolver does not move missions across the city"),Traffic->TryFindTransportClearing(DropCenter,Clearing,350,200));
 Traffic->WaterTileFlags.Init(1,128*128);
 TestFalse(TEXT("Water cannot be a clearing"),Traffic->TryFindTransportClearing(WaitCenter,Clearing));
 Traffic->WaterTileFlags.Init(0,128*128);
 TransportWorld.bResolve=true;
 Missions->MissionSystem.ResolveTransportDestinationClearings();
 const auto* Adjusted=Missions->MissionSystem.FindRecord(TransportId);
 TestTrue(TEXT("Old roof destination relocates to nearby clearing"),Adjusted->SecondaryX!=76 || Adjusted->SecondaryY!=64);
 TestEqual(TEXT("Primary map marker follows clearing"),Adjusted->TileX,Adjusted->SecondaryX);
 const FIntPoint SavedDestination(Adjusted->SecondaryX,Adjusted->SecondaryY);
 Missions->MissionSystem.ResolveTransportDestinationClearings();
 TestTrue(TEXT("Repeated restore keeps the same destination"),SavedDestination==FIntPoint(Adjusted->SecondaryX,Adjusted->SecondaryY));
 const int32 FreshTransportId=Missions->MissionSystem.CreateEventAt(76,64,TYPE_Transport);
 const auto* FreshTransport=Missions->MissionSystem.FindRecord(FreshTransportId);
 TestNotNull(TEXT("New transport can still be created beside a blocked building"),FreshTransport);
 if (FreshTransport)
 {
  FVector FreshPickup,FreshDrop;
  TestTrue(TEXT("New mission pickup has helicopter clearance"),Traffic->TryGetTileCenterWorldLocation(FreshTransport->TertiaryX,FreshTransport->TertiaryY,FreshPickup) && Traffic->IsTransportClearing(FreshPickup));
  TestTrue(TEXT("New mission destination has helicopter clearance"),Traffic->TryGetTileCenterWorldLocation(FreshTransport->SecondaryX,FreshTransport->SecondaryY,FreshDrop) && Traffic->IsTransportClearing(FreshDrop));
  TestTrue(TEXT("New destination stays in its original neighbourhood"),FVector::Dist2D(FreshDrop,DropCenter)<=3200);
 }
 auto* Fare=Person(4,WaitCenter+FVector(0,0,24)); Fare->MissionEventId=TransportId;
 System->AssignedEvent=TransportId; System->bReturning=false;
 System->MissionAttemptSeconds=0; System->WorkSeconds=0; System->bHasLandingSurface=false;
 Heli->SetAutopilotTransform(WaitCenter+FVector(-2000,0,1800),FRotator::ZeroRotator,0.05f);
 bool bFareBoarded=false,bTouchedBuilding=false,bRecalled=false;
 for(int32 Step=0;Step<3600 && !Fare->HasMissionResolutionReported();++Step)
 {
  System->UpdateSupport(0.05f); Ops->Update(0.05f); Fare->Tick(0.05f);
  bFareBoarded |= Fare->GetBehaviorCarrier()==Heli;
  // A loaded recall still owes this passenger the destination stored on their own event.
  if(bFareBoarded && !bRecalled) { System->RecallSupport(); bRecalled=true; }
  bTouchedBuilding |= Heli->GetParkingWorldBounds().Intersect(DropBuilding->GetComponentsBoundingBox()) ||
   Heli->GetParkingWorldBounds().Intersect(WaitBuilding->GetComponentsBoundingBox());
 }
 AddInfo(FString::Printf(TEXT("Transport result: helicopter=%s fare=%s boarded=%d delivered=%d status=%s"),
  *Heli->GetActorLocation().ToString(),*Fare->GetActorLocation().ToString(),bFareBoarded,Fare->HasMissionResolutionReported(),*Ops->GetStatus()));
 TestTrue(TEXT("Helper collects passenger via nearby clearing"),bFareBoarded);
 TestTrue(TEXT("Helper delivers transport even after loaded recall"),Fare->HasMissionResolutionReported());
 TestFalse(TEXT("Transport flight keeps rotors and tail outside buildings"),bTouchedBuilding);
 TestTrue(TEXT("Delivered fare remains an ordinary living pedestrian"),!Fare->IsHidden() && Fare->IsBehaviorActive() && !Fare->IsMissionPatientDead());
 TestTrue(TEXT("Landing stays in the original neighbourhood"),FVector::Dist2D(Heli->GetActorLocation(),DropCenter)<=1600);
 TestTrue(TEXT("Fare exits beside the nearby landing point"),FVector::Dist2D(Fare->GetActorLocation(),Heli->GetActorLocation())<=180);
 TestEqual(TEXT("Transport credits exactly one delivery"),Missions->MissionSystem.FindRecord(TransportId)->TransportDelivered,1);
 Ground->Destroy(); DropBuilding->Destroy(); WaitBuilding->Destroy();
 Missions->MissionSystem=FSimCopterMissionSystem();
 Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();

 // Even a forced helper contact must not invoke the player's accident/fine path.
 auto* UnharmedCar=World->SpawnActor<ASimCopterGroundAgent>(); UnharmedCar->AgentKind=ESimCopterGroundAgentKind::Vehicle;
 UnharmedCar->SetOwner(Traffic); UnharmedCar->SetActorLocation(Heli->GetActorLocation());
 UnharmedCar->CurrentVelocityCmPerSec=FVector(800,0,0);
 const int32 CashBefore=Missions->GetSessionCash(),ScoreBefore=Missions->GetSessionScore();
 Ops->SetSupportGoal(Heli->GetActorLocation()); Ops->Update(0.05f);
 TestFalse(TEXT("Helper contact cannot tip or ignite a car"),UnharmedCar->bVehicleTipped || UnharmedCar->bVehicleExploded);
 TestEqual(TEXT("Helper contact cannot fine the player"),Missions->GetSessionCash(),CashBefore);
 TestEqual(TEXT("Helper contact cannot dock player score"),Missions->GetSessionScore(),ScoreBefore);
 UnharmedCar->Destroy();

 // Defensive attribution remains per person and survives a save; it grants no false rescue.
 const int32 FailedId=Missions->MissionSystem.CreateIncidentMedevacAt(PX,PY);
 auto* Protected=Person(6,Heli->GetActorLocation()); Protected->MissionEventId=FailedId;
 TestTrue(TEXT("Responsibility fixture boards support"),Protected->BoardCarrier(Heli,false,true));
 TArray<uint8> PersonSave; TestTrue(TEXT("Support passenger saves"),Protected->CaptureRuntimeSaveState(PersonSave));
 TestTrue(TEXT("Support passenger restores"),Protected->RestoreRuntimeSaveState(PersonSave));
 TestTrue(TEXT("Helper responsibility survives save"),Protected->IsAirSupportPassenger());
 TestTrue(TEXT("Unexpected helper casualty resolves without player penalty"),Missions->NotifyMissionPersonDied(Protected));
 const auto* Failed=Missions->MissionSystem.FindRecord(FailedId);
 TestTrue(TEXT("Helper failure neither charges a casualty nor invents a delivery"),Failed && Failed->Casualties==0 && Failed->MedevacDelivered==0 && Failed->MedevacVictims==0);
 GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
 return true;
}
#endif

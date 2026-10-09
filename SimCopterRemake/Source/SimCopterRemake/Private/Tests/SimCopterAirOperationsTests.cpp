#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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
 Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TowClamp,true);
 TestTrue(TEXT("Purchased clamp is available on the helicopter"),Heli->IsToolAvailable(ESimCopterHelicopterTool::TowClamp));
 TestTrue(TEXT("Buying the tow clamp enables the normal mission rate"),System->CanPlayerTow());
 System->UpdateStalledCars(149.0f);
 TestEqual(TEXT("Equipped timer advances at normal speed"),System->StalledCarSeconds,1.0f);
 auto* ParkedPilot=World->SpawnActor<ASimCopterOnFootPawn>(); ParkedPilot->SetParkedHelicopter(Heli); PC->Possess(ParkedPilot);
 TestTrue(TEXT("Walking away from an equipped helicopter retains towing capability"),System->CanPlayerTow());
 PC->Possess(Heli); ParkedPilot->Destroy();
 Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TowClamp,false);
 System->UpdateStalledCars(3.0f);
 TestTrue(TEXT("Selling the clamp slows an already pending mission"),FMath::IsNearlyEqual(System->StalledCarSeconds,0.5f));
 System->StalledCarSeconds=150.0f;

 Heli->ControllerLeftBumperPressed();
 TestTrue(TEXT("LB turns left without opening a wheel"),Heli->bBumperLeftHeld && Heli->ControllerMode==ESimCopterControllerMode::None);
 TestTrue(TEXT("LB reaches flight yaw controls"),Heli->BuildFlightInputs().bTurnLeftKey);
 Heli->ControllerRightBumperPressed();
 TestFalse(TEXT("Both bumpers suspend turning"),Heli->BuildFlightInputs().bTurnLeftKey || Heli->BuildFlightInputs().bTurnRightKey);
 TestTrue(TEXT("Both bumpers open Tools without R3"),Heli->ControllerMode==ESimCopterControllerMode::ToolWheel);
 Heli->ControllerEnterExitPressed();
 TestTrue(TEXT("Y switches to Dispatch"),Heli->ControllerMode==ESimCopterControllerMode::DispatchWheel);
 Heli->ControllerLeftBumperReleased();
 TestTrue(TEXT("Releasing either bumper closes wheel"),Heli->ControllerMode==ESimCopterControllerMode::None);
 TestFalse(TEXT("Remaining held bumper cannot cause an unexpected turn"),Heli->BuildFlightInputs().bTurnRightKey);
 Heli->ControllerRightBumperReleased(); Heli->ControllerRightBumperPressed();
 TestTrue(TEXT("Fresh RB turns right"),Heli->BuildFlightInputs().bTurnRightKey);
 Heli->ControllerLeftBumperPressed();
 TestTrue(TEXT("Reverse press order opens same wheel"),Heli->ControllerMode==ESimCopterControllerMode::ToolWheel);
 Heli->ControllerCancelPressed(); Heli->ControllerRightBumperReleased(); Heli->ControllerLeftBumperReleased();
 TestTrue(TEXT("Cancel and chord release leave menus closed"),Heli->ControllerMode==ESimCopterControllerMode::None);
 TestEqual(TEXT("Map mission helper dispatch entry"),SimCopterControllerInput::GetDispatchSelection(4).ServiceIndex,100);
 TestFalse(TEXT("Helper locked before a passed level"),System->OrderSupport(1,true));

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
 Heli->CargoKeyboardCableInput=1; Heli->bControllerDPadUpHeld=true;
 Heli->ControllerMode=ESimCopterControllerMode::ToolWheel; Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Menu navigation cannot reel a deployed cage"),Ops->CableInput,0.0f);
 Heli->ControllerMode=ESimCopterControllerMode::None; Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Cable resumes its manual raise input outside menus"),Ops->CableInput,1.0f);
 Heli->SetSelectedTool(ESimCopterHelicopterTool::WaterBucket); Heli->UpdateControllerInput(0.01f);
 TestEqual(TEXT("Changing away from cargo stops the cable"),Ops->CableInput,0.0f);
 Heli->bControllerDPadUpHeld=false; Heli->SetSelectedTool(ESimCopterHelicopterTool::CaptureCage);
 Ops->CargoLocation=FVector(1000,1000,50);
 TArray<ASimCopterGroundAgent*> Captives;
 for(int32 K=0;K<4;++K) Captives.Add(Person(K==0?10:0,Ops->CargoLocation+FVector(K*8,0,0)));
 TestTrue(TEXT("Cage captures actual people"),Ops->TryGrabCargo());
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
 auto* Yard=World->SpawnActor<ASimCopterRecoverySite>(); Yard->Configure(ESimCopterRecoverySite::AutoRepair,FVector(2500,2500,0)); System->Sites.Add(Yard);
 auto* HealthyCar=World->SpawnActor<ASimCopterGroundAgent>(); HealthyCar->AgentKind=ESimCopterGroundAgentKind::Vehicle; HealthyCar->SetOwner(Traffic);
 System->StartStalledCar(false);
 TestFalse(TEXT("Unequipped player receives no second ambient stalled car"),HealthyCar->IsTowableVehicle());
 HealthyCar->Destroy();
 Ops->bCage=false; Ops->bDeployed=true; Ops->CargoLocation=Car->GetActorLocation();
 TestTrue(TEXT("Clamp grabs stalled vehicle"),Ops->TryGrabCargo());
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
 TestTrue(TEXT("Capsized hull is clamp eligible"),Ambient->FindTowableBoat(FVector(8000,8000,0),150,BoatIndex));
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
 Heli->SetDebugToolGrant(ESimCopterHelicopterTool::TowClamp,true);
 System->SupportHelicopter=Heli; System->AssignedEvent=ServiceCar->GetTowMissionId(); System->bAutomatic=false;
 System->SupportHome=FVector(3500,3500,600); Heli->SetActorLocation(System->SupportHome);
 for(int32 K=0;K<1200 && !ServiceCar->HasMissionResolutionReported();++K) { System->UpdateSupport(0.05f); Ops->Update(0.05f); }
 TestTrue(TEXT("AI helper independently delivers its assigned car"),ServiceCar->HasMissionResolutionReported());

 // Helper cabin releases must use distinct door positions, not a shared destination point.
 Heli->SetActorLocation(FVector(15000,15000,100)); Heli->VelocityCmPerSec=FVector::ZeroVector;
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
 auto* Police=Person(7,FVector(10000,10000,1450));
 TestTrue(TEXT("Living officer occupies taser seat"),Police->BoardCarrier(Heli,false,true));
 auto* TaserTarget=Person(10,FVector(11000,10000,300));
 Heli->bPrimaryToolUseHeld=true; Heli->bPrimaryToolUsePressed=true;
 Heli->TogglePoliceTaser();
 TestTrue(TEXT("Police passenger enters taser camera with AI piloting"),Ops->IsPoliceTaserActive() && Ops->IsAIPiloted());
 TestFalse(TEXT("Entering taser mode clears held and queued aircraft weapon input"),Heli->bPrimaryToolUseHeld || Heli->bPrimaryToolUsePressed);
 if(Ops->PoliceTaserCamera)
 {
  Ops->PoliceTaserCamera->SetActorLocation(FVector(10000,10000,1500));
  TaserTarget->CurrentVelocityCmPerSec=FVector(120,80,0);
  TaserTarget->ExternalVelocityCmPerSec=FVector(90,30,0);
  Ops->PoliceTaserAim=(TaserTarget->GetActorLocation()-Ops->PoliceTaserCamera->GetActorLocation()).Rotation(); Ops->FirePoliceTaser();
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
  ArmedHeli->GetAirOperations()->bPoliceTaserActive=true;
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
  auto* Bystander=Person(0,FVector(10600,10400,600));
  auto* ShieldedSuspect=Person(11,FVector(11200,10800,-300));
  Ops->PoliceTaserAim=(ShieldedSuspect->GetActorLocation()-Ops->PoliceTaserCamera->GetActorLocation()).Rotation();
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
 Ops->TogglePoliceTaser(); TestFalse(TEXT("Leaving taser restores manual piloting"),Ops->IsAIPiloted());
 Police->AlightFromCarrier(false);
 auto* ArrestTarget=Person(12,FVector(6000,6000,24));
 auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
 Pilot->CameraComponent->SetWorldLocationAndRotation(FVector(5500,6000,24),FRotator::ZeroRotator);
 Pilot->StartTaserAim(); Pilot->FireTaser();
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
 Pilot->UpdateAirOperations(0.1f);
 TestFalse(TEXT("Putting a person down does not instantly pick them up again"),Pilot->IsCarryingMissionPerson());
 Pilot->MissionPickupCooldownSeconds=0;
 ArrestTarget->FinishPassengerFall(0); Pilot->UpdateAirOperations(0.01f);
 TestTrue(TEXT("Pickup resumes when drop cooldown expires"),Pilot->CarriedMissionPerson.Get()==ArrestTarget);
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
 TestTrue(TEXT("Helper unlock persists between levels"),Career->IsAirSupportUnlocked());
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
 System->AssignedEvent=INDEX_NONE; System->bAutomatic=true; System->bReturning=false;
 System->UpdateSupport(0.05f);
 TestEqual(TEXT("Automatic helper never treats Base Location as a mission"),System->AssignedEvent,INDEX_NONE);
 GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
 return true;
}
#endif

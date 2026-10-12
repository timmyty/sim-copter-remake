#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Game/SimCopterSaveSubsystem.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "UI/SimCopterHangarShop.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterFleetEquipmentTest,"SimCopter.SaveGame.FleetEquipmentRepair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterFleetEquipmentTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Current=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Other=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	auto* Career=Instance->GetSubsystem<USimCopterCareerSubsystem>();
	auto* Saves=Instance->GetSubsystem<USimCopterSaveSubsystem>();
	auto* Save=NewObject<USimCopterSaveGame>(Instance);
	Save->CityWeights.Init(0.0f,7);
	Save->bHasAircraftState=true; Save->ActiveHelicopterTypeIndex=4;
	Save->OwnedHelicopterMask=(1<<4)|(1<<0);
	Save->CareerEquipmentMask=0x13; // Original aircraft has the cannon.
	Other->RestoreSavedCareerState(0,0x07,0,1.0f,0.0f,0); // Other aircraft has the harness.
	FSimCopterParkedAircraftSave Record; Record.TypeIndex=0; Record.Identity=TEXT("FleetEquipmentOther");
	if (!TestTrue(TEXT("Capture old parked aircraft state"),Other->CaptureRuntimeSaveState(Record.RuntimeState))) return false;
	Save->ParkedAircraft.Add(Record);
	Saves->PendingLoadedGame=Save;
	if (!TestTrue(TEXT("Restore career before aircraft"),Saves->ApplyPendingMissionAndCareerState(Missions)) ||
		!TestTrue(TEXT("Load and repair old fleet equipment"),Saves->ApplyPendingAircraftState(World))) return false;
	auto Fleet=SimCopterHelicopterParking::GetPlayerFleet(World,Career);
	if (!TestEqual(TEXT("Both owned aircraft restored"),Fleet.Num(),2)) return false;
	for (auto* Aircraft:Fleet)
		TestEqual(TEXT("All prior purchases survive on both aircraft"),Aircraft->GetEquipmentState().CareerEquipmentMask,0x17);
	Current=Fleet[0]; Other=Fleet[1];
	auto* Support=World->SpawnActor<ASimCopterHelicopterPawn>();
	Support->GetAirOperations()->SetSupportAircraft(true);
	Support->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TearGas,true);
	auto* Apache=World->SpawnActor<ASimCopterHelicopterPawn>();
	if (!TestTrue(TEXT("Load unclaimed Apache"),Apache->SwitchHelicopterModel(2))) return false;
	Apache->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TearGas,true);
	Current->SetDebugToolGrant(ESimCopterHelicopterTool::TearGas,true);
	SimCopterHelicopterParking::SynchronizeFleetEquipment(World,Career);
	TestEqual(TEXT("Support, unclaimed aircraft and debug tools do not grant purchases"),Other->GetEquipmentState().CareerEquipmentMask,0x17);
	TestFalse(TEXT("Support aircraft is not upgraded"),Support->GetEquipmentState().HasCareerBit(4));
	TestFalse(TEXT("Unclaimed aircraft is not upgraded"),Apache->GetEquipmentState().HasCareerBit(4));
	Current->SetDebugToolGrant(ESimCopterHelicopterTool::TearGas,false);
	Current->RestoreSavedCareerState(Current->GetHelicopterTypeIndex(),0x1f,2,1.0f,0.0f,0);
	SimCopterHelicopterParking::SynchronizeFleetEquipment(World,Career);
	TestEqual(TEXT("Repair never refills an existing tear gas launcher"),Current->GetEquipmentState().CareerTearGasRounds,2);
	TestEqual(TEXT("Newly fitted tear gas includes its ammunition"),Other->GetEquipmentState().CareerTearGasRounds,SimCopterHelicopterRegistry::TearGasCapacity);
	// Selling a global item must not remove a deployed cage from a different airframe.
	Other->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,true);
	SimCopterHelicopterParking::SynchronizeFleetEquipment(World,Career);
	Other->SelectCaptureCage(); Other->GetAirOperations()->UseCargoTool();
	TestTrue(TEXT("Other aircraft deploys its cage"),Other->GetAirOperations()->IsDeployed());
	SimCopterHangarShop::FContext Shop; Shop.Helicopter=Current; Shop.Career=Career; Shop.Missions=Missions;
	TestFalse(TEXT("Cannot sell cage deployed on another helicopter"),SimCopterHangarShop::GetUpgradeRowState(Shop,5).bCanSell);
	// Discovery is another way of acquiring an aircraft and must inherit exact ownership.
	Current->SetCareerEquipmentOwned(ESimCopterHelicopterTool::WaterBucket,false);
	Other->SetCareerEquipmentOwned(ESimCopterHelicopterTool::WaterBucket,false);
	const int32 BeforeDiscovery=SimCopterHelicopterParking::GetFleetEquipmentMask(World,Career);
	auto* Controller=World->SpawnActor<APlayerController>();
	Apache->EnterHelicopter(Controller,false);
	TestTrue(TEXT("Discovered Apache joins owned fleet"),Career->OwnsHelicopter(2));
	TestEqual(TEXT("Discovered Apache inherits purchases without restoring sold equipment"),Apache->GetEquipmentState().CareerEquipmentMask,BeforeDiscovery);
	return true;
}
#endif

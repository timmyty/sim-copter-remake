#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Game/SimCopterGameMode.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "City/SimCopterAirport.h"
#include "City/SimCopterHangar.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterSaveSubsystem.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SimCopterHangarShop.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCareerAircraftTransferTest,"SimCopter.Career.OnFootAircraftAndCageTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCareerAircraftTransferTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Career=Instance->GetSubsystem<USimCopterCareerSubsystem>(); Career->BeginCareer();
	Career->SetHelicopterOwned(5,true);
	auto SpawnAircraft=[&](const TCHAR* Name)
	{
		FActorSpawnParameters Params; Params.Name=Name;
		return World->SpawnActor<ASimCopterHelicopterPawn>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
	};
	auto* AirportAircraft=SpawnAircraft(TEXT("A_UnrelatedAirportAircraft"));
	auto* Helper=SpawnAircraft(TEXT("B_SupportAircraft")); Helper->GetAirOperations()->SetSupportAircraft(true);
	auto* Selected=SpawnAircraft(TEXT("Z_PurchasedAgusta"));
	TestTrue(TEXT("Selected helicopter model loads"),Selected->SwitchHelicopterModel(5));
	Selected->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,true);
	Selected->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,true);
	const int32 PurchasedMask=Selected->GetEquipmentState().CareerEquipmentMask;
	auto* PC=World->SpawnActor<ASimCopterPlayerController>();
	PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	PC->Possess(Pilot); Pilot->SetParkedHelicopter(Selected);
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.BeginSession(); Missions->MissionSystem.AddCash(3000);
	Missions->CaptureCareerCityTransfer();
	TestEqual(TEXT("Hangar advancement captures the pilot's selected aircraft, not actor name order"),
		Career->GetPendingCityTransfer().ActiveHelicopterTypeIndex,5);
	TestEqual(TEXT("Capture cage and other purchases are captured on foot"),Career->GetPendingCityTransfer().CareerEquipmentMask,PurchasedMask);
	TestEqual(TEXT("Completion cash is preserved"),Career->GetPendingCityTransfer().Cash,4000);
	Selected->Destroy(); AirportAircraft->Destroy(); Helper->Destroy();
	// The next map creates a fresh default pawn, then the real game-mode restore consumes the transfer.
	auto* NextAircraft=SpawnAircraft(TEXT("NextCityDefaultAircraft"));
	Pilot->SetParkedHelicopter(NextAircraft);
	auto* Mode=World->SpawnActor<ASimCopterGameMode>();
	Mode->ApplyPendingAircraftRestores(World);
	TestEqual(TEXT("Next island starts with the selected Agusta"),NextAircraft->GetHelicopterTypeIndex(),5);
	TestEqual(TEXT("Next island retains the complete equipment mask"),NextAircraft->GetEquipmentState().CareerEquipmentMask,PurchasedMask);
	TestTrue(TEXT("Capture cage is available immediately"),NextAircraft->IsToolAvailable(ESimCopterHelicopterTool::CaptureCage));
	TestFalse(TEXT("Successful restoration consumes the transfer"),Career->HasPendingCityTransfer());
	Missions->CaptureCareerCityTransfer();
	TestEqual(TEXT("Equipment also survives a subsequent island transition"),Career->GetPendingCityTransfer().CareerEquipmentMask,PurchasedMask);
	PC->Possess(NextAircraft); Missions->CaptureCareerCityTransfer();
	TestEqual(TEXT("Advancing while seated still selects the flown aircraft"),Career->GetPendingCityTransfer().ActiveHelicopterTypeIndex,5);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterOwnedFleetTransferTest,"SimCopter.Career.OwnedFleetAirportTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterOwnedFleetTransferTest::RunTest(const FString&)
{
	using namespace SimCopterHelicopterParking;
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Career=Instance->GetSubsystem<USimCopterCareerSubsystem>(); Career->BeginCareer();
	Career->EnsurePricesLoaded(SimCopterOriginalGame::ResolveRoot());
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	if (!TestTrue(TEXT("Airport data loads"),Traffic->RebuildSpawnData())) return false;
	FVector FirstPad;
	if (!TestTrue(TEXT("Airport pad exists"),Traffic->TryGetAirportPadWorldLocation(0,FirstPad))) return false;
	auto* Hangar=World->SpawnActor<ASimCopterHangar>();
	if (!TestTrue(TEXT("Hangar placed"),Hangar->PlaceAtAirport(Traffic,FirstPad))) return false;
	auto* Current=World->SpawnActor<ASimCopterHelicopterPawn>();
	Current->SwitchHelicopterModel(4); Current->PlaceOnHelipad(FirstPad,0);
	auto* PC=World->SpawnActor<ASimCopterPlayerController>();
	PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	PC->Possess(Pilot); Pilot->SetParkedHelicopter(Current);
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.BeginSession(); Missions->AddSessionCash(100000);
	SimCopterHangarShop::FContext Shop;
	Shop.Career=Career; Shop.Missions=Missions; Shop.Helicopter=Current; Shop.Hangar=Hangar;
	FString Message;
	if (!TestTrue(TEXT("Buy a Dauphin while Schweizer remains active"),SimCopterHangarShop::BuyHelicopter(Shop,6,Message))) return false;
	TestTrue(TEXT("Buy fleet harness"),SimCopterHangarShop::BuyUpgrade(Shop,4,Message));
	const int32 Cash=Missions->GetSessionCash();
	const int32 Mask=Current->GetEquipmentState().CareerEquipmentMask;
	Missions->CaptureCareerCityTransfer();
	TestEqual(TEXT("Parked purchase does not change active helicopter"),Career->GetPendingCityTransfer().ActiveHelicopterTypeIndex,4);
	for (auto* Aircraft:GetPlayerFleet(World,Career)) Aircraft->Destroy();
	// Recreate the next map's sole default aircraft, then use the production restore path.
	Current=World->SpawnActor<ASimCopterHelicopterPawn>();
	Current->SwitchHelicopterModel(4); Current->PlaceOnHelipad(FirstPad,0); Pilot->SetParkedHelicopter(Current);
	auto* Mode=World->SpawnActor<ASimCopterGameMode>();
	Mode->ApplyPendingAircraftRestores(World);
	TestEqual(TEXT("Both owned helicopters physically arrive in the next city"),GetPlayerFleet(World,Career).Num(),2);
	auto FindType=[&](int32 Type) -> ASimCopterHelicopterPawn*
	{
		for (auto* Aircraft:GetPlayerFleet(World,Career)) if (Aircraft->GetHelicopterTypeIndex()==Type) return Aircraft;
		return nullptr;
	};
	auto* Dauphin=FindType(6);
	TestNotNull(TEXT("The parked Dauphin survives graduation"),Dauphin);
	if (Dauphin)
	{
		TestEqual(TEXT("Dauphin keeps purchased fittings"),Dauphin->GetEquipmentState().CareerEquipmentMask,Mask);
		TestFalse(TEXT("Arriving aircraft have separate footprints"),OverlapsParkedAircraft(Dauphin->GetParkingWorldBounds(),{Current->GetParkingWorldBounds()}));
	}
	TestEqual(TEXT("Restoration never charges the purchase again"),Missions->GetSessionCash(),Cash);
	TestEqual(TEXT("Pilot still selects the active helicopter"),Pilot->GetParkedHelicopter(),Current);
	Mode->ApplyPendingAircraftRestores(World);
	TestEqual(TEXT("Repeated restoration does not duplicate the fleet"),GetPlayerFleet(World,Career).Num(),2);

	// An old save can retain the owned bit but contain no parked-aircraft record at all.
	auto* Saves=Instance->GetSubsystem<USimCopterSaveSubsystem>();
	auto* Saved=NewObject<USimCopterSaveGame>(Instance);
	Saved->bHasAircraftState=true; Saved->ActiveHelicopterTypeIndex=4;
	Saved->CareerEquipmentMask=Mask; Saved->FuelFraction=0.37f; Saved->DamageFraction=0.2f;
	Saves->PendingLoadedGame=Saved; Saves->bPendingMissionStateApplied=true;
	Mode->ApplyPendingAircraftRestores(World);
	TestNotNull(TEXT("Loading an affected save recovers its missing Dauphin"),FindType(6));
	TestTrue(TEXT("Repair preserves saved active fuel"),FMath::IsNearlyEqual(Current->GetFuelFraction(),0.37f));
	TestTrue(TEXT("Repair preserves saved active damage"),FMath::IsNearlyEqual(Current->GetDamageFraction(),0.2f));
	const FTransform ActiveTransform=Current->GetActorTransform();
	Dauphin=FindType(6);
	if (Dauphin)
	{
		const FTransform ParkedTransform=Dauphin->GetActorTransform();
		Mode->ApplyPendingAircraftRestores(World);
		TestTrue(TEXT("Existing parked aircraft stay where saved"),Dauphin->GetActorTransform().Equals(ParkedTransform));
	}
	TestTrue(TEXT("Existing active aircraft stays where saved"),Current->GetActorTransform().Equals(ActiveTransform));
	if (Dauphin) Dauphin->Destroy();
	Career->SetHelicopterOwned(6,false);
	Mode->ApplyPendingAircraftRestores(World);
	TestNull(TEXT("Sold aircraft is never recreated"),FindType(6));

	// The full original nine-model fleet needs corner pads as well as the eight shop pads.
	for (const auto& Definition:SimCopterHelicopterRegistry::GetDefinitions()) Career->SetHelicopterOwned(Definition.InternalTypeIndex,true);
	Mode->ApplyPendingAircraftRestores(World);
	const auto Fleet=GetPlayerFleet(World,Career);
	TestEqual(TEXT("Every owned model fits on the airport"),Fleet.Num(),9);
	TArray<FBox> Occupied;
	for (const auto* Aircraft:Fleet)
	{
		bool bOnPad=false;
		for (int32 Index=0;Index<SimCopterAirport::PadCount;++Index)
		{
			FVector Surface;
			if (Traffic->TryGetAirportPadWorldLocation(Index,Surface)) bOnPad|=FVector::DistSquared2D(Aircraft->GetActorLocation(),Surface)<1;
		}
		TestTrue(TEXT("Owned helicopter stands on an airport pad"),bOnPad);
		if (Aircraft!=Current) TestTrue(TEXT("Restored fleet faces toward base instead of putting tail inside hangar"),
			FVector::DotProduct(Aircraft->GetActorForwardVector(),(Hangar->GetActorLocation()-Aircraft->GetActorLocation()).GetSafeNormal2D())>0.9f);
		TestFalse(TEXT("Full fleet footprints do not overlap"),OverlapsParkedAircraft(Aircraft->GetParkingWorldBounds(),Occupied));
		Occupied.Add(Aircraft->GetParkingWorldBounds());
		TestEqual(TEXT("Every restored model has the purchased equipment"),Aircraft->GetEquipmentState().CareerEquipmentMask,Mask);
	}
	return true;
}
#endif

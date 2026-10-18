#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "City/SimCity2000CityActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Game/SimCopterPlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Game/SimCopterCheats.h"
#include "UObject/UnrealType.h"
#include "Missions/SimCopterMissionSystemActor.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterPlaytestRegressionTest, "SimCopter.PlaytestOctober",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FSimCopterPlaytestRegressionTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* Name : {TEXT("TurboDescent"), TEXT("CollisionSweep"), TEXT("ReverseTaxi"),
		TEXT("HangarRecovery"), TEXT("CityOptionsAndCash"), TEXT("PatientFarewell"), TEXT("BandFacing")})
	{ Names.Add(Name); Commands.Add(Name); }
}
bool FSimCopterPlaytestRegressionTest::RunTest(const FString& Case)
{
	using namespace SimCopterFixed;
	using namespace SimCopterMissions;
	if (Case == TEXT("TurboDescent"))
	{
		for (float Dt : {1.f/120, 1.f/60, 1.f/30, .1f})
		{
			FSimCopterFlightModel Normal;
			Normal.ResetOnSurface(0,0,0); Normal.State=ESimCopterFlightState::Flying;
			Normal.Altitude=FromFloat(500); Normal.RotorSpeed=FSimCopterFlightModel::RotorTopSpeed;
			auto Turbo=Normal;
			FSimCopterFlightInputs Input; Input.ClimbCommand=-1;
			FSimCopterFlightEnvironment Env; Env.bTerrainFlat=true;
			FSimCopterFlightEvents Events;
			for (int32 Frame=0; Frame<int32(2/Dt); ++Frame)
			{
				Input.bTurbo=false; Normal.Step(Dt,Input,Env,Events);
				Input.bTurbo=true; Turbo.Step(Dt,Input,Env,Events);
				TestEqual(TEXT("Turbo descent has ordinary controlled speed"),Turbo.ClimbSpeed,Normal.ClimbSpeed);
				TestTrue(TEXT("Descent never reverses its sign"),Turbo.ClimbSpeed<=0);
			}
			TestEqual(TEXT("Frame-rate-independent descent displacement matches"),Turbo.Altitude,Normal.Altitude);
		}
		return true;
	}
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	if (Case == TEXT("CityOptionsAndCash"))
	{
		auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
		Missions->MissionSystem.Initialize(nullptr,42); Missions->MissionSystem.BeginSession();
		Missions->SessionMode=ESimCopterMissionSessionMode::UserCityJobs;
		const int32 OriginalCountdown=Missions->MissionSystem.SpawnCountdown;
		Missions->SetCustomCityOptions(4,2);
		TestEqual(TEXT("Twice the mission frequency halves the pending wait"),Missions->MissionSystem.SpawnCountdown,OriginalCountdown/2);
		TestEqual(TEXT("Future spawn intervals also halve"),Missions->MissionSystem.EasyIntervalCache,Missions->MissionSystem.Tuning.EasyInterval/2);
		TestEqual(TEXT("Manual progress permits stage four at zero score"),Missions->GetCustomCityProgressionTier(),4);
		TestEqual(TEXT("Progress does not grant earned points"),Missions->GetSessionScore(),0);
		TestEqual(TEXT("Frequency reaches scheduler"),Missions->MissionSystem.MissionFrequency,2.f);
		const int32 Cash=Missions->GetSessionCash();
		for(int32 I=0;I<60;++I) Missions->MissionSystem.PostEvent(EVT_SpeederPursuit,-1,1);
		Missions->MissionSystem.Tick(0);
		TestEqual(TEXT("Repeated waiting-for-cops events never mint cash"),Missions->GetSessionCash(),Cash);
		TArray<uint8> Save;
		TestTrue(TEXT("Custom settings save"),Missions->CaptureRuntimeSaveState(Save));
		Missions->SetCustomCityOptions(1,.25f);
		TestTrue(TEXT("Custom settings restore"),Missions->RestoreRuntimeSaveState(Save));
		TestEqual(TEXT("Manual progress persists"),Missions->GetCustomCityProgressionTier(),4);
		TestEqual(TEXT("Frequency persists"),Missions->MissionSystem.MissionFrequency,2.f);
		// v2 ends with the score; v3 appends only the tier/frequency options.
		Save.SetNum(Save.Num()-sizeof(int32)-sizeof(float));
		int32 V2=2; FMemory::Memcpy(Save.GetData()+sizeof(uint32),&V2,sizeof(V2));
		TestTrue(TEXT("Real v2 layout remains readable"),Missions->RestoreRuntimeSaveState(Save));
		TestEqual(TEXT("Old saves retain default frequency"),Missions->MissionSystem.MissionFrequency,1.f);
		return true;
	}
	if (Case == TEXT("PatientFarewell") || Case == TEXT("BandFacing"))
	{
		auto* Person=World->SpawnActor<ASimCopterGroundAgent>();
		if (Case == TEXT("BandFacing"))
		{
			auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>(); Person->SetOwner(Traffic);
			Person->BehaviorContext.ResetToState(17);
			Person->SetBandFormationTarget(Person->GetActorLocation(),37,true);
			Person->SetActorRotation(FRotator(0,37,0));
			for(int32 Facing=0;Facing<8;++Facing)
			{
				Person->BehaviorContext.Attributes[EBhavAttr::Facing]=Facing;
				Person->ApplyBehaviorFacingRotation();
				TestTrue(TEXT("VM cannot snap a formation musician from side to side"),FMath::IsNearlyEqual(Person->GetActorRotation().Yaw,37.0));
			}
			return true;
		}
		Person->InitialPersonState=6; Person->SetMissionInjuredPose(); Person->SetMissionResolutionReported(true);
		Person->ResumeDeliveredPatientBehavior(); Person->Tick(1);
		TestEqual(TEXT("Accepted living patient acknowledges rescue"),Person->ForcedFigureMnemonic,FString(TEXT("Whoa")));
		TestFalse(TEXT("Patient stays visible during farewell"),Person->IsHidden());
		TArray<uint8> Save; TestTrue(TEXT("Farewell can be saved"),Person->CaptureRuntimeSaveState(Save));
		TestTrue(TEXT("Farewell restores"),Person->RestoreRuntimeSaveState(Save));
		TestEqual(TEXT("Remaining farewell duration survives load"),Person->DeliveredPatientCelebrationSeconds,5.f);
		Person->Tick(1); Person->Tick(1);
		TestEqual(TEXT("Patient waves after acknowledgement"),Person->ForcedFigureMnemonic,FString(TEXT("WvNo")));
		Person->Tick(3.1f);
		TestTrue(TEXT("Patient leaves after farewell"),Person->IsActorBeingDestroyed());
		return true;
	}
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	TestTrue(TEXT("Aircraft loads"),Heli->SwitchHelicopterModel(4));
	if (Case == TEXT("CollisionSweep"))
	{
		auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(15,1000,1000));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
		Wall->SetActorLocation(FVector(800,0,1000));
		Heli->SetActorLocation(FVector(0,0,1000)); Heli->SeedFlightModelFromActor(); Heli->FlightModel.bCheatInvulnerable=true;
		for(float Displacement : {1000.f,20000.f,40000.f})
		{
			Heli->FlightModel.PosZ=FromFloat(Displacement/Heli->OriginalUnitToCm);
			Heli->ApplyFlightModelToActor(.1f);
			TestTrue(TEXT("Full high-speed sweep stops before thin building wall"),Heli->GetActorLocation().X<800-15);
			TestTrue(TEXT("Fixed-point model commits collision result"),FMath::IsNearlyEqual(ToFloat(Heli->FlightModel.PosZ)*Heli->OriginalUnitToCm,float(Heli->GetActorLocation().X),.1f));
		}
		Heli->FlightModel.PosZ=FromFloat(-300/Heli->OriginalUnitToCm); Heli->ApplyFlightModelToActor(.1f);
		TestTrue(TEXT("Aircraft can move away after contact"),Heli->GetActorLocation().X<-290);
		return true;
	}
	if (Case == TEXT("ReverseTaxi"))
	{
		Heli->SwitchHelicopterModel(SimCopterHelicopterRegistry::PlaneTypeIndex);
		Heli->PlaceOnHelipad(FVector::ZeroVector,0); Heli->bEngineRunning=true;
		Heli->PlaneThrottle=.3f; Heli->bEngineShutdownHeld=true;
		FSimCopterFlightInputs Brake; Brake.ClimbCommand=-1;
		FSimCopterFlightEnvironment Env; Env.bTerrainFlat=true;
		for(int32 I=0;I<100;++I) { Heli->UpdateEngineState(.1f); Heli->StepFixedWing(.1f,Brake,Env); }
		TestTrue(TEXT("Holding brake reaches controlled reverse taxi"),Heli->FlightModel.ForwardSpeed<0);
		TestTrue(TEXT("Brake does not kill reverse engine"),Heli->bEngineRunning);
		Brake.ClimbCommand=0; Heli->StepFixedWing(1,Brake,Env);
		TestEqual(TEXT("Releasing brake stops reverse"),Heli->FlightModel.ForwardSpeed,0);
		Heli->FlightModel.State=ESimCopterFlightState::Flying; Heli->FlightModel.Altitude=FromFloat(500);
		Brake.ClimbCommand=-1; Heli->StepFixedWing(1,Brake,Env);
		TestTrue(TEXT("Airborne plane never reverses"),Heli->FlightModel.ForwardSpeed>0);
		return true;
	}
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	if(!TestTrue(TEXT("Airport pad data loads"),Traffic->RebuildSpawnData())) return false;
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	FindFProperty<FBoolProperty>(City->GetClass(),TEXT("bLoadOnConstruction"))->SetPropertyValue_InContainer(City,false);
	City->FinishSpawning(FTransform::Identity);
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	PC->PlayerInput=NewObject<UPlayerInput>(PC);
	PC->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>(); PC->PlayerCameraManager->InitializeFor(PC); PC->Possess(Heli);
	Heli->SetActorLocation(FVector(1000,1000,3000)); Heli->SeedFlightModelFromActor();
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,true);
	Heli->FlightModel.Fuel=1; Heli->FlightModel.HitPoints=1;
	const auto Command=SimCopterCheats::Parse(TEXT("Respawn helicopter"));
	TestTrue(TEXT("Hangar respawn cheat parses"),Command.Num()==1 && Command[0].Code==SimCopterCheats::ECode::RespawnHelicopter);
	TestTrue(TEXT("Aircraft recalls to free hangar pad"),Heli->RespawnParkedAtHangar());
	TestEqual(TEXT("Recalled aircraft is parked"),Heli->FlightModel.State,ESimCopterFlightState::Parked);
	TestFalse(TEXT("Recalled engine is off"),Heli->bEngineRunning);
	TestTrue(TEXT("Recalled fuel and health restored"),Heli->FlightModel.Fuel>1 && Heli->FlightModel.HitPoints>1);
	TestTrue(TEXT("Owned cage survives recall"),Heli->IsToolAvailable(ESimCopterHelicopterTool::CaptureCage));
	// This isolated world has airport coordinates but no rendered city collision.
	auto* Pad=World->SpawnActor<AActor>(); auto* PadFloor=NewObject<UBoxComponent>(Pad);
	Pad->SetRootComponent(PadFloor); PadFloor->SetBoxExtent(FVector(1000,1000,10));
	PadFloor->SetCollisionResponseToAllChannels(ECR_Block); PadFloor->RegisterComponent();
	Pad->SetActorLocation(Heli->GetActorLocation()-FVector(0,0,Heli->GetHelipadRestingOriginOffsetCm()+10));
	Heli->ExitHelicopter(); auto* Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Pilot exits recalled aircraft"),Pilot)) return false;
	Pilot->SetActorLocation(FVector(0,0,-6000)); PC->PlayerTick(.1f);
	TestEqual(TEXT("Below-world pilot returns inside aircraft"),PC->GetPawn().Get(),static_cast<APawn*>(Heli));
	TestTrue(TEXT("Recovered aircraft is above map floor"),Heli->GetActorLocation().Z>0);
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flight/SimCopterFlightModel.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterTravelBoostTest, "SimCopter.Flight.TravelBoostAndTurbo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterTravelBoostTest::RunTest(const FString&)
{
	using namespace SimCopterFixed;
	for (const bool Easy : {false,true})
	for (const float Step : {.025f,.1f})
	{
		FSimCopterFlightModel Normal;
		Normal.ResetOnSurface(0,0,0);
		Normal.State=ESimCopterFlightState::Flying; Normal.Altitude=FromFloat(500);
		Normal.RotorSpeed=FSimCopterFlightModel::RotorTopSpeed;
		Normal.PitchTarget=Normal.PitchSmoothed=Normal.ForwardSpeed=FromFloat(100);
		Normal.bEasyFlightModel=Easy;
		auto Boost=Normal, Cheat=Normal, Both=Normal;
		FSimCopterFlightInputs Cruise, Shift, Turbo, Combined;
		Shift.bTravelBoost=true; Turbo.bTurbo=true; Combined.bTravelBoost=Combined.bTurbo=true;
		FSimCopterFlightEnvironment Env; Env.bTerrainFlat=true;
		FSimCopterFlightEvents Events;
		for (int32 Frame=0;Frame<60;++Frame)
		{
			Normal.Step(Step,Cruise,Env,Events); Boost.Step(Step,Shift,Env,Events);
			Cheat.Step(Step,Turbo,Env,Events); Both.Step(Step,Combined,Env,Events);
			TestTrue(TEXT("Normal Shift is exactly 30 percent faster without compounding"),
				FMath::IsNearlyEqual(ToFloat(Boost.HorizontalSpeed),ToFloat(Normal.HorizontalSpeed)*1.3f,.01f));
			TestEqual(TEXT("Shift leaves climb and descent unchanged"),Boost.ClimbSpeed,Normal.ClimbSpeed);
			TestEqual(TEXT("Cheat speed stays identical even if both input flags are set"),Both.HorizontalSpeed,Cheat.HorizontalSpeed);
			TestEqual(TEXT("Cheat climb stays identical"),Both.ClimbSpeed,Cheat.ClimbSpeed);
		}
		TestTrue(TEXT("Travel boost displays a steeper nose angle"),Boost.DisplayPitchTenthDeg()>Normal.DisplayPitchTenthDeg());
		TestEqual(TEXT("Cheat attitude remains unchanged"),Both.DisplayPitchTenthDeg(),Cheat.DisplayPitchTenthDeg());
		Boost.Step(Step,Cruise,Env,Events); Normal.Step(Step,Cruise,Env,Events);
		TestEqual(TEXT("Releasing Shift restores ordinary speed"),Boost.HorizontalSpeed,Normal.HorizontalSpeed);
		TestEqual(TEXT("Releasing Shift restores ordinary tilt"),Boost.DisplayPitchTenthDeg(),Normal.DisplayPitchTenthDeg());
		Boost.ResetOnSurface(0,0,0); Boost.Step(Step,Shift,Env,Events);
		TestFalse(TEXT("Parked rotor cannot enable travel boost"),Boost.bTravelBoostActive);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterHelperCruiseTest, "SimCopter.AirOperations.HelperCruisePacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterHelperCruiseTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	const FVector Start(0,0,5000); Heli->SetActorLocation(Start);
	auto* Ops=Heli->GetAirOperations(); Ops->SetSupportAircraft(true); Ops->SetSupportGoal(Start+FVector(5000,0,0));
	for (int32 Frame=0;Frame<10;++Frame) Ops->Update(.1f);
	TestTrue(TEXT("Actual helper update cruises at 600 cm per second"),FMath::IsNearlyEqual(float(Heli->GetActorLocation().X),600.f,.1f));
	TestTrue(TEXT("Cruise keeps target altitude"),FMath::IsNearlyEqual(float(Heli->GetActorLocation().Z),5000.f,.1f));
	return true;
}
#endif

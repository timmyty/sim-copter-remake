#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flight/SimCopterFlightModel.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterBaseTravelSpeedTest, "SimCopter.Flight.BaseTravelSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterBaseTravelSpeedTest::RunTest(const FString&)
{
	using namespace SimCopterFixed;
	const int32 GameplayScale=GetDefault<ASimCopterHelicopterPawn>()->GetFlightModel().BaseTravelScale;
	TestTrue(TEXT("New and restored gameplay pawns default to 1.3x travel"),FMath::IsNearlyEqual(ToFloat(GameplayScale),1.3f,.0001f));
	for (bool Easy : {false,true}) for (float Step : {.025f,.1f}) for (float Heading : {0.0f,450.0f,1800.0f})
	{
		FSimCopterFlightModel Reference;
		Reference.ResetOnSurface(0,0,0); Reference.State=ESimCopterFlightState::Flying;
		Reference.Altitude=FromFloat(500); Reference.RotorSpeed=FSimCopterFlightModel::RotorTopSpeed;
		Reference.PitchTarget=Reference.PitchSmoothed=Reference.ForwardSpeed=FromFloat(100);
		Reference.SlideTarget=Reference.SlideSmoothed=FromFloat(30);
		Reference.Heading=FromFloat(Heading); Reference.bEasyFlightModel=Easy;
		auto Normal=Reference,Boost=Reference,Turbo=Reference,OldTurbo=Reference;
		Normal.BaseTravelScale=Boost.BaseTravelScale=Turbo.BaseTravelScale=GameplayScale;
		FSimCopterFlightEnvironment Env; Env.bTerrainFlat=true;
		FSimCopterFlightEvents Events;
		FSimCopterFlightInputs Cruise,Shift,Cheat; Shift.bTravelBoost=true; Cheat.bTurbo=true;
		for (int32 Frame=0; Frame<60; ++Frame)
		{
			Reference.Step(Step,Cruise,Env,Events); Normal.Step(Step,Cruise,Env,Events);
			Boost.Step(Step,Shift,Env,Events); Turbo.Step(Step,Cheat,Env,Events); OldTurbo.Step(Step,Cheat,Env,Events);
			TestTrue(TEXT("Normal travel is 30% faster in any direction without accumulating"),FMath::IsNearlyEqual(ToFloat(Normal.HorizontalSpeed),ToFloat(Reference.HorizontalSpeed)*1.3f,.01f));
			TestTrue(TEXT("Normal Shift remains another 30% over the new base"),FMath::IsNearlyEqual(ToFloat(Boost.HorizontalSpeed),ToFloat(Reference.HorizontalSpeed)*1.69f,.02f));
			TestEqual(TEXT("Climb is unchanged"),Normal.ClimbSpeed,Reference.ClimbSpeed);
			TestEqual(TEXT("Attitude chase is unchanged"),Normal.ForwardSpeed,Reference.ForwardSpeed);
			TestEqual(TEXT("Turbo retains its previous absolute speed"),Turbo.HorizontalSpeed,OldTurbo.HorizontalSpeed);
		}
		TestTrue(TEXT("World displacement includes the base increase"),FMath::IsNearlyEqual(ToFloat(Normal.PosX),ToFloat(Reference.PosX)*1.3f,.03f) && FMath::IsNearlyEqual(ToFloat(Normal.PosZ),ToFloat(Reference.PosZ)*1.3f,.03f));
		Boost.Step(Step,Cruise,Env,Events); Normal.Step(Step,Cruise,Env,Events);
		TestEqual(TEXT("Shift release returns to the new normal speed"),Boost.HorizontalSpeed,Normal.HorizontalSpeed);
		Normal.ResetOnSurface(0,0,0); Normal.Step(Step,Cruise,Env,Events);
		TestEqual(TEXT("Parked helicopter stays still"),Normal.HorizontalSpeed,0);
	}
	return true;
}

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

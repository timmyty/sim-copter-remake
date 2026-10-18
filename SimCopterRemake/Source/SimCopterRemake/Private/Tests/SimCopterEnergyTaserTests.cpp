#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Ground/SimCopterEnergyTaser.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterPilotEquipment.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Missions/SimCopterMissionSystemActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterEnergyTaserChargeTest,"SimCopter.EnergyTaser.ChargeGeometryAndAudio",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterEnergyTaserChargeTest::RunTest(const FString&)
{
	SimCopterEnergyTaser::FCharge Charge;
	Charge.Update(60,false); TestEqual(TEXT("No enemy never forms a ball"),Charge.Energy,0.f);
	Charge.Update(.25f,true); TestTrue(TEXT("Charging visibly progresses"),Charge.Energy>0 && Charge.Energy<1);
	TestFalse(TEXT("An unfinished ball cannot fire"),Charge.CanFire());
	Charge.Update(.5f,true); TestTrue(TEXT("A completed ball can be clicked to fire"),Charge.CanFire());
	Charge.Update(0,false); TestEqual(TEXT("Losing the criminal immediately removes energy"),Charge.Energy,0.f);
	TestEqual(TEXT("Losing the criminal closes the emitters"),Charge.Deployment,0.f);
	TestFalse(TEXT("A targetless weapon cannot fire"),Charge.CanFire());
	Charge.Update(.01f,true); TestFalse(TEXT("Reacquiring requires a new charge"),Charge.CanFire());
	Charge.Update(.7f,true); TestTrue(TEXT("Holding the sight on a criminal powers up again"),Charge.CanFire());
	Charge.Update(.7f,true); Charge.Consume(); TestFalse(TEXT("A released ball is consumed"),Charge.CanFire());
	FMaxisMeshSection Closed,Open,Dark,Lit,Orb;
	SimCopterPilotEquipment::BuildTaser(Closed); SimCopterPilotEquipment::BuildTaser(Open,1);
	TestTrue(TEXT("Emitter physically opens sideways"),Open.LocalBounds.GetSize().Y>Closed.LocalBounds.GetSize().Y*1.8f);
	TestTrue(TEXT("Normal receiver has detailed redesign geometry"),Closed.Vertices.Num()>500);
	SimCopterPilotEquipment::BuildTaserCircuits(Dark,0,0,0); SimCopterPilotEquipment::BuildTaserCircuits(Lit,1,1,0);
	TestTrue(TEXT("Lit blue-green circuit color differs from standby"),Lit.VertexColors[0].G>Dark.VertexColors[0].G*5 && Lit.VertexColors[0].B>Lit.VertexColors[0].R*5);
	SimCopterPilotEquipment::BuildTaserOrb(Orb,0,0,0); TestTrue(TEXT("No uncharged orb geometry"),Orb.Vertices.IsEmpty());
	TArray<uint32> Hashes;
	for (auto Kind:{ESimCopterTaserSound::Charge,ESimCopterTaserSound::Release,ESimCopterTaserSound::Seek,ESimCopterTaserSound::Impact})
	{
		const auto Clip=USimCopterAudioSubsystem::BuildTaserSound(Kind);
		TestTrue(TEXT("Each sound contains valid mono PCM"),Clip.IsValid() && Clip.Channels==1 && Clip.Pcm16.Num()==FMath::RoundToInt(Clip.Duration*Clip.SampleRate)*2);
		int64 Power=0; int32 Peak=0;
		for (int32 I=0;I<Clip.Pcm16.Num();I+=2) { int16 V; FMemory::Memcpy(&V,Clip.Pcm16.GetData()+I,2); Power+=int64(V)*V; Peak=FMath::Max(Peak,FMath::Abs(int32(V))); }
		TestTrue(TEXT("Sound has audible signal without clipped PCM"),Power/Clip.Pcm16.Num()>100000 && Peak<30000);
		const uint32 Hash=FCrc::MemCrc32(Clip.Pcm16.GetData(),Clip.Pcm16.Num());
		TestFalse(TEXT("Charge release seek and impact have distinct waveforms"),Hashes.Contains(Hash)); Hashes.Add(Hash);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterEnergyTaserTest,"SimCopter.EnergyTaser.Runtime",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterEnergyTaserTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>(); Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay=false;
	Pilot->SetActorLocation(FVector(0,0,24));
	Pilot->CameraComponent->SetWorldLocationAndRotation(FVector(0,0,24),FRotator::ZeroRotator);
	Pilot->StartTaserAim(); Pilot->UpdateEnergyTaser(1);
	const auto OrbCount=[&]() { int32 Count=0; for (TActorIterator<ASimCopterTaserOrb> It(World);It;++It) ++Count; return Count; };
	Pilot->FireTaser(); TestEqual(TEXT("Clicking empty space cannot invent a ball"),OrbCount(),0);
	const auto Person=[&](bool bCriminal,FVector Position)
	{
		auto* P=World->SpawnActor<ASimCopterGroundAgent>(); P->InitialPersonState=bCriminal ? 13 : 1;
		P->BehaviorContext.ResetToState(P->InitialPersonState); P->SetActorLocation(Position); P->bSnapToGround=false;
		P->BehaviorContext.Attributes[EBhavAttr::MedevacHealth]=100;
		return P;
	};
	auto* Civilian=Person(false,FVector(400,32.5,24)); Pilot->UpdateEnergyTaser(1);
	TestEqual(TEXT("A civilian never charges the weapon"),Pilot->TaserCharge.Energy,0.f);
	auto* Suspect=Person(true,FVector(800,65,24));
	TestNull(TEXT("Civilian in the firing line blocks acquisition"),SimCopterEnergyTaser::FindTarget(World,Pilot,FVector(0,0,24),FVector(800,65,0)));
	Civilian->SetActorLocation(FVector(400,400,24));
	Pilot->UpdateEnergyTaser(.3f); TestTrue(TEXT("Near crosshair suspect begins the real weapon charge"),Pilot->TaserCharge.Energy>0);
	Pilot->FireTaser(); TestEqual(TEXT("Click during formation cannot shoot"),OrbCount(),0);
	Pilot->UpdateEnergyTaser(.4f); TestEqual(TEXT("Aiming alone never launches the charged ball"),OrbCount(),0);
	TestTrue(TEXT("Charged orb is drawn on the actual weapon"),Pilot->TaserMesh->GetProcMeshSection(2)!=nullptr);
	Pilot->FireTaser(); TestEqual(TEXT("One click launches exactly one ball"),OrbCount(),1);
	TestFalse(TEXT("Damage is deferred until physical contact"),Suspect->IsTaserStunned());
	Pilot->FireTaser(); TestEqual(TEXT("Repeated clicks respect recharge"),OrbCount(),1);
	ASimCopterTaserOrb* Shot=nullptr; for (TActorIterator<ASimCopterTaserOrb> It(World);It;++It) Shot=*It;
	if (!TestNotNull(TEXT("Traveling orb exists"),Shot)) return false;
	Pilot->StopTaserAim();
	for (int32 Frame=0;Frame<200 && !Shot->HasImpacted();++Frame)
	{
		Suspect->AddActorWorldOffset(FVector(0,1,0)); Shot->Tick(.01f);
	}
	TestTrue(TEXT("Orb seeks a moving suspect after the weapon is lowered"),Suspect->IsTaserStunned());
	TestFalse(TEXT("Orb never creates a dead person or medical patient"),Suspect->IsMissionPatientDead() || Suspect->IsMedevacVictim());
	TestEqual(TEXT("Orb preserves health"),Suspect->GetBehaviorAttribute(EBhavAttr::MedevacHealth),100);
	Suspect->TickAirOperations(3);
	TArray<uint8> Saved; TestTrue(TEXT("Timed stun saves"),Suspect->CaptureRuntimeSaveState(Saved));
	auto* Restored=Person(true,FVector(0,1000,24)); TestTrue(TEXT("Timed stun restores"),Restored->RestoreRuntimeSaveState(Saved));
	TestEqual(TEXT("Remaining stun duration survives save/load"),Restored->TaserStunSeconds,9.f);
	Restored->TickAirOperations(8.9f); TestTrue(TEXT("Suspect remains stunned before deadline"),Restored->IsTaserStunned());
	Restored->TickAirOperations(.2f); TestFalse(TEXT("Uncollected suspect recovers automatically"),Restored->IsInPoliceCustody());
	Restored->Destroy();
	TArray<uint8> Legacy=Saved; Legacy.SetNum(Legacy.Num()-3*sizeof(float)-2); // v11 stun/flags, v12 megaphone hold, v13 patient farewell
	const int32 Version=10; FMemory::Memcpy(Legacy.GetData()+sizeof(uint32),&Version,4);
	Restored=Person(true,FVector(0,1200,24)); TestTrue(TEXT("Previous version 10 remains readable"),Restored->RestoreRuntimeSaveState(Legacy)); Restored->Destroy();
	auto* Carrier=World->SpawnActor<ASimCopterHelicopterPawn>(); Carrier->SetActorLocation(FVector(5000,5000,500));
	TestTrue(TEXT("Collecting a stunned suspect still handcuffs them"),Suspect->BoardCarrier(Carrier,false,true));
	Suspect->TickAirOperations(30); TestTrue(TEXT("Timed recovery never frees a handcuffed passenger"),Suspect->IsHandcuffed());
	Suspect->Destroy(); Shot->Destroy();
	// The camera can see over this obstacle, but the actual muzzle cannot shoot through it.
	auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10,150,120)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Wall->SetActorLocation(FVector(450,0,24));
	Suspect=Person(true,FVector(800,0,24));
	Pilot->TaserCharge={}; Pilot->TaserCooldown=0; Pilot->StartTaserAim(); Pilot->UpdateEnergyTaser(1);
	TestEqual(TEXT("A criminal through a wall cannot activate the taser"),Pilot->TaserCharge.Energy,0.f);
	Shot=World->SpawnActor<ASimCopterTaserOrb>(); Shot->Launch(Pilot,Suspect,FVector(0,0,24),FVector::ForwardVector);
	for (int32 Frame=0;Frame<100 && !Shot->HasImpacted();++Frame) Shot->Tick(.01f);
	TestTrue(TEXT("Traveling orb stops at world geometry"),Shot->HasImpacted() && Shot->GetActorLocation().X<460);
	TestFalse(TEXT("Blocked orb cannot stun through the wall"),Suspect->IsTaserStunned());
	Shot->Destroy(); Wall->Destroy();
	Shot=World->SpawnActor<ASimCopterTaserOrb>(); Shot->Launch(Pilot,Suspect,FVector(0,0,24),FVector::ForwardVector);
	Suspect->Destroy(); Shot->Tick(.01f); TestTrue(TEXT("Deleted target safely dissipates the shot"),Shot->HasImpacted()); Shot->Destroy();
	auto* Car=World->SpawnActor<ASimCopterGroundAgent>(); Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),600);
	Car->SetActorLocation(FVector(900,0,24)); Car->bSnapToGround=false;
	Pilot->CameraComponent->SetWorldRotation((SimCopterEnergyTaser::TargetPoint(Car)-Pilot->CameraComponent->GetComponentLocation()).Rotation());
	Pilot->TaserCooldown=0; Pilot->StartTaserAim(); Pilot->UpdateEnergyTaser(.7f);
	TestFalse(TEXT("Ordinary traffic cannot power up the handheld taser"),Pilot->TaserCharge.CanFire());
	Car->MakeCriminalCar(42,0); Pilot->UpdateEnergyTaser(.7f);
	TestTrue(TEXT("An active criminal vehicle powers up the handheld taser"),Pilot->TaserCharge.CanFire());
	TestTrue(TEXT("Active criminal car is eligible"),SimCopterEnergyTaser::IsTarget(Car));
	Shot=World->SpawnActor<ASimCopterTaserOrb>(); Shot->Launch(Pilot,Car,FVector(0,0,24),(SimCopterEnergyTaser::TargetPoint(Car)-FVector(0,0,24)).GetSafeNormal());
	for (int32 Frame=0;Frame<200 && !Shot->HasImpacted();++Frame) Shot->Tick(.01f);
	TestTrue(TEXT("Energy impact temporarily stops a criminal car"),Car->IsVehicleTaserDisabled());
	Car->TickAirOperations(8.1f); TestTrue(TEXT("Energy taser holds vehicle past the former eight seconds"),Car->IsVehicleTaserDisabled());
	Car->TickAirOperations(8.0f); TestFalse(TEXT("Criminal car recovers without damage or a tow mission"),Car->IsVehicleImmobilized() || Car->GetTowMissionId()!=INDEX_NONE);
	Car->Destroy(); Shot->Destroy();
	Car=World->SpawnActor<ASimCopterGroundAgent>(); Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),600); Car->SetFleeing(true);
	TestFalse(TEXT("Fleeing alone does not make civilian traffic an enemy"),SimCopterEnergyTaser::IsTarget(Car));
	Car->MakeCriminalCar(42,0); World->SpawnActor<ASimCopterMissionSystemActor>();
	TestFalse(TEXT("Stale pooled criminal car with no active event is ignored"),SimCopterEnergyTaser::IsTarget(Car));
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "City/SimCity2000CityActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterInteraction.h"
#include "Ground/SimCopterCriminalCar.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Formats/SimCopterPeopleReader.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMegaphoneEffectsTest, "SimCopter.Interaction.MegaphoneEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterMegaphoneEffectsTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->FinishSpawning(FTransform::Identity);
	City->WaterGameplayCornerZ.Init(0,129*129); City->WaterGameplayTerrainClasses.Init(10,128*128);
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>(); Traffic->SourceCityActor=City;
	Traffic->PeopleTileClasses.Init(7,128*128); Traffic->TileCenterWorldZ.Init(0,128*128);
	Traffic->WaterTileFlags.Init(0,128*128); Traffic->XbldTileIds.Init(0x1d,128*128); Traffic->ActiveTileSize=400;
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>(); Heli->SetActorLocation(FVector(200,-200,1200));
	auto* PC = World->SpawnActor<APlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine);
	World->AddController(PC); PC->Possess(Heli);
	Heli->SpotlightTarget.bValid=true; Heli->SpotlightTarget.Band=0;
	Heli->SpotlightTarget.WorldLocation=FVector(200,-200,0);
	Traffic->SetSpotlightMarkSource(Heli->SpotlightTarget.WorldLocation,0,true);
	Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Heli->GetActorLocation(),Heli->SpotlightTarget.Tile.X,Heli->SpotlightTarget.Tile.Y);
	auto Model=MakeShared<FPeopleBehaviorModel>(); FString Error;
	if (!TestTrue(TEXT("Original behavior data loads"),FSimCopterPeopleReader::LoadFromFile(
		FSimCopterPeopleReader::ResolvePeoplePath(SimCopterOriginalGame::ResolveRoot()),*Model,Error))) return false;
	auto* Person=World->SpawnActor<ASimCopterGroundAgent>(); Person->SetOwner(Traffic);
	Person->BehaviorModel=Model; Person->bBehaviorActive=true; Person->SetActorLocation(FVector(230,-200,40));
	FSimCopterInteractionEvent Event; Event.Source=Heli; Event.Mode=ESimCopterInteractionMode::Megaphone; Event.MessageIndex=1;
	for (int32 State : {0,10,11,12,13})
	{
		Person->BehaviorContext=FSimCopterPersonContext(); Person->BehaviorContext.ResetToState(State);
		TestTrue(TEXT("Civilian and criminal accept Stop Criminal"),Person->ApplyInteraction(Event));
		FSimCopterBehaviorVM::Tick(Person->BehaviorContext,*Model,*Person);
		TestEqual(TEXT("Original message branch plays Whoa"),Person->BehaviorContext.PendingAnimMnemonic,FString(TEXT("Whoa")));
		TestEqual(TEXT("Original message branch waits in Idle-80"),Person->BehaviorContext.Stack.Last().ProgramId,1104);
		TestFalse(TEXT("Megaphone does not arrest the person"),Person->IsHandcuffed());
	}
	// Prove the same live event reaches the VM after a save, and that the original same-tile
	// condition still applies. An older save stores the message separately from its attributes.
	Person->BehaviorContext=FSimCopterPersonContext(); Person->BehaviorContext.ResetToState(10);
	Person->ApplyInteraction(Event);
	Person->BehaviorContext.Attributes[13]=0; // Mirror the broken attribute block in old saves.
	TArray<uint8> Saved; TestTrue(TEXT("Pending reaction saves"),Person->CaptureRuntimeSaveState(Saved));
	// v12 appends the vehicle stop timer and v13 the patient farewell timer.
	// Remove both suffixes to construct a real v11 payload.
	TArray<uint8> Legacy=Saved; Legacy.SetNum(Legacy.Num()-2*sizeof(float));
	int32 LegacyVersion=11; FMemory::Memcpy(Legacy.GetData()+sizeof(uint32),&LegacyVersion,sizeof(LegacyVersion));
	TestTrue(TEXT("Version-11 pending reaction still loads"),Person->RestoreRuntimeSaveState(Legacy));
	TestEqual(TEXT("Old save repairs the attribute used by the shipped script"),int32(Person->BehaviorContext.Attributes[13]),1);
	TestTrue(TEXT("Pending reaction restores"),Person->RestoreRuntimeSaveState(Saved));
	FSimCopterBehaviorVM::Tick(Person->BehaviorContext,*Model,*Person);
	TestEqual(TEXT("Restored reaction retains Stop Criminal"),Person->BehaviorContext.PendingAnimMnemonic,FString(TEXT("Whoa")));
	Person->BehaviorContext=FSimCopterPersonContext(); Person->BehaviorContext.ResetToState(10);
	Person->SetActorLocation(FVector(650,-200,40)); Person->ApplyInteraction(Event);
	FSimCopterBehaviorVM::Tick(Person->BehaviorContext,*Model,*Person);
	TestNotEqual(TEXT("Adjacent tile does not get the special Whoa pause"),Person->BehaviorContext.PendingAnimMnemonic,FString(TEXT("Whoa")));
	Person->SetActorLocation(FVector(230,-200,40));
	Person->BehaviorContext=FSimCopterPersonContext(); Person->BehaviorContext.ResetToState(10);
	Heli->SelectedMegaphoneMessage=ESimCopterMegaphoneMessage::StopCriminal;
	Heli->BroadcastMegaphoneMessage(); FSimCopterBehaviorVM::Tick(Person->BehaviorContext,*Model,*Person);
	TestEqual(TEXT("Actual helicopter broadcast reaches the original Whoa reaction"),Person->BehaviorContext.PendingAnimMnemonic,FString(TEXT("Whoa")));
	Person->BehaviorContext=FSimCopterPersonContext(); Person->BehaviorContext.ResetToState(10);
	Heli->SpotlightTarget.Band=3; Heli->BroadcastMegaphoneMessage();
	TestEqual(TEXT("Out-of-range broadcast does not interrupt the person"),Person->BehaviorContext.LastReactionProgramId,INDEX_NONE);
	Heli->SpotlightTarget.Band=0;
	for (int32 Message : {2,3})
	{
		Person->BehaviorContext=FSimCopterPersonContext(); Person->BehaviorContext.ResetToState(3);
		Person->BehaviorContext.Attributes[EBhavAttr::Speed]=8; Event.MessageIndex=Message;
		Person->ApplyInteraction(Event); FSimCopterBehaviorVM::Tick(Person->BehaviorContext,*Model,*Person);
		TestEqual(TEXT("Other megaphone messages still reach their original calming branch"),int32(Person->BehaviorContext.Attributes[EBhavAttr::Speed]),7);
	}
	Person->Destroy(); Event.MessageIndex=1;
	auto* Car=World->SpawnActor<ASimCopterGroundAgent>(); Car->SetOwner(Traffic);
	Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),250);
	Car->SetActorLocation(FVector(200,-200,40)); Car->bSnapToGround=false;
	Traffic->VehicleAgents.Add(Car); Traffic->VehicleTrafficStates.Add(Car);
	Car->MakeSpeeder();
	TestFalse(TEXT("Unlit speeder ignores Stop Criminal"),Traffic->ApplyVehicleInteraction(*Car,Event));
	Car->SetSpotlightMark(2);
	int32 X,Y; Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Car->GetActorLocation(),X,Y);
	Traffic->XbldTileIds[Y*128+X]=0x27;
	TestFalse(TEXT("Speeder cannot pull over inside an intersection"),Traffic->ApplyVehicleInteraction(*Car,Event));
	Traffic->XbldTileIds[Y*128+X]=0x1d;
	TestTrue(TEXT("Lit speeder accepts Stop Criminal on an open road"),Traffic->ApplyVehicleInteraction(*Car,Event));
	TestTrue(TEXT("Broadcast orders a real vehicle stop"),Car->IsStopOrdered());
	TestEqual(TEXT("Broadcast arms the original ten-second waiting reward delay"),Car->GetSpeederRewardCooldownSeconds(),10.0f);
	Traffic->UpdateSpeeders(1); TestTrue(TEXT("Speeder brakes to rest"),Car->IsStopped());
	TestFalse(TEXT("Megaphone stop alone is not a capture"),Car->HasMissionResolutionReported());
	for (int32 Tick=0; Tick<120; ++Tick) Traffic->UpdateTrafficInteractions(.05f);
	TestTrue(TEXT("Nearby player keeps the stopped speeder waiting"),Car->IsStopped());
	Heli->SetActorLocation(FVector(15000,-15000,1200));
	for (int32 Tick=0; Tick<340; ++Tick) Traffic->UpdateTrafficInteractions(.05f);
	TestFalse(TEXT("Abandoned speeder resumes driving without awarding a capture"),Car->IsStopped() || Car->IsStopOrdered() || Car->HasMissionResolutionReported());
	Car->ClearSpeeder(); Car->MakeCriminalCar(42,65536); Car->SetSpotlightMark(2);
	TestFalse(TEXT("CARROBBR retains its original self-halt rejection while cruising"),Traffic->ApplyVehicleInteraction(*Car,Event));
	TestTrue(TEXT("Police can still stop a marked CARROBBR"),Car->TryOrderStop(SimCopterCriminalCar::PoliceCarMessageId));
	Car->Destroy();
	auto* CivilianCar=World->SpawnActor<ASimCopterGroundAgent>(); CivilianCar->SetOwner(Traffic);
	CivilianCar->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),250);
	CivilianCar->SetActorLocation(FVector(200,-200,40)); CivilianCar->bSnapToGround=false;
	Traffic->VehicleAgents.Add(CivilianCar); Traffic->VehicleTrafficStates.Add(CivilianCar);
	TestTrue(TEXT("Ordinary traffic also hears the original stop command"),Traffic->ApplyVehicleInteraction(*CivilianCar,Event));
	TestTrue(TEXT("Ordinary traffic stops temporarily"),CivilianCar->IsStopOrdered());
	Saved.Reset(); CivilianCar->CaptureRuntimeSaveState(Saved);
	TestTrue(TEXT("Temporary traffic stop saves and reloads"),CivilianCar->RestoreRuntimeSaveState(Saved));
	TestTrue(TEXT("Reload retains stop ownership"),CivilianCar->HasMegaphoneStop());
	for (int32 Tick=0; Tick<120; ++Tick) Traffic->UpdateTrafficInteractions(.05f);
	TestFalse(TEXT("Ordinary traffic resumes after its original brief hold"),CivilianCar->IsStopOrdered() || CivilianCar->IsStopped());
	PC->UnPossess();
	return true;
}
#endif

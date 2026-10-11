#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "City/SimCity2000CityActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Formats/SimCopterOriginalGamePaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterWindTurbineTest,"SimCopter.CityIntro.WindTurbineLifecycle",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterWindTurbineTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	City->CityFile.FilePath=SimCopterOriginalGame::ResolveRoot()/TEXT("cities/career/city18.sc2");
	City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	if (!TestTrue(TEXT("Myrtle Dam contains turbine rotors"),City->BuildingInstanceComponents.IsValidIndex(City->WindTurbineComponentIndex))) return false;
	auto* Rotors=City->BuildingInstanceComponents[City->WindTurbineComponentIndex].Get();
	const int32 Count=Rotors->GetInstanceCount();
	TestTrue(TEXT("Entire wind farm receives blades"),Count>10);
	// Trace the blade's actual triangles, then repeat after rotation. Empty sectors stay open.
	const auto CheckBladeCollision=[&]
	{
		FTransform Pose; Rotors->GetInstanceTransform(0,Pose,true);
		const float Radius=Rotors->GetStaticMesh()->GetBounds().BoxExtent.Z;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(TurbineBladeTest),true);
		for (int32 Blade=0; Blade<3; ++Blade)
		{
			const FQuat Spin(FVector::ForwardVector,Blade*2*PI/3);
			const FVector Point=Spin.RotateVector(FVector(0,0,Radius*.65f));
			FHitResult Hit;
			TestTrue(TEXT("Each spinning blade blocks aircraft/visibility traces"),World->LineTraceSingleByChannel(Hit,
				Pose.TransformPosition(Point+FVector(50,0,0)),Pose.TransformPosition(Point-FVector(50,0,0)),ECC_Visibility,Query) && Hit.GetComponent()==Rotors);
			const FVector Gap=FQuat(FVector::ForwardVector,PI/3).RotateVector(Point);
			FHitResult GapHit;
			World->LineTraceSingleByChannel(GapHit,Pose.TransformPosition(Gap+FVector(50,0,0)),Pose.TransformPosition(Gap-FVector(50,0,0)),ECC_Visibility,Query);
			TestFalse(TEXT("Space between blades is not a solid rotor disc"),GapHit.GetComponent()==Rotors);
		}
	};
	CheckBladeCollision();
	FTransform Before,After; Rotors->GetInstanceTransform(0,Before);
	City->AdvanceWindTurbines(.5f); Rotors->GetInstanceTransform(0,After);
	CheckBladeCollision();
	TestTrue(TEXT("Blades turn about the fixed original hub"),Before.GetLocation().Equals(After.GetLocation()) && !Before.GetRotation().Equals(After.GetRotation()));
	City->AdvanceWindTurbines(0); FTransform Held; Rotors->GetInstanceTransform(0,Held);
	TestTrue(TEXT("Zero elapsed time holds rotor pose"),Held.Equals(After));
	const int32 Owner=City->ComponentInstanceBuildings[City->WindTurbineComponentIndex][0];
	const FIntPoint Tile=City->Buildings[Owner].OriginTile;
	TArray<FIntPoint> Cleared;
	TestTrue(TEXT("Tower can be demolished"),City->DemolishBuildingAtTile(Tile.X,Tile.Y,Cleared));
	TestEqual(TEXT("Demolition removes the matching rotor"),Rotors->GetInstanceCount(),Count-1);
	City->AdvanceWindTurbines(.5f);
	for (int32 I=0; I<Rotors->GetInstanceCount(); ++I)
	{
		FTransform Pose; Rotors->GetInstanceTransform(I,Pose);
		const auto& Building=City->Buildings[City->ComponentInstanceBuildings[City->WindTurbineComponentIndex][I]];
		TestTrue(TEXT("Swap removal preserves every remaining hub"),Pose.GetLocation().Equals(Building.PlacementOrigin+City->WindTurbineHub));
	}
	City->RebuildCity();
	Rotors=City->BuildingInstanceComponents[City->WindTurbineComponentIndex].Get();
	TestEqual(TEXT("Rebuilding replaces rather than duplicates the wind farm"),Rotors->GetInstanceCount(),Count);
	City->RestoreDemolishedBuildingOrigins({Tile},Cleared);
	TestEqual(TEXT("Loading a demolished tower also removes its blades"),Rotors->GetInstanceCount(),Count-1);
	return true;
}
#endif

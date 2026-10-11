#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UnrealType.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "City/SimCity2000CityActor.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterTransportCityClearingsTest,
	"SimCopter.Transport.CityClearings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FSimCopterTransportCityClearingsTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame/cities"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files,*Root,TEXT("*.sc2"),true,false);
	Files.Sort();
	for (const FString& File : Files)
	{
		FString Relative=File;
		FPaths::MakePathRelativeTo(Relative,*(Root+TEXT("/")));
		Names.Add(FPaths::ChangeExtension(Relative,TEXT("")).Replace(TEXT("/"),TEXT(".")).Replace(TEXT(" "),TEXT("_")));
		Commands.Add(File);
	}
}

bool FSimCopterTransportCityClearingsTest::RunTest(const FString& Parameters)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); CollectGarbage(RF_NoFlags); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	// Configure the same editor-exposed properties a map uses, before construction.
	for (const TCHAR* Name : {TEXT("bLoadOnConstruction"),TEXT("bLoadOnBeginPlay"),
		TEXT("bRenderProceduralMapExtension"),TEXT("bRenderStreetLightSpotLights")})
	{
		auto* Property=FindFProperty<FBoolProperty>(City->GetClass(),Name);
		if (!TestNotNull(Name,Property)) return false;
		Property->SetPropertyValue_InContainer(City,false);
	}
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	auto* CityFile=FindFProperty<FStructProperty>(City->GetClass(),TEXT("CityFile"));
	if (!TestNotNull(TEXT("CityFile editor property"),CityFile)) return false;
	CityFile->ContainerPtrToValuePtr<FFilePath>(City)->FilePath=Parameters;
	City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	if (!TestTrue(TEXT("City placement data loads"),Traffic->RebuildSpawnData())) return false;
	TArray<FBox> Buildings;
	TArray<FVector> Anchors;
	for (int32 Y=0;Y<128;++Y) for (int32 X=0;X<128;++X)
	{
		FBox Bounds(ForceInit);
		if (City->TryGetBuildingBoundsAtTile(X,Y,Bounds)) Buildings.Add(Bounds);
		const int32 Id=Traffic->GetXbldTileId(X,Y);
		FVector Center;
		if (Id>0x6f && Id<0xdc && Traffic->TryGetTileCenterWorldLocation(X,Y,Center)) Anchors.Add(Center);
	}
	int32 Checked=0,Resolved=0;
	if (FPaths::GetBaseFilename(Parameters)==TEXT("city0"))
	{
		auto* Aircraft=World->SpawnActor<ASimCopterHelicopterPawn>();
		Aircraft->SwitchHelicopterModel(0);
		const FBox Bounds=Aircraft->GetParkingWorldBounds();
		AddInfo(FString::Printf(TEXT("Helper parking bounds: %s"),*Bounds.ToString()));
		const float X=FMath::Max(FMath::Abs(Bounds.Min.X),FMath::Abs(Bounds.Max.X));
		const float Y=FMath::Max(FMath::Abs(Bounds.Min.Y),FMath::Abs(Bounds.Max.Y));
		TestTrue(TEXT("Clearing fits helper rotor and tail at every heading, with margin"),FMath::Sqrt(X*X+Y*Y)+24<=275);
		Aircraft->Destroy();
	}
	const auto CheckAnchor = [&](const FVector& Anchor)
	{
		++Checked; FVector Clearing;
		if (!Traffic->TryFindTransportClearing(Anchor,Clearing,275,3200)) return;
		++Resolved;
		TestTrue(TEXT("Original neighbourhood retained"),FVector::Dist2D(Clearing,Anchor)<=3200.01f);
		float GroundZ=0; uint8 TerrainClass=0;
		TestTrue(TEXT("Actual terrain surface exists"),City->TryGetWaterGameplaySurface(Clearing,GroundZ,TerrainClass));
		TestTrue(TEXT("Drop point is dry ground, not a roof"),TerrainClass>=10 && FMath::Abs(Clearing.Z-GroundZ)<1);
		bool OverlapsBuilding=false;
		for (const FBox& Bounds : Buildings)
			if (Clearing.X+275>=Bounds.Min.X && Clearing.X-275<=Bounds.Max.X &&
				Clearing.Y+275>=Bounds.Min.Y && Clearing.Y-275<=Bounds.Max.Y) { OverlapsBuilding=true; break; }
		TestFalse(TEXT("Whole helicopter footprint lies outside every rendered building"),OverlapsBuilding);
		FVector Again;
		TestTrue(TEXT("Saved destination does not drift on reload"),Traffic->TryFindTransportClearing(Clearing,Again,275,3200) && Again.Equals(Clearing,0.1));
	};
	// Distribute probes through every city's original mission-building pool. If the
	// sample misses every clearing, exhaust the pool before declaring the city unusable.
	const int32 Stride=FMath::Max(1,Anchors.Num()/24);
	for (int32 Index=0;Index<Anchors.Num();Index+=Stride) CheckAnchor(Anchors[Index]);
	if (Resolved==0)
	{
		for (int32 Index=0;Index<Anchors.Num() && Resolved==0;++Index)
			if (Index%Stride!=0) CheckAnchor(Anchors[Index]);
	}
	AddInfo(FString::Printf(TEXT("%s: %d/%d sampled original mission anchors have a nearby clearing"),*FPaths::GetBaseFilename(Parameters),Resolved,Checked));
	TestTrue(TEXT("City still has playable transport destinations"),Resolved>0);
	return true;
}
#endif

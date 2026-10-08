#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "City/SimCopterHangar.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "Flight/SimCopterHelicopterPresentation.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Game/SimCopterSaveSubsystem.h"
#include "Ground/SimCopterApachePool.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "UI/SimCopterHangarShop.h"
#include "UI/SimCopterHangarArt.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterApacheCityTest, "SimCopter.Apache.OriginalCityMarker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterApacheCityTest::RunTest(const FString& Parameters)
{
	const FString Root = SimCopterOriginalGame::ResolveRoot();
	int32 CitiesWithApache = 0;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		FSimCity2000City City; FString Error;
		const FString Path = Root / FString::Printf(TEXT("cities/career/city%d.sc2"), Index);
		if (!TestTrue(*Path, FSimCity2000Reader::LoadCityFromFile(Path, City, Error))) return false;
		const FIntPoint Tile = SimCopterHelicopterParking::FindApacheSpawnTile(
			[&City](int32 X, int32 Y) { return int32(City.Tiles[Y * 128 + X].Building); });
		if (Tile.X != INDEX_NONE && SimCopterHelicopterParking::IsApacheEncounterCity(Index))
		{
			++CitiesWithApache;
			TestTrue(TEXT("Original late-game maps reveal Apache"), Index >= 25 && Index <= 29);
			AddInfo(FString::Printf(TEXT("Apache in %s (City%d), tile %d,%d"), *City.CityName, Index, Tile.X, Tile.Y));
		}
	}
	TestEqual(TEXT("All five original late-game spawn maps qualify"), CitiesWithApache, 5);
	TestFalse(TEXT("Earlier maps stay hidden"), SimCopterHelicopterParking::IsApacheEncounterCity(24));
	TestFalse(TEXT("Invalid city stays hidden"), SimCopterHelicopterParking::IsApacheEncounterCity(30));
	TestEqual(TEXT("First marked tile wins in row-major order"), SimCopterHelicopterParking::FindApacheSpawnTile(
		[](int32 X, int32 Y) { return (X == 4 && Y == 3) || (X == 3 && Y == 4) ? 0xe7 : 0; }), FIntPoint(4, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterApacheShopTest, "SimCopter.Apache.MysteryPurchaseAndWeapons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterApacheShopTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Career = NewObject<USimCopterCareerSubsystem>(NewObject<UGameInstance>());
	Career->EnsurePricesLoaded(SimCopterOriginalGame::ResolveRoot()); Career->BeginCareer();
	int32 Highest = 0;
	for (const auto& Def : SimCopterHelicopterRegistry::GetDefinitions())
		if (!Def.bApacheArmament) Highest = FMath::Max(Highest, Career->GetHelicopterPrice(Def.InternalTypeIndex));
	const int32 Price = Career->GetHelicopterPrice(2);
	TestTrue(TEXT("Reference prices loaded"), Highest > 0);
	TestEqual(TEXT("Apache price tracks most expensive civilian"), Price, Highest * 3);
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr, 1);
	Missions->MissionSystem.LoadCareerData(SimCopterOriginalGame::ResolveRoot() / TEXT("tweak/career.twk"));
	Missions->AddSessionCash(Price + 5000 - Missions->GetSessionCash());
	auto* Current = World->SpawnActor<ASimCopterHelicopterPawn>();
	TestTrue(TEXT("Starting model loads"), Current->SwitchHelicopterModel(4));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	if (!TestTrue(TEXT("Airport data loads"), Traffic->RebuildSpawnData())) return false;
	FVector Pad; Traffic->TryGetAirportPadWorldLocation(0, Pad); Current->PlaceOnHelipad(Pad, 0);
	auto* Hangar = World->SpawnActor<ASimCopterHangar>();
	TestTrue(TEXT("Hangar places"), Hangar->PlaceAtAirport(Traffic, Pad));
	SimCopterHangarShop::FContext Shop; Shop.Career = Career; Shop.Missions = Missions; Shop.Helicopter = Current; Shop.Hangar = Hangar;
	constexpr int32 Row = SimCopterHangarLayout::ApacheCatalogRow;
	FString Message;
	TestTrue(TEXT("Hidden in ordinary city"), SimCopterHangarShop::GetHelicopterRowState(Shop, Row).bMystery);
	TestFalse(TEXT("Direct buy cannot bypass mystery"), SimCopterHangarShop::BuyHelicopter(Shop, Row, Message));
	TestEqual(TEXT("Hidden price is not exposed"), SimCopterHangarShop::GetHelicopterRowState(Shop, Row).ItemValue, 0);
	// Keep real airport pads; add a distant original encounter marker for this fixture.
	Traffic->XbldTileIds[10 * 128 + 10] = 0xe7;
	Missions->MissionSystem.SelectCareerCity(24);
	TestTrue(TEXT("Earlier city with F-15 tile stays hidden"), SimCopterHangarShop::GetHelicopterRowState(Shop, Row).bMystery);
	SimCopterHelicopterParking::EnsureApacheEncounter(Traffic, Current, Career);
	TestFalse(TEXT("Earlier city does not consume encounter"), Career->HasSpawnedApacheEncounter());
	Missions->MissionSystem.SelectCareerCity(29);
	for (int32 CityIndex = 25; CityIndex <= 29; ++CityIndex)
	{
		Missions->MissionSystem.SelectCareerCity(CityIndex);
		TestFalse(TEXT("Each late-game map reveals mystery before boarding"), SimCopterHangarShop::GetHelicopterRowState(Shop, Row).bMystery);
	}
	SimCopterHelicopterParking::EnsureApacheEncounter(Traffic, Current, Career);
	TArray<AActor*> Fleet;
	auto Count = [&]() { Fleet.Reset(); UGameplayStatics::GetAllActorsOfClass(World, ASimCopterHelicopterPawn::StaticClass(), Fleet); return Fleet.Num(); };
	TestEqual(TEXT("Secret world aircraft spawns alongside starter"), Count(), 2);
	TestFalse(TEXT("Secret is unclaimed until boarding or purchase"), Career->OwnsHelicopter(2));
	SimCopterHelicopterParking::EnsureApacheEncounter(Traffic, Current, Career);
	TestEqual(TEXT("Repeated spawn does not duplicate"), Count(), 2);
	TestFalse(TEXT("Marked city reveals card"), SimCopterHangarShop::GetHelicopterRowState(Shop, Row).bMystery);
	Missions->AddSessionCash(Price - 1 - Missions->GetSessionCash());
	TestFalse(TEXT("One Buck short cannot buy"), SimCopterHangarShop::BuyHelicopter(Shop, Row, Message));
	TestEqual(TEXT("Failed purchase retains secret"), Count(), 2);
	Missions->AddSessionCash(1);
	const FTransform Before = Current->GetActorTransform();
	TestTrue(TEXT("Exact funds buy armed delivery"), SimCopterHangarShop::BuyHelicopter(Shop, Row, Message));
	TestEqual(TEXT("Exactly three times civilian cost charged"), Missions->GetSessionCash(), 0);
	TestEqual(TEXT("Delivery replaces unclaimed aircraft without duplicate"), Count(), 2);
	TestTrue(TEXT("Current helicopter preserved"), Before.Equals(Current->GetActorTransform()));
	TestTrue(TEXT("Ownership recorded"), Career->OwnsHelicopter(2));
	ASimCopterHelicopterPawn* Apache = nullptr;
	for (AActor* Actor : Fleet) if (CastChecked<ASimCopterHelicopterPawn>(Actor)->IsApacheHelicopter()) Apache = CastChecked<ASimCopterHelicopterPawn>(Actor);
	if (!TestNotNull(TEXT("Delivered Apache exists"), Apache)) return false;
	TestNull(TEXT("Purchase does not take possession"), Apache->GetController());
	Apache->GetApachePool()->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot()); // BeginPlay is intentionally not run in this world.
	TestTrue(TEXT("Missiles included"), Apache->IsToolAvailable(ESimCopterHelicopterTool::ApacheMissile));
	TestTrue(TEXT("Gun included"), Apache->IsToolAvailable(ESimCopterHelicopterTool::ApacheMachineGun));
	TestTrue(TEXT("Missile launches from aircraft tool path"), Apache->TryBeginToolUse(ESimCopterHelicopterTool::ApacheMissile));
	TestEqual(TEXT("One active missile"), Apache->GetApachePool()->GetActiveMissileCount(), 1);
	TestFalse(TEXT("Missile cooldown enforced"), Apache->TryBeginToolUse(ESimCopterHelicopterTool::ApacheMissile));
	Apache->SetSelectedTool(ESimCopterHelicopterTool::ApacheMachineGun); Apache->StartPrimaryToolUse();
	Apache->EmitApacheMachineGunFrame(); Apache->EmitApacheMachineGunFrame();
	const int32 Bullets = Apache->GetApachePool()->GetActiveBulletCount();
	TestTrue(TEXT("Held gun emits successive tracers"), Bullets >= 2);
	Apache->StopPrimaryToolUse(); Apache->EmitApacheMachineGunFrame();
	TestEqual(TEXT("Release stops gun"), Apache->GetApachePool()->GetActiveBulletCount(), Bullets);
	TestFalse(TEXT("Civilian aircraft cannot fire missile"), Current->TryBeginToolUse(ESimCopterHelicopterTool::ApacheMissile));
	TestFalse(TEXT("Cannot buy duplicate owned Apache"), SimCopterHangarShop::BuyHelicopter(Shop, Row, Message));
	TestTrue(TEXT("Empty parked Apache can be sold"), SimCopterHangarShop::SellHelicopter(Shop, Row, Message));
	SimCopterHelicopterParking::EnsureApacheEncounter(Traffic, Current, Career);
	TestEqual(TEXT("Sale cannot farm another free encounter"), Count(), 1);
	Traffic->XbldTileIds[10 * 128 + 10] = 0;
	TestFalse(TEXT("Clearing the discovered site cannot hide the card in this city"), SimCopterHangarShop::GetHelicopterRowState(Shop, Row).bMystery);
	auto* Save = NewObject<USimCopterSaveGame>(); Save->bApacheEncounterSpawned = Career->HasSpawnedApacheEncounter();
	TArray<uint8> Bytes; TestTrue(TEXT("Encounter state serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	auto* Loaded = Cast<USimCopterSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestTrue(TEXT("Encounter remains consumed after reload"), Loaded && Loaded->bApacheEncounterSpawned);
	Career->BeginCareer(); TestFalse(TEXT("New career resets encounter"), Career->HasSpawnedApacheEncounter());
	TestTrue(TEXT("New ordinary career hides the card"), SimCopterHangarShop::GetHelicopterRowState(Shop, Row).bMystery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterFleetPaintTest, "SimCopter.Apache.AllNinePaintModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterFleetPaintTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	FMaxisMeshLibrary Library; FString Error;
	if (!TestTrue(TEXT("Original fleet data loads"), Library.LoadFromOriginalGameRoot(SimCopterOriginalGame::ResolveRoot(), Error))) return false;
	for (const auto& Def : SimCopterHelicopterRegistry::GetDefinitions())
	{
		if (!TestTrue(*Def.DisplayName, Heli->SwitchHelicopterModel(Def.InternalTypeIndex))) continue;
		const TArray<FColor>* Source = nullptr;
		const auto* Object = Library.FindObjectByObjectId(Def.BodyObjectId, &Source);
		if (!TestNotNull(TEXT("Body palette"), Source)) continue;
		FString FaceReport = TEXT("material,x,y,z,r,g,b\n");
		for (const auto& Face : Object->Faces)
		{
			FVector Center = FVector::ZeroVector;
			for (const auto Index : Face.VertexIndices) Center += FVector(Object->Vertices[Index].X, Object->Vertices[Index].Y, Object->Vertices[Index].Z);
			Center /= FMath::Max(1, Face.VertexIndices.Num());
			const FColor Color = (*Source)[Face.MaterialIndex];
			FaceReport += FString::Printf(TEXT("%d,%f,%f,%f,%d,%d,%d\n"),Face.MaterialIndex,Center.X,Center.Y,Center.Z,Color.R,Color.G,Color.B);
		}
		FFileHelper::SaveStringToFile(FaceReport, *(FPaths::ProjectDir() / FString::Printf(TEXT("../Docs/scratchpad/rescue-pilot-fixes/faces-%d.csv"), Def.InternalTypeIndex)));
		TArray<FColor> Paint; SimCopterHelicopterPresentation::MakePaintPalette(*Source, Paint, Def.InternalTypeIndex);
		TestEqual(TEXT("Windows and tires retain black"), Paint[0], (*Source)[0]);
		for (int32 Index = 240; Index < 256; ++Index) TestEqual(TEXT("Navigation lights unchanged"), Paint[Index], (*Source)[Index]);
		if (Def.bApacheArmament) TestTrue(TEXT("Military trim stays dark olive"), Paint[48].G > Paint[48].B && Paint[48].R < 80);
		// Confirm the actual aircraft used its catalog palette, including the common build path.
		// A correctly remapped helper is insufficient if PrepareHelicopterModel omits TypeIndex.
		const int32 PaintRegion[] = {16,176,48,80,16,176,16,176,192};
		const FColor Expected = FLinearColor(Paint[PaintRegion[Def.InternalTypeIndex]]).ToFColor(false);
		int32 MatchingVertices = 0;
		for (const auto& Vertex : Heli->HeliBodyMeshComponent->GetProcMeshSection(0)->ProcVertexBuffer)
			if (Vertex.Color == Expected) ++MatchingVertices;
		TestTrue(TEXT("Catalog livery reaches the actual fuselage"), MatchingVertices > 20);
		auto* Mat = Cast<UMaterialInstanceDynamic>(Heli->HeliBodyMeshComponent->GetMaterial(0));
		if (TestNotNull(TEXT("Dedicated paint MID applied"), Mat))
		{
			TestTrue(TEXT("Paint has dedicated parent"), Mat->Parent->GetName() == TEXT("M_SimCopterHelicopterPaint"));
			TestEqual(TEXT("Paint is dielectric, not shared fleet chrome"), Heli->GetVehicleMetallic(), 0.04f);
			float Roughness = 0; Mat->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Roughness")), Roughness);
			TestEqual(TEXT("Apache matte; civilian satin"), Roughness, Def.bApacheArmament ? 0.78f : 0.44f);
		}
		// Export the actual procedural body/cabin vertices for an offline color/geometry sheet.
		FString Obj; int32 Offset = 1;
		for (int32 Section = 0; Section < Heli->HeliBodyMeshComponent->GetNumSections(); ++Section)
		{
			const auto* Mesh = Heli->HeliBodyMeshComponent->GetProcMeshSection(Section); if (!Mesh) continue;
			Obj += FString::Printf(TEXT("g %s\n"), Section == 0 ? TEXT("body") : TEXT("glass"));
			for (const auto& V : Mesh->ProcVertexBuffer)
				Obj += FString::Printf(TEXT("v %f %f %f %f %f %f\n"), V.Position.X, V.Position.Y, V.Position.Z, V.Color.R / 255.f, V.Color.G / 255.f, V.Color.B / 255.f);
			for (int32 I = 0; I < Mesh->ProcIndexBuffer.Num(); I += 3)
				Obj += FString::Printf(TEXT("f %d %d %d\n"), Offset + Mesh->ProcIndexBuffer[I], Offset + Mesh->ProcIndexBuffer[I+1], Offset + Mesh->ProcIndexBuffer[I+2]);
			Offset += Mesh->ProcVertexBuffer.Num();
		}
		FFileHelper::SaveStringToFile(Obj, *(FPaths::ProjectDir() / FString::Printf(TEXT("../Docs/scratchpad/rescue-pilot-fixes/model-%d.obj"), Def.InternalTypeIndex)));
	}
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "Formats/SimCopterPeopleCityRules.h"
#include "Formats/MaxisWindowsBitmapReader.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterHelicopterPresentation.h"
#include "Game/SimCopterPlayerController.h"
#include "UI/SimCopterHangarArt.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"
#include "ImageUtils.h"

namespace
{
TArray<FColor> TexturePixels(UTexture2D* Texture)
{
	TArray<FColor> Result;
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty()) return Result;
	auto& Mip = Texture->GetPlatformData()->Mips[0];
	const auto* Pixels = static_cast<const FColor*>(Mip.BulkData.LockReadOnly());
	if (Pixels) Result.Append(Pixels, Mip.SizeX * Mip.SizeY);
	Mip.BulkData.Unlock();
	return Result;
}
UTexture2D* HeadTexture(UProceduralMeshComponent* Mesh, int32 Section)
{
	auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Section));
	UTexture* Texture = nullptr;
	if (Material) Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Texture")), Texture);
	return Cast<UTexture2D>(Texture);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterNpcAppearanceTest, "SimCopter.Appearance.IdentityAcrossViews",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterNpcAppearanceTest::RunTest(const FString&)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	const FString Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/npc-appearance"));
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	if (!TestTrue(TEXT("Agusta loads"), Heli->SwitchHelicopterModel(5))) return false;
	auto* Art = NewObject<USimCopterHangarArt>(); Art->SetOriginalGameRoot(Root);
	FMaxisTextureImage Sheet; FString Error;
	if (!TestTrue(TEXT("Original portrait sheet loads"), FMaxisWindowsBitmapReader::LoadPalettedBitmapFromFile(
		Root / TEXT("bmp/people1.bmp"), Sheet, Error, 254))) return false;
	auto* Person = World->SpawnActor<ASimCopterGroundAgent>();
	for (int32 Class = 0; Class <= 21; ++Class)
	{
		if (Class == 10 || Class == 16 || Class == 17) continue; // animals do not use human seats
		Heli->RemoveMissionPassengers(100);
		Person->SetInitialBehaviorClass(Class); Person->InitialPersonState = 0;
		Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
		Person->BehaviorContext.Attributes[EBhavAttr::HeadImageIndex] = FSimCopterPeopleCityRules::GetHeadImageIndexForBehaviorClass(Class);
		Person->RefreshHeadImageIndex();
		TestTrue(TEXT("Actual pedestrian figure loads"), Person->bUsingPedestrianFigure);
		TestEqual(TEXT("Same actor enters the manifest"), Heli->AddMissionPassengersForMission(1, 771,
			ESimCopterMissionPassengerKind::Transport, Person), 1);
		for (int32 Clothes : {0, 3, 9})
		{
			Person->SetPedestrianFigureClothesOffset(Clothes);
			Heli->RefreshCabinOccupants();
			const auto* Walking = Person->OriginalMeshComponent->GetProcMeshSection(0);
			const auto* Seated = Heli->CabinOccupantsMesh->GetProcMeshSection(0);
			if (!TestTrue(TEXT("Walking and seated bodies exist"), Walking && Seated)) return false;
			TestEqual(TEXT("Seating preserves every body part"), Seated->ProcVertexBuffer.Num(), Walking->ProcVertexBuffer.Num());
			bool bMatchingColors = Walking->ProcVertexBuffer.Num() == Seated->ProcVertexBuffer.Num();
			for (int32 I = 0; bMatchingColors && I < Walking->ProcVertexBuffer.Num(); ++I)
				bMatchingColors = Walking->ProcVertexBuffer[I].Color == Seated->ProcVertexBuffer[I].Color;
			TestTrue(FString::Printf(TEXT("Class %d clothes %d retains the exact body colors"), Class, Clothes), bMatchingColors);
			const auto BodyHead = TexturePixels(Person->FigureHeadTexture);
			const auto CabinHead = TexturePixels(HeadTexture(Heli->CabinOccupantsMesh, 2));
			TestTrue(TEXT("Seated face is the walking person's actual head texture"), !BodyHead.IsEmpty() && BodyHead == CabinHead);
			TestEqual(TEXT("Manifest uses current head"), Heli->MissionPassengerSlots[0].HeadImageIndex, Person->GetHeadImageIndex());
			const uint32 StableKey = Heli->CabinOccupantsKey;
			Heli->RefreshCabinOccupants();
			TestEqual(TEXT("Unchanged identity reuses cabin geometry"), Heli->CabinOccupantsKey, StableKey);
			for (int32 Mood = 0; Mood < 3; ++Mood)
			{
				const auto* Brush = Art->GetPassengerPortrait(Person->GetPedestrianFigureName(), Clothes, Person->GetHeadImageIndex(), Mood);
				if (!TestNotNull(TEXT("Matching portrait exists for each expression"), Brush)) return false;
				auto* Texture = Cast<UTexture2D>(Brush->GetResourceObject());
				const auto Pixels = TexturePixels(Texture);
				TestEqual(TEXT("Portrait preserves nearest-neighbor sampling"), Texture->Filter, TF_Nearest);
				bool bFaceIntact = Pixels.Num() == 27 * 33;
				for (int32 Y = 0; bFaceIntact && Y < 24; ++Y)
					for (int32 X = 0; bFaceIntact && X < 27; ++X)
						bFaceIntact = Pixels[Y*27+X] == Sheet.Pixels[(Mood*33+Y)*Sheet.Width + (Person->GetHeadImageIndex()+1)*27+X];
				TestTrue(TEXT("Face, hair and mood survive clothing matching"), bFaceIntact);
				TestTrue(TEXT("Portrait cache reuses the exact identity"), Brush == Art->GetPassengerPortrait(
					Person->GetPedestrianFigureName(), Clothes, Person->GetHeadImageIndex(), Mood));
				if (Clothes == 0 && Mood == 0)
					FImageUtils::SaveImageByExtension(*(Output / FString::Printf(TEXT("portrait-%02d.png"), Class)), FImageView(Pixels.GetData(), 27, 33));
			}
		}
	}
	// The user's example must really change shirt pixels, not only a cache key.
	const auto Red = TexturePixels(Cast<UTexture2D>(Art->GetPassengerPortrait(TEXT("Blonde"), 0, 4, 0)->GetResourceObject()));
	const auto Blue = TexturePixels(Cast<UTexture2D>(Art->GetPassengerPortrait(TEXT("Blonde"), 3, 4, 0)->GetResourceObject()));
	TestTrue(TEXT("Blonde red and blue shirts produce different portraits"), Red != Blue);
	TestTrue(TEXT("Red portrait shirt stays red"), Red[31*27+2].R > Red[31*27+2].B * 2);
	TestTrue(TEXT("Blue portrait follows blue walking clothes"), Blue[31*27+2].B > Blue[31*27+2].R * 2);

	// A saved bandaged face previously retained the texture created before VM restoration.
	Heli->RemoveMissionPassengers(100);
	Person->SetInitialBehaviorClass(0); Person->InitialPersonState = 0;
	Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
	Person->SetPedestrianFigureClothesOffset(3);
	Person->BehaviorContext.Attributes[EBhavAttr::HeadImageIndex] = 10;
	Person->RefreshHeadImageIndex();
	const auto SavedHead = TexturePixels(Person->FigureHeadTexture);
	TArray<uint8> Save; TestTrue(TEXT("Identity saves"), Person->CaptureRuntimeSaveState(Save));
	auto* Restored = World->SpawnActor<ASimCopterGroundAgent>();
	TestTrue(TEXT("Identity restores on a different actor"), Restored->RestoreRuntimeSaveState(Save));
	TestEqual(TEXT("Clothes survive save/load"), Restored->GetPedestrianFigureClothesOffset(), 3);
	TestEqual(TEXT("Pool-selected body survives actor recreation"), Restored->GetPedestrianFigureName(), Person->GetPedestrianFigureName());
	TestTrue(TEXT("Saved head selector and texture agree"), SavedHead == TexturePixels(Restored->FigureHeadTexture));
	Heli->AddMissionPassengersForMission(1, 772, ESimCopterMissionPassengerKind::Medevac, Restored);
	Heli->RefreshCabinOccupants();
	TestTrue(TEXT("Restored patient uses their actual head in the cabin"), SavedHead == TexturePixels(HeadTexture(Heli->CabinOccupantsMesh, 2)));
	Heli->RemoveMissionPassengers(100); Heli->RefreshCabinOccupants();
	TestTrue(TEXT("Vacated cabin clears bodies and heads"), Heli->CabinOccupantsMesh->GetNumSections() == 0);

	// A medic's new animation follows living cabin patients, without writing health or moods.
	auto* Medic = World->SpawnActor<ASimCopterGroundAgent>();
	Medic->SetInitialBehaviorClass(12); Medic->InitialPersonState = 5;
	Medic->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
	Heli->AddMissionPassengersForMission(1, 773, ESimCopterMissionPassengerKind::Rescue, Medic);
	TestFalse(TEXT("Medic rests with no patient"), Heli->IsPassengerProvidingMedicalCare(0));
	Restored->BehaviorContext.Attributes[EBhavAttr::State] = 6;
	Restored->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 50;
	Restored->BehaviorContext.Attributes[EBhavAttr::WrittenOff] = 0;
	Heli->AddMissionPassengersForMission(1, 774, ESimCopterMissionPassengerKind::Medevac, Restored);
	TestTrue(TEXT("Medic provides care to a living patient aboard"), Heli->IsPassengerProvidingMedicalCare(0));
	TestFalse(TEXT("Patient does not display the medic animation"), Heli->IsPassengerProvidingMedicalCare(1));
	const auto Care0 = TexturePixels(Cast<UTexture2D>(Art->GetPassengerPortrait(TEXT("Medik"), 0, 3, 0, 0)->GetResourceObject()));
	const auto Care1 = TexturePixels(Cast<UTexture2D>(Art->GetPassengerPortrait(TEXT("Medik"), 0, 3, 0, 1)->GetResourceObject()));
	TestTrue(TEXT("Care moves the medic's hands"), Care0 != Care1);
	for (int32 Mood = 0; Mood < 3; ++Mood)
	{
		const auto Normal = TexturePixels(Cast<UTexture2D>(Art->GetPassengerPortrait(TEXT("Medik"), 0, 3, Mood)->GetResourceObject()));
		for (int32 Frame = 0; Frame < 6; ++Frame)
		{
			const auto Pixels = TexturePixels(Cast<UTexture2D>(Art->GetPassengerPortrait(TEXT("Medik"), 0, 3, Mood, Frame)->GetResourceObject()));
			TestTrue(TEXT("Care preserves the medic's face and flight reaction"), FMemory::Memcmp(Normal.GetData(), Pixels.GetData(), 23*27*sizeof(FColor)) == 0);
			if (Mood == 0) FImageUtils::SaveImageByExtension(*(Output/FString::Printf(TEXT("medic-care-%d.png"), Frame)), FImageView(Pixels.GetData(),27,33));
		}
	}
	TestEqual(TEXT("Presentation never changes patient health"), Restored->GetBehaviorAttribute(EBhavAttr::MedevacHealth), uint16(50));
	Restored->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 0;
	TestFalse(TEXT("Care stops when the patient dies"), Heli->IsPassengerProvidingMedicalCare(0));
	Restored->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = uint16(-2);
	TestFalse(TEXT("Negative signed health is also dead"), Heli->IsPassengerProvidingMedicalCare(0));
	Restored->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 50;
	Restored->BehaviorContext.Attributes[EBhavAttr::WrittenOff] = 1;
	TestFalse(TEXT("Written-off patient is not treated"), Heli->IsPassengerProvidingMedicalCare(0));
	Restored->BehaviorContext.Attributes[EBhavAttr::WrittenOff] = 0;
	Heli->RemoveMissionPassengersForMission(1, 774, ESimCopterMissionPassengerKind::Medevac, Restored);
	TestFalse(TEXT("Care stops after hospital unloading"), Heli->IsPassengerProvidingMedicalCare(0));
	Medic->bClaimedPassengerSeat = true; Medic->BehaviorCarrier = Heli;
	Medic->ReactToCabinImpact();
	TestEqual(TEXT("Medic still flinches after an impact"), Heli->MissionPassengerSlots[0].PortraitState, 2);
	Medic->SetPedestrianFigureClothesOffset(3); Heli->RefreshCabinOccupants();
	TestEqual(TEXT("Appearance refresh preserves the impact reaction"), Heli->MissionPassengerSlots[0].PortraitState, 2);
	Medic->UpdateCabinImpactPortrait(10.0f);
	TestEqual(TEXT("Original idle expression returns after the impact"), Heli->MissionPassengerSlots[0].PortraitState, 1);
	Heli->RemoveMissionPassengers(100);

	auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
	World->AddController(Controller); Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->Possess(Heli);
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>();
	if (!TestTrue(TEXT("Original on-foot pilot loads"), Pilot->LoadOriginalBodyFigure())) return false;
	const auto* PilotBody = Pilot->OriginalBodySpriteComponent->GetProcMeshSection(0);
	for (int32 Type = 0; Type <= 9; ++Type)
	{
		TestTrue(TEXT("Aircraft model loads"), Heli->SwitchHelicopterModel(Type)); Heli->RefreshCabinOccupants();
		TestTrue(TEXT("All aircraft use the actual pilot head"), TexturePixels(Pilot->FigureHeadTexture) == TexturePixels(HeadTexture(Heli->CabinOccupantsMesh, 1)));
		const auto* SeatedPilot = Heli->CabinOccupantsMesh->GetProcMeshSection(0);
		if (!TestTrue(TEXT("Walking and seated pilot bodies exist"), PilotBody && SeatedPilot)) return false;
		TestEqual(TEXT("Pilot preserves the original body parts"), SeatedPilot->ProcVertexBuffer.Num(), PilotBody->ProcVertexBuffer.Num());
	}
	Controller->UnPossess(); Heli->RefreshCabinOccupants();
	TestEqual(TEXT("Pilot disappears after exiting"), Heli->CabinOccupantsMesh->GetNumSections(), 0);
	return true;
}
#endif

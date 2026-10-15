#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Game/SimCopterSettings.h"
#include "ProceduralMeshComponent.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMouseFrameRateTest, "SimCopter.Performance.MouseTravelAcrossFrameRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterMouseFrameRateTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>();
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	const auto* Settings = USimCopterSettings::Get(World);
	const float MouseX = Settings ? Settings->GetMouseSensitivityX() : 1.f;
	const float MouseY = Settings ? Settings->GetMouseSensitivityY() : 1.f;
	const float StickX = Settings ? Settings->GetControllerSensitivityX() : 1.f;
	for (const int32 Fps : {30, 60, 144, 0})
	{
		TArray<float> Steps;
		if (Fps) for (int32 I = 0; I < Fps; ++I) Steps.Add(1.f / Fps);
		else Steps = {.01f, .01f, .12f, .01f, .25f, .1f, .2f, .3f};
		Pilot->SetActorRotation(FRotator::ZeroRotator);
		Pilot->CameraPitchDeg = 0;
		Heli->CameraMode = ESimCopterCameraMode::Chase;
		Heli->CameraYawOffsetDeg = Heli->CameraPitchOffsetDeg = 0;
		Heli->bCameraDragActive = true;
		for (const float Dt : Steps)
		{
			// Identical total mouse displacement, delivered in differently sized frames.
			Pilot->MouseLookYaw(6 * Dt); Pilot->MouseLookPitch(2 * Dt);
			Pilot->UpdateLookYaw(Dt); Pilot->UpdateCamera(Dt);
			Heli->MouseLookYaw(6 * Dt); Heli->MouseLookPitch(-2 * Dt);
			Heli->UpdateCamera(Dt);
		}
		TestTrue(FString::Printf(TEXT("On-foot yaw preserves mouse travel at %d FPS (0 = hitches)"), Fps),
			FMath::IsNearlyEqual(float(Pilot->GetActorRotation().Yaw), 6 * MouseX * Pilot->LookYawSpeedDegPerSec / 60, .001f));
		TestTrue(TEXT("On-foot pitch preserves mouse travel"), FMath::IsNearlyEqual(Pilot->CameraPitchDeg, -2 * MouseY * Pilot->LookPitchSpeedDegPerSec / 60, .001f));
		TestTrue(TEXT("Helicopter drag preserves mouse travel"), FMath::IsNearlyEqual(Heli->CameraYawOffsetDeg, 6 * MouseX * Heli->CameraYawSpeedDegPerSec / 60, .001f));
		TestTrue(TEXT("Helicopter pitch preserves mouse travel"), FMath::IsNearlyEqual(Heli->CameraPitchOffsetDeg, -2 * MouseY * Heli->CameraPitchSpeedDegPerSec / 60, .001f));
		Pilot->MouseLookYaw(0); Pilot->MouseLookPitch(0);
		Pilot->SetActorRotation(FRotator::ZeroRotator);
		Pilot->ControllerLookYaw(.1f);
		for (const float Dt : Steps) Pilot->UpdateLookYaw(Dt);
		TestTrue(TEXT("Controller remains a time-based turn rate"), FMath::IsNearlyEqual(float(Pilot->GetActorRotation().Yaw), .1f * StickX * Pilot->LookYawSpeedDegPerSec, .001f));
		Pilot->ControllerLookYaw(0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterFigureClipReuseTest, "SimCopter.Performance.PopulationClipReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterFigureClipReuseTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	TArray<ASimCopterGroundAgent*> People;
	for (int32 I = 0; I < 32; ++I)
	{
		auto* Person = World->SpawnActor<ASimCopterGroundAgent>();
		Person->SetInitialBehaviorClass(I % 10);
		Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
		if (!TestTrue(TEXT("Original figure loads"), Person->bUsingPedestrianFigure)) return false;
		Person->RebuildFigureClip(TEXT("1Wal")); Person->RebuildFigureClip(TEXT("NoMo"));
		People.Add(Person);
	}
	World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
	const int32 SectionsBefore = People[0]->OriginalMeshComponent->GetNumSections();
	const FProcMeshVertex* VerticesBefore = People[0]->OriginalMeshComponent->GetProcMeshSection(0)->ProcVertexBuffer.GetData();
	const double Start = FPlatformTime::Seconds();
	bool bAnyRebuild = false;
	for (int32 Switch = 0; Switch < 12; ++Switch)
	{
		for (auto* Person : People)
		{
			Person->RebuildFigureClip(Switch % 2 ? TEXT("NoMo") : TEXT("1Wal"));
			bAnyRebuild |= Person->OriginalMeshComponent->IsRenderStateDirty();
		}
		World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
	}
	AddInfo(FString::Printf(TEXT("32 NPCs x 12 idle/walk switches including deferred render updates: %.3f ms"), (FPlatformTime::Seconds()-Start)*1000));
	TestFalse(TEXT("Revisiting prepared clips never rebuilds the render proxy"), bAnyRebuild);
	TestEqual(TEXT("Repeated transitions keep bounded section storage"), People[0]->OriginalMeshComponent->GetNumSections(), SectionsBefore);
	TestTrue(TEXT("Prepared vertex storage survives repeated transitions"), People[0]->OriginalMeshComponent->GetProcMeshSection(0)->ProcVertexBuffer.GetData() == VerticesBefore);
	for (auto* Person : People)
	{
		int32 VisibleBodies = 0;
		for (int32 I = 0; I < Person->OriginalMeshComponent->GetNumSections(); I += 2)
			VisibleBodies += Person->OriginalMeshComponent->IsMeshSectionVisible(I) ? 1 : 0;
		TestEqual(TEXT("Only the selected pose is visible"), VisibleBodies, 1);
	}
	// Compare the selected geometry with an independently rebuilt original clip, including
	// returning to cached poses after clothing/species changes and cache eviction.
	auto* Reference = NewObject<UProceduralMeshComponent>(World);
	auto* Person = People[0];
	for (const int32 Class : {0, 10, 16, 17})
	{
		Person->SetInitialBehaviorClass(Class);
		Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
		if (!TestTrue(TEXT("Figure reload succeeds"), Person->bUsingPedestrianFigure)) return false;
		for (const int32 Clothes : {2, 7})
		{
			Person->SetPedestrianFigureClothesOffset(Clothes);
			const auto& Figure = Person->FigureShared->Model.Figures[Person->FigureIndex];
			for (const auto& Binding : Figure.ClipIndexByMnemonic)
			{
				if (!TestTrue(TEXT("Authored clip binds"), Person->RebuildFigureClip(Binding.Key))) return false;
				FString Effective = Binding.Key;
				if (Figure.Name.StartsWith(TEXT("2DOG")) || Figure.Name.StartsWith(TEXT("Coww")))
					Effective = Effective == TEXT("1Wal") || Effective == TEXT("1Run") || Effective == TEXT("Tote") ? TEXT("DgRn") : TEXT("DgSt");
				const auto* Clip = Person->FigureShared->Model.FindClip(Figure, Effective);
				if (!Clip) Clip = Person->FigureShared->Model.FindClip(Figure, TEXT("NoMo"));
				if (!TestNotNull(TEXT("Reference clip exists"), Clip)) return false;
				FSimCopterPopulationFigure::FBuildParams Params;
				Params.HeightCm = 44.f * (Figure.Name.StartsWith(TEXT("2DOG")) ? 1.f/3 : 1.f);
				Params.ClothesOffset = Clothes;
				Params.bTexturedHead = Person->FigureHeadMaterialInstance != nullptr;
				bool HasHead = false;
				FSimCopterPopulationFigure::BuildClipSections(Reference, Figure, *Clip, Person->FigureShared->Palette, Params, Person->FigureCalibration, HasHead);
				TestTrue(TEXT("Cache is bounded by 32 poses or one larger clip"), Person->OriginalMeshComponent->GetNumSections() <= FMath::Max(64, Clip->FrameCount*2));
				for (const int32 Frame : {0, Clip->FrameCount-1})
				{
					Person->ShowFigureFrame(Frame);
					for (int32 Part = 0; Part < 2; ++Part)
					{
						const auto* Actual = Person->OriginalMeshComponent->GetProcMeshSection(Person->FigureFirstSection + Frame*2 + Part);
						const auto* Expected = Reference->GetProcMeshSection(Frame*2 + Part);
						bool Matches = Actual && Expected && Actual->ProcIndexBuffer == Expected->ProcIndexBuffer && Actual->ProcVertexBuffer.Num() == Expected->ProcVertexBuffer.Num();
						for (int32 V = 0; Matches && V < Actual->ProcVertexBuffer.Num(); ++V)
						{
							const auto& A = Actual->ProcVertexBuffer[V]; const auto& E = Expected->ProcVertexBuffer[V];
							Matches = A.Position.Equals(E.Position, 1.e-5) && A.Normal.Equals(E.Normal, 1.e-5) && A.Color == E.Color && A.UV0.Equals(E.UV0, 1.e-5);
						}
						TestTrue(TEXT("Prepared body/head geometry matches the original builder"), Matches);
					}
					int32 Visible = 0;
					for (int32 S = 0; S < Person->OriginalMeshComponent->GetNumSections(); ++S)
						Visible += Person->OriginalMeshComponent->IsMeshSectionVisible(S) ? 1 : 0;
					TestEqual(TEXT("Cached poses never overlap"), Visible, HasHead ? 2 : 1);
				}
			}
		}
	}
	Person->SetInitialBehaviorClass(0);
	Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
	Person->RebuildFigureClip(TEXT("1Wal"));
	Person->FigureCurrentFrame = Person->FigureFrameCount-1;
	Person->ShowFigureFrame(Person->FigureCurrentFrame);
	TArray<uint8> Save;
	TestTrue(TEXT("Prepared animation saves using ordinary clip/frame identity"), Person->CaptureRuntimeSaveState(Save));
	auto* Restored = World->SpawnActor<ASimCopterGroundAgent>();
	TestTrue(TEXT("Animation restores into a fresh cache"), Restored->RestoreRuntimeSaveState(Save));
	TestEqual(TEXT("Saved clip preserved"), Restored->FigureMnemonic, Person->FigureMnemonic);
	TestEqual(TEXT("Saved frame preserved"), Restored->FigureCurrentFrame, Person->FigureCurrentFrame);
	TestTrue(TEXT("Restored frame is visible at its prepared offset"), Restored->OriginalMeshComponent->IsMeshSectionVisible(Restored->FigureFirstSection + Restored->FigureCurrentFrame*2));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPopulationSpawnTest, "SimCopter.Performance.PopulationSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPopulationSpawnTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	for (const int32 Class : {0, 4, 10, 16, 17})
	{
		const double Start = FPlatformTime::Seconds();
		TArray<ASimCopterGroundAgent*> People;
		for (int32 I = 0; I < 4; ++I)
		{
			const FTransform Transform(FVector(I * 200.f, Class * 200.f, 100.f));
			auto* Person = World->SpawnActorDeferred<ASimCopterGroundAgent>(ASimCopterGroundAgent::StaticClass(), Transform);
			Person->SetInitialBehaviorClass(Class);
			Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85, true);
			TestEqual(TEXT("Deferred spawn does not build a throwaway figure before initialization"), Person->OriginalMeshComponent->GetNumSections(), 0);
			Person->FinishSpawning(Transform);
			Person->DispatchBeginPlay();
			TestTrue(TEXT("Finished spawn has its configured original figure"), Person->IsUsingOriginalMesh());
			TestEqual(TEXT("Behavior starts with the configured class"), Person->GetBehaviorAttribute(EBhavAttr::BehaviorClass), Class);
			People.Add(Person);
		}
		const double Built = FPlatformTime::Seconds();
		World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
		int64 Vertices = 0;
		for (int32 S = 0; S < People[0]->OriginalMeshComponent->GetNumSections(); ++S)
			Vertices += People[0]->OriginalMeshComponent->GetProcMeshSection(S)->ProcVertexBuffer.Num();
		AddInfo(FString::Printf(TEXT("Spawn class %d (%s), 4 actors: build %.3f ms, deferred render %.3f ms; %d sections, %lld vertices/actor"),
			Class, *People[0]->PedestrianFigureName, (Built-Start)*1000, (FPlatformTime::Seconds()-Built)*1000,
			People[0]->OriginalMeshComponent->GetNumSections(), Vertices));
		for (auto* Person : People) Person->Destroy();
		World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterFastFlightPopulationTest, "SimCopter.Performance.FastFlightPopulationBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterFastFlightPopulationTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	FString City = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../../SimCopter Remake v1.0.1/Windows/SimCopter/cities/HEVEN2026.SC2"));
	if (!IFileManager::Get().FileExists(*City)) City = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame/cities/city17.sc2"));
	Traffic->CityFile.FilePath = City;
	if (!TestTrue(TEXT("Flight city spawn data loads"), Traffic->RebuildSpawnData())) return false;
	Traffic->bSimulateWholeMap = false;
	if (!TestTrue(TEXT("City has population nodes"), !Traffic->PedestrianNodes.IsEmpty())) return false;
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Controller = World->SpawnActor<APlayerController>();
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	World->AddController(Controller); Controller->Possess(Heli);
	int32 TotalCreated = 0, FramesWithSpawns = 0;
	double WorstMs = 0, TotalMs = 0;
	for (int32 Step = 0; Step < 120; ++Step)
	{
		// Jump across multiple city tiles each 15 frames, then hold still to drain
		// the queued leading edge. Includes prune/refill, scripted people and vehicles.
		const int32 NodeIndex = ((Step / 15) * 97) % Traffic->PedestrianNodes.Num();
		Heli->SetActorLocation(Traffic->PedestrianNodes[NodeIndex].Location + FVector(0,0,2000));
		TSet<ASimCopterGroundAgent*> Before;
		for (auto P : Traffic->PedestrianAgents) if (P.IsValid()) Before.Add(P.Get());
		for (auto P : Traffic->VehicleAgents) if (P.IsValid()) Before.Add(P.Get());
		const double Start = FPlatformTime::Seconds();
		Traffic->UpdateAgentPool(1.f / 60);
		World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
		const double Ms = (FPlatformTime::Seconds()-Start)*1000;
		TotalMs += Ms; WorstMs = FMath::Max(WorstMs, Ms);
		int32 People = 0, Cars = 0;
		for (auto P : Traffic->PedestrianAgents) if (P.IsValid() && !Before.Contains(P.Get())) ++People;
		for (auto P : Traffic->VehicleAgents) if (P.IsValid() && !Before.Contains(P.Get())) ++Cars;
		TestTrue(TEXT("A moving-city frame creates at most one pedestrian"), People <= 1);
		TestTrue(TEXT("A moving-city frame creates at most one vehicle"), Cars <= 1);
		TotalCreated += People + Cars;
		FramesWithSpawns += People + Cars > 0;
		TestTrue(TEXT("Pending tiles stay bounded by the local scan area"), Traffic->PendingAmbientSpawnTiles.Num() <= FMath::Square(2 * Traffic->OriginalAmbientScanRadiusTiles + 1));
	}
	TestTrue(TEXT("Population continues filling across multiple frames"), TotalCreated >= 8 && FramesWithSpawns >= 8);
	AddInfo(FString::Printf(TEXT("120 moving-city refill frames: %d actors across %d frames, %.3f ms total, %.3f ms worst including deferred render work"), TotalCreated, FramesWithSpawns, TotalMs, WorstMs));
	Controller->UnPossess();
	return true;
}
#endif

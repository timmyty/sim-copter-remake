#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "City/SimCopterDriveIn.h"
#include "City/SimCity2000CityActor.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Game/SimCopterPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Components/SceneCaptureComponent2D.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"

class FSimCopterDriveInPlaybackCommand : public IAutomationLatentCommand
{
public:
	explicit FSimCopterDriveInPlaybackCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (!World)
		{
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			// AActor::ProcessEvent suppresses media delegates before actors initialize.
			World->InitializeActorsForPlay(FURL());
			Controller = World->SpawnActor<ASimCopterPlayerController>();
			Controller->Possess(World->SpawnActor<APawn>());
			City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
			City->bLoadOnConstruction = false;
			City->bRenderProceduralMapExtension = false;
			City->bRenderStreetLightSpotLights = false;
			City->CityFile.FilePath = SimCopterOriginalGame::ResolveRoot() / TEXT("cities/career/city0.sc2");
			City->FinishSpawning(FTransform::Identity);
			City->RebuildCity();
			TArray<FIntPoint> Theaters; City->GetDriveInTiles(Theaters);
			Test->TestEqual(TEXT("Loaded map has a real theater and minimap location"), Theaters.Num(), 1);
			DriveIn = ASimCopterDriveInPlayer::Get(World);
			Test->AddInfo(Controller->ExecuteCheatCodes(TEXT("hsi")));
			Test->TestEqual(TEXT("The authored theater screen is found"), DriveIn->GetScreenCount(), 1);
			Started = FPlatformTime::Seconds();
		}
		DriveIn->Tick(.05f);
		if (FPlatformTime::Seconds() - Started > 40)
		{
			Test->AddError(TEXT("Timed out waiting for a decoded drive-in frame")); return Finish();
		}
		if (!DriveIn->Player || !DriveIn->Player->IsPlaying() || DriveIn->Player->GetTime().GetTotalSeconds() < 1 || DriveIn->Texture->GetSurfaceWidth() < 1) return false;
		const FIntPoint Expected = Stage == 0 ? FIntPoint(1280, 720) : FIntPoint(1080, 1080);
		Test->TestEqual(TEXT("Decoded video retains prepared dimensions"), DriveIn->Player->GetVideoTrackDimensions(INDEX_NONE, INDEX_NONE), Expected);
		Test->TestTrue(TEXT("Movie loops"), DriveIn->Player->IsLooping());
		Test->TestTrue(TEXT("Movie audio track available"), DriveIn->Player->GetNumTracks(EMediaPlayerTrack::Audio) > 0);
		Test->TestEqual(TEXT("Theater has spatial sound"), DriveIn->Sounds.Num(), 1);
		auto* Target = NewObject<UTextureRenderTarget2D>(World);
		Target->RenderTargetFormat = RTF_RGBA8; Target->InitAutoFormat(640, 360); Target->UpdateResourceImmediate(true);
		DriveIn->ScreenMaterial->SetScalarParameterValue(TEXT("EmissiveNits"), 1);
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, Target, DriveIn->ScreenMaterial);
		FlushRenderingCommands();
		TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		int32 Lit = 0;
		for (auto Color : Pixels) if (Color.R > 20 || Color.G > 20 || Color.B > 20) ++Lit;
		Test->TestTrue(TEXT("External media sampler renders non-black decoded video"), Lit > Pixels.Num() / 100);
		FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir() / FString::Printf(TEXT("../Docs/scratchpad/drive-in-ufo/movie-%d.png"), Stage)), FImageView(Pixels.GetData(), 640, 360));
		CheckTheaterPicture();
		if (Stage++ == 0)
		{
			Test->AddInfo(Controller->ExecuteCheatCodes(TEXT("Lights, Camera, Action!")));
			Test->TestEqual(TEXT("Switching replaces screens without duplicates"), DriveIn->GetScreenCount(), 1);
			Started = FPlatformTime::Seconds(); return false;
		}
		Controller->ExecuteCheatCodes(TEXT("Lights, Camera, Action!"));
		Test->TestFalse(TEXT("Repeating original cheat stops playback"), DriveIn->IsVideoActive());
		Test->TestEqual(TEXT("Stopping removes screens"), DriveIn->GetScreenCount(), 0);
		Test->TestEqual(TEXT("Stopping removes sound components"), DriveIn->Sounds.Num(), 0);
		TArray<FIntPoint> Theaters, Cleared;
		City->GetDriveInTiles(Theaters);
		if (!Theaters.IsEmpty()) City->DemolishBuildingAtTile(Theaters[0].X, Theaters[0].Y, Cleared, true);
		City->GetDriveInTiles(Theaters);
		Test->TestTrue(TEXT("Demolished theater no longer has a minimap logo"), Theaters.IsEmpty());
		TArray<FSimCopterDriveInSurface> Surfaces;
		City->GetDriveInSurfaces(Surfaces);
		Test->TestTrue(TEXT("Demolished theater no longer supplies a movie screen"), Surfaces.IsEmpty());
		return Finish();
	}
private:
	FAutomationTestBase* Test;
	UWorld* World = nullptr;
	UGameInstance* Instance = nullptr;
	ASimCopterPlayerController* Controller = nullptr;
	ASimCity2000CityActor* City = nullptr;
	ASimCopterDriveInPlayer* DriveIn = nullptr;
	double Started = 0;
	int32 Stage = 0;
	void CheckTheaterPicture()
	{
		if (DriveIn->Surfaces.IsEmpty()) return;
		const auto& Corners = DriveIn->Surfaces[0].Corners;
		const FVector Centre = (Corners[0] + Corners[2]) * .5;
		const float Width = FVector::Distance(Corners[0], Corners[1]);
		const auto* Picture = DriveIn->Screens[0]->GetProcMeshSection(1);
		if (!Test->TestNotNull(TEXT("Decoded movie has a picture section"), Picture)) return;
		if (!Test->TestEqual(TEXT("Picture has all four corners"), Picture->ProcVertexBuffer.Num(), 4)) return;
		const auto& Vertices = Picture->ProcVertexBuffer;
		const double Height = FVector::Distance(Corners[0], Corners[3]);
		Test->TestTrue(TEXT("Movie spans the complete theater screen width"),
			FMath::IsNearlyEqual(FVector::Distance(Vertices[0].Position, Vertices[1].Position), double(Width), 0.1));
		Test->TestTrue(TEXT("Movie occupies three quarters of the theater screen height"),
			FMath::IsNearlyEqual(FVector::Distance(Vertices[0].Position, Vertices[3].Position), Height * .75, 0.1));
		const FVector Offset = (Vertices[0].Position + Vertices[2].Position) * .5 - Centre;
		Test->TestTrue(TEXT("Movie stays vertically centred"),
			FMath::Abs(FVector::DotProduct(Offset, (Corners[3] - Corners[0]).GetSafeNormal())) < .1);
		Test->TestTrue(TEXT("Stretching keeps the complete source picture without cropping"),
			Vertices[0].UV0.Equals(FVector2D(0, 0)) && Vertices[1].UV0.Equals(FVector2D(1, 0)) &&
			Vertices[2].UV0.Equals(FVector2D(1, 1)) && Vertices[3].UV0.Equals(FVector2D(0, 1)));
		// CO182's parking lot is on the +X side of its screen after the Maxis-to-city transform.
		const FVector Eye = Centre + City->GetActorTransform().TransformVectorNoScale(FVector::ForwardVector) * Width;
		auto* Capture = NewObject<USceneCaptureComponent2D>(DriveIn);
		Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->CaptureSource = SCS_FinalColorLDR; Capture->FOVAngle = 70;
		Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
		Capture->ShowFlags.SetEyeAdaptation(false);
		Capture->ShowFlags.SetTonemapper(false);
		Capture->ShowFlags.SetBloom(false);
		auto* Target = NewObject<UTextureRenderTarget2D>(DriveIn);
		Target->RenderTargetFormat = RTF_RGBA8; Target->InitAutoFormat(640, 640); Target->UpdateResourceImmediate(true);
		Capture->TextureTarget = Target;
		Capture->SetWorldLocationAndRotation(Eye, (Centre - Eye).Rotation());
		Capture->RegisterComponent();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		World->SendAllEndOfFrameUpdates();
		const FString Evidence = FPaths::ProjectDir() / TEXT("../Docs/scratchpad/drive-in-full-screen");
		IFileManager::Get().MakeDirectory(*Evidence, true);
		auto Read = [&](const TCHAR* Name)
		{
			Capture->CaptureScene(); FlushRenderingCommands();
			TArray<FColor> Result; Target->GameThread_GetRenderTargetResource()->ReadPixels(Result);
			for (auto& Pixel : Result) Pixel.A = 255;
			FImageUtils::SaveImageByExtension(*(Evidence / FString::Printf(TEXT("movie-%d-%s.png"), Stage, Name)), FImageView(Result.GetData(), 640, 640));
			return Result;
		};
		// Keep the black backing visible: only the decoded picture may change these pixels.
		DriveIn->Screens[0]->SetMeshSectionVisible(1, false);
		const TArray<FColor> WithoutMovie = Read(TEXT("theater-blank"));
		DriveIn->Screens[0]->SetMeshSectionVisible(1, true);
		const TArray<FColor> WithMovie = Read(TEXT("theater-video"));
		Capture->HiddenActors.Add(City);
		Read(TEXT("video-without-building"));
		int32 Changed = 0;
		for (int32 Index = 0; Index < WithMovie.Num(); ++Index)
		{
			const auto A = WithMovie[Index], B = WithoutMovie[Index];
			if (FMath::Abs(int(A.R) - int(B.R)) + FMath::Abs(int(A.G) - int(B.G)) + FMath::Abs(int(A.B) - int(B.B)) > 32) ++Changed;
		}
		Test->AddInfo(FString::Printf(TEXT("Movie %d changes %d of %d scene pixels when the theater is visible"), Stage, Changed, WithMovie.Num()));
		Test->TestTrue(TEXT("Movie picture is visible from the parking lot with the actual theater geometry present"), Changed > WithMovie.Num() / 100);
		Capture->DestroyComponent();
	}
	bool Finish() { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDriveInPlaybackTest, "SimCopter.DriveIn.Playback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FSimCopterDriveInPlaybackTest::RunTest(const FString&)
{
	if (GUsingNullRHI) { AddError(TEXT("Run this decoder/material test with -RenderOffscreen, not -NullRHI")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FSimCopterDriveInPlaybackCommand(this));
	return true;
}
#endif

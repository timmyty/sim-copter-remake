#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "City/SimCopterDriveIn.h"
#include "City/SimCity2000CityActor.h"
#include "Formats/SimCity2000Reader.h"
#include "Engine/Engine.h"
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

class FSimCopterDriveInPlaybackCommand : public IAutomationLatentCommand
{
public:
	explicit FSimCopterDriveInPlaybackCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (!World)
		{
			const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			// AActor::ProcessEvent suppresses media delegates before actors initialize.
			World->InitializeActorsForPlay(FURL());
			City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
			City->bLoadOnConstruction = false;
			City->FinishSpawning(FTransform::Identity);
			auto& Building = City->Buildings.AddDefaulted_GetRef();
			Building.XbldId = 182; Building.OriginTile = FIntPoint::ZeroValue;
			Building.PlacementOrigin = FVector::ZeroVector;
			City->TileBuildingIds.Init(INDEX_NONE, FSimCity2000City::TileCount); City->TileBuildingIds[0] = 0;
			DriveIn = ASimCopterDriveInPlayer::Get(World);
			Test->AddInfo(DriveIn->PlayVideo(TEXT("HSI.mp4")));
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
		if (Stage++ == 0)
		{
			Test->AddInfo(DriveIn->PlayVideo(TEXT("LightsCameraActionSimCopter.mp4")));
			Test->TestEqual(TEXT("Switching replaces screens without duplicates"), DriveIn->GetScreenCount(), 1);
			Started = FPlatformTime::Seconds(); return false;
		}
		DriveIn->PlayVideo(TEXT("LightsCameraActionSimCopter.mp4"), true);
		Test->TestFalse(TEXT("Repeating original cheat stops playback"), DriveIn->IsVideoActive());
		Test->TestEqual(TEXT("Stopping removes screens"), DriveIn->GetScreenCount(), 0);
		Test->TestEqual(TEXT("Stopping removes sound components"), DriveIn->Sounds.Num(), 0);
		return Finish();
	}
private:
	FAutomationTestBase* Test;
	UWorld* World = nullptr;
	ASimCity2000CityActor* City = nullptr;
	ASimCopterDriveInPlayer* DriveIn = nullptr;
	double Started = 0;
	int32 Stage = 0;
	bool Finish() { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return true; }
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

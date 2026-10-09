#include "City/SimCopterDriveIn.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterEffectExposure.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "UObject/ConstructorHelpers.h"

FString SimCopterDriveIn::VideoDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../DriveInVideos"));
}

bool SimCopterDriveIn::ResolveVideo(const FString& FileName, FString& OutPath, FString& OutError)
{
	OutPath.Reset();
	// Only local files inside the documented movie folders; no URLs or path traversal.
	if (FileName.IsEmpty() || FileName != FPaths::GetCleanFilename(FileName) || FileName.Contains(TEXT(":")) ||
		FileName.Contains(TEXT("..")) || !FPaths::GetExtension(FileName).Equals(TEXT("mp4"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("Use a movie filename ending in .mp4, without a folder path.");
		return false;
	}
	for (const FString& Directory : {VideoDirectory(), FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("DriveInVideos"))})
	{
		const FString Candidate = Directory / FileName;
		if (IFileManager::Get().FileExists(*Candidate)) { OutPath = Candidate; return true; }
	}
	OutError = FString::Printf(TEXT("Put %s in %s, then enter the code again."), *FileName, *VideoDirectory());
	return false;
}

FVector2D SimCopterDriveIn::FitVideo(float VideoAspect, float ScreenAspect)
{
	if (VideoAspect <= 0 || ScreenAspect <= 0) return FVector2D(1, 1);
	return VideoAspect > ScreenAspect ? FVector2D(1, ScreenAspect / VideoAspect) : FVector2D(VideoAspect / ScreenAspect, 1);
}

void ASimCity2000CityActor::GetDriveInTiles(TArray<FIntPoint>& OutTiles) const
{
	OutTiles.Reset();
	for (const auto& Building : Buildings)
	{
		if (!Building.bDemolished && Building.XbldId == 182)
			OutTiles.Add(Building.OriginTile + FIntPoint(1, 1));
	}
}

void ASimCity2000CityActor::GetDriveInSurfaces(TArray<FSimCopterDriveInSurface>& OutSurfaces) const
{
	// SCHOOK: MovieTexture 0x0049ab10 / 0x0049ad10. CO182 object 7 samples SIM3D
	// page 2, cell 1; derive the screen from that authored face, not guessed dimensions.
	FMaxisMeshLibrary Library; FString Error;
	if (!Library.LoadFromOriginalGameRoot(SimCopterOriginalGame::ResolveRoot(), Error)) return;
	const FMaxisMeshObject* Object = Library.FindObjectByObjectId(7);
	if (!Object) return;
	for (const auto& Face : Object->Faces)
	{
		if (Face.FaceType != 18 || Face.TextureAtlasIndex != 2 || Face.MaterialIndex != 1 || Face.VertexIndices.Num() != 4) continue;
		for (const auto& Building : Buildings)
		{
			if (Building.bDemolished || Building.XbldId != 182) continue;
			FSimCopterDriveInSurface Surface; Surface.Tile = Building.OriginTile;
			for (uint16 Index : Face.VertexIndices)
			{
				FVector Point = FMaxisMeshReader::ConvertMaxisVertexToUnreal(Object->Vertices[Index], OriginalMeshUnitsPerCentimeter) *
					(GetTileSize() / OriginalMeshSourceTileSize);
				Point.X = -Point.X; Point.Y = -Point.Y;
				Surface.Corners.Add(GetActorTransform().TransformPosition(Building.PlacementOrigin + Point));
			}
			OutSurfaces.Add(MoveTemp(Surface));
		}
	}
}

ASimCopterDriveInPlayer::ASimCopterDriveInPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Materials/M_SimCopterDriveIn.M_SimCopterDriveIn"));
	ScreenParent = Material.Object;
}

ASimCopterDriveInPlayer* ASimCopterDriveInPlayer::Get(UWorld* World)
{
	if (!World) return nullptr;
	for (TActorIterator<ASimCopterDriveInPlayer> It(World); It; ++It) return *It;
	return World->SpawnActor<ASimCopterDriveInPlayer>();
}

void ASimCopterDriveInPlayer::ClearScreens()
{
	for (auto Sound : Sounds) if (Sound) { Sound->Stop(); Sound->DestroyComponent(); }
	for (auto Screen : Screens) if (Screen) Screen->DestroyComponent();
	Sounds.Reset(); Screens.Reset(); Cities.Reset(); Surfaces.Reset();
}

FString ASimCopterDriveInPlayer::PlayVideo(const FString& FileName, bool bToggle)
{
	if (bToggle && CurrentFile == FileName) { StopVideo(); return TEXT("Drive-in movie stopped."); }
	FString Path, Error;
	if (!SimCopterDriveIn::ResolveVideo(FileName, Path, Error)) return Error;
	if (!ScreenParent) return TEXT("Drive-in screen material is unavailable.");
	TArray<FSimCopterDriveInSurface> NewSurfaces;
	TArray<TWeakObjectPtr<ASimCity2000CityActor>> NewCities;
	for (TActorIterator<ASimCity2000CityActor> City(GetWorld()); City; ++City)
	{
		const int32 Before = NewSurfaces.Num();
		City->GetDriveInSurfaces(NewSurfaces);
		for (int32 Index = Before; Index < NewSurfaces.Num(); ++Index) NewCities.Add(*City);
	}
	if (NewSurfaces.IsEmpty()) return TEXT("This city has no standing drive-in theater available.");
	StopVideo();
	Surfaces = MoveTemp(NewSurfaces); Cities = MoveTemp(NewCities);
	if (!Player)
	{
		Player = NewObject<UMediaPlayer>(this);
		Player->PlayOnOpen = false;
		Player->OnMediaOpened.AddDynamic(this, &ASimCopterDriveInPlayer::MediaOpened);
		Player->OnMediaOpenFailed.AddDynamic(this, &ASimCopterDriveInPlayer::MediaFailed);
		Texture = NewObject<UMediaTexture>(this);
		Texture->NewStyleOutput = true;
		Texture->Filter = TF_Bilinear;
		Texture->SetMediaPlayer(Player); Texture->AutoClear = true;
		Texture->ClearColor = FLinearColor::Black;
		Texture->UpdateResource();
		ScreenMaterial = UMaterialInstanceDynamic::Create(ScreenParent, this);
		ScreenMaterial->SetTextureParameterValue(TEXT("VideoTexture"), Texture);
	}
	Player->SetLooping(true);
	for (int32 Index = 0; Index < Surfaces.Num(); ++Index)
	{
		auto* Screen = NewObject<UProceduralMeshComponent>(this);
		Screen->SetupAttachment(RootComponent); Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Screen->SetCastShadow(false); Screen->RegisterComponent();
		Screen->SetMaterial(0, ScreenMaterial); Screen->SetMaterial(1, ScreenMaterial);
		Screens.Add(Screen); BuildScreen(Index, 1);
		auto* Sound = NewObject<UMediaSoundComponent>(this);
		Sound->SetupAttachment(RootComponent); Sound->bAllowSpatialization = true;
		Sound->bOverrideAttenuation = true; Sound->AttenuationOverrides.bAttenuate = true;
		Sound->AttenuationOverrides.AttenuationShapeExtents = FVector(300);
		Sound->AttenuationOverrides.FalloffDistance = 4000;
		Sound->SetMediaPlayer(Player); Sound->RegisterComponent();
		Sound->SetWorldLocation((Surfaces[Index].Corners[0] + Surfaces[Index].Corners[2]) * 0.5);
		Sound->SetVolumeMultiplier(0.65f); Sounds.Add(Sound);
	}
	CurrentFile = FileName;
	if (!Player->OpenFile(Path)) { StopVideo(); return TEXT("Could not open this movie. Use H.264 video and AAC audio in an MP4 file."); }
	return FString::Printf(TEXT("Opening %s at %d drive-in theater(s). Enter Stop video to stop."), *FileName, Screens.Num());
}

void ASimCopterDriveInPlayer::BuildScreen(int32 Index, float Aspect)
{
	const auto& Corners = Surfaces[Index].Corners;
	const FVector Centre = (Corners[0] + Corners[2]) * 0.5;
	const FVector Right = (Corners[1] - Corners[0]) * 0.5;
	const FVector Down = (Corners[3] - Corners[0]) * 0.5;
	// The authored top-left/right/bottom-left axes face the parking lot in this order.
	// Reversing them puts the picture behind both the original screen and our black backing.
	const FVector Normal = FVector::CrossProduct(Right, Down).GetSafeNormal();
	const FVector2D Fit = SimCopterDriveIn::FitVideo(Aspect, Right.Size() / Down.Size());
	const TArray<int32> Triangles = {0, 1, 2, 0, 2, 3};
	const TArray<FVector2D> UV = {{0,0}, {1,0}, {1,1}, {0,1}};
	TArray<FVector> Normals; Normals.Init(Normal, 4);
	TArray<FProcMeshTangent> Tangents; Tangents.Init(FProcMeshTangent(Right.GetSafeNormal(), false), 4);
	// Black backing plus an inset picture: aspect ratios fit without stretching or cropping.
	for (int32 Part = 0; Part < 2; ++Part)
	{
		const FVector R = Right * (Part ? Fit.X : 1), D = Down * (Part ? Fit.Y : 1);
		const FVector C = Centre + Normal * (Part ? 1.5 : 1.0);
		TArray<FVector> Vertices = {C-R-D, C+R-D, C+R+D, C-R+D};
		TArray<FLinearColor> Colors; Colors.Init(Part ? FLinearColor::White : FLinearColor::Black, 4);
		Screens[Index]->CreateMeshSection_LinearColor(Part, Vertices, Triangles, Normals, UV, Colors, Tangents, false);
	}
}

void ASimCopterDriveInPlayer::MediaOpened(FString)
{
	if (CurrentFile.IsEmpty()) return;
	const float Aspect = Player->GetVideoTrackAspectRatio(INDEX_NONE, INDEX_NONE);
	for (int32 Index = 0; Index < Screens.Num(); ++Index) BuildScreen(Index, Aspect);
	bWasPaused = UGameplayStatics::IsGamePaused(this);
	if (!bWasPaused) Player->Play();
	for (auto Sound : Sounds) Sound->Start();
}

void ASimCopterDriveInPlayer::MediaFailed(FString)
{
	StopVideo();
	for (TActorIterator<ASimCopterMissionSystemActor> It(GetWorld()); It; ++It)
		It->ShowAirOperationsMessage(TEXT("Movie could not be decoded. Use H.264/AAC MP4."));
}

void ASimCopterDriveInPlayer::StopVideo()
{
	CurrentFile.Reset();
	if (Player) Player->Close();
	ClearScreens();
}

void ASimCopterDriveInPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Player || CurrentFile.IsEmpty()) return;
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	if (bPaused != bWasPaused) { if (bPaused) Player->Pause(); else Player->Play(); bWasPaused = bPaused; }
	ScreenMaterial->SetScalarParameterValue(TEXT("EmissiveNits"), USimCopterEffectExposureSubsystem::GetEffectEmissiveNitsForWorld(GetWorld()) * 0.6f);
	bool bAnyStanding = false;
	for (int32 Index = 0; Index < Screens.Num(); ++Index)
	{
		const bool bStanding = Cities[Index].IsValid() && Cities[Index]->HasStandingBuildingAtTile(Surfaces[Index].Tile.X, Surfaces[Index].Tile.Y);
		Screens[Index]->SetVisibility(bStanding);
		Sounds[Index]->SetVolumeMultiplier(bStanding && !bPaused ? 0.65f : 0.0f);
		bAnyStanding |= bStanding;
	}
	if (!bAnyStanding) StopVideo();
}

void ASimCopterDriveInPlayer::EndPlay(const EEndPlayReason::Type Reason)
{
	StopVideo(); Super::EndPlay(Reason);
}

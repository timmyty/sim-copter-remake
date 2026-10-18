// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SimCopterHangarArt.h"

#include "Engine/Texture2D.h"
#include "Flight/SimCopterHelicopterRegistry.h"
#include "Formats/MaxisWindowsBitmapReader.h"
#include "Ground/SimCopterPopulationSprite.h"
#include "Ground/SimCopterPopulationFigure.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Styling/SlateBrush.h"

namespace
{
// cat_<model>.bmp, in catalog row order.
const TCHAR* const CatalogDrawingFiles[SimCopterHangarLayout::CivilianCatalogTabCount] = {
	TEXT("CAT_SCHW.BMP"), // 0 Schweizer 300
	TEXT("CAT_JET.BMP"),  // 1 Jet Ranger
	TEXT("CAT_HUGH.BMP"), // 2 MD 500 (the exe calls it Hughes 500)
	TEXT("CAT_MD5.BMP"),  // 3 MD 520
	TEXT("CAT_BELL.BMP"), // 4 Bell 212
	TEXT("CAT_AUG.BMP"),  // 5 Agusta
	TEXT("CAT_DAUP.BMP"), // 6 Dauphin
	TEXT("CAT_MDE.BMP"),  // 7 MD Explorer
};

// cat_<model>t.bmp, same order, plus the upgrades page's strip.
const TCHAR* const CatalogTabStripFiles[SimCopterHangarLayout::CivilianCatalogTabCount] = {
	TEXT("CAT_SCHT.BMP"),
	TEXT("CAT_JETT.BMP"),
	TEXT("CAT_HUGT.BMP"),
	TEXT("CAT_MD5T.BMP"),
	TEXT("CAT_BELT.BMP"),
	TEXT("CAT_AUGT.BMP"),
	TEXT("CAT_DAUT.BMP"),
	TEXT("CAT_MDET.BMP"),
};

const TCHAR* const UpgradesTabStripFile = TEXT("CAT_EQUT.BMP");

FString GetUpscaledSlateFileName(const FString& OriginalFileName)
{
	const FString BaseName = FPaths::GetBaseFilename(OriginalFileName);

	if (BaseName.Equals(TEXT("BUTTON"), ESearchCase::IgnoreCase))
	{
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("MSSN_BTN-upscaled.png"))))
		{
			return TEXT("MSSN_BTN-upscaled.png");
		}
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("BUTTON-upscaled.png"))))
		{
			return TEXT("BUTTON-upscaled.png");
		}
	}
	else if (BaseName.Equals(TEXT("MSSN_BTN"), ESearchCase::IgnoreCase))
	{
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("MSSN_BTN-upscaled.png"))))
		{
			return TEXT("MSSN_BTN-upscaled.png");
		}
	}
	else if (BaseName.Equals(TEXT("MAIN1"), ESearchCase::IgnoreCase))
	{
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("MAIN1-upscaled-rows-off.png"))))
		{
			return TEXT("MAIN1-upscaled-rows-off.png");
		}
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("MAIN1-upscaled.png"))))
		{
			return TEXT("MAIN1-upscaled.png");
		}
	}

	else if (BaseName.Equals(TEXT("SLIDERBH"), ESearchCase::IgnoreCase))
	{
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("SLIDERBH-no-blue.png"))))
		{
			return TEXT("SLIDERBH-no-blue.png");
		}
	}
	else if (BaseName.StartsWith(TEXT("red-light"), ESearchCase::IgnoreCase))
	{
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("red-light-upscaled.png"))))
		{
			return TEXT("red-light-upscaled.png");
		}
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), TEXT("red-light.png"))))
		{
			return TEXT("red-light.png");
		}
	}

	const FString Candidate = BaseName + TEXT("-upscaled.png");
	if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), Candidate)))
	{
		return Candidate;
	}

	const FString CandidatePng = BaseName + TEXT(".png");
	if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), CandidatePng)))
	{
		return CandidatePng;
	}

	return FString();
}
}

const FSlateBrush* USimCopterHangarArt::GetPassengerPortrait(
	const FString& FigureName, int32 ClothesOffset, int32 HeadIndex, int32 Mood, int32 CareFrame)
{
	HeadIndex = FMath::Clamp(HeadIndex, 0, 10);
	Mood = FMath::Clamp(Mood, 0, 2);
	CareFrame = CareFrame < 0 ? INDEX_NONE : CareFrame % 6;
	const FString Key = FString::Printf(TEXT("Person:%s:%d:%d:%d:%d"), *FigureName, ClothesOffset, HeadIndex, Mood, CareFrame);
	if (const auto* Cached = Brushes.Find(Key)) return Cached->Get();
	FString Error;
	const auto Shared = FSimCopterPopulationFigure::GetShared(OriginalGameRoot, Error);
	if (!Shared.IsValid()) return nullptr;
	const int32 FigureIndex = Shared->Model.FindFigureIndex(FigureName);
	if (!Shared->Model.Figures.IsValidIndex(FigureIndex)) return nullptr;
	const auto& Figure = Shared->Model.Figures[FigureIndex];
	// ARCP torso parts measured in the shipped NoMo poses. The first part of these four
	// figures is the belt/pelvis, while Ne0 is their shirt; other human figures start at it.
	const bool bSecondPart = FigureName == TEXT("pilot") || FigureName == TEXT("Kopp") ||
		FigureName == TEXT("5man") || FigureName == TEXT("Fireman");
	if (!Figure.Parts.IsValidIndex(bSecondPart ? 1 : 0)) return nullptr;
	const auto& Shirt = Figure.Parts[bSecondPart ? 1 : 0];
	const int32 ShirtRamp = Shirt.FixedColor == 0
		? (Shirt.ColorIndex + FMath::Clamp(ClothesOffset, 0, 13)) % 14 : Shirt.ColorIndex;
	// PEOPLE1's clothing ramps, read from the original palette. Limit replacements to
	// the shoulders so hair, skin, hats, bandages and all three expressions stay intact.
	static constexpr int32 PortraitShirtRamp[] = {4, 8, 0, 9, 5, 2, 5, 3, 11, 6, 2};
	FMaxisTextureImage Sheet;
	if (!FMaxisWindowsBitmapReader::LoadPalettedBitmapFromFile(
		ResolveBitmapPath(TEXT("PEOPLE1.BMP")), Sheet, Error, TransparentPaletteIndex)) return nullptr;
	constexpr int32 Width = FSimCopterPopulationSprite::People1FrameWidth;
	constexpr int32 Height = FSimCopterPopulationSprite::People1FrameHeight;
	if (Sheet.Width < (HeadIndex + 2) * Width || Sheet.Height < (Mood + 1) * Height) return nullptr;
	FMaxisTextureImage Portrait;
	Portrait.Width = Width; Portrait.Height = Height;
	Portrait.Pixels.SetNumUninitialized(Width * Height);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			FColor Pixel = Sheet.Pixels[(Mood * Height + Y) * Sheet.Width + (HeadIndex + 1) * Width + X];
			if (Y >= 24 && Pixel.A != 0)
			{
				for (int32 Shade = 0; Shade < 16; ++Shade)
				{
					const int32 Source = 16 + PortraitShirtRamp[HeadIndex] * 16 + Shade;
					const int32 Target = 0x20 + ShirtRamp * 16 + Shade;
					if (!Shared->Palette.IsValidIndex(Source) || !Shared->Palette.IsValidIndex(Target)) continue;
					// PEOPLE1 stores the same ramps at VGA precision (each RGB channel rounded
					// down to a multiple of four); GEO's CMAP retains all eight bits.
					const auto& ModelColor = Shared->Palette[Source];
					const FColor PortraitColor(ModelColor.R & 0xfc, ModelColor.G & 0xfc, ModelColor.B & 0xfc);
					if (Pixel == PortraitColor)
					{
						Pixel = Shared->Palette[Target];
						break;
					}
				}
			}
			Portrait.Pixels[Y * Width + X] = Pixel;
		}
	}
	if (CareFrame != INDEX_NONE)
	{
		// A requested presentation addition, not an original PEOPLE1 animation. Preserve the
		// face/mood above the shoulders; animate uniform sleeves and gloved hands tending a
		// patient in the foreground. Six cached pixel-art frames need no per-frame uploads.
		auto Rect = [&](int32 X, int32 Y, int32 W, int32 H, FColor Color)
		{
			for (int32 Row = FMath::Max(0, Y); Row < FMath::Min(Height, Y + H); ++Row)
				for (int32 Col = FMath::Max(0, X); Col < FMath::Min(Width, X + W); ++Col)
					Portrait.Pixels[Row * Width + Col] = Color;
		};
		const FColor Sleeve = Shared->Palette[0x24 + ShirtRamp * 16];
		const FColor Glove(206, 234, 229), GloveShade(105, 158, 158), Outline(32, 52, 58);
		const int32 HandY = 25 + (CareFrame == 1 || CareFrame == 4 ? 2 : CareFrame == 2 || CareFrame == 5 ? 1 : 0);
		Rect(3, 30, 21, 3, Outline);
		Rect(4, 30, 19, 2, FColor(188, 205, 207)); // patient blanket
		Rect(5, 23, 4, 5, Sleeve); Rect(18, 23, 4, 5, Sleeve);
		for (int32 I = 0; I < 5; ++I)
		{
			const int32 Y = 24 + (HandY - 24) * I / 4;
			Rect(7 + I, Y, 3, 3, Sleeve); Rect(17 - I, Y, 3, 3, Sleeve);
		}
		Rect(10, HandY, 8, 4, Outline);
		Rect(11, HandY, 6, 3, GloveShade);
		Rect(11, HandY, 5, 2, Glove);
		Rect(13, HandY + 2, 4, 1, Glove);
	}
	auto* Texture = FSimCopterPopulationSprite::CreateTextureFromImage(this, Portrait, TEXT("PassengerPortrait"));
	if (!Texture) return nullptr;
	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->ImageSize = FVector2D(Width, Height);
	Brush->DrawAs = ESlateBrushDrawType::Image;
	Textures.Add(Key, Texture);
	Brushes.Add(Key, Brush);
	return Brush.Get();
}

void USimCopterHangarArt::SetOriginalGameRoot(const FString& InOriginalGameRoot)
{
	if (OriginalGameRoot == InOriginalGameRoot)
	{
		return;
	}

	OriginalGameRoot = InOriginalGameRoot;
	StopMenuSkyMovie();
	StopCareerCityMovies();
	MenuSkyMovieBrush.Reset();
	MenuSkyTexture = nullptr;
	MenuSkyPlayer = nullptr;
	CareerCityMovieBrushes.Reset();
	CareerCityTextures.Reset();
	CareerCityPlayers.Reset();
	Textures.Reset();
	Brushes.Reset();
}

bool USimCopterHangarArt::IsUsable() const
{
	return !OriginalGameRoot.IsEmpty() && FPaths::DirectoryExists(FPaths::Combine(OriginalGameRoot, TEXT("BMP")));
}

FString USimCopterHangarArt::ResolveBitmapPath(const FString& FileName) const
{
	if (OriginalGameRoot.IsEmpty() || FileName.IsEmpty())
	{
		return FString();
	}

	// The shipped folder is upper case on the reference install, but a copy made on a
	// case-sensitive filesystem may not be, so try the obvious spellings before giving up.
	const TCHAR* const Folders[] = { TEXT("BMP"), TEXT("bmp") };
	const FString Names[] = { FileName, FileName.ToUpper(), FileName.ToLower() };

	for (const TCHAR* Folder : Folders)
	{
		for (const FString& Name : Names)
		{
			const FString Candidate = FPaths::Combine(OriginalGameRoot, Folder, Name);
			if (IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
		}
	}

	return FString();
}

const FSlateBrush* USimCopterHangarArt::GetBitmap(const FString& FileName, const bool bColorKeyed)
{
	const FString UpscaledName = GetUpscaledSlateFileName(FileName);
	if (!UpscaledName.IsEmpty())
	{
		if (const FSlateBrush* Upscaled = GetBundledSlateImage(UpscaledName))
		{
			return Upscaled;
		}
	}
	// Opaque inspection and keyed page rendering must not share a cached texture:
	// otherwise whichever loads MBox.bmp first decides whether its cyan corners show.
	const FString CacheKey = FileName.ToLower() + (bColorKeyed ? TEXT("|keyed") : TEXT("|opaque"));
	return BuildBrush(CacheKey, FileName, bColorKeyed, FIntRect(), ESimCopterArtRotation::None);
}

void USimCopterHangarArt::RegisterRuntimeTexture(const FString& Key, UTexture2D* Texture)
{
	if (Texture != nullptr)
	{
		Textures.Add(Key, Texture);
	}
}

UTexture2D* USimCopterHangarArt::FindRuntimeTexture(const FString& Key) const
{
	const TObjectPtr<UTexture2D>* Found = Textures.Find(Key);
	return (Found != nullptr) ? Found->Get() : nullptr;
}

const FSlateBrush* USimCopterHangarArt::GetBundledSlateImage(const FString& FileName)
{
	const FString CacheKey = FString::Printf(TEXT("Slate/%s"), *FileName);
	if (const TSharedPtr<FSlateBrush>* Existing = Brushes.Find(CacheKey))
	{
		return Existing->IsValid() ? Existing->Get() : nullptr;
	}

	Brushes.Add(CacheKey, nullptr);

	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Slate"), FileName);
	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Path);
	if (Texture == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter bundled art: could not load '%s'."), *Path);
		return nullptr;
	}

	Texture->Filter = TF_Bilinear;
	Texture->UpdateResource();
	Textures.Add(CacheKey, Texture);

	TSharedRef<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
	Brush->DrawAs = ESlateBrushDrawType::Image;
	Brushes.Add(CacheKey, Brush);

	return &Brush.Get();
}

FString USimCopterHangarArt::ResolveMenuSkyMoviePath() const
{
	// Generated is intentionally gitignored alongside the user's original art. The bake tool
	// preserves the original frame count and timing while changing only the unsupported Smacker
	// container/codec into a Media Foundation-readable MP4.
	TArray<FString, TInlineAllocator<3>> Candidates;
	Candidates.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Generated/Movies/MENUSKY.mp4")));
	Candidates.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies/MENUSKY.mp4")));
	if (!OriginalGameRoot.IsEmpty())
	{
		Candidates.Add(FPaths::Combine(OriginalGameRoot, TEXT("SMK/MENUSKY.mp4")));
	}

	for (FString Candidate : Candidates)
	{
		Candidate = FPaths::ConvertRelativePathToFull(Candidate);
		FPaths::NormalizeFilename(Candidate);
		// Deliberately the PHYSICAL file system, not IFileManager: Media Foundation opens the file
		// through the OS and cannot see inside a .pak, so a movie that is only staged as UFS
		// "exists" to Unreal and then decodes nothing, leaving the media texture full of garbage on
		// screen. Answering "no movie" here gets the SKYCOOL fallback instead, which at least
		// looks deliberate. DefaultGame.ini stages this folder as NonUFS to keep it a real file.
		if (IPlatformFile::GetPlatformPhysical().FileExists(*Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

const FSlateBrush* USimCopterHangarArt::GetMenuSkyMovieBrush()
{
	const FString MoviePath = ResolveMenuSkyMoviePath();
	if (MoviePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SimCopter front end: MENUSKY.mp4 has not been baked; run Tools/Unreal/BakeMenuSky.py."));
		return nullptr;
	}

	if (MenuSkyPlayer == nullptr)
	{
		MenuSkyPlayer = NewObject<UMediaPlayer>(this, TEXT("OriginalMenuSkyPlayer"));
		MenuSkyPlayer->PlayOnOpen = true;
	}
	if (MenuSkyTexture == nullptr)
	{
		MenuSkyTexture = NewObject<UMediaTexture>(this, TEXT("OriginalMenuSkyTexture"));
		MenuSkyTexture->AutoClear = true;
		MenuSkyTexture->ClearColor = FLinearColor(0.28f, 0.58f, 0.72f, 1.0f);
		MenuSkyTexture->NewStyleOutput = true;
		MenuSkyTexture->Filter = TF_Bilinear;
		MenuSkyTexture->SetMediaPlayer(MenuSkyPlayer);
		MenuSkyTexture->UpdateResource();
	}
	if (!MenuSkyMovieBrush.IsValid())
	{
		MenuSkyMovieBrush = MakeShared<FSlateBrush>();
		MenuSkyMovieBrush->SetResourceObject(MenuSkyTexture);
		MenuSkyMovieBrush->ImageSize = FVector2D(640.0f, 480.0f);
		MenuSkyMovieBrush->DrawAs = ESlateBrushDrawType::Image;
	}

	// SCHOOK: MainMenuSkyMovie 0x0044d070. The original opens MENUSKY.SMK, binds the display
	// palette, then writes 1 to movie+8: its loop flag. OpenFile is asynchronous, just as the
	// original movie object is; PlayOnOpen starts it once Media Foundation has the first sample.
	MenuSkyPlayer->SetLooping(true);
	if (!MenuSkyPlayer->OpenFile(MoviePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter front end: could not open '%s'."), *MoviePath);
		return nullptr;
	}

	return MenuSkyMovieBrush.Get();
}

void USimCopterHangarArt::StopMenuSkyMovie()
{
	if (MenuSkyPlayer != nullptr)
	{
		MenuSkyPlayer->Close();
	}
}

FString USimCopterHangarArt::ResolveCareerCityMoviePath(const int32 CityIndex) const
{
	if (CityIndex < 0 || CityIndex >= 30)
	{
		return FString();
	}

	const FString FileName = FString::Printf(TEXT("CITY%d_S.mp4"), CityIndex);
	TArray<FString, TInlineAllocator<3>> Candidates;
	Candidates.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Generated/Movies/Career"), FileName));
	Candidates.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Movies/Career"), FileName));
	if (!OriginalGameRoot.IsEmpty())
	{
		Candidates.Add(FPaths::Combine(OriginalGameRoot, TEXT("SMK"), FileName));
	}

	for (FString Candidate : Candidates)
	{
		Candidate = FPaths::ConvertRelativePathToFull(Candidate);
		FPaths::NormalizeFilename(Candidate);
		if (IPlatformFile::GetPlatformPhysical().FileExists(*Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

const FSlateBrush* USimCopterHangarArt::GetCareerCityMovieBrush(const int32 CityIndex)
{
	if (const TSharedPtr<FSlateBrush>* Existing = CareerCityMovieBrushes.Find(CityIndex))
	{
		// CloseScreen closes players but keeps the art cache. Reentering New Career
		// must restart the original rotating preview, in either gameplay mode.
		// Keep the brush/texture stable and do not restart an asynchronous open.
		if (UMediaPlayer* Player = CareerCityPlayers.FindRef(CityIndex))
		{
			Player->SetLooping(true);
			if (Player->IsClosed() || Player->HasError())
			{
				const FString MoviePath = ResolveCareerCityMoviePath(CityIndex);
				if (MoviePath.IsEmpty() || !Player->OpenFile(MoviePath)) return nullptr;
			}
			else if (!Player->IsPreparing() && !Player->IsPlaying()) Player->Play();
		}
		return Existing->IsValid() ? Existing->Get() : nullptr;
	}

	const FString MoviePath = ResolveCareerCityMoviePath(CityIndex);
	if (MoviePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SimCopter career select: CITY%d_S.mp4 has not been baked; run Tools/Unreal/BakeCareerPreviews.py."),
			CityIndex);
		return nullptr;
	}

	UMediaPlayer* Player = NewObject<UMediaPlayer>(
		this, *FString::Printf(TEXT("OriginalCareerCity%dPlayer"), CityIndex));
	Player->PlayOnOpen = true;
	Player->SetLooping(true);

	UMediaTexture* Texture = NewObject<UMediaTexture>(
		this, *FString::Printf(TEXT("OriginalCareerCity%dTexture"), CityIndex));
	Texture->AutoClear = true;
	Texture->ClearColor = FLinearColor(0.45f, 0.58f, 0.63f, 1.0f);
	Texture->NewStyleOutput = true;
	Texture->Filter = TF_Bilinear;
	Texture->SetMediaPlayer(Player);
	Texture->UpdateResource();

	TSharedRef<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->ImageSize = FVector2D(200.0f, 108.0f);
	Brush->DrawAs = ESlateBrushDrawType::Image;

	CareerCityPlayers.Add(CityIndex, Player);
	CareerCityTextures.Add(CityIndex, Texture);
	CareerCityMovieBrushes.Add(CityIndex, Brush);

	// SCHOOK: CareerSelectPage 0x00457c90 / FUN_00407c50(2, city<N>) appends
	// "_s.smk" and loops that city's 200x108, 75-frame rotating layout inside its panel.
	if (!Player->OpenFile(MoviePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter career select: could not open '%s'."), *MoviePath);
		CareerCityMovieBrushes.Remove(CityIndex);
		CareerCityTextures.Remove(CityIndex);
		CareerCityPlayers.Remove(CityIndex);
		return nullptr;
	}

	return &Brush.Get();
}

void USimCopterHangarArt::StopCareerCityMovies()
{
	for (const TPair<int32, TObjectPtr<UMediaPlayer>>& Entry : CareerCityPlayers)
	{
		if (Entry.Value != nullptr)
		{
			Entry.Value->Close();
		}
	}
}

const FSlateBrush* USimCopterHangarArt::GetStripFrame(
	const FString& FileName,
	const int32 FrameIndex,
	const int32 FrameCount)
{
	if (FrameCount <= 0 || FrameIndex < 0 || FrameIndex >= FrameCount)
	{
		return nullptr;
	}

	const FString UpscaledName = GetUpscaledSlateFileName(FileName);
	if (!UpscaledName.IsEmpty())
	{
		if (const FSlateBrush* UpscaledBrush = GetBundledSlateImage(UpscaledName))
		{
			const FString CacheKey = FString::Printf(TEXT("StripFrame/%s#%dof%d"), *FileName, FrameIndex, FrameCount);
			if (const TSharedPtr<FSlateBrush>* Existing = Brushes.Find(CacheKey))
			{
				return Existing->IsValid() ? Existing->Get() : nullptr;
			}

			const float U0 = static_cast<float>(FrameIndex) / static_cast<float>(FrameCount);
			const float U1 = static_cast<float>(FrameIndex + 1) / static_cast<float>(FrameCount);

			TSharedRef<FSlateBrush> FrameBrush = MakeShared<FSlateBrush>(*UpscaledBrush);
			FrameBrush->SetUVRegion(FBox2f(FVector2f(U0, 0.0f), FVector2f(U1, 1.0f)));

			const float FrameAspect = (UpscaledBrush->ImageSize.X / static_cast<float>(FrameCount)) / FMath::Max(1.0f, UpscaledBrush->ImageSize.Y);
			const float FrameHeight = 28.0f;
			FrameBrush->ImageSize = FVector2D(FrameHeight * FrameAspect, FrameHeight);

			Brushes.Add(CacheKey, FrameBrush);
			return &FrameBrush.Get();
		}
	}

	const FSlateBrush* Whole = BuildBrush(
		FString::Printf(TEXT("%s-WholeOriginal"), *FileName),
		FileName,
		/*bColorKeyed=*/true,
		FIntRect(),
		ESimCopterArtRotation::None);
	if (Whole == nullptr)
	{
		return nullptr;
	}

	const int32 FrameWidth = FMath::FloorToInt(Whole->ImageSize.X) / FrameCount;
	if (FrameWidth <= 0)
	{
		return nullptr;
	}

	return GetSubImage(
		FileName,
		FIntRect(FrameIndex * FrameWidth, 0, (FrameIndex + 1) * FrameWidth, FMath::FloorToInt(Whole->ImageSize.Y)),
		/*bColorKeyed=*/true);
}

const FSlateBrush* USimCopterHangarArt::GetSubImage(
	const FString& FileName,
	const FIntRect& Source,
	const bool bColorKeyed,
	const ESimCopterArtRotation Rotation,
	const bool bNearestNeighbor)
{
	if (Source.Min.X < 0 || Source.Min.Y < 0 || Source.Width() <= 0 || Source.Height() <= 0)
	{
		return nullptr;
	}

	const FString CacheKey = FString::Printf(
		TEXT("%s@%d,%d,%d,%d%s r%d%s"),
		*FileName,
		Source.Min.X,
		Source.Min.Y,
		Source.Max.X,
		Source.Max.Y,
		bColorKeyed ? TEXT("k") : TEXT(""),
		static_cast<int32>(Rotation),
		bNearestNeighbor ? TEXT(" point") : TEXT(""));

	if (const TSharedPtr<FSlateBrush>* Existing = Brushes.Find(CacheKey))
	{
		return Existing->IsValid() ? Existing->Get() : nullptr;
	}

	const FString UpscaledName = GetUpscaledSlateFileName(FileName);
	if (!bNearestNeighbor && !UpscaledName.IsEmpty() && Rotation == ESimCopterArtRotation::None)
	{
		if (const FSlateBrush* UpscaledBrush = GetBundledSlateImage(UpscaledName))
		{
			const FSlateBrush* OriginalBrush = BuildBrush(
				FString::Printf(TEXT("%s-OriginalDimensionsOnly"), *FileName),
				FileName,
				bColorKeyed,
				FIntRect(),
				ESimCopterArtRotation::None);

			float OrigWidth = OriginalBrush != nullptr ? OriginalBrush->ImageSize.X : 0.0f;
			float OrigHeight = OriginalBrush != nullptr ? OriginalBrush->ImageSize.Y : 0.0f;

			if (OrigWidth <= 0.0f || OrigHeight <= 0.0f)
			{
				const FString BaseName = FPaths::GetBaseFilename(FileName);
				if (BaseName.Equals(TEXT("CARSEL"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 557.0f;
					OrigHeight = 743.0f;
				}
				else if (BaseName.Equals(TEXT("BUTTON"), ESearchCase::IgnoreCase) || BaseName.Equals(TEXT("MSSN_BTN"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 300.0f;
					OrigHeight = 28.0f;
				}
				else if (BaseName.Equals(TEXT("FLAPBTN0"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 74.0f;
					OrigHeight = 29.0f;
				}
				else if (BaseName.Equals(TEXT("FLAPBTN1"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 38.0f;
					OrigHeight = 24.0f;
				}
				else if (BaseName.Equals(TEXT("FLAPBTN2"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 34.0f;
					OrigHeight = 29.0f;
				}
				else if (BaseName.Equals(TEXT("FLAP-dispatch"), ESearchCase::IgnoreCase) || BaseName.Equals(TEXT("FLAP_DISPATCH"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 232.0f;
					OrigHeight = 58.0f;
				}
				else if (BaseName.Equals(TEXT("FLAP-apache-missles-gun"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 138.0f;
					OrigHeight = 58.0f;
				}
				else if (BaseName.StartsWith(TEXT("FLAP"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 138.0f;
					OrigHeight = 58.0f;
				}
				else if (BaseName.StartsWith(TEXT("SLIDERBH"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 192.0f;
					OrigHeight = 32.0f;
				}
				else if (BaseName.StartsWith(TEXT("red-light"), ESearchCase::IgnoreCase))
				{
					OrigWidth = 33.0f;
					OrigHeight = 34.0f;
				}
			}

			if (OrigWidth > 0.0f && OrigHeight > 0.0f)
			{
				const float U0 = FMath::Clamp(static_cast<float>(Source.Min.X) / OrigWidth, 0.0f, 1.0f);
				const float V0 = FMath::Clamp(static_cast<float>(Source.Min.Y) / OrigHeight, 0.0f, 1.0f);
				const float U1 = FMath::Clamp(static_cast<float>(Source.Max.X) / OrigWidth, 0.0f, 1.0f);
				const float V1 = FMath::Clamp(static_cast<float>(Source.Max.Y) / OrigHeight, 0.0f, 1.0f);

				TSharedRef<FSlateBrush> SubBrush = MakeShared<FSlateBrush>(*UpscaledBrush);
				SubBrush->SetUVRegion(FBox2f(FVector2f(U0, V0), FVector2f(U1, V1)));
				SubBrush->ImageSize = FVector2D(Source.Width(), Source.Height());

				Brushes.Add(CacheKey, SubBrush);
				return &SubBrush.Get();
			}
		}
	}

	return BuildBrush(CacheKey, FileName, bColorKeyed, Source, Rotation, bNearestNeighbor);
}

const FSlateBrush* USimCopterHangarArt::GetCatalogDrawing(const int32 CatalogRow)
{
	if (CatalogRow < 0 || CatalogRow >= SimCopterHangarLayout::CivilianCatalogTabCount)
	{
		return nullptr;
	}
	return GetBitmap(CatalogDrawingFiles[CatalogRow]);
}

const FSlateBrush* USimCopterHangarArt::GetCatalogTabStrip(const int32 CatalogRow)
{
	const bool bUpgrades = CatalogRow < 0 || CatalogRow >= SimCopterHangarLayout::CivilianCatalogTabCount;
	return GetBitmap(bUpgrades ? UpgradesTabStripFile : CatalogTabStripFiles[CatalogRow]);
}

const FSlateBrush* USimCopterHangarArt::BuildBrush(
	const FString& CacheKey,
	const FString& FileName,
	const bool bColorKeyed,
	const FIntRect& Source,
	const ESimCopterArtRotation Rotation,
	const bool bNearestNeighbor)
{
	if (const TSharedPtr<FSlateBrush>* Existing = Brushes.Find(CacheKey))
	{
		// A cached miss is stored as an empty entry so a missing file is only looked for once.
		return Existing->IsValid() ? Existing->Get() : nullptr;
	}

	Brushes.Add(CacheKey, nullptr);

	const FString Path = ResolveBitmapPath(FileName);
	if (Path.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter hangar art: '%s' is not in the original game's BMP folder."), *FileName);
		return nullptr;
	}

	FMaxisTextureImage Image;
	FString Error;
	if (!FMaxisWindowsBitmapReader::LoadPalettedBitmapFromFile(
			Path,
			Image,
			Error,
			bColorKeyed ? TransparentPaletteIndex : FMaxisWindowsBitmapReader::NoTransparentPaletteIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter hangar art: %s"), *Error);
		return nullptr;
	}

	if (Source.Width() > 0 && Source.Height() > 0)
	{
		// A sub-rectangle: one frame of a button strip, or one cell of a page.
		const FIntRect Clipped(
			FMath::Clamp(Source.Min.X, 0, Image.Width),
			FMath::Clamp(Source.Min.Y, 0, Image.Height),
			FMath::Clamp(Source.Max.X, 0, Image.Width),
			FMath::Clamp(Source.Max.Y, 0, Image.Height));
		if (Clipped.Width() <= 0 || Clipped.Height() <= 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("SimCopter art: rect (%d,%d)-(%d,%d) is outside '%s' (%dx%d)."),
				Source.Min.X, Source.Min.Y, Source.Max.X, Source.Max.Y, *FileName, Image.Width, Image.Height);
			return nullptr;
		}

		FMaxisTextureImage Frame;
		Frame.Width = Clipped.Width();
		Frame.Height = Clipped.Height();
		Frame.Pixels.SetNumUninitialized(Frame.Width * Frame.Height);
		for (int32 Y = 0; Y < Frame.Height; ++Y)
		{
			FMemory::Memcpy(
				&Frame.Pixels[Y * Frame.Width],
				&Image.Pixels[(Clipped.Min.Y + Y) * Image.Width + Clipped.Min.X],
				Frame.Width * sizeof(FColor));
		}
		Image = MoveTemp(Frame);
	}

	if (Rotation != ESimCopterArtRotation::None)
	{
		FMaxisTextureImage Turned;
		Turned.Width = Image.Height;
		Turned.Height = Image.Width;
		Turned.Pixels.SetNumUninitialized(Turned.Width * Turned.Height);
		for (int32 Y = 0; Y < Image.Height; ++Y)
		{
			for (int32 X = 0; X < Image.Width; ++X)
			{
				const int32 TurnedX = Rotation == ESimCopterArtRotation::Clockwise90
					? Image.Height - 1 - Y
					: Y;
				const int32 TurnedY = Rotation == ESimCopterArtRotation::Clockwise90
					? X
					: Image.Width - 1 - X;
				Turned.Pixels[TurnedY * Turned.Width + TurnedX] = Image.Pixels[Y * Image.Width + X];
			}
		}
		Image = MoveTemp(Turned);
	}

	UTexture2D* Texture = FSimCopterPopulationSprite::CreateTextureFromImage(this, Image, TEXT("SimCopterHangarArt"));
	if (Texture == nullptr)
	{
		return nullptr;
	}

	// Full pages use smooth scaling, but the original 27x33 passenger cells must retain their
	// exact palette pixels. Point sampling also prevents transparent chroma-key neighbors from
	// contributing a cyan fringe at the silhouette.
	Texture->Filter = bNearestNeighbor ? TF_Nearest : TF_Bilinear;
	Texture->UpdateResource();

	Textures.Add(CacheKey, Texture);

	TSharedRef<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->ImageSize = FVector2D(Image.Width, Image.Height);
	Brush->DrawAs = ESlateBrushDrawType::Image;
	Brushes.Add(CacheKey, Brush);

	return &Brush.Get();
}

namespace SimCopterHangarLayout
{
int32 GetCatalogRowForTypeIndex(const int32 TypeIndex)
{
	const FSimCopterHelicopterDefinition* Definition = SimCopterHelicopterRegistry::FindByTypeIndex(TypeIndex);
	return Definition != nullptr ? Definition->CatalogIndex : INDEX_NONE;
}

int32 GetTypeIndexForCatalogRow(const int32 CatalogRow)
{
	if (CatalogRow < 0 || CatalogRow >= CatalogTabCount)
	{
		return INDEX_NONE;
	}

	for (const FSimCopterHelicopterDefinition& Definition : SimCopterHelicopterRegistry::GetDefinitions())
	{
		if (Definition.CatalogIndex == CatalogRow)
		{
			return Definition.InternalTypeIndex;
		}
	}

	return INDEX_NONE;
}
}

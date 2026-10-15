#include "City/SimCopterWaterTowers.h"

#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "EngineFontServices.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontMeasure.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterCareerProgression.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "RHI.h"

namespace SimCopterWaterTowers
{
FMaxisMeshObject PrepareModel(const FMaxisMeshObject& Original)
{
	FMaxisMeshObject Result = Original;
	if (Original.Header.Id != ObjectId) return Result;
	int32 Bottom = MAX_int32, Top = MIN_int32;
	for (const auto& Face : Original.Faces) if (Face.FaceType == 19)
		for (const uint16 Index : Face.VertexIndices) if (Original.Vertices.IsValidIndex(Index))
		{
			Bottom = FMath::Min(Bottom, Original.Vertices[Index].Y);
			Top = FMath::Max(Top, Original.Vertices[Index].Y);
		}
	if (Top <= Bottom) return Result;
	for (auto& Face : Result.Faces)
	{
		const bool bTank = Face.FaceType == 19;
		bool bRoof = !Face.VertexIndices.IsEmpty();
		for (const uint16 Index : Face.VertexIndices)
			bRoof &= Result.Vertices.IsValidIndex(Index) && FMath::Abs(int64(Result.Vertices[Index].Y) - Top) <= 655;
		// One section/material for tank, cap and frame; the red vertex channel selects the finish.
		Face.MaterialIndex = bTank ? 0 : bRoof ? 1 : 2;
		Face.RawUVs.Reset();
		double FirstU = 0;
		for (int32 Corner = 0; Corner < Face.VertexIndices.Num(); ++Corner)
		{
			const auto& V = Result.Vertices[Face.VertexIndices[Corner]];
			double U = 0.5 - FMath::Atan2(-double(V.X), -double(V.Z)) / (2.0 * PI);
			if (Corner == 0) FirstU = U;
			else if (bTank) U += FMath::RoundToDouble(FirstU - U); // unwrap the cylinder seam
			const double Height = double(V.Y - Bottom) / double(Top - Bottom);
			if (bRoof)
				Face.RawUVs.Add(FIntPoint(FMath::RoundToInt(double(V.X) / 96.0 + 32768.0),
					FMath::RoundToInt(double(V.Z) / 96.0 + 32768.0)));
			else
				Face.RawUVs.Add(FIntPoint(FMath::RoundToInt(U * 65536.0), FMath::RoundToInt(Height * 65536.0)));
		}
	}
	return Result;
}

FString ResolveCityName(const FString& SourceFile, const FString& MapName)
{
	// Career CNAM fields can differ from the name shown by the original game's city picker.
	// Match the actual original path, so an imported file merely called city0.sc2 keeps its name.
	const FString CareerDir = SimCopterOriginalGame::ResolveDirectory(TEXT("cities")) / TEXT("career");
	for (int32 Index = 0; Index < SimCopterCareerProgression::CityCount; ++Index)
		if (FPaths::IsSamePath(SourceFile, CareerDir / (SimCopterCareerProgression::GetMapBaseName(Index) + TEXT(".sc2"))))
			return FString(SimCopterCareerProgression::GetCityName(Index)).ToUpper();
	FString Name = MapName.TrimStartAndEnd();
	Name.ReplaceInline(TEXT("\r"), TEXT(" ")); Name.ReplaceInline(TEXT("\n"), TEXT(" "));
	return Name.IsEmpty() ? TEXT("WATER WORKS") : Name.ToUpper();
}

UMaterialInstanceDynamic* CreateMaterial(UObject* Owner, UMaterialInterface* Parent, const FString& CityName)
{
	if (!Parent) return nullptr;
	auto* Material = UMaterialInstanceDynamic::Create(Parent, Owner);
	if (!Material || GUsingNullRHI || !FApp::CanEverRender() || !GEngine || !Owner->GetWorld()) return Material;
	if (!FEngineFontServices::IsInitialized() || !FEngineFontServices::Get().GetFontCache() || !FEngineFontServices::Get().GetFontMeasure()) return Material;
	UFont* EngineFont = GEngine->GetLargeFont();
	if (!EngineFont) return Material;
	auto* Label = NewObject<UTextureRenderTarget2D>(Owner);
	Label->ClearColor = FLinearColor::Black;
	Label->RenderTargetFormat = RTF_RGBA8;
	Label->bAutoGenerateMips = true;
	Label->Filter = TF_Trilinear;
	Label->InitAutoFormat(LabelWidth, LabelHeight);
	Label->UpdateResourceImmediate(true);
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Owner, Label, Canvas, Size, Context);
	if (Canvas)
	{
		auto Line = [&](const FString& Text, float Y, float MaxWidth, float MaxHeight)
		{
			// Rasterize at the target size instead of enlarging the engine's small bitmap font.
			FSlateFontInfo Font = EngineFont->GetLegacySlateFontInfo();
			Font.Size = FMath::RoundToInt(MaxHeight);
			Font.TypefaceFontName = TEXT("Bold");
			Font.FontObject = EngineFont; // Canvas requires a UFont even when a Slate font is supplied.
			const FVector2D Extent = FEngineFontServices::Get().GetFontMeasure()->Measure(Text, Font);
			auto& Glyphs = FEngineFontServices::Get().GetFontCache()->GetCharacterList(Font, 1.0f);
			for (const TCHAR Character : Text) Glyphs.GetCharacter(Character, Font.FontFallback);
			// The first city load may precede Slate's normal atlas upload at frame end.
			FEngineFontServices::Get().UpdateCache();
			const float Scale = FMath::Min(MaxWidth / FMath::Max(1.0, Extent.X), MaxHeight / FMath::Max(1.0, Extent.Y));
			FCanvasTextItem Item(FVector2D((LabelWidth - Extent.X * Scale) * 0.5, Y - Extent.Y * Scale * 0.5),
				FText::FromString(Text), Font, FLinearColor::White);
			Item.Scale = FVector2D(Scale);
			Canvas->DrawItem(Item);
		};
		Line(TEXT("CITY OF"), 40, 310, 34);
		Line(CityName, 127, 920, 100);
		Line(TEXT("MUNICIPAL WATER"), 218, 540, 30);
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Owner, Context);
		// Generate the mip chain once, after drawing. No tick, readback or per-tower texture.
		Label->UpdateResourceImmediate(false);
		Material->SetTextureParameterValue(TEXT("CityLettering"), Label);
	}
	return Material;
}
}

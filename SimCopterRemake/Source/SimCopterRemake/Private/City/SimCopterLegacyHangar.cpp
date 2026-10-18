// Original JamesIV4 hangar shell retained for the optional original-style gameplay mode.
// Geometry and atlas mapping from upstream 11977d3, SimCopterHangar.cpp.
#include "City/SimCopterHangar.h"
#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "Formats/MaxisMeshReader.h"
#include "Formats/MaxisTextureReader.h"
#include "Ground/SimCopterPopulationSprite.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
namespace LegacyHangar {
constexpr float WidthTiles=1.9f, DepthTiles=1.9f, EavesHeightTiles=.5f, ApexHeightTiles=.8f;
constexpr float DoorWidthTiles=1.25f, DoorHeightTiles=.42f, FloorLiftCm=2, TextureRepeatCm=170;
constexpr int32 TexturePage=40, WallTextureCell=23, RoofTextureCell=52, FloorTextureCell=61;
}
namespace {
const FLinearColor WallColor(0.52f, 0.56f, 0.50f, 1.0f);
const FLinearColor WallShadeColor(0.40f, 0.44f, 0.39f, 1.0f);
const FLinearColor RoofColor(0.33f, 0.35f, 0.36f, 1.0f);
const FLinearColor TrimColor(0.72f, 0.55f, 0.16f, 1.0f);
const FLinearColor FloorColor(0.22f, 0.22f, 0.23f, 1.0f);

struct FShellSection
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	bool IsEmpty() const { return Triangles.Num() == 0; }

	// One quad, wound both ways: the player walks inside the shell, so every face has to be
	// visible from both sides. (SimCopterPopulationBody's box builder does the same for the same
	// reason.) UVs are given per corner so a wall can tile along its length and stretch over its
	// height without a second mapping pass.
	void AddQuad(
		const FVector& A, const FVector2D& UVA,
		const FVector& B, const FVector2D& UVB,
		const FVector& C, const FVector2D& UVC,
		const FVector& D, const FVector2D& UVD,
		const FLinearColor& Color)
	{
		const FVector Normal = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
		const FVector Tangent = (B - A).GetSafeNormal();

		const int32 Base = Vertices.Num();
		Vertices.Append({ A, B, C, D });
		UVs.Append({ UVA, UVB, UVC, UVD });
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			Normals.Add(Normal);
			VertexColors.Add(Color);
			Tangents.Add(FProcMeshTangent(Tangent, false));
		}

		Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Triangles.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}

	void AddTriangle(
		const FVector& A, const FVector2D& UVA,
		const FVector& B, const FVector2D& UVB,
		const FVector& C, const FVector2D& UVC,
		const FLinearColor& Color)
	{
		const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
		const FVector Tangent = (B - A).GetSafeNormal();

		const int32 Base = Vertices.Num();
		Vertices.Append({ A, B, C });
		UVs.Append({ UVA, UVB, UVC });
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			Normals.Add(Normal);
			VertexColors.Add(Color);
			Tangents.Add(FProcMeshTangent(Tangent, false));
		}

		Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 1 });
	}
};
}
void ASimCopterHangar::BuildLegacyShellMesh()
{
	using namespace LegacyHangar;

	const float HalfWidth = WidthTiles * TileSizeCm * 0.5f;
	const float HalfDepth = DepthTiles * TileSizeCm * 0.5f;
	const float Eaves = EavesHeightTiles * TileSizeCm;
	const float Apex = ApexHeightTiles * TileSizeCm;
	const float HalfDoor = FMath::Min(DoorWidthTiles * TileSizeCm * 0.5f, HalfWidth - 1.0f);
	const float DoorTop = FMath::Min(DoorHeightTiles * TileSizeCm, Eaves - 1.0f);

	// The doorway faces +X, which is the actor's forward axis - PlaceAtAirport yaws the whole
	// actor so that axis points at the player's start.
	const float Front = HalfDepth;
	const float Back = -HalfDepth;

	// Four surfaces, four skins: walls, roof, floor, and the painted band over the door.
	FShellSection Walls;
	FShellSection Roof;
	FShellSection Floor;
	FShellSection Trim;

	// Every wall shares one mapping so the texture runs continuously round the building: U is the
	// distance travelled along the wall in texture repeats, V is 1 at the ground and 0 at the
	// eaves - which puts the cell's dark eaves band where the eaves are.
	const float Repeat = FMath::Max(1.0f, TextureRepeatCm);
	auto WallV = [Eaves](const float Z) { return 1.0f - Z / FMath::Max(1.0f, Eaves); };

	// Floor slab, lifted clear of the apron it sits on.
	{
		const float FloorZ = FloorLiftCm;
		const float UDepth = (HalfDepth * 2.0f) / Repeat;
		const float VWidth = (HalfWidth * 2.0f) / Repeat;
		Floor.AddQuad(
			FVector(Back, -HalfWidth, FloorZ), FVector2D(0.0f, 0.0f),
			FVector(Front, -HalfWidth, FloorZ), FVector2D(UDepth, 0.0f),
			FVector(Front, HalfWidth, FloorZ), FVector2D(UDepth, VWidth),
			FVector(Back, HalfWidth, FloorZ), FVector2D(0.0f, VWidth),
			FLinearColor::White);
	}

	// Side walls up to the eaves.
	{
		const float UDepth = (HalfDepth * 2.0f) / Repeat;
		for (const float Side : { -HalfWidth, HalfWidth })
		{
			Walls.AddQuad(
				FVector(Back, Side, 0.0f), FVector2D(0.0f, WallV(0.0f)),
				FVector(Front, Side, 0.0f), FVector2D(UDepth, WallV(0.0f)),
				FVector(Front, Side, Eaves), FVector2D(UDepth, WallV(Eaves)),
				FVector(Back, Side, Eaves), FVector2D(0.0f, WallV(Eaves)),
				FLinearColor::White);
		}
	}

	// Back wall plus its gable.
	{
		const float UWidth = (HalfWidth * 2.0f) / Repeat;
		Walls.AddQuad(
			FVector(Back, -HalfWidth, 0.0f), FVector2D(0.0f, WallV(0.0f)),
			FVector(Back, HalfWidth, 0.0f), FVector2D(UWidth, WallV(0.0f)),
			FVector(Back, HalfWidth, Eaves), FVector2D(UWidth, WallV(Eaves)),
			FVector(Back, -HalfWidth, Eaves), FVector2D(0.0f, WallV(Eaves)),
			FLinearColor::White);
		Walls.AddTriangle(
			FVector(Back, -HalfWidth, Eaves), FVector2D(0.0f, WallV(Eaves)),
			FVector(Back, HalfWidth, Eaves), FVector2D(UWidth, WallV(Eaves)),
			FVector(Back, 0.0f, Apex), FVector2D(UWidth * 0.5f, WallV(Apex)),
			FLinearColor::White);
	}

	// Front wall: two jambs, a lintel, and the gable above them - the hole between is the door.
	{
		const float JambWidth = HalfWidth - HalfDoor;
		const float UJamb = JambWidth / Repeat;
		const float UDoor = (HalfDoor * 2.0f) / Repeat;
		const float UWidth = (HalfWidth * 2.0f) / Repeat;

		Walls.AddQuad(
			FVector(Front, -HalfWidth, 0.0f), FVector2D(0.0f, WallV(0.0f)),
			FVector(Front, -HalfDoor, 0.0f), FVector2D(UJamb, WallV(0.0f)),
			FVector(Front, -HalfDoor, Eaves), FVector2D(UJamb, WallV(Eaves)),
			FVector(Front, -HalfWidth, Eaves), FVector2D(0.0f, WallV(Eaves)),
			FLinearColor::White);
		Walls.AddQuad(
			FVector(Front, HalfDoor, 0.0f), FVector2D(UJamb + UDoor, WallV(0.0f)),
			FVector(Front, HalfWidth, 0.0f), FVector2D(UWidth, WallV(0.0f)),
			FVector(Front, HalfWidth, Eaves), FVector2D(UWidth, WallV(Eaves)),
			FVector(Front, HalfDoor, Eaves), FVector2D(UJamb + UDoor, WallV(Eaves)),
			FLinearColor::White);
		Walls.AddQuad(
			FVector(Front, -HalfDoor, DoorTop), FVector2D(UJamb, WallV(DoorTop)),
			FVector(Front, HalfDoor, DoorTop), FVector2D(UJamb + UDoor, WallV(DoorTop)),
			FVector(Front, HalfDoor, Eaves), FVector2D(UJamb + UDoor, WallV(Eaves)),
			FVector(Front, -HalfDoor, Eaves), FVector2D(UJamb, WallV(Eaves)),
			FLinearColor::White);
		Walls.AddTriangle(
			FVector(Front, -HalfWidth, Eaves), FVector2D(0.0f, WallV(Eaves)),
			FVector(Front, HalfWidth, Eaves), FVector2D(UWidth, WallV(Eaves)),
			FVector(Front, 0.0f, Apex), FVector2D(UWidth * 0.5f, WallV(Apex)),
			FLinearColor::White);
	}

	// Gable roof: two slopes from the eaves to the ridge.
	{
		const float UDepth = (HalfDepth * 2.0f) / Repeat;
		const float SlopeLength = FMath::Sqrt(HalfWidth * HalfWidth + (Apex - Eaves) * (Apex - Eaves));
		const float VSlope = SlopeLength / Repeat;

		Roof.AddQuad(
			FVector(Back, -HalfWidth, Eaves), FVector2D(0.0f, 0.0f),
			FVector(Front, -HalfWidth, Eaves), FVector2D(UDepth, 0.0f),
			FVector(Front, 0.0f, Apex), FVector2D(UDepth, VSlope),
			FVector(Back, 0.0f, Apex), FVector2D(0.0f, VSlope),
			FLinearColor::White);
		Roof.AddQuad(
			FVector(Back, 0.0f, Apex), FVector2D(0.0f, 0.0f),
			FVector(Front, 0.0f, Apex), FVector2D(UDepth, 0.0f),
			FVector(Front, HalfWidth, Eaves), FVector2D(UDepth, VSlope),
			FVector(Back, HalfWidth, Eaves), FVector2D(0.0f, VSlope),
			FLinearColor::White);
	}

	// A band over the doorway, so the building reads as the way in from the air as well. This one
	// stays painted rather than skinned - it is the only part that is not the original's.
	{
		const float BandZ = DoorTop + (Eaves - DoorTop) * 0.35f;
		const float BandHeight = FMath::Max(6.0f, (Eaves - DoorTop) * 0.3f);
		Trim.AddQuad(
			FVector(Front + 2.0f, -HalfDoor, BandZ), FVector2D(0.0f, 0.0f),
			FVector(Front + 2.0f, HalfDoor, BandZ), FVector2D(1.0f, 0.0f),
			FVector(Front + 2.0f, HalfDoor, BandZ + BandHeight), FVector2D(1.0f, 1.0f),
			FVector(Front + 2.0f, -HalfDoor, BandZ + BandHeight), FVector2D(0.0f, 1.0f),
			TrimColor);
	}

	// Without the original artwork the skinned sections fall back to flat paint, which is what the
	// shell looked like before the textures were wired up.
	UMaterialInterface* WallMaterial = ResolveLegacyCellMaterial(WallTextureCell);
	UMaterialInterface* RoofMaterial = ResolveLegacyCellMaterial(RoofTextureCell);
	UMaterialInterface* FloorMaterial = ResolveLegacyCellMaterial(FloorTextureCell);
	if (WallMaterial == nullptr)
	{
		for (FLinearColor& Color : Walls.VertexColors) { Color = WallColor; }
	}
	if (RoofMaterial == nullptr)
	{
		for (FLinearColor& Color : Roof.VertexColors) { Color = RoofColor; }
	}
	if (FloorMaterial == nullptr)
	{
		for (FLinearColor& Color : Floor.VertexColors) { Color = FloorColor; }
	}

	ShellMesh->ClearAllMeshSections();

	const TPair<FShellSection*, UMaterialInterface*> SectionPlan[] = {
		{ &Walls, WallMaterial },
		{ &Roof, RoofMaterial },
		{ &Floor, FloorMaterial },
		{ &Trim, nullptr },
	};

	int32 SectionIndex = 0;
	for (const TPair<FShellSection*, UMaterialInterface*>& Entry : SectionPlan)
	{
		FShellSection& Section = *Entry.Key;
		if (Section.IsEmpty())
		{
			continue;
		}

		// Only the shell proper carries collision; the painted band is a decal on the front wall
		// and would otherwise put an invisible ledge across the doorway.
		const bool bCollides = &Section != &Trim;
		ShellMesh->CreateMeshSection_LinearColor(
			SectionIndex,
			Section.Vertices,
			Section.Triangles,
			Section.Normals,
			Section.UVs,
			Section.VertexColors,
			Section.Tangents,
			bCollides);

		UMaterialInterface* Material = Entry.Value != nullptr ? Entry.Value : ShellMaterial.Get();
		if (Material != nullptr)
		{
			ShellMesh->SetMaterial(SectionIndex, Material);
		}
		++SectionIndex;
	}

	// The trigger fills the interior but stops short of the walls and the doorway, so the shell
	// only opens once the player is properly inside.
	const float Inset = FMath::Min(TileSizeCm * 0.2f, HalfWidth * 0.4f);
	EntryTrigger->SetBoxExtent(FVector(
		FMath::Max(10.0f, HalfDepth - Inset),
		FMath::Max(10.0f, HalfWidth - Inset),
		FMath::Max(10.0f, Eaves * 0.5f)));
	EntryTrigger->SetRelativeLocation(FVector(-Inset * 0.5f, 0.0f, Eaves * 0.5f));
}

UMaterialInterface* ASimCopterHangar::ResolveLegacyCellMaterial(const int32 AtlasCell)
{
	using namespace LegacyHangar;

	if (!TexturedMaterial) TexturedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SimCopterLitTexture.M_SimCopterLitTexture"));
	if (TexturedMaterial == nullptr)
	{
		return nullptr;
	}

	if (const TObjectPtr<UMaterialInterface>* Existing = CellMaterials.Find(AtlasCell))
	{
		return Existing->Get();
	}

	// A cached miss is stored so a missing install is only looked for once per cell.
	CellMaterials.Add(AtlasCell, nullptr);

	const FString RootPath = ResolveOriginalGameRoot();
	if (RootPath.IsEmpty())
	{
		return nullptr;
	}

	// The composite pages are palette-indexed, and the palette that decodes them is the GEO pack's
	// own CMAP - the same pairing the city builder uses (sim3d1.max + BMP/SIM3D.BMP).
	FMaxisMeshFile MeshFile;
	FString Error;
	if (!FMaxisMeshReader::LoadMeshFileFromFile(FPaths::Combine(RootPath, TEXT("GEO/sim3d1.max")), MeshFile, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter hangar: no palette for the wall textures - %s"), *Error);
		return nullptr;
	}

	FMaxisCompositeBitmap Pages;
	if (!FMaxisTextureReader::LoadCompositeBitmapFromFile(
			FPaths::Combine(RootPath, TEXT("BMP/SIM3D.BMP")),
			MeshFile.ColorMap,
			Pages,
			Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter hangar: could not read the texture pages - %s"), *Error);
		return nullptr;
	}

	const FMaxisTextureImage* Page = Pages.FindImage(TexturePage);
	if (Page == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter hangar: SIM3D.BMP has no page %d."), TexturePage);
		return nullptr;
	}

	FMaxisTextureImage CellImage;
	if (!FMaxisTextureReader::ExtractAtlasTile(*Page, AtlasCell, CellImage, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter hangar: %s"), *Error);
		return nullptr;
	}

	UTexture2D* Texture = FSimCopterPopulationSprite::CreateTextureFromImage(this, CellImage, TEXT("SimCopterHangarSkin"));
	if (Texture == nullptr)
	{
		return nullptr;
	}

	// A 32x32 cell tiled across a wall: wrap both ways, and keep the sprite path's nearest filter
	// so the hangar stays as crunchy as the buildings around it.
	Texture->AddressX = TA_Wrap;
	Texture->AddressY = TA_Wrap;
	Texture->UpdateResource();

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TexturedMaterial, this);
	if (Material == nullptr)
	{
		return nullptr;
	}
	Material->SetTextureParameterValue(TEXT("Texture"), Texture);

	CellMaterials.Add(AtlasCell, Material);
	return Material;
}

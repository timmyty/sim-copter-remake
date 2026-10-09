#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "City/SimCopterDriveInPlacement.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterDriveIn.h"
#include "City/SimCopterCityGeometryRules.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "UI/SimCopterMapRaster.h"
#include "UI/SimCopterMapArt.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

using namespace SimCopterDriveInPlacement;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDriveInSiteTest, "SimCopter.DriveIn.SafePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterDriveInSiteTest::RunTest(const FString&)
{
	FSimCity2000City City;
	City.Tiles.SetNum(FSimCity2000City::TileCount);
	for (auto& Tile : City.Tiles) Tile.Building = 0x70;
	// Only one possible lot; surrounding foundations constrain all its boundary vertices.
	for (int32 Y = 20; Y <= 22; ++Y)
	for (int32 X = 20; X <= 22; ++X) City.Tiles[Y * 128 + X].Building = 0;
	TArray<int16> Corners; Corners.Init(32, 129 * 129);
	const FIntPoint Site(20, 20), None(INDEX_NONE, INDEX_NONE);
	TestEqual(TEXT("Clear level lot selected"), FindSite(City, Corners), Site);
	const FSimCity2000Tile Original = City.Tiles[21 * 128 + 21];
	auto& Tile = City.Tiles[21 * 128 + 21];
	for (const uint8 Obstacle : {uint8(0x1d), uint8(0x70), uint8(0xd5), uint8(0x0e)})
	{
		Tile.Building = Obstacle;
		TestEqual(TEXT("Roads, buildings, large parks and utilities cannot be cleared"), FindSite(City, Corners), None);
	}
	Tile = Original; Tile.bWater = true;
	TestEqual(TEXT("Water excluded"), FindSite(City, Corners), None);
	Tile = Original; Tile.Altitude = 3;
	TestEqual(TEXT("Uneven ALTM excluded"), FindSite(City, Corners), None);
	Tile = Original; Tile.Zone = 7;
	TestEqual(TEXT("Airport reservations excluded"), FindSite(City, Corners), None);
	Tile = Original; Tile.Underground = 1;
	TestEqual(TEXT("Buried networks can remain under a lot"), FindSite(City, Corners), Site);
	Corners[21 * 129 + 21] = 97;
	TestEqual(TEXT("Steep rendered ground excluded even with flat ALTM"), FindSite(City, Corners), None);
	Corners[21 * 129 + 21] = 40;
	TestEqual(TEXT("Slight interior grading is allowed on a clear lot"), FindSite(City, Corners), Site);
	City.Tiles[19 * 128 + 21].Building = 0x1d;
	Corners[20 * 129 + 21] = 64;
	TestEqual(TEXT("Cannot grade a shared road foundation to another height"), FindSite(City, Corners), None);
	Corners[20 * 129 + 21] = 32;
	Stamp(City, Site, &Corners);
	TestEqual(TEXT("Foundation is actually leveled before placement"), Corners[21 * 129 + 21], int16(32));
	TestEqual(TEXT("Existing road stays intact"), City.Tiles[19 * 128 + 21].Building, uint8(0x1d));
	TestEqual(TEXT("Buried network is preserved"), City.Tiles[21 * 128 + 21].Underground, uint8(1));
	TArray<uint8> Claimed;
	TestEqual(TEXT("Stamped cells make one original 3x3 building"),
		FSimCopterCityGeometryRules::ClaimOriginalBuildingFootprint(City, 20, 20, Claimed), FIntPoint(3, 3));
	TestEqual(TEXT("Existing theater prevents duplicate"), FindSite(City, Corners), None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDriveInAllMapsTest, "SimCopter.DriveIn.AllMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterDriveInAllMapsTest::RunTest(const FString&)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *(SimCopterOriginalGame::ResolveRoot() / TEXT("cities")), TEXT("*.sc2"), true, false);
	TestTrue(TEXT("Original map collection available"), Files.Num() >= 30);
	Files.Sort();
	for (const FString& File : Files)
	{
		FSimCity2000City Original; FString Error;
		if (!TestTrue(*File, FSimCity2000Reader::LoadCityFromFile(File, Original, Error))) continue;
		FSimCity2000City City = Original;
		const TArray<int16> Corners = ASimCity2000CityActor::PrepareCityForGameplay(City);
		TArray<uint8> Claimed; int32 Count = 0;
		FIntPoint Site(INDEX_NONE, INDEX_NONE);
		for (int32 Y = 0; Y < 128; ++Y)
		for (int32 X = 0; X < 128; ++X)
		{
			if (City.Tiles[Y * 128 + X].Building != BuildingId) continue;
			if (FSimCopterCityGeometryRules::ClaimOriginalBuildingFootprint(City, X, Y, Claimed) != FIntPoint(3, 3)) continue;
			++Count; Site = FIntPoint(X, Y);
			if (Original.Tiles[Y * 128 + X].Building == BuildingId) continue;
			const int16 Height = int16((City.Tiles[Y * 128 + X].Altitude + 1) * 32);
			for (int32 Dy = 0; Dy <= Span; ++Dy)
			for (int32 Dx = 0; Dx <= Span; ++Dx)
				TestEqual(TEXT("Every added lot vertex meets the building base"), Corners[(Y + Dy) * 129 + X + Dx], Height);
			for (int32 Dy = 0; Dy < Span; ++Dy)
			for (int32 Dx = 0; Dx < Span; ++Dx)
			{
				const auto& Before = Original.Tiles[(Y + Dy) * 128 + X + Dx];
				TestTrue(TEXT("Only vacant ground or landscaping replaced"), Before.Building == 0 || (Before.Building >= 6 && Before.Building <= 13));
				TestFalse(TEXT("No water replaced"), Before.bWater);
			}
		}
		TestTrue(*FString::Printf(TEXT("%s has a complete theater"), *File), Count > 0);
		FSimCity2000City Reload = Original;
		ASimCity2000CityActor::PrepareCityForGameplay(Reload);
		for (int32 I = 0; I < City.Tiles.Num(); ++I)
			if (City.Tiles[I].Building != Reload.Tiles[I].Building || City.Tiles[I].Zone != Reload.Tiles[I].Zone)
				{ AddError(TEXT("Placement changed on reload")); break; }
		AddInfo(FString::Printf(TEXT("%s: %d theater(s), tile %d,%d"), *File, Count, Site.X, Site.Y));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDriveInMarkerTest, "SimCopter.DriveIn.MinimapLogo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterDriveInMarkerTest::RunTest(const FString&)
{
	using namespace SimCopterMap;
	FSimCopterMapFrame Frame; Frame.DriveInTiles.Add(FIntPoint(67, 67));
	FSimCopterMapSettings Settings; Settings.bShowServiceBlips = Settings.bShowMissionBlips = false;
	FSimCopterMapRaster Raster;
	FSimCopterMapCity Map;
	Map.Xbld.Init(0, MapTiles * MapTiles); Map.TerrainClass.Init(0x20, MapTiles * MapTiles);
	Map.AltitudeShade.Init(0, MapTiles * MapTiles); Frame.City = &Map;
	TArray<FColor> Palette; FSimCopterMapArt::LoadPalette(SimCopterOriginalGame::ResolveRoot(), Palette);
	for (int32 Zoom = 0; Zoom <= MaxZoom; ++Zoom)
	{
		Settings.Zoom = Zoom; Raster.Render(Frame, Settings);
		const int32 X = CentreX - 3 * (1 << Zoom), Y = CentreY - 3 * (1 << Zoom);
		TestEqual(TEXT("Cinema screen border at correct north-up coordinate"), Raster.GetPixel(X - 4, Y - 4), Color::LabelText);
		TestEqual(TEXT("Cinema play symbol visible at every zoom"), Raster.GetPixel(X + 1, Y - 1), Color::Heading);
		if (Palette.Num() == 256)
		{
			TArray<FColor> Pixels;
			for (int32 Py = 0; Py < BufferHeight * 4; ++Py)
			for (int32 Px = 0; Px < BufferWidth * 4; ++Px) Pixels.Add(Palette[Raster.GetPixel(Px / 4, Py / 4)]);
			FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir() / FString::Printf(TEXT("../Docs/scratchpad/drive-in-radio/minimap-zoom%d.png"), Zoom)),
				FImageView(Pixels.GetData(), BufferWidth * 4, BufferHeight * 4));
		}
	}
	Frame.DriveInTiles = {FIntPoint(0, 0)}; Settings.Zoom = 3;
	Frame.City = nullptr;
	Raster.Render(Frame, Settings);
	TestEqual(TEXT("Off-map theaters do not pin a logo to the edge"), Raster.GetPixel(ViewOriginX, ViewOriginY), uint8(0));
	return true;
}
#endif

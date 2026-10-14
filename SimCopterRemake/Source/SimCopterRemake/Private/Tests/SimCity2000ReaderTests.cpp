// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Misc/AutomationTest.h"

namespace
{
void AppendUInt32BE(TArray<uint8>& Bytes, const uint32 Value)
{
	Bytes.Add(static_cast<uint8>(Value >> 24));
	Bytes.Add(static_cast<uint8>(Value >> 16));
	Bytes.Add(static_cast<uint8>(Value >> 8));
	Bytes.Add(static_cast<uint8>(Value));
}

void AppendCityChunk(TArray<uint8>& Bytes, const ANSICHAR* Id, const TArray<uint8>& Data, const bool bUncompressed)
{
	TArray<uint8> Stored;
	if (bUncompressed)
	{
		Stored = Data;
	}
	else
	{
		// Literal-only SC2 RLE keeps the fixture independent of the reader's decoder.
		for (int32 Offset = 0; Offset < Data.Num(); Offset += 127)
		{
			const int32 Count = FMath::Min(127, Data.Num() - Offset);
			Stored.Add(static_cast<uint8>(Count));
			Stored.Append(Data.GetData() + Offset, Count);
		}
	}
	Bytes.Append(reinterpret_cast<const uint8*>(Id), 4);
	AppendUInt32BE(Bytes, static_cast<uint32>(Stored.Num()));
	Bytes.Append(Stored);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCity2000RleTest,
	"SimCopter.Formats.SimCity2000.Rle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCity2000RleTest::RunTest(const FString& Parameters)
{
	const uint8 Bytes[] = { 0x03, 0x01, 0x02, 0x03, 0x82, 0xAA };
	TArray<uint8> Compressed;
	Compressed.Append(Bytes, UE_ARRAY_COUNT(Bytes));

	TArray<uint8> Decoded;
	FString Error;
	if (!TestTrue(TEXT("RLE decode succeeds"), FSimCity2000Reader::DecodeRleChunk(Compressed, 6, Decoded, Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("Decoded byte count"), Decoded.Num(), 6);
	TestEqual(TEXT("Literal byte 0"), Decoded[0], static_cast<uint8>(0x01));
	TestEqual(TEXT("Literal byte 2"), Decoded[2], static_cast<uint8>(0x03));
	TestEqual(TEXT("Repeated byte 3"), Decoded[3], static_cast<uint8>(0xAA));
	TestEqual(TEXT("Repeated byte 5"), Decoded[5], static_cast<uint8>(0xAA));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCity2000AltitudeDecodingTest,
	"SimCopter.Formats.SimCity2000.AltitudeDecoding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCity2000AltitudeDecodingTest::RunTest(const FString& Parameters)
{
	// FUN_004abc20: ALTM has three five-bit fields; XTER > 0x0f determines water.
	// These exact samples used to be sought in whichever commercial city was installed.
	// Build them into a complete in-memory SC2 so the checks always exercise the parser.
	struct FSample
	{
		int32 Index;
		uint16 RawAltitude;
		uint8 Terrain;
		uint8 Altitude;
		uint8 SecondaryAltitude;
		uint8 Slope;
		bool bWater;
	};
	const FSample Samples[] = {
		{ 0, 0x0004, 0x00, 4, 0, 0, false },
		{ 1, 0x0082, 0x10, 2, 4, 0, true },
		{ 2, 0x1408, 0x05, 8, 0, 5, false },
		// Same ALTM on dry land, and water without ALTM bit 7: water is an XTER flag.
		{ 127, 0x0082, 0x0f, 2, 4, 0, false },
		{ 128, 0x0004, 0x10, 4, 0, 0, true },
		// Field maxima and the unused high bit, including the last tile in the map.
		{ 255, 0x83e0, 0x0f, 0, 31, 0, false },
		{ 256, 0x7c1f, 0x0f, 31, 0, 31, false },
		{ FSimCity2000City::TileCount - 1, 0xffff, 0x10, 31, 31, 31, true },
	};

	TArray<uint8> Altitudes;
	Altitudes.SetNumZeroed(FSimCity2000City::TileCount * 2);
	TArray<uint8> Terrain;
	Terrain.SetNumZeroed(FSimCity2000City::TileCount);
	for (const FSample& Sample : Samples)
	{
		Altitudes[Sample.Index * 2] = static_cast<uint8>(Sample.RawAltitude >> 8);
		Altitudes[Sample.Index * 2 + 1] = static_cast<uint8>(Sample.RawAltitude);
		Terrain[Sample.Index] = Sample.Terrain;
	}

	TArray<uint8> Body;
	const uint8 Type[] = { 'S', 'C', 'D', 'H' };
	Body.Append(Type, UE_ARRAY_COUNT(Type));
	AppendCityChunk(Body, "ALTM", Altitudes, true);
	AppendCityChunk(Body, "XTER", Terrain, false);
	TArray<uint8> EmptyMap;
	EmptyMap.SetNumZeroed(FSimCity2000City::TileCount);
	for (const ANSICHAR* Id : { "XBLD", "XZON", "XUND", "XTXT", "XBIT" })
	{
		AppendCityChunk(Body, Id, EmptyMap, false);
	}
	TArray<uint8> Bytes;
	const uint8 Form[] = { 'F', 'O', 'R', 'M' };
	Bytes.Append(Form, UE_ARRAY_COUNT(Form));
	AppendUInt32BE(Bytes, static_cast<uint32>(Body.Num()));
	Bytes.Append(Body);

	FSimCity2000City City;
	FString Error;
	if (!TestTrue(TEXT("Loads controlled altitude fixture"),
		FSimCity2000Reader::LoadCityFromBytes(Bytes, TEXT("AltitudeSamples.sc2"), City, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("Fixture tile count"), City.Tiles.Num(), FSimCity2000City::TileCount))
	{
		return false;
	}
	TestEqual(TEXT("All seven map chunks load"), City.Chunks.Num(), 7);
	for (const FSample& Sample : Samples)
	{
		const FSimCity2000Tile& Tile = City.Tiles[Sample.Index];
		const FString Label = FString::Printf(TEXT("Tile %d ALTM %04x XTER %02x"),
			Sample.Index, Sample.RawAltitude, Sample.Terrain);
		TestEqual(Label + TEXT(" raw big-endian altitude"), Tile.RawAltitude, Sample.RawAltitude);
		TestEqual(Label + TEXT(" base altitude"), Tile.Altitude, Sample.Altitude);
		TestEqual(Label + TEXT(" secondary altitude"), Tile.SecondaryAltitude, Sample.SecondaryAltitude);
		TestEqual(Label + TEXT(" slope"), Tile.Slope, Sample.Slope);
		TestEqual(Label + TEXT(" terrain"), Tile.Terrain, Sample.Terrain);
		TestEqual(Label + TEXT(" water flag"), Tile.bWater, Sample.bWater);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimCity2000ReferenceCityTest,
	"SimCopter.Formats.SimCity2000.ReferenceCity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCity2000ReferenceCityTest::RunTest(const FString& Parameters)
{
	FString DemoCityPath = SimCopterOriginalGame::ResolveFile(TEXT("cities/career/city0.sc2"));
	if (DemoCityPath.IsEmpty())
	{
		DemoCityPath = SimCopterOriginalGame::ResolveFile(TEXT("cities/Demo.sc2"));
	}
	if (DemoCityPath.IsEmpty())
	{
		AddWarning(TEXT("Skipping optional reference city test: neither cities/career/city0.sc2 nor cities/Demo.sc2 is installed. AltitudeDecoding covers the parser without these files."));
		return true;
	}

	FSimCity2000City City;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("Loads '%s'"), *DemoCityPath), FSimCity2000Reader::LoadCityFromFile(DemoCityPath, City, Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("Tile count"), City.Tiles.Num(), FSimCity2000City::TileCount);
	TestEqual(TEXT("Chunk count"), City.Chunks.Num(), 21);
	TestTrue(TEXT("City name is populated"), !City.CityName.IsEmpty());
	TestTrue(TEXT("ALTM chunk exists"), City.HasChunk(TEXT("ALTM")));
	TestTrue(TEXT("XBLD chunk exists"), City.HasChunk(TEXT("XBLD")));
	// Exact altitude samples belong to AltitudeDecoding: career and demo maps differ.

	return true;
}

#endif

#pragma once

#include "CoreMinimal.h"

struct FSimCity2000City;

// Remake addition: reuse CO182 on a level lot while preserving neighboring foundations.
namespace SimCopterDriveInPlacement
{
	constexpr uint8 BuildingId = 182;
	constexpr int32 Span = 3;
	SIMCOPTERREMAKE_API FIntPoint FindSite(const FSimCity2000City& City, const TArray<int16>& Corners);
	SIMCOPTERREMAKE_API void Stamp(FSimCity2000City& City, FIntPoint Origin, TArray<int16>* Corners = nullptr);
}

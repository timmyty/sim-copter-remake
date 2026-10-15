#pragma once

#include "CoreMinimal.h"
#include "Missions/SimCopterMissionSystem.h"

// Requested remake rules; these are not retail difficulty/placement rules.
namespace SimCopterCustomCityProgression
{
inline int32 TierForScore(int32 BestScore)
{
	return BestScore >= 3000 ? 4 : BestScore >= 1500 ? 3 : BestScore >= 500 ? 2 : 1;
}

inline int32 PassengerLimit(int32 Seats, bool bMedical)
{
	return FMath::Max(0, Seats - (bMedical ? 1 : 0));
}

struct FCapabilities
{
	int32 Tier = 1;
	int32 Seats = 2;
	bool bAdvancedAircraft = false;
	bool bHarness = false;
	bool bFireEquipment = false;
	bool bTearGas = false;
};

inline bool Allows(int32 Type, const FCapabilities& C)
{
	using namespace SimCopterMissions;
	switch (Type)
	{
	case TYPE_Riot: return C.Tier >= 4 && C.bAdvancedAircraft && C.bTearGas;
	case TYPE_Burglar: case TYPE_Arsonist: return C.Tier >= 3;
	case TYPE_Robber: case TYPE_Mugger: return C.Tier >= 2;
	case TYPE_PlaneCrash: case TYPE_TrainCrash: return C.Tier >= 3 && C.bFireEquipment && C.bHarness && C.Seats > 0;
	// Fires are available from the first stage whenever the aircraft carries water equipment,
	// so a new user city is not all Transport work (user request, v1.0.19).
	case TYPE_BuildingFire: case TYPE_CarFireEvent: return C.bFireEquipment;
	case TYPE_TrainRescue: return C.Tier >= 3 && C.bHarness && C.Seats > 0;
	case TYPE_BoatRescue: return C.Tier >= 2 && C.bHarness && C.Seats > 0;
	case TYPE_RooftopRescue: return C.bHarness && C.Seats > 0;
	case TYPE_Medevac: return PassengerLimit(C.Seats, true) > 0;
	case TYPE_Transport: return C.Seats > 0;
	default: return true;
	}
}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SimCopterHangarShop.h"

#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "City/SimCopterHangar.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Kismet/GameplayStatics.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Missions/SimCopterMissionSystem.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "UI/SimCopterHangarArt.h"

namespace
{
using namespace SimCopterMissions;

// FUN_0042d840's equipment permutation: upgrades page row -> equipment bit index.
constexpr int32 UpgradeRowToEquipmentIndex[SimCopterHangarShop::UpgradeRowCount] = { 0, 1, 3, 4, 2, 5, 6 };

// The tool each equipment bit index stands for (registry order: bucket, megaphone, harness,
// tear gas, cannon - the same order FSimCopterEquipmentDefinition::EquipmentIndex numbers).
constexpr ESimCopterHelicopterTool EquipmentIndexToTool[7] = {
	ESimCopterHelicopterTool::WaterBucket,
	ESimCopterHelicopterTool::Megaphone,
	ESimCopterHelicopterTool::RescueHarness,
	ESimCopterHelicopterTool::TearGas,
	ESimCopterHelicopterTool::WaterCannon,
	ESimCopterHelicopterTool::TowClamp,
	ESimCopterHelicopterTool::CaptureCage,
};

// Strings 490..494, in upgrades page row order.
const TCHAR* const UpgradeDescriptions[SimCopterHangarShop::UpgradeRowCount] = {
	TEXT("Bambi Bucket - No one seems to know why it's called this. Used for dropping water on fires. Refilled by dropping it into any convenient open water source."),
	TEXT("Megaphone -Allows you to communicate with people and cars below. Useful in emergency, traffic and law enforcement missions. Effective range: 300 meters."),
	TEXT("Tear Gas Launcher - For law enforcement use only. Useful for when your citizens get out of line. Holds ten canisters in delightful non-toxic and biodegradable forms."),
	TEXT("Water Cannon - Basically a giant squirt gun. Eliminates need to fly directly over fires. You'll need the bucket for refills. The high pressure causes significant recoil."),
	TEXT("Rescue Harness - Used for picking up people in emergency situations. When you need to rescue people and you can't land then the rescue harness is the right tool for the job."),
	TEXT("Tow Clamp - Lower onto a stalled car or capsized boat. Carry cars to Auto Repair or Junkyard, boats to a Harbor repair platform. Use again to grab; G releases. Page Up/Down reels the cable."),
	TEXT("Capture Cage - Lower the open cage over up to four people, then close it. Hold Page Up at the top for a moment to board available seats. Criminals are cuffed. G opens the floor and releases occupants."),
};

// Strings 410..414: the inventory's five tick columns, left to right.
const TCHAR* const InventoryColumnNames[SimCopterHangarShop::InventoryColumnCount] = {
	TEXT("Rescue Harness"),
	TEXT("Bambi Bucket"),
	TEXT("Water Cannon"),
	TEXT("Megaphone"),
	TEXT("Teargas"),
};

constexpr ESimCopterHelicopterTool InventoryColumnTools[SimCopterHangarShop::InventoryColumnCount] = {
	ESimCopterHelicopterTool::RescueHarness,
	ESimCopterHelicopterTool::WaterBucket,
	ESimCopterHelicopterTool::WaterCannon,
	ESimCopterHelicopterTool::Megaphone,
	ESimCopterHelicopterTool::TearGas,
};

// Strings 400..408, indexed by runtime type.
const TCHAR* const ModelNames[9] = {
	TEXT("Jet Ranger"),
	TEXT("MD 500"),
	TEXT("Apache"),
	TEXT("Bell 212"),
	TEXT("Schweizer 300"),
	TEXT("Agusta"),
	TEXT("Dauphin"),
	TEXT("MD Explorer"),
	TEXT("MD 520"),
};

// Expanded catalog biographies; source notes: Docs/NpcMedicalAndHelicopterUpdate.md.
// Original strings 460..467 establish catalog row order.
const TCHAR* const CatalogHistory[SimCopterHangarLayout::CatalogTabCount] = {
	TEXT("The Schweizer 300 began as the Hughes 269, which first flew in 1956. The three-seat Hughes 300 followed in 1964, and the more powerful 300C was certified in 1970. Schweizer acquired the helicopter line in 1986, continuing a design closely associated with pilot training and light utility work."),
	TEXT("The Bell 206 JetRanger became a familiar light turbine helicopter in civilian service. Its family has served flight schools, police units and commercial operators for decades. In 1983, Dick Smith completed the first solo helicopter flight around the world in a JetRanger III, demonstrating the design's versatility beyond local flights."),
	TEXT("The MD 500 belongs to the Hughes light-helicopter family and retains its compact cabin and conventional tail rotor. The MD 500E continued that lineage under McDonnell Douglas. Together with related models, it formed the light-helicopter product line that passed from Boeing to MD Helicopters in 1999."),
	TEXT("The MD 520N developed the compact MD light-helicopter layout around NOTAR technology. Instead of a conventional exposed tail rotor, the system uses controlled airflow through the tail boom and a directional jet. It is a single-engine, five-place member of the same family as the MD 500."),
	TEXT("The Bell 212 developed the Bell 204/205 utility-helicopter layout into a twin-engine aircraft. It belongs to the Huey family, whose broad cabin and adaptable airframe supported military and civilian work. Its utility heritage is reflected in this game's large passenger capacity and emphasis on carrying people and equipment."),
	TEXT("Agusta's A109 first flew on 4 August 1971. Designed in Italy, it combined twin engines, a streamlined fuselage and retractable wheeled landing gear. The family developed into a platform for passenger transport, public services and medical work. This catalog depicts the earlier A109 generation represented in SimCopter."),
	TEXT("The twin-engine Dauphin first flew in January 1975, developing the earlier Dauphin design into a larger aircraft family. Its enclosed Fenestron tail rotor became a distinctive feature. The combination of a roomy cabin and streamlined body gave the family a role in passenger transport, rescue and public-service operations."),
	TEXT("The MD Explorer extended NOTAR technology into a twin-engine helicopter. It was part of the McDonnell Douglas light-helicopter family and the product line transferred from Boeing to MD Helicopters in 1999. Its eight-place layout, including the pilot, offered a larger cabin than the small MD single-engine models."),
};

// Strings 470..477.
const TCHAR* const CatalogSpecialties[SimCopterHangarLayout::CatalogTabCount] = {
	TEXT("Training, observation and light utility work. The small piston helicopter is a useful starting point for learning precise hovering and landing. In the city, use it for short trips and small pickups; its two passenger seats make it less suitable for incidents with many victims. Plan extra hospital trips when necessary."),
	TEXT("Local transport, patrol, observation and news work. Four passenger seats give it a practical balance between small pickups and everyday city jobs. In SimCopter, it is a flexible light aircraft for transport, medical evacuations and rescue work when fitted with the appropriate equipment."),
	TEXT("Light utility, patrol and observation. Its compact shape suits careful approaches to constrained pickup areas, while four passenger seats support small rescue parties. In SimCopter, compare its speed and load limits with the JetRanger before buying; a compact body still requires room for the main rotor."),
	TEXT("Light utility, patrol and passenger transport. NOTAR removes the exposed tail rotor, a distinctive feature for work around people and obstacles. In SimCopter it carries four passengers, so it is suited to small medical and rescue loads. Keep clearance around the entire aircraft during pickups."),
	TEXT("Utility transport, search and rescue, and moving larger groups. Fourteen passenger seats make this one of the most useful choices for crowded rescue sites or several patients at once. Its larger airframe needs a generous landing area; use a rescue harness when a safe landing is impractical."),
	TEXT("Fast passenger transport, corporate flying and medical work. Seven passenger seats provide useful capacity without the size of the largest utility helicopters. In SimCopter its catalog speed makes it attractive for long cross-city flights and urgent hospital trips. Leave enough room to settle accurately onto roof pads."),
	TEXT("Passenger transport, public-service work and rescue. Thirteen passenger seats and a high catalog speed suit large pickups followed by longer flights. The enclosed tail rotor is a recognizable feature, but the main rotor still needs clearance. Choose a broad landing area when collecting a group."),
	TEXT("Executive transport, utility and medical evacuation. The twin-engine layout and NOTAR tail distinguish it from the smaller MD models. In SimCopter it offers seven passenger seats and a high catalog speed, making it useful for mixed rescue and transport duties without stepping up to the largest cabins."),
};

// Strings 480..487.
const TCHAR* const CatalogDescriptions[SimCopterHangarLayout::CatalogTabCount] = {
	TEXT("A compact piston helicopter with a rounded cabin, exposed structure and skid landing gear. Its modest speed rewards planning short routes between nearby incidents. The small cabin fills quickly, so check the number of people at a pickup before committing to one trip.\n\nCatalog specifications\nEngine: Single Textron Piston\nEmpty Weight: 500 kg/1100 lbs.\nCapacity: 430 kg/950 lbs.\nSeating: 2 passengers\nSpeed: 150 km/h/80 kt"),
	TEXT("A light turbine helicopter with a conventional main-and-tail-rotor arrangement and skid landing gear. It offers a substantial speed and seating increase over the Schweizer. Four passenger places make it a useful everyday aircraft, though large evacuation jobs will still require repeat visits.\n\nCatalog specifications\nEngine: Single Allison Turboshaft\nEmpty Weight: 750 kg/1650 lbs.\nCapacity: 700kg/1550 lbs.\nSeating: 4 passengers\nSpeed: 220 km/h/120 kt"),
	TEXT("A small turbine helicopter with an egg-shaped cabin, slim tail boom and skid landing gear. The catalog pairs a light empty weight with four passenger seats. Use its compact dimensions for careful positioning, and allow room to hover while passengers or rescuers approach.\n\nCatalog specifications\nEngine: Single Allison Turboshaft\nEmpty Weight: 670 kg/1500 lbs.\nCapacity: 690 kg/1500 lbs.\nSeating: 4 passengers\nSpeed: 230 km/h/125 kt"),
	TEXT("A light turbine helicopter recognizable by its NOTAR tail boom. It keeps the small-cabin role of the MD 500 while offering different performance figures. Four passenger seats fit small teams; fit the tools required by the mission and account for their weight alongside fuel and passengers.\n\nCatalog specifications\nEngine: Single Allison Turboshaft\nEmpty Weight: 720 kg/1600 lbs.\nCapacity: 800 kg/1750 lbs.\nSeating: 4 passengers\nSpeed: 240 km/h/130 kt"),
	TEXT("A broad-cabin utility helicopter with twin turbines, skid landing gear and much more passenger room than the light models. It trades catalog speed for carrying capacity. A good choice when reducing the number of trips matters more than reaching the next site as quickly as possible.\n\nCatalog specifications\nEngine: Twin Turbine\nEmpty Weight: 2800 kg/6300 lbs.\nCapacity: 2200 kg/4900 lbs.\nSeating: 14 passengers\nSpeed: 185 km/h/100kt"),
	TEXT("A streamlined twin-turbine helicopter with retractable wheels and a long glazed cabin. The Agusta balances seven passenger seats with one of the fastest speeds in the catalog. Its clear windows reveal the pilot and occupants, including medical passengers awaiting hospital handoff.\n\nCatalog specifications\nEngine: Twin Turbine\nEmpty Weight: 1600 kg/3000 lbs.\nCapacity: 1100 kg/2500 lbs.\nSeating: 7 passengers\nSpeed: 280 km/h/150 kt"),
	// The accented e is escaped so this table survives a non-UTF-8 read of the file.
	TEXT("A streamlined twin-turbine helicopter with a large cabin and enclosed tail rotor. Its thirteen passenger seats make it suited to moving a substantial group while retaining a high catalog speed. Larger pickups still need a stable landing or a deliberate sequence of harness transfers.\n\nCatalog specifications\nEngine: Two Turbom\u00E9ca Turboshafts\nEmpty Weight: 2300 kg/5000 lbs.\nCapacity: 2000 kg/4350 lbs.\nSeating: 13 passengers\nSpeed: 280 km/h/150 kt"),
	TEXT("A twin-turbine helicopter with a roomy cabin and NOTAR tail. Its seven passenger seats sit between the four-seat light aircraft and the large utility machines. Use it when the mission needs both useful cabin capacity and fast travel across the city.\n\nCatalog specifications\nEngine: Two PW206A Turboshafts\nEmpty Weight: 1500 kg/3300 lbs.\nCapacity: 1200 kg/2600 lbs.\nSeating: 7 passengers\nSpeed: 275 km/h/150 kt"),
};

// Strings 570..587, paired with the type bit each one names.
struct FMissionTypeName
{
	int32 TypeMask;
	const TCHAR* Name;
};

const FMissionTypeName MissionTypeNames[] = {
	{ TYPE_VehicleTow, TEXT("Stalled vehicle") },
	{ TYPE_BoatTow, TEXT("Boat Recovery") },
	{ TYPE_Riot,          TEXT("Riot") },            // 571
	{ TYPE_RooftopRescue, TEXT("Rooftop Rescue") },  // 572
	{ TYPE_BoatRescue,    TEXT("Boat Rescue") },     // 573
	{ TYPE_TrainRescue,   TEXT("Train Rescue") },    // 574
	{ TYPE_Medevac,       TEXT("Medevac") },         // 575
	{ TYPE_Transport,     TEXT("Transport") },       // 576
	{ TYPE_BuildingFire,  TEXT("Fire") },            // 577
	{ TYPE_PlaneCrash,    TEXT("Plane Crash") },     // 578
	{ TYPE_TrainCrash,    TEXT("Train Crash") },     // 579
	{ TYPE_Burglar,       TEXT("Burglar") },         // 580
	{ TYPE_Arsonist,      TEXT("Arsonist") },        // 581
	{ TYPE_Mugger,        TEXT("Mugger") },          // 582
	{ TYPE_Robber,        TEXT("Robber") },          // 583
	{ TYPE_CarFire,       TEXT("Burning Car") },     // 584
	{ TYPE_TrafficJam,    TEXT("Traffic Jam") },     // 585
	{ TYPE_BaseLocation,  TEXT("Base Location") },   // 586 (0x24a; 587 "Non-Mission Event" is 0x24b, no bit)
};

USimCopterCareerSubsystem* GetCareer(const SimCopterHangarShop::FContext& Context)
{
	return Context.Career.Get();
}
}

namespace SimCopterHangarShop
{
int32 GetEquipmentIndexForUpgradeRow(const int32 UpgradeRow)
{
	return (UpgradeRow >= 0 && UpgradeRow < UpgradeRowCount) ? UpgradeRowToEquipmentIndex[UpgradeRow] : INDEX_NONE;
}

ESimCopterHelicopterTool GetToolForUpgradeRow(const int32 UpgradeRow)
{
	const int32 EquipmentIndex = GetEquipmentIndexForUpgradeRow(UpgradeRow);
	return EquipmentIndex == INDEX_NONE ? ESimCopterHelicopterTool::Count : EquipmentIndexToTool[EquipmentIndex];
}

const TCHAR* GetUpgradeDescription(const int32 UpgradeRow)
{
	return (UpgradeRow >= 0 && UpgradeRow < UpgradeRowCount) ? UpgradeDescriptions[UpgradeRow] : TEXT("");
}

ESimCopterHelicopterTool GetToolForInventoryColumn(const int32 ColumnIndex)
{
	return (ColumnIndex >= 0 && ColumnIndex < InventoryColumnCount)
		? InventoryColumnTools[ColumnIndex]
		: ESimCopterHelicopterTool::Count;
}

const TCHAR* GetInventoryColumnName(const int32 ColumnIndex)
{
	return (ColumnIndex >= 0 && ColumnIndex < InventoryColumnCount) ? InventoryColumnNames[ColumnIndex] : TEXT("");
}

const TCHAR* GetModelDisplayName(const int32 TypeIndex)
{
	return (TypeIndex >= 0 && TypeIndex < UE_ARRAY_COUNT(ModelNames)) ? ModelNames[TypeIndex] : TEXT("");
}

const TCHAR* GetCatalogHistory(const int32 CatalogRow)
{
	if (CatalogRow == SimCopterHangarLayout::ApacheCatalogRow) return TEXT("A hidden military helicopter awaits in this city. The Apache can be found at the original secret site, or purchased here for delivery to a clear hangar pad.");
	return (CatalogRow >= 0 && CatalogRow < SimCopterHangarLayout::CatalogTabCount) ? CatalogHistory[CatalogRow] : TEXT("");
}

const TCHAR* GetCatalogSpecialties(const int32 CatalogRow)
{
	if (CatalogRow == SimCopterHangarLayout::ApacheCatalogRow) return TEXT("Armed flight with missiles and a continuous-fire machine gun. Select a weapon in Tools and use the primary tool action to fire. The machine gun fires while held.");
	return (CatalogRow >= 0 && CatalogRow < SimCopterHangarLayout::CatalogTabCount) ? CatalogSpecialties[CatalogRow] : TEXT("");
}

const TCHAR* GetCatalogDescription(const int32 CatalogRow)
{
	if (CatalogRow == SimCopterHangarLayout::ApacheCatalogRow) return TEXT("The Apache is flyable with the same flight controls as the civilian fleet. Its missile launcher and machine gun are included; no equipment purchase is needed. It has no passenger seats.\n\nPurchase price: three times the most expensive civilian helicopter. Find and board the hidden aircraft for the original discovery route, or buy hangar delivery.");
	return (CatalogRow >= 0 && CatalogRow < SimCopterHangarLayout::CatalogTabCount) ? CatalogDescriptions[CatalogRow] : TEXT("");
}

const TCHAR* GetInventoryHeaderCentre() { return TEXT("MOOSE\nAVIONICS"); }
const TCHAR* GetInventoryHeaderLeft() { return TEXT("MAX AERO\n3547 N. CAROLINA AVE.\nSIMCITY, SIMWORLD\n(707) 492-3154"); }
const TCHAR* GetInventoryHeaderRight() { return TEXT("REECE INC.\n777 HILARY DR.\nBUBBER, IN SIMUSA"); }
const TCHAR* GetUpgradeHeaderCentre() { return TEXT("MATTIE T./MIKE B. ELECTRONIX, INC."); }
const TCHAR* GetUpgradeHeaderLeft() { return TEXT("MOOSE\nAVIONICS\n1220 JASE DR.\n997 TALI, SIMUSA 47078"); }
const TCHAR* GetUpgradeHeaderRight() { return TEXT("MYKA/MONIQUE-  QUESTAR VID. CORP.\nROALAINE VIA JOSE K.\nROIANNE QUO ORIANNA T."); }

const TCHAR* GetMissionTypeLogName(const int32 TypeMask)
{
	// Compound masks (rescue 0x90 / 0x110 / 0x80010) have to win over the single bits they
	// contain, so the table is walked most-specific first.
	const FMissionTypeName* Best = nullptr;
	for (const FMissionTypeName& Entry : MissionTypeNames)
	{
		if ((TypeMask & Entry.TypeMask) != Entry.TypeMask)
		{
			continue;
		}
		if (Best == nullptr || FMath::CountBits(static_cast<uint32>(Entry.TypeMask)) > FMath::CountBits(static_cast<uint32>(Best->TypeMask)))
		{
			Best = &Entry;
		}
	}

	// String 570.
	return Best != nullptr ? Best->Name : TEXT("Unknown");
}

bool FContext::IsUsable() const
{
	return Career.IsValid();
}

int32 GetCurrentFunds(const FContext& Context)
{
	const ASimCopterMissionSystemActor* Missions = Context.Missions.Get();
	return Missions != nullptr ? Missions->GetSessionCash() : 0;
}

int32 GetCheapestHelicopterPrice(const FContext& Context)
{
	const USimCopterCareerSubsystem* Career = GetCareer(Context);
	if (Career == nullptr)
	{
		return 0;
	}

	int32 Cheapest = 0;
	for (int32 Row = 0; Row < SimCopterHangarLayout::CatalogTabCount; ++Row)
	{
		const int32 Price = Career->GetHelicopterPrice(SimCopterHangarLayout::GetTypeIndexForCatalogRow(Row));
		if (Price > 0 && (Cheapest == 0 || Price < Cheapest))
		{
			Cheapest = Price;
		}
	}
	return Cheapest;
}

FRowState GetHelicopterRowState(const FContext& Context, const int32 CatalogRow)
{
	FRowState State;

	USimCopterCareerSubsystem* Career = GetCareer(Context);
	const int32 TypeIndex = SimCopterHangarLayout::GetTypeIndexForCatalogRow(CatalogRow);
	State.bMystery = TypeIndex == 2;
	if (Career == nullptr || TypeIndex == INDEX_NONE)
	{
		State.Reason = TEXT("This model is not for sale.");
		return State;
	}

	State.bOwned = Career->OwnsHelicopter(TypeIndex);
	State.bMystery = false;
	if (TypeIndex == 2 && !State.bOwned)
	{
		FVector Surface;
		const auto* Traffic = Context.Missions.IsValid() ? Cast<ASimCopterTrafficSystemActor>(
			UGameplayStatics::GetActorOfClass(Context.Missions.Get(), ASimCopterTrafficSystemActor::StaticClass())) : nullptr;
		State.bMystery = !Career->HasSpawnedApacheEncounter() &&
			!SimCopterHelicopterParking::TryGetApacheSpawnSurface(Traffic, Surface);
		if (State.bMystery)
		{
			State.Reason = TEXT("This mystery helicopter is revealed only in its special city.");
			return State; // Gate the transaction too, not just the card's presentation.
		}
	}
	State.ItemValue = State.bOwned ? Career->GetHelicopterTradeInValue(TypeIndex) : Career->GetHelicopterPrice(TypeIndex);

	if (State.bOwned)
	{
		// Any airframe on the books can be sold, the starting Schweizer included - trading up is
		// the point of the page. The one guard: a sale that empties the books has to leave the
		// player enough to buy the cheapest chopper back, or there is nothing left to fly.
		if (Career->GetOwnedHelicopterCount() > 1)
		{
			State.bCanSell = true;
		}
		else
		{
			const int32 Cheapest = GetCheapestHelicopterPrice(Context);
			State.bCanSell = Cheapest > 0 && GetCurrentFunds(Context) + State.ItemValue >= Cheapest;
			if (!State.bCanSell)
			{
				State.Reason = TEXT("Selling your last helicopter must leave enough to buy another.");
			}
		}
	}
	else
	{
		State.bCanBuy = State.ItemValue > 0 && GetCurrentFunds(Context) >= State.ItemValue;
		if (!State.bCanBuy)
		{
			State.Reason = State.ItemValue > 0 ? TEXT("Not enough funds.") : TEXT("No price for this model.");
		}
	}

	return State;
}

FRowState GetUpgradeRowState(const FContext& Context, const int32 UpgradeRow)
{
	FRowState State;

	const ASimCopterHelicopterPawn* Helicopter = Context.Helicopter.Get();
	const ESimCopterHelicopterTool Tool = GetToolForUpgradeRow(UpgradeRow);
	if (Helicopter == nullptr || Tool == ESimCopterHelicopterTool::Count)
	{
		State.Reason = TEXT("No helicopter to fit this to.");
		return State;
	}

	const int32 Bit = SimCopterHelicopterRegistry::GetToolCareerBit(Tool);
	State.bOwned = Helicopter->GetEquipmentState().HasCareerBit(Bit);
	State.ItemValue = State.bOwned
		? SimCopterHelicopterRegistry::GetEquipmentSellValue(Tool)
		: SimCopterHelicopterRegistry::GetEquipmentPrice(Tool);

	if (State.bOwned)
	{
		State.bCanSell = !((Tool==ESimCopterHelicopterTool::TowClamp || Tool==ESimCopterHelicopterTool::CaptureCage) && Helicopter->GetAirOperations()->IsDeployed());
		if(!State.bCanSell) State.Reason=TEXT("Stow the sling before selling this equipment.");
	}
	else
	{
		State.bCanBuy = GetCurrentFunds(Context) >= State.ItemValue;
		if (!State.bCanBuy)
		{
			State.Reason = TEXT("Not enough funds.");
		}
	}

	return State;
}

bool BuyHelicopter(const FContext& Context, const int32 CatalogRow, FString& OutMessage)
{
	USimCopterCareerSubsystem* Career = GetCareer(Context);
	ASimCopterMissionSystemActor* Missions = Context.Missions.Get();
	ASimCopterHelicopterPawn* Helicopter = Context.Helicopter.Get();
	const int32 TypeIndex = SimCopterHangarLayout::GetTypeIndexForCatalogRow(CatalogRow);

	const FRowState State = GetHelicopterRowState(Context, CatalogRow);
	if (Career == nullptr || Missions == nullptr || TypeIndex == INDEX_NONE || !State.bCanBuy)
	{
		OutMessage = State.Reason.IsEmpty() ? TEXT("That purchase is not available.") : State.Reason;
		return false;
	}

	ASimCopterHelicopterPawn* Delivered = SimCopterHelicopterParking::SpawnOnFreePad(Context.Hangar.Get(), Helicopter, TypeIndex, OutMessage);
	if (Delivered == nullptr)
	{
		return false;
	}
	// Paid delivery replaces the unclaimed encounter only after a clear pad succeeds.
	if (TypeIndex == 2)
	{
		TArray<AActor*> Aircraft;
		UGameplayStatics::GetAllActorsOfClass(Missions, ASimCopterHelicopterPawn::StaticClass(), Aircraft);
		for (AActor* Actor : Aircraft)
			if (Actor != Delivered && CastChecked<ASimCopterHelicopterPawn>(Actor)->IsApacheHelicopter() &&
				!CastChecked<ASimCopterHelicopterPawn>(Actor)->IsSupportAircraft() && !CastChecked<ASimCopterHelicopterPawn>(Actor)->GetController()) Actor->Destroy();
		Career->SetApacheEncounterSpawned(true);
	}
	Missions->AddSessionCash(-State.ItemValue);
	Career->SetHelicopterOwned(TypeIndex, true);

	OutMessage = FString::Printf(TEXT("Bought %s for %d Bucks."), GetModelDisplayName(TypeIndex), State.ItemValue);
	return true;
}

bool SellHelicopter(const FContext& Context, const int32 CatalogRow, FString& OutMessage)
{
	USimCopterCareerSubsystem* Career = GetCareer(Context);
	ASimCopterMissionSystemActor* Missions = Context.Missions.Get();
	const int32 TypeIndex = SimCopterHangarLayout::GetTypeIndexForCatalogRow(CatalogRow);

	const FRowState State = GetHelicopterRowState(Context, CatalogRow);
	if (Career == nullptr || Missions == nullptr || TypeIndex == INDEX_NONE || !State.bCanSell)
	{
		OutMessage = State.Reason.IsEmpty() ? TEXT("That sale is not available.") : State.Reason;
		return false;
	}

	TArray<AActor*> Aircraft;
	UGameplayStatics::GetAllActorsOfClass(Missions, ASimCopterHelicopterPawn::StaticClass(), Aircraft);
	for (AActor* Actor : Aircraft)
	{
		ASimCopterHelicopterPawn* Sold = CastChecked<ASimCopterHelicopterPawn>(Actor);
		if (Sold->IsSupportAircraft() || Sold->GetHelicopterTypeIndex() != TypeIndex) continue;
		if (Sold->GetController() != nullptr || Sold->GetPassengerCount() > 0 || Sold->HasHarnessRider() || Sold->GetAirOperations()->HasCargo())
		{
			OutMessage = TEXT("Empty and park the helicopter before selling it.");
			return false;
		}
	}
	for (AActor* Actor : Aircraft)
	{
		if (!CastChecked<ASimCopterHelicopterPawn>(Actor)->IsSupportAircraft() && CastChecked<ASimCopterHelicopterPawn>(Actor)->GetHelicopterTypeIndex() == TypeIndex)
		{
			Actor->Destroy();
		}
	}

	Missions->AddSessionCash(State.ItemValue);
	Career->SetHelicopterOwned(TypeIndex, false);

	OutMessage = FString::Printf(TEXT("Sold %s for %d Bucks."), GetModelDisplayName(TypeIndex), State.ItemValue);
	return true;
}

bool DeliverCheatHelicopter(const FContext& Context, int32 TypeIndex, FString& OutMessage)
{
	auto* Career = GetCareer(Context);
	if (!Career || !Career->Cheats.bCEO || TypeIndex < 0 || TypeIndex > 8) return false;
	if (Career->OwnsHelicopter(TypeIndex))
	{
		OutMessage = TEXT("That helicopter is already in your fleet.");
		return false;
	}
	// Catalog digits address runtime types directly, bypassing catalog row order.
	auto* Delivered = SimCopterHelicopterParking::SpawnOnFreePad(
		Context.Hangar.Get(), Context.Helicopter.Get(), TypeIndex, OutMessage);
	if (!Delivered) return false;
	Career->SetHelicopterOwned(TypeIndex, true);
	if (TypeIndex == 2) Career->SetApacheEncounterSpawned(true);
	OutMessage = FString::Printf(TEXT("%s delivered free of charge."), GetModelDisplayName(TypeIndex));
	return true;
}

bool BuyUpgrade(const FContext& Context, const int32 UpgradeRow, FString& OutMessage)
{
	ASimCopterMissionSystemActor* Missions = Context.Missions.Get();
	ASimCopterHelicopterPawn* Helicopter = Context.Helicopter.Get();
	const ESimCopterHelicopterTool Tool = GetToolForUpgradeRow(UpgradeRow);

	const FRowState State = GetUpgradeRowState(Context, UpgradeRow);
	if (Missions == nullptr || Helicopter == nullptr || Tool == ESimCopterHelicopterTool::Count || !State.bCanBuy)
	{
		OutMessage = State.Reason.IsEmpty() ? TEXT("That purchase is not available.") : State.Reason;
		return false;
	}

	Missions->AddSessionCash(-State.ItemValue);
	Helicopter->SetCareerEquipmentOwned(Tool, true);

	OutMessage = FString::Printf(
		TEXT("Bought the %s for %d Bucks."),
		SimCopterHelicopterRegistry::GetToolDisplayName(Tool),
		State.ItemValue);
	return true;
}

bool SellUpgrade(const FContext& Context, const int32 UpgradeRow, FString& OutMessage)
{
	ASimCopterMissionSystemActor* Missions = Context.Missions.Get();
	ASimCopterHelicopterPawn* Helicopter = Context.Helicopter.Get();
	const ESimCopterHelicopterTool Tool = GetToolForUpgradeRow(UpgradeRow);

	const FRowState State = GetUpgradeRowState(Context, UpgradeRow);
	if (Missions == nullptr || Helicopter == nullptr || Tool == ESimCopterHelicopterTool::Count || !State.bCanSell)
	{
		OutMessage = State.Reason.IsEmpty() ? TEXT("That sale is not available.") : State.Reason;
		return false;
	}

	Missions->AddSessionCash(State.ItemValue);
	Helicopter->SetCareerEquipmentOwned(Tool, false);

	OutMessage = FString::Printf(
		TEXT("Sold the %s for %d Bucks."),
		SimCopterHelicopterRegistry::GetToolDisplayName(Tool),
		State.ItemValue);
	return true;
}
}

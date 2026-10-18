#include "Flight/SimCopterAirOperations.h"
#include "Game/SimCopterSettings.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Ground/SimCopterInteraction.h"
#include "City/SimCity2000CityActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool CanAssignAirSupport(const SimCopterMissions::FSimCopterMissionRecord& Record)
{
	return Record.bActive && Record.EventId >= 0 &&
		!SimCopterMissions::FSimCopterMissionSystem::IsBaseLocationRecord(Record) &&
		Record.Category != SimCopterMissions::CAT_ExpireSilently &&
		Record.Category != SimCopterMissions::CAT_CompleteNow;
}
}

bool USimCopterAirOperationsSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type==EWorldType::Game || Type==EWorldType::PIE;
}
TStatId USimCopterAirOperationsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(SimCopterAirOperations,STATGROUP_Tickables);
}
ASimCopterTrafficSystemActor* USimCopterAirOperationsSubsystem::Traffic() const
{
	return Cast<ASimCopterTrafficSystemActor>(UGameplayStatics::GetActorOfClass(GetWorld(),ASimCopterTrafficSystemActor::StaticClass()));
}
ASimCopterMissionSystemActor* USimCopterAirOperationsSubsystem::Missions() const
{
	return Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(GetWorld(),ASimCopterMissionSystemActor::StaticClass()));
}
void USimCopterAirOperationsSubsystem::Deinitialize()
{
	Sites.Reset(); SupportHelicopter.Reset(); Super::Deinitialize();
}
bool USimCopterAirOperationsSubsystem::IsSupportUnlocked() const
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return false;
	// The helper is a reward for finishing THIS city. A carried career flag (including
	// one in an older save or a cheat warp transfer) cannot authorize work in a new city.
	const auto* Mission = GetWorld() ? Missions() : nullptr;
	return Mission && Mission->IsLevelComplete();
}
void USimCopterAirOperationsSubsystem::Tick(float DeltaSeconds)
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return;
	if (!GetWorld() || !GetWorld()->HasBegunPlay() || GetWorld()->IsPaused()) return;
	auto* Mission=Missions(); if (!Mission) return;
	ThinkSeconds-=DeltaSeconds;
	if (ThinkSeconds<=0)
	{
		ThinkSeconds=1.0f; EnsureSites(); RefreshRecoveryWaypoints();
		if (Mission->IsLevelComplete())
			if (auto* GI=GetWorld()->GetGameInstance())
				if (auto* Career=GI->GetSubsystem<USimCopterCareerSubsystem>(); Career && !Career->IsAirSupportUnlocked())
				{
					Career->SetAirSupportUnlocked(true);
					Mission->ShowAirOperationsMessage(TEXT("Level passed! Air Support is available. Select a map mission and choose AIR: MISSION in Dispatch to request help."));
				}
		EnsureSupport();
	}
	UpdateStalledCars(DeltaSeconds);
	if (IsSupportUnlocked()) UpdateSupport(DeltaSeconds);
}
void USimCopterAirOperationsSubsystem::EnsureSites()
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return;
	if (bSitesReady) return;
	auto* Road=Traffic(); auto* City=Road?Road->GetCityActor():nullptr;
	if (!Road || !City) return;
	// Original SC2 cities need no edited map asset: service yards occupy clear terrain and
	// harbors occupy open shore water. Never replace a city building to make room.
	for (int32 Pass=1;Pass<3;++Pass)
	{
		bool Placed=false;
		for (int32 Y=4;Y<124 && !Placed;++Y) for (int32 X=4;X<124 && !Placed;++X)
		{
			if (Road->GetXbldTileId(X,Y)!=0) continue;
			FVector Center; if (!Road->TryGetTileCenterWorldLocation(X,Y,Center)) continue;
			float Z=0; uint8 Class=255;
			if (!City->TryGetWaterGameplaySurface(Center,Z,Class)) continue;
			const bool Water=Class<10;
			if (Water!=(Pass==2)) continue;
			if (Sites.ContainsByPredicate([&](auto Site){return Site.IsValid() && FVector::Dist2D(Center,Site->GetActorLocation())<2400;})) continue;
			bool FootprintClear=true, NearShore=false, NearRoad=false;
			for (int32 DY=-1;DY<=1;++DY) for(int32 DX=-1;DX<=1;++DX)
			{
				const int32 Id=Road->GetXbldTileId(X+DX,Y+DY);
				if (Id>=0x1D && Id<=0x2B) NearRoad=true;
				if (DX==0 && DY==0) continue;
				FVector Neighbor; float NZ=0; uint8 NC=255;
				if (!Road->TryGetTileCenterWorldLocation(X+DX,Y+DY,Neighbor) || !City->TryGetWaterGameplaySurface(Neighbor,NZ,NC)) { FootprintClear=false; continue; }
				NearShore|=NC>=10;
				if (!Water && (NC<10 || FMath::Abs(NZ-Z)>25)) FootprintClear=false;
			}
			if (!FootprintClear || (Water && !NearShore) || (!Water && !NearRoad)) continue;
			Center.Z=Z+(Water?45:5);
			auto* Site=GetWorld()->SpawnActor<ASimCopterRecoverySite>(Center,FRotator::ZeroRotator);
			if (Site) { Site->Configure(static_cast<ESimCopterRecoverySite>(Pass),Center); Sites.Add(Site); Placed=true; }
		}
	}
	bSitesReady=!Sites.IsEmpty();
}
ASimCopterRecoverySite* USimCopterAirOperationsSubsystem::FindRecoverySite(const FVector& From,bool bBoat) const
{
	ASimCopterRecoverySite* Best=nullptr; float BestDistance=MAX_flt;
	for(auto Site:Sites) if (Site.IsValid() && Site->GetKind()==(bBoat?ESimCopterRecoverySite::Harbor:ESimCopterRecoverySite::Junkyard))
	{
		const float D=FVector::DistSquared(From,Site->GetActorLocation());
		if(D<BestDistance) { BestDistance=D; Best=Site.Get(); }
	}
	return Best;
}
void USimCopterAirOperationsSubsystem::RefreshRecoveryWaypoints()
{
	bool bCars=false, bBoats=false;
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		bCars|=It->IsCageRecoverableVehicle();
	for (TActorIterator<ASimCopterAmbientVehiclesActor> It(GetWorld());It;++It)
		bBoats|=It->HasRecoverableBoat();
	if (const auto* Mission=Missions())
		for (const auto& Record:Mission->GetMissionRecords())
		{
			if (!CanAssignAirSupport(Record)) continue;
			bCars|=(Record.TypeMask & SimCopterMissions::TYPE_VehicleTow)!=0;
			bBoats|=(Record.TypeMask & SimCopterMissions::TYPE_BoatTow)!=0;
		}
	for (auto Site:Sites) if (Site.IsValid())
		Site->SetWaypointActive(Site->GetKind()==ESimCopterRecoverySite::Harbor ? bBoats : bCars);
}
bool USimCopterAirOperationsSubsystem::FindServiceRoof(uint8 BuildingId,const FVector& From,FVector& OutRoof) const
{
	auto* Road=Traffic(); if(!Road) return false;
	if(!ServiceRoofs.Contains(BuildingId))
	{
		TArray<FVector> Roofs;
		for(int32 Y=0;Y<128;++Y) for(int32 X=0;X<128;++X)
		{
			if(Road->GetXbldTileId(X,Y)!=BuildingId) continue;
			FVector Center; float Extent=0;
			if(Road->TryGetBuildingRoofPost(X,Y,Center,Extent)) Roofs.Add(Center);
		}
		if(Roofs.IsEmpty()) return false;
		ServiceRoofs.Add(BuildingId,MoveTemp(Roofs));
	}
	float Best=MAX_flt;
	for(const FVector& Roof:ServiceRoofs[BuildingId])
	{
		const float D=FVector::DistSquared(From,Roof);
		if(D<Best) { Best=D; OutRoof=Roof; }
	}
	return Best<MAX_flt;
}

bool USimCopterAirOperationsSubsystem::CanPlayerTow() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const ASimCopterHelicopterPawn* Helicopter = Cast<ASimCopterHelicopterPawn>(Player);
	if (const auto* Pilot = Cast<ASimCopterOnFootPawn>(Player))
		Helicopter = Pilot->GetParkedHelicopter();
	return Helicopter && !Helicopter->IsSupportAircraft() && Helicopter->IsToolAvailable(ESimCopterHelicopterTool::CaptureCage);
}

void USimCopterAirOperationsSubsystem::UpdateStalledCars(float DeltaSeconds)
{
	const bool bCanTow = CanPlayerTow();
	// Supplemental jobs: ten minutes with a cage, fifteen without. Retain the saved
	// 150-second scale so existing countdowns and equipment changes need no migration.
	StalledCarSeconds -= DeltaSeconds * (bCanTow ? 0.25f : 1.0f / 6.0f);
	if (StalledCarSeconds <= 0.0f)
	{
		StalledCarSeconds = 150.0f;
		StartStalledCar(bCanTow);
	}
}

void USimCopterAirOperationsSubsystem::StartStalledCar(bool bCanTow)
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return;
	if(!FindRecoverySite(FVector::ZeroVector,false)) return;
	// Original missions own dispatch while there is work to do. Collision-created
	// recoveries still use EnsureTowMission immediately and do not pass through here.
	if (auto* Mission=Missions())
		for (const auto& R:Mission->GetMissionRecords())
			if (CanAssignAirSupport(R) && !SimCopterMissions::IsRecoveryMission(R.TypeMask)) return;
	int32 Existing=0;
	for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It) if(It->IsTowableVehicle()) ++Existing;
	if(Existing >= 1) return;
	auto* Road=Traffic();
	for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		if(It->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle && !It->IsHidden() && !It->IsCriminalCar() &&
			!It->IsVehicleImmobilized() && It->MissionEventId==INDEX_NONE && Road && !Road->IsDispatchVehicle(**It))
		{ It->SetVehicleStalled(false); break; }
}
void USimCopterAirOperationsSubsystem::EnsureSupport()
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return;
	if(!IsSupportUnlocked() || SupportHelicopter.IsValid()) return;
	for(TActorIterator<ASimCopterHelicopterPawn> It(GetWorld());It;++It)
		if(It->IsSupportAircraft()) { SupportHelicopter=*It; return; }
	auto* Road=Traffic(); if(!Road) return;
	FVector Pad;
	if(!Road->TryGetAirportPadWorldLocation(0,Pad)) return;
	SupportHome=Pad+FVector(700,700,2200);
	const FTransform Spawn(FRotator::ZeroRotator,SupportHome);
	auto* Heli=GetWorld()->SpawnActorDeferred<ASimCopterHelicopterPawn>(ASimCopterHelicopterPawn::StaticClass(),Spawn,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if(!Heli) return;
	Heli->AutoPossessPlayer=EAutoReceiveInput::Disabled;
	Heli->GetAirOperations()->SetSupportAircraft(true);
	Heli->GetAirOperations()->SetSupportGoal(SupportHome);
	UGameplayStatics::FinishSpawningActor(Heli,Spawn);
	if (!Heli->SwitchHelicopterModel(0)) { Heli->Destroy(); return; }
	// Do not introduce the helper inside a tall building beside the airport.
	const FBox SpawnBounds = Heli->GetParkingWorldBounds();
	FCollisionQueryParams SpawnQuery(SCENE_QUERY_STAT(AirSupportSpawn),true,Heli);
	bool bClearSpawn = false;
	for (int32 Attempt=0;Attempt<64;++Attempt)
	{
		const FVector Offset(0,0,Attempt*500.0f);
		if (!GetWorld()->OverlapBlockingTestByChannel(SpawnBounds.GetCenter()+Offset,FQuat::Identity,
			ECC_Visibility,FCollisionShape::MakeBox(SpawnBounds.GetExtent()+FVector(20)),SpawnQuery))
		{
			SupportHome += Offset;
			Heli->SetAutopilotTransform(SupportHome,FRotator::ZeroRotator,0.05f);
			Heli->GetAirOperations()->SetSupportGoal(SupportHome);
			bClearSpawn = true;
			break;
		}
	}
	if (!bClearSpawn) { Heli->Destroy(); return; }
	Heli->SetDebugToolGrant(ESimCopterHelicopterTool::CaptureCage,true);
	Heli->GetAirOperations()->SetStatus(TEXT("AIR SUPPORT: available"));
	SupportHelicopter=Heli;
}
bool USimCopterAirOperationsSubsystem::OrderSupport(int32 EventId)
{
	if (!USimCopterSettings::RemakeFeaturesEnabled(this)) return false;
	if(!IsSupportUnlocked()) return false;
	EnsureSupport(); if(!SupportHelicopter.IsValid()) return false;
	if(SupportHelicopter->GetPassengerCount()>0 || SupportHelicopter->GetAirOperations()->HasCargo()) return false;
	if(!Missions() || !Missions()->GetMissionRecords().ContainsByPredicate(
		[&](const auto& Record){return Record.EventId==EventId && CanAssignAirSupport(Record);})) return false;
	AssignedEvent=EventId;
	WorkSeconds=0; MissionAttemptSeconds=0; bReturning=false;
	bHasLandingSurface=false;
	return true;
}
void USimCopterAirOperationsSubsystem::RecallSupport()
{
	AssignedEvent=INDEX_NONE; bReturning=true;
	WorkSeconds=0; MissionAttemptSeconds=0;
}
FString USimCopterAirOperationsSubsystem::GetSupportStatus() const
{
	if(!IsSupportUnlocked()) return TEXT("Air Support unlocks after passing this level.");
	if(!SupportHelicopter.IsValid()) return TEXT("Air Support preparing at base.");
	return SupportHelicopter->GetAirOperations()->GetStatus();
}
bool USimCopterAirOperationsSubsystem::IsRescuingPerson(const ASimCopterGroundAgent* Person) const
{
	return Person && SupportHelicopter.IsValid() && AssignedEvent != INDEX_NONE &&
		Person->MissionEventId == AssignedEvent && !Person->HasMissionResolutionReported() &&
		!Person->IsMissionCarried() && (!Person->GetBehaviorCarrier() || Person->GetBehaviorCarrier() == SupportHelicopter.Get());
}

void USimCopterAirOperationsSubsystem::UpdateSupport(float DeltaSeconds)
{
	auto* Heli=SupportHelicopter.Get(); auto* Mission=Missions(); auto* Road=Traffic();
	if(!Heli || !Mission || !Road) return;
	auto* Ops=Heli->GetAirOperations();
	const auto HoverOver = [Heli](const FVector& Surface, float DoorClearance)
	{
		const FVector Here = Heli->GetActorLocation();
		const FBox Bounds = Heli->GetParkingWorldBounds();
		const float DoorOffset = Here.Z - Heli->GetPassengerDropWorldLocation().Z;
		const float BodyOffset = Bounds.IsValid ? Here.Z - Bounds.Min.Z + 16.0f : DoorOffset;
		return Surface + FVector(0,0,FMath::Max(DoorOffset+DoorClearance,BodyOffset));
	};
	const auto FindLanding = [&](const FVector& Origin, int32 Event, ASimCopterGroundAgent* Person, FVector& Surface)
	{
		const FBox Bounds = Heli->GetParkingWorldBounds();
		const FVector Offset = Bounds.GetCenter()-Heli->GetActorLocation();
		const FVector Half = Bounds.GetExtent();
		const float Radius = FMath::Max(275.0f, float(FMath::Max(Half.X+FMath::Abs(Offset.X), Half.Y+FMath::Abs(Offset.Y)))+24);
		// Hold one reachable clearing while the passenger walks to it; chasing the moving
		// person's feet continually changed the descent point beside walls.
		if (!bHasLandingSurface || LandingEvent != Event || LandingPerson.Get() != Person ||
			FVector::Dist2D(Origin,LandingSurface)>3200 || (Person && FMath::Abs(Origin.Z-LandingSurface.Z)>40) ||
			!Road->IsTransportClearing(LandingSurface,Radius))
		{
			LandingEvent=Event; LandingPerson=Person;
			bHasLandingSurface=Road->TryFindTransportClearing(Origin,LandingSurface,Radius,3200,Person?&Origin:nullptr);
		}
		Surface=LandingSurface;
		return bHasLandingSurface;
	};
	const auto& Records=Mission->GetMissionRecords();
	const auto* Record=Records.FindByPredicate([&](const auto& R){return CanAssignAirSupport(R) && R.EventId==AssignedEvent;});
	// Loaded people and slung cargo are always delivered before accepting another job or recall.
	if((Ops->GetTowVehicle() || Ops->HasBoat()))
	{
		auto* Site=FindRecoverySite(Heli->GetActorLocation(),Ops->HasBoat());
		if(Site)
		{
			Ops->SetSupportGoal(Site->GetActorLocation()+FVector(0,0,float(Heli->GetActorLocation().Z-Heli->GetCaptureCageMountTransform().GetLocation().Z)+Ops->GetCableLength()+70));
			Ops->SetStatus(TEXT("AIR SUPPORT: delivering recovered vehicle"));
			Ops->TryDeliverCargo();
		}
		return;
	}
	if(Heli->GetPassengerCount()>0)
	{
		const auto Slots=Heli->GetMissionPassengerSlots();
		bool Police=false, Medical=false;
		for(const auto& Slot:Slots) if(auto* P=Slot.Person.Get()) { Police|=P->IsHandcuffed() && !P->IsMedevacVictim(); Medical|=P->IsMedevacVictim(); }
		FVector Destination=SupportHome-FVector(700,700,2200);
		Road->TryGetAirportPadWorldLocation(0,Destination);
		if(Police || Medical)
		{
			if(!FindServiceRoof(Police?0xD2:0xD1,Heli->GetActorLocation(),Destination)) { Ops->SetStatus(TEXT("AIR SUPPORT: awaiting a service roof")); return; }
		}
		else if (!Slots.IsEmpty() && Slots[0].Person.IsValid() &&
			Slots[0].Person->GetMissionPassengerKind()==ESimCopterMissionPassengerKind::Transport)
		{
			int32 X=0,Y=0;
			const int32 Event = Slots[0].Person->MissionEventId;
			FVector Origin;
			if (!Mission->TryGetMissionDestinationTile(Event,X,Y) ||
				!Road->TryGetTileCenterWorldLocation(X,Y,Origin) || !FindLanding(Origin,Event,nullptr,Destination))
			{ Ops->SetSupportGoal(Heli->GetActorLocation()); Ops->SetStatus(TEXT("AIR SUPPORT: awaiting a clear drop-off")); return; }
		}
		Ops->SetSupportGoal(HoverOver(Destination,25));
		Ops->SetStatus(Police?TEXT("AIR SUPPORT: transporting prisoners"):Medical?TEXT("AIR SUPPORT: transporting patients"):TEXT("AIR SUPPORT: transporting passengers"));
		if(FVector::Distance(Heli->GetPassengerDropWorldLocation(),Destination)<180 && Heli->GetVelocityCmPerSec().Size()<100 && !Police)
		{
			for(const auto& Slot:Slots) if(auto* P=Slot.Person.Get())
			{
				// Use the same occupied-door queue as the player's aircraft.
				FVector Exit;
				if (!Heli->FindClearPassengerExit(P,Exit) || (P->MissionEventId != INDEX_NONE &&
					!Mission->IsPassengerDeliveryLocationAllowed(P->GetMissionPassengerKind(), Exit-FVector(0,0,P->GetCapsuleHalfHeightCm()), P->MissionEventId))) break;
				if (!P->AlightFromCarrier()) break;
				if (Mission->NotifyMissionPersonDelivered(P))
				{
					if (P->IsMedevacVictim()) P->ResumeDeliveredPatientBehavior();
					else if (P->GetMissionPassengerKind()==ESimCopterMissionPassengerKind::Transport) P->BecomeAmbientPedestrian();
					else { P->SetMissionRetiredAlivePose(); P->SetLifeSpan(8); }
				}
			}
			WorkSeconds=0;
		}
		return;
	}
	// Each player order authorizes one event. Finishing or losing that event never
	// claims a different emergency, even when other work is waiting nearby.
	if(!Record || bReturning)
	{
		AssignedEvent=INDEX_NONE; Ops->SetSupportGoal(SupportHome);
		WorkSeconds=0; MissionAttemptSeconds=0;
		Ops->SetStatus(bReturning?TEXT("AIR SUPPORT: recalled to base"):TEXT("AIR SUPPORT: awaiting orders"));
		if(FVector::Distance(Heli->GetActorLocation(),SupportHome)<100) bReturning=false;
		return;
	}
	MissionAttemptSeconds+=DeltaSeconds;
	if(MissionAttemptSeconds>180) { AssignedEvent=INDEX_NONE; bReturning=true; MissionAttemptSeconds=0; Ops->SetStatus(TEXT("AIR SUPPORT: unable to reach target; returning")); return; }
	Ops->SetStatus(FString::Printf(TEXT("AIR SUPPORT: %s"),*Record->Name));
	ASimCopterGroundAgent* TargetPerson=nullptr; ASimCopterGroundAgent* TargetCar=nullptr;
	for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
	{
		if(It->IsHidden() || It->IsActorBeingDestroyed() || It->IsMissionCarried() || It->GetBehaviorCarrier()) continue;
		if(It->GetTowMissionId()==AssignedEvent && It->IsTowableVehicle()) TargetCar=*It;
		if(It->MissionEventId==AssignedEvent && It->GetAgentKind()==ESimCopterGroundAgentKind::Pedestrian && !It->IsEmergencyCrewMember() && !It->HasMissionResolutionReported())
		{ TargetPerson=*It; break; }
	}
	FVector Target;
	if (TargetPerson && (Record->TypeMask & (SimCopterMissions::TYPE_Medevac |
		SimCopterMissions::TYPE_RescuePeople | SimCopterMissions::TYPE_Transport))) TargetPerson->BeginAirSupportRescue();
	if(Record->TypeMask==SimCopterMissions::TYPE_BoatTow)
	{
		auto* Ambient=Cast<ASimCopterAmbientVehiclesActor>(UGameplayStatics::GetActorOfClass(GetWorld(),ASimCopterAmbientVehiclesActor::StaticClass()));
		if(!Ambient || !Ambient->GetBoatTowLocation(0,Target)) { AssignedEvent=INDEX_NONE; return; }
		const float Height=Heli->GetActorLocation().Z-Heli->GetCaptureCageMountTransform().GetLocation().Z+180;
		Ops->SetSupportGoal(Target+FVector(0,0,Height));
		if(FVector::Dist2D(Heli->GetActorLocation(),Target)<120 && FMath::Abs(Heli->GetActorLocation().Z-Target.Z-Height)<80)
		{
			Heli->SetSelectedTool(ESimCopterHelicopterTool::CaptureCage);
			if(!Ops->IsDeployed()) Ops->UseCargoTool(); else Ops->TryGrabCargo();
		}
		return;
	}
	if(TargetCar) Target=TargetCar->GetActorLocation();
	else if(TargetPerson) Target=TargetPerson->GetActorLocation()-FVector(0,0,TargetPerson->GetCapsuleHalfHeightCm());
	else if(!Road->TryGetTileCenterWorldLocation(Record->TileX,Record->TileY,Target)) { AssignedEvent=INDEX_NONE; return; }
	const bool bTransportPickup = TargetPerson && TargetPerson->GetMissionPassengerKind()==ESimCopterMissionPassengerKind::Transport;
	if (bTransportPickup)
	{
		FVector Clearing;
		if (!FindLanding(Target,AssignedEvent,TargetPerson,Clearing))
		{ Ops->SetSupportGoal(Heli->GetActorLocation()); Ops->SetStatus(TEXT("AIR SUPPORT: awaiting a clear pickup")); return; }
		Target=Clearing;
	}
	const FVector Goal = TargetCar ? Target+FVector(0,0,float(Heli->GetActorLocation().Z-Heli->GetCaptureCageMountTransform().GetLocation().Z)+180) : HoverOver(Target,45);
	const float Height = Goal.Z-Target.Z;
	Ops->SetSupportGoal(Goal);
	if(FVector::Dist2D(Heli->GetActorLocation(),Target)>170 || FMath::Abs(Heli->GetActorLocation().Z-Target.Z-Height)>100) return;
	if (bTransportPickup)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SupportPassengerApproach),true,Heli);
		for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It) Query.AddIgnoredActor(*It);
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		const FVector Feet=TargetPerson->GetActorLocation()-FVector(0,0,TargetPerson->GetCapsuleHalfHeightCm());
		if (GetWorld()->SweepSingleByObjectType(Hit,Feet+FVector(0,0,50),Target+FVector(0,0,50),
			FQuat::Identity,Objects,FCollisionShape::MakeSphere(24),Query))
		{ bHasLandingSurface=false; WorkSeconds=0; return; }
		// Walk the original person to the settled helper. The player's BHAV 291 target
		// remains player-owned; this guidance only exists during an assigned helper pickup.
		TargetPerson->SetGuidanceMoveTarget(Target+FVector(0,0,TargetPerson->GetCapsuleHalfHeightCm()),
			0.5f,ASimCopterGroundAgent::ShippedPassengerWalkSpeedCmPerSec);
		if (FVector::Dist2D(TargetPerson->GetActorLocation(),Heli->GetActorLocation())>170 ||
			Heli->GetVelocityCmPerSec().Size()>100) { WorkSeconds=0; return; }
	}
	WorkSeconds+=DeltaSeconds;
	if(TargetCar)
	{
		Heli->SetSelectedTool(ESimCopterHelicopterTool::CaptureCage);
		if(!Ops->IsDeployed()) Ops->UseCargoTool();
		else Ops->TryGrabCargo();
		return;
	}
	if(TargetPerson && WorkSeconds>1.5f)
	{
		if(TargetPerson->IsArrestableSuspect()) TargetPerson->StunForArrest();
		if(!TargetPerson->IsRiotParticipant()) TargetPerson->BoardCarrier(Heli,false,true);
		else
		{
			FSimCopterInteractionEvent Event; Event.Source=Heli; Event.Mode=ESimCopterInteractionMode::Megaphone; Event.MessageIndex=3;
			TargetPerson->ApplyInteraction(Event);
		}
		WorkSeconds=0; return;
	}
	if((Record->TypeMask & (SimCopterMissions::TYPE_BuildingFire|SimCopterMissions::TYPE_CarFire|SimCopterMissions::TYPE_Debris))!=0)
	{
		ASimCopterMissionSystemActor::FServiceFireTarget Fire;
		if(Mission->TryAcquireServiceFireTarget(FIntPoint(Record->TileX,Record->TileY),5,Fire))
		{
			Mission->SpawnServiceWaterJet(Heli->GetPassengerDropWorldLocation(),Fire.World);
			if(WorkSeconds>0.3f) { Mission->ApplyWaterParticleImpact(Fire.World,65536); WorkSeconds=0; }
		}
	}
	if((Record->TypeMask & SimCopterMissions::TYPE_TrafficJam)!=0 && WorkSeconds>5) Mission->ClearTrafficJamEvent(AssignedEvent);
}
void USimCopterAirOperationsSubsystem::SerializeState(FArchive& Ar)
{
	// Keep the legacy byte layout. Automatic assignments from older saves were not
	// player orders; retire them, but UpdateSupport still delivers loaded cargo/people.
	bool bLegacyAutomatic=false;
	Ar << bLegacyAutomatic << bReturning << AssignedEvent << WorkSeconds << MissionAttemptSeconds << StalledCarSeconds << SupportHome;
	if(Ar.IsLoading())
	{
		if(bLegacyAutomatic) RecallSupport();
		SupportHelicopter.Reset(); ThinkSeconds=0; bHasLandingSurface=false; LandingPerson.Reset();
	}
}

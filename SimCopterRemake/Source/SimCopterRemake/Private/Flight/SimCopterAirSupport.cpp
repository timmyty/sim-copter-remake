#include "Flight/SimCopterAirOperations.h"
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
	auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
	auto* Career=GI?GI->GetSubsystem<USimCopterCareerSubsystem>():nullptr;
	return Career && Career->IsAirSupportUnlocked();
}
void USimCopterAirOperationsSubsystem::Tick(float DeltaSeconds)
{
	if (!GetWorld() || !GetWorld()->HasBegunPlay() || GetWorld()->IsPaused()) return;
	auto* Mission=Missions(); if (!Mission) return;
	ThinkSeconds-=DeltaSeconds;
	if (ThinkSeconds<=0)
	{
		ThinkSeconds=1.0f; EnsureSites();
		if (Mission->IsLevelComplete())
			if (auto* GI=GetWorld()->GetGameInstance())
				if (auto* Career=GI->GetSubsystem<USimCopterCareerSubsystem>(); Career && !Career->IsAirSupportUnlocked())
				{
					Career->SetAirSupportUnlocked(true);
					Mission->ShowAirOperationsMessage(TEXT("Level passed! Air Support is now helping. Keep flying, assign a mission in Dispatch, or choose Next Level in the hangar."));
				}
		EnsureSupport();
	}
	UpdateStalledCars(DeltaSeconds);
	if (IsSupportUnlocked()) UpdateSupport(DeltaSeconds);
}
void USimCopterAirOperationsSubsystem::EnsureSites()
{
	if (bSitesReady) return;
	auto* Road=Traffic(); auto* City=Road?Road->GetCityActor():nullptr;
	if (!Road || !City) return;
	// Original SC2 cities need no edited map asset: service yards occupy clear terrain and
	// harbors occupy open shore water. Never replace a city building to make room.
	for (int32 Pass=0;Pass<3;++Pass)
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
	for(auto Site:Sites) if (Site.IsValid() && (Site->GetKind()==ESimCopterRecoverySite::Harbor)==bBoat)
	{
		const float D=FVector::DistSquared(From,Site->GetActorLocation());
		if(D<BestDistance) { BestDistance=D; Best=Site.Get(); }
	}
	return Best;
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
	return Helicopter && !Helicopter->IsSupportAircraft() && Helicopter->IsToolAvailable(ESimCopterHelicopterTool::TowClamp);
}

void USimCopterAirOperationsSubsystem::UpdateStalledCars(float DeltaSeconds)
{
	const bool bCanTow = CanPlayerTow();
	// Remake ambient jobs: every 15 minutes without towing, versus 2.5 minutes when equipped.
	// Scaling the existing saved countdown also handles buying/selling the clamp mid-interval.
	StalledCarSeconds -= DeltaSeconds * (bCanTow ? 1.0f : 1.0f / 6.0f);
	if (StalledCarSeconds <= 0.0f)
	{
		StalledCarSeconds = 150.0f;
		StartStalledCar(bCanTow);
	}
}

void USimCopterAirOperationsSubsystem::StartStalledCar(bool bCanTow)
{
	if(!FindRecoverySite(FVector::ZeroVector,false)) return;
	int32 Existing=0;
	for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It) if(It->IsTowableVehicle()) ++Existing;
	if(Existing >= (bCanTow ? 2 : 1)) return;
	auto* Road=Traffic();
	for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		if(It->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle && !It->IsHidden() && !It->IsCriminalCar() &&
			!It->IsVehicleImmobilized() && It->MissionEventId==INDEX_NONE && Road && !Road->IsDispatchVehicle(**It))
		{ It->SetVehicleStalled(false); break; }
}
void USimCopterAirOperationsSubsystem::EnsureSupport()
{
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
	Heli->SwitchHelicopterModel(0);
	Heli->SetDebugToolGrant(ESimCopterHelicopterTool::TowClamp,true);
	Heli->SetDebugToolGrant(ESimCopterHelicopterTool::CaptureCage,true);
	Heli->GetAirOperations()->SetStatus(TEXT("AIR SUPPORT: available"));
	SupportHelicopter=Heli;
}
bool USimCopterAirOperationsSubsystem::OrderSupport(int32 EventId,bool bEnableAutomatic)
{
	if(!IsSupportUnlocked()) return false;
	EnsureSupport(); if(!SupportHelicopter.IsValid()) return false;
	if(SupportHelicopter->GetPassengerCount()>0 || SupportHelicopter->GetAirOperations()->HasCargo()) return false;
	if(!bEnableAutomatic && (!Missions() || !Missions()->GetMissionRecords().ContainsByPredicate(
		[&](const auto& Record){return Record.EventId==EventId && CanAssignAirSupport(Record);}))) return false;
	this->bAutomatic=bEnableAutomatic; AssignedEvent=bEnableAutomatic?INDEX_NONE:EventId;
	WorkSeconds=0; MissionAttemptSeconds=0; bReturning=false;
	return true;
}
void USimCopterAirOperationsSubsystem::RecallSupport()
{
	bAutomatic=false; AssignedEvent=INDEX_NONE; bReturning=true;
	WorkSeconds=0; MissionAttemptSeconds=0;
}
FString USimCopterAirOperationsSubsystem::GetSupportStatus() const
{
	if(!IsSupportUnlocked()) return TEXT("Air Support unlocks after passing a level.");
	if(!SupportHelicopter.IsValid()) return TEXT("Air Support preparing at base.");
	return SupportHelicopter->GetAirOperations()->GetStatus();
}
void USimCopterAirOperationsSubsystem::UpdateSupport(float DeltaSeconds)
{
	auto* Heli=SupportHelicopter.Get(); auto* Mission=Missions(); auto* Road=Traffic();
	if(!Heli || !Mission || !Road) return;
	auto* Ops=Heli->GetAirOperations();
	const auto& Records=Mission->GetMissionRecords();
	const auto* Record=Records.FindByPredicate([&](const auto& R){return CanAssignAirSupport(R) && R.EventId==AssignedEvent;});
	// Loaded people and slung cargo are always delivered before accepting another job or recall.
	if((Ops->GetTowVehicle() || Ops->HasBoat()))
	{
		auto* Site=FindRecoverySite(Heli->GetActorLocation(),Ops->HasBoat());
		if(Site) { Ops->SetSupportGoal(Site->GetActorLocation()+FVector(0,0,float(Heli->GetActorLocation().Z-Heli->GetCargoAnchorWorldLocation().Z)+Ops->GetCableLength()+70)); Ops->SetStatus(TEXT("AIR SUPPORT: delivering recovered vehicle")); }
		return;
	}
	if(Heli->GetPassengerCount()>0)
	{
		const auto Slots=Heli->GetMissionPassengerSlots();
		bool Police=false, Medical=false;
		for(const auto& Slot:Slots) if(auto* P=Slot.Person.Get()) { Police|=P->IsHandcuffed() && !P->IsMedevacVictim(); Medical|=P->IsMedevacVictim(); }
		FVector Destination=SupportHome-FVector(700,700,2200);
		if(Police || Medical)
		{
			if(!FindServiceRoof(Police?0xD2:0xD1,Heli->GetActorLocation(),Destination)) { Ops->SetStatus(TEXT("AIR SUPPORT: awaiting a service roof")); return; }
		}
		else if(Record && (Record->TypeMask & SimCopterMissions::TYPE_Transport))
		{
			int32 X=0,Y=0;
			if(Mission->TryGetMissionDestinationTile(Record->EventId,X,Y)) Road->TryGetTileCenterWorldLocation(X,Y,Destination);
		}
		Ops->SetSupportGoal(Destination+FVector(0,0,Heli->GetPassengerDropHeightOffsetCm()+25));
		Ops->SetStatus(Police?TEXT("AIR SUPPORT: transporting prisoners"):Medical?TEXT("AIR SUPPORT: transporting patients"):TEXT("AIR SUPPORT: transporting passengers"));
		if(FVector::Distance(Heli->GetPassengerDropWorldLocation(),Destination)<180 && Heli->GetVelocityCmPerSec().Size()<100 && !Police)
		{
			for(const auto& Slot:Slots) if(auto* P=Slot.Person.Get())
			{
				// Use the same occupied-door queue as the player's aircraft.
				if (!P->AlightFromCarrier()) break;
				if(Mission->NotifyMissionPersonDelivered(P)) { P->SetMissionRetiredAlivePose(); P->SetLifeSpan(8); }
			}
			WorkSeconds=0;
		}
		return;
	}
	if(!Record && bAutomatic && !bReturning)
	{
		const int32 PlayerIndex=Mission->GetMapFocusRecordIndex();
		float Nearest=MAX_flt;
		for(const auto& R:Records)
		{
			if(!CanAssignAirSupport(R)) continue;
			FVector Point; if(!Road->TryGetTileCenterWorldLocation(R.TileX,R.TileY,Point)) continue;
			float D=FVector::DistSquared(Heli->GetActorLocation(),Point);
			if(Records.IsValidIndex(PlayerIndex) && Records[PlayerIndex].EventId==R.EventId) D+=100000000;
			if(D<Nearest) { Nearest=D; Record=&R; AssignedEvent=R.EventId; WorkSeconds=0; MissionAttemptSeconds=0; }
		}
	}
	if(!Record || bReturning)
	{
		AssignedEvent=INDEX_NONE; Ops->SetSupportGoal(SupportHome);
		Ops->SetStatus(bReturning?TEXT("AIR SUPPORT: recalled to base"):TEXT("AIR SUPPORT: waiting for a mission"));
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
	if(Record->TypeMask==SimCopterMissions::TYPE_BoatTow)
	{
		auto* Ambient=Cast<ASimCopterAmbientVehiclesActor>(UGameplayStatics::GetActorOfClass(GetWorld(),ASimCopterAmbientVehiclesActor::StaticClass()));
		if(!Ambient || !Ambient->GetBoatTowLocation(0,Target)) { AssignedEvent=INDEX_NONE; return; }
		const float Height=Heli->GetActorLocation().Z-Heli->GetCargoAnchorWorldLocation().Z+180;
		Ops->SetSupportGoal(Target+FVector(0,0,Height));
		if(FVector::Dist2D(Heli->GetActorLocation(),Target)<120 && FMath::Abs(Heli->GetActorLocation().Z-Target.Z-Height)<80)
		{
			Heli->SetSelectedTool(ESimCopterHelicopterTool::TowClamp);
			if(!Ops->IsDeployed()) Ops->UseCargoTool(); else Ops->TryGrabCargo();
		}
		return;
	}
	if(TargetCar) Target=TargetCar->GetActorLocation();
	else if(TargetPerson) Target=TargetPerson->GetActorLocation()-FVector(0,0,TargetPerson->GetCapsuleHalfHeightCm());
	else if(!Road->TryGetTileCenterWorldLocation(Record->TileX,Record->TileY,Target)) { AssignedEvent=INDEX_NONE; return; }
	const float Height=TargetCar?float(Heli->GetActorLocation().Z-Heli->GetCargoAnchorWorldLocation().Z)+180:Heli->GetPassengerDropHeightOffsetCm()+45;
	Ops->SetSupportGoal(Target+FVector(0,0,Height));
	if(FVector::Dist2D(Heli->GetActorLocation(),Target)>170 || FMath::Abs(Heli->GetActorLocation().Z-Target.Z-Height)>100) return;
	WorkSeconds+=DeltaSeconds;
	if(TargetCar)
	{
		Heli->SetSelectedTool(ESimCopterHelicopterTool::TowClamp);
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
	Ar << bAutomatic << bReturning << AssignedEvent << WorkSeconds << MissionAttemptSeconds << StalledCarSeconds << SupportHome;
	if(Ar.IsLoading()) { SupportHelicopter.Reset(); ThinkSeconds=0; }
}

#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Ground/SimCopterParticleFX.h"
#include "City/SimCity2000CityActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
struct FRecoveryMesh
{
	TArray<FVector> V, N;
	TArray<int32> I;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	void Box(const FVector& Center, const FVector& Half, const FLinearColor& Color, const FQuat& Rotation = FQuat::Identity)
	{
		static const int32 Faces[6][4] = {{0,2,3,1},{4,5,7,6},{0,1,5,4},{2,6,7,3},{0,4,6,2},{1,3,7,5}};
		FVector P[8];
		for (int32 K=0; K<8; ++K) P[K]=Center+Rotation.RotateVector(FVector((K&1)?Half.X:-Half.X,(K&2)?Half.Y:-Half.Y,(K&4)?Half.Z:-Half.Z));
		for (const auto& Face : Faces)
		{
			const int32 Base=V.Num();
			const FVector Normal=FVector::CrossProduct(P[Face[1]]-P[Face[0]],P[Face[2]]-P[Face[0]]).GetSafeNormal();
			for (int32 J=0; J<4; ++J) { V.Add(P[Face[J]]); N.Add(Normal); UV.Add(FVector2D(J==1||J==2,J>=2)); C.Add(Color); }
			I.Append({Base,Base+1,Base+2,Base,Base+2,Base+3});
		}
	}
	void Bar(const FVector& A, const FVector& B, float Radius, const FLinearColor& Color)
	{
		Box((A+B)*0.5f,FVector(Radius,Radius,FVector::Distance(A,B)*0.5f),Color,FQuat::FindBetweenNormals(FVector::UpVector,(B-A).GetSafeNormal()));
	}
	void Apply(UProceduralMeshComponent* Mesh, bool bCollision=false)
	{
		Mesh->CreateMeshSection_LinearColor(0,V,I,N,UV,C,TArray<FProcMeshTangent>(),bCollision);
	}
};

template <typename T> T* ActorIn(UWorld* World)
{
	return Cast<T>(UGameplayStatics::GetActorOfClass(World,T::StaticClass()));
}
}

namespace SimCopterAirOperations
{
void DrawTaserDischarge(USimCopterParticleFXComponent* FX, const FVector& Start, const FVector& End)
{
	if (!FX) return;
	const FVector Side = FVector::CrossProduct((End-Start).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
	for (int32 K=0; K<24; ++K)
	{
		const FVector Arc = FMath::Lerp(Start, End, K/23.0f) + Side * (K%2 ? 3.0f : -3.0f);
		FX->SpawnParticle(Arc, FVector::ZeroVector, 2, FLinearColor(0.2f,0.65f,1), 0.18f, 0);
	}
}
bool IsCriminalState(int32 State) { return State >= 10 && State <= 13; }
bool FallCausesInjury(float DistanceCm, bool bInWater) { return !bInWater && DistanceCm >= SafeFallCm; }
FVector DentVertex(const FVector& Vertex, const FVector& Impact, const FVector& BodyCenter)
{
	const float Weight=FMath::Square(FMath::Clamp(1.0f-FVector::Distance(Vertex,Impact)/80.0f,0.0f,1.0f));
	return Vertex+(BodyCenter-Vertex).GetSafeNormal()*FMath::Min(22.0f,FVector::Distance(Vertex,BodyCenter)*0.45f)*Weight;
}
ASimCopterGroundAgent* TracePerson(UWorld* World, AActor* Source, const FVector& Start, const FVector& Direction, float Range, FVector& OutEnd)
{
	OutEnd=Start+Direction.GetSafeNormal()*Range;
	if (!World) return nullptr;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AirOperationsAim),true,Source);
	FHitResult Hit;
	// Test rendered geometry first, then the small figure capsules. A non-suspect blocks a shot.
	if (World->LineTraceSingleByChannel(Hit,Start,OutEnd,ECC_Visibility,Query)) OutEnd=Hit.ImpactPoint;
	ASimCopterGroundAgent* Best=Cast<ASimCopterGroundAgent>(Hit.GetActor());
	float BestDistance=FVector::Distance(Start,OutEnd);
	for (TActorIterator<ASimCopterGroundAgent> It(World); It; ++It)
	{
		if (It->GetAgentKind()!=ESimCopterGroundAgentKind::Pedestrian || It->IsHidden() || It->IsMissionCarried() || It->GetBehaviorCarrier()) continue;
		const FVector Target=It->GetActorLocation();
		const float Along=FVector::DotProduct(Target-Start,Direction.GetSafeNormal());
		if (Along<0 || Along>BestDistance) continue;
		if (FVector::Distance(Start+Direction.GetSafeNormal()*Along,Target)>FMath::Max(12.0f,It->GetCapsuleHalfHeightCm()*0.65f)) continue;
		Best=*It; BestDistance=Along; OutEnd=Target;
	}
	return Best;
}
}

ASimCopterRecoverySite::ASimCopterRecoverySite()
{
	Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RecoverySite"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->bUseComplexAsSimpleCollision=true;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/M_SimCopterLitVertexColor.M_SimCopterLitVertexColor"));
	if (Mat.Succeeded()) Material=Mat.Object;
}

void ASimCopterRecoverySite::Configure(ESimCopterRecoverySite Kind, const FVector& Location)
{
	SiteKind=Kind; SetActorLocation(Location);
	FRecoveryMesh Shape;
	const bool bHarbor=Kind==ESimCopterRecoverySite::Harbor;
	const FLinearColor Steel(0.23f,0.30f,0.36f), Yellow(0.95f,0.63f,0.05f), Deck(0.25f,0.27f,0.28f);
	Shape.Box(FVector(0,0,-15),FVector(220,200,15),Deck);
	// Open center is the delivery target. Harbor cradles visibly support the recovered hull.
	for (float Y : {-75.0f,75.0f}) Shape.Box(FVector(0,Y,25),FVector(110,12,25),Steel);
	for (float X : {-200.0f,200.0f}) Shape.Box(FVector(X,0,3),FVector(8,180,3),Yellow);
	if (bHarbor)
	{
		Shape.Box(FVector(-175,-155,170),FVector(15,15,170),Yellow);
		Shape.Box(FVector(-40,-155,335),FVector(150,12,12),Yellow);
		Shape.Bar(FVector(80,-155,328),FVector(80,-155,120),3,Steel);
		Shape.Box(FVector(80,-155,110),FVector(16,12,8),Yellow);
		for (float X : {-160.0f,160.0f}) for (float Y : {-155.0f,155.0f}) Shape.Box(FVector(X,Y,-100),FVector(12,12,100),Steel);
	}
	else if (Kind==ESimCopterRecoverySite::AutoRepair)
	{
		for (float Y : {-175.0f,175.0f}) Shape.Box(FVector(-170,Y,90),FVector(22,22,90),Yellow);
		Shape.Box(FVector(-170,0,175),FVector(25,195,10),Steel);
	}
	else
	{
		for (int32 K=0;K<4;++K) Shape.Box(FVector(-155+K*85,170,35),FVector(30,25,30+K*4),FLinearColor(0.35f,0.15f,0.06f));
	}
	for(auto& Vertex:Shape.V) { Vertex.X*=0.72; Vertex.Y*=0.72; }
	Shape.Apply(Mesh,true); Mesh->SetMaterial(0,Material);
	auto* Sign=NewObject<UTextRenderComponent>(this);
	AddInstanceComponent(Sign); Sign->SetupAttachment(Mesh); Sign->RegisterComponent();
	Sign->SetRelativeLocation(FVector(0,0,bHarbor?365:215));
	Sign->SetHorizontalAlignment(EHTA_Center); Sign->SetWorldSize(35);
	Sign->SetTextRenderColor(FColor::Yellow);
	Sign->SetText(FText::FromString(bHarbor?TEXT("HARBOR - SHIP REPAIR"):Kind==ESimCopterRecoverySite::AutoRepair?TEXT("AUTO REPAIR"):TEXT("JUNKYARD")));
}

USimCopterAirOperationsComponent::USimCopterAirOperationsComponent()
{
	PrimaryComponentTick.bCanEverTick=false; // Pawn orders this after movement and before the view update.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/M_SimCopterLitVertexColor.M_SimCopterLitVertexColor"));
	if (Mat.Succeeded()) Material=Mat.Object;
}
ASimCopterHelicopterPawn* USimCopterAirOperationsComponent::Helicopter() const { return Cast<ASimCopterHelicopterPawn>(GetOwner()); }
bool USimCopterAirOperationsComponent::HasCargo() const { return TowVehicle.IsValid() || BoatIndex!=INDEX_NONE || !CagePeople.IsEmpty(); }
bool USimCopterAirOperationsComponent::IsCargoToolSelected() const
{
	const auto* Heli=Helicopter();
	return Heli && (Heli->GetActiveTool()==ESimCopterHelicopterTool::TowClamp || Heli->GetActiveTool()==ESimCopterHelicopterTool::CaptureCage);
}
void USimCopterAirOperationsComponent::EnsureMesh()
{
	if (CargoMesh || !GetOwner()) return;
	CargoMesh=NewObject<UProceduralMeshComponent>(GetOwner(),TEXT("AirOperationsCable"));
	GetOwner()->AddInstanceComponent(CargoMesh); CargoMesh->SetupAttachment(GetOwner()->GetRootComponent());
	CargoMesh->SetAbsolute(true,true,true); CargoMesh->RegisterComponent(); CargoMesh->SetWorldTransform(FTransform::Identity);
	CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); CargoMesh->SetMaterial(0,Material);
	CageAnchor=NewObject<USceneComponent>(GetOwner(),TEXT("CageOccupants"));
	GetOwner()->AddInstanceComponent(CageAnchor); CageAnchor->SetupAttachment(GetOwner()->GetRootComponent());
	CageAnchor->SetAbsolute(true,true,true); CageAnchor->RegisterComponent();
}
void USimCopterAirOperationsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	EndPoliceTaser();
	if (Reason==EEndPlayReason::Destroyed) ReleaseCargo();
	Super::EndPlay(Reason);
}
bool USimCopterAirOperationsComponent::HasClearLine(const FVector& From, const FVector& To, AActor* Target) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CargoLine),true,GetOwner());
	Query.AddIgnoredActor(Target);
	// People already inside the same cage volume must not occlude one another's capture.
	if(bCage) for(TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		if(It->GetAgentKind()==ESimCopterGroundAgentKind::Pedestrian) Query.AddIgnoredActor(*It);
	FHitResult Hit;
	return GetWorld() && !GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Query);
}
void USimCopterAirOperationsComponent::UseCargoTool()
{
	if (!IsCargoToolSelected()) return;
	const bool bWantCage=Helicopter()->GetActiveTool()==ESimCopterHelicopterTool::CaptureCage;
	if (!Helicopter()->IsToolAvailable(Helicopter()->GetActiveTool())) return;
	if (HasCargo() && bCage!=bWantCage) { Status=TEXT("Unload the current sling before changing equipment."); return; }
	EnsureMesh();
	if (!bDeployed || bCage!=bWantCage)
	{
		bCage=bWantCage; bCageOpen=bCage; bDeployed=true; CableLength=140;
		CargoLocation=Helicopter()->GetCargoAnchorWorldLocation()-FVector(0,0,CableLength);
		CargoVelocity=FVector::ZeroVector;
		Status=bCage?TEXT("Cage open. Lower over people; use again to close."):TEXT("Clamp deployed. Lower onto a stalled car or capsized boat; use to grab.");
	}
	else if (HasCargo())
	{
		if (!TryDeliverCargo()) Status=TEXT("Carry cars to Auto Repair/Junkyard; boats to Harbor. G opens/releases.");
	}
	else if (!TryGrabCargo())
	{
		if (CableLength<=145) { bDeployed=false; CargoMesh->SetVisibility(false); }
		else Status=TEXT("No eligible cargo inside the clamp/cage. Move closer or lower the cable.");
	}
}
bool USimCopterAirOperationsComponent::TryGrabCargo()
{
	if (!bDeployed || !Helicopter()) return false;
	EnsureMesh();
	if (bCage)
	{
		if (!CagePeople.IsEmpty()) return false;
		CageAnchor->SetWorldLocation(CargoLocation);
		for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It && CagePeople.Num()<SimCopterAirOperations::CageCapacity; ++It)
		{
			if (It->GetAgentKind()!=ESimCopterGroundAgentKind::Pedestrian || It->IsHidden() || It->IsMissionCarried() || It->GetBehaviorCarrier() || It->IsMissionPatientDead()) continue;
			const FVector Delta=It->GetActorLocation()-CargoLocation;
			if (FMath::Abs(Delta.X)>72 || FMath::Abs(Delta.Y)>72 || FMath::Abs(Delta.Z)>65 || !HasClearLine(CargoLocation+FVector(0,0,45),It->GetActorLocation(),*It)) continue;
			if (It->IsArrestableSuspect()) It->StunForArrest();
			const int32 Seat=CagePeople.Num();
			It->SetCarriedBy(CageAnchor,FVector((Seat%2?1:-1)*28,(Seat/2?1:-1)*28,0),FRotator::ZeroRotator);
			It->SetForcedPedestrianFigureClip(TEXT("NoMo"));
			CagePeople.Add(*It);
		}
		bCageOpen=false;
		Status=FString::Printf(TEXT("Cage closed: %d occupants. Hold UP at the top to board; G opens."),CagePeople.Num());
		return !CagePeople.IsEmpty();
	}
	if (HasCargo()) return false;
	ASimCopterGroundAgent* Best=nullptr; float Distance=120;
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It)
	{
		const float D=FVector::Distance(It->GetActorLocation(),CargoLocation);
		if (It->IsTowableVehicle() && !It->IsHidden() && !It->IsVehicleTowed() && D<Distance && HasClearLine(CargoLocation,It->GetActorLocation(),*It)) { Best=*It; Distance=D; }
	}
	if (Best)
	{
		TowVehicle=Best; Best->SetVehicleTowed(true); Status=TEXT("Car clamped. Deliver gently to Auto Repair or Junkyard."); return true;
	}
	if (auto* Ambient=ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld()); Ambient && Ambient->FindTowableBoat(CargoLocation,170,BoatIndex))
	{
		if (!Ambient->SetBoatTow(BoatIndex,true,CargoLocation))
		{
			BoatIndex=INDEX_NONE; Status=TEXT("Rescue everyone from the hull before towing it."); return false;
		}
		Status=TEXT("Boat clamped. Deliver to the Harbor repair cradle."); return true;
	}
	return false;
}
void USimCopterAirOperationsComponent::OpenCargo()
{
	if (!bDeployed) return;
	if (TryDeliverCargo()) return;
	ReleaseCargo(); bCageOpen=bCage;
	Status=bCage?TEXT("Cage floor open. Occupants released."):TEXT("Tow clamp released.");
}
void USimCopterAirOperationsComponent::ReleaseCargo(bool bOpenCage)
{
	if (TowVehicle.IsValid()) TowVehicle->SetVehicleTowed(false);
	TowVehicle.Reset();
	if (BoatIndex!=INDEX_NONE)
		if (auto* Ambient=ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld())) Ambient->SetBoatTow(BoatIndex,false,CargoLocation);
	BoatIndex=INDEX_NONE;
	if (bOpenCage)
	{
		for (auto Person : CagePeople) if (Person.IsValid())
		{
			Person->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Person->SetActorHiddenInGame(false);
			Person->BeginPassengerFall(Person->MissionEventId,SimCopterAirOperations::SafeFallCm);
		}
		CagePeople.Reset(); bCageOpen=true;
	}
	BoardHoldSeconds=0;
}
bool USimCopterAirOperationsComponent::TryDeliverCargo()
{
	if (!HasCargo() || bCage || !GetWorld()) return false;
	auto* Ops=GetWorld()->GetSubsystem<USimCopterAirOperationsSubsystem>();
	auto* Site=Ops?Ops->FindRecoverySite(CargoLocation,BoatIndex!=INDEX_NONE):nullptr;
	if (!Site || FVector::Dist2D(CargoLocation,Site->GetActorLocation())>180 || FMath::Abs(CargoLocation.Z-Site->GetActorLocation().Z-70)>100 || CargoVelocity.Size()>200) return false;
	if (TowVehicle.IsValid())
	{
		TowVehicle->SetActorLocation(Site->GetActorLocation()+FVector(0,0,65));
		TowVehicle->CompleteVehicleTow(Site->GetKind()==ESimCopterRecoverySite::Junkyard); TowVehicle.Reset();
	}
	if (BoatIndex!=INDEX_NONE)
	{
		auto* Ambient=ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld());
		if (!Ambient) return false;
		Ambient->SetBoatTow(BoatIndex,true,Site->GetActorLocation()+FVector(0,0,70));
		if (!Ambient->FinishBoatTow(BoatIndex)) return false;
		BoatIndex=INDEX_NONE;
	}
	Status=TEXT("Recovery complete. Cargo accepted.");
	return true;
}
void USimCopterAirOperationsComponent::DrawCargo()
{
	if (!CargoMesh || !Helicopter()) return;
	CargoMesh->SetVisibility(bDeployed);
	if (!bDeployed) return;
	FRecoveryMesh Shape;
	const FLinearColor Steel(0.3f,0.35f,0.4f), Yellow(0.95f,0.65f,0.06f);
	const FVector Anchor=Helicopter()->GetCargoAnchorWorldLocation();
	Shape.Bar(Anchor,CargoLocation+FVector(0,0,bCage?65:15),2,Steel);
	if (bCage)
	{
		if (!bCageOpen) Shape.Box(CargoLocation-FVector(0,0,28),FVector(75,75,3),Steel);
		else Shape.Box(CargoLocation+FVector(-78,0,-55),FVector(3,75,30),Yellow);
		for (float X : {-75.0f,75.0f}) for (float Y : {-75.0f,75.0f}) Shape.Bar(CargoLocation+FVector(X,Y,-28),CargoLocation+FVector(X,Y,58),3,Yellow);
		for (int32 K=-2;K<=2;++K) for (float Side : {-75.0f,75.0f})
		{
			Shape.Bar(CargoLocation+FVector(K*25,Side,-28),CargoLocation+FVector(K*25,Side,58),1.5f,Steel);
			Shape.Bar(CargoLocation+FVector(Side,K*25,-28),CargoLocation+FVector(Side,K*25,58),1.5f,Steel);
		}
		Shape.Box(CargoLocation+FVector(0,0,60),FVector(78,78,3),Yellow);
	}
	else
	{
		Shape.Box(CargoLocation+FVector(0,0,20),FVector(32,10,7),Yellow);
		for (float X : {-32.0f,32.0f})
		{
			const float Grip=HasCargo()?0.55f:1.0f;
			Shape.Bar(CargoLocation+FVector(X,0,20),CargoLocation+FVector(X*Grip,0,-20),5,Steel);
			Shape.Box(CargoLocation+FVector(X*Grip,0,-20),FVector(10,12,4),Yellow);
		}
	}
	Shape.Apply(CargoMesh);
}
void USimCopterAirOperationsComponent::Update(float DeltaSeconds)
{
	if (!Helicopter() || !GetWorld()) return;
	ResolveSavedCargo();
	ShotCooldown=FMath::Max(0.0f,ShotCooldown-DeltaSeconds);
	if (bPoliceTaserActive) UpdatePoliceTaser(DeltaSeconds);
	else if (bSupportAircraft) AdvanceAutopilot(AutopilotGoal,DeltaSeconds);
	FinishPoliceHandoffs(DeltaSeconds);
	// A contact counts once until the helicopter separates, even if it rests on the car.
	TSet<TWeakObjectPtr<ASimCopterGroundAgent>> Contacts;
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It)
	{
		if (It->GetAgentKind()!=ESimCopterGroundAgentKind::Vehicle || It->IsHidden() || It->IsVehicleTowed()) continue;
		const float Gap=Helicopter()->GetDistanceToAirframeCm(It->GetActorLocation(),false);
		if (Gap > It->GetCollisionRadiusCm()+8) continue;
		Contacts.Add(*It);
		if (!VehicleContacts.Contains(*It) && Helicopter()->GetVelocityCmPerSec().Size()+It->GetCurrentVelocityCmPerSec().Size()>35)
			It->ApplyHelicopterVehicleImpact(It->GetActorLocation()+(Helicopter()->GetActorLocation()-It->GetActorLocation()).GetSafeNormal()*It->GetCollisionRadiusCm());
	}
	VehicleContacts=MoveTemp(Contacts);
	if (!bDeployed) return;
	EnsureMesh();
	CagePeople.RemoveAll([](auto P){return !P.IsValid();});
	CableLength=FMath::Clamp(CableLength-CableInput*360.0f*DeltaSeconds,140.0f,SimCopterAirOperations::MaxCableCm);
	const FVector Anchor=Helicopter()->GetCargoAnchorWorldLocation();
	const FVector Desired=Anchor-FVector(0,0,CableLength);
	float Remaining=FMath::Min(DeltaSeconds,0.2f);
	while (Remaining>0)
	{
		const float Dt=FMath::Min(Remaining,1.0f/60.0f); Remaining-=Dt;
		CargoVelocity+=((Desired-CargoLocation)*12.0f-CargoVelocity*4.5f)*Dt;
		const FVector Next=CargoLocation+CargoVelocity*Dt;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SlingSweep),true,GetOwner());
		Query.AddIgnoredActor(TowVehicle.Get()); for (auto P:CagePeople) Query.AddIgnoredActor(P.Get());
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit,CargoLocation,Next,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(bCage?30:12),Query))
		{
			CargoLocation=Hit.Location+Hit.Normal*2; CargoVelocity=FVector::VectorPlaneProject(CargoVelocity,Hit.Normal)*0.4f;
		}
		else CargoLocation=Next;
		const FVector Offset=CargoLocation-Anchor;
		if (Offset.Size()>CableLength+80) CargoLocation=Anchor+Offset.GetSafeNormal()*(CableLength+80);
	}
	CageAnchor->SetWorldLocation(CargoLocation);
	if (TowVehicle.IsValid()) TowVehicle->SetActorLocation(CargoLocation-FVector(0,0,25));
	if (BoatIndex!=INDEX_NONE)
		if (auto* Ambient=ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld()); !Ambient || !Ambient->SetBoatTow(BoatIndex,true,CargoLocation)) BoatIndex=INDEX_NONE;
	if (bCage && !CagePeople.IsEmpty() && CableLength<=141 && CableInput>0.5f)
	{
		BoardHoldSeconds+=DeltaSeconds;
		if (BoardHoldSeconds>=SimCopterAirOperations::CageBoardHoldSeconds)
		{
			for (int32 K=CagePeople.Num()-1;K>=0;--K)
				if (CagePeople[K].IsValid() && CagePeople[K]->BoardCarrier(Helicopter(),false,true)) CagePeople.RemoveAt(K);
			Status=CagePeople.IsEmpty()?TEXT("Cage occupants aboard. Criminals handcuffed."):TEXT("Cabin full. Remaining occupants stay in the cage.");
			BoardHoldSeconds=0;
		}
	}
	else BoardHoldSeconds=0;
	TryDeliverCargo(); DrawCargo();
	if (!Status.IsEmpty()) Helicopter()->SetAirOperationsStatus(Status);
}

bool USimCopterAirOperationsComponent::HasPolicePassenger() const
{
	if (const auto* Heli=Helicopter()) for (const auto& Slot:Heli->GetMissionPassengerSlots())
		if (auto* P=Slot.Person.Get(); P && !P->IsMissionPatientDead() && !P->IsHandcuffed() &&
			(P->InitialPersonState==7 || P->InitialPersonState==8 || P->GetBehaviorAttribute(EBhavAttr::State)==7 || P->GetBehaviorAttribute(EBhavAttr::State)==8)) return true;
	return false;
}
void USimCopterAirOperationsComponent::TogglePoliceTaser()
{
	if (bPoliceTaserActive) { EndPoliceTaser(); return; }
	auto* Heli=Helicopter();
	if (!Heli || !HasPolicePassenger() || Heli->CanTransferMissionPassengers()) { Status=TEXT("Police taser needs a living police passenger and an airborne helicopter."); if(Heli) Heli->SetAirOperationsStatus(Status); return; }
	auto* PC=Cast<APlayerController>(Heli->GetController()); if (!PC) return;
	FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
	PoliceTaserFocus=Eye+View.Vector()*2500;
	float Nearest=FMath::Square(4500.0f);
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		if (It->IsArrestableSuspect() && !It->IsHidden() && !It->IsInPoliceCustody())
		{
			const float D=FVector::DistSquared(It->GetActorLocation(),Heli->GetActorLocation());
			if(D<Nearest) { Nearest=D; PoliceTaserFocus=It->GetActorLocation(); }
		}
	PoliceTaserCamera=GetWorld()->SpawnActor<ACameraActor>(); if (!PoliceTaserCamera) return;
	PoliceTaserAim=(PoliceTaserFocus-Heli->GetPassengerDropWorldLocation()).Rotation();
	PoliceTaserCamera->GetCameraComponent()->SetFieldOfView(28);
	bPoliceTaserActive=true; PC->SetViewTarget(PoliceTaserCamera); PC->bShowMouseCursor=false;
	Status=TEXT("POLICE TASER | AI pilot orbiting | mouse/right stick aim | click/X/RT fire | N or R3+Y exit");
	Heli->SetAirOperationsStatus(Status);
}
void USimCopterAirOperationsComponent::EndPoliceTaser()
{
	if (bPoliceTaserActive) if (auto* Heli=Helicopter()) if (auto* PC=Cast<APlayerController>(Heli->GetController())) { PC->SetViewTarget(Heli); PC->bShowMouseCursor=true; }
	bPoliceTaserActive=false;
	if (PoliceTaserCamera) PoliceTaserCamera->Destroy(); PoliceTaserCamera=nullptr;
}
void USimCopterAirOperationsComponent::AimPoliceTaser(float Yaw,float Pitch,float DeltaSeconds)
{
	if (!bPoliceTaserActive) return;
	PoliceTaserAim.Yaw+=Yaw*45.0f*DeltaSeconds;
	PoliceTaserAim.Pitch=FMath::Clamp(PoliceTaserAim.Pitch+Pitch*35.0f*DeltaSeconds,-85.0f,45.0f);
}
void USimCopterAirOperationsComponent::UpdatePoliceTaser(float DeltaSeconds)
{
	if (!HasPolicePassenger() || !Helicopter()->IsPlayerControlled()) { EndPoliceTaser(); return; }
	FVector Offset=Helicopter()->GetActorLocation()-PoliceTaserFocus;
	FVector Radial=Offset.GetSafeNormal2D(); if (Radial.IsNearlyZero()) Radial=FVector::ForwardVector;
	const FVector Tangent=FVector::CrossProduct(FVector::UpVector,Radial);
	FVector Goal=PoliceTaserFocus+Radial*800+Tangent*120+FVector(0,0,650);
	AdvanceAutopilot(Goal,DeltaSeconds,350);
	const FVector Seat=Helicopter()->GetPassengerDropWorldLocation()+FVector(0,0,55);
	if (PoliceTaserCamera) PoliceTaserCamera->SetActorLocationAndRotation(Seat,PoliceTaserAim);
}
void USimCopterAirOperationsComponent::FirePoliceTaser()
{
	if (!bPoliceTaserActive || !PoliceTaserCamera || ShotCooldown>0) return;
	if (!HasPolicePassenger() || Helicopter()->CanTransferMissionPassengers()) { EndPoliceTaser(); return; }
	ShotCooldown=1.25f;
	FVector End;
	auto* Person=SimCopterAirOperations::TracePerson(GetWorld(),Helicopter(),PoliceTaserCamera->GetActorLocation(),PoliceTaserAim.Vector(),6000,End);
	// Remake-only nonlethal taser. Never dispatch an Apache projectile, damage, or death interaction.
	const bool Hit=Person && Person->StunForArrest();
	Status=Hit?TEXT("TASER: suspect stunned alive. Retrieve, handcuff and deliver to police."):TEXT("TASER: no active suspect hit. Recharging.");
	Helicopter()->SetAirOperationsStatus(Status);
	SimCopterAirOperations::DrawTaserDischarge(Helicopter()->FindComponentByClass<USimCopterParticleFXComponent>(), PoliceTaserCamera->GetActorLocation(), End);
}
bool USimCopterAirOperationsComponent::AdvanceAutopilot(const FVector& Goal,float DeltaSeconds,float Speed)
{
	auto* Heli=Helicopter(); if (!Heli || DeltaSeconds<=0) return false;
	FVector Target=Goal;
	const FVector From=Heli->GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AirSupportNavigation),true,Heli);
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
	FHitResult Hit;
	const FVector Horizontal=(Goal-From).GetSafeNormal2D();
	// Follow a clear altitude corridor; never fly straight through a facade on the way to a job.
	if (GetWorld()->SweepSingleByChannel(Hit,From,From+Horizontal*FMath::Min(1000.0f,float(FVector::Dist2D(From,Goal))),FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(100),Query))
		Target=From+FVector(0,0,600);
	else if (FVector::Dist2D(From,Goal)>500) Target.Z=FMath::Max(From.Z,Goal.Z);
	const FVector Next=FMath::VInterpConstantTo(From,Target,DeltaSeconds,Speed);
	if (GetWorld()->SweepSingleByChannel(Hit,From,Next,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(80),Query))
		return false;
	FRotator Facing=Heli->GetActorRotation();
	if (!Horizontal.IsNearlyZero()) Facing.Yaw=FMath::FInterpTo(Facing.Yaw,Horizontal.Rotation().Yaw,DeltaSeconds,2.0f);
	Heli->SetAutopilotTransform(Next,Facing,DeltaSeconds);
	return FVector::Distance(Next,Goal)<80;
}

void USimCopterAirOperationsComponent::FinishPoliceHandoffs(float DeltaSeconds)
{
	auto* Heli=Helicopter();
	if (!Heli || !Heli->GetMissionPassengerSlots().ContainsByPredicate([](const auto& S){ return S.Person.IsValid() && S.Person->IsHandcuffed() && !S.Person->IsMedevacVictim(); })) { PoliceHandoffSeconds=0; return; }
	auto* Ops=GetWorld()->GetSubsystem<USimCopterAirOperationsSubsystem>();
	FVector Roof;
	if (!Ops || !Ops->FindServiceRoof(0xD2,Heli->GetActorLocation(),Roof) || FVector::Dist2D(Roof,Heli->GetActorLocation())>300 ||
		FMath::Abs(Heli->GetPassengerDropWorldLocation().Z-Roof.Z)>110 || Heli->GetVelocityCmPerSec().Size()>100)
	{ PoliceHandoffSeconds=0; return; }
	ASimCopterGroundAgent* Officer=nullptr;
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		if (!It->IsMissionPatientDead() && !It->IsHidden() && !It->GetBehaviorCarrier() && !It->IsMissionCarried() &&
			(It->InitialPersonState==7 || It->GetBehaviorAttribute(EBhavAttr::State)==7) && FVector::Dist2D(It->GetActorLocation(),Roof)<400 && FMath::Abs(It->GetActorLocation().Z-Roof.Z)<100)
		{ Officer=*It; break; }
	if (!Officer)
	{
		int32 X=0,Y=0;
		if(auto* Road=Ops->Traffic(); Road && Road->TryGetPeopleTileCoordinateAtWorldLocation(Roof,X,Y))
			Road->TrySpawnOriginalPersonAtTile(X,Y,0x0e,7,INDEX_NONE,nullptr,INDEX_NONE,true,true);
		PoliceHandoffSeconds=0; return;
	}
	Officer->SetMissionScriptedMover();
	PoliceHandoffSeconds+=DeltaSeconds;
	if (PoliceHandoffSeconds<1.5f) return;
	const auto Slots=Heli->GetMissionPassengerSlots();
	for (const auto& Slot:Slots) if (auto* Prisoner=Slot.Person.Get(); Prisoner && Prisoner->IsHandcuffed() && !Prisoner->IsMedevacVictim())
	{
		const FVector Feet = Officer->GetActorLocation()-FVector(0,0,Officer->GetCapsuleHalfHeightCm())+FVector(35,0,0);
		bool bOccupied = false;
		for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
			if (*It != Prisoner && !It->IsHidden() && !It->IsMissionCarried() && !It->GetBehaviorCarrier() &&
				FVector::DistSquared2D(It->GetActorLocation(),Feet)<FMath::Square(26.0f) &&
				FMath::Abs(It->GetActorLocation().Z-It->GetCapsuleHalfHeightCm()-Feet.Z)<30)
			{ bOccupied = true; break; }
		if (bOccupied) break; // The previous prisoner must clear the officer's handoff point first.
		Prisoner->CompletePoliceDelivery(Feet);
		Heli->RefreshPassengerDisplay();
		Status=TEXT("Roof officer accepted prisoner into custody."); Heli->SetAirOperationsStatus(Status);
		break;
	}
	PoliceHandoffSeconds=0; Officer->ResumeSuspendedPedestrianBehavior();
}

void USimCopterAirOperationsComponent::SerializeState(FArchive& Ar)
{
	Ar << bSupportAircraft << bDeployed << bCage << bCageOpen << CableLength << CargoLocation << CargoVelocity << BoatIndex << BoardHoldSeconds << AutopilotGoal;
	TArray<FName> Names;
	if (Ar.IsSaving())
	{
		Names.Add(TowVehicle.IsValid()?TowVehicle->GetRuntimeSaveIdentityName():NAME_None);
		for(auto P:CagePeople) if(P.IsValid()) Names.Add(P->GetRuntimeSaveIdentityName());
	}
	Ar << Names;
	if (Ar.IsLoading())
	{
		if (Names.Num()>SimCopterAirOperations::CageCapacity+1 || !FMath::IsFinite(CableLength) || CargoLocation.ContainsNaN() || CargoVelocity.ContainsNaN()) { Ar.SetError(); return; }
		CagePeople.Reset(); TowVehicle.Reset();
		SavedCargoNames=MoveTemp(Names); bSavedCargoPending=true;
		CableLength=FMath::Clamp(CableLength,140.0f,SimCopterAirOperations::MaxCableCm);
		CableInput=0; EndPoliceTaser();
	}
}
void USimCopterAirOperationsComponent::ResolveSavedCargo()
{
	if (!bSavedCargoPending || !GetWorld()) return;
	// Traffic restore happens after aircraft restore. Resolve on the first simulation tick.
	bSavedCargoPending=false; EnsureMesh(); CageAnchor->SetWorldLocation(CargoLocation);
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
	{
		const int32 Index=SavedCargoNames.IndexOfByKey(It->GetRuntimeSaveIdentityName());
		if (Index==0 && It->IsTowableVehicle()) { TowVehicle=*It; It->SetVehicleTowed(true); }
		else if (Index>0 && bCage)
		{
			const int32 Slot=CagePeople.Num();
			It->SetCarriedBy(CageAnchor,FVector((Slot%2?1:-1)*28,(Slot/2?1:-1)*28,0),FRotator::ZeroRotator);
			It->SetForcedPedestrianFigureClip(TEXT("NoMo")); CagePeople.Add(*It);
		}
	}
	SavedCargoNames.Reset(); DrawCargo();
}

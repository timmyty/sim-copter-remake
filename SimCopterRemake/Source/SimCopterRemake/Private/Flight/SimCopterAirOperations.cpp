#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Ground/SimCopterParticleFX.h"
#include "Game/SimCopterPlayerController.h"
#include "Game/SimCopterSettings.h"
#include "Engine/LocalPlayer.h"
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

bool IsRemotePursuitTarget(const ASimCopterGroundAgent* Agent)
{
	if (!IsValid(Agent) || Agent->IsActorBeingDestroyed() || Agent->IsHidden() || Agent->HasMissionResolutionReported() ||
		Agent->IsMissionCarried() || Agent->GetBehaviorCarrier() || Agent->IsInPoliceCustody()) return false;
	if (Agent->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle)
		return (Agent->IsCriminalCar() || Agent->IsFleeing()) && !Agent->HasVehicleExploded() && !Agent->IsVehicleTowed();
	return Agent->IsArrestableSuspect();
}

FVector RemotePursuitFocus(const ASimCopterGroundAgent* Agent)
{
	FBox Body(ForceInit);
	if (Agent->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle && Agent->TryGetBodyLocalBoundsCm(Body))
		return Agent->GetActorTransform().TransformPosition(Body.GetCenter());
	return Agent->GetActorLocation();
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
ASimCopterGroundAgent* TraceRemoteTaserTarget(UWorld* World, AActor* Source, const FVector& Start, const FVector& Direction, float Range, FVector& OutEnd)
{
	auto* Best = TracePerson(World, Source, Start, Direction, Range, OutEnd);
	if (!World) return Best;
	// Vehicle render meshes have no query collision. Test their oriented body bounds too,
	// clipped to the first person/world hit so the discharge cannot pass through obstacles.
	for (TActorIterator<ASimCopterGroundAgent> It(World); It; ++It)
	{
		if (It->GetAgentKind() != ESimCopterGroundAgentKind::Vehicle || It->IsHidden() ||
			It->IsActorBeingDestroyed() || *It == Source) continue;
		FBox Body(ForceInit);
		if (!It->TryGetBodyLocalBoundsCm(Body)) continue;
		const FTransform Transform = It->GetActorTransform();
		FVector HitLocation, HitNormal;
		float HitTime = 0.0f;
		if (FMath::LineExtentBoxIntersection(Body, Transform.InverseTransformPosition(Start),
			Transform.InverseTransformPosition(OutEnd), FVector::ZeroVector, HitLocation, HitNormal, HitTime))
		{
			OutEnd = Transform.TransformPosition(HitLocation);
			Best = *It;
		}
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
	EndRemoteTaser();
	EndPilotRecovery();
	if (Reason==EEndPlayReason::Destroyed) ReleaseCargo();
	Super::EndPlay(Reason);
}

void USimCopterAirOperationsComponent::SetCagePanelInput(float Direction)
{
	CagePanelInput = 0;
	if (Direction == 0 || !Helicopter() || !Helicopter()->IsToolAvailable(ESimCopterHelicopterTool::CaptureCage)) return;
	Helicopter()->SetSelectedTool(ESimCopterHelicopterTool::CaptureCage);
	if (Direction < 0 && (!bDeployed || !bCage)) UseCargoTool();
	if (bDeployed && bCage) CagePanelInput = FMath::Clamp(Direction, -1.0f, 1.0f);
}

FString USimCopterAirOperationsComponent::GetCageReadout() const
{
	if (!bDeployed || !bCage) return TEXT("STOWED");
	if (CageFormation < 1) return TEXT("FORMING");
	if (bCageOpen) return TEXT("OPEN / AUTO");
	if (CageSeal < 1) return TEXT("SEALING");
	return FString::Printf(TEXT("SEALED %d / 4"), CagePeople.Num());
}

void USimCopterAirOperationsComponent::UpdateCageMount()
{
	const bool bFitted = Helicopter()->IsToolAvailable(ESimCopterHelicopterTool::CaptureCage);
	if (bFitted && !CageMountMesh)
	{
		CageMountMesh = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("CaptureCageSkidEmitters"));
		GetOwner()->AddInstanceComponent(CageMountMesh);
		CageMountMesh->SetupAttachment(GetOwner()->GetRootComponent());
		CageMountMesh->SetAbsolute(true, true, true);
		CageMountMesh->RegisterComponent();
		CageMountMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CageMountMesh->SetMaterial(0, Material);
		FRecoveryMesh Shape;
		const FLinearColor Steel(0.3f,0.35f,0.4f), Yellow(0.95f,0.65f,0.06f);
		for (float End : {-75.0f, 75.0f}) Shape.Box(FVector(End,0,0), FVector(5,75,5), Steel);
		for (float Side : {-75.0f, 75.0f})
		{
			Shape.Box(FVector(0,Side,0), FVector(80,6,5), Steel);
			for (int32 K=-3; K<=3; ++K)
			{
				Shape.Box(FVector(K*25,Side,0), FVector(5,8,6), Yellow);
				Shape.Bar(FVector(K*25,Side,0), FVector(K*25,Side,-8), 2, Steel);
			}
		}
		Shape.Apply(CageMountMesh);
	}
	if (CageMountMesh)
	{
		CageMountMesh->SetVisibility(bFitted);
		CageMountMesh->SetWorldTransform(Helicopter()->GetCaptureCageMountTransform());
	}
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
		CageFormation=0; CageSeal=0; CageCaptureDelay=0;
		CargoLocation=(bCage ? Helicopter()->GetCaptureCageMountTransform().GetLocation() : Helicopter()->GetCargoAnchorWorldLocation())-FVector(0,0,CableLength);
		CargoVelocity=FVector::ZeroVector;
		Status=bCage?TEXT("Cage forming from skids. Lower over people; the floor seals automatically."):TEXT("Clamp deployed. Lower onto a stalled car or capsized boat; use to grab.");
	}
	else if (HasCargo())
	{
		if (!TryDeliverCargo()) Status=bCage?TEXT("Raise and hold at the top to board. Deliver prisoners to the police roof. G opens the floor."):TEXT("Carry cars to Auto Repair/Junkyard; boats to Harbor. G opens/releases.");
	}
	else if (!TryGrabCargo())
	{
		if (CableLength<=145) { bDeployed=false; }
		else Status=TEXT("No eligible cargo inside the clamp/cage. Move closer or lower the cable.");
	}
}
bool USimCopterAirOperationsComponent::TryGrabCargo()
{
	if (!bDeployed || !Helicopter()) return false;
	EnsureMesh();
	if (bCage)
	{
		if (!bCageOpen || !CagePeople.IsEmpty()) return false;
		CageAnchor->SetWorldLocation(CargoLocation);
		for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It && CagePeople.Num()<SimCopterAirOperations::CageCapacity; ++It)
		{
			if (It->GetAgentKind()!=ESimCopterGroundAgentKind::Pedestrian || It->IsHidden() || It->IsMissionCarried() || It->GetBehaviorCarrier() || It->IsMissionPatientDead()) continue;
			const FVector Delta=FRotator(0,Helicopter()->GetActorRotation().Yaw,0).UnrotateVector(It->GetActorLocation()-CargoLocation);
			if (FMath::Abs(Delta.X)>72 || FMath::Abs(Delta.Y)>72 || FMath::Abs(Delta.Z)>65 || !HasClearLine(CargoLocation+FVector(0,0,45),It->GetActorLocation(),*It)) continue;
			if (It->IsArrestableSuspect()) It->StunForArrest();
			const int32 Seat=CagePeople.Num();
			It->SetCarriedBy(CageAnchor,FVector((Seat%2?1:-1)*28,(Seat/2?1:-1)*28,0),FRotator::ZeroRotator);
			It->SetForcedPedestrianFigureClip(TEXT("NoMo"));
			CagePeople.Add(*It);
		}
		if (CagePeople.IsEmpty()) return false;
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
		// Opening must give released people time to fall clear of the automatic capture volume.
		CageCaptureDelay=1.5f;
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
	CargoMesh->SetVisibility(bDeployed || (bCage && CageFormation > 0));
	if (!bDeployed && (!bCage || CageFormation <= 0)) return;
	FRecoveryMesh Shape;
	const FLinearColor Steel(0.3f,0.35f,0.4f), Yellow(0.95f,0.65f,0.06f);
	const FVector Anchor=Helicopter()->GetCargoAnchorWorldLocation();
	if (bCage)
	{
		const FTransform Mount = Helicopter()->GetCaptureCageMountTransform();
		const FQuat Yaw = FRotator(0, Helicopter()->GetActorRotation().Yaw, 0).Quaternion();
		auto FloorPoint = [&](float X, float Y)
		{
			return FMath::Lerp(Mount.TransformPosition(FVector(X,Y,-8)),
				CargoLocation + Yaw.RotateVector(FVector(X,Y,-28)), double(CageFormation));
		};
		// Telescoping bars emerge directly from the skid rails, never a cable-hung box.
		for (int32 K=-3; K<=3; ++K) for (float Side : {-75.0f,75.0f})
		{
			Shape.Bar(Mount.TransformPosition(FVector(K*25,Side,-8)), FloorPoint(K*25,Side), 2, Steel);
			Shape.Bar(Mount.TransformPosition(FVector(Side,K*25,-8)), FloorPoint(Side,K*25), 2, Steel);
			if (CageSeal > 0)
				Shape.Bar(FloorPoint(K*25,Side), FloorPoint(K*25,Side*(1-CageSeal)), 2.5f, Steel);
		}
		for (float Side : {-75.0f,75.0f})
		{
			Shape.Bar(FloorPoint(-75,Side), FloorPoint(75,Side), 3, Yellow);
			Shape.Bar(FloorPoint(Side,-75), FloorPoint(Side,75), 3, Yellow);
		}
	}
	else
	{
		Shape.Bar(Anchor,CargoLocation+FVector(0,0,15),2,Steel);
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
	// Saves retain the on-foot pilot's parked aircraft and the aircraft's engine state.
	// Reconnect that real pair after loading instead of persisting a transient camera or pointer.
	if (!IsAIPiloted() && !Cast<APlayerController>(Helicopter()->GetController()) && Helicopter()->IsEngineRunning() && !Helicopter()->bIsLanded && !Helicopter()->IsFixedWingAircraft())
		for (TActorIterator<ASimCopterOnFootPawn> It(GetWorld());It;++It)
			if (Cast<APlayerController>(It->GetController()) && It->GetParkedHelicopter()==Helicopter()) { BeginPilotRecovery(*It); break; }
	ResolveSavedCargo();
	UpdateCageMount();
	UpdateRemoteTaserMount();
	CageCaptureDelay=FMath::Max(0.0f,CageCaptureDelay-DeltaSeconds);
	if (!IsCargoToolSelected()) CagePanelInput=0;
	// The same LOWER action deploys the cage from its skid mounts on mouse, keys and gamepad.
	if (CableInput < -0.5f && Helicopter()->GetActiveTool()==ESimCopterHelicopterTool::CaptureCage && (!bDeployed || !bCage)) UseCargoTool();
	CageFormation=FMath::FInterpConstantTo(CageFormation,bDeployed && bCage?1.0f:0.0f,DeltaSeconds,2.5f);
	CageSeal=FMath::FInterpConstantTo(CageSeal,bCageOpen?0.0f:1.0f,DeltaSeconds,3.0f);
	ShotCooldown=FMath::Max(0.0f,ShotCooldown-DeltaSeconds);
	if (bRemoteTaserActive) UpdateRemoteTaser(DeltaSeconds);
	else if (IsRecoveringPilot()) UpdatePilotRecovery(DeltaSeconds);
	// Requested helper pacing; remote pursuit and pilot recovery retain their own speeds.
	else if (bSupportAircraft) AdvanceAutopilot(AutopilotGoal,DeltaSeconds,600.0f);
	FinishPoliceHandoffs(DeltaSeconds);
	// A contact counts once until the helicopter separates, even if it rests on the car.
	TSet<TWeakObjectPtr<ASimCopterGroundAgent>> Contacts;
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); !IsAIPiloted() && It; ++It)
	{
		if (It->GetAgentKind()!=ESimCopterGroundAgentKind::Vehicle || It->IsHidden() || It->IsVehicleTowed()) continue;
		const float Gap=Helicopter()->GetDistanceToAirframeCm(It->GetActorLocation(),false);
		if (Gap > It->GetCollisionRadiusCm()+8) continue;
		Contacts.Add(*It);
		if (!VehicleContacts.Contains(*It) && Helicopter()->GetVelocityCmPerSec().Size()+It->GetCurrentVelocityCmPerSec().Size()>35)
			It->ApplyHelicopterVehicleImpact(It->GetActorLocation()+(Helicopter()->GetActorLocation()-It->GetActorLocation()).GetSafeNormal()*It->GetCollisionRadiusCm());
	}
	VehicleContacts=MoveTemp(Contacts);
	if (!bDeployed) { DrawCargo(); return; }
	EnsureMesh();
	CagePeople.RemoveAll([](auto P){return !P.IsValid();});
	const float MotionInput = bCage && HasCargo() && CageSeal < 1 ? 0.0f : CableInput;
	CableLength=FMath::Clamp(CableLength-MotionInput*360.0f*DeltaSeconds,140.0f,SimCopterAirOperations::MaxCableCm);
	const FVector Anchor=bCage ? Helicopter()->GetCaptureCageMountTransform().GetLocation() : Helicopter()->GetCargoAnchorWorldLocation();
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
	CageAnchor->SetWorldRotation(FRotator(0,Helicopter()->GetActorRotation().Yaw,0));
	if (bCage && bCageOpen && CageFormation >= 1 && CageSeal <= 0 && CageCaptureDelay <= 0) TryGrabCargo();
	if (TowVehicle.IsValid()) TowVehicle->SetActorLocation(CargoLocation-FVector(0,0,25));
	if (BoatIndex!=INDEX_NONE)
		if (auto* Ambient=ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld()); !Ambient || !Ambient->SetBoatTow(BoatIndex,true,CargoLocation)) BoatIndex=INDEX_NONE;
	if (bCage && !CagePeople.IsEmpty() && CageSeal>=1 && CableLength<=141 && CableInput>0.5f)
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
	if (bCage && CagePeople.IsEmpty() && CableLength<=141 && CableInput>0.5f)
	{
		bDeployed=false;
		Status=TEXT("Capture cage stowed on skids.");
	}
	TryDeliverCargo(); DrawCargo();
	if (!Status.IsEmpty()) Helicopter()->SetAirOperationsStatus(Status);
}

// Remake-only equipment: a small two-electrode gimbal on the nose of every helicopter.
// Bounds are in ModelPivot coordinates, so this follows each fleet model and its bank.
void USimCopterAirOperationsComponent::UpdateRemoteTaserMount()
{
	auto* Heli = Helicopter();
	if (!Heli) return;
	if (Heli->IsFixedWingAircraft())
	{
		if (RemoteTaserMount) RemoteTaserMount->SetVisibility(false, true);
		return;
	}
	if (!RemoteTaserMount)
	{
		const auto Mesh = [&](const TCHAR* Name, USceneComponent* Parent)
		{
			auto* Part = NewObject<UProceduralMeshComponent>(Heli, Name);
			Heli->AddInstanceComponent(Part); Part->SetupAttachment(Parent);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false); Part->RegisterComponent(); Part->SetMaterial(0, Material);
			return Part;
		};
		RemoteTaserMount = Mesh(TEXT("RemoteTaserMount"), Heli->ModelPivot);
		RemoteTaserWeapon = Mesh(TEXT("RemoteTaserWeapon"), RemoteTaserMount);
		const FLinearColor Steel(.16f,.19f,.22f), Yellow(.95f,.63f,.04f), Dark(.025f,.035f,.045f), Blue(.08f,.65f,.9f);
		FRecoveryMesh Mount;
		Mount.Box(FVector(-6,0,9), FVector(12,9,3), Steel);
		for (float Y : {-7.f,7.f}) Mount.Bar(FVector(-4,Y,8),FVector(0,Y,0),2,Steel);
		Mount.Apply(RemoteTaserMount);
		FRecoveryMesh Gun;
		Gun.Box(FVector(5,0,0),FVector(12,5,4),Dark);
		Gun.Box(FVector(7,0,1),FVector(9,5.5f,2),Yellow);
		Gun.Box(FVector(18,0,0),FVector(2,5,4),Steel);
		for (float Y : {-3.f,3.f})
		{
			Gun.Bar(FVector(18,Y,0),FVector(27,Y,0),1.2f,Steel);
			Gun.Box(FVector(27,Y,0),FVector(1,1.5f,1.5f),Blue);
		}
		Gun.Box(FVector(11,0,5),FVector(4,3,1.5f),Dark);
		Gun.Box(FVector(15,0,5),FVector(.5f,2,1),Blue);
		Gun.Apply(RemoteTaserWeapon);
	}
	FBox Bounds;
	if (!Heli->TryGetAirframeLocalBoundsCm(Bounds)) Bounds=FBox(FVector(-80,-55,-50),FVector(80,55,40));
	RemoteTaserMount->SetRelativeLocation(FVector(Bounds.Max.X+1,0,Bounds.Min.Z+Bounds.GetSize().Z*.25f));
	RemoteTaserMount->SetRelativeScale3D(FVector(.65f));
	RemoteTaserMount->SetVisibility(true,true);
	if (bRemoteTaserActive) RemoteTaserWeapon->SetWorldRotation(RemoteTaserAim);
	else RemoteTaserWeapon->SetRelativeRotation(FRotator(-8,0,0));
}
FVector USimCopterAirOperationsComponent::GetRemoteTaserMuzzle() const
{
	return RemoteTaserWeapon ? RemoteTaserWeapon->GetComponentTransform().TransformPosition(FVector(28,0,0)) : Helicopter()->GetActorLocation();
}
FString USimCopterAirOperationsComponent::GetRemoteTaserReadout() const
{
	if (!bRemoteTaserActive) return TEXT("N / R3+Y");
	const TCHAR* Target = !RemoteTaserTarget.IsValid() ? TEXT("SEARCHING") :
		(RemoteTaserTarget->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle ? TEXT("CAR") : TEXT("SUSPECT"));
	return FString::Printf(TEXT("AUTO ORBIT | %s | %s"),Target,ShotCooldown>0 ? TEXT("RECHARGING") : TEXT("READY"));
}
void USimCopterAirOperationsComponent::ToggleRemoteTaser()
{
	if (bRemoteTaserActive) { EndRemoteTaser(); return; }
	auto* Heli=Helicopter();
	if (!Heli || Heli->IsFixedWingAircraft() || Heli->bIsLanded || Heli->FlightModel.State==ESimCopterFlightState::Dying)
	{ Status=TEXT("Take off to use the remote taser."); if(Heli) Heli->SetAirOperationsStatus(Status); return; }
	auto* PC=Cast<APlayerController>(Heli->GetController()); if (!PC) return;
	Heli->ResetTransientInputState();
	UpdateRemoteTaserMount();
	FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
	RemoteTaserFocus=Eye+View.Vector()*2500;
	RemoteTaserTarget.Reset(); RemoteAimOffset=FVector2D::ZeroVector;
	RemoteTaserAim=View;
	RemoteTargetSearchSeconds=RemotePursuitPlanSeconds=0;
	SelectRemoteTaserTarget(true);
	RemoteTaserCamera=GetWorld()->SpawnActor<ACameraActor>(); if (!RemoteTaserCamera) return;
	RemoteTaserAim=(RemoteTaserFocus-GetRemoteTaserMuzzle()).Rotation();
	RemoteTaserCamera->GetCameraComponent()->SetFieldOfView(45);
	bRemoteTaserActive=true;
	UpdateRemoteTaserMount();
	RemoteTaserCamera->SetActorLocationAndRotation(GetRemoteTaserMuzzle(),RemoteTaserAim);
	// A full-turn gimbal must not look into its own fuselage when aiming behind the nose.
	PC->HiddenActors.AddUnique(Heli);
	PC->SetViewTarget(RemoteTaserCamera);
	SetRemoteTaserCursor(false);
	Status=TEXT("REMOTE TASER | AUTO ORBIT follows cars and suspects | mouse / right stick aim | click / X / RT fire | hold RMB for cockpit | N / R3+Y exits");
	Heli->SetAirOperationsStatus(Status);
}
void USimCopterAirOperationsComponent::EndRemoteTaser()
{
	if (bRemoteTaserActive) if (auto* Heli=Helicopter()) if (auto* PC=Cast<APlayerController>(Heli->GetController()))
	{
		PC->SetViewTarget(Heli); PC->HiddenActors.Remove(Heli); SetRemoteTaserCursor(true);
	}
	bRemoteTaserActive=false;
	if (RemoteTaserCamera) RemoteTaserCamera->Destroy(); RemoteTaserCamera=nullptr;
	RemoteTaserTarget.Reset();
	if (auto* Heli=Helicopter()) { Heli->ResetTransientInputState(); Heli->RefreshCrosshairVisibility(); }
}
void USimCopterAirOperationsComponent::AimRemoteTaser(float Yaw,float Pitch,float DeltaSeconds)
{
	if (!bRemoteTaserActive) return;
	OffsetRemoteTaserAim(Yaw*45.0f*DeltaSeconds,Pitch*35.0f*DeltaSeconds);
}
void USimCopterAirOperationsComponent::OffsetRemoteTaserAim(float YawDegrees,float PitchDegrees)
{
	RemoteAimOffset.X=FRotator::NormalizeAxis(RemoteAimOffset.X+YawDegrees);
	// Clamp the actual sight pitch, not a fixed offset from the downward orbit view.
	// This reaches the same up/down limits from any orbit altitude without input accumulating at the stops.
	const float BasePitch=(RemoteTaserFocus-RemoteTaserMount->GetComponentLocation()).Rotation().Pitch;
	RemoteAimOffset.Y=FMath::Clamp(BasePitch+RemoteAimOffset.Y+PitchDegrees,-85.0f,85.0f)-BasePitch;
}
void USimCopterAirOperationsComponent::SetRemoteTaserCursor(bool bVisible)
{
	auto* PC=Cast<APlayerController>(Helicopter()->GetController()); if (!PC) return;
	PC->bShowMouseCursor=bVisible;
	if (!PC->GetLocalPlayer() || !PC->GetLocalPlayer()->ViewportClient) return;
	if (bVisible)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); PC->SetInputMode(Mode);
	}
	Helicopter()->RestoreGameViewportFocus();
}
void USimCopterAirOperationsComponent::UpdateRemoteTaserControls(float MouseYaw,float MousePitch,float StickYaw,float StickPitch,float DeltaSeconds)
{
	if (!bRemoteTaserActive) return;
	auto* PC=Cast<APlayerController>(Helicopter()->GetController()); if (!PC || PC->IsPaused()) return;
	const auto* GamePC=Cast<ASimCopterPlayerController>(PC);
	if (Helicopter()->IsCheckupMenuOpen() || (GamePC && (GamePC->IsSettingsOpen() || GamePC->IsReplayPanelOpen()))) return;
	const bool bCursor=PC->IsInputKeyDown(EKeys::RightMouseButton);
	if (PC->bShowMouseCursor!=bCursor) { SetRemoteTaserCursor(bCursor); MouseYaw=MousePitch=0; }
	const auto* Settings=USimCopterSettings::Get(this);
	// Mouse axes are per-frame travel, whereas a held stick is a rate. Scaling both by dt
	// made mouse aim vary with frame rate and required dragging a cursor across the screen.
	if (!bCursor) OffsetRemoteTaserAim(MouseYaw*2.5f*(Settings ? Settings->GetMouseSensitivityX() : 1),
		-MousePitch*2.5f*(Settings ? Settings->GetMouseSensitivityY() : 1));
	AimRemoteTaser(StickYaw*(Settings ? Settings->GetControllerSensitivityX() : 1),
		-StickPitch*(Settings ? Settings->GetControllerSensitivityY() : 1),DeltaSeconds);
}
void USimCopterAirOperationsComponent::SelectRemoteTaserTarget(bool bInitial)
{
	auto* Previous=RemoteTaserTarget.Get();
	auto* Best=IsRemotePursuitTarget(Previous) && FVector::DistSquared(Helicopter()->GetActorLocation(),Previous->GetActorLocation())<FMath::Square(9000.f) ? Previous : nullptr;
	const bool bKeepPrevious=Best!=nullptr && !bInitial;
	float BestScore=TNumericLimits<float>::Max();
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
	{
		if (!IsRemotePursuitTarget(*It)) continue;
		const FVector Focus=RemotePursuitFocus(*It);
		const float Distance=FVector::Distance(Helicopter()->GetActorLocation(),Focus);
		if (Distance>6000) continue;
		// Follow the same burglar when they leave or re-enter their getaway car.
		const bool bSameBurglar=Previous && (It->GetBehaviorStartingVehicle()==Previous || (!bKeepPrevious && Previous->GetBehaviorStartingVehicle()==*It));
		const float Dot=FVector::DotProduct(RemoteTaserAim.Vector(),(Focus-GetRemoteTaserMuzzle()).GetSafeNormal());
		const bool bVisible=HasClearLine(GetRemoteTaserMuzzle(),Focus,*It);
		const bool bSightedNew=bKeepPrevious && *It!=Previous && bVisible && Dot>.995f;
		// Keep a stable pursuit unless the player sights another criminal or its burglar exits.
		if (bKeepPrevious && *It!=Previous && !bSameBurglar && !bSightedNew) continue;
		const float Score=Distance+(1-Dot)*1800+(bVisible ? 0 : 6000)-(bSameBurglar ? 12000 : 0)-(bSightedNew ? 8000 : 0);
		if (Score<BestScore) { Best=*It; BestScore=Score; }
	}
	RemoteTaserTarget=Best;
	if (Best) RemoteTaserFocus=RemotePursuitFocus(Best);
	if (Best!=Previous)
	{
		RemotePursuitPlanSeconds=0;
		if (!bInitial)
		{
			// A target handoff changes the flight plan without snapping the player's sight.
			const FRotator Base=(RemoteTaserFocus-RemoteTaserMount->GetComponentLocation()).Rotation();
			RemoteAimOffset=FVector2D(FRotator::NormalizeAxis(RemoteTaserAim.Yaw-Base.Yaw),RemoteTaserAim.Pitch-Base.Pitch);
		}
	}
}
FVector USimCopterAirOperationsComponent::FindRemoteTaserPursuitOffset(const FVector& Velocity) const
{
	const FVector From=Helicopter()->GetActorLocation();
	FVector Radial=Velocity.SizeSquared2D()>FMath::Square(100.f) ? -Velocity.GetSafeNormal2D() : (From-RemoteTaserFocus).GetSafeNormal2D();
	if (Radial.IsNearlyZero()) Radial=-Helicopter()->GetActorForwardVector();
	const FVector Lead=Velocity*.4f;
	// A robber inside solid geometry may have no visible vantage. Hold above the last
	// sighting after a bounded climb instead of ascending forever or following through walls.
	FVector Best=From-RemoteTaserFocus-Lead+FVector(0,0,FMath::Clamp(RemoteTaserFocus.Z+2500-From.Z,0.0,600.0));
	float BestScore=TNumericLimits<float>::Max();
	// Prefer a trailing shot, then look around either side or above intervening buildings.
	// Actual travel still uses the full-airframe swept autopilot, including other aircraft.
	for (float Height : {900.f,1600.f,2500.f})
		for (float Angle : {0.f,45.f,-45.f,90.f,-90.f,135.f,-135.f,180.f})
		{
			const FVector Offset=Radial.RotateAngleAxis(Angle,FVector::UpVector)*1200+FVector(0,0,Height);
			const FVector Candidate=RemoteTaserFocus+Lead+Offset;
			if (!IsRecoveryRouteClear(Candidate,Candidate) || !HasClearLine(Candidate,RemoteTaserFocus,RemoteTaserTarget.Get())) continue;
			const float Score=FVector::DistSquared(From,Candidate)+FMath::Abs(Angle)*2000+
				(IsRecoveryRouteClear(From,Candidate) ? 0 : 100000000.f);
			if (Score<BestScore) { Best=Offset; BestScore=Score; }
		}
	return Best;
}
void USimCopterAirOperationsComponent::UpdateRemoteTaser(float DeltaSeconds)
{
	if (!Cast<APlayerController>(Helicopter()->GetController())) { EndRemoteTaser(); return; }
	RemoteTargetSearchSeconds-=DeltaSeconds;
	if (RemoteTargetSearchSeconds<=0) { SelectRemoteTaserTarget(); RemoteTargetSearchSeconds=.25f; }
	if (auto* Target=RemoteTaserTarget.Get(); IsRemotePursuitTarget(Target))
	{
		RemoteTaserFocus=RemotePursuitFocus(Target);
		const FVector Velocity=Target->GetCurrentVelocityCmPerSec().GetClampedToMaxSize2D(4000);
		RemotePursuitPlanSeconds-=DeltaSeconds;
		if (RemotePursuitPlanSeconds<=0) { RemotePursuitOffset=FindRemoteTaserPursuitOffset(Velocity); RemotePursuitPlanSeconds=.5f; }
		const FVector Goal=RemoteTaserFocus+Velocity*.4f+RemotePursuitOffset;
		const float Speed=FMath::Clamp(Velocity.Size2D()+FVector::Distance(Helicopter()->GetActorLocation(),Goal)*1.8f,350.f,5000.f);
		AdvanceAutopilot(Goal,DeltaSeconds,Speed,true);
	}
	else Helicopter()->SetAutopilotTransform(Helicopter()->GetActorLocation(),Helicopter()->GetActorRotation(),DeltaSeconds);
	const FRotator Facing(0,(RemoteTaserFocus-Helicopter()->GetActorLocation()).Rotation().Yaw,0);
	Helicopter()->SetActorRotation(Facing);
	Helicopter()->FlightModel.Heading=SimCopterFixed::WrapAngle(SimCopterFixed::FromFloat(FRotator::ClampAxis(Facing.Yaw)*10));
	// Keep the target in the helmet sight as the aircraft moves; mouse/stick adjust the sight.
	RemoteTaserAim=(RemoteTaserFocus-RemoteTaserMount->GetComponentLocation()).Rotation();
	RemoteTaserAim.Yaw+=RemoteAimOffset.X;
	RemoteTaserAim.Pitch=FMath::Clamp(RemoteTaserAim.Pitch+RemoteAimOffset.Y,-85.0f,85.0f);
	UpdateRemoteTaserMount();
	if (RemoteTaserCamera) RemoteTaserCamera->SetActorLocationAndRotation(GetRemoteTaserMuzzle(),RemoteTaserAim);
}
void USimCopterAirOperationsComponent::FireRemoteTaser()
{
	if (!bRemoteTaserActive || !RemoteTaserCamera || ShotCooldown>0) return;
	if (!Cast<APlayerController>(Helicopter()->GetController())) { EndRemoteTaser(); return; }
	ShotCooldown=1.25f;
	FVector End;
	UpdateRemoteTaserMount();
	const FVector Muzzle=GetRemoteTaserMuzzle();
	auto* Target=SimCopterAirOperations::TraceRemoteTaserTarget(GetWorld(),Helicopter(),Muzzle,RemoteTaserAim.Vector(),6000,End);
	// Remake-only nonlethal taser. Never dispatch an Apache projectile, damage, or death interaction.
	if (Target && Target->DisableVehicleWithTaser())
		Status=FString::Printf(TEXT("TASER: car disabled for %.0f seconds. Recharging."), SimCopterAirOperations::VehicleTaserDisableSeconds);
	else if (Target && Target->StunForArrest())
		Status=TEXT("TASER: suspect stunned alive. Retrieve, handcuff and deliver to police.");
	else
		Status=TEXT("TASER: no active suspect or drivable car hit. Recharging.");
	Helicopter()->SetAirOperationsStatus(Status);
	SimCopterAirOperations::DrawTaserDischarge(Helicopter()->FindComponentByClass<USimCopterParticleFXComponent>(), Muzzle, End);
}
bool USimCopterAirOperationsComponent::AdvanceAutopilot(const FVector& Goal,float DeltaSeconds,float Speed,bool bAllowApproachDescent)
{
	auto* Heli=Helicopter(); if (!Heli || DeltaSeconds<=0) return false;
	FVector Target=Goal;
	const FVector From=Heli->GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AirSupportNavigation),true,Heli);
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld());It;++It)
		if (It->GetAgentKind() == ESimCopterGroundAgentKind::Pedestrian || It->IsVehicleTowed()) Query.AddIgnoredActor(*It);
	FHitResult Hit;
	// The former 80 cm sphere let the fuselage, tail and rotors clip buildings and cars.
	// Use a yaw-independent footprint so turning cannot swing a tail through an obstacle.
	const FBox Bounds = Heli->GetParkingWorldBounds();
	const FVector Offset = Bounds.IsValid ? Bounds.GetCenter()-From : FVector::ZeroVector;
	const FVector Half = Bounds.IsValid ? Bounds.GetExtent() : FVector(190);
	const float Radius = FMath::Max(Half.X + FMath::Abs(Offset.X), Half.Y + FMath::Abs(Offset.Y));
	const FVector CenterOffset(0,0,Offset.Z);
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(Radius+12,Radius+12,Half.Z+8));
	const auto Blocked = [&](const FVector& A, const FVector& B)
	{
		if (GetWorld()->SweepSingleByChannel(Hit,A+CenterOffset,B+CenterOffset,FQuat::Identity,ECC_Visibility,Shape,Query)) return true;
		// Aircraft meshes are not world-static blockers. Keep the same clearance from
		// the player's helicopter even when its root collision ignores visibility queries.
		for (TActorIterator<ASimCopterHelicopterPawn> It(GetWorld()); It; ++It)
		{
			if (*It == Heli || It->IsHidden() || It->IsActorBeingDestroyed()) continue;
			const FBox Other = It->GetParkingWorldBounds();
			if (Other.IsValid && FMath::LineBoxIntersection(Other.ExpandBy(Shape.GetExtent()),
				A+CenterOffset,B+CenterOffset,B-A)) return true;
		}
		return false;
	};
	const FVector Horizontal=(Goal-From).GetSafeNormal2D();
	// Follow a clear altitude corridor; never fly straight through a facade on the way to a job.
	const float Distance = FVector::Dist2D(From,Goal);
	if (Distance > 10 && Blocked(From,From+Horizontal*FMath::Min(1000.0f,Distance)))
		Target=From+FVector(0,0,600);
	else if (Distance > 10 && !bAllowApproachDescent) Target.Z=FMath::Max(From.Z,Goal.Z);
	// Finish the horizontal approach before descending, instead of cutting into the roof edge.
	const FVector Next=FMath::VInterpConstantTo(From,Target,FMath::Min(DeltaSeconds,0.1f),Speed);
	if (!From.Equals(Next,0.01) && Blocked(From,Next))
	{
		Heli->SetAutopilotTransform(From,Heli->GetActorRotation(),DeltaSeconds);
		return false;
	}
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
		CableInput=0; CagePanelInput=0;
		CageFormation=bDeployed && bCage?1.0f:0.0f;
		CageSeal=bCageOpen?0.0f:1.0f;
		CageCaptureDelay=0;
		EndRemoteTaser();
		EndPilotRecovery();
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

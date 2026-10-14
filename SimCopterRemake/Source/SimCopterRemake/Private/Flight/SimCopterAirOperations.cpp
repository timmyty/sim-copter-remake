#include "Flight/SimCopterAirOperations.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "City/SimCopterEffectExposure.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterEnergyTaser.h"
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
#include "Materials/MaterialInstanceDynamic.h"
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
	void TaperedBar(const FVector& A, const FVector& B, float StartRadius, float EndRadius, const FLinearColor& Color)
	{
		FVector Side, Up; (B-A).GetSafeNormal().FindBestAxisVectors(Side,Up);
		FVector P[8];
		for (int32 K=0; K<8; ++K)
		{
			const float Radius=(K&4) ? EndRadius : StartRadius;
			P[K]=((K&4) ? B : A)+Side*((K&1) ? Radius : -Radius)+Up*((K&2) ? Radius : -Radius);
		}
		static const int32 Faces[6][4]={{0,2,3,1},{4,5,7,6},{0,1,5,4},{2,6,7,3},{0,4,6,2},{1,3,7,5}};
		for (const auto& Face:Faces)
		{
			const int32 Base=V.Num();
			const FVector Normal=FVector::CrossProduct(P[Face[1]]-P[Face[0]],P[Face[2]]-P[Face[0]]).GetSafeNormal();
			for (int32 J=0; J<4; ++J) { V.Add(P[Face[J]]); N.Add(Normal); UV.Add(FVector2D(.5,.5)); C.Add(Color); }
			I.Append({Base,Base+1,Base+2,Base,Base+2,Base+3});
		}
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
	return SimCopterEnergyTaser::IsTarget(Agent);
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
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.bStartWithTickEnabled=false;
	Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RecoverySite"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->bUseComplexAsSimpleCollision=true;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Materials/M_SimCopterLitVertexColor.M_SimCopterLitVertexColor"));
	if (Mat.Succeeded()) Material=Mat.Object;
	WaypointRing=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RecoveryWaypointRing"));
	WaypointRing->SetupAttachment(Mesh);
	WaypointRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaypointRing->SetCastShadow(false);
	WaypointRing->SetVisibility(false);
}

void ASimCopterRecoverySite::SetWaypointActive(bool bActive)
{
	bWaypointActive=bActive && SiteKind!=ESimCopterRecoverySite::AutoRepair;
	WaypointRing->SetVisibility(bWaypointActive);
	SetActorTickEnabled(bWaypointActive);
	if (bWaypointActive) Tick(0);
}

void ASimCopterRecoverySite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RingMaterial && bWaypointActive)
		RingMaterial->SetScalarParameterValue(TEXT("EmissiveNits"),
			USimCopterEffectExposureSubsystem::GetEffectEmissiveNitsForWorld(GetWorld()) *
			(1.6f+0.3f*FMath::Sin(GetWorld()->GetTimeSeconds()*3.0f)));
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
	// Actual world geometry, visible from above and oblique angles in packaged builds.
	// Its inner edge matches the delivery radius; collision remains with the service deck.
	FRecoveryMesh Ring;
	const FLinearColor RingColor=bHarbor ? FLinearColor(0.05f,0.65f,1) : FLinearColor(1,0.68f,0.04f);
	for (int32 K=0; K<96; ++K)
	{
		const float A=K*2*PI/96, B=(K+1)*2*PI/96;
		const float Radius=SimCopterAirOperations::RecoveryRadiusCm+6;
		Ring.Bar(FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,12),
			FVector(FMath::Cos(B)*Radius,FMath::Sin(B)*Radius,12),6,RingColor);
	}
	Ring.Apply(WaypointRing);
	RingMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Materials/M_SimCopterParticleFX.M_SimCopterParticleFX")),this);
	WaypointRing->SetMaterial(0,RingMaterial);
	SetWaypointActive(false);
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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrailMat(TEXT("/Game/Materials/M_SimCopterParticleFX.M_SimCopterParticleFX"));
	if (TrailMat.Succeeded()) TaserTrailMaterial=TrailMat.Object;
}
ASimCopterHelicopterPawn* USimCopterAirOperationsComponent::Helicopter() const { return Cast<ASimCopterHelicopterPawn>(GetOwner()); }
bool USimCopterAirOperationsComponent::HasCargo() const { return TowVehicle.IsValid() || BoatIndex!=INDEX_NONE || !CagePeople.IsEmpty(); }
bool USimCopterAirOperationsComponent::IsCargoToolSelected() const
{
	const auto* Heli=Helicopter();
	return Heli && Heli->GetActiveTool()==ESimCopterHelicopterTool::CaptureCage;
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
	if (RemoteTaserTrail)
	{
		RemoteTaserTrail->GetOwner()->Destroy();
		RemoteTaserTrail = nullptr;
		TaserTrailMaterialInstance = nullptr;
	}
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
	if (HasBoat()) return TEXT("SEALED / BOAT");
	if (TowVehicle.IsValid()) return TEXT("SEALED / CAR");
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
	if (!Helicopter()->IsToolAvailable(Helicopter()->GetActiveTool())) return;
	EnsureMesh();
	if (!bDeployed)
	{
		bCage=true; bCageOpen=true; bDeployed=true; CableLength=140;
		CageFormation=0; CageSeal=0; CageCaptureDelay=0;
		CargoLocation=Helicopter()->GetCaptureCageMountTransform().GetLocation()-FVector(0,0,CableLength);
		CargoVelocity=FVector::ZeroVector;
		Status=TEXT("Lower the cage over people, stalled/fleeing cars or rescued boat hulls. The floor seals automatically.");
	}
	else if (HasCargo())
	{
		Status=TowVehicle.IsValid() || HasBoat() ? TEXT("Cars: JUNKYARD. Boats: HARBOR. Both: police roof. Lower and press OPEN / G to deliver.") : TEXT("Raise and hold at the top to board. Deliver prisoners to the police roof. G opens the floor.");
	}
	else if (!TryGrabCargo())
	{
		if (CableLength<=145) { bDeployed=false; }
		else Status=TEXT("No eligible cargo inside the cage. Move closer or lower it.");
	}
}
bool USimCopterAirOperationsComponent::TryGrabCargo()
{
	if (!bDeployed || !bCage || !bCageOpen || HasCargo() || CageCaptureDelay>0 || !Helicopter()) return false;
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
		if (!CagePeople.IsEmpty())
		{
			bCageOpen=false;
			Status=FString::Printf(TEXT("Cage closed: %d occupants. Hold UP at the top to board; G opens."),CagePeople.Num());
			return true;
		}
	}
	if (HasCargo()) return false;
	ASimCopterGroundAgent* Best=nullptr; float Distance=120;
	for (TActorIterator<ASimCopterGroundAgent> It(GetWorld()); It; ++It)
	{
		const float D=FVector::Distance(It->GetActorLocation(),CargoLocation);
		if (It->IsCageRecoverableVehicle() && !It->IsVehicleTowed() && D<Distance && HasClearLine(CargoLocation,It->GetActorLocation(),*It)) { Best=*It; Distance=D; }
	}
	if (Best)
	{
		TowVehicle=Best; Best->SetVehicleTowed(true); bCageOpen=false;
		Status=TEXT("Car secured. Floor sealing. Carry to the JUNKYARD ring or police roof, lower and press OPEN / G.");
		if (auto* System=GetWorld()->GetSubsystem<USimCopterAirOperationsSubsystem>()) System->RefreshRecoveryWaypoints();
		return true;
	}
	if (auto* Ambient=ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld()); Ambient && Ambient->FindTowableBoat(CargoLocation,170,BoatIndex))
	{
		FVector HullLocation;
		if (!Ambient->GetBoatTowLocation(BoatIndex,HullLocation) || !HasClearLine(CargoLocation,HullLocation,Ambient))
		{ BoatIndex=INDEX_NONE; return false; }
		if (!Ambient->SetBoatTow(BoatIndex,true,CargoLocation))
		{
			BoatIndex=INDEX_NONE; Status=TEXT("Rescue everyone from the hull before towing it."); return false;
		}
		bCageOpen=false;
		Status=TEXT("Boat secured. Floor sealing. Carry to the HARBOR ring or police roof, lower and press OPEN / G.");
		if (auto* System=GetWorld()->GetSubsystem<USimCopterAirOperationsSubsystem>()) System->RefreshRecoveryWaypoints();
		return true;
	}
	return false;
}
void USimCopterAirOperationsComponent::OpenCargo()
{
	if (!bDeployed) return;
	if (TryDeliverCargo()) return;
	ReleaseCargo(); bCageOpen=bCage;
	Status=TEXT("Cage floor open. Cargo released.");
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
	if ((!TowVehicle.IsValid() && !HasBoat()) || !GetWorld() || bCageOpen || CageSeal<1) return false;
	auto* Ops=GetWorld()->GetSubsystem<USimCopterAirOperationsSubsystem>();
	auto* Ambient=HasBoat()?ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld()):nullptr;
	auto IsCargoDropInside=[&](const FVector& Center,double Extent,bool bSquare)
	{
		FBox Body;
		FTransform Drop;
		bool bHasGeometry=false;
		if (TowVehicle.IsValid())
		{
			bHasGeometry=TowVehicle->TryGetBodyLocalBoundsCm(Body);
			Drop=TowVehicle->GetActorTransform(); Drop.SetLocation(CargoLocation-FVector(0,0,25));
		}
		else if (Ambient)
		{
			bHasGeometry=Ambient->GetBoatTowGeometry(BoatIndex,Body,Drop);
			Drop.SetLocation(CargoLocation);
		}
		if (!bHasGeometry)
			return FVector::Dist2D(CargoLocation,Center)<=Extent && CargoLocation.Z-Center.Z>=-30 && CargoLocation.Z-Center.Z<=370;
		// Match UpdateCargo between physics ticks, including the hull's capsize rotation.
		// The visible footprint and bottom govern a short drop, not the cage/mesh pivot.
		const double BottomHeight=Body.TransformBy(Drop).Min.Z-Center.Z;
		if (BottomHeight < -70 || BottomHeight > 300) return false;
		constexpr int32 SamplesPerAxis=9;
		int32 Inside=0;
		for (int32 X=0; X<SamplesPerAxis; ++X) for (int32 Y=0; Y<SamplesPerAxis; ++Y)
		{
			const FVector Sample=Drop.TransformPosition(FVector(
				FMath::Lerp(Body.Min.X,Body.Max.X,(X+.5)/SamplesPerAxis),
				FMath::Lerp(Body.Min.Y,Body.Max.Y,(Y+.5)/SamplesPerAxis),Body.Min.Z))-Center;
			Inside+=bSquare ? (FMath::Abs(Sample.X)<=Extent && FMath::Abs(Sample.Y)<=Extent) : Sample.SizeSquared2D()<=FMath::Square(Extent);
		}
		return Inside>SamplesPerAxis*SamplesPerAxis/2;
	};
	FVector PoliceRoof;
	float RoofHalfExtent=0;
	int32 RoofX=0,RoofY=0;
	auto* Road=Ops?Ops->Traffic():nullptr;
	if (Ops && (HasBoat() || (TowVehicle.IsValid() && (TowVehicle->IsCriminalCar() || TowVehicle->IsTowableVehicle()))) &&
		Road && Road->TryGetPeopleTileCoordinateAtWorldLocation(CargoLocation,RoofX,RoofY) &&
		Road->GetXbldTileId(RoofX,RoofY)==0xD2 && Road->TryGetBuildingRoofPost(RoofX,RoofY,PoliceRoof,RoofHalfExtent) &&
		IsCargoDropInside(PoliceRoof,RoofHalfExtent,true) &&
		CargoVelocity.Size()<=200)
	{
		if (HasBoat())
		{
			if (!Ambient || !Ambient->FinishBoatTow(BoatIndex,&PoliceRoof)) return false;
			BoatIndex=INDEX_NONE;
		}
		else
		{
			TowVehicle->SetActorLocation(PoliceRoof+FVector(0,0,65));
			TowVehicle->CompleteVehicleTow(false); TowVehicle.Reset();
		}
		bCageOpen=true; CageCaptureDelay=1.5f; BoardHoldSeconds=0;
		Ops->RefreshRecoveryWaypoints();
		Status=TEXT("Police station accepted the vehicle. Recovery complete.");
		return true;
	}
	if (Ops) Ops->RefreshRecoveryWaypoints();
	auto* Site=Ops?Ops->FindRecoverySite(CargoLocation,BoatIndex!=INDEX_NONE):nullptr;
	if (!Site || !Site->IsWaypointActive() || CargoVelocity.Size()>200) return false;
	if (!IsCargoDropInside(Site->GetActorLocation(),SimCopterAirOperations::RecoveryRadiusCm,false)) return false;
	if (TowVehicle.IsValid())
	{
		TowVehicle->SetActorLocation(Site->GetActorLocation()+FVector(0,0,65));
		TowVehicle->CompleteVehicleTow(Site->GetKind()==ESimCopterRecoverySite::Junkyard); TowVehicle.Reset();
	}
	if (BoatIndex!=INDEX_NONE)
	{
		if (!Ambient) return false;
		const FVector CradleSurface=Site->GetActorLocation()+FVector(0,0,50);
		if (!Ambient->FinishBoatTow(BoatIndex,&CradleSurface)) return false;
		BoatIndex=INDEX_NONE;
	}
	bCageOpen=true; CageCaptureDelay=1.5f; BoardHoldSeconds=0;
	Ops->RefreshRecoveryWaypoints();
	Status=TEXT("Recovery complete. Cargo accepted.");
	return true;
}
FBox USimCopterAirOperationsComponent::GetCageCargoBounds() const
{
	FBox Bounds(FVector(-75,-75,-28),FVector(75,75,65));
	const FTransform CageFrame(FRotator(0,Helicopter()->GetActorRotation().Yaw,0),CargoLocation);
	FBox Body(ForceInit);
	if (TowVehicle.IsValid() && TowVehicle->TryGetBodyLocalBoundsCm(Body))
	{
		for (int32 K=0; K<8; ++K)
		{
			const FVector Corner((K&1)?Body.Max.X:Body.Min.X,(K&2)?Body.Max.Y:Body.Min.Y,(K&4)?Body.Max.Z:Body.Min.Z);
			const FVector Local=CageFrame.InverseTransformPosition(TowVehicle->GetActorTransform().TransformPosition(Corner));
			Bounds+=Local-FVector(12,12,12); Bounds+=Local+FVector(12,12,12);
		}
	}
	if (HasBoat())
		if (const auto* Ambient=ActorIn<ASimCopterAmbientVehiclesActor>(GetWorld()); Ambient && Ambient->GetBoatTowBounds(BoatIndex,Body))
			for (int32 K=0; K<8; ++K)
			{
				const FVector Corner((K&1)?Body.Max.X:Body.Min.X,(K&2)?Body.Max.Y:Body.Min.Y,(K&4)?Body.Max.Z:Body.Min.Z);
				const FVector Local=CageFrame.InverseTransformPosition(Corner);
				Bounds+=Local-FVector(12,12,12); Bounds+=Local+FVector(12,12,12);
			}
	return Bounds;
}

void USimCopterAirOperationsComponent::DrawCargo()
{
	if (!CargoMesh || !Helicopter()) return;
	CargoMesh->SetVisibility(bDeployed || (bCage && CageFormation > 0));
	if (!bDeployed && (!bCage || CageFormation <= 0)) return;
	FRecoveryMesh Shape;
	const FLinearColor Steel(0.3f,0.35f,0.4f), Yellow(0.95f,0.65f,0.06f);
	if (bCage)
	{
		const FTransform Mount = Helicopter()->GetCaptureCageMountTransform();
		const FQuat Yaw = FRotator(0, Helicopter()->GetActorRotation().Yaw, 0).Quaternion();
		const FBox Bounds=GetCageCargoBounds();
		const float HalfX=FMath::Max(FMath::Abs(Bounds.Min.X),FMath::Abs(Bounds.Max.X));
		const float HalfY=FMath::Max(FMath::Abs(Bounds.Min.Y),FMath::Abs(Bounds.Max.Y));
		auto FloorPoint = [&](float X, float Y)
		{
			return FMath::Lerp(Mount.TransformPosition(FVector(X,Y,-8)),
				CargoLocation + Yaw.RotateVector(FVector(X*HalfX/75,Y*HalfY/75,Bounds.Min.Z)), double(CageFormation));
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
	Shape.Apply(CargoMesh);
}
void USimCopterAirOperationsComponent::Update(float DeltaSeconds)
{
	if (!Helicopter() || !GetWorld()) return;
	// Saves retain the on-foot pilot's parked aircraft and the aircraft's engine state.
	// Reconnect that real pair after loading instead of persisting a transient camera or pointer.
	if (bAutopilotEnabled && !IsAIPiloted() && !Cast<APlayerController>(Helicopter()->GetController()) && Helicopter()->IsEngineRunning() && !Helicopter()->bIsLanded && !Helicopter()->IsFixedWingAircraft())
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
	UpdateRemoteTaserDischarge(DeltaSeconds);
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
	// Keep the roof of a vehicle/hull below the skid fittings even when fully raised.
	const float MinimumCable=TowVehicle.IsValid() || HasBoat() ? FMath::Max(140.0f,float(GetCageCargoBounds().Max.Z)+50) : 140.0f;
	CableLength=FMath::Clamp(CableLength-MotionInput*360.0f*DeltaSeconds,MinimumCable,SimCopterAirOperations::MaxCableCm);
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
	if (bCage && !HasCargo() && CableLength<=141 && CableInput>0.5f)
	{
		bDeployed=false;
		Status=TEXT("Capture cage stowed on skids.");
	}
	DrawCargo();
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
FString USimCopterAirOperationsComponent::GetRemoteTaserReadout(bool bCompact) const
{
	if (!bRemoteTaserActive) return TEXT("N / X: Tools");
	if (bCompact) return FString::Printf(TEXT("%.1fx  |  %s"),RemoteScopeZoom,ShotCooldown>0 ? TEXT("RECHARGING") : TEXT("READY"));
	const TCHAR* Target = !RemoteTaserTarget.IsValid() ? TEXT("SEARCHING") :
		(RemoteTaserTarget->GetAgentKind()==ESimCopterGroundAgentKind::Vehicle ? TEXT("CAR") : TEXT("SUSPECT"));
	return FString::Printf(TEXT("%s | %s | %.1fx | %s"),bRemoteHolding ? TEXT("HOLD") : TEXT("FOLLOW"),Target,RemoteScopeZoom,ShotCooldown>0 ? TEXT("RECHARGING") : TEXT("READY"));
}
void USimCopterAirOperationsComponent::SetAutopilotEnabled(bool bEnabled)
{
	if (bAutopilotEnabled==bEnabled) return;
	bAutopilotEnabled=bEnabled;
	if (!bEnabled) EndPilotRecovery();
	// Replan from the current hover when enabled again, never from a stale pursuit goal.
	RemotePursuitPlanSeconds=0; RemotePursuitSpeed=0; RemoteObstructedSeconds=0;
	if (auto* Heli=Helicopter())
	{
		RemotePursuitGoal=Heli->GetActorLocation();
		if (bRemoteTaserActive && !bEnabled)
		{
			bRemoteHolding=true;
			Heli->SetAutopilotTransform(RemotePursuitGoal,Heli->GetActorRotation(),.01f);
		}
	}
}
void USimCopterAirOperationsComponent::ToggleRemoteTaser()
{
	auto* Heli=Helicopter();
	if (bRemoteTaserActive)
	{
		auto* Target=RemoteTaserTarget.IsValid() ? RemoteTaserTarget.Get() : LastRemoteTaserTarget.Get();
		// A successful stun ends pursuit, but the player still needs to face that suspect
		// to retrieve them. Only a deliberate return to flight turns the aircraft; cleanup
		// during possession loss, save restoration or destruction must not change heading.
		const bool bStunnedSuspect=IsValid(Target) && Target->IsTaserStunned() && !Target->IsHandcuffed() &&
			!Target->IsActorBeingDestroyed() && !Target->IsHidden() && !Target->HasMissionResolutionReported() &&
			!Target->IsMissionCarried() && !Target->GetBehaviorCarrier() && !Target->IsMedevacVictim() && !Target->IsMissionPatientDead();
		if (bAutopilotEnabled && Heli && (IsRemotePursuitTarget(Target) || bStunnedSuspect) &&
			FVector::DistSquared(Heli->GetActorLocation(),Target->GetActorLocation())<FMath::Square(9000.f))
		{
			const FVector Direction=(RemotePursuitFocus(Target)-Heli->GetActorLocation()).GetSafeNormal2D();
			if (!Direction.IsNearlyZero())
			{
				FRotator Facing=Heli->GetActorRotation(); Facing.Yaw=Direction.Rotation().Yaw;
				Heli->SetActorRotation(Facing);
				// Keep the original tenth-degree flight state in sync or the next tick undoes the turn.
				Heli->FlightModel.Heading=SimCopterFixed::WrapAngle(SimCopterFixed::FromFloat(FRotator::ClampAxis(Facing.Yaw)*10));
			}
		}
		EndRemoteTaser(); return;
	}
	if (!Heli || Heli->IsFixedWingAircraft() || Heli->bIsLanded || Heli->FlightModel.State==ESimCopterFlightState::Dying)
	{ Status=TEXT("Take off to use the remote taser."); if(Heli) Heli->SetAirOperationsStatus(Status); return; }
	auto* PC=Cast<APlayerController>(Heli->GetController()); if (!PC) return;
	Heli->ResetTransientInputState();
	UpdateRemoteTaserMount();
	FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
	RemoteTaserFocus=Eye+View.Vector()*2500;
	RemoteTaserTarget.Reset();
	LastRemoteTaserTarget.Reset();
	RemoteTaserAim=FRotator(FMath::Clamp(View.Pitch,-85.f,85.f),View.Yaw,0);
	RemotePursuitSpeed=FMath::Min(Heli->VelocityCmPerSec.Size(),Heli->GetMaxForwardSpeedCmPerSec()*
		SimCopterFixed::ToFloat(Heli->FlightModel.BaseTravelScale)*SimCopterFixed::ToFloat(FSimCopterFlightModel::TravelBoostScale));
	RemoteTargetSearchSeconds=RemotePursuitPlanSeconds=0;
	RemotePursuitGoal=Heli->GetActorLocation();
	RemoteObstructedSeconds=0; bRemoteHolding=!bAutopilotEnabled;
	RemoteScopeZoom=RemoteScopeZoomTarget=1;
	SelectRemoteTaserTarget();
	RemoteTaserCamera=GetWorld()->SpawnActor<ACameraActor>(); if (!RemoteTaserCamera) return;
	RemoteTaserCamera->GetCameraComponent()->SetFieldOfView(45);
	bRemoteTaserActive=true;
	UpdateRemoteTaserMount();
	RemoteTaserCamera->SetActorLocationAndRotation(GetRemoteTaserMuzzle(),RemoteTaserAim);
	// A full-turn gimbal must not look into its own fuselage when aiming behind the nose.
	PC->HiddenActors.AddUnique(Heli);
	PC->SetViewTarget(RemoteTaserCamera);
	SetRemoteTaserCursor(false);
	Status=TEXT("REMOTE TASER | steady hover with a clear shot | mouse / right stick aim | wheel zoom / hold LT for 4x | click / A / RT fire | hold RMB for cockpit | N or X then Fly Helicopter returns to flight");
	Heli->SetAirOperationsStatus(Status);
}
void USimCopterAirOperationsComponent::EndRemoteTaser()
{
	if (bRemoteTaserActive) if (auto* Heli=Helicopter()) if (auto* PC=Cast<APlayerController>(Heli->GetController()))
	{
		PC->SetViewTarget(Heli); PC->HiddenActors.Remove(Heli); SetRemoteTaserCursor(true);
	}
	bRemoteTaserActive=false;
	RemoteScopeZoom=RemoteScopeZoomTarget=1;
	bRemoteHolding=false; RemoteObstructedSeconds=0;
	if (RemoteTaserCamera) RemoteTaserCamera->Destroy(); RemoteTaserCamera=nullptr;
	RemoteTaserTarget.Reset();
	LastRemoteTaserTarget.Reset();
	if (auto* Heli=Helicopter()) { Heli->ResetTransientInputState(); Heli->RefreshCrosshairVisibility(); }
}
void USimCopterAirOperationsComponent::AimRemoteTaser(float Yaw,float Pitch,float DeltaSeconds)
{
	if (!bRemoteTaserActive) return;
	OffsetRemoteTaserAim(Yaw*45.0f*DeltaSeconds/RemoteScopeZoom,Pitch*35.0f*DeltaSeconds/RemoteScopeZoom);
}
bool USimCopterAirOperationsComponent::CanControlRemoteTaser() const
{
	const auto* Heli=Helicopter();
	const auto* PC=Heli ? Cast<APlayerController>(Heli->GetController()) : nullptr;
	if (!bRemoteTaserActive || !PC || PC->IsPaused() || Heli->GetControllerMode()!=ESimCopterControllerMode::None || Heli->IsCheckupMenuOpen()) return false;
	const auto* GamePC=Cast<ASimCopterPlayerController>(PC);
	return !GamePC || (!GamePC->IsSettingsOpen() && !GamePC->IsReplayPanelOpen());
}
void USimCopterAirOperationsComponent::ZoomRemoteTaser(float Steps)
{
	if (!CanControlRemoteTaser() || !FMath::IsFinite(Steps)) return;
	const auto* PC=Cast<APlayerController>(Helicopter()->GetController());
	if (PC->IsInputKeyDown(EKeys::RightMouseButton)) return;
	RemoteScopeZoomTarget=FMath::Clamp(RemoteScopeZoomTarget+Steps*.5f,1.f,4.f);
}
void USimCopterAirOperationsComponent::OffsetRemoteTaserAim(float YawDegrees,float PitchDegrees)
{
	// World-stabilized gimbal: pursuit, aircraft turns and target handoffs never steer the sight.
	RemoteTaserAim.Yaw=FRotator::NormalizeAxis(RemoteTaserAim.Yaw+YawDegrees);
	RemoteTaserAim.Pitch=FMath::Clamp(RemoteTaserAim.Pitch+PitchDegrees,-85.0f,85.0f);
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
	if (!CanControlRemoteTaser()) return;
	auto* PC=Cast<APlayerController>(Helicopter()->GetController());
	const bool bCursor=PC->IsInputKeyDown(EKeys::RightMouseButton);
	if (PC->bShowMouseCursor!=bCursor) { SetRemoteTaserCursor(bCursor); MouseYaw=MousePitch=0; }
	const auto* Settings=USimCopterSettings::Get(this);
	// Mouse axes are per-frame travel, whereas a held stick is a rate. Scaling both by dt
	// made mouse aim vary with frame rate and required dragging a cursor across the screen.
	if (!bCursor) OffsetRemoteTaserAim(MouseYaw*2.5f*(Settings ? Settings->GetMouseSensitivityX() : 1)/RemoteScopeZoom,
		-MousePitch*2.5f*(Settings ? Settings->GetMouseSensitivityY() : 1)/RemoteScopeZoom);
	AimRemoteTaser(StickYaw*(Settings ? Settings->GetControllerSensitivityX() : 1),
		-StickPitch*(Settings ? Settings->GetControllerSensitivityY() : 1),DeltaSeconds);
}
void USimCopterAirOperationsComponent::SelectRemoteTaserTarget()
{
	auto* Previous=RemoteTaserTarget.Get();
	auto* Best=IsRemotePursuitTarget(Previous) && FVector::DistSquared(Helicopter()->GetActorLocation(),Previous->GetActorLocation())<FMath::Square(9000.f) ? Previous : nullptr;
	const bool bKeepPrevious=Best!=nullptr;
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
	if (Best) { RemoteTaserFocus=RemotePursuitFocus(Best); LastRemoteTaserTarget=Best; }
	if (Best!=Previous)
	{
		RemotePursuitPlanSeconds=0;
		RemoteObstructedSeconds=0;
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
	if (auto* Target=RemoteTaserTarget.Get(); bAutopilotEnabled && IsRemotePursuitTarget(Target))
	{
		RemoteTaserFocus=RemotePursuitFocus(Target);
		const FVector Velocity=Target->GetCurrentVelocityCmPerSec().GetClampedToMaxSize2D(4000);
		const float Distance=FVector::Distance(GetRemoteTaserMuzzle(),RemoteTaserFocus);
		const bool bClear=HasClearLine(GetRemoteTaserMuzzle(),RemoteTaserFocus,Target);
		RemoteObstructedSeconds=bClear ? 0.f : RemoteObstructedSeconds+FMath::Max(0.f,DeltaSeconds);
		// Remake aiming assist: keep a useful firing position rather than orbiting a
		// moving target. Separate enter/leave distances prevent boundary oscillation;
		// a brief occlusion must not kick the platform out from under the crosshair.
		const bool bWasHolding=bRemoteHolding;
		bRemoteHolding=bRemoteHolding ? Distance<4800 && RemoteObstructedSeconds<.8f : Distance<3500 && bClear;
		if (bRemoteHolding)
		{
			RemotePursuitSpeed=0;
			RemotePursuitGoal=Helicopter()->GetActorLocation();
			Helicopter()->SetAutopilotTransform(RemotePursuitGoal,Helicopter()->GetActorRotation(),DeltaSeconds);
		}
		else
		{
			if (bWasHolding) RemotePursuitPlanSeconds=0;
			RemotePursuitPlanSeconds-=DeltaSeconds;
			if (RemotePursuitPlanSeconds<=0) { RemotePursuitOffset=FindRemoteTaserPursuitOffset(Velocity); RemotePursuitPlanSeconds=2.f; }
			const FVector Goal=RemoteTaserFocus+Velocity*.4f+RemotePursuitOffset;
			const float Dt=FMath::Clamp(DeltaSeconds,0.f,.1f);
			RemotePursuitGoal=FMath::Lerp(RemotePursuitGoal,Goal,1.f-FMath::Exp(-3.f*Dt));
			const float TurboSpeed=Helicopter()->GetMaxForwardSpeedCmPerSec()*
				SimCopterFixed::ToFloat(Helicopter()->FlightModel.BaseTravelScale)*SimCopterFixed::ToFloat(FSimCopterFlightModel::TravelBoostScale);
			const float Speed=FMath::Clamp(Velocity.Size2D()+FVector::Distance(Helicopter()->GetActorLocation(),RemotePursuitGoal)*1.8f,0.f,TurboSpeed);
			RemotePursuitSpeed=FMath::FInterpConstantTo(RemotePursuitSpeed,Speed,Dt,TurboSpeed);
			AdvanceAutopilot(RemotePursuitGoal,DeltaSeconds,RemotePursuitSpeed,true);
		}
	}
	else
	{
		RemotePursuitSpeed=0;
		bRemoteHolding=true; RemoteObstructedSeconds=0;
		RemotePursuitGoal=Helicopter()->GetActorLocation();
		Helicopter()->SetAutopilotTransform(Helicopter()->GetActorLocation(),Helicopter()->GetActorRotation(),DeltaSeconds);
	}
	UpdateRemoteTaserMount();
	if (RemoteTaserCamera)
	{
		if (CanControlRemoteTaser())
		{
			const float Zoom=Helicopter()->ControllerLeftTriggerInput>.5f ? 4.f : RemoteScopeZoomTarget;
			RemoteScopeZoom=FMath::Lerp(RemoteScopeZoom,Zoom,1.f-FMath::Exp(-12.f*FMath::Max(0.f,DeltaSeconds)));
		}
		// Optical magnification: divide the projection's half-angle tangent, keeping
		// the centre ray at the real muzzle so zoom cannot move or extend a shot.
		RemoteTaserCamera->GetCameraComponent()->SetFieldOfView(FMath::RadiansToDegrees(2.f*FMath::Atan(FMath::Tan(FMath::DegreesToRadians(22.5f))/RemoteScopeZoom)));
		RemoteTaserCamera->SetActorLocationAndRotation(GetRemoteTaserMuzzle(),RemoteTaserAim);
	}
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
	bool bHit = false;
	// Remake-only nonlethal taser. Never dispatch an Apache projectile, damage, or death interaction.
	if (Target && Target->DisableVehicleWithTaser())
	{
		bHit = true;
		Status=FString::Printf(TEXT("TASER: car disabled for %.0f seconds. Recharging."), SimCopterAirOperations::VehicleTaserDisableSeconds);
	}
	else if (Target && Target->StunForArrest())
	{
		bHit = true;
		Status=TEXT("TASER: suspect stunned alive. Retrieve, handcuff and deliver to police.");
	}
	else
		Status=TEXT("TASER: no active suspect or drivable car hit. Recharging.");
	Helicopter()->SetAirOperationsStatus(Status);
	const bool bPersonContact = Target && Target->GetAgentKind() == ESimCopterGroundAgentKind::Pedestrian;
	DrawRemoteTaserDischarge(Muzzle, End, bHit || bPersonContact);
	if ((bHit || bPersonContact) && RemoteTaserTrail)
	{
		TaserImpactTarget = Target;
		TaserImpactSeconds = 0.7f;
		UpdateTaserImpact(0);
	}
	if (auto* Audio = USimCopterAudioSubsystem::Get(this))
	{
		// Separate voices let the discharge and contact cue play together. Both obey effects volume.
		Audio->PlayFile2D(TEXT("laser"), SimCopterSound::ESoundDir::Root, 0.8f, true);
		if (bHit) Audio->PlayFile2D(TEXT("punch3"), SimCopterSound::ESoundDir::Root, 0.9f, true);
	}
}

void USimCopterAirOperationsComponent::DrawRemoteTaserDischarge(const FVector& Start, const FVector& End, bool bHit)
{
	if (!RemoteTaserTrail)
	{
		// The remote camera hides the whole helicopter through PlayerController::HiddenActors.
		// World-space coordinates alone cannot exempt a helicopter-owned primitive from that
		// list. Keep both the discharge and body shock on a separate, unattached FX actor.
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.Owner = GetOwner();
		AActor* Effects = GetWorld()->SpawnActor<AActor>(Params);
		if (!Effects) return;
		RemoteTaserTrail = NewObject<UProceduralMeshComponent>(Effects, TEXT("RemoteTaserWhiteTrail"));
		Effects->AddInstanceComponent(RemoteTaserTrail);
		Effects->SetRootComponent(RemoteTaserTrail);
		RemoteTaserTrail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RemoteTaserTrail->SetCastShadow(false);
		RemoteTaserTrail->SetCanEverAffectNavigation(false);
		RemoteTaserTrail->RegisterComponent();
		RemoteTaserTrail->SetWorldTransform(FTransform::Identity);
		TaserTrailMaterialInstance = UMaterialInstanceDynamic::Create(TaserTrailMaterial, this);
		RemoteTaserTrail->SetMaterial(0, TaserTrailMaterialInstance);
	}
	RemoteTaserTrail->ClearAllMeshSections();
	TaserImpactTarget.Reset(); TaserImpactSeconds = 0;
	const FVector Direction = (End-Start).GetSafeNormal();
	const FVector Side = Direction.Rotation().RotateVector(FVector::RightVector);
	const FVector Up = Direction.Rotation().RotateVector(FVector::UpVector);
	FRecoveryMesh Trail;
	constexpr int32 Segments = 32;
	for (int32 Strand = 0; Strand < 2; ++Strand)
	{
		const float Sign = Strand == 0 ? -1.0f : 1.0f;
		// The sight sits between the two scaled muzzle electrodes. Starting both solid
		// bars at its eye encloses the camera and fills the reticle with a white square.
		const FVector Electrode = Start + Side*(Sign*2.0f);
		FVector Previous = Electrode;
		for (int32 I = 1; I <= Segments; ++I)
		{
			const float T = float(I)/Segments;
			const float Spread = FMath::Sin(T*PI);
			const FVector Next = FMath::Lerp(Electrode, End, T) + Spread *
				(Side * (Sign*10.0f + (I%2 ? 5.0f : -5.0f)) + Up * (I%3-1)*5.0f);
			const float Radius = .18f+T*.8f;
			if (I == 1) Trail.TaperedBar(Previous, Next, .001f, Radius, FLinearColor::White);
			else Trail.Bar(Previous, Next, Radius, FLinearColor::White);
			Previous = Next;
		}
	}
	if (bHit)
		for (int32 I = 0; I < 8; ++I)
		{
			const float Angle = I*PI/4;
			Trail.Bar(End, End+(Side*FMath::Cos(Angle)+Up*FMath::Sin(Angle))*22, 1.8f, FLinearColor::White);
		}
	// The particle material's centre is opaque; constant UVs turn its cards into continuous lines.
	for (FVector2D& UV : Trail.UV) UV = FVector2D(0.5, 0.5);
	Trail.Apply(RemoteTaserTrail);
	RemoteTaserTrail->SetVisibility(true);
	TaserTrailSeconds = 0.3f;
	UpdateRemoteTaserDischarge(0);
}

void USimCopterAirOperationsComponent::UpdateRemoteTaserDischarge(float DeltaSeconds)
{
	if (!RemoteTaserTrail || (TaserTrailSeconds <= 0 && TaserImpactSeconds <= 0)) return;
	TaserTrailSeconds = FMath::Max(0.0f, TaserTrailSeconds-FMath::Max(0.0f, DeltaSeconds));
	if (TaserTrailSeconds <= 0)
	{
		RemoteTaserTrail->ClearMeshSection(0);
	}
	UpdateTaserImpact(DeltaSeconds);
	RemoteTaserTrail->SetVisibility(TaserTrailSeconds > 0 || TaserImpactSeconds > 0);
	if (TaserTrailSeconds <= 0 && TaserImpactSeconds <= 0) RemoteTaserTrail->ClearAllMeshSections();
	if (TaserTrailMaterialInstance)
		TaserTrailMaterialInstance->SetScalarParameterValue(TEXT("EmissiveNits"),
			USimCopterEffectExposureSubsystem::GetEffectEmissiveNitsForWorld(GetWorld())*2);
}

void USimCopterAirOperationsComponent::UpdateTaserImpact(float DeltaSeconds)
{
	TaserImpactSeconds = FMath::Max(0.0f, TaserImpactSeconds-FMath::Max(0.0f, DeltaSeconds));
	auto* Target = TaserImpactTarget.Get();
	FBox Body(ForceInit);
	const bool bPerson = Target && Target->GetAgentKind() == ESimCopterGroundAgentKind::Pedestrian;
	if (IsValid(Target) && !Target->IsActorBeingDestroyed() && !Target->IsHidden())
	{
		if (bPerson)
		{
			// The original figure changes shape for the stunned pose. Use its current rendered
			// bounds, not the standing collision capsule, so the electricity stays on the person.
			if (auto* Figure = Target->FindComponentByClass<UProceduralMeshComponent>(); Figure && Figure->IsVisible())
			{
				FBox Pose(ForceInit);
				for (int32 Section = 0; Section < Figure->GetNumSections(); ++Section)
					if (const auto* Part = Figure->GetProcMeshSection(Section); Part && Part->bSectionVisible)
						Pose += Part->SectionLocalBox;
				if (Pose.IsValid) Body = Pose.TransformBy(Figure->GetComponentTransform().GetRelativeTransform(Target->GetActorTransform()));
			}
			if (!Body.IsValid || Body.GetSize().IsNearlyZero())
			{
				const float HalfHeight = Target->GetCapsuleHalfHeightCm();
				const FVector Half(FMath::Max(10.0f,HalfHeight*0.45f),FMath::Max(10.0f,HalfHeight*0.45f),HalfHeight);
				Body = FBox(-Half, Half);
			}
		}
		else Target->TryGetBodyLocalBoundsCm(Body);
	}
	if (TaserImpactSeconds <= 0 || !IsValid(Target) || Target->IsActorBeingDestroyed() || Target->IsHidden() || !Body.IsValid)
	{
		TaserImpactSeconds = 0; TaserImpactTarget.Reset();
		RemoteTaserTrail->ClearMeshSection(1);
		return;
	}
	Body = Body.ExpandBy(5);
	const float Age = 0.7f-TaserImpactSeconds;
	const FTransform Transform = Target->GetActorTransform();
	const FVector Corners[] = {FVector(Body.Min.X,Body.Min.Y,0),FVector(Body.Max.X,Body.Min.Y,0),
		FVector(Body.Max.X,Body.Max.Y,0),FVector(Body.Min.X,Body.Max.Y,0)};
	FRecoveryMesh Arcs;
	for (int32 Ring = 0; Ring < 3; ++Ring)
	{
		FVector Previous;
		for (int32 I = 0; I <= 32; ++I)
		{
			// Broken, jagged streamers travel around the actual oriented body, above its paint.
			const float Phase = FMath::Frac(Age*1.8f + Ring/3.0f + I*0.7f/32)*4;
			const int32 Side = FMath::FloorToInt(Phase);
			FVector Point = FMath::Lerp(Corners[Side], Corners[(Side+1)%4], Phase-Side);
			Point.Z = FMath::Lerp(Body.Min.Z, Body.Max.Z,
				FMath::Clamp(0.2f+Ring*0.3f+FMath::Sin(I*2.3f+Age*45)*0.1f,0.0f,1.0f));
			if (bPerson)
			{
				// Three broken spiral bolts flicker from feet to head around the visible body.
				const float T = I/32.0f;
				const float Angle = (Age*2.4f + Ring/3.0f + T*1.4f)*2*PI;
				const float Jitter = 1.0f + FMath::Sin(I*2.3f+Age*65)*0.12f;
				Point = Body.GetCenter() + FVector(FMath::Cos(Angle)*Body.GetExtent().X*Jitter,
					FMath::Sin(Angle)*Body.GetExtent().Y*Jitter, FMath::Lerp(-Body.GetExtent().Z,Body.GetExtent().Z,T));
			}
			Point = Transform.TransformPosition(Point);
			if (I > 0) Arcs.Bar(Previous, Point, 1.2f, FLinearColor::White);
			Previous = Point;
		}
	}
	for (FVector2D& UV : Arcs.UV) UV = FVector2D(0.5, 0.5);
	RemoteTaserTrail->CreateMeshSection_LinearColor(1,Arcs.V,Arcs.I,Arcs.N,Arcs.UV,Arcs.C,TArray<FProcMeshTangent>(),false);
	RemoteTaserTrail->SetMaterial(1,TaserTrailMaterialInstance);
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
	// The remote platform translates without swinging its nose-mounted camera.
	// Returning to manual flight explicitly faces the suspect in ToggleRemoteTaser.
	if (!bRemoteTaserActive && !Horizontal.IsNearlyZero()) Facing.Yaw=FMath::FInterpTo(Facing.Yaw,Horizontal.Rotation().Yaw,DeltaSeconds,2.0f);
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
		// Old saves can contain a deployed clamp. Reuse its payload in the replacement cage.
		if (!bCage) bCageOpen=BoatIndex==INDEX_NONE && !SavedCargoNames.ContainsByPredicate([](FName Name){return !Name.IsNone();});
		bCage=true;
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
		if (Index==0 && It->IsCageRecoverableVehicle()) { TowVehicle=*It; It->SetVehicleTowed(true); bCageOpen=false; CageSeal=1; }
		else if (Index>0 && bCage)
		{
			const int32 Slot=CagePeople.Num();
			It->SetCarriedBy(CageAnchor,FVector((Slot%2?1:-1)*28,(Slot/2?1:-1)*28,0),FRotator::ZeroRotator);
			It->SetForcedPedestrianFigureClip(TEXT("NoMo")); CagePeople.Add(*It);
		}
	}
	if (HasBoat()) { bCageOpen=false; CageSeal=1; }
	SavedCargoNames.Reset(); DrawCargo();
}

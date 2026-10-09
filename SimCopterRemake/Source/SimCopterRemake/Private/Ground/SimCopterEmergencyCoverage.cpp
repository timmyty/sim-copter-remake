#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

float ASimCopterTrafficSystemActor::RoadLinkTravelSeconds(int32 From, int32 To) const
{
	// Conservative planning speed reserves 40% for bends/acceleration. Each waypoint
	// also includes 0.12 seconds of scheduling/turning time, including at low frame rates.
	return FVector::Distance(RoadNodes[From].Location, RoadNodes[To].Location) /
		(SimCopterDispatch::EmergencySpeedCmPerSec * 0.6f) + 0.12f;
}

float ASimCopterTrafficSystemActor::RoadRouteTravelSeconds(const TArray<int32>& Route) const
{
	float Seconds = 0;
	for (int32 I = 1; I < Route.Num(); ++I) Seconds += RoadLinkTravelSeconds(Route[I-1], Route[I]);
	return Seconds;
}

void ASimCopterTrafficSystemActor::GetRoadRoutingNeighbors(int32 Node, TArray<int32>& Out) const
{
	Out = RoadNodes[Node].Neighbors;
	for (int32 Approach : RoadNodes[Node].Neighbors)
	{
		int32 Exit, ExitRoad;
		if (FindLinkedTunnelExit(Node, Approach, Exit, ExitRoad)) Out.AddUnique(Exit);
	}
}

void ASimCopterTrafficSystemActor::EnsureEmergencyRoadCoverage()
{
	for (AActor* Depot : EmergencyDepots) if (IsValid(Depot)) Depot->Destroy();
	EmergencyDepots.Reset();
	if (RoadNodes.IsEmpty()) return;
	// Real roads remain the source of routing. Each isolated component gets its own
	// local headquarters; no invisible links drive responders through buildings or water.
	for (int32 Service = 0; Service < int32(SimCopterDispatch::EService::Count); ++Service)
	{
		TArray<float> Travel;
		Travel.Init(TNumericLimits<float>::Max(), RoadNodes.Num());
		struct FVisit { float Seconds; int32 Node; };
		const auto Earlier = [](const FVisit& A, const FVisit& B) { return A.Seconds < B.Seconds; };
		auto AddCoverage = [&](int32 Origin)
		{
			TArray<FVisit> Queue;
			Travel[Origin] = 0;
			Queue.HeapPush({0, Origin}, Earlier);
			TArray<int32> Neighbors;
			while (!Queue.IsEmpty())
			{
				FVisit Current;
				Queue.HeapPop(Current, Earlier, EAllowShrinking::No);
				if (Current.Seconds > Travel[Current.Node]) continue;
				GetRoadRoutingNeighbors(Current.Node, Neighbors);
				for (int32 Next : Neighbors)
				{
					const float Cost = Current.Seconds + RoadLinkTravelSeconds(Current.Node, Next);
					if (Cost < Travel[Next])
					{
						Travel[Next] = Cost;
						Queue.HeapPush({Cost, Next}, Earlier);
					}
				}
			}
		};
		for (const auto& Station : DispatchStations[Service])
			if (const int32* Index = RoadNodeIndexByTile.Find(Station.RoadTile)) AddCoverage(*Index);
		for (;;)
		{
			int32 Uncovered = INDEX_NONE;
			float Worst = SimCopterDispatch::CoverageTravelSeconds;
			for (int32 I = 0; I < Travel.Num(); ++I)
				if (Travel[I] > Worst) { Worst = Travel[I]; Uncovered = I; }
			if (Uncovered == INDEX_NONE) break;
			const auto& Node = RoadNodes[Uncovered];
			SimCopterDispatch::FStation Station;
			Station.Service = static_cast<SimCopterDispatch::EService>(Service);
			Station.Tile = Station.RoadTile = FIntPoint(Node.FileX, Node.FileY);
			DispatchStations[Service].Add(Station);
			AddCoverage(Uncovered);

			// Compact, marked roadside service garages. Offsetting to the road shoulder
			// keeps the launch tile clear; no map-specific assets or saved-city edits.
			if (!GetWorld()) continue;
			AActor* Depot = GetWorld()->SpawnActor<AActor>();
			if (!Depot) continue;
			EmergencyDepots.Add(Depot);
			Depot->SetOwner(this);
			auto* Root = NewObject<USceneComponent>(Depot);
			Depot->SetRootComponent(Root); Depot->AddInstanceComponent(Root); Root->RegisterComponent();
			FVector Along = Node.Neighbors.IsEmpty() ? FVector::ForwardVector :
				(RoadNodes[Node.Neighbors[0]].Location - Node.Location).GetSafeNormal2D();
			const FVector Shoulder = FVector::CrossProduct(Along, FVector::UpVector);
			Depot->SetActorLocation(Node.Location + Shoulder * (ActiveTileSize * 0.44f) + Along * ((Service - 1) * 95));
			Depot->SetActorRotation(Shoulder.Rotation());
			auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			const FLinearColor Color = Service == 0 ? FLinearColor(0.65f,0.04f,0.02f) :
				Service == 1 ? FLinearColor(0.03f,0.12f,0.65f) : FLinearColor(0.85f,0.85f,0.8f);
			auto Box = [&](FVector Location, FVector Size, FLinearColor Tint)
			{
				auto* Mesh = NewObject<UStaticMeshComponent>(Depot);
				Depot->AddInstanceComponent(Mesh); Mesh->SetupAttachment(Root);
				Mesh->SetStaticMesh(Cube); Mesh->SetRelativeLocation(Location); Mesh->SetRelativeScale3D(Size / 100);
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
				if (auto* Mat = Mesh->CreateAndSetMaterialInstanceDynamic(0)) Mat->SetVectorParameterValue(TEXT("Color"), Tint);
			};
			Box(FVector(0,0,28), FVector(75,84,56), Color);
			Box(FVector(-38,0,20), FVector(2,62,38), FLinearColor(0.12f,0.14f,0.16f));
			Box(FVector(0,0,58), FVector(82,90,6), FLinearColor(0.18f,0.2f,0.23f));
			auto* Sign = NewObject<UTextRenderComponent>(Depot);
			Depot->AddInstanceComponent(Sign); Sign->SetupAttachment(Root);
			Sign->SetRelativeLocation(FVector(-40,0,65)); Sign->SetRelativeRotation(FRotator(0,180,0));
			Sign->SetHorizontalAlignment(EHTA_Center); Sign->SetWorldSize(12);
			Sign->SetText(FText::FromString(Service == 0 ? TEXT("FIRE HQ") : Service == 1 ? TEXT("POLICE HQ") : TEXT("EMS HQ")));
			Sign->RegisterComponent();
		}
	}
}

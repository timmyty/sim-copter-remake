#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterBaseballGame.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Engine/World.h"

void ASimCopterTrafficSystemActor::ClearBaseballGames()
{
	for (auto& Pair : BaseballGames) if (auto* Game=Pair.Value.Get()) { Game->Stop(); Game->Destroy(); }
	BaseballGames.Reset();
}

void ASimCopterTrafficSystemActor::PruneBaseballGames()
{
	const FVector Focus=GetPopulationFocusLocation();
	for (auto It=BaseballGames.CreateIterator(); It; ++It)
	{
		auto* Game=It.Value().Get();
		const bool bValidField=PedestrianNodes.IsValidIndex(It.Key()) &&
			GetXbldTileId(PedestrianNodes[It.Key()].FileX,PedestrianNodes[It.Key()].FileY)==0xd7;
		if (!Game || !bPlayFullBaseballGames || !bValidField ||
			FVector::DistSquared2D(Game->GetActorLocation(),Focus)>FMath::Square(12*ActiveTileSize))
		{
			if (Game) { Game->Stop(); Game->Destroy(); }
			It.RemoveCurrent();
		}
	}
}

int32 ASimCopterTrafficSystemActor::EnsureBaseballGame(int32 TileX, int32 TileY, int32& AttemptsRemaining)
{
	int32 NodeIndex=INDEX_NONE;
	if (!TryResolvePedestrianNodeForTile(TileX,TileY,NodeIndex)) return 0;
	const auto& Node=PedestrianNodes[NodeIndex];
	if (FVector::DistSquared2D(Node.Location,GetPopulationFocusLocation())>
		FMath::Square(FMath::Max(1,OriginalAmbientScanRadiusTiles)*ActiveTileSize)) return 0;
	if (auto* Existing = BaseballGames.FindRef(NodeIndex).Get())
	{
		if (!Existing->bPopulationRosterPending) return 0;
		const int32 Before = PedestrianAgents.Num();
		Existing->SyncPlayers();
		const int32 Created = PedestrianAgents.Num() - Before;
		AttemptsRemaining = FMath::Max(0, AttemptsRemaining - FMath::Max(1, Created));
		return Created;
	}
	if (BaseballGames.Num()>=2) return 0;
	// An old save can contain retail players. Retire only healthy ambient members of this
	// exact field. Patient/mission identities survive, and defer play while on the diamond.
	TArray<ASimCopterGroundAgent*> Legacy;
	for (auto& Ptr : PedestrianAgents) if (auto* P=Ptr.Get(); P && P->GetRouteTargetNode()==NodeIndex &&
		(P->InitialBehaviorProgramId==1205 || P->InitialBehaviorProgramId==1206))
	{
		if (!ASimCopterBaseballGame::CanPlay(P))
		{
			if (!P->IsMissionCarried() && !P->GetBehaviorCarrier() &&
				FVector::DistSquared2D(P->GetActorLocation(),Node.Location)<FMath::Square(ActiveTileSize*2)) return 0;
			continue;
		}
		Legacy.Add(P);
	}
	if (PedestrianAgents.Num()-Legacy.Num()+13>MaxPedestrianAgents) return 0;
	for (auto* P : Legacy) { PedestrianAgents.Remove(P); P->Destroy(); }
	FActorSpawnParameters Params; Params.Owner=this;
	auto* Game=GetWorld()->SpawnActor<ASimCopterBaseballGame>(Params);
	if (!Game) return 0;
	const int32 Before=PedestrianAgents.Num();
	BaseballGames.Add(NodeIndex,Game); Game->Initialize(this,NodeIndex);
	const int32 Created=PedestrianAgents.Num()-Before;
	AttemptsRemaining=FMath::Max(0,AttemptsRemaining-FMath::Max(1,Created));
	return Created;
}

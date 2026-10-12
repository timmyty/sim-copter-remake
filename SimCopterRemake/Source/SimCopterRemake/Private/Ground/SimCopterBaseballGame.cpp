#include "Ground/SimCopterBaseballGame.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterPopulationFigure.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

namespace
{
// Same diamond orientation as retail SpawnSpecialBuildingPeople (004c25b0).
const FVector2D Bases[] = {{70,-70}, {20,-70}, {20,-20}, {70,-20}, {70,-70}};
const FVector2D Fielders[] = {{45,-45}, {81,-81}, {13,-76}, {5,-12}, {47,1}, {77,-12}, {-45,-52}, {-35,35}, {48,48}};
// 5man's jersey uses color ramp 1. Offsets 3/6 select the original red/blue
// ramps (4/7); the old offsets 1/6 selected two similar blue jerseys (2/7).
constexpr int32 TeamClothes[] = {3,6};
// A low-poly sphere and tapered wooden bat; no cooked asset or physics body.
void BuildProp(UProceduralMeshComponent* Mesh, bool bBall)
{
	TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV;
	TArray<FLinearColor> C; TArray<FProcMeshTangent> Tangents;
	const auto Point=[bBall](int32 Ring,int32 Slice)
	{
		const double Angle=Slice*UE_TWO_PI/12;
		const double Radius=bBall ? 3*FMath::Cos(-UE_HALF_PI+Ring*UE_PI/8) : Ring==0 || Ring==8 ? 0 : Ring<4 ? 1.2 : 2.6;
		const double X=bBall ? 3*FMath::Sin(-UE_HALF_PI+Ring*UE_PI/8) : Ring*4;
		return FVector(X,Radius*FMath::Cos(Angle),Radius*FMath::Sin(Angle));
	};
	for (int32 Ring=0; Ring<8; ++Ring) for (int32 Slice=0; Slice<12; ++Slice)
	{
		const FVector A=Point(Ring,Slice), B=Point(Ring,Slice+1), D=Point(Ring+1,Slice), E=Point(Ring+1,Slice+1);
		const FVector Normal=FVector::CrossProduct(B-A,E-A).GetSafeNormal();
		const int32 Start=V.Num();
		for (const FVector& P : {A,B,E,D})
		{
			V.Add(P); N.Add(Normal); UV.Add(FVector2D::ZeroVector);
			C.Add(bBall ? FLinearColor::White : FLinearColor(.6f,.32f,.1f));
			Tangents.Add(FProcMeshTangent((B-A).GetSafeNormal(),false));
		}
		// Unreal's visible side winds clockwise, opposite the right-hand-rule normal.
		T.Append({Start,Start+2,Start+1,Start,Start+3,Start+2});
	}
	Mesh->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tangents,false);
	Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_SimCopterLitVertexColor.M_SimCopterLitVertexColor")));
}

const TCHAR* PlayName(ESimCopterBaseballPlay Play)
{
	switch (Play)
	{
	case ESimCopterBaseballPlay::Ball: return TEXT("BALL");
	case ESimCopterBaseballPlay::Strike: return TEXT("STRIKE");
	case ESimCopterBaseballPlay::Foul: return TEXT("FOUL");
	case ESimCopterBaseballPlay::Single: return TEXT("SINGLE");
	case ESimCopterBaseballPlay::Double: return TEXT("DOUBLE");
	case ESimCopterBaseballPlay::Triple: return TEXT("TRIPLE");
	case ESimCopterBaseballPlay::HomeRun: return TEXT("HOME RUN");
	case ESimCopterBaseballPlay::GroundOut: return TEXT("OUT AT FIRST");
	default: return TEXT("CAUGHT - OUT");
	}
}
}

ASimCopterBaseballGame::ASimCopterBaseballGame()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f / 15; // one bounded decision loop per active field
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Field")));
	Ball = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Baseball")); Ball->SetupAttachment(RootComponent);
	Bat = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Bat")); Bat->SetupAttachment(RootComponent);
	for (auto* Mesh : {Ball.Get(), Bat.Get()})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false); Mesh->SetCastShadow(false);
	}
	BoardRoot=CreateDefaultSubobject<USceneComponent>(TEXT("ScoreboardRoot")); BoardRoot->SetupAttachment(RootComponent);
	BoardBacking=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ScoreboardBacking")); BoardBacking->SetupAttachment(BoardRoot);
	BoardBacking->SetCollisionEnabled(ECollisionEnabled::NoCollision); BoardBacking->SetCanEverAffectNavigation(false);
	BoardLamps=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ScoreboardLamps")); BoardLamps->SetupAttachment(BoardRoot);
	BoardLamps->SetCollisionEnabled(ECollisionEnabled::NoCollision); BoardLamps->SetCanEverAffectNavigation(false); BoardLamps->SetCastShadow(false);
}

bool ASimCopterBaseballGame::CanPlay(const ASimCopterGroundAgent* P)
{
	return IsValid(P) && !P->IsActorBeingDestroyed() && !P->IsHidden() &&
		P->MissionEventId == INDEX_NONE && P->GetReplayPersonState() == 0 &&
		!P->IsMissionCarried() && !P->GetBehaviorCarrier() && !P->IsKnockedDown() &&
		!P->IsMedevacVictim() && !P->IsMissionPatientDead();
}

void ASimCopterBaseballGame::Initialize(ASimCopterTrafficSystemActor* InTraffic, int32 InNode)
{
	Traffic = InTraffic; NodeIndex = InNode;
	SetActorLocation(InTraffic->PedestrianNodes[InNode].Location);
	Random.Initialize(InTraffic->RandomSeed + InNode * 719 + int32(GetWorld()->GetTimeSeconds()));
	BuildProp(Ball,true); BuildProp(Bat,false);
	Ball->SetVisibility(false); Bat->SetVisibility(false);
	InitializeScoreboard();
	Announcement = TEXT("PLAY BALL");
	SyncPlayers(); SetFormation(); UpdateBoard();
}

FVector ASimCopterBaseballGame::Point(FVector2D Offset, float Height) const
{
	const auto* T = Traffic.Get();
	return GetActorLocation() + (T ? T->ActiveCityToWorldTransform.TransformVector(
		FVector(Offset.X,Offset.Y,0) * (T->ActiveTileSize/64.f)) : FVector::ZeroVector) + FVector(0,0,Height);
}
FVector ASimCopterBaseballGame::Base(int32 Index) const { return Point(Bases[FMath::Clamp(Index,0,4)]); }
FVector ASimCopterBaseballGame::Defense(int32 Slot) const { return Point(Fielders[Slot]); }

void ASimCopterBaseballGame::Retire(int32 Player)
{
	if (auto* P = Players[Player].Get())
	{
		P->BaseballGame.Reset();
		// A hurt/carried/mission-owned person is real gameplay state, never a disposable prop.
		if (CanPlay(P))
		{
			if (auto* T = Traffic.Get()) T->PedestrianAgents.Remove(P);
			P->Destroy();
		}
	}
	Players[Player].Reset();
}

void ASimCopterBaseballGame::Stop()
{
	for (int32 I=0; I<18; ++I) Retire(I);
	Ball->SetVisibility(false); Bat->SetVisibility(false);
}
void ASimCopterBaseballGame::EndPlay(const EEndPlayReason::Type Reason) { Stop(); Super::EndPlay(Reason); }

bool ASimCopterBaseballGame::SyncPlayers()
{
	auto* T = Traffic.Get(); if (!T) return false;
	bool Needed[18] = {};
	for (int32 I=0; I<9; ++I) Needed[(1-Score.BattingTeam)*9+I] = true;
	Needed[Score.Batter()] = true;
	for (int32 Runner : Score.Bases) if (Runner != INDEX_NONE) Needed[Runner] = true;
	for (int32 I=0; I<18; ++I) if (!Needed[I]) Retire(I);
	bool bComplete = true;
	for (int32 I=0; I<18; ++I)
	{
		if (!Needed[I]) continue;
		if (Players[I].IsValid())
		{
			if (!CanPlay(Players[I].Get())) bComplete = false;
			else Players[I]->BaseballGame = this;
			continue;
		}
		FVector2D Offset = I/9 != Score.BattingTeam ? Fielders[I%9] : Bases[0]+FVector2D(4,5);
		for (int32 B=0; B<3; ++B) if (Score.Bases[B]==I) Offset = Bases[B+1];
		const auto& Node = T->PedestrianNodes[NodeIndex];
		if (!T->TrySpawnOriginalPersonAtTile(Node.FileX,Node.FileY,4,0,
			I/9==Score.BattingTeam ? 1205 : 1206,&Offset,TeamClothes[I/9])) { bComplete=false; continue; }
		auto* P = T->PedestrianAgents.Last().Get();
		Players[I] = P; P->BaseballGame = this;
		P->BaseballTarget = P->GetActorLocation(); P->BaseballFacing = 0;
		P->AddTickPrerequisiteActor(this);
	}
	return bComplete;
}

void ASimCopterBaseballGame::SetGoal(int32 Player, const FVector& Location, const FVector& Facing, const TCHAR* Clip, float Speed)
{
	if (auto* P = Players[Player].Get())
	{
		P->BaseballTarget = Location;
		P->BaseballFacing = (Facing-Location).Rotation().Yaw;
		P->BaseballClip = Clip; P->BaseballSpeed = Speed;
	}
}

bool ASimCopterBaseballGame::SetFormation()
{
	for (int32 I=0; I<9; ++I) SetGoal((1-Score.BattingTeam)*9+I,Defense(I),Base(0));
	SetGoal(Score.Batter(),Point(Bases[0]+FVector2D(4,5)),Defense(0),TEXT("HipH"));
	for (int32 I=0; I<3; ++I) if (Score.Bases[I]!=INDEX_NONE) SetGoal(Score.Bases[I],Base(I+1),Base(I+2));
	for (const auto& Player : Players)
		if (auto* P=Player.Get(); P && FVector::DistSquared2D(P->GetActorLocation(),P->BaseballTarget)>FMath::Square(25.f)) return false;
	return true;
}

void ASimCopterBaseballGame::SetPhase(EPhase Next) { Phase=Next; PhaseAge=0; }

void ASimCopterBaseballGame::StartRunners()
{
	for (int32 I=0; I<18; ++I) RunNext[I]=RunEnd[I]=0;
	const bool bWalk = Play==ESimCopterBaseballPlay::Ball && Score.Balls==3;
	const bool bHit = Play>=ESimCopterBaseballPlay::Single && Play<=ESimCopterBaseballPlay::HomeRun;
	const int32 Advance = bHit ? int32(Play)-int32(ESimCopterBaseballPlay::Single)+1 : 1;
	if (bHit || bWalk || Play==ESimCopterBaseballPlay::GroundOut)
	{
		RunNext[Score.Batter()]=1; RunEnd[Score.Batter()]=Advance;
	}
	bool bForced = bWalk;
	for (int32 B=0; B<3; ++B)
	{
		const int32 Runner = Score.Bases[B];
		if (Runner==INDEX_NONE) { bForced=false; continue; }
		if (bHit || bForced) { RunNext[Runner]=B+2; RunEnd[Runner]=FMath::Min(4,B+1+Advance); }
	}
}

bool ASimCopterBaseballGame::MoveRunners()
{
	bool bDone=true;
	for (int32 I=0; I<18; ++I)
	{
		if (RunNext[I]==0 || RunNext[I]>RunEnd[I]) continue;
		auto* P=Players[I].Get(); if (!P) { bDone=false; continue; }
		if (FVector::DistSquared2D(P->GetActorLocation(),Base(RunNext[I]))<FMath::Square(16.f)) ++RunNext[I];
		if (RunNext[I]<=RunEnd[I])
		{
			SetGoal(I,Base(RunNext[I]),Base(FMath::Min(4,RunNext[I]+1)),TEXT("NoMo"),
				Play==ESimCopterBaseballPlay::GroundOut ? 85 : 180);
			bDone=false;
		}
	}
	return bDone;
}

void ASimCopterBaseballGame::FinishPlay()
{
	Announcement=PlayName(Play);
	if (Play==ESimCopterBaseballPlay::Strike && Score.Strikes==2) Announcement=TEXT("STRIKEOUT");
	if (Play==ESimCopterBaseballPlay::Ball && Score.Balls==3) Announcement=TEXT("WALK");
	const int32 OldTeam=Score.BattingTeam;
	Score.Apply(Play);
	if (Score.bFinal) Announcement=Score.Runs[0]>Score.Runs[1] ? TEXT("VISITORS WIN") : TEXT("HOME WINS");
	else if (OldTeam!=Score.BattingTeam) Announcement=TEXT("THREE OUTS - CHANGE SIDES");
	Ball->SetVisibility(false); Bat->SetVisibility(false);
	SetPhase(Score.bFinal ? EPhase::Final : EPhase::Result); UpdateBoard();
}

void ASimCopterBaseballGame::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	auto* T=Traffic.Get(); if (!T || !T->PedestrianNodes.IsValidIndex(NodeIndex)) { Stop(); Destroy(); return; }
	const float Dt=FMath::Clamp(DeltaSeconds,0.f,.25f); Age+=Dt; PhaseAge+=Dt;
	UpdateScoreboardLighting();
	// Medical/fire/carry state always owns a person first. Wait until a casualty has left
	// this field; do not clone a healthy replacement on top of somebody awaiting rescue.
	for (int32 I=0; I<18; ++I) if (auto* P=Players[I].Get(); P && !CanPlay(P))
	{
		P->BaseballGame.Reset();
		if (FVector::DistSquared2D(P->GetActorLocation(),GetActorLocation())>FMath::Square(T->ActiveTileSize*2) || P->IsMissionCarried())
			Players[I].Reset();
		else
		{
			for (auto& Other : Players) if (auto* Q=Other.Get(); Q && Q!=P) Q->BaseballTarget=Q->GetActorLocation();
			Announcement=TEXT("TIME - FIELD INTERRUPTED"); UpdateBoard();
			Ball->SetVisibility(false); Bat->SetVisibility(false); SetPhase(EPhase::Ready); return;
		}
	}
	if (Phase==EPhase::Ready)
	{
		if (Age<RetrySeconds) return;
		if (!SyncPlayers()) { RetrySeconds=Age+1; Announcement=TEXT("WAITING FOR PLAYERS"); UpdateBoard(); return; }
		if (!SetFormation() || PhaseAge<2) return;
		Announcement=TEXT("PITCHING"); UpdateBoard();
		SetPhase(EPhase::Windup);
	}
	const int32 Pitcher=(1-Score.BattingTeam)*9;
	if (Phase==EPhase::Windup)
	{
		SetGoal(Pitcher,Defense(0),Base(0),TEXT("WvNo"));
		Ball->SetVisibility(true); Ball->SetWorldLocation(Defense(0)+FVector(0,0,32));
		if (PhaseAge>=.85f)
		{
			const int32 Roll=Random.RandRange(0,99);
			Play=Roll<25 ? ESimCopterBaseballPlay::Ball : Roll<47 ? ESimCopterBaseballPlay::Strike :
				Roll<62 ? ESimCopterBaseballPlay::Foul : Roll<76 ? ESimCopterBaseballPlay::GroundOut :
				Roll<85 ? ESimCopterBaseballPlay::FlyOut : Roll<95 ? ESimCopterBaseballPlay::Single :
				Roll<98 ? ESimCopterBaseballPlay::Double : Roll==98 ? ESimCopterBaseballPlay::Triple : ESimCopterBaseballPlay::HomeRun;
			BallStart=Defense(0)+FVector(0,0,32); BallEnd=Base(0)+FVector(0,0,25);
			SetPhase(EPhase::Pitch);
		}
	}
	if (Phase==EPhase::Pitch)
	{
		Ball->SetWorldLocation(FMath::Lerp(BallStart,BallEnd,FMath::Clamp(PhaseAge/.7f,0.f,1.f)));
		if (PhaseAge>=.7f)
		{
			StartRunners(); bBallCaught=false;
			BallStart=BallEnd;
			if (Play<=ESimCopterBaseballPlay::Foul)
			{
				BallEnd=Play==ESimCopterBaseballPlay::Foul ? Point({85,-45},30) : Defense(1)+FVector(0,0,25);
				SetPhase(EPhase::Return);
			}
			else
			{
				const FVector2D Landing=Play==ESimCopterBaseballPlay::GroundOut ? FVector2D(35,-8) :
					Play==ESimCopterBaseballPlay::HomeRun ? FVector2D(-85,75) : FVector2D(Random.FRandRange(-45,15),Random.FRandRange(0,50));
				BallEnd=Point(Landing,Play==ESimCopterBaseballPlay::FlyOut ? 30 : 5);
				Fielder=Pitcher+2;
				for (int32 I=3; I<9; ++I) if (FVector::DistSquared2D(Defense(I),BallEnd)<FVector::DistSquared2D(Defense(Fielder%9),BallEnd)) Fielder=Pitcher+I;
				SetGoal(Fielder,BallEnd,Base(0),TEXT("WvNo"),210);
				SetPhase(EPhase::InPlay);
			}
		}
	}
	if (Phase==EPhase::InPlay)
	{
		const bool bRunnersDone=MoveRunners();
		const float Flight=Play==ESimCopterBaseballPlay::GroundOut ? .85f : 2.5f;
		const float Alpha=FMath::Clamp(PhaseAge/Flight,0.f,1.f);
		const float Arc=Play==ESimCopterBaseballPlay::GroundOut ? 8 : 160;
		Ball->SetWorldLocation(FMath::Lerp(BallStart,BallEnd,Alpha)+FVector(0,0,4*Arc*Alpha*(1-Alpha)));
		if (PhaseAge>=Flight)
		{
			if (Play==ESimCopterBaseballPlay::HomeRun) { if (bRunnersDone) FinishPlay(); }
			else if (auto* P=Players[Fielder].Get(); P && FVector::DistSquared2D(P->GetActorLocation(),BallEnd)<FMath::Square(25.f))
			{
				bBallCaught=true;
				if (Play==ESimCopterBaseballPlay::FlyOut) FinishPlay();
				else
				{
					SetGoal(Fielder,P->GetActorLocation(),Play==ESimCopterBaseballPlay::GroundOut ? Base(1) : Defense(0),TEXT("WvNo"));
					BallStart=BallEnd+FVector(0,0,25);
					BallEnd=(Play==ESimCopterBaseballPlay::GroundOut ? Base(1) : Defense(0))+FVector(0,0,25);
					if (Play==ESimCopterBaseballPlay::GroundOut) SetGoal(Pitcher+2,Base(1),BallStart,TEXT("WvNo"));
					SetPhase(EPhase::Return);
				}
			}
		}
	}
	if (Phase==EPhase::Return)
	{
		const bool bRunnersDone=MoveRunners();
		const float Alpha=FMath::Clamp(PhaseAge/.75f,0.f,1.f);
		Ball->SetWorldLocation(FMath::Lerp(BallStart,BallEnd,Alpha)+FVector(0,0,30*4*Alpha*(1-Alpha)));
		if (PhaseAge>=.8f && (bRunnersDone || Play==ESimCopterBaseballPlay::GroundOut)) FinishPlay();
	}
	if ((Phase==EPhase::InPlay || Phase==EPhase::Return) && PhaseAge>14)
	{
		// A landed aircraft or obstruction must not award runs to runners who never arrived.
		Announcement=TEXT("TIME - REPLAY PITCH"); Ball->SetVisibility(false); SetPhase(EPhase::Ready); UpdateBoard();
	}
	if (Phase==EPhase::Result && PhaseAge>2) { SyncPlayers(); SetPhase(EPhase::Ready); }
	if (Phase==EPhase::Final && PhaseAge>15) { Score=FSimCopterBaseballScore(); Announcement=TEXT("NEW GAME"); SetPhase(EPhase::Ready); UpdateBoard(); }
	// The bat follows the active batter, with a short contact sweep. Benched players have
	// no actors, meshes, collision components or behavior ticks.
	const bool bAtBat=Phase==EPhase::Ready || Phase==EPhase::Windup || Phase==EPhase::Pitch;
	Bat->SetVisibility(bAtBat);
	if (auto* P=Players[Score.Batter()].Get(); P && bAtBat)
	{
		const float Swing=Phase==EPhase::Pitch && Play!=ESimCopterBaseballPlay::Ball ? FMath::Clamp((PhaseAge-.42f)/.28f,0.f,1.f) : 0;
		const FRotator Rotation(0,P->GetActorRotation().Yaw+100-170*Swing,0);
		Bat->SetWorldLocation(P->GetActorLocation()+FVector(0,0,29-P->GetCapsuleHalfHeightCm())+P->GetActorForwardVector()*9);
		Bat->SetWorldRotation(Rotation);
		if (Swing>0) P->BaseballClip=TEXT("2Gab");
	}
}

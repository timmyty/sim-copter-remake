#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterBaseballGame.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"

bool ASimCopterGroundAgent::IsPlayingBaseball() const
{
	return BaseballGame.IsValid() && ASimCopterBaseballGame::CanPlay(this);
}

bool ASimCopterGroundAgent::UpdateBaseball(float DeltaSeconds)
{
	if (!IsPlayingBaseball()) return false;
	const float Dt = FMath::Clamp(DeltaSeconds,0.f,.1f);
	FVector Delta = BaseballTarget-GetActorLocation(); Delta.Z=0;
	const float Distance = Delta.Size();
	const FVector Before = GetActorLocation();
	if (Distance>3)
	{
		const FVector Step = Delta.GetSafeNormal()*FMath::Min(Distance,BaseballSpeed*Dt);
		MoveAgainstCityGeometry(Step,Step.Rotation().Quaternion());
	}
	else SetActorRotation(FRotator(0,BaseballFacing,0));
	ContainOutsideBuildingGeometry();
	if (bSnapToGround) UpdateGroundSnap(Dt);
	CurrentVelocityCmPerSec = Dt>SMALL_NUMBER ? (GetActorLocation()-Before)/Dt : FVector::ZeroVector;
	BehaviorStepVelocityCmPerSec=FVector::ZeroVector; BehaviorStepTimeRemainingSeconds=0;
	// Reuse decoded figure clips without advancing retail's unrelated pantomime VM.
	const bool bMoving=CurrentVelocityCmPerSec.SizeSquared2D()>1;
	const FString Clip=bMoving ? TEXT("1Wal") : BaseballClip;
	if (FigureMnemonic!=Clip) RebuildFigureClip(Clip);
	if (FigureFrameCount>1)
	{
		FigureFrameTime+=Dt*FigureFrameRate;
		const int32 Frame=FMath::FloorToInt(FigureFrameTime)%FigureFrameCount;
		if (Frame!=FigureCurrentFrame)
		{
			FigureCurrentFrame=Frame;
			FSimCopterPopulationFigure::ShowFrame(OriginalMeshComponent,FigureFrameCount,Frame,bFigureHasHeadSection);
		}
	}
	VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);
	UpdateWalkingVoice(bMoving ? 15 : 0);
	UpdateWaterSubmersion(Dt);
	return true;
}

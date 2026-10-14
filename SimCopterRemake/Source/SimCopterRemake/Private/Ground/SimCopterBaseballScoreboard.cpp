#include "Ground/SimCopterBaseballGame.h"
#include "City/SimCopterEffectExposure.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

namespace
{
// Requested stadium prop, not a retail scoreboard port. Local +X faces home plate;
// +Y is the reader's left. All geometry stays fixed in the stadium's coordinate frame.
constexpr float CabinetHeight=310;
const FLinearColor Paint(.64f,.60f,.43f), Steel(.065f,.085f,.072f), Dark(.009f,.013f,.011f);
const FLinearColor Amber(1.f,.38f,.035f);

struct FLampField { float Y,Z,Pitch; int32 Slots; };
const FLampField Fields[] = {
	{200,34,9,2}, {-200,34,9,2}, {0,34,6,2}, // visitors, home, inning
	{180,-69,5.5f,1}, {0,-69,5.5f,1}, {-180,-69,5.5f,1}, // ball, strike, out
	{0,-5,2.5f,3}, {0,-122,3.8f,26} // half inning, message ribbon
};

// A small 5x7 sign alphabet. The same stencils paint the cabinet labels and select
// incandescent bulbs, avoiding a modern UI font or a per-character component.
const TCHAR* Alphabet=TEXT("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-/");
const uint8 Glyphs[][7] = {
	{14,17,17,17,17,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
	{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
	{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
	{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
	{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
	{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
	{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
	{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
	{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
	{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
	{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
	{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},{0,0,0,31,0,0,0},{1,2,2,4,8,8,16}
};

struct FBoardMesh
{
	TArray<FVector> V,N;
	TArray<int32> T;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	TArray<FProcMeshTangent> Tangents;
	void Face(FVector A,FVector B,FVector D,FVector E,FLinearColor Color)
	{
		const int32 Start=V.Num();
		const FVector Normal=FVector::CrossProduct(B-A,D-A).GetSafeNormal();
		for (const FVector& P : {A,B,D,E})
		{
			V.Add(P); N.Add(Normal); UV.Add(FVector2D::ZeroVector); C.Add(Color);
			Tangents.Add(FProcMeshTangent((B-A).GetSafeNormal(),false));
		}
		T.Append({Start,Start+2,Start+1,Start,Start+3,Start+2});
	}
	void Box(FVector Center,FVector Extent,FLinearColor Color,FQuat Rotation=FQuat::Identity)
	{
		auto P=[&](float X,float Y,float Z) { return Center+Rotation.RotateVector(Extent*FVector(X,Y,Z)); };
		Face(P(1,-1,-1),P(1,1,-1),P(1,1,1),P(1,-1,1),Color);
		Face(P(-1,1,-1),P(-1,-1,-1),P(-1,-1,1),P(-1,1,1),Color*.7f);
		Face(P(1,1,-1),P(-1,1,-1),P(-1,1,1),P(1,1,1),Color*.8f);
		Face(P(-1,-1,-1),P(1,-1,-1),P(1,-1,1),P(-1,-1,1),Color*.8f);
		Face(P(-1,-1,1),P(1,-1,1),P(1,1,1),P(-1,1,1),Color);
		Face(P(-1,1,-1),P(1,1,-1),P(1,-1,-1),P(-1,-1,-1),Color*.6f);
	}
	void Rect(float X,float Left,float Bottom,float Right,float Top,FLinearColor Color)
	{
		Face({X,Left,Bottom+CabinetHeight},{X,Right,Bottom+CabinetHeight},
			{X,Right,Top+CabinetHeight},{X,Left,Top+CabinetHeight},Color);
	}
	void Bulb(float X,float Y,float Z,float Radius,FLinearColor Color)
	{
		// Octagonal lenses, all in one mesh section; no light actors or dynamic shadows.
		const int32 Start=V.Num();
		for (int32 I=0; I<8; ++I)
		{
			const float A=I*UE_TWO_PI/8;
			V.Add({X,Y+Radius*FMath::Cos(A),CabinetHeight+Z+Radius*FMath::Sin(A)});
			N.Add(FVector::ForwardVector); UV.Add(FVector2D::ZeroVector); C.Add(Color);
			Tangents.Add(FProcMeshTangent(0,1,0));
		}
		for (int32 I=1; I<7; ++I) T.Append({Start,Start+I+1,Start+I});
	}
	void Lettering(const FString& Text,float Y,float Z,float Pitch,FLinearColor Color,bool bLamps)
	{
		const float Left=Y+(Text.Len()*6-1)*Pitch/2;
		for (int32 I=0; I<Text.Len(); ++I)
		{
			const TCHAR* Glyph=FCString::Strchr(Alphabet,Text[I]); if (!Glyph) continue;
			for (int32 Row=0; Row<7; ++Row) for (int32 Col=0; Col<5; ++Col)
			{
				if (!(Glyphs[Glyph-Alphabet][Row] & (1<<(4-Col)))) continue;
				const float PY=Left-(I*6+Col+.5f)*Pitch, PZ=Z+(3-Row)*Pitch;
				if (bLamps) Bulb(5.5f,PY,PZ,Pitch*(Pitch<6 ? .45f : .33f),Color);
				else Rect(4.5f,PY-Pitch*.48f,PZ-Pitch*.48f,PY+Pitch*.48f,PZ+Pitch*.48f,Color);
			}
		}
	}
	void Apply(UProceduralMeshComponent* Mesh,int32 Section)
	{
		Mesh->CreateMeshSection_LinearColor(Section,V,T,N,UV,C,Tangents,false);
	}
};
}

void ASimCopterBaseballGame::InitializeScoreboard()
{
	BoardRoot->SetWorldLocation(Point({-60,60}));
	const FVector ToPlate=Base(0)-BoardRoot->GetComponentLocation();
	BoardRoot->SetWorldRotation(FRotator(0,ToPlate.Rotation().Yaw,0));
	FBoardMesh M;
	// Support columns meet the actual field/wall surface, including sloping city terrain.
	for (float Y : {-240.f,240.f})
	{
		const FVector Foot=BoardRoot->GetComponentTransform().TransformPosition({-18,Y,0});
		FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(BaseballScoreboardFooting),true,this);
		float Ground=0;
		if (GetWorld()->LineTraceSingleByChannel(Hit,Foot+FVector(0,0,120),Foot-FVector(0,0,1200),ECC_Visibility,Query))
			Ground=Hit.ImpactPoint.Z-Foot.Z;
		M.Box({-18,Y,Ground+5},{29,26,7},FLinearColor(.25f,.24f,.2f));
		M.Box({-18,Y,(Ground+330)/2},{10,10,(330-Ground)/2},Steel);
		M.Box({-6,Y,Ground+15},{2,18,3},Paint*.5f);
	}
	M.Box({-18,0,145},{8,250,6},Steel);
	const FVector BraceStart(-23,-240,55), BraceEnd(-23,240,170);
	M.Box((BraceStart+BraceEnd)/2,{5,5,(BraceEnd-BraceStart).Size()/2},Steel,
		FQuat::FindBetweenNormals(FVector::UpVector,(BraceEnd-BraceStart).GetSafeNormal()));
	// Full cabinet and rain hood, with rear reinforcing ribs and a faded cream border.
	M.Box({-11,0,CabinetHeight},{15,330,150},FLinearColor(.028f,.043f,.036f));
	M.Box({-6,0,CabinetHeight+153},{23,335,4},Steel);
	M.Box({-4,0,CabinetHeight-150},{20,334,4},Steel);
	for (float Y=-300; Y<=300; Y+=60) M.Box({-28,Y,CabinetHeight},{3,3,143},Steel);
	M.Rect(4.3f,-326,-146,326,-143,Paint); M.Rect(4.3f,-326,143,326,146,Paint);
	M.Rect(4.3f,-326,-146,-323,146,Paint); M.Rect(4.3f,323,-146,326,146,Paint);
	M.Rect(4.3f,-306,-16,-40,-14,Paint*.4f); M.Rect(4.3f,40,-16,306,-14,Paint*.4f);
	M.Rect(4.3f,-306,-98,306,-96,Paint*.4f);
	for (float Y : {-316.f,316.f}) for (float Z : {-136.f,0.f,136.f}) M.Bulb(5,Y,Z,2,Paint*.6f);
	M.Lettering(TEXT("SIMCOPTER PARK"),0,122,3.2f,Paint,false);
	M.Lettering(TEXT("VISITORS"),200,90,3,Paint,false);
	M.Lettering(TEXT("HOME"),-200,90,3,Paint,false);
	M.Lettering(TEXT("INNING"),0,88,2.4f,Paint,false);
	M.Lettering(TEXT("BALL"),180,-35,2.3f,Paint,false);
	M.Lettering(TEXT("STRIKE"),0,-35,2.3f,Paint,false);
	M.Lettering(TEXT("OUT"),-180,-35,2.3f,Paint,false);
	M.Rect(4.3f,287,77,297,103,FLinearColor(.55f,.035f,.02f));
	M.Rect(4.3f,-297,77,-287,103,FLinearColor(.025f,.12f,.4f));
	for (const auto& F : Fields)
	{
		const float HalfWidth=(F.Slots*6-1)*F.Pitch/2;
		M.Rect(4.4f,F.Y-HalfWidth-6,F.Z-3.5f*F.Pitch-4,F.Y+HalfWidth+6,F.Z+3.5f*F.Pitch+4,Dark);
		// Unlit sockets remain visible between active bulbs, like an old matrix panel.
		for (int32 Row=0; Row<7; ++Row) for (int32 Col=0; Col<F.Slots*6-1; ++Col)
			M.Bulb(4.6f,F.Y+HalfWidth-(Col+.5f)*F.Pitch,F.Z+(3-Row)*F.Pitch,F.Pitch*.33f,FLinearColor(.010f,.008f,.004f));
	}
	M.Apply(BoardBacking,0);
	auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_SimCopterLitVertexColor.M_SimCopterLitVertexColor"));
	BoardBacking->SetMaterial(0,Material);
	BoardLampMaterial=UMaterialInstanceDynamic::Create(Material,this);
	for (int32 I=0; I<UE_ARRAY_COUNT(Fields); ++I) BoardLamps->SetMaterial(I,BoardLampMaterial);
	BoardReadout.SetNum(UE_ARRAY_COUNT(Fields));
	UpdateScoreboardLighting();
}

void ASimCopterBaseballGame::UpdateScoreboardLighting()
{
	if (BoardLampMaterial)
		USimCopterEffectExposureSubsystem::ApplyEmissiveNits(BoardLampMaterial,GetWorld(),true,.65f);
}

void ASimCopterBaseballGame::UpdateBoard()
{
	const FString Message=Score.bFinal ? (Score.Runs[0]>Score.Runs[1] ? TEXT("FINAL - VISITORS WIN") : TEXT("FINAL - HOME WIN")) : Announcement;
	const TArray<FString> Readout={FString::Printf(TEXT("%02d"),Score.Runs[0]),FString::Printf(TEXT("%02d"),Score.Runs[1]),
		FString::Printf(TEXT("%02d"),Score.Inning),FString::FromInt(Score.Balls),FString::FromInt(Score.Strikes),
		FString::FromInt(Score.Outs),Score.bFinal ? TEXT("END") : Score.BattingTeam==0 ? TEXT("TOP") : TEXT("BOT"),Message};
	for (int32 I=0; I<Readout.Num(); ++I)
	{
		if (BoardReadout[I]==Readout[I]) continue;
		BoardReadout[I]=Readout[I];
		const auto& F=Fields[I];
		// Rare three-digit totals/extra innings still fit their physical display window.
		const float Pitch=F.Pitch*FMath::Min(1.f,float(F.Slots)/FMath::Max(1,Readout[I].Len()));
		FBoardMesh M; M.Lettering(Readout[I],F.Y,F.Z,Pitch,Amber,true); M.Apply(BoardLamps,I);
	}
}

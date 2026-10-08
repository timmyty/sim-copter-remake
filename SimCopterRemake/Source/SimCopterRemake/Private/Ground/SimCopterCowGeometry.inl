// Deliberate visual redesign requested by the user. Coww keeps its behaviour and clip
// timing, but no longer turns the original screen-space fill strokes into solid boxes.
// Included inside PopulationFigure's geometry namespace to share winding/normal helpers.
void AppendCowEllipsoid(FMeshArrays& M, const FVector& Center, const FVector& Radii,
	const FLinearColor& Color, bool bPatches = false)
{
	constexpr int32 Rings = 10, Sides = 20;
	auto Point = [&](int32 R, int32 S)
	{
		const double P = UE_PI * R / Rings, A = UE_TWO_PI * S / Sides;
		return Center + FVector(FMath::Sin(P) * FMath::Cos(A), FMath::Sin(P) * FMath::Sin(A), FMath::Cos(P)) * Radii;
	};
	auto Normal = [&](const FVector& P) { return ((P - Center) / (Radii * Radii)).GetSafeNormal(); };
	for (int32 R = 0; R < Rings; ++R)
	{
		for (int32 S = 0; S < Sides; ++S)
		{
			const FVector Q[4] = {Point(R,S), Point(R,S+1), Point(R+1,S+1), Point(R+1,S)};
			const FVector N[4] = {Normal(Q[0]), Normal(Q[1]), Normal(Q[2]), Normal(Q[3])};
			const FVector Mid = ((Q[0]+Q[1]+Q[2]+Q[3]) * 0.25 - Center) / Radii;
			// Patches are face colours on the actual body surface, never overlapping shells.
			const bool bDark = bPatches && (
				FMath::Square((Mid.X + 0.48) / 0.42) + FMath::Square((Mid.Z - 0.15) / 0.76) < 1.0
				|| FMath::Square((Mid.X - 0.48) / 0.35) + FMath::Square((Mid.Z - 0.48) / 0.68) < 1.0
				|| (Mid.Z > 0.75 && Mid.X < -0.2));
			const FLinearColor C = bDark ? FLinearColor(0.025f,0.03f,0.032f) : Color;
			if (R == 0 || R == Rings - 1)
			{
				const bool Top = R == 0;
				const FVector T[3] = {Top ? Q[0] : Q[3], Top ? Q[3] : Q[0], Top ? Q[2] : Q[1]};
				const FVector TN[3] = {Normal(T[0]),Normal(T[1]),Normal(T[2])};
				AppendTriangle(M,T,(TN[0]+TN[1]+TN[2]).GetSafeNormal(),C,nullptr,TN);
			}
			else AppendQuad(M,Q,(N[0]+N[1]+N[2]+N[3]).GetSafeNormal(),C,false,nullptr,N);
		}
	}
}

void AppendCowLimb(FMeshArrays& M, const FVector& A, const FVector& B,
	double StartRadius, double EndRadius, const FLinearColor& Color)
{
	const FVector Axis = (B-A).GetSafeNormal();
	FVector U = FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal();
	if (U.IsNearlyZero()) U = FVector::RightVector;
	const FVector V = FVector::CrossProduct(Axis,U).GetSafeNormal();
	for (int32 S = 0; S < 10; ++S)
	{
		const double A0 = UE_TWO_PI*S/10.0, A1 = UE_TWO_PI*(S+1)/10.0;
		const FVector N0 = U*FMath::Cos(A0)+V*FMath::Sin(A0), N1 = U*FMath::Cos(A1)+V*FMath::Sin(A1);
		const FVector Q[4] = {A+N0*StartRadius,A+N1*StartRadius,B+N1*EndRadius,B+N0*EndRadius};
		const FVector N[4] = {N0,N1,N1,N0};
		AppendQuad(M,Q,(N0+N1).GetSafeNormal(),Color,false,nullptr,N);
		const FVector CapA[3] = {A,Q[1],Q[0]}, CapB[3] = {B,Q[3],Q[2]};
		AppendTriangle(M,CapA,-Axis,Color,nullptr,nullptr);
		AppendTriangle(M,CapB,Axis,Color,nullptr,nullptr);
	}
}

void BuildCowFrame(FMeshArrays& M, int32 Frame, int32 FrameCount, float HeightCm)
{
	const FLinearColor White(0.72f,0.70f,0.64f), Black(0.025f,0.03f,0.032f);
	const FLinearColor Pink(0.48f,0.23f,0.20f), Horn(0.55f,0.46f,0.30f);
	const bool bWalking = FrameCount > 2;
	const double Phase = UE_TWO_PI*Frame/FMath::Max(FrameCount,1);
	const float Bob = bWalking ? 0.6f*FMath::Sin(Phase*2) : 0.0f;
	// Model units have ground at zero; the final uniform scale matches population height.
	AppendCowEllipsoid(M,FVector(-4,0,34+Bob),FVector(27,12,16),White,true);
	AppendCowEllipsoid(M,FVector(20,0,37+Bob),FVector(10,8,12),White);
	AppendCowEllipsoid(M,FVector(30,0,45+Bob),FVector(8,7,11),Black);
	AppendCowEllipsoid(M,FVector(35,0,42+Bob),FVector(10,6,7),White);
	AppendCowEllipsoid(M,FVector(43,0,38+Bob),FVector(6,7.5,4.5),Pink);
	for (float Side : {-1.0f,1.0f})
	{
		AppendCowEllipsoid(M,FVector(47,Side*3.5,40+Bob),FVector(1.4,1.0,0.8),Black);
		AppendCowEllipsoid(M,FVector(32,Side*6.6,48+Bob),FVector(2,0.65,2),White);
		AppendCowEllipsoid(M,FVector(32.5,Side*7.1,48+Bob),FVector(1.0,0.35,1.25),Black);
		AppendCowEllipsoid(M,FVector(27,Side*11,51+Bob),FVector(4.5,6,2),Black);
		AppendCowEllipsoid(M,FVector(28,Side*12,51.8+Bob),FVector(3.0,3.8,0.9),Pink);
		const FVector H(26,Side*5,54+Bob), Tip(26,Side*8,61+Bob);
		AppendCowLimb(M,H,Tip,1.8,1.0,Horn);
		AppendCowLimb(M,Tip,FVector(28,Side*8.5,64+Bob),1.0,0.1,Horn);
		for (int32 Rear = 0; Rear < 2; ++Rear)
		{
			const float X = Rear ? -23.0f : 17.0f;
			const double LegPhase = Phase + ((Side > 0) != (Rear > 0) ? UE_PI : 0.0);
			const float Stride = bWalking ? 5.0f*FMath::Sin(LegPhase) : 0.0f;
			const float Lift = bWalking ? 4.0f*FMath::Max(0.0,FMath::Cos(LegPhase)) : 0.0f;
			const FVector Hip(X,Side*8,30+Bob), Knee(X+Stride*0.4f+(Rear ? -2 : 1),Side*9,16+Lift*0.5f);
			const FVector Foot(X+Stride,Side*9,3+Lift);
			AppendCowLimb(M,Hip,Knee,3.5,2.2,White);
			AppendCowEllipsoid(M,Knee,FVector(2.3,2.3,2.5),White);
			AppendCowLimb(M,Knee,Foot,2.0,1.8,White);
			// Two halves make a cloven hoof, with a small visible split.
			for (float Toe : {-1.0f,1.0f})
				AppendCowEllipsoid(M,Foot+FVector(0.8,Toe*1.0,-0.5),FVector(3,0.92,2.5),Black);
		}
	}
	AppendCowEllipsoid(M,FVector(-15,0,20+Bob),FVector(6,5,4),Pink);
	for (float X : {-17.0f,-13.0f}) for (float Y : {-2.5f,2.5f})
		AppendCowLimb(M,FVector(X,Y,18+Bob),FVector(X,Y,14.5+Bob),0.8,0.65,Pink);
	const float Sway = 2.0f*FMath::Sin(Phase);
	const FVector TailRoot(-29,0,39+Bob), TailMid(-33,Sway,25+Bob), TailEnd(-34,Sway*1.5f,12+Bob);
	AppendCowLimb(M,TailRoot,TailMid,1.1,0.8,White);
	AppendCowLimb(M,TailMid,TailEnd,0.8,0.6,White);
	AppendCowEllipsoid(M,TailEnd-FVector(0,0,1),FVector(1.8,1.7,3.5),Black);
	for (FVector& P : M.Vertices) P *= FMath::Max(HeightCm,1.0f)/65.0f;
}

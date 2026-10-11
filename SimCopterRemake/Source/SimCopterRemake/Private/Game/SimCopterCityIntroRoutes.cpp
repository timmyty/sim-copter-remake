#include "Game/SimCopterCityIntro.h"
#include "City/SimCity2000CityActor.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Misc/Paths.h"

namespace
{
// Hand-authored from the full CD movies, not recovered original camera numbers.
// Tile hints resolve against the loaded geometry. These retain the subject and
// direction of each sequence while allowing different terrain/building heights.
// Feature: 0 = district, 1 = airport, 2 = peak, otherwise the original XBLD id.
struct FCityTourScript
{
	FVector2D Entry, Pass, Finale;
	int32 Feature;
	float DiveSeconds, FinaleRange, FinaleBearing, FinalePitch;
	float AircraftPass;
};
const FCityTourScript Scripts[] =
{
	{{72,62},{86,80},{98,78},0,       3,24,  80,18,18}, // Sea Cliff: cliffs, port, open water
	{{48,58},{52,78},{66,94},1,       6,22,  65,24,20}, // Islandtown: inner bay, storage, airport
	{{78,60},{88,72},{86,94},1,       6,23,  60,25,17}, // Diabloville: islands, airport
	{{68,60},{70,72},{80,85},1,       6,24,  70,24,11}, // CatNip Cove: aircraft, bridges, airport
	{{86,60},{92,68},{84,88},1,       6,25,  65,16,15}, // Cypress: close tower pass, airport
	{{80,62},{85,73},{80,96},1,       3,21,  65,23,21}, // Berkeley: towers, coast, hangars
	{{60,66},{60,80},{67,88},1,       3,18,  55,35,20}, // Treeton: central towers, downward airport finish
	{{83,77},{78,43},{103,14},2,      3,23,  90,12,29}, // Keithly: valley, mountain and airliner
	{{60,64},{77,64},{71,90},1,       3,25,  65,15,29}, // Circlopolis: bridges, tower, helicopter pass
	{{80,66},{65,48},{92,34},1,       3,31,  65,18,14}, // River Rail: canyon, aircraft, mountain airport
	{{70,66},{81,70},{80,88},1,       3,24,  65,22,15}, // Cumberland: islands, jet, tower, airport
	{{80,61},{84,76},{95,88},0,       3,25,  65,18,14}, // Scotville: hillside church and rooftops
	{{65,68},{63,72},{69,84},0,       3,13,  60,62,14}, // Kentown: river, plane, steep rooftop descent
	{{71,65},{74,72},{82,91},1,       8,23,  65,28,15}, // Tigger Town: long overview, islands, airport
	{{65,64},{76,72},{92,88},0,       6,22,  65,15,20}, // Terraton: bridge network, waterfront skyline
	{{79,66},{87,78},{94,92},200,     6,27,  65,18,12}, // Happyland: suburbs, dishes beside towers
	{{69,52},{75,49},{66,102},1,      6,32,  65,12,20.3f}, // Roseland: buildings, close plane at 20s, water, airport
	{{72,66},{82,75},{82,95},1,       6,30,  65,16,16}, // Waterton: inlet, coast, airport
	{{93,75},{114,35},{109,19},200,   6,37, -90,10,21}, // Myrtle Dam: dam, runway, forest, entire dish field
	{{80,66},{93,73},{99,88},188,     3,30,  65,16,15}, // River Valley: bridges, slopes, industry
	{{70,64},{78,76},{79,91},0,       3,35,  65,12,20}, // Canyon: streets, coast turn, skyline
	{{65,68},{67,79},{75,90},251,     6,33,  65,16,18}, // Hidden Valley: valley walls and arcologies
	{{64,66},{70,79},{86,89},215,     3,25,  65,34,16}, // Cancer: baseball fields and dense streets
	{{75,62},{66,48},{91,25},2,       0,24,  65,12,15}, // Calebopolis: helicopter tracking around mountains
	{{64,65},{73,78},{87,89},0,       6,33,  65,19,18}, // Valley: valley streets, mountains, wide skyline
	{{67,65},{77,75},{90,90},0,       3,16,  65,40,11}, // Whattheheck: river crossing, close rooftops
	{{65,63},{77,74},{84,88},0,       3,32,  65,15,29}, // Four Cities: river districts and close helicopter
	{{65,62},{71,76},{88,89},0,       3,13,  65,55,17}, // Toronto: towers into low courtyard/construction pass
	{{67,65},{80,74},{83,92},0,       3,27,  65,20,17}, // Conville: domed towers and descending panorama
	{{66,64},{76,76},{87,90},0,       6,36,  65,13,23}, // Metropolis: broad skyline, helicopter and plane
};
static_assert(UE_ARRAY_COUNT(Scripts)==30);

FVector Smooth(const TArray<FVector>& Values, const TArray<float>& Times, float Time)
{
	int32 I=0;
	while (I+2<Times.Num() && Time>Times[I+1]) ++I;
	const int32 J=I+1, Before=FMath::Max(0,I-1), After=FMath::Min(Values.Num()-1,J+1);
	const float Span=Times[J]-Times[I], T=FMath::Clamp((Time-Times[I])/Span,0.0f,1.0f);
	const FVector M0=(Values[J]-Values[Before])*(Span/(Times[J]-Times[Before]));
	const FVector M1=(Values[After]-Values[I])*(Span/(Times[After]-Times[I]));
	return FMath::CubicInterp(Values[I],M0,Values[J],M1,T);
}
}

FTransform FSimCopterCityIntroRoute::Evaluate(float Alpha) const
{
	if (Frames.IsEmpty())
	{
		const FRotator Rotation(-Elevation,-90,0);
		return FTransform(Rotation,Bounds.GetCenter()-Rotation.Vector()*Distance);
	}
	const float Sample=FMath::Clamp(Alpha*Duration/FrameSeconds,0.0f,float(Frames.Num()-1));
	const int32 A=FMath::FloorToInt(Sample), B=FMath::Min(A+1,Frames.Num()-1);
	const float T=Sample-A;
	FVector Position=FMath::Lerp(Frames[A].Position,Frames[B].Position,T);
	FVector Target=FMath::Lerp(Frames[A].Target,Frames[B].Target,T);
	const float Weight=FMath::Lerp(Frames[A].OverviewWeight,Frames[B].OverviewWeight,T);
	Position+=(Position-Target).GetSafeNormal()*(Distance-ReferenceDistance)*Weight;
	return FTransform((Target-Position).Rotation(),Position);
}

FTransform FSimCopterCityIntroRoute::EvaluateAircraft(float Seconds, int32 Index) const
{
	const float Pass=AircraftPassSeconds+(Index==1 ? 9 : 0);
	const FVector Right=FVector::CrossProduct(FVector::UpVector,AircraftDirection);
	if (CityIndex==23 && Index==0)
	{
		const float Angle=Seconds*.16f;
		const FVector Position=AircraftStart+(AircraftDirection*FMath::Cos(Angle)*32+Right*FMath::Sin(Angle)*26)*TileSize;
		FRotator Heading=(-AircraftDirection*FMath::Sin(Angle)*32+Right*FMath::Cos(Angle)*26).Rotation();
		Heading.Roll=-6;
		return FTransform(Heading,Position,FVector(3));
	}
	const FVector Position=AircraftStart+AircraftDirection*(Seconds-Pass)*TileSize*3.5f
		+Right*Index*TileSize*16+FVector(0,0,Index*TileSize*4);
	FRotator Rotation=AircraftDirection.Rotation();
	Rotation.Roll=Index ? 4 : -4;
	return FTransform(Rotation,Position,FVector(bHelicopterSubject && Index==0 ? 3 : 8));
}

void FSimCopterCityIntroRoute::Fit(float AspectRatio)
{
	Distance=0;
	if (!Bounds.IsValid) return;
	const float TanX=FMath::Tan(FMath::DegreesToRadians(FieldOfView*0.5f))*0.90f;
	const float TanY=TanX/FMath::Clamp(AspectRatio,0.35f,4.0f)*0.84f;
	const float Start=Frames.IsEmpty() ? 90 : (Frames[0].Target-Frames[0].Position).Rotation().Yaw;
	for (float Bearing=Start-15;Bearing<=Start+15;Bearing+=2)
	{
		const FRotationMatrix Axes(FRotator(-Elevation,float(Bearing),0));
		for (int32 Corner=0;Corner<8;++Corner)
		{
			const FVector Offset=Bounds.GetExtent()*FVector(Corner&1?1:-1,Corner&2?1:-1,Corner&4?1:-1);
			const double Depth=FVector::DotProduct(Offset,Axes.GetUnitAxis(EAxis::X));
			Distance=FMath::Max(Distance,float(FMath::Max(FMath::Abs(FVector::DotProduct(Offset,Axes.GetUnitAxis(EAxis::Y)))/TanX,
				FMath::Abs(FVector::DotProduct(Offset,Axes.GetUnitAxis(EAxis::Z)))/TanY)-Depth));
		}
	}
	Distance=FMath::Max(Distance,float(Bounds.GetExtent().Z+2000)/FMath::Sin(FMath::DegreesToRadians(Elevation)));
}

FSimCopterCityIntroRoute USimCopterCityIntro::BuildRoute(const ASimCopterTrafficSystemActor& Traffic)
{
	FSimCopterCityIntroRoute R;
	const auto* City=Traffic.GetCityActor();
	if (!City) return R;
	R.TileSize=City->GetTileSize();
	FVector Origin, X, Y;
	if (!Traffic.TryGetTileCenterWorldLocation(0,0,Origin) || !Traffic.TryGetTileCenterWorldLocation(1,0,X)
		|| !Traffic.TryGetTileCenterWorldLocation(0,1,Y)) return R;
	X=(X-Origin).GetSafeNormal2D()*R.TileSize; Y=(Y-Origin).GetSafeNormal2D()*R.TileSize;
	TArray<FBox> Buildings;
	TSet<FIntPoint> Seen;
	for (int32 TY=0;TY<128;++TY) for (int32 TX=0;TX<128;++TX)
	{
		FVector P; Traffic.TryGetTileCenterWorldLocation(TX,TY,P);
		R.Bounds+=P-FVector(R.TileSize*.5f,R.TileSize*.5f,0);
		R.Bounds+=P+FVector(R.TileSize*.5f,R.TileSize*.5f,0);
		FBox B;
		if (City->TryGetBuildingBoundsAtTile(TX,TY,B))
		{
			R.Bounds+=B;
			const FIntPoint Key(FMath::RoundToInt(B.GetCenter().X),FMath::RoundToInt(B.GetCenter().Y));
			if (!Seen.Contains(Key)) { Seen.Add(Key); Buildings.Add(B); }
		}
	}
	R.Fit(16.0f/9); R.ReferenceDistance=R.Distance;
	// Use exact stems only: an imported "city16-copy" must not silently select Roseland.
	const FString Stem=FPaths::GetBaseFilename(City->GetResolvedCityPath());
	for (int32 I=0;I<30;++I) if (Stem.Equals(FString::Printf(TEXT("city%d"),I),ESearchCase::IgnoreCase)) R.CityIndex=I;
	const FCityTourScript Fallback={{64,64},{77,72},{85,90},1,4,28,65,22,15};
	const auto& Script=R.CityIndex==INDEX_NONE ? Fallback : Scripts[R.CityIndex];
	auto Ground=[&](FVector2D Tile)
	{
		FVector P; Traffic.TryGetTileCenterWorldLocation(FMath::Clamp(FMath::RoundToInt(Tile.X),0,127),FMath::Clamp(FMath::RoundToInt(Tile.Y),0,127),P);
		const FVector XY=Origin+X*Tile.X+Y*Tile.Y; P.X=XY.X; P.Y=XY.Y; return P;
	};
	auto Subject=[&](FVector2D Hint,int32 Feature)
	{
		FVector2D Selected=Hint;
		if (Feature==1)
		{
			const auto Airport=Traffic.GetAirportOriginTile();
			if (Airport.X>=0 && Airport.Y>=0 && Airport.X<128 && Airport.Y<128) Selected=FVector2D(Airport)+FVector2D(1.5,1.5);
		}
		else if (Feature>=2 || (Feature==0 && R.CityIndex>=11))
		{
			double Best=TNumericLimits<double>::Max();
			for (int32 TY=0;TY<128;++TY) for (int32 TX=0;TX<128;++TX)
			{
				const FVector2D Tile(TX,TY);
				if (Feature==0 ? Traffic.GetXbldTileId(TX,TY)<112 : Feature!=2 && Traffic.GetXbldTileId(TX,TY)!=Feature) continue;
				const FVector P=Ground(Tile);
				const double Score=Feature==2 ? FVector2D::DistSquared(Tile,Hint)-P.Z/R.TileSize*180 : FVector2D::DistSquared(Tile,Hint);
				if (Score<Best) { Best=Score; Selected=Tile; }
			}
		}
		return Selected;
	};
	const FVector2D Finale=Subject(Script.Finale,Script.Feature);
	FVector End=Ground(Finale)+FVector(0,0,R.TileSize*1.2f);
	R.FinaleBounds=FBox(End-FVector(R.TileSize*2),End+FVector(R.TileSize*2));
	FBox FinaleBuilding;
	if (Script.Feature>2 && City->TryGetBuildingBoundsAtTile(FMath::RoundToInt(Finale.X),FMath::RoundToInt(Finale.Y),FinaleBuilding))
	{ R.FinaleBounds=FinaleBuilding; End=FinaleBuilding.GetCenter(); }
	if (Script.Feature==200)
	{
		// Include the entire field, rather than pointing at one dish. The orange
		// dishes seen in CITY18_B are the original PP200 (XBLD 0xc8) wind-power art.
		FBox Field(ForceInit);
		for (int32 TY=0;TY<128;++TY) for (int32 TX=0;TX<128;++TX)
			if (Traffic.GetXbldTileId(TX,TY)==200 && FVector2D::Distance(FVector2D(TX,TY),Finale)<30)
			{
				FBox B;
				if (City->TryGetBuildingBoundsAtTile(TX,TY,B)) Field+=B;
			}
		if (Field.IsValid) { R.FinaleBounds=Field; End=Field.GetCenter(); }
	}
	const FVector Entry=Ground(Script.Entry)+FVector(0,0,R.TileSize*2);
	const FVector Pass=Ground(Script.Pass)+FVector(0,0,R.TileSize*3);
	TArray<float> Times={0,Script.DiveSeconds,Script.DiveSeconds+3,FMath::Max(11.0f,Script.DiveSeconds+5),16,21,26,29,R.Duration};
	// Calebopolis opens on a following helicopter; no duplicate time at zero.
	if (Script.DiveSeconds==0) { Times[1]=2; Times[2]=5; }
	auto Offset=[&](float Range,float Bearing,float Pitch)
	{
		const float A=FMath::DegreesToRadians(Bearing), P=FMath::DegreesToRadians(Pitch);
		return (X*FMath::Cos(A)+Y*FMath::Sin(A))*Range*FMath::Cos(P)+FVector(0,0,R.TileSize*Range*FMath::Sin(P));
	};
	TArray<FVector> Targets={R.Bounds.GetCenter(),R.Bounds.GetCenter(),Entry,Entry,Pass,Pass,End,End,End};
	TArray<FVector> Positions;
	Positions.Add(Targets[0]+Offset(R.Distance/R.TileSize,-90,R.Elevation));
	Positions.Add(Targets[1]+Offset(R.Distance/R.TileSize,-78,R.Elevation));
	Positions.Add(Entry+Offset(95,-52,58));
	Positions.Add(Entry+Offset(53,-10,58));
	Positions.Add(Pass+Offset(32,25,24));
	Positions.Add(Pass+Offset(30,55,15));
	Positions.Add(End+Offset(Script.FinaleRange*1.3f,Script.FinaleBearing-12,Script.FinalePitch+5));
	Positions.Add(End+Offset(Script.FinaleRange,Script.FinaleBearing,Script.FinalePitch));
	Positions.Add(End+Offset(Script.FinaleRange*.97f,Script.FinaleBearing+2,Script.FinalePitch));
	if (R.CityIndex==18)
	{
		// CITY18_B reveals the farm first, then brings individual turbines into
		// the foreground over its last seconds instead of holding a distant view.
		Positions[7]=End+Offset(18,-90,7);
		Positions[8]=End+Offset(15,-88,7);
	}
	if (R.CityIndex==16)
	{
		// CITY16_B frames 255..315 pass the tall waterfront buildings and the
		// airliner; 330..375 look down across water before the airport rises into view.
		Times={0,2,6,8,11,14,17,20,22.5f,24.5f,27,29,R.Duration};
		const FVector Water=Ground(FVector2D(86,80));
		Targets={R.Bounds.GetCenter(),R.Bounds.GetCenter(),R.Bounds.GetCenter(),Entry,Entry,Pass,Pass,Pass,Water,Water,End,End,End};
		Positions={Targets[0]+Offset(R.Distance/R.TileSize,-90,R.Elevation),Targets[1]+Offset(R.Distance/R.TileSize,-87,R.Elevation),
			Targets[2]+Offset(R.Distance/R.TileSize*.84f,-65,45),Entry+Offset(76,-48,60),Entry+Offset(49,-13,57),
			Pass+Offset(35,10,38),Pass+Offset(22,35,22),Pass+Offset(18,60,30),Water+Offset(25,40,60),
			Water+Offset(27,65,34),End+Offset(35,65,14),End+Offset(32,65,12),End+Offset(31,67,12)};
	}
	for (int32 I=0;I<=451;++I)
	{
		const float T=I*R.FrameSeconds;
		FSimCopterCityIntroFrame Frame;
		Frame.Position=Smooth(Positions,Times,T); Frame.Target=Smooth(Targets,Times,T);
		Frame.OverviewWeight=1-FMath::SmoothStep(Script.DiveSeconds,Script.DiveSeconds+3,T);
		R.Frames.Add(Frame);
	}
	// Keep the near-camera aircraft flight deterministic and independent of the
	// saved ambient fleet. The camera follows the same path used by its mesh.
	R.bHelicopterSubject=R.CityIndex==8 || R.CityIndex==23 || R.CityIndex==26 || R.CityIndex==29;
	R.AircraftPassSeconds=Script.AircraftPass;
	const int32 PassFrame=FMath::Clamp(FMath::RoundToInt(Script.AircraftPass/R.FrameSeconds),0,451);
	const FVector Facing=(R.Frames[PassFrame].Target-R.Frames[PassFrame].Position).GetSafeNormal();
	R.AircraftDirection=(X*.8f+Y*.6f).GetSafeNormal();
	R.AircraftStart=R.Frames[PassFrame].Position+Facing*R.TileSize*14+FVector(0,0,R.TileSize*2);
	if (R.CityIndex==23)
	{
		R.AircraftStart=End+FVector(0,0,R.TileSize*8);
		// The rebuilt mountain can be taller than the movie's terrain. Keep the
		// complete orbit above its actual terrain and roofs, including the views
		// across the inside of the orbit, so the tracked helicopter stays visible.
		float FlightFloor=R.AircraftStart.Z;
		for (int32 TY=0;TY<128;++TY) for (int32 TX=0;TX<128;++TX)
		{
			const FVector P=Ground(FVector2D(TX,TY));
			if (FVector::DistSquared2D(P,End)<FMath::Square(R.TileSize*38))
				FlightFloor=FMath::Max(FlightFloor,float(P.Z+R.TileSize*6));
		}
		for (const auto& B:Buildings)
			if (FVector::DistSquared2D(B.GetCenter(),End)<FMath::Square(R.TileSize*38))
				FlightFloor=FMath::Max(FlightFloor,float(B.Max.Z+R.TileSize*4));
		R.AircraftStart.Z=FlightFloor;
		for (int32 I=0;I<R.Frames.Num();++I)
		{
			const float T=I*R.FrameSeconds;
			const auto Aircraft=R.EvaluateAircraft(T,0);
			const FVector Heading=Aircraft.GetRotation().GetForwardVector();
			const FVector Behind=-Heading*R.TileSize*13;
			const FVector Side=FVector::CrossProduct(FVector::UpVector,Heading)*R.TileSize*6;
			const FVector Camera=Aircraft.GetLocation()+Behind+Side+FVector(0,0,R.TileSize*4);
			R.Frames[I].Position=Camera;
			R.Frames[I].Target=Aircraft.GetLocation();
			R.Frames[I].OverviewWeight=0;
		}
	}
	// Adapt clearance to the rebuilt map. Dilate required heights by half a second
	// and ease the lift across one second, so passing roofs never causes a camera pop.
	TArray<float> Lift; Lift.Init(0,R.Frames.Num());
	for (int32 I=0;I<R.Frames.Num();++I)
	{
		const FVector P=R.Frames[I].Position;
		const FVector D=P-Origin;
		const FVector2D Tile(FVector::DotProduct(D,X)/(R.TileSize*R.TileSize),FVector::DotProduct(D,Y)/(R.TileSize*R.TileSize));
		float Floor=R.Bounds.Min.Z;
		if (Tile.X>=0 && Tile.X<128 && Tile.Y>=0 && Tile.Y<128) Floor=Ground(Tile).Z;
		for (const auto& B:Buildings)
			if (P.X>B.Min.X-150 && P.X<B.Max.X+150 && P.Y>B.Min.Y-150 && P.Y<B.Max.Y+150) Floor=FMath::Max(Floor,float(B.Max.Z));
		const float Required=FMath::Max(0.0f,Floor+R.TileSize*1.5f-float(P.Z));
		for (int32 J=FMath::Max(0,I-15);J<=FMath::Min(I+15,Lift.Num()-1);++J)
			Lift[J]=FMath::Max(Lift[J],Required*(1-FMath::SmoothStep(7.0f,16.0f,float(FMath::Abs(I-J)))));
	}
	for (int32 I=0;I<R.Frames.Num();++I) R.Frames[I].Position.Z+=Lift[I];
	if (R.CityIndex!=23)
	{
		const FVector Look=(R.Frames[PassFrame].Target-R.Frames[PassFrame].Position).GetSafeNormal();
		R.AircraftStart=R.Frames[PassFrame].Position+Look*R.TileSize*(R.CityIndex==16 ? 8 : 14)+FVector(0,0,R.TileSize*1.5f);
		if (R.CityIndex==16) R.AircraftDirection=(-Look+FVector::CrossProduct(FVector::UpVector,Look)*.5f).GetSafeNormal2D();
	}
	return R;
}

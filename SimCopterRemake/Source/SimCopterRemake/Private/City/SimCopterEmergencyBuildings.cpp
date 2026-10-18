#include "City/SimCopterEmergencyBuildings.h"
#include "City/SimCopterLandmarkGeometry.h"
#include "Algo/Reverse.h"

namespace SimCopterEmergencyBuildings
{
bool IsEmergencyBuilding(int32 Id) { return Id==HospitalId || Id==PoliceId || Id==FireId; }
bool IsDetail(const FMaxisMeshObject& Model,const FMaxisMeshFace& Face)
{
	return IsEmergencyBuilding(Model.Header.Id) && Face.FaceType==15 && Face.TextureAtlasIndex==DetailAtlasTag;
}

FMaxisMeshObject MakeModel(const FMaxisMeshObject& Original, bool bUpdated)
{
	FMaxisMeshObject R=Original;
	if (!IsEmergencyBuilding(R.Header.Id) || R.Vertices.IsEmpty()) return R;
	if (!bUpdated)
	{
		// Before this artwork update FS211 matched HO209's height. Keep the exact
		// former integer scaling, original faces, roof UVs and facade assignments.
		if (R.Header.Id == FireId)
		{
			int64 Top = 0;
			for (const auto& V : R.Vertices) Top = FMath::Max<int64>(Top, V.Y);
			if (Top > 0 && Top < 13950477)
				for (auto& V : R.Vertices) V.Y = int32(int64(V.Y) * 13950477 / Top);
		}
		return R;
	}
	// These are presentation additions to HO209/PO210/FS211, selected by the original
	// FUN_0047c0c0 building dispatch. Work in GEO X/Y-up/Z units (one unit = 6.25 cm).
	// Roofs remain the current remake's geometry/art; the fire model arrives unstretched.
	SimCopterLandmarkGeometry::FBuilder B{R};
	auto Tag=[&](int32 First,bool Windows=false)
	{
		for(int32 I=First;I<R.Faces.Num();++I)
		{
			R.Faces[I].TextureAtlasIndex=DetailAtlasTag;
			if(Windows) for(auto& UV:R.Faces[I].RawUVs) UV.X+=256*65536;
		}
	};
	auto Box=[&](FVector C,FVector S,uint8 Finish,bool Windows=false)
	{
		const int32 First=R.Faces.Num(); B.Box(C,S,Finish); Tag(First,Windows);
	};
	auto Beam=[&](FVector A,FVector D,double Radius,uint8 Finish)
	{
		const int32 First=R.Faces.Num(); B.Beam(A,D,Radius,Finish); Tag(First);
	};
	auto Face=[&](const TArray<FVector>& Points,uint8 Finish,bool Windows=false)
	{
		const int32 First=R.Faces.Num(); B.Face(Points,Finish); Tag(First,Windows);
	};
	auto Rail=[&](FVector A,FVector D,uint8 Finish)
	{
		const int32 Posts=FMath::Max(1,FMath::CeilToInt((D-A).Size()/5));
		for(int32 I=0;I<=Posts;++I) Box(FMath::Lerp(A,D,double(I)/Posts)+FVector(0,6,0),{.65,12,.65},Finish);
		Beam(A+FVector(0,11.7,0),D+FVector(0,11.7,0),.55,Finish);
		Beam(A+FVector(0,5.5,0),D+FVector(0,5.5,0),.3,Finish);
	};

	// Retained legacy faces use the same centre-facing convention as the existing reader.
	// Normalize their winding before mixing with the builder's consistently wound details.
	FVector Center=FVector::ZeroVector;
	for(const auto& V:R.Vertices) Center+=FVector(V.X,V.Y,V.Z)/65536.;
	Center/=R.Vertices.Num();
	for(auto& F:R.Faces)
	{
		if(F.VertexIndices.Num()<3) continue;
		FVector N=FVector::ZeroVector,C=FVector::ZeroVector;
		for(int32 I=0;I<F.VertexIndices.Num();++I)
		{
			const auto& VA=R.Vertices[F.VertexIndices[I]];
			const auto& VB=R.Vertices[F.VertexIndices[(I+1)%F.VertexIndices.Num()]];
			const FVector A=FVector(VA.X,VA.Y,VA.Z)/65536.,D=FVector(VB.X,VB.Y,VB.Z)/65536.;
			N+=FVector::CrossProduct(A,D); C+=A;
		}
		if(FVector::DotProduct(N,C/F.VertexIndices.Num()-Center)>0)
		{
			Algo::Reverse(F.VertexIndices); Algo::Reverse(F.RawUVs);
		}
	}
	R.Header.bConsistentWinding=true;

	if(R.Header.Id==HospitalId)
	{
		// The reference's balcony stacks belong to the entrance-facing recesses,
		// beside the lower wing, not to the two rear corners of the ward tower.
		for(double X:{38.5}) for(double Z:{-57.8,57.8}) for(double Y:{33.,66.,99.,132.,165.,198.})
		{
			Box({X,Y,Z},{20.4,2.4,20.4},Orange);
			const double EdgeX=X<0?X-9.8:X+9.8,EdgeZ=FMath::Sign(Z)*(FMath::Abs(Z)+9.8);
			Rail({EdgeX,Y+1.2,Z-9.8},{EdgeX,Y+1.2,Z+9.8},Orange);
			Rail({X-9.8,Y+1.2,EdgeZ},{X+9.8,Y+1.2,EdgeZ},Orange);
		}
		// Recess the ground-floor doorway under the front wing to create a covered
		// arrival area. Its original roof at Y=105.65 remains untouched. Keep the
		// central approach and the existing ground medic's exterior post clear.
		R.Faces.RemoveAll([&](const FMaxisMeshFace& F)
		{
			if(F.FaceType!=18 || F.TextureAtlasIndex!=39 || F.VertexIndices.Num()!=4) return false;
			for(auto I:F.VertexIndices) if(FMath::Abs(R.Vertices[I].X/65536.-87.4)>.02) return false;
			return true;
		});
		auto HospitalWall=[&](double X,double Bottom,double Top)
		{
			B.Face({{X,Bottom,-47.4},{X,Bottom,47.4},{X,Top,47.4},{X,Top,-47.4}},2);
			auto& F=R.Faces.Last();F.FaceType=18;F.TextureAtlasIndex=39;
		};
		HospitalWall(87.4,35.8,105.65);HospitalWall(64.5,0,33);
		// Two separate overhead sections with an open centre approach. Each canopy
		// turns the corner of the entrance wing; there is no third bridging awning.
		for(double Side:{-1.,1.})
		{
			Box({80.5,34,Side*31.5},{31,3.2,31.8},Cyan);
			Box({90.6,35.7,Side*31.5},{10.8,.7,32.4},Orange);
			Box({94.4,16.4,Side*35},{2.2,32.8,2.2},Stone);
		}
		// Orange balustrade follows all three exposed edges of the lower roof.
		Rail({86.5,105.65,-46.5},{86.5,105.65,46.5},Orange);
		for(double Side:{-1.,1.}) Rail({49,105.65,Side*46.5},{86.5,105.65,Side*46.5},Orange);
		Box({64.85,13.5,0},{.4,26,27},Glass);
		for(double Z:{-14.,0.,14.}) Box({65.2,13.8,Z},{.8,27,.8},Stone);
		// Side canopy bays make the covered area legible from the isometric view as well.
		for(double S:{-1.,1.})
		{
			Box({69,34,S*56.5},{36,3.2,18},Cyan);
			for(double X:{54.,83.}) Box({X,16.4,S*63.5},{2.2,32.8,2.2},Stone);
		}
	}
	else if(R.Header.Id==FireId)
	{
		// Replace only the facade polygons. Keep both existing roof levels, their exact
		// material/UV assignment and every beacon. FS211's actual roof is Y=107.79.
		R.Faces.RemoveAll([](const FMaxisMeshFace& F){ return (F.FaceType==18 && F.TextureAtlasIndex==39) ||
			(F.FaceType==18 && F.TextureAtlasIndex==2 && F.MaterialIndex>=20 && F.MaterialIndex<=23); });
		constexpr double L=-84.05,H=82.7,Z0=-83.68,Z1=83.08,Roof=107.79;
		Face({{L,0,Z1},{L,0,Z0},{L,Roof,Z0},{L,Roof,Z1}},Brick,true);
		Face({{L,0,Z0},{H,0,Z0},{H,Roof,Z0},{L,Roof,Z0}},Brick,true);
		Face({{H,0,Z1},{L,0,Z1},{L,Roof,Z1},{H,Roof,Z1}},Brick,true);
		Face({{H,46,Z0},{H,46,Z1},{H,Roof,Z1},{H,Roof,Z0}},Brick,true);
		// The reference's three arched apparatus bays are actual recesses, not a door decal.
		for(double Z:{-55.,0.,55.})
		{
			Box({74,1.3,Z},{39,2.6,43},Stone);
			Box({65,21,Z},{1,40,40},Dark);
			Box({74,22,Z-21.5},{18,44,3},Stone); Box({74,22,Z+21.5},{18,44,3},Stone);
			for(int32 I=0;I<12;++I)
			{
				const double A=PI*I/12,D=PI*(I+1)/12;
				Face({{83.1,32+11*sin(A),Z+20*cos(A)},{83.1,32+11*sin(D),Z+20*cos(D)},
					{83.1,32+14*sin(D),Z+23*cos(D)},{83.1,32+14*sin(A),Z+23*cos(A)}},Stone);
				Face({{82.7,32+14*sin(A),Z+23*cos(A)},{82.7,32+14*sin(D),Z+23*cos(D)},
					{82.7,47,Z+23*cos(D)},{82.7,47,Z+23*cos(A)}},Brick);
			}
			// Dark opening soffit; one partially raised shutter and two parked engine noses.
			Box({74,43.5,Z},{18,2,40},Dark);
			if(Z!=0)
			{
				const int32 PaintStart=R.Faces.Num();
				Box({74,10,Z},{14,13,29},Orange);
				Box({76,20,Z},{11,10,27},Orange);
				for(int32 F=PaintStart;F<R.Faces.Num();++F) for(auto& UV:R.Faces[F].RawUVs) UV.X+=64*65536;
				Box({81.7,21,Z},{.5,7,24},Glass);
				Box({81.8,13.5,Z},{.6,1.5,29},Stone);
				Box({82,5,Z},{1.4,2,31},Metal);
				for(double S:{-1.,1.}) Box({82,10,Z+S*11},{1,2.4,3.3},Gold);
			}
		}
		for(double Z:{-81.5,-27.5,27.5,80.8}) Box({76.4,23,Z},{12.6,46,10},Brick);
		for(double Y:{48.,77.,105.8})
		{
			Box({-.675,Y,Z0-.5},{167.8,1.8,1.8},Stone);
			Box({-.675,Y,Z1+.5},{167.8,1.8,1.8},Stone);
			Box({H+.5,Y,-.3},{1.8,1.8,167.8},Stone);
			Box({L-.5,Y,-.3},{1.8,1.8,167.8},Stone);
		}
		// Preserve the original upper service-room volume below its original roof.
		Face({{-40.28,107.79,-39.9},{38.93,107.79,-39.9},{38.93,136.21,-39.9},{-40.28,136.21,-39.9}},Brick,true);
		Face({{38.93,107.79,39.31},{-40.28,107.79,39.31},{-40.28,136.21,39.31},{38.93,136.21,39.31}},Brick,true);
		Face({{-40.28,107.79,39.31},{-40.28,107.79,-39.9},{-40.28,136.21,-39.9},{-40.28,136.21,39.31}},Brick,true);
		Face({{38.93,107.79,-39.9},{38.93,107.79,39.31},{38.93,136.21,39.31},{38.93,136.21,-39.9}},Brick,true);
		// Reference detail pass: narrow copper downpipes and bay lintel trim, clear of the retained roofs.
		for(double Z:{-78.,77.}) Beam({L-.8,3,Z},{L-.8,103,Z},.5,Metal);
		for(double Z:{-54.,0.,54.}) Box({82.3,44,Z},{1.2,1.5,38},Stone);
		// Two square chimney stacks off the landing square; hollow dark tops and stone caps.
		for(double Z:{40.,66.})
		{
			Box({-65,119.8,Z},{9,24,9},Brick);
			Box({-65,132,Z},{11,2,11},Stone);
			Box({-65,133.1,Z},{6.4,.3,6.4},Dark);
		}
		// Tilted dish: open concave bowl, rim, feed horn, tripod and pedestal.
		const FVector DishCenter(53,124,-58),Axis=FVector(.65,.75,-.15).GetSafeNormal();
		const FVector U=FVector::CrossProduct(Axis,FVector(0,0,1)).GetSafeNormal(),V=FVector::CrossProduct(Axis,U);
		Box({53,110,-58},{12,4,12},Stone);
		for(double Z:{-63.,-53.}) Beam({50,112,Z},DishCenter,1.2,Metal);
		for(int32 I=0;I<20;++I)
		{
			const double A=2*PI*I/20,D=2*PI*(I+1)/20;
			auto P=[&](double Radius,double Angle){return DishCenter+Axis*(Radius*Radius/35)+Radius*(cos(Angle)*U+sin(Angle)*V);};
			Face({DishCenter,P(7,A),P(7,D)},Metal);
			Face({P(7,A),P(12,A),P(12,D),P(7,D)},Metal);
			Beam(P(12,A),P(12,D),.55,Stone);
			if(I%5==0) Beam(P(12,A),DishCenter+Axis*13,.28,Dark);
		}
		Beam(DishCenter+Axis*11,DishCenter+Axis*14,1.2,Metal);
	}
	else
	{
		// PO210 already has the reference's tall centre and asymmetric low wings. Retain
		// that outline and the current height; replace wall art with blue glazed cladding.
		for(auto& F:R.Faces)
		{
			if(F.FaceType!=18 || F.TextureAtlasIndex!=2 || F.MaterialIndex<60 || F.MaterialIndex>63) continue;
			F.FaceType=15; F.MaterialIndex=Cyan; F.TextureAtlasIndex=DetailAtlasTag; F.RawUVs.Reset();
			const auto& A=R.Vertices[F.VertexIndices[0]];
			bool ConstantX=true; for(auto I:F.VertexIndices) ConstantX &= FMath::Abs(int64(R.Vertices[I].X)-A.X)<16;
			for(auto I:F.VertexIndices)
			{
				const auto& P=R.Vertices[I];
				F.RawUVs.Add(FIntPoint((ConstantX?P.Z:P.X)/16+256*65536,65536-P.Y/16));
			}
		}
		// Broad approach to the recessed entrance on GEO -Z, as in the source sprite.
		for(int32 I=0;I<7;++I) Box({0,.9+I*.9,-68+I*2.8},{78-I*.8,1.8+I*1.8,43-I*5.6},Stone);
		Box({0,23,-45.4},{35,21,.5},Glass);
		for(double X:{-24.,-12.,0.,12.,24.}) Box({X,22,-50},{2.2,25,3},Stone);
		Box({0,35,-48.3},{56,4,10},Stone);
		Box({0,38,-48.3},{59,2,11},Cyan);
		// Slim glazed entrance sidelights and metal door mullions at the existing recess.
		for(double X:{-20.,20.}) Box({X,23,-45.5},{4,20,.4},Glass);
		for(double X:{-8.,8.}) Beam({X,13,-45.7},{X,33,-45.7},.25,Metal);
		// Gold shield medallions on the high front and side, below the unchanged roof.
		for(bool Side:{false,true})
		{
			auto P=[&](double U,double Y){return Side?FVector(48.4,Y,U):FVector(U,Y,-45.4);};
			Face({P(-8,177),P(8,177),P(8,165),P(0,158),P(-8,165)},Gold);
			Beam(P(0,162),P(0,174),.65,Stone); Beam(P(-5,169),P(5,169),.65,Stone);
		}
	}
	B.Finish();
	return R;
}
}

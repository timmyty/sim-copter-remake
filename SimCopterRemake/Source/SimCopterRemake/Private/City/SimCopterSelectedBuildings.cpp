#include "City/SimCopterSelectedBuildings.h"
#include "City/SimCopterLandmarkGeometry.h"

namespace SimCopterSelectedBuildings
{
namespace
{
enum EFinish : uint8 { Stone, CreamWindows, BrickWindows, GlassWindows, Roof, Red, Gold, Metal,
	Leaves, Grass, Water, Solar, Wood, Dark, White, Copper };

// Author in a 200 x 100 x 200 envelope, then fit the original GEO dimensions exactly.
// Silhouette proportions differ by building; there is no tile-size or city-wide scale change.
struct FAuthor
{
	FMaxisMeshObject& M;
	SimCopterLandmarkGeometry::FBuilder B;
	FVector Scale;
	FAuthor(FMaxisMeshObject& Model, FVector S):M(Model),B{Model},Scale(S) {}
	FVector P(FVector V) const { return V*Scale; }
	void Face(TArray<FVector> V,uint8 F) { for(auto& Q:V) Q=P(Q);B.Face(V,F); }
	void Box(FVector C,FVector S,uint8 F,int32 Top=INDEX_NONE) { B.Box(P(C),P(S),F,Top); }
	void Beam(FVector A,FVector C,double R,uint8 F) { B.Beam(P(A),P(C),R*FMath::Min(Scale.X,Scale.Z),F); }
	void Drum(FVector C,double R,double Top,double H,uint8 F,int32 N=32,bool Cap=true)
	{
		for(int32 I=0;I<N;++I)
		{
			const double A=2*PI*I/N,D=2*PI*(I+1)/N;
			const FVector V0=C+FVector(R*cos(A),0,R*sin(A)),V1=C+FVector(R*cos(D),0,R*sin(D));
			const FVector V2=C+FVector(Top*cos(D),H,Top*sin(D)),V3=C+FVector(Top*cos(A),H,Top*sin(A));
			if(Top<.01) Face({V0,V1,C+FVector(0,H,0)},F);else Face({V0,V1,V2,V3},F);
			if(Cap && Top>.01) Face({C+FVector(0,H,0),V3,V2},F);
		}
	}
	void Ball(FVector C,FVector R,uint8 F,int32 N=24,int32 Rows=12,bool UpperOnly=false)
	{
		for(int32 J=0;J<Rows;++J)
		{
			const double A=(UpperOnly?0:-PI/2)+(UpperOnly?PI/2:PI)*J/Rows;
			const double D=(UpperOnly?0:-PI/2)+(UpperOnly?PI/2:PI)*(J+1)/Rows;
			for(int32 I=0;I<N;++I)
			{
				const double U=2*PI*I/N,V=2*PI*(I+1)/N;
				auto Q=[&](double T,double L){return C+FVector(R.X*cos(T)*cos(L),R.Y*sin(T),R.Z*cos(T)*sin(L));};
				if(J==Rows-1) Face({Q(A,U),Q(A,V),C+FVector(0,R.Y,0)},F);
				else if(!UpperOnly && J==0) Face({C-FVector(0,R.Y,0),Q(D,V),Q(D,U)},F);
				else Face({Q(A,U),Q(A,V),Q(D,V),Q(D,U)},F);
			}
		}
	}
	void Frustum(FVector C,FVector Base,FVector Top,double H,uint8 F)
	{
		const FVector A=C-FVector(Base.X,0,Base.Z),D=C+FVector(Base.X,0,Base.Z);
		const FVector E=C+FVector(-Top.X,H,-Top.Z),G=C+FVector(Top.X,H,Top.Z);
		Face({{A.X,C.Y,A.Z},{D.X,C.Y,A.Z},{G.X,E.Y,E.Z},E},F);
		Face({{D.X,C.Y,D.Z},{A.X,C.Y,D.Z},{E.X,E.Y,G.Z},G},F);
		Face({{A.X,C.Y,D.Z},{A.X,C.Y,A.Z},E,{E.X,E.Y,G.Z}},F);
		Face({{D.X,C.Y,A.Z},{D.X,C.Y,D.Z},G,{G.X,E.Y,E.Z}},F);
		Face({E,{G.X,E.Y,E.Z},G,{E.X,E.Y,G.Z}},Roof);
	}
	void Rail(FVector A,FVector D,double H=3,uint8 F=Metal,int32 Posts=8)
	{
		Beam(A+FVector(0,H,0),D+FVector(0,H,0),.35,F);
		for(int32 I=0;I<=Posts;++I) Beam(FMath::Lerp(A,D,double(I)/Posts),FMath::Lerp(A,D,double(I)/Posts)+FVector(0,H,0),.3,F);
	}
	void Tree(double X,double Z,double Y=1,double H=12)
	{
		Drum({X,Y,Z},1,1,H*.6,Wood,8);
		// Keep foliage round when the building's authored envelope is very tall or flat.
		const double R=FMath::Min(H*.37*Scale.Y,H*.74*FMath::Min(Scale.X,Scale.Z));
		Ball({X,Y+H*.68,Z},{R/Scale.X,H*.42,R/Scale.Z},Leaves,16,8);
		Ball({X+R*.45/Scale.X,Y+H*.55,Z},{R*.7/Scale.X,H*.3,R*.7/Scale.Z},Leaves,12,6);
		Ball({X-R*.4/Scale.X,Y+H*.72,Z+R*.3/Scale.Z},{R*.75/Scale.X,H*.28,R*.75/Scale.Z},Leaves,12,6);
	}
	void Steps(FVector C,double W,double Depth,double H,int32 N=7)
	{ for(int32 I=0;I<N;++I) Box(C+FVector(0,H*(I+1)/(2*N),Depth*I/N),{W,H*(I+1)/N,Depth*2*(N-I)/N},Stone); }
	void Gable(FVector C,FVector S,double Rise,uint8 F=Roof)
	{
		const FVector A=C-S*.5,D=C+S*.5;const double Y=C.Y;
		Face({{A.X,Y,A.Z},{D.X,Y,A.Z},{C.X,Y+Rise,A.Z}},Stone);
		Face({{D.X,Y,D.Z},{A.X,Y,D.Z},{C.X,Y+Rise,D.Z}},Stone);
		Face({{A.X,Y,D.Z},{A.X,Y,A.Z},{C.X,Y+Rise,A.Z},{C.X,Y+Rise,D.Z}},F);
		Face({{C.X,Y+Rise,D.Z},{C.X,Y+Rise,A.Z},{D.X,Y,A.Z},{D.X,Y,D.Z}},F);
	}
	void Tank(FVector C,double R,double H,uint8 F=Metal)
	{
		Drum(C,R,R,H,F);Ball(C+FVector(0,H,0),{R,H*.12,R},F,32,6,true);
		for(double Y:{.05,.52,.94}) Drum(C+FVector(0,H*Y,0),R+.6,R+.6,.6,White,32,false);
	}
	void Pipe(FVector A,FVector C,double R=1.4,uint8 F=Metal) { Beam(A,C,R,F); }
	void Stack(FVector C,double R,double H,bool Smoke=true)
	{
		Drum(C,R,R*.8,H,Stone);Drum(C+FVector(0,H*.76,0),R*.85,R*.83,H*.08,Red);
		Drum(C+FVector(0,H-.9,0),R*.8,R*.8,.9,Dark);
		// Same marker format consumed by the existing smoke pool, aligned to the new chimney.
		if(!Smoke)return;
		FMaxisMeshFace Marker;Marker.FaceType=26;Marker.MaterialIndex=1;Marker.VertexCount=1;
		Marker.VertexIndices.Add(M.Vertices.Num());Marker.RawUVs.Add(FIntPoint::ZeroValue);
		const FVector V=P(C+FVector(0,H,0));M.Vertices.Add({int32(V.X*65536),int32(V.Y*65536),int32(V.Z*65536)});M.Faces.Add(Marker);
	}
	void Equipment(FVector C,double S=10)
	{ Box(C+FVector(0,2,0),{S,4,S*.7},Metal,Dark);for(int32 I=0;I<4;++I)Box(C+FVector((I-1.5)*S/5,4.2,0),{.6,.5,S*.65},White); }
	void RoundedHall(FVector C,double RX,double RZ,double H,double Radius,uint8 F)
	{
		TArray<FVector> Ring;
		for(int32 Corner=0;Corner<4;++Corner)for(int32 I=0;I<=8;++I)
		{
			double T=(Corner+double(I)/8)*PI/2;
			Ring.Add(C+FVector((Corner==0 || Corner==3?1:-1)*(RX-Radius)+Radius*cos(T),0,(Corner<2?1:-1)*(RZ-Radius)+Radius*sin(T)));
		}
		for(int32 I=0;I<Ring.Num();++I)
		{
			FVector A=Ring[I],D=Ring[(I+1)%Ring.Num()],Up(0,H,0);
			Face({A,D,D+Up,A+Up},F);Face({C+Up,A+Up,D+Up},Roof);
		}
	}
	void Fence(double W,double D,double Y=1,double H=4)
	{ Rail({-W,Y,-D},{W,Y,-D},H);Rail({W,Y,-D},{W,Y,D},H);Rail({W,Y,D},{-W,Y,D},H);Rail({-W,Y,D},{-W,Y,-D},H); }
	void Dish(FVector C,double R,double Rise)
	{
		for(int32 J=0;J<6;++J)for(int32 I=0;I<48;++I)
		{
			auto Q=[&](int32 K,int32 L){double T=double(K)/6,A=2*PI*L/48;return C+FVector(R*T*cos(A),Rise*T*T,R*T*sin(A));};
			if(J==0) Face({C,Q(1,I),Q(1,I+1)},White);else Face({Q(J,I),Q(J+1,I),Q(J+1,I+1),Q(J,I+1)},White);
			if(J==5) Beam(Q(6,I),Q(6,I+1),.8,Metal);
		}
		Beam(C,C+FVector(0,Rise+18,0),2,Metal);
		for(double A:{0.,2.0*PI/3,4.0*PI/3}) Beam(C+FVector(R*cos(A),Rise,R*sin(A)),C+FVector(0,Rise+16,0),.6,Metal);
		Ball(C+FVector(0,Rise+18,0),{3,2,3},Red,12,6);
	}
	void Crane(FVector C,double H,double Reach,uint8 Paint=Gold)
	{
		Box(C+FVector(0,2,0),{16,4,16},Stone);
		for(double X:{-3.,3.})for(double Z:{-3.,3.}) Beam(C+FVector(X,3,Z),C+FVector(X,H,Z),.7,Paint);
		for(int32 I=0;I<12;++I)
		{
			double Y=3+(H-3)*I/12,Y1=3+(H-3)*(I+1)/12;
			for(double S:{-3.,3.}){Beam(C+FVector(-3,Y,S),C+FVector(3,Y1,S),.4,Paint);Beam(C+FVector(S,Y,-3),C+FVector(S,Y1,3),.4,Paint);}
		}
		for(double Z:{-2.5,2.5})for(double Y:{H-2,H+2}) Beam(C+FVector(-Reach*.3,Y,Z),C+FVector(Reach,Y,Z),.6,Paint);
		for(int32 I=0;I<18;++I)
		{
			double X=Reach*(-.3+1.3*I/18),X1=Reach*(-.3+1.3*(I+1)/18);
			for(double Z:{-2.5,2.5})Beam(C+FVector(X,H-2,Z),C+FVector(X1,H+2,Z),.35,Paint);
		}
		Box(C+FVector(-Reach*.23,H,0),{FMath::Abs(Reach)*.14,7,9},Stone);
		Box(C+FVector(5,H-3,6),{8,7,8},GlassWindows,Paint);
		Beam(C+FVector(0,H+9,0),C+FVector(Reach,H+2,0),.25,Dark);
		Beam(C+FVector(0,H,0),C+FVector(0,H+9,0),.6,Paint);
		Beam(C+FVector(Reach*.82,H,0),C+FVector(Reach*.82,H*.35,0),.22,Dark);
		Box(C+FVector(Reach*.82,H*.34,0),{4,3,4},Dark);
	}
};

void Civic(FAuthor& A,int32 Id)
{
	if(Id==10) // Corporate Headquarters: offset white framed towers, rooftop landing pad.
	{
		A.Box({0,8,0},{176,16,174},CreamWindows,Roof);
		A.Box({-34,49,22},{73,80,94},GlassWindows,White);A.Box({42,37,-30},{73,58,98},GlassWindows,White);
		A.Box({-34,91,22},{50,6,58},CreamWindows,Roof);
		for(double X:{-70.5,2.5})for(double Z:{-25.,69.})A.Box({X,49,Z},{1.4,80,1.4},White);
		for(double X:{5.5,78.5})for(double Z:{-79.,19.})A.Box({X,37,Z},{1.4,58,1.4},White);
		for(double Y=18;Y<88;Y+=4) A.Box({-34,Y,22},{74,0.6,95},White);
		for(double Y=18;Y<66;Y+=4) A.Box({42,Y,-30},{74,.6,99},White);
		A.Drum({42,66.2,-30},25,25,.5,Roof);A.Drum({42,66.8,-30},22,22,.2,Gold);
		A.Box({42,67.1,-30},{23,.3,3},White);for(double X:{32.,52.})A.Box({X,67.1,-30},{3,.3,23},White);
		A.Equipment({-35,95,20},18);for(double X:{-78.,78.})for(double Z:{-78.,-30.,35.,78.}) A.Tree(X,Z,1,9);
	}
	else if(Id==20) // Church: stone buttresses, pointed windows and a central gable.
	{
		A.Box({0,23,0},{105,44,134},Stone);A.Gable({0,45,0},{116,0,144},37);
		for(double X:{-58.,58.})for(double Z:{-58.,-22.,14.,50.})
		{
			A.Box({X,21,Z},{10,40,8},White);A.Frustum({X,41,Z},{7,0,6},{1,0,1},13,Stone);
			A.Box({X*.91,27,Z+13},{1,23,9},Solar);A.Beam({X*.93,16,Z+13},{X*.93,38,Z+13},.6,White);
		}
		A.Box({0,26,-69},{30,41,2},Dark);A.Gable({0,46,-70},{33,0,3},14,White);
		A.Beam({0,76,-10},{0,100,-10},1.2,Gold);A.Beam({-10,91,-10},{10,91,-10},1.1,Gold);
		A.Steps({0,0,-82},54,8,7);for(double X:{-80.,80.}) A.Tree(X,55,1,16);
	}
	else if(Id==23) // Library: red rotunda, white drum/colonnade, cupola.
	{
		A.Drum({0,1,0},88,88,5,Stone);A.Drum({0,6,0},72,72,39,CreamWindows);
		for(int32 I=0;I<24;++I){double T=2*PI*I/24;A.Drum({76*cos(T),7,76*sin(T)},1.8,1.8,37,White,8);}
		A.Drum({0,44,0},80,80,4,White);A.Drum({0,48,0},82,5,33,Red,48);
		A.Drum({0,81,0},12,12,12,CreamWindows,16);A.Drum({0,93,0},15,0,7,Gold,24);A.Steps({0,0,-85},50,6,6);
	}
	else if(Id==241) // Mayor: white mansion with hipped slate roof and front portico.
	{
		A.Box({0,29,14},{144,54,112},CreamWindows);A.Frustum({0,56,14},{80,0,64},{45,0,28},28,Roof);
		A.Box({-55,84,30},{9,20,12},BrickWindows);A.Box({48,84,30},{9,20,12},BrickWindows);
		A.Box({0,7,-62},{72,12,38},Stone);for(double X:{-29.,-10.,10.,29.}) A.Drum({X,12,-68},2,2,40,White,12);
		A.Gable({0,53,-64},{82,0,32},18);A.Box({0,27,-43},{19,30,2},Dark);A.Steps({0,0,-85},58,8,7);
		for(double X:{-84.,84.}) A.Tree(X,35,1,24);
	}
	else if(Id==24 || Id==27 || Id==229 || Id==228)
	{
		const bool Museum=Id==24,School=Id==27,Prison=Id==228;
		const uint8 Wall=Museum?White:CreamWindows;
		A.Box({0,3,0},{190,6,185},Stone);
		A.Box({0,40,32},{140,70,84},Wall,Roof);
		for(double X:{-66.,66.}) A.Box({X,26,-20},{47,46,125},Wall,Roof);
		A.Box({0,77,34},{72,7,75},Stone,Roof);
		A.Box({0,85,35},{65,13,65},Wall,Roof);
		if(Museum)
		{
			// Stepped classical wings with broad white colonnades and a raised centre.
			for(double X:{-68.,68.})for(double Z=-70;Z<45;Z+=12) A.Drum({X+(X<0?-25:25),7,Z},1.8,1.8,39,White,8);
			A.Box({0,34,-49},{85,48,15},CreamWindows);A.Gable({0,59,-52},{98,0,23},18,White);
			for(double X:{-36.,-24.,-12.,0.,12.,24.,36.}) A.Drum({X,8,-64},2,2,46,White,12);
			A.Steps({0,0,-83},100,8,8);A.Equipment({0,93,35},13);
		}
		else if(Prison)
		{
			for(double X:{-87.,87.})for(double Z:{-87.,87.}){A.Box({X,38,Z},{24,72,24},CreamWindows);A.Frustum({X,74,Z},{15,0,15},{8,0,8},8,Red);}
			A.Fence(93,92,4,12);A.Box({0,18,-89},{55,30,12},Stone);A.Box({0,16,-96},{24,23,1},Dark);
			for(double X=-10;X<=10;X+=4)A.Beam({X,5,-97},{X,27,-97},.5,Metal);
		}
		else
		{
			A.Box({0,17,-42},{38,28,5},Dark);A.Gable({0,45,-48},{60,0,20},21,Roof);
			for(double X:{-20.,0.,20.}) A.Drum({X,4,-55},1.7,1.7,40,White,10);
			A.Steps({0,0,-73},70,8,5);for(double X:{-87.,87.})for(double Z:{-73.,72.}) A.Tree(X,Z,5,12);
			A.Equipment({-35,76,33},15);A.Equipment({35,76,33},15);
			if(!School){A.Drum({0,92,35},12,12,4,Metal);A.Ball({0,96,35},{12,4,12},White,24,8,true);}
		}
	}
}

void Park(FAuthor& A,bool Zoo)
{
	A.Box({0,1,0},{190,2,190},Grass);A.Fence(94,94,2,4);
	A.Box({0,2.3,0},{12,.4,184},Stone);A.Box({0,2.3,0},{184,.4,12},Stone);
	for(double X:{-47.,47.})for(double Z:{-47.,47.})
	{
		if(Z<0 && X<0) { A.Drum({X,2.4,Z},29,29,.3,Water,32);A.Ball({X+14,4,Z+13},{9,8,7},Stone,10,5); }
		else if(Zoo){A.Box({X,2.6,Z},{69,1,64},Grass);for(double S:{-32.,32.})A.Rail({X+S,3,Z-32},{X+S,3,Z+32},10,Metal,14);A.Rail({X-32,3,Z-32},{X+32,3,Z-32},10,Metal,14);A.Rail({X-32,3,Z+32},{X+32,3,Z+32},10,Metal,14);A.Box({X+10,16,Z+8},{25,26,23},Wood);A.Gable({X+10,29,Z+8},{29,0,28},12);}
		else {A.Tree(X,Z,2,45);A.Tree(X-19,Z+16,2,32);A.Tree(X+21,Z-14,2,39);}
	}
	for(double X:{-81.,81.})for(double Z:{-80.,-40.,0.,40.,80.}) A.Tree(X,Z,2,Zoo?70:87);
	for(double X:{-18.,18.})for(double Z:{-64.,64.}){A.Box({X,7,Z},{9,2,23},Wood);for(double Y:{4.,10.})A.Beam({X-4,Y,Z-10},{X-4,Y,Z+10},.8,Metal);}
	if(Zoo){A.Ball({20,18,48},{22,24,20},Stone,16,8);A.Drum({20,10,48},12,0,65,Stone,10);A.Tree(-40,58,2,55);}
}

void Utilities(FAuthor& A,int32 Id)
{
	A.Box({0,1,0},{188,2,188},Stone);
	if(Id==21 || Id==242) // Desalinization / water treatment.
	{
		A.Box({-60,24,10},{53,45,88},CreamWindows,Roof);
		if(Id==21)A.Tank({32,3,15},57,70);else {A.Drum({32,3,15},57,57,90,Stone,48,false);A.Drum({32,4,15},53,53,85,Water,48);A.Beam({-25,94,15},{89,94,15},1.5,Metal);A.Beam({32,89,15},{32,95,15},2,Metal);}
		for(int32 I=0;I<5;++I){double X=-68+I*29;A.Pipe({X,6,-79},{X,13,-39},2,Solar);A.Pipe({X,13,-39},{X+14,13,-39},2,Solar);}
		if(Id==21) { A.Stack({-69,47,31},5,49);A.Equipment({-60,47,-15},18); }
		else { for(int32 I=0;I<4;++I)A.Box({-67.+I*40.,5,-70},{27,6,24},Water);A.Equipment({-61,48,17},18); }
	}
	else if(Id==231) // Water pump: compact brick pump house and raised pipe bends.
	{
		A.Box({0,28,0},{110,52,118},BrickWindows);A.Gable({0,55,0},{125,0,130},24,Roof);
		for(double X:{-72.,72.}){A.Pipe({X,4,-65},{X,69,-65},5,Solar);A.Pipe({X,69,-65},{X*.64,69,-65},5,Solar);A.Pipe({X*.64,69,-65},{X*.64,47,-65},5,Solar);}
		A.Equipment({0,82,15},19);
	}
	else if(Id==244) // Missile silo: sealed round hatch, track, blast walls and service hut.
	{
		A.Box({0,2,0},{185,4,180},Grass);A.Box({0,4,5},{127,4,133},Stone);
		A.Drum({0,8,5},53,53,5,Metal,48);A.Drum({0,13,5},47,47,1,Roof,48);
		A.Box({0,14,5},{2,.5,91},White);for(double X:{-59.,59.})A.Box({X,8,5},{4,6,160},Metal);
		A.Box({-67,40,61},{36,70,32},CreamWindows,Roof);A.Fence(91,89,4,17);
		A.Beam({-67,76,61},{-67,100,61},.7,White);A.Box({65,21,70},{21,30,18},Metal);
	}
}

void Power(FAuthor& A,int32 Id)
{
	if(Id==217 || Id==218)
	{
		// Original paired hydro slopes stay separately oriented by the XBLD dispatch.
		A.Face({{-96,1,-94},{96,1,-94},{96,89,58},{-96,89,58}},Stone);
		A.Face({{96,1,94},{-96,1,94},{-96,89,58},{96,89,58}},Stone);
		A.Face({{-96,1,94},{-96,1,-94},{-96,89,58}},Stone);A.Face({{96,1,-94},{96,1,94},{96,89,58}},Stone);
		A.Box({0,89,61},{196,5,24},Stone);A.Rail({-95,91,52},{95,91,52},8,Metal,20);
		for(double X:{-65.,-22.,22.,65.}){A.Box({X,47,-13},{13,15,5},Dark);A.Box({X,72,31},{13,12,5},Dark);}
		if(Id==218)for(auto& V:A.M.Vertices){V.X=-V.X;V.Z=-V.Z;}
		return;
	}
	A.Box({0,1,0},{194,2,194},Roof);
	if(Id==220 || Id==221 || Id==226)
	{
		const bool Gas=Id==220,Coal=Id==226;
		A.Box({-12,25,20},{137,48,105},Gas?BrickWindows:CreamWindows,Roof);
		A.Box({59,15,-34},{62,28,61},Gas?BrickWindows:CreamWindows,Roof);
		for(int32 I=0;I<(Coal || Gas?3:2);++I)A.Stack({-54.+I*43.,49,44},6,49);
		if(Coal){A.Box({-58,9,-63},{64,16,40},Dark);A.Beam({-85,15,-65},{-20,46,-20},3,Metal);}
		else if(Gas){for(double X:{-74.,-48.,-22.})A.Tank({X,2,-65},10,26,Metal);for(double X:{30.,66.})A.Ball({X,23,-72},{15,18,15},Solar,24,12);}
		else {A.Tank({57,2,-67},29,33,Red);A.Tank({62,2,56},29,42,Metal);}
		for(double X:{-62.,-30.,2.,34.}){A.Pipe({X,6,-84},{X,6,-38});A.Pipe({X,6,-38},{X,35,-38});}
		for(double X:{-55.,-20.,15.})A.Equipment({X,50,-7},16);
	}
	else if(Id==222) // Nuclear: two waisted cooling towers and domed containment.
	{
		A.Box({-37,30,-22},{111,57,116},CreamWindows,Roof);
		A.Drum({-45,59,0},24,24,15,White);A.Ball({-45,74,0},{24,25,24},White,32,12,true);
		for(double Z:{-48.,53.})
		{
			for(int32 J=0;J<12;++J){double T=double(J)/12,U=double(J+1)/12;auto Radius=[](double V){return 20+18*FMath::Square((V-.65)/.65);};A.Drum({56,3+T*85,Z},Radius(T),Radius(U),85./12,Stone,40,false);}
			A.Drum({56,87.5,Z},25.2,25.2,1,White,40,false);A.Drum({56,80,Z},23,23,.5,Dark,40);
		}
		A.Equipment({-68,59,-55},15);
	}
	else if(Id==223) // Solar heliostats around a compact receiver mast.
	{
		A.Drum({0,2,0},20,17,17,Stone);A.Stack({0,19,0},6,78,false);
		for(int32 Row=-2;Row<=2;++Row)for(int32 Col=-3;Col<=3;++Col)
		{
			if(FMath::Abs(Row)+FMath::Abs(Col)<2)continue;double X=Col*26,Z=Row*34;
			A.Beam({X,2,Z},{X,21,Z},1,Metal);
			A.Face({{X-11,12,Z-11},{X+11,12,Z-11},{X+11,29,Z+11},{X-11,29,Z+11}},Solar);
			A.Beam({X-11,12,Z-11},{X+11,12,Z-11},.5,White);A.Beam({X-11,29,Z+11},{X+11,29,Z+11},.5,White);
		}
		A.Box({-62,15,-82},{45,26,24},BrickWindows,Roof);
	}
	else if(Id==224) // Microwave receiving bowl, radial ribs, feed and low control house.
	{
		A.Drum({0,2,15},40,31,34,Stone);A.Drum({0,36,15},28,57,14,Metal);
		A.Dish({0,50,15},88,27);A.Box({-60,12,-73},{52,22,37},CreamWindows,Roof);A.Equipment({-60,24,-73},17);
	}
	else if(Id==225) // Fusion: rounded reactor hall and an exposed central toroidal chamber.
	{
		A.RoundedHall({0,2,0},88,67,65,20,CreamWindows);A.RoundedHall({0,67,0},88,67,3,20,Metal);
		A.Box({0,72,0},{120,6,85},Metal,Dark);
		A.Drum({0,75,0},35,35,13,Metal,48,false);A.Drum({0,76,0},27,27,3,Solar,48);
		for(int32 I=0;I<16;++I){double T=2*PI*I/16;A.Box({35*cos(T),83,35*sin(T)},{4,13,4},Gold);}
		for(double X:{-72.,72.})for(double Z:{-45.,0.,45.})A.Equipment({X,68,Z},15);
		A.Beam({0,90,0},{0,100,0},.5,White);
	}
	A.Fence(96,96,2,4);
}

void Arcology(FAuthor& A,int32 Id)
{
	A.Box({0,1,0},{194,2,194},Grass);
	if(Id==151) // Plymouth: dark taper, red corner spines and open structural base.
	{
		for(double X:{-83.,83.})for(double Z:{-83.,83.})A.Beam({X,2,Z},{X*.82,26,Z*.82},3,Metal);
		A.Frustum({0,22,0},{86,0,86},{39,0,39},72,GlassWindows);
		for(double X:{-1.,1.})for(double Z:{-1.,1.})A.Beam({X*87,22,Z*87},{X*40,96,Z*40},1.5,Red);
		for(double Y:{23.,72.,94.})A.Box({0,Y,0},{Y<30?175.:Y<80?113.:82.,1.2,Y<30?175.:Y<80?113.:82.},Red,Roof);
		A.Box({0,10,0},{103,16,113},CreamWindows);A.Equipment({20,95,12},16);A.Beam({-20,95,18},{-20,100,18},.6,Solar);
	}
	else if(Id==245) // Forest: stacked silver saucers, exposed support ring, garden torus.
	{
		A.Drum({0,2,0},55,55,13,CreamWindows);A.Drum({0,15,0},38,38,56,GlassWindows);
		for(double Y:{20.,33.,46.,59.}){A.Drum({0,Y,0},55,68,5,White);A.Drum({0,Y+5,0},68,44,6,Metal);A.Drum({0,Y+3,0},68.5,68.5,2,GlassWindows,48);}
		for(double X:{-59.,59.})A.Box({X,30,0},{9,53,18},White,Roof);
		A.Steps({0,1,-83},65,14,12);
		for(int32 I=0;I<8;++I){double T=2*PI*I/8;A.Beam({49*cos(T),8,49*sin(T)},{49*cos(T),64,49*sin(T)},1.5,Metal);}
		A.Drum({0,66,0},38,32,8,White);A.Drum({0,74,0},32,20,7,Metal);
		A.Drum({0,81,0},83,83,3,White,48,false);A.Drum({0,84,0},83,78,3,Grass,48,false);
		for(int32 I=0;I<48;++I){double T=2*PI*I/48,U=2*PI*(I+1)/48;A.Face({{31*cos(T),87,31*sin(T)},{78*cos(T),87,78*sin(T)},{78*cos(U),87,78*sin(U)},{31*cos(U),87,31*sin(U)}},Grass);}
		A.Drum({0,76,0},17,17,21,CreamWindows);A.Ball({0,96,0},{12,4,12},Solar,24,8,true);
		for(int32 I=0;I<20;++I){double T=2*PI*I/20;A.Tree(67*cos(T),67*sin(T),87,8);A.Beam({83*cos(T),83,83*sin(T)},{18*cos(T),89,18*sin(T)},.6,White);}
	}
	else if(Id==246) // Darco: biomorphic black stepped shell, ribs, eyes and organic legs.
	{
		for(double X:{-1.,1.})for(double Z:{-1.,1.})
		{
			A.Beam({X*81,3,Z*67},{X*60,26,Z*45},7,Dark);A.Beam({X*60,26,Z*45},{X*23,49,Z*13},10,Dark);
			A.Ball({X*60,27,Z*45},{13,12,14},Dark,20,10);
		}
		A.Ball({0,56,8},{52,28,38},Dark,32,20);A.Ball({-16,83,9},{28,17,30},Dark,32,16);
		A.Ball({-35,71,-4},{45,8,23},Dark,32,12);A.Ball({34,48,7},{53,9,44},Dark,32,12);
		for(int32 I=0;I<16;++I){double T=2*PI*I/16;for(int32 J=0;J<14;++J){double Y=32+J*3.5,Y1=Y+3.5;auto R=[](double H){return 51*sqrt(FMath::Max(.05,1-FMath::Square((H-56)/29)));};A.Beam({R(Y)*cos(T),Y,8+R(Y)*.72*sin(T)},{R(Y1)*cos(T),Y1,8+R(Y1)*.72*sin(T)},.65,Metal);}}
		A.Box({-24,87,-20},{18,3,2},GlassWindows);A.Box({-5,88,-21},{14,3,2},GlassWindows);
		// Rows of small amber lights follow the organic shell, as in the SC2000 sprite.
		for(int32 Row=0;Row<8;++Row)for(int32 Col=0;Col<18;++Col)
		{
			double Y=40+Row*5,T=2*PI*Col/18,R=52*sqrt(FMath::Max(.05,1-FMath::Square((Y-56)/28)));
			A.Box({R*cos(T),Y,8+R*.74*sin(T)},{1.3,.65,1.3},Gold);
		}
		A.Drum({-14,98,6},11,5,2,Red);A.Drum({-48,2,-55},31,31,.5,Water);
		for(double X:{-70.,70.}) A.Tree(X,72,2,10);
	}
	else if(Id==152) // Launch: silver cylindrical habitat, clear-framed biosphere and thrust legs.
	{
		for(double X:{-1.,1.})for(double Z:{-1.,1.}){A.Beam({X*69,3,Z*64},{X*33,30,Z*32},7,Metal);A.Drum({X*69,2,Z*64},14,9,7,White);}
		A.Drum({0,26,0},45,80,9,Metal);A.Drum({0,35,0},80,80,36,Metal,64);
		for(int32 I=0;I<12;++I)
		{
			double T=2*PI*I/12;A.Ball({80*cos(T),53,80*sin(T)},{7,13,7},White,16,10);
			for(double Y:{40.,52.,64.})for(double D:{-.12,.12})A.Box({81*cos(T+D),Y,81*sin(T+D)},{3,5,3},GlassWindows);
		}
		for(double Y:{35.,47.,59.,70.}) A.Drum({0,Y,0},81,81,1.2,Metal,64);
		for(int32 I=0;I<12;++I){double T=2*PI*I/12;A.Beam({81*cos(T),36,81*sin(T)},{81*cos(T),70,81*sin(T)},.8,Metal);}
		A.Ball({0,72,0},{79,28,79},16,48,16,true);
		A.Drum({0,71,0},79,79,1,Grass,64);A.Drum({-22,72,-15},25,25,.25,Water,40);
		A.Ball({28,75,15},{13,10,15},Stone,12,8);A.Drum({29,75,16},12,1,16,Stone,12);
		for(int32 I=0;I<15;++I){double T=2*PI*I/15;A.Tree(57*cos(T),57*sin(T),72,5+I%4);}
		// Sparse glazing ribs leave the reference's landscape visible from every side.
		for(int32 I=0;I<12;++I)for(int32 J=0;J<12;++J){double T=2*PI*I/12,U=PI*.5*J/12,V=PI*.5*(J+1)/12;A.Beam({79*cos(U)*cos(T),72+28*sin(U),79*cos(U)*sin(T)},{79*cos(V)*cos(T),72+28*sin(V),79*cos(V)*sin(T)},.32,White);}
		for(double Y:{.35,.7}){double R=79*cos(Y*PI/2);A.Drum({0,72+28*sin(Y*PI/2),0},R,R,.15,White,64,false);}
		A.Beam({0,6,-85},{0,25,-40},13,Metal);
	}
	else if(Id==153) // Braun Llama Dome: gold arches, tapering core, panoramic ring.
	{
		A.Frustum({0,2,0},{75,0,75},{20,0,20},54,CreamWindows);A.Drum({0,51,0},10,10,29,Metal,24);
		for(int32 I=0;I<4;++I)for(int32 J=0;J<18;++J){double T=PI*.5*I;auto Q=[&](int32 K){double U=double(K)/18;return FVector(83*cos(T)*cos(U*PI/2),3+66*sin(U*PI/2),83*sin(T)*cos(U*PI/2));};A.Beam(Q(J),Q(J+1),2.2,Gold);}
		A.Drum({0,79,0},47,47,3,White,48);A.Drum({0,82,0},47,47,6,GlassWindows,48);A.Drum({0,88,0},48,38,2,Metal,48);
		A.Drum({0,90,0},17,17,4,White);A.Drum({0,94,0},20,10,2,Metal);A.Beam({0,96,0},{0,100,0},1.2,Gold);
		A.Steps({0,1,-88},79,15,13);for(double X:{-87.,87.})for(double Z:{-75.,75.})A.Tree(X,Z,2,9);
	}
}
}

bool IsSelected(int32 Id)
{
	switch(Id)
	{
	case 10:case 20:case 21:case 23:case 24:case 27:case 141:case 143:
	case 151:case 152:case 153:case 213:case 214:case 217:case 218:
	case 220:case 221:case 222:case 223:case 224:case 225:case 226:
	case 228:case 229:case 230:case 231:case 241:case 242:case 244:case 245:case 246:
	case 324:case 326:case 367:return true;
	default:return false;
	}
}
bool IsDetail(const FMaxisMeshFace& F){return F.FaceType==15 && F.TextureAtlasIndex==DetailTag;}
FLinearColor FinishColor(uint8 F){return FLinearColor(F&1,(F>>1)&1,(F>>2)&1,(F>>3)&1);}
bool IsCausewayTile(int32 Id){return Id>=0x49 && Id<=0x50;}
bool IsCausewayModel(int32 Id){return Id>=247 && Id<=250;}

FMaxisMeshObject MakeModel(const FMaxisMeshObject& Original)
{
	if(!IsSelected(Original.Header.Id) || Original.Vertices.IsEmpty())return Original;
	FBox Bounds(ForceInit);for(const auto& V:Original.Vertices)Bounds+=FVector(V.X,V.Y,V.Z)/65536.;
	FMaxisMeshObject R=Original;R.Vertices.Reset();R.Faces.Reset();R.Header.bConsistentWinding=true;
	const FVector Extent=Bounds.GetSize();
	FAuthor A(R,{Extent.X/200,FMath::Max(1.,Bounds.Max.Y)/100,Extent.Z/200});
	const int32 Id=R.Header.Id;
	if(Id>=217 && Id<=226)Power(A,Id);
	else if(Id==151 || Id==152 || Id==153 || Id==245 || Id==246)Arcology(A,Id);
	else if(Id==324 || Id==326 || Id==367)Park(A,Id==367);
	else if(Id==21 || Id==231 || Id==242 || Id==244)Utilities(A,Id);
	else if(Id==213 || Id==214)
	{
		A.Box({0,1,0},{185,2,185},Wood);A.Box({-14,3,8},{135,4,140},Stone);
		const int32 Floors=Id==213?3:2;
		for(int32 J=0;J<Floors;++J)
		{
			double Y=5+J*12;A.Box({-14,Y,8},{134,1.5,137},Stone);
			for(double X:{-74.,-35.,5.,45.})for(double Z:{-53.,-10.,33.,72.})A.Box({X,Y+6,Z},{2.5,12,2.5},Red);
		}
		A.Crane({61,1,50},89,-132,Id==213?Gold:Red);
		for(int32 I=0;I<5;++I)A.Box({-68.+I*16.,6,-81},{12,9,10},Id==213?BrickWindows:Metal);
	}
	else if(Id==141) // Harbour crane: rotating pedestal and raised lattice jib.
	{
		A.Box({0,2,0},{180,4,170},Stone);
		A.Frustum({25,4,0},{44,0,44},{30,0,30},28,Gold);
		A.Drum({25,32,0},31,31,5,Dark);
		A.Box({25,44,0},{56,18,48},Gold,Roof);
		A.Box({4,48,-25},{23,14,3},GlassWindows);
		for(double Z:{-7.,7.})
		{
			A.Beam({20,54,Z},{-82,99,Z},1.2,Gold);
			A.Beam({37,54,Z},{-82,99,Z},1.2,Gold);
			for(int32 I=0;I<10;++I){double T=double(I)/10,U=double(I+1)/10;A.Beam({FMath::Lerp(20.,-82.,T),54+45*T,Z},{FMath::Lerp(37.,-82.,U),54+45*U,Z},.7,Gold);}
		}
		A.Beam({-82,98,0},{-82,42,0},.3,Dark);A.Box({-82,40,0},{5,4,5},Dark);
		A.Beam({41,54,0},{-82,98,0},.3,Dark);A.Box({51,44,0},{16,15,46},Dark);
	}
	else if(Id==143)
	{
		A.Box({0,4,0},{160,8,150},Stone);A.Drum({0,8,0},28,23,64,CreamWindows,24);
		A.Drum({0,72,0},43,48,4,Dark);A.Drum({0,76,0},48,48,9,GlassWindows,24);A.Drum({0,85,0},50,43,3,Metal);
		A.Drum({0,88,0},24,24,3,Roof);A.Beam({-12,91,0},{-12,100,0},.6,Red);A.Dish({10,90,0},12,2);
		for(int32 I=0;I<12;++I){double T=2*PI*I/12;A.Beam({48*cos(T),76,48*sin(T)},{48*cos(T),85,48*sin(T)},.8,Gold);}
	}
	else if(Id==230)
	{
		A.Box({0,2,0},{190,4,190},White);A.Box({0,5,0},{167,3,167},Water);A.Box({0,10,0},{70,9,70},Stone);
		A.Frustum({0,15,0},{24,0,24},{17,0,17},14,Stone);
		A.Beam({-12,29,0},{-8,56,0},5,Copper);A.Beam({12,29,0},{8,56,0},5,Copper);
		A.Ball({0,66,0},{20,23,12},Copper,20,12);A.Ball({0,91,0},{11,9,10},Copper,20,10);
		A.Beam({-17,77,0},{-24,53,-3},4,Copper);A.Beam({17,78,0},{28,91,-6},4,Copper);
		A.Box({-22,54,-4},{13,17,4},Copper);
	}
	else Civic(A,Id);
	// Retain the original warning-light colors and phase metadata, moving their anchors
	// onto the corresponding new roofs/rims. Face 25 is consumed by the shared blink pool.
	int32 BeaconIndex=0;
	for(const auto& SourceFace:Original.Faces)if(SourceFace.FaceType==25 && !SourceFace.VertexIndices.IsEmpty())
	{
		const auto& SourceVertex=Original.Vertices[SourceFace.VertexIndices[0]];
		const FVector Old=FVector(SourceVertex.X,SourceVertex.Y,SourceVertex.Z)/65536.;
		FVector Position=Old/A.Scale;
		if(Id==143)Position={-12,97.+BeaconIndex,0};
		else if(Id==152)Position.Y=72+28*sqrt(FMath::Max(0.,1-(Position.X*Position.X+Position.Z*Position.Z)/(79*79)));
		else if(Id==153)Position.Y=100;
		else if(Id==222)
		{
			if(BeaconIndex==0)Position={-68,64,-55};
			else {double T=2*PI*((BeaconIndex-1)%6)/6;Position={56+25.2*cos(T),88.5,(BeaconIndex<=6?53.:-48.)+25.2*sin(T)};}
		}
		else if(Id==245)
		{
			if(Position.Y>90)Position={FMath::Sign(Position.X)*2.,100,0};
			else {double T=atan2(Position.Z,Position.X);Position={83*cos(T),84,83*sin(T)};}
		}
		else if(Id==246)Position={-14,100,6};
		FMaxisMeshFace Beacon=SourceFace;Beacon.VertexIndices.Reset();Beacon.VertexIndices.Add(R.Vertices.Num());Beacon.VertexCount=1;
		const FVector V=A.P(Position);R.Vertices.Add({int32(V.X*65536),int32(V.Y*65536),int32(V.Z*65536)});R.Faces.Add(MoveTemp(Beacon));++BeaconIndex;
	}
	for(auto& F:R.Faces)if(F.FaceType==15)F.TextureAtlasIndex=DetailTag;
	A.B.Finish();return R;
}

FMaxisMeshObject MakeCauseway(const FMaxisMeshObject& Original,bool Support)
{
	FMaxisMeshObject R=Original;if(!IsCausewayModel(R.Header.Id))return R;
	// Keep exact road polygons, UVs and slope profiles; detail is below/outside the road surface.
	const int32 First=R.Faces.Num();SimCopterLandmarkGeometry::FBuilder B{R};
	const bool AlongX=R.Header.Id==248 || R.Header.Id==250;
	auto P=[&](FVector V){return AlongX?FVector(V.Z,V.Y,V.X):V;};
	for(double X:{-25.,25.})
	{
		B.Beam(P({X,29,-32}),P({X,29,32}),.9,Metal);
		if(R.Header.Id==247 || R.Header.Id==248) B.Beam(P({X,26,-32}),P({X,26,32}),1.2,Stone);
	}
	if(Support && (R.Header.Id==247 || R.Header.Id==248))
	{
		// One slender pier every eight tiles: 32 metres between centres at the default scale.
		B.Box(P({0,-15,0}),AlongX?FVector(7,82,8):FVector(8,82,7),Stone);
		B.Box(P({0,25,0}),AlongX?FVector(9,4,47):FVector(47,4,9),Stone);
		B.Box(P({0,-55,0}),{17,4,17},Stone);
	}
	for(int32 I=First;I<R.Faces.Num();++I)R.Faces[I].TextureAtlasIndex=DetailTag;
	B.Finish();return R;
}
}

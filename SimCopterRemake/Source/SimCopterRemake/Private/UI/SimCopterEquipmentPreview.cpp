#include "SimCopterEquipmentPreview.h"
#include "City/SimCopterHangarGeometry.h"
#include "Flight/SimCopterHelicopterPresentation.h"
#include "Flight/SimCopterHelicopterRegistry.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/SimCopterHangarShop.h"

namespace SimCopterEquipmentPreview
{
const TCHAR* GetFittingDescription(int32 Row)
{
	const TCHAR* Descriptions[] = {
		TEXT("Water bucket suspended below the skids."),
		TEXT("Cabin-side loudspeaker ready for announcements."),
		TEXT("Twin canister launcher fitted beside the cabin."),
		TEXT("Forward water nozzle fitted below the nose."),
		TEXT("Side winch and rescue sling ready to deploy."),
		TEXT("Skid-mounted capture cage lowered for recovery."),
		TEXT("Nose-mounted taser and aiming scope.")
	};
	return Row >= 0 && Row < UE_ARRAY_COUNT(Descriptions) ? Descriptions[Row] : TEXT("");
}

FModel Build(int32 TypeIndex, int32 OwnedMask, int32 HighlightRow)
{
	FModel Result;
	const auto* Definition = SimCopterHelicopterRegistry::FindByTypeIndex(TypeIndex);
	if (!Definition) { Result.Error = TEXT("Select a helicopter to preview."); return Result; }
	FString LoadError;
	const auto Library = FMaxisMeshLibrary::GetShared(SimCopterOriginalGame::ResolveRoot(), LoadError);
	if (!Library) { Result.Error = TEXT("Helicopter preview unavailable."); return Result; }
	using SimCopterHangarGeometry::FProp;
	FProp Model;
	auto Object = [&](FProp& Target, const FString& Name, FVector Offset = FVector::ZeroVector)
	{
		const TArray<FColor>* Palette = nullptr;
		if (const auto* Source = Library->FindObjectByTableName(Name, &Palette))
		{
			TArray<FColor> Paint;
			if (Palette) SimCopterHelicopterPresentation::MakePaintPalette(*Palette, Paint, TypeIndex);
			FMaxisMeshSection Solid, Blur;
			FMaxisProceduralMeshBuilder::BuildPaletteColoredSections(*Source, Palette ? &Paint : nullptr,
				262144, 1, false, FLinearColor(.35f,.38f,.4f), Solid, &Blur);
			Target.Append(Solid, Offset);
		}
	};
	// The same GEO hierarchy/offsets as FUN_00483c20, expressed in native model units.
	Object(Model, Definition->BodyObjectName);
	if (Model.IsEmpty()) { Result.Error = TEXT("Helicopter geometry unavailable."); return Result; }
	const FBox Body = Model.Bounds();
	Object(Model, Definition->MainRotorObjectName);
	if (!Definition->bNoTailRotor) Object(Model, TEXT("ROTORTL"), Definition->ToTailRotorOffsetCm(.25f));
	const float Side = Body.Max.Y + .22f;
	const float Bottom = Body.Min.Z;
	const float CabinX = FMath::Lerp(Body.Min.X, Body.Max.X, .76);
	const float CabinZ = FMath::Lerp(Body.Min.Z, Body.Max.Z, .4);
	const FLinearColor Steel(.24f,.3f,.33f), Rubber(.035f,.045f,.05f), Orange(.95f,.36f,.055f);
	for (int32 Row = 0; Row < SimCopterHangarShop::UpgradeRowCount; ++Row)
	{
		const bool bHighlighted = Row == HighlightRow;
		if (!bHighlighted && !(OwnedMask & SimCopterHelicopterRegistry::GetToolCareerBit(SimCopterHangarShop::GetToolForUpgradeRow(Row)))) continue;
		FProp Fitting;
		const FLinearColor Accent = bHighlighted ? Orange : Steel;
		switch (Row)
		{
		case 0:
		{
			const FVector Mount(CabinX-.5f, 0, Bottom);
			const FVector Bucket = Mount - FVector(0,0,bHighlighted ? 1.8f : .65f);
			Fitting.Bar(Mount, Bucket, .035f, Steel);
			Object(Fitting, TEXT("BUCKET"), Bucket);
			Fitting.Cylinder(Bucket, FVector::UpVector, .45f, .12f, Accent);
			break;
		}
		case 1:
			Fitting.Bar({CabinX,Side-.35f,CabinZ},{CabinX,Side+.3f,CabinZ},.06f,Steel);
			Fitting.Cylinder({CabinX,Side+.3f,CabinZ},FVector::XAxisVector,.19f,.5f,Accent);
			Fitting.Cylinder({CabinX+.28f,Side+.3f,CabinZ},FVector::XAxisVector,.33f,.16f,Accent);
			Fitting.Cylinder({CabinX+.37f,Side+.3f,CabinZ},FVector::XAxisVector,.26f,.015f,Rubber);
			break;
		case 2:
			Fitting.Box({CabinX-.8f,Side,CabinZ-.3f},{.7f,.3f,.2f},Steel);
			for (float Y : {Side,Side+.27f})
			{
				Fitting.Cylinder({CabinX-.6f,Y,CabinZ-.15f},FVector::XAxisVector,.12f,.75f,Accent);
				Fitting.Cylinder({CabinX-.21f,Y,CabinZ-.15f},FVector::XAxisVector,.095f,.02f,Rubber);
			}
			break;
		case 3:
			Object(Fitting, TEXT("CANNON"));
			Fitting.Bar({CabinX,0,Bottom+.1f},{Body.Max.X+.2f,0,Bottom+.1f},.09f,Accent);
			Fitting.Cylinder({Body.Max.X+.25f,0,Bottom+.1f},FVector::XAxisVector,.16f,.35f,Accent);
			break;
		case 4:
		{
			Object(Fitting, TEXT("BRACKET"));
			const FVector Mount(CabinX-.2f,Side+.65f,CabinZ+.7f);
			Fitting.Bar({CabinX-.2f,Side-.3f,CabinZ+.1f},Mount,.06f,Accent);
			Fitting.Cylinder(Mount,FVector::YAxisVector,.22f,.32f,Accent);
			const FVector Sling = Mount - FVector(0,0,bHighlighted ? 2.2f : .55f);
			Fitting.Bar(Mount,Sling,.025f,Steel);
			Object(Fitting, TEXT("HARNESS"), Sling);
			Fitting.Bar(Sling,Sling+FVector(-.3f,0,-.45f),.055f,Orange);
			Fitting.Bar(Sling,Sling+FVector(.3f,0,-.45f),.055f,Orange);
			Fitting.Bar(Sling+FVector(-.3f,0,-.45f),Sling+FVector(.3f,0,-.45f),.055f,Orange);
			break;
		}
		case 6:
			Fitting.Box({Body.Max.X+.15f,0,Bottom+.25f},{.45f,.22f,.22f},Steel);
			Fitting.Cylinder({Body.Max.X+.5f,0,Bottom+.25f},FVector::XAxisVector,.12f,.5f,Accent);
			break;
		case 5:
		{
			const float Z = Bottom - (bHighlighted ? 1.8f : .4f);
			for (float X : {CabinX-1.1f,CabinX+.5f}) for (float Y : {-.7f,.7f})
			{
				Fitting.Bar({X,Y,Bottom},{X,Y,Z},.025f,Steel);
				Fitting.Bar({X,Y,Z},{X,Y,Z-.8f},.045f,Accent);
			}
			for (float Y : {-.7f,.7f}) for (float Height : {Z,Z-.4f,Z-.8f})
				Fitting.Bar({CabinX-1.1f,Y,Height},{CabinX+.5f,Y,Height},.035f,Accent);
			for (float X : {CabinX-1.1f,CabinX-.3f,CabinX+.5f})
				Fitting.Bar({X,-.7f,Z-.8f},{X,.7f,Z-.8f},.04f,Accent);
			break;
		}
		}
		if (bHighlighted) Result.HighlightTriangleCount = Fitting.Triangles.Num()/3;
		Model.Append(Fitting);
	}
	Result.Bounds = Model.Bounds();
	Result.Mesh = MoveTemp(Model);
	return Result;
}
}

void SSimCopterEquipmentPreview::Construct(const FArguments& Args)
{
	TypeIndex=Args._TypeIndex; OwnedMask=Args._OwnedMask; HighlightRow=Args._HighlightRow;
	SetVisibility(EVisibility::HitTestInvisible);
}

void SSimCopterEquipmentPreview::Tick(const FGeometry&, double, float)
{
	const int32 Type=TypeIndex.Get(), Mask=OwnedMask.Get(), Row=HighlightRow.Get();
	if (Type==LastType && Mask==LastMask && Row==LastRow) return;
	LastType=Type; LastMask=Mask; LastRow=Row;
	const auto Model=SimCopterEquipmentPreview::Build(Type,Mask,Row);
	Error=Model.Error; Triangles.Reset(); ProjectedBounds=FBox2D(ForceInit);
	// A fixed right-front three-quarter view keeps the side winch and nose fittings visible.
	const FVector View=FVector(1,1.8,.8).GetSafeNormal();
	const FVector Right=FVector::CrossProduct(FVector::UpVector,View).GetSafeNormal();
	const FVector Up=FVector::CrossProduct(View,Right);
	const FVector Light=FVector(.4,1,2).GetSafeNormal();
	const auto& Mesh=Model.Mesh;
	for (int32 I=0; I+2<Mesh.Triangles.Num(); I+=3)
	{
		FTriangle Face; Face.Depth=0;
		FLinearColor Color(0,0,0,0);
		for (int32 Corner=0; Corner<3; ++Corner)
		{
			const int32 Index=Mesh.Triangles[I+Corner];
			const FVector P=Mesh.Vertices[Index];
			Face.Points[Corner]=FVector2f(FVector::DotProduct(P,Right),-FVector::DotProduct(P,Up));
			ProjectedBounds+=FVector2D(Face.Points[Corner]);
			Face.Depth+=FVector::DotProduct(P,View)/3;
			Color+=Mesh.VertexColors[Index]*(.7f+.3f*FMath::Abs(FVector::DotProduct(Mesh.Normals[Index],Light)))/3;
		}
		Color.A=1; Face.Color=Color; Triangles.Add(Face);
	}
	Triangles.StableSort([](const FTriangle& A,const FTriangle& B){return A.Depth<B.Depth;});
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SSimCopterEquipmentPreview::OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,
	FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool) const
{
	const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	FSlateDrawElement::MakeBox(Out,Layer,Geometry.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.27f,.33f,.35f));
	if (Triangles.IsEmpty())
	{
		FSlateDrawElement::MakeText(Out,Layer+1,Geometry.ToPaintGeometry(),Error,
			FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),10),ESlateDrawEffect::None,FLinearColor::White);
		return Layer+1;
	}
	const FVector2f Size(Geometry.GetLocalSize()), Span(ProjectedBounds.GetSize());
	const float Scale=FMath::Min((Size.X-24)/FMath::Max(Span.X,1.f),(Size.Y-24)/FMath::Max(Span.Y,1.f));
	const FVector2f Center(ProjectedBounds.GetCenter());
	const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
	TArray<FSlateVertex> Vertices; TArray<SlateIndex> Indices;
	for (const auto& Face : Triangles)
	{
		for (int32 Corner=0; Corner<3; ++Corner)
		{
			Indices.Add(static_cast<SlateIndex>(Vertices.Num()));
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
				Size*.5f+(Face.Points[Corner]-Center)*Scale,FVector2f(.5f,.5f),(Face.Color*Style.GetColorAndOpacityTint()).ToFColor(true)));
		}
	}
	FSlateDrawElement::MakeCustomVerts(Out,Layer+1,Resource,Vertices,Indices,nullptr,0,0);
	return Layer+1;
}

void SSimCopterEquipmentIcon::Construct(const FArguments& Args)
{
	Row=Args._Row; Owned=Args._Owned; Highlighted=Args._Highlighted;
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SSimCopterEquipmentIcon::OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,
	FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
	const FLinearColor Ink=Highlighted.Get() ? FLinearColor(1,.68f,.25f) : FLinearColor(.72f,.82f,.85f);
	FSlateDrawElement::MakeBox(Out,Layer,Geometry.ToPaintGeometry(),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None,FLinearColor(.04f,.08f,.10f));
	auto Line=[&](std::initializer_list<FVector2f> Points,FLinearColor Color,float Width=1.8f)
	{
		TArray<FVector2f> Scaled; const FVector2f Scale=FVector2f(Geometry.GetLocalSize())/34.f;
		for (auto P:Points) Scaled.Add(P*Scale);
		FSlateDrawElement::MakeLines(Out,Layer+1,Geometry.ToPaintGeometry(),Scaled,ESlateDrawEffect::None,Color,true,Width);
	};
	switch(Row)
	{
	case 0: Line({{8,13},{11,26},{24,26},{27,13},{8,13}},Ink); Line({{10,13},{13,6},{22,6},{25,13}},Ink); break;
	case 1: Line({{7,14},{14,14},{26,7},{26,25},{14,19},{7,19},{7,14}},Ink); Line({{14,20},{16,27}},Ink); break;
	case 2: Line({{7,12},{26,12},{26,17},{7,17},{7,12}},Ink); Line({{7,20},{26,20},{26,25},{7,25},{7,20}},Ink); break;
	case 3: Line({{6,25},{6,19},{18,19},{18,12},{27,12}},Ink); Line({{6,28},{12,28}},Ink); Line({{23,8},{29,8},{29,16},{23,16}},Ink); break;
	case 4: Line({{8,9},{24,9},{24,14},{16,14},{16,20},{9,28},{24,28},{16,20}},Ink); Line({{8,7},{8,17}},Ink); break;
	case 6: Line({{5,13},{23,13},{23,20},{5,20},{5,13}},Ink); Line({{23,15},{30,15},{30,18},{23,18}},Ink); Line({{10,10},{19,10}},Ink); break;
	case 5: Line({{7,12},{27,12},{27,28},{7,28},{7,12},{16,5},{27,12}},Ink); Line({{13,12},{13,28}},Ink); Line({{21,12},{21,28}},Ink); Line({{7,21},{27,21}},Ink); break;
	}
	if (Owned.Get())
	{
		Line({{22,5},{26,9},{32,2}},FLinearColor(.015f,.055f,.025f),5);
		Line({{22,5},{26,9},{32,2}},FLinearColor(.3f,1,.55f),2.5f);
	}
	return Layer+1;
}

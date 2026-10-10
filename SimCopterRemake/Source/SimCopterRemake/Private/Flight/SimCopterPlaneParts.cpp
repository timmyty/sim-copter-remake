#include "Flight/SimCopterPlaneParts.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Formats/MaxisProceduralMeshBuilder.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

USimCopterPlanePartsComponent::USimCopterPlanePartsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

bool USimCopterPlanePartsComponent::IsPropellerFace(const FMaxisMeshObject& Object, const FMaxisMeshFace& Face)
{
	// Measured PLANE1 nose: both blades, spinner and the two type-11 blur discs
	// lie beyond authored Z=3.60 metres. GEO forward is Unreal X.
	if (Face.VertexIndices.IsEmpty() || Face.FaceType == 25) return false;
	for (uint16 Index : Face.VertexIndices)
		if (!Object.Vertices.IsValidIndex(Index) || Object.Vertices[Index].Z < 3.60 * 262144) return false;
	return true;
}

bool USimCopterPlanePartsComponent::IsGearFace(const FMaxisMeshObject& Object, const FMaxisMeshFace& Face)
{
	// Three wheel fairings and their supporting struts descend below Y=-1.19.
	// Exclude the propeller, whose lower blade reaches that height as well.
	bool bBelow = false;
	for (uint16 Index : Face.VertexIndices)
	{
		if (!Object.Vertices.IsValidIndex(Index) || Object.Vertices[Index].Z > 3.5 * 262144) return false;
		bBelow |= Object.Vertices[Index].Y < -1.19 * 262144;
	}
	return bBelow;
}

void USimCopterPlanePartsComponent::RemoveMovingFaces(FMaxisMeshObject& Body)
{
	Body.Faces.RemoveAll([&Body](const FMaxisMeshFace& Face) { return IsPropellerFace(Body, Face) || IsGearFace(Body, Face); });
}

void USimCopterPlanePartsComponent::Build(const FMaxisMeshObject& Source, const TArray<FColor>* Palette,
	float UnitsPerCm, float Scale, UMaterialInterface* Paint, UMaterialInterface* Blur)
{
	FMaxisMeshObject Prop = Source, Wheels = Source;
	Prop.Faces.RemoveAll([&Source](const auto& Face) { return !IsPropellerFace(Source, Face); });
	Wheels.Faces.RemoveAll([&Source](const auto& Face) { return !IsGearFace(Source, Face); });
	FMaxisMeshSection Blades, Disc, Undercarriage;
	FMaxisProceduralMeshBuilder::BuildPaletteColoredSections(Prop, Palette, UnitsPerCm, Scale, true,
		FLinearColor::Gray, Blades, &Disc);
	FMaxisProceduralMeshBuilder::BuildPaletteColoredSection(Wheels, Palette, UnitsPerCm, Scale, true,
		FLinearColor::Gray, Undercarriage);
	const FVector Hub(3.64 * 262144 / UnitsPerCm * Scale, .02 * 262144 / UnitsPerCm * Scale, -.21 * 262144 / UnitsPerCm * Scale);
	auto Make = [this](const TCHAR* Name)
	{
		// The ambient fleet owns several planes. Each needs independent blades and
		// wheels; reusing an actor-level component name would rebuild another plane's parts.
		auto* Mesh = NewObject<UProceduralMeshComponent>(GetOwner(),
			MakeUniqueObjectName(GetOwner(), UProceduralMeshComponent::StaticClass(), FName(Name)));
		Mesh->SetupAttachment(this);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		GetOwner()->AddInstanceComponent(Mesh); Mesh->RegisterComponent();
		return Mesh;
	};
	Propeller = Make(TEXT("PlanePropeller")); Gear = Make(TEXT("PlaneLandingGear"));
	auto Fill = [](UProceduralMeshComponent* Mesh, int32 Index, FMaxisMeshSection& Section, UMaterialInterface* Material, FVector Offset)
	{
		for (auto& Vertex : Section.Vertices) Vertex -= Offset;
		Mesh->CreateMeshSection_LinearColor(Index, Section.Vertices, Section.Triangles, Section.Normals,
			Section.UVs, Section.VertexColors, Section.Tangents, false);
		Mesh->SetMaterial(Index, Material);
	};
	Fill(Propeller, 0, Blades, Paint, Hub);
	if (Blur && !Disc.IsEmpty())
	{
		auto* Material = UMaterialInstanceDynamic::Create(Blur, this);
		Material->SetScalarParameterValue(TEXT("DiscOpacity"), .12f);
		Material->SetVectorParameterValue(TEXT("DiscColor"), FLinearColor(.18f, .18f, .18f));
		Fill(Propeller, 1, Disc, Material, Hub);
	}
	Propeller->SetRelativeLocation(Hub);
	Fill(Gear, 0, Undercarriage, Paint, FVector::ZeroVector);
	PreviousWorld = GetComponentLocation();
	SetPresentationTime(0, false, true);
}

void USimCopterPlanePartsComponent::SetPresentationTime(float Seconds, bool bSpinning, bool bGearDown)
{
	const auto* Body = Cast<UPrimitiveComponent>(GetAttachParent());
	const bool bBodyVisible = !Body || Body->IsVisible();
	if (Propeller)
	{
		Propeller->SetVisibility(bBodyVisible);
		// A non-integer number of turns per presentation frame prevents a frozen
		// stroboscopic blade at common 30/60 Hz rates. The translucent disc carries speed.
		if (bSpinning) PropellerAngle = FMath::Fmod(Seconds * 1733.0f, 360.0f);
		Propeller->SetRelativeRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(PropellerAngle)));
		Propeller->SetMeshSectionVisible(1, bSpinning);
	}
	if (Gear) Gear->SetVisibility(bBodyVisible && bGearDown);
}

bool USimCopterPlanePartsComponent::IsGearVisible() const { return Gear && Gear->IsVisible(); }

void USimCopterPlanePartsComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (Propeller) { Propeller->DestroyComponent(); Propeller = nullptr; }
	if (Gear) { Gear->DestroyComponent(); Gear = nullptr; }
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USimCopterPlanePartsComponent::TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(Dt, Type, Tick);
	const auto* Plane = Cast<ASimCopterHelicopterPawn>(GetOwner());
	const bool bMoving = !GetComponentLocation().Equals(PreviousWorld, .01f);
	PreviousWorld = GetComponentLocation();
	const bool bSpinning = bMoving || (Plane && Plane->IsEngineRunning());
	if (bSpinning) RunningSeconds += Dt;
	SetPresentationTime(RunningSeconds, bSpinning, Plane ? Plane->IsPlaneLandingGearDown() : true);
}

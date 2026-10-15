#include "City/SimCity2000CityActor.h"
#include "Algo/Reverse.h"
#include "City/SimCopterRuntimeStaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Formats/MaxisMeshReader.h"

void ASimCity2000CityActor::BuildWindTurbines(const FMaxisMeshObject& OriginalModel)
{
	if (!Buildings.ContainsByPredicate([](const auto& B) { return B.XbldId == 200 && !B.bDemolished; })) return;
	// The user-requested three-blade treatment replaces PP200's type-11 disc.
	// Take its pivot and swept radius from the original GEO rather than guessing
	// a new scale. Original tower, nacelle, placement and demolition remain intact.
	const float OriginalMeshScale = OriginalMeshSourceTileSize > 0 ? TileSize / OriginalMeshSourceTileSize : 1;
	FBox Disc(ForceInit);
	for (const auto& Face : OriginalModel.Faces) if (Face.FaceType == 11)
		for (uint16 Index : Face.VertexIndices) if (OriginalModel.Vertices.IsValidIndex(Index))
		{
			FVector P = FMaxisMeshReader::ConvertMaxisVertexToUnreal(OriginalModel.Vertices[Index], OriginalMeshUnitsPerCentimeter) * OriginalMeshScale;
			P.X *= -1; P.Y *= -1; Disc += P;
		}
	if (!Disc.IsValid) return;
	const float Radius = FMath::Max(Disc.GetExtent().Y, Disc.GetExtent().Z);
	WindTurbineHub = Disc.GetCenter();
	FSimCopterRuntimeMeshSection Section;
	Section.Material = VertexColorMaterial;
	const FLinearColor Paint(FColor(218,224,226));
	auto Face = [&](TArray<FVector> Points, const FVector& Outside)
	{
		FVector Normal = FVector::CrossProduct(Points[1]-Points[0], Points[2]-Points[0]).GetSafeNormal();
		if (FVector::DotProduct(Normal, Outside) < 0) { Algo::Reverse(Points); Normal *= -1; }
		const int32 Base = Section.Vertices.Num();
		for (const auto& P : Points)
		{
			Section.Vertices.Add(P); Section.Normals.Add(Normal); Section.VertexColors.Add(Paint);
			Section.UV0.Add(FVector2D::ZeroVector); Section.Tangents.Add(FProcMeshTangent(0,1,0));
		}
		for (int32 I=1; I<Points.Num()-1; ++I) Section.Triangles.Append({Base,Base+I,Base+I+1});
	};
	for (int32 Blade=0; Blade<3; ++Blade)
	{
		const FQuat Rotation(FVector::ForwardVector, Blade*2*PI/3);
		const FVector2D Outline[] = {{-.045,.08},{-.10,.30},{-.07,.65},{-.015,1.0},{.02,.99},{.075,.60},{.10,.24},{.045,.08}};
		TArray<FVector> Front, Back;
		for (auto P : Outline)
		{
			Front.Add(Rotation.RotateVector(FVector(-.028,P.X,P.Y)*Radius));
			Back.Add(Rotation.RotateVector(FVector(.028,P.X,P.Y)*Radius));
		}
		Face(Front,-FVector::ForwardVector); Face(Back,FVector::ForwardVector);
		for (int32 I=0; I<Front.Num(); ++I)
		{
			const int32 J=(I+1)%Front.Num();
			const FVector Edge=(Front[I]+Front[J])*.5f-Rotation.RotateVector(FVector(0,0,.45f)*Radius);
			Face({Front[I],Back[I],Back[J],Front[J]},Edge);
		}
	}
	// A shallow hub covers the blade roots and joins the existing nacelle.
	for (int32 I=0; I<12; ++I)
	{
		const float A=I*2*PI/12, B=(I+1)*2*PI/12;
		const FVector P(-.06f*Radius,FMath::Sin(A)*Radius*.105f,FMath::Cos(A)*Radius*.105f);
		const FVector Q(-.06f*Radius,FMath::Sin(B)*Radius*.105f,FMath::Cos(B)*Radius*.105f);
		const FVector Back(.12f*Radius,0,0);
		Face({P,Q,FVector(-.12f*Radius,0,0)},-FVector::ForwardVector);
		Face({P,P+Back,Q+Back,Q},FVector(0,P.Y+Q.Y,P.Z+Q.Z));
		Face({P+Back,FVector(.06f*Radius,0,0),Q+Back},FVector::ForwardVector);
	}
	auto* Mesh=SimCopterRuntimeStaticMesh::Build(this,{Section},true);
	if (!Mesh) return;
	auto* Rotors=NewObject<UInstancedStaticMeshComponent>(this,MakeUniqueObjectName(this,UInstancedStaticMeshComponent::StaticClass(),TEXT("WindTurbineBlades")));
	Rotors->SetStaticMesh(Mesh); Rotors->SetupAttachment(SceneRoot);
	Rotors->SetMobility(EComponentMobility::Movable); Rotors->SetCastShadow(true);
	Rotors->SetCollisionObjectType(ECC_WorldDynamic);
	Rotors->SetCollisionResponseToAllChannels(ECR_Block);
	Rotors->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Rotors->SetCanEverAffectNavigation(false);
	Rotors->SetRemoveSwap(); Rotors->RegisterComponent();
	WindTurbineComponentIndex=BuildingInstanceComponents.Add(Rotors);
	BuildingModelMeshes.Add(Mesh); ComponentInstanceBuildings.AddDefaulted();
	for (int32 Id=0; Id<Buildings.Num(); ++Id)
	{
		auto& Building=Buildings[Id];
		if (Building.XbldId==200 && !Building.bDemolished)
			Building.Parts.Add(AddBuildingInstance(WindTurbineComponentIndex,Id,Building.PlacementOrigin+WindTurbineHub,/*bWindowHashNudge*/false));
	}
	AdvanceWindTurbines(0);
}

void ASimCity2000CityActor::AdvanceWindTurbines(float DeltaSeconds)
{
	if (!BuildingInstanceComponents.IsValidIndex(WindTurbineComponentIndex)) return;
	auto* Rotors=BuildingInstanceComponents[WindTurbineComponentIndex].Get();
	if (!Rotors || Rotors->GetInstanceCount()==0) return;
	WindTurbineSeconds=FMath::Fmod(WindTurbineSeconds+FMath::Max(0.0f,DeltaSeconds),300.0f);
	const auto& Owners=ComponentInstanceBuildings[WindTurbineComponentIndex];
	for (int32 I=0; I<Owners.Num(); ++I)
	{
		const int32 Id=Owners[I];
		// Twelve RPM; staggered phases avoid a field of mechanically locked rotors.
		const FQuat Spin(FVector::ForwardVector,WindTurbineSeconds*(2*PI/5)+Id*2.399963f);
		Rotors->UpdateInstanceTransform(I,FTransform(Spin,Buildings[Id].PlacementOrigin+WindTurbineHub),false,I==Owners.Num()-1,true);
	}
}

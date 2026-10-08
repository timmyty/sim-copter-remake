#if WITH_DEV_AUTOMATION_TESTS
#include "Ground/SimCopterPopulationFigure.h"
#include "ProceduralMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCowGeometryTest,
 "SimCopter.Figures.CowGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCowGeometryTest::RunTest(const FString& Parameters)
{
 FString Error;
 const auto Shared = FSimCopterPopulationFigure::GetShared(FPaths::Combine(FPaths::ProjectDir(),TEXT("../Reference/SimCopterOriginalGame")),Error);
 if (!TestTrue(TEXT("Reference figures load"),Shared.IsValid())) { AddError(Error); return false; }
 const int32 Index = Shared->Model.FindFigureIndex(TEXT("Coww"));
 if (!TestTrue(TEXT("Cow exists"),Index != INDEX_NONE)) return false;
 const auto& Figure = Shared->Model.Figures[Index];
 auto* Mesh = NewObject<UProceduralMeshComponent>();
 FSimCopterPopulationFigure::FBuildParams Params;
 Params.HeightCm = 65;
 const FString OutputDir = FPaths::Combine(FPaths::ProjectDir(),TEXT("../Docs/scratchpad/cow-mesh"));
 IFileManager::Get().MakeDirectory(*OutputDir,true);
 for (const TCHAR* Mnemonic : {TEXT("DgSt"),TEXT("DgRn")})
 {
  const auto* Clip = Shared->Model.FindClip(Figure,Mnemonic);
  if (!TestNotNull(TEXT("Cow clip"),Clip)) return false;
  bool HasHead = true;
  TestTrue(TEXT("Build cow"),FSimCopterPopulationFigure::BuildClipSections(Mesh,Figure,*Clip,Shared->Palette,Params,{},HasHead));
  TestFalse(TEXT("Cow does not use a human head texture"),HasHead);
  for (int32 Frame = 0; Frame < Clip->FrameCount; ++Frame)
  {
   const auto* Section = Mesh->GetProcMeshSection(Frame*2);
   if (!TestNotNull(TEXT("Body section"),Section)) return false;
   FBox Bounds(ForceInit);
   bool Valid = true, Winding = true;
   FString Ply = FString::Printf(TEXT("ply\nformat ascii 1.0\nelement vertex %d\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nelement face %d\nproperty list uchar int vertex_indices\nend_header\n"),Section->ProcVertexBuffer.Num(),Section->ProcIndexBuffer.Num()/3);
   for (const auto& V : Section->ProcVertexBuffer)
   {
    Bounds += V.Position;
    Valid &= !V.Position.ContainsNaN() && !V.Normal.ContainsNaN() && FMath::IsNearlyEqual(V.Normal.Size(),1.0,0.01);
    Ply += FString::Printf(TEXT("%f %f %f %d %d %d\n"),V.Position.X,V.Position.Y,V.Position.Z,V.Color.R,V.Color.G,V.Color.B);
   }
   for (int32 I = 0; I < Section->ProcIndexBuffer.Num(); I+=3)
   {
    const uint32 A=Section->ProcIndexBuffer[I], B=Section->ProcIndexBuffer[I+1], C=Section->ProcIndexBuffer[I+2];
    Valid &= A<uint32(Section->ProcVertexBuffer.Num()) && B<uint32(Section->ProcVertexBuffer.Num()) && C<uint32(Section->ProcVertexBuffer.Num());
    if (!Valid) break;
    const auto& VA=Section->ProcVertexBuffer[A]; const auto& VB=Section->ProcVertexBuffer[B]; const auto& VC=Section->ProcVertexBuffer[C];
    const FVector Cross=FVector::CrossProduct(VB.Position-VA.Position,VC.Position-VA.Position);
    Winding &= Cross.SizeSquared()>1.e-10 && FVector::DotProduct(Cross,VA.Normal+VB.Normal+VC.Normal)<0;
    Ply += FString::Printf(TEXT("3 %u %u %u\n"),A,B,C);
   }
   TestTrue(TEXT("Finite mesh and valid indices"),Valid);
   TestTrue(TEXT("Outward clockwise winding and no degenerate triangles"),Winding);
   TestTrue(TEXT("Hooves meet ground without sinking"),FMath::Abs(Bounds.Min.Z)<0.01);
   TestTrue(TEXT("Cow fits population height"),Bounds.Max.Z<65.01 && Bounds.Max.Z>60);
   TestTrue(TEXT("Long body silhouette"),Bounds.GetSize().X>80 && Bounds.GetSize().Y<40);
   TestTrue(TEXT("Preview exported"),FFileHelper::SaveStringToFile(Ply,*FPaths::Combine(OutputDir,FString::Printf(TEXT("%s-%d.ply"),Mnemonic,Frame))));
  }
  FSimCopterPopulationFigure::ShowFrame(Mesh,Clip->FrameCount,Clip->FrameCount-1,HasHead);
  TestTrue(TEXT("Last frame visible"),Mesh->GetProcMeshSection((Clip->FrameCount-1)*2)->bSectionVisible);
  TestFalse(TEXT("First frame hidden"),Mesh->GetProcMeshSection(0)->bSectionVisible);
 }
 return true;
}
#endif

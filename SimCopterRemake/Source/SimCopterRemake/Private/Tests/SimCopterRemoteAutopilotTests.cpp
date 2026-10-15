#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Components/BoxComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "UI/SSimCopterToolFlaps.h"
#include "UI/SimCopterHangarArt.h"
#include "Widgets/Input/SButton.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRemoteAutopilotTest,"SimCopter.RemoteRecovery.AutopilotSwitch",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterRemoteAutopilotTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Floor=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(10000,10000,20));
	Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-20));
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>(); Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RemoteTaser, true); auto* Ops=Heli->GetAirOperations();
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC); PC->Possess(Heli);
	Heli->SetActorLocation(FVector(0,0,1800)); Heli->SeedFlightModelFromActor(); Heli->bEngineRunning=true; Heli->bIsLanded=false;
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,true);
	TestTrue(TEXT("New aircraft defaults to autopilot enabled"),Ops->IsAutopilotEnabled());
	auto Controls=SNew(SSimCopterToolFlaps).Pawn(Heli);
	TSharedPtr<SButton> Switch;
	TFunction<void(TSharedRef<SWidget>)> FindSwitch=[&](TSharedRef<SWidget> Widget)
	{
		if (Widget->GetTag()==TEXT("RemoteAutopilotSwitch")) Switch=StaticCastSharedRef<SButton>(Widget);
		auto* Children=Widget->GetChildren(); for (int32 I=0;I<Children->Num();++I) FindSwitch(Children->GetChildAt(I));
	};
	FindSwitch(Controls->BuildRemoteTaserFlap());
	if (!TestTrue(TEXT("Panel contains right-side autopilot switch"),Switch.IsValid())) return false;
	TestFalse(TEXT("Switch does not steal flight keyboard focus"),Switch->SupportsKeyboardFocus());
	Switch->SetClickMethod(EButtonClickMethod::MouseDown);
	const FPointerEvent Click(0,FVector2D::ZeroVector,FVector2D::ZeroVector,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
	Switch->OnMouseButtonDown(FGeometry(),Click);
	TestFalse(TEXT("Clicking actual switch disables autopilot"),Ops->IsAutopilotEnabled());
	auto* Car=World->SpawnActor<ASimCopterGroundAgent>();
	Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),1000); Car->MakeCriminalCar(42,0); Car->SetActorLocation(FVector(5000,0,24));
	Ops->ToggleRemoteTaser();
	const FVector Hold=Heli->GetActorLocation(); const FRotator Heading=Heli->GetActorRotation();
	Ops->AimRemoteTaser(1,.5f,.2f); const FVector Aim=Ops->GetRemoteTaserDirection();
	for (int32 I=0;I<80;++I) { Car->SetActorLocation(FVector(5000,I*10,24)); Ops->Update(.05f); }
	TestTrue(TEXT("OFF holds position with a moving distant criminal"),Heli->GetActorLocation().Equals(Hold,.01));
	TestTrue(TEXT("OFF retains free aim and steady heading"),Aim.Equals(Ops->GetRemoteTaserDirection(),.001) && Heli->GetActorRotation().Equals(Heading,.01));
	TestTrue(TEXT("OFF keeps taser camera and weapon available"),Ops->IsRemoteTaserActive() && Ops->RemoteTaserCamera);
	Ops->FireRemoteTaser(); TestTrue(TEXT("OFF still fires"),Ops->ShotCooldown>0);
	Ops->SetAutopilotEnabled(true);
	for (int32 I=0;I<80;++I) Ops->Update(.05f);
	TestTrue(TEXT("ON resumes pursuit"),FVector::Distance(Hold,Heli->GetActorLocation())>100);
	Ops->SetAutopilotEnabled(false); const FRotator ExitHeading=Heli->GetActorRotation(); Ops->ToggleRemoteTaser();
	TestTrue(TEXT("OFF returning to manual flight does not auto-turn"),Heli->GetActorRotation().Equals(ExitHeading,.01));
	Car->Destroy();
	TArray<uint8> Saved; TestTrue(TEXT("Aircraft saves OFF"),Heli->CaptureRuntimeSaveState(Saved));
	Ops->SetAutopilotEnabled(true); TestTrue(TEXT("Aircraft restores OFF"),Heli->RestoreRuntimeSaveState(Saved));
	TestFalse(TEXT("Save load retains disabled autopilot"),Ops->IsAutopilotEnabled());
	TArray<uint8> Legacy=Saved; Legacy.SetNum(Legacy.Num()-1); const int32 Version=3;
	FMemory::Memcpy(Legacy.GetData()+sizeof(uint32),&Version,sizeof(Version));
	TestTrue(TEXT("Version 3 aircraft still loads"),Heli->RestoreRuntimeSaveState(Legacy));
	TestTrue(TEXT("Legacy saves default ON"),Ops->IsAutopilotEnabled());
	Ops->SetAutopilotEnabled(false); Heli->bIsLanded=false; Heli->bEngineRunning=true;
	Heli->ExitHelicopter(); auto* Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Pilot exits with autopilot OFF"),Pilot)) return false;
	TestFalse(TEXT("OFF prevents exit-triggered recovery"),Ops->IsRecoveringPilot());
	for (int32 I=0;I<100;++I) Ops->Update(.05f);
	Ops->BeginPilotRecovery(Pilot);
	TestFalse(TEXT("Neither update reconnect nor explicit recovery bypasses OFF"),Ops->IsRecoveringPilot());
	TestTrue(TEXT("Pilot stays on foot without automatic attachment"),PC->GetPawn()==Pilot && !Pilot->IsAutoRescueAttached());
	Ops->SetAutopilotEnabled(true); Ops->Update(.05f);
	TestTrue(TEXT("ON reconnects unattended recovery"),Ops->IsRecoveringPilot());
	Ops->SetAutopilotEnabled(false);
	TestFalse(TEXT("OFF cancels an in-progress recovery"),Ops->IsRecoveringPilot());
	PC->Possess(Heli); Pilot->Destroy();
	TestFalse(TEXT("Reboarding retains OFF"),Ops->IsAutopilotEnabled());
	if (FParse::Param(FCommandLine::Get(),TEXT("SimRemoteAutopilotPreview")))
	{
		auto* Art=NewObject<USimCopterHangarArt>(Heli); Art->SetOriginalGameRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame")));
		for (float HudScale:{1.f,1.5f}) for (bool Enabled:{false,true})
		{
			Ops->SetAutopilotEnabled(Enabled); Ops->bRemoteTaserActive=Enabled;
			auto Panel=SNew(SSimCopterToolFlaps).Pawn(Heli).Art(Art).Scale(1.768292f*HudScale).HudScale(HudScale);
			FWidgetRenderer Renderer(true); auto* Target=Renderer.DrawWidget(Panel->BuildRemoteTaserFlap(),FVector2D(244*HudScale,103*HudScale));
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false); Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
			TestTrue(TEXT("Panel preview saved"),FImageUtils::SaveImageByExtension(*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/remote-taser-autopilot")/FString::Printf(TEXT("panel-%d-%.1f.png"),Enabled,HudScale)),FImageView(Pixels.GetData(),Target->SizeX,Target->SizeY)));
		}
		Ops->bRemoteTaserActive=false;
	}
	return true;
}
#endif

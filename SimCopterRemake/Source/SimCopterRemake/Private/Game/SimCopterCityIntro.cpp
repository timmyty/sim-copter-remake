#include "Game/SimCopterCityIntro.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Audio/SimCopterRadio.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "City/SimCity2000CityActor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Misc/App.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "SceneView.h"
#include "SceneViewExtension.h"

// Keep shader-driven water/clouds moving without consuming mission time during
// the showcase. This changes rendering time only, in this world only.
class FSimCopterTourViewExtension final : public FSceneViewExtensionBase
{
public:
	FSimCopterTourViewExtension(const FAutoRegister& Register, UWorld* InWorld)
		: FSceneViewExtensionBase(Register), World(InWorld), StartTime(InWorld->GetTimeSeconds()) {}
	float Elapsed = 0, Delta = 0;
	bool bEnabled = true;
	virtual void SetupViewFamily(FSceneViewFamily& Family) override
	{
		if (!bEnabled || !World.IsValid() || !Family.Scene || Family.Scene->GetWorld() != World.Get()) return;
		Family.Time = FGameTime::CreateDilated(Family.Time.GetRealTimeSeconds(),
			Family.Time.GetDeltaRealTimeSeconds(), StartTime + Elapsed, Delta);
		Family.bWorldIsPaused = Delta <= 0;
	}
private:
	TWeakObjectPtr<UWorld> World;
	double StartTime;
};

namespace
{
class SSimCopterCityTour final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterCityTour) {}
		SLATE_EVENT(FSimpleDelegate, OnSkip)
	SLATE_END_ARGS()
	FString CityName;
	float Fade = 1;
	void Construct(const FArguments& Args) { OnSkip = Args._OnSkip; ForceVolatile(true); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640, 480); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& E) override
	{ if (!E.IsRepeat()) OnSkip.ExecuteIfBound(); return FReply::Handled(); }
	virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
	virtual FReply OnKeyChar(const FGeometry&, const FCharacterEvent&) override { return FReply::Handled(); }
	virtual FReply OnAnalogValueChanged(const FGeometry&, const FAnalogInputEvent&) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override
	{ OnSkip.ExecuteIfBound(); return FReply::Handled(); }
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
		FSlateWindowElementList& E, int32 L, const FWidgetStyle&, bool) const override
	{
		const FVector2D Size = G.GetLocalSize();
		const float Scale = FMath::Min(Size.X / 1280.0, Size.Y / 720.0);
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		for (float Y : {0.0f, float(Size.Y - 48 * Scale)})
			FSlateDrawElement::MakeBox(E, L, G.ToPaintGeometry(FVector2D(Size.X,48*Scale),
				FSlateLayoutTransform(FVector2D(0,Y))), White, ESlateDrawEffect::None, FLinearColor::Black);
		auto Text = [&](const FString& Value, float Y, int32 FontSize)
		{
			FSlateDrawElement::MakeText(E,L+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(Scale,FVector2D(36*Scale,Y))),
				Value,FCoreStyle::GetDefaultFontStyle("Regular",FontSize),ESlateDrawEffect::None,FLinearColor::White);
		};
		Text(CityName, 10*Scale, 23);
		Text(TEXT("Press any key or controller button to skip"), Size.Y-32*Scale, 14);
		FSlateDrawElement::MakeBox(E,L+2,G.ToPaintGeometry(),White,ESlateDrawEffect::None,FLinearColor(0,0,0,Fade));
		return L+2;
	}
private:
	FSimpleDelegate OnSkip;
};

void SetTourHud(UWorld* World, bool bHide)
{
	for (TActorIterator<ASimCopterMissionSystemActor> It(World); It; ++It) It->SetHudHiddenForReplay(bHide);
	for (TActorIterator<ASimCopterHelicopterPawn> It(World); It; ++It) It->SetHudHiddenForReplay(bHide);
}
}

USimCopterCityIntro::USimCopterCityIntro()
{
	PrimaryComponentTick.bCanEverTick=true;
	PrimaryComponentTick.bStartWithTickEnabled=false;
	PrimaryComponentTick.bTickEvenWhenPaused=true;
}

bool USimCopterCityIntro::Start()
{
	UWorld* World=GetWorld();
	if (bPlaying || !World || !World->GetGameViewport() || !FApp::CanEverRender() || !FSlateApplication::IsInitialized()) return false;
	auto* PC=Cast<ASimCopterPlayerController>(World->GetFirstPlayerController());
	if (!PC || !PC->GetPawn()) return false;
	ASimCopterTrafficSystemActor* Traffic=nullptr;
	for (TActorIterator<ASimCopterTrafficSystemActor> It(World); It; ++It) { Traffic=*It; break; }
	if (!Traffic) return false;
	Route=BuildRoute(*Traffic);
	if (!Route.Bounds.IsValid) return false;
	Camera=World->SpawnActor<ACameraActor>();
	if (!Camera) return false;
	Camera->GetCameraComponent()->SetFieldOfView(FSimCopterCityIntroRoute::FieldOfView);
	Camera->GetCameraComponent()->bConstrainAspectRatio=false;
	Camera->GetCameraComponent()->AspectRatioAxisConstraint=EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
	Controller=PC; Elapsed=0; bSkipRequested=false; bPlaying=true;
	bPreviousCursor=PC->bShowMouseCursor;
	bPreviousFullCameraUpdate=PC->bShouldPerformFullTickWhenPaused;
	PC->bShouldPerformFullTickWhenPaused=true;
	PC->FlushPressedKeys();
	PC->PushPause();
	PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
	PresentationClock=FSceneViewExtensions::NewExtension<FSimCopterTourViewExtension>(World);
	for (TActorIterator<ASimCopterAmbientVehiclesActor> It(World); It; ++It)
	{ Ambient=*It; It->BeginCityTour(Route.Bounds,&Route); break; }
	SetTourHud(World,true);
	auto Widget=SNew(SSimCopterCityTour).OnSkip(FSimpleDelegate::CreateUObject(this,&USimCopterCityIntro::RequestSkip));
	Widget->CityName=Traffic->GetCityActor()->GetCityName();
	if (Widget->CityName.IsEmpty()) Widget->CityName=TEXT("City tour");
	Overlay=Widget;
	World->GetGameViewport()->AddViewportWidgetContent(Widget,500);
	FInputModeUIOnly Input;
	Input.SetWidgetToFocus(Widget); Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Input); PC->bShowMouseCursor=false;
	PC->SetViewTarget(Camera);
	UpdateCamera();
	StartMusic();
	LastTickSeconds=FPlatformTime::Seconds();
	SetComponentTickEnabled(true);
	return true;
}

void USimCopterCityIntro::StartMusic()
{
	StopMusic();
	if (auto* Radio = USimCopterRadioSubsystem::Get(this))
	{
		Radio->SetCityIntroSuspended(true);
		bRadioSuspended = true;
	}
	if (auto* Audio = USimCopterAudioSubsystem::Get(this))
		Music = Audio->CreateCityIntroMusic(Route.CityIndex, this);
	if (Music)
	{
		UpdateMusic(FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive());
		Music->Play();
	}
}

void USimCopterCityIntro::UpdateMusic(bool bActive)
{
	if (!Music) return;
	Music->SetPaused(!bActive);
	if (auto* Audio = USimCopterAudioSubsystem::Get(this))
		Music->SetVolumeMultiplier(Audio->VolumeIndexToGain(Audio->GetMasterVolume()));
}

void USimCopterCityIntro::StopMusic()
{
	if (Music) { Music->Stop(); Music->DestroyComponent(); Music = nullptr; }
	if (bRadioSuspended)
	{
		if (auto* Radio = USimCopterRadioSubsystem::Get(this)) Radio->SetCityIntroSuspended(false);
		bRadioSuspended = false;
	}
}

void USimCopterCityIntro::UpdateCamera()
{
	FVector2D Size(1280,720);
	if (GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->GetViewportSize(Size);
	const float Aspect = Size.Y>0 ? float(Size.X/Size.Y) : 16.0f/9;
	if (!FMath::IsNearlyEqual(ViewportAspect,Aspect)) { Route.Fit(Aspect); ViewportAspect=Aspect; }
	Camera->SetActorTransform(Route.Evaluate(Elapsed/FSimCopterCityIntroRoute::Duration));
	if (Ambient.IsValid()) Ambient->UpdateCityTour(Elapsed);
	if (Controller.IsValid() && Controller->PlayerCameraManager)
		Controller->PlayerCameraManager->UpdateCamera(0);
	if (Overlay.IsValid())
	{
		auto Widget=StaticCastSharedPtr<SSimCopterCityTour>(Overlay);
		// Keep the original final composition visible through the last source frame.
		Widget->Fade=1-FMath::Clamp(Elapsed/0.3f,0.0f,1.0f);
	}
}

void USimCopterCityIntro::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime,TickType,TickFunction);
	if (!bPlaying) return;
	const double Now=FPlatformTime::Seconds();
	// Pause the tour as well when the player switches applications.
	const bool bActive = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive();
	const float Step = bActive ? FMath::Max(float(Now-LastTickSeconds),0.0f) : 0;
	UpdateMusic(bActive);
	Elapsed+=Step;
	for (TActorIterator<ASimCity2000CityActor> It(GetWorld()); It; ++It) It->AdvanceWindTurbines(Step);
	if (PresentationClock) { PresentationClock->Elapsed=Elapsed; PresentationClock->Delta=Step; }
	LastTickSeconds=Now;
	if (bSkipRequested || Elapsed>=FSimCopterCityIntroRoute::Duration || !Controller.IsValid() || !IsValid(Camera)) Finish();
	else UpdateCamera();
}

void USimCopterCityIntro::Finish()
{
	StopMusic();
	if (!bPlaying) return;
	bPlaying=false;
	SetComponentTickEnabled(false);
	UWorld* World=GetWorld();
	if (World && World->GetGameViewport() && Overlay.IsValid()) World->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	Overlay.Reset();
	if (PresentationClock) PresentationClock->bEnabled=false;
	PresentationClock.Reset();
	if (Ambient.IsValid()) Ambient->EndCityTour();
	Ambient.Reset();
	if (auto* PC=Controller.Get())
	{
		// Restore the possessed pawn, including a helicopter restored from a saved game.
		if (PC->GetPawn()) PC->SetViewTarget(PC->GetPawn());
		if (PC->PlayerCameraManager) { PC->PlayerCameraManager->SetGameCameraCutThisFrame(); PC->PlayerCameraManager->UpdateCamera(0); }
		PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
		PC->bShouldPerformFullTickWhenPaused=bPreviousFullCameraUpdate;
		PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=bPreviousCursor;
		PC->FlushPressedKeys();
		PC->PopPause();
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
	if (World) SetTourHud(World,false);
	if (IsValid(Camera)) Camera->Destroy();
	Camera=nullptr; Controller.Reset(); ViewportAspect=0;
}

void USimCopterCityIntro::EndPlay(const EEndPlayReason::Type Reason)
{
	Finish();
	Super::EndPlay(Reason);
}

#include "Game/SimCopterCityIntro.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Audio/SimCopterRadio.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterDayNight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/SimCopterPlayerController.h"
#include "Game/SimCopterSettings.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Misc/App.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"

namespace
{
// CelestialVault's m_SimpleVolumetricCloud_TOD exposes "Cloud_GlobalCoverage", which its day
// sequence animates through a material track. Tours hold 1 PM, often a clear sky.
const FName TourCloudCoverageParameter(TEXT("Cloud_GlobalCoverage"));
float GTourCloudCoverage = 0.5f;

FAutoConsoleVariableRef CVarTourCloudCoverage(
	TEXT("SimCopter.Tour.CloudCoverage"),
	GTourCloudCoverage,
	TEXT("Minimum Cloud_GlobalCoverage during city tours (light cloud cover). <= 0 disables the override."),
	ECVF_Default);

}

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
	BeginTourLighting();
	bLoggedTourClouds = false;
	BeginPreviewCloudBanks();
	BeginTourClouds();

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

void USimCopterCityIntro::BeginTourLighting()
{
	if (bLightingOverride) return;
	auto* Settings=USimCopterSettings::Get(this);
	if (!Settings) return;
	PreviousTimeMode=static_cast<uint8>(Settings->GetTimeOfDayMode());
	PreviousStaticHour=Settings->GetStaticTimeOfDayHours();
	PreviousLiveHour=-1;
	auto* Day=USimCopterDayNightSubsystem::Get(this);
	float LiveHour=0;
	if (Day && Day->TryGetLiveTimeOfDayHours(LiveHour)) PreviousLiveHour=LiveHour;
	// Presentation only: do not save these temporary settings to the player's profile.
	bLightingOverride=true;
	Settings->SetTimeOfDayMode(ESimCopterTimeOfDayMode::Static);
	Settings->SetStaticTimeOfDayHours(13);
	if (Day) Day->ApplyTimeOfDaySettings();
}

void USimCopterCityIntro::RestoreLighting()
{
	if (!bLightingOverride) return;
	bLightingOverride=false;
	if (auto* Settings=USimCopterSettings::Get(this))
	{
		Settings->SetTimeOfDayMode(static_cast<ESimCopterTimeOfDayMode>(PreviousTimeMode));
		Settings->SetStaticTimeOfDayHours(PreviousStaticHour);
		if (auto* Day=USimCopterDayNightSubsystem::Get(this))
		{
			Day->ApplyTimeOfDaySettings();
			if (PreviousLiveHour>=0) Day->RestoreSavedTimeOfDay(PreviousLiveHour);
		}
	}
}

void USimCopterCityIntro::BeginPreviewCloudBanks()
{
	if (!PreviewClouds.IsEmpty() || !GetOwner() || Route.CloudBanks.IsEmpty()) return;
	// The component belongs to GameMode (AInfo), which is hidden in game. Give the
	// presentation a visible, transient owner rather than inheriting that hidden flag.
	PreviewCloudActor = GetWorld()->SpawnActor<AActor>();
	if (!PreviewCloudActor) return;
	PreviewCloudActor->SetFlags(RF_Transient);
	PreviewCloudActor->SetActorEnableCollision(false);
	PreviewCloudActor->SetActorTickEnabled(false);
	if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LocalFogVolume")))
	{
		PreviousLocalCloudCVar = CVar->GetInt();
		CVar->Set(1, ECVF_SetByGameOverride);
	}
	// Overlapping soft volumes provide real depth/parallax on the low fly-through.
	// They do not need the much higher, planet-scale CelestialVault cloud layer.
	FRandomStream Shape(1647 + Route.CityIndex);
	for (const FVector4& Bank : Route.CloudBanks)
	{
		for (int32 Lobe=0; Lobe<11; ++Lobe)
		{
			const float Angle = Lobe * UE_TWO_PI / 6;
			const float Radius = Bank.W * (Lobe == 0 ? .75f : Lobe < 7 ? Shape.FRandRange(.45f,.65f) : Shape.FRandRange(.22f,.38f));
			const FVector Offset = Lobe == 0 ? FVector::ZeroVector :
				FVector(FMath::Cos(Angle)*Bank.W*.85f,FMath::Sin(Angle)*Bank.W*.6f,Shape.FRandRange(-.2f,.25f)*Bank.W);
			auto* Cloud = NewObject<ULocalFogVolumeComponent>(PreviewCloudActor);
			Cloud->SetWorldLocation(FVector(Bank)+Offset);
			Cloud->SetWorldScale3D(FVector(Radius/ULocalFogVolumeComponent::GetBaseVolumeSize()));
			Cloud->RadialFogExtinction = .08f * 2000.0f / Radius;
			// UE combines radial and height coverage multiplicatively: zero height
			// density makes even a dense radial cloud completely transparent.
			Cloud->HeightFogExtinction = .8f;
			Cloud->HeightFogFalloff = 1.0f;
			Cloud->FogPhaseG = .1f;
			Cloud->FogAlbedo = FLinearColor(.98f,.985f,1);
			const float Light = Shape.FRandRange(.7f,1.0f);
			Cloud->FogEmissive = FLinearColor(15000,15500,16000) * Light;
			Cloud->RegisterComponent();
			PreviewClouds.Add(Cloud); PreviewCloudOrigins.Add(Cloud->GetComponentLocation());
		}
	}
}

void USimCopterCityIntro::UpdatePreviewCloudBanks()
{
	// Presentation time pauses with the tour; never consume gameplay weather/RNG.
	for (int32 I=0; I<PreviewClouds.Num(); ++I)
		if (IsValid(PreviewClouds[I]))
			PreviewClouds[I]->SetWorldLocation(PreviewCloudOrigins[I]+FVector(Elapsed*Route.TileSize*.12f,Elapsed*Route.TileSize*.04f,0));
}

void USimCopterCityIntro::BeginTourClouds()
{
	if (bCloudOverride || GTourCloudCoverage <= 0.0f || Route.CityIndex == 29) return;
	UWorld* World = GetWorld();
	if (!World) return;
	UVolumetricCloudComponent* Cloud = nullptr;

	for (TObjectIterator<UVolumetricCloudComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && !It->IsBeingDestroyed()) { Cloud = *It; break; }
	}
	if (!Cloud || !Cloud->GetMaterial()) return;
	bCloudOverride = true;
	bLoggedTourClouds = true;
	TourCloud = Cloud;
	PreviousCloudMaterial = Cloud->GetMaterial();
	PreviousCloudCoverage = -1;
	PreviousCloudMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TourCloudCoverageParameter), PreviousCloudCoverage);
	UE_LOG(LogTemp, Display, TEXT("SimCopter tour clouds: coverage %.2f -> %.2f on %s."),
		PreviousCloudCoverage, FMath::Max(PreviousCloudCoverage, GTourCloudCoverage), *PreviousCloudMaterial->GetName());
	// The day sequence animates a dynamic instance; reuse it, or make one for the tour only.
	TourCloudMaterial = Cast<UMaterialInstanceDynamic>(PreviousCloudMaterial);
	bCreatedTourCloudMaterial = TourCloudMaterial == nullptr;
	if (!TourCloudMaterial)
	{
		TourCloudMaterial = UMaterialInstanceDynamic::Create(PreviousCloudMaterial, Cloud);
		Cloud->SetMaterial(TourCloudMaterial);
	}
	// Low Power Graphics switches the cloud layer off entirely; a flyover can afford it.
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricCloud")))
	{
		PreviousVolumetricCloudCVar = CVar->GetInt();
		if (PreviousVolumetricCloudCVar == 0) CVar->Set(1, ECVF_SetByGameOverride); // Low Power's own priority
	}
	ApplyTourClouds();
}

void USimCopterCityIntro::ApplyTourClouds()
{
	// The cloud layer or the city can still be arriving when the tour starts on a fresh launch,
	// so keep trying until the override takes (BeginTourClouds returns at once once it has).
	if (!bCloudOverride) BeginTourClouds();
	if (!bCloudOverride || !TourCloudMaterial) return;
	// Low Power Graphics may be applied after the tour began (first launch loads settings late),
	// switching the layer back off underneath it. Hold it on for the flyover.
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricCloud")))
	{
		if (CVar->GetInt() == 0)
		{
			PreviousVolumetricCloudCVar = 0;
			CVar->Set(1, ECVF_SetByGameOverride);
		}
	}
	// The day sequence's material track evaluates the 1 PM tour time on a later frame and can put
	// its own animated instance on the layer, leaving ours writing to a material nobody draws.
	// Follow whatever the layer actually uses.
	if (UVolumetricCloudComponent* Cloud = TourCloud.Get(); Cloud && Cloud->GetMaterial() &&
		Cloud->GetMaterial() != TourCloudMaterial)
	{
		if (auto* Live = Cast<UMaterialInstanceDynamic>(Cloud->GetMaterial()))
		{
			TourCloudMaterial = Live;
			bCreatedTourCloudMaterial = false;
		}
		else
		{
			PreviousCloudMaterial = Cloud->GetMaterial();
			TourCloudMaterial = UMaterialInstanceDynamic::Create(PreviousCloudMaterial, Cloud);
			bCreatedTourCloudMaterial = true;
			Cloud->SetMaterial(TourCloudMaterial);
		}
	}
	const float Coverage = FMath::Max(PreviousCloudCoverage, GTourCloudCoverage);
	float Current = -1;
	if (!TourCloudMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TourCloudCoverageParameter), Current) ||
		!FMath::IsNearlyEqual(Current, Coverage))
	{
		TourCloudMaterial->SetScalarParameterValue(TourCloudCoverageParameter, Coverage);
	}
}

void USimCopterCityIntro::RestoreTourClouds()
{
	for (auto Cloud : PreviewClouds) if (IsValid(Cloud)) Cloud->DestroyComponent();
	PreviewClouds.Reset(); PreviewCloudOrigins.Reset();
	if (IsValid(PreviewCloudActor)) PreviewCloudActor->Destroy();
	PreviewCloudActor = nullptr;
	if (PreviousLocalCloudCVar >= 0)
		if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LocalFogVolume")))
			CVar->Set(PreviousLocalCloudCVar, ECVF_SetByGameOverride);
	PreviousLocalCloudCVar = -1;
	if (!bCloudOverride) return;
	bCloudOverride = false;
	if (UVolumetricCloudComponent* Cloud = TourCloud.Get())
	{
		if (TourCloudMaterial && bCreatedTourCloudMaterial && Cloud->GetMaterial() == TourCloudMaterial)
		{
			Cloud->SetMaterial(PreviousCloudMaterial);
		}
		else if (TourCloudMaterial && PreviousCloudCoverage >= 0)
		{
			TourCloudMaterial->SetScalarParameterValue(TourCloudCoverageParameter, PreviousCloudCoverage);
		}
	}
	if (PreviousVolumetricCloudCVar == 0)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricCloud")))
			CVar->Set(0, ECVF_SetByGameOverride);
	}
	PreviousVolumetricCloudCVar = -1;
	TourCloud.Reset();
	TourCloudMaterial = nullptr;
	PreviousCloudMaterial = nullptr;
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
	// The tour pauses city ticks. Refresh the billboard geometry AFTER publishing the
	// new camera, or old smoke/beacon cards can become large sheets across the flyover.
	for (TActorIterator<ASimCity2000CityActor> It(GetWorld()); It; ++It)
		It->RefreshCameraFacingEffects(GetWorld()->GetTimeSeconds()+Elapsed);
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
	ApplyTourClouds();
	UpdatePreviewCloudBanks();
	LastTickSeconds=Now;
	if (bSkipRequested || Elapsed>=FSimCopterCityIntroRoute::Duration || !Controller.IsValid() || !IsValid(Camera)) Finish();
	else UpdateCamera();
}

void USimCopterCityIntro::Finish()
{
	StopMusic();
	RestoreTourClouds();
	RestoreLighting();
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
		for (TActorIterator<ASimCity2000CityActor> It(World); It; ++It)
			It->RefreshCameraFacingEffects(World->GetTimeSeconds());
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

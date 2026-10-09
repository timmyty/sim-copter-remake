#include "Game/SimCopterCityIntro.h"
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
#include "Missions/SimCopterMissionSystemActor.h"
#include "Misc/App.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace
{
class SSimCopterCityTour final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterCityTour) {}
		SLATE_EVENT(FSimpleDelegate, OnSkip)
	SLATE_END_ARGS()
	FString CityName, Caption;
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
		for (float Y : {0.0f, float(Size.Y - 92 * Scale)})
			FSlateDrawElement::MakeBox(E, L, G.ToPaintGeometry(FVector2D(Size.X,92*Scale),
				FSlateLayoutTransform(FVector2D(0,Y))), White, ESlateDrawEffect::None, FLinearColor::Black);
		auto Text = [&](const FString& Value, float Y, int32 FontSize)
		{
			FSlateDrawElement::MakeText(E,L+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(Scale,FVector2D(36*Scale,Y))),
				Value,FCoreStyle::GetDefaultFontStyle("Regular",FontSize),ESlateDrawEffect::None,FLinearColor::White);
		};
		Text(CityName, 24*Scale, 26);
		Text(Caption, Size.Y-76*Scale, 20);
		Text(TEXT("Press any key or controller button to skip"), Size.Y-38*Scale, 13);
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

FTransform FSimCopterCityIntroShot::Evaluate(float Alpha) const
{
	const float T = FMath::Clamp(Alpha,0.0f,1.0f);
	const float Angle = FMath::DegreesToRadians(-35.0f + 40.0f * T);
	const FVector Position = Focus + FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,CameraZ-Focus.Z);
	return FTransform((Focus-Position).Rotation(), Position);
}

TArray<FSimCopterCityIntroShot> USimCopterCityIntro::BuildShots(const ASimCopterTrafficSystemActor& Traffic)
{
	TArray<FSimCopterCityIntroShot> Result;
	const ASimCity2000CityActor* City = Traffic.GetCityActor();
	if (!City) return Result;
	FBox Bounds(ForceInit), Landmark(ForceInit);
	FVector Shore = FVector::ZeroVector, Services = FVector::ZeroVector;
	bool bShore = false, bServices = false;
	float BestLandmarkHeight = -1;
	for (int32 Y=0;Y<128;++Y)
	for (int32 X=0;X<128;++X)
	{
		FVector Point;
		if (!Traffic.TryGetTileCenterWorldLocation(X,Y,Point)) continue;
		Bounds += Point;
		const int32 Id = Traffic.GetXbldTileId(X,Y);
		FBox Building;
		if (City->TryGetBuildingBoundsAtTile(X,Y,Building))
		{
			Bounds += Building;
			if (Building.GetSize().Z > BestLandmarkHeight)
			{ BestLandmarkHeight=Building.GetSize().Z; Landmark=Building; }
		}
		if (!bServices && (Id==0xd1 || Id==0xd2 || Id==0xd3))
		{ Services=Point; bServices=true; }
		float WaterZ; uint8 TerrainClass;
		if (!bShore && X>8 && Y>8 && X<120 && Y<120 &&
			City->TryGetWaterGameplaySurface(Point,WaterZ,TerrainClass) && TerrainClass<10)
		{
			FVector Neighbor;
			float NeighborZ; uint8 NeighborClass;
			if (Traffic.TryGetTileCenterWorldLocation(X+1,Y,Neighbor) &&
				City->TryGetWaterGameplaySurface(Neighbor,NeighborZ,NeighborClass) && NeighborClass>=10)
			{ Shore=FVector(Point.X,Point.Y,WaterZ); bShore=true; }
		}
	}
	if (!Bounds.IsValid) return Result;
	const float Tile = City->GetTileSize();
	const float Span = FMath::Max3(float(Bounds.GetSize().X),float(Bounds.GetSize().Y),Tile*16);
	auto Add = [&](FVector Focus,float Radius,const TCHAR* Caption)
	{
		FSimCopterCityIntroShot& Shot = Result.AddDefaulted_GetRef();
		Shot.Focus=Focus; Shot.Radius=Radius; Shot.Caption=Caption;
		// Stay above the highest rendered roof/terrain throughout each orbit.
		Shot.CameraZ=Bounds.Max.Z+FMath::Max(Tile*3,Radius*0.55f);
	};
	Add(Bounds.GetCenter(),Span*0.65f,TEXT("Welcome to the city"));
	if (Landmark.IsValid) Add(Landmark.GetCenter(),FMath::Max(Tile*8,float(Landmark.GetSize().Size())),TEXT("Skyline and landmarks"));
	if (bShore) Add(Shore,Tile*12,TEXT("Waterfront"));
	else if (bServices) Add(Services,Tile*10,TEXT("Emergency services"));
	else Add(Bounds.GetCenter()+FVector(0,Span*0.2f,0),Span*0.4f,TEXT("Around the city"));
	FVector Pad;
	if (Traffic.TryGetAirportPadWorldLocation(0,Pad)) Add(Pad,Tile*8,TEXT("Your home base"));
	return Result;
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
	Shots=BuildShots(*Traffic);
	if (Shots.IsEmpty()) return false;
	Camera=World->SpawnActor<ACameraActor>();
	if (!Camera) return false;
	Camera->GetCameraComponent()->SetFieldOfView(65);
	Camera->GetCameraComponent()->bConstrainAspectRatio=false;
	Controller=PC; Elapsed=0; bSkipRequested=false; bPlaying=true;
	bPreviousCursor=PC->bShowMouseCursor;
	bPreviousFullCameraUpdate=PC->bShouldPerformFullTickWhenPaused;
	PC->bShouldPerformFullTickWhenPaused=true;
	PC->FlushPressedKeys();
	PC->PushPause();
	PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
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
	LastTickSeconds=FPlatformTime::Seconds();
	UpdateCamera();
	SetComponentTickEnabled(true);
	return true;
}

void USimCopterCityIntro::UpdateCamera()
{
	const int32 Index=FMath::Clamp(FMath::FloorToInt(Elapsed/ShotSeconds),0,Shots.Num()-1);
	const float Time=FMath::Fmod(Elapsed,ShotSeconds);
	Camera->SetActorTransform(Shots[Index].Evaluate(Time/ShotSeconds));
	if (Controller.IsValid() && Controller->PlayerCameraManager)
		Controller->PlayerCameraManager->UpdateCamera(0);
	if (Overlay.IsValid())
	{
		auto Widget=StaticCastSharedPtr<SSimCopterCityTour>(Overlay);
		Widget->Caption=Shots[Index].Caption;
		Widget->Fade=1-FMath::Clamp(FMath::Min(Time,ShotSeconds-Time)/0.35f,0.0f,1.0f);
	}
}

void USimCopterCityIntro::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime,TickType,TickFunction);
	if (!bPlaying) return;
	const double Now=FPlatformTime::Seconds();
	// Pause the tour as well when the player switches applications.
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive())
		Elapsed+=FMath::Min(float(Now-LastTickSeconds),0.1f);
	LastTickSeconds=Now;
	if (bSkipRequested || Elapsed>=Shots.Num()*ShotSeconds || !Controller.IsValid() || !IsValid(Camera)) Finish();
	else UpdateCamera();
}

void USimCopterCityIntro::Finish()
{
	if (!bPlaying) return;
	bPlaying=false;
	SetComponentTickEnabled(false);
	UWorld* World=GetWorld();
	if (World && World->GetGameViewport() && Overlay.IsValid()) World->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	Overlay.Reset();
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
	Camera=nullptr; Controller.Reset(); Shots.Reset();
}

void USimCopterCityIntro::EndPlay(const EEndPlayReason::Type Reason)
{
	Finish();
	Super::EndPlay(Reason);
}

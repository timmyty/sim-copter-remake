#include "Missions/SimCopterWitnessBriefing.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

USimCopterWitnessBriefing::USimCopterWitnessBriefing()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USimCopterWitnessBriefing::Report(ASimCopterGroundAgent* Suspect)
{
	if (Suspect != nullptr) Pending.AddUnique(Suspect);
	CaptureDelay = 0.25f; // Let the figure's first animation/mesh update reach the renderer.
}

void USimCopterWitnessBriefing::TakePhoto(ASimCopterGroundAgent& Suspect)
{
	if (!Photo)
	{
		Photo = NewObject<UTextureRenderTarget2D>(this);
		Photo->RenderTargetFormat = RTF_RGBA8;
		Photo->ClearColor = FLinearColor::Black;
		Photo->InitAutoFormat(512, 384);
		Capture = NewObject<USceneCaptureComponent2D>(GetOwner());
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->CaptureSource = SCS_FinalColorLDR;
		Capture->FOVAngle = 48;
		Capture->TextureTarget = Photo;
		Capture->RegisterComponent();
		PhotoBrush.SetResourceObject(Photo);
		PhotoBrush.ImageSize = FVector2D(256, 192);
		PhotoBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	const FVector Target = Suspect.GetActorLocation() + FVector(0, 0, 15);
	FVector Eye = Target + Suspect.GetActorForwardVector() * 320 + FVector(0, 0, 40);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WitnessPhoto), true, &Suspect);
	// Search around the person's eye level so a neighbouring building cannot hide the suspect.
	for (int32 I = 0; I < 8; ++I)
	{
		const FVector Candidate = Target + Suspect.GetActorForwardVector().RotateAngleAxis(I * 45.0f, FVector::UpVector) * 320 + FVector(0, 0, 40);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Target, Candidate, ECC_Camera, Query))
		{ Eye = Candidate; break; }
		// If every direction is enclosed, keep the camera on this side of the first wall.
		Eye = Target + (Candidate - Target).GetSafeNormal() * FMath::Max(50.0f, Hit.Distance - 25.0f);
	}
	Capture->SetWorldLocationAndRotation(Eye, (Target - Eye).Rotation());
	Capture->CaptureScene();
	bPhotoPending = true;
	CaptureDelay = 0.1f;
	bSuspectWearsShades = Suspect.GetPedestrianFigureName().Equals(TEXT("SHADES"), ESearchCase::IgnoreCase);
	int32 X = 0, Y = 0;
	Suspect.TryGetTileCoordinate(X, Y);
	Caption = FText::FromString(FString::Printf(TEXT("WITNESS PHOTO — ROBBER %d\n%s\nLast seen: grid %d, %d"),
		Suspect.MissionEventId, bSuspectWearsShades ? TEXT("On foot, wearing sunglasses.") : TEXT("Suspect seen leaving on foot."), X, Y));
	EnsurePanel();
}

void USimCopterWitnessBriefing::EnsurePanel()
{
	if (Panel.IsValid() || !GetWorld()->GetGameViewport()) return;
	Panel = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(20, 80)
		[
			SNew(SBorder).Visibility_Lambda([this]() { return DisplayRemaining > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.035f, 0.05f, 0.95f)).Padding(10)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(256).HeightOverride(192)[SNew(SImage).Image(&PhotoBrush)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					SNew(STextBlock).Text_Lambda([this]() { return Caption; }).WrapTextAt(256)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12)).ColorAndOpacity(FLinearColor::White)
				]
			]
		];
	GetWorld()->GetGameViewport()->AddViewportWidgetContent(Panel.ToSharedRef(), 15);
}

void USimCopterWitnessBriefing::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	DisplayRemaining = FMath::Max(0.0f, DisplayRemaining - DeltaTime);
	CaptureDelay = FMath::Max(0.0f, CaptureDelay - DeltaTime);
	if (bPhotoPending && CaptureDelay <= 0)
	{
		bPhotoPending = false;
		DisplayRemaining = 18.0f;
		bVoicePending = true;
	}
	if (bVoicePending)
	{
		if (auto* Audio = USimCopterAudioSubsystem::Get(this); Audio && !Audio->IsRadioVoicePlayingOrQueued())
		{
			const FString Wav = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Briefings"),
				bSuspectWearsShades ? TEXT("robber-shades.wav") : TEXT("robber-witness.wav"));
			Audio->PlayFile2D(Wav, SimCopterSound::ESoundDir::Root);
			bVoicePending = false;
			DisplayRemaining = FMath::Max(DisplayRemaining, 12.0f);
		}
	}
	if (DisplayRemaining <= 0 && !bPhotoPending && !bVoicePending && CaptureDelay <= 0 && Pending.Num() > 0)
	{
		const auto Suspect = Pending[0]; Pending.RemoveAt(0);
		if (Suspect.IsValid() && !Suspect->HasMissionResolutionReported()) TakePhoto(*Suspect);
	}
}

void USimCopterWitnessBriefing::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Panel.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
		GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Panel.ToSharedRef());
	Panel.Reset();
	if (Capture) Capture->DestroyComponent();
	Super::EndPlay(Reason);
}

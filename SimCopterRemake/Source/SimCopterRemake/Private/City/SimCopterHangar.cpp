// Copyright Epic Games, Inc. All Rights Reserved.

#include "City/SimCopterHangar.h"
#include "SimCopterHangarInterior.h"

#include "Audio/SimCopterAudioSubsystem.h"
#include "City/SimCity2000CityActor.h"
#include "City/SimCopterAirport.h"
#include "City/SimCopterEffectExposure.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterHelicopterParking.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "ProceduralMeshComponent.h"
#include "UI/SimCopterHangarArt.h"
#include "UObject/ConstructorHelpers.h"

#include "../UI/SSimCopterHangarMenu.h"

namespace
{
void RefreshInteriorFill(UProceduralMeshComponent* Interior, UProceduralMeshComponent* Shell,
	UProceduralMeshComponent* Glazing, UPointLightComponent* Light, UPointLightComponent* DeskLight, const UWorld* World)
{
	const float Daylight=USimCopterEffectExposureSubsystem::GetEffectEmissiveNitsForWorld(World);
	// A modest ambient floor keeps the reference's dark wood/paint. Inverse-exposure point
	// lighting used to bleach every object as the outdoor camera adapted.
	if (auto* Material=Cast<UMaterialInstanceDynamic>(Interior->GetMaterial(0)))
		Material->SetScalarParameterValue(TEXT("EmissiveNits"),FMath::Max(2.0f,Daylight*.85f));
	if (auto* Material=Cast<UMaterialInstanceDynamic>(Shell->GetMaterial(0)))
		Material->SetScalarParameterValue(TEXT("EmissiveNits"),FMath::Max(1.0f,Daylight*.65f));
	if (auto* Material=Cast<UMaterialInstanceDynamic>(Glazing->GetMaterial(0)))
		Material->SetScalarParameterValue(TEXT("EmissiveNits"),FMath::Max(3.0f,Daylight*1.25f));
	// Window bounce adds actual directional shading, instead of flattening the room with
	// an exposure-independent lamp or an entirely emissive material.
	Light->SetIntensity(FMath::Max(250.0f,Daylight*12.0f));
	DeskLight->SetIntensity(FMath::Max(60.0f,Daylight*3.0f));
}
constexpr const TCHAR* VertexColorMaterialPath =
	TEXT("/Game/Materials/M_SimCopterLitVertexColor.M_SimCopterLitVertexColor");
}

namespace SimCopterHangarPlacement
{
FIntPoint GetPlotOriginTile(const FIntPoint& AirportOrigin)
{
	return SimCopterAirport::GetTerminalTile(AirportOrigin);
}

FVector2D GetPlotCentreTile(const FIntPoint& AirportOrigin)
{
	// The 2x2 spans tile offsets 1 and 2, so its centre in tile-centre coordinates is +1.5.
	const FIntPoint PlotOrigin = GetPlotOriginTile(AirportOrigin);
	return FVector2D(PlotOrigin.X + 0.5f, PlotOrigin.Y + 0.5f);
}

void GetPlotTiles(const FIntPoint& AirportOrigin, TArray<FIntPoint>& OutTiles)
{
	OutTiles.Reset();
	const FIntPoint PlotOrigin = GetPlotOriginTile(AirportOrigin);
	for (int32 OffsetY = 0; OffsetY < 2; ++OffsetY)
	{
		for (int32 OffsetX = 0; OffsetX < 2; ++OffsetX)
		{
			OutTiles.Emplace(PlotOrigin.X + OffsetX, PlotOrigin.Y + OffsetY);
		}
	}
}

float GetSnappedFacingYawDegrees(const FVector& From, const FVector& Target)
{
	const FVector Delta = Target - From;
	if (Delta.SizeSquared2D() < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	return FMath::UnwindDegrees(FMath::RoundToFloat(Yaw / 90.0f) * 90.0f);
}
}

ASimCopterHangar::ASimCopterHangar()
{
	// The overlap event does the work; the tick only re-checks, so it runs a few times a second.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.15f;

	ShellMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ShellMesh"));
	SetRootComponent(ShellMesh);
	ShellMesh->bUseAsyncCooking = true;
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ShellMesh->SetCollisionObjectType(ECC_WorldStatic);
	ShellMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ShellMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ShellMesh->SetCanEverAffectNavigation(false);

	InteriorMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("InteriorMesh"));
	InteriorMesh->SetupAttachment(ShellMesh);
	InteriorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteriorMesh->SetCanEverAffectNavigation(false);
	GlazingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GlazingMesh"));
	GlazingMesh->SetupAttachment(ShellMesh);
	GlazingMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GlazingMesh->SetCollisionObjectType(ECC_WorldStatic);
	GlazingMesh->SetCollisionResponseToAllChannels(ECR_Block);
	GlazingMesh->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
	GlazingMesh->SetCastShadow(false);
	GlazingMesh->SetCanEverAffectNavigation(false);
	InteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	InteriorLight->SetupAttachment(ShellMesh);
	InteriorLight->SetMobility(EComponentMobility::Movable);
	InteriorLight->SetIntensityUnits(ELightUnits::Candelas);
	InteriorLight->SetIntensity(1200.0f);
	InteriorLight->SetInverseExposureBlend(0.0f);
	InteriorLight->SetLightColor(FLinearColor(1.0f, 0.93f, 0.82f));
	InteriorLight->SetCastShadows(true);
	InteriorLight->SetSourceRadius(18);
	WorkshopLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WorkshopLight"));
	WorkshopLight->SetupAttachment(ShellMesh);
	WorkshopLight->SetMobility(EComponentMobility::Movable);
	WorkshopLight->SetIntensityUnits(ELightUnits::Candelas);
	WorkshopLight->SetInverseExposureBlend(0.0f);
	WorkshopLight->SetLightColor(FLinearColor(.93f,.96f,1.0f));
	WorkshopLight->SetSourceRadius(12);
	WorkshopLight->SetCastShadows(true);

	EntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryTrigger"));
	EntryTrigger->SetupAttachment(ShellMesh);
	EntryTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EntryTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	EntryTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntryTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EntryTrigger->SetGenerateOverlapEvents(true);
	EntryTrigger->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterialFinder(VertexColorMaterialPath);
	if (VertexColorMaterialFinder.Succeeded())
	{
		ShellMaterial = VertexColorMaterialFinder.Object;
	}

}

void ASimCopterHangar::BeginPlay()
{
	Super::BeginPlay();

	BuildShellMesh();
	EntryTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASimCopterHangar::HandleTriggerBeginOverlap);
}

void ASimCopterHangar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bShellOpen)
	{
		CloseShell();
	}
	Super::EndPlay(EndPlayReason);
}

void ASimCopterHangar::BuildShellMesh()
{
	using namespace SimCopterHangarPlacement;
	auto* StructureMaterial=UMaterialInstanceDynamic::Create(ShellMaterial,this);
	auto* WindowMaterial=UMaterialInstanceDynamic::Create(ShellMaterial,this);
	auto* InteriorMaterial=UMaterialInstanceDynamic::Create(ShellMaterial,this);
	SimCopterHangarInterior::BuildStructure(*ShellMesh,*GlazingMesh,StructureMaterial,WindowMaterial,TileSizeCm);
	SimCopterHangarInterior::Build(*InteriorMesh,InteriorMaterial,ResolveOriginalGameRoot(),TileSizeCm);
	RefreshInteriorFill(InteriorMesh,ShellMesh,GlazingMesh,InteriorLight,WorkshopLight,GetWorld());

	// Preserve the terminal plot and approach clearance. Only the reference's roof/door height
	// and the interior's human-scale proportions change; the surrounding helipad ring stays clear.
	const float HalfWidth=WidthTiles*TileSizeCm*.5f, HalfDepth=DepthTiles*TileSizeCm*.5f;
	const float Eaves=EavesHeightTiles*TileSizeCm;
	const float Inset=FMath::Min(TileSizeCm*.2f,HalfWidth*.4f);
	EntryTrigger->SetBoxExtent(FVector(HalfDepth-Inset,HalfWidth-Inset,Eaves*.5f));
	EntryTrigger->SetRelativeLocation(FVector(-Inset*.5f,0,Eaves*.5f));
	InteriorLight->SetRelativeLocation(FVector(0,TileSizeCm*.5f,Eaves*.7f));
	InteriorLight->SetAttenuationRadius(TileSizeCm*1.65f);
	WorkshopLight->SetRelativeLocation(FVector(TileSizeCm*.78f,-TileSizeCm*.23f,TileSizeCm*.34f));
	WorkshopLight->SetAttenuationRadius(TileSizeCm*.85f);
}

bool ASimCopterHangar::PlaceAtAirport(ASimCopterTrafficSystemActor* Traffic, const FVector& FacingTarget)
{
	using namespace SimCopterHangarPlacement;

	if (Traffic == nullptr)
	{
		return false;
	}

	ASimCity2000CityActor* CityActor = Traffic->GetCityActor();
	if (CityActor != nullptr)
	{
		TileSizeCm = CityActor->GetTileSize();
	}

	// The pad table gives an exact tile -> world basis that works for the fallback airport too:
	// pad 8 is the block's (0,0) tile, pad 9 its (3,0) and pad 3 its (0,3).
	FVector PadOrigin, PadX, PadY;
	if (!Traffic->TryGetAirportPadWorldLocation(8, PadOrigin) ||
		!Traffic->TryGetAirportPadWorldLocation(9, PadX) ||
		!Traffic->TryGetAirportPadWorldLocation(3, PadY))
	{
		UE_LOG(LogTemp, Warning, TEXT("SimCopter hangar: the airport has no pads to anchor to."));
		return false;
	}

	const FVector AxisX = (PadX - PadOrigin) / 3.0f;
	const FVector AxisY = (PadY - PadOrigin) / 3.0f;
	if (AxisX.SizeSquared() < KINDA_SMALL_NUMBER || AxisY.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// Where the player starts, in the block's own tile offsets.
	const FVector TargetDelta = FacingTarget - PadOrigin;
	const FVector2D TargetTileOffset(
		FVector::DotProduct(TargetDelta, AxisX) / AxisX.SizeSquared(),
		FVector::DotProduct(TargetDelta, AxisY) / AxisY.SizeSquared());

	const FIntPoint Origin = Traffic->GetAirportOriginTile();

	// The hangar takes the terminal's plot, so the offset is fixed - the middle of the block.
	const FVector2D PlotCentre = GetPlotCentreTile(Origin);
	const FVector2D PlotOffset(PlotCentre.X - Origin.X, PlotCentre.Y - Origin.Y);

	// Height comes straight off the pad table. FUN_004829f0 flattens the whole 4x4 to the terminal
	// tile's one height-map sample, so the plot is level with every pad by construction and there
	// is nothing to trace for - tracing here would only find the terminal's own roof.
	const FVector Location = PadOrigin + AxisX * PlotOffset.X + AxisY * PlotOffset.Y;

	// Take the original's airport building off the plot first. Its base (object 0x165) is a flat
	// 2x2 slab sitting exactly on the apron, so leaving it there z-fights with the hangar floor -
	// and its walls would stand inside ours. No rubble: this is a site being built on, not a fire.
	if (CityActor != nullptr)
	{
		TArray<FIntPoint> PlotTiles;
		GetPlotTiles(Origin, PlotTiles);

		TArray<FIntPoint> Cleared;
		int32 DemolishedCount = 0;
		for (const FIntPoint& Tile : PlotTiles)
		{
			if (CityActor->DemolishBuildingAtTile(Tile.X, Tile.Y, Cleared, /*bLeaveRubble=*/false))
			{
				Traffic->ClearXbldTiles(Cleared);
				++DemolishedCount;
			}
		}

		UE_LOG(LogTemp, Display,
			TEXT("SimCopter hangar: cleared %d building(s) off the terminal plot at (%d, %d)."),
			DemolishedCount,
			GetPlotOriginTile(Origin).X,
			GetPlotOriginTile(Origin).Y);
	}

	// Turn the doorway toward the player's start, square with the city grid so the doors open onto
	// a pad row rather than across a corner of the ring.
	const float Yaw = GetSnappedFacingYawDegrees(Location, FacingTarget);
	SetActorLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// TileSizeCm may have changed since the constructor built the shell.
	BuildShellMesh();

	UE_LOG(LogTemp, Display,
		TEXT("SimCopter hangar: standing on the airport terminal plot of block (%d, %d) at %s facing %.0f deg."),
		Origin.X,
		Origin.Y,
		*Location.ToCompactString(),
		Yaw);

	return true;
}

FVector ASimCopterHangar::GetTagWorldLocation() const
{
	using namespace SimCopterHangarPlacement;
	return GetActorLocation() + FVector(0.0f, 0.0f, ApexHeightTiles * TileSizeCm + TileSizeCm * 0.25f);
}

bool ASimCopterHangar::IsAnyShellOpen(const UWorld* World)
{
	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<ASimCopterHangar> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->IsShellOpen())
		{
			return true;
		}
	}

	return false;
}

void ASimCopterHangar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshInteriorFill(InteriorMesh,ShellMesh,GlazingMesh,InteriorLight,WorkshopLight,GetWorld());

	if (bShellOpen)
	{
		return;
	}

	// The overlap event covers walking in. This covers arriving any other way - a teleport, a
	// respawn, or a frame where the trigger's overlap state was rebuilt underneath the pawn.
	const APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (Pawn != nullptr && IsInsideEntryVolume(Pawn->GetActorLocation()))
	{
		TryOpenForPawn(Pawn);
	}
}

bool ASimCopterHangar::IsInsideEntryVolume(const FVector& WorldLocation) const
{
	if (EntryTrigger == nullptr)
	{
		return false;
	}

	const FVector Local = EntryTrigger->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = EntryTrigger->GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void ASimCopterHangar::TryOpenForPawn(AActor* Candidate)
{
	if (bShellOpen || Candidate == nullptr)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (World != nullptr && World->GetTimeSeconds() < ReopenAllowedTimeSeconds)
	{
		return;
	}

	// Only the player on foot walks into the hangar; a helicopter that clips the volume, or an
	// NPC wandering past, must not open the shop.
	const ASimCopterOnFootPawn* OnFoot = Cast<ASimCopterOnFootPawn>(Candidate);
	if (OnFoot == nullptr || OnFoot->GetController() == nullptr)
	{
		return;
	}

	OpenShell(Cast<APlayerController>(OnFoot->GetController()));
}

void ASimCopterHangar::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryOpenForPawn(OtherActor);
}

FString ASimCopterHangar::ResolveOriginalGameRoot() const
{
	// Same rule as the city actor: an authored override only wins while it still points at a real
	// install, so a level saved against a developer checkout does not blank the shell in a
	// packaged build.
	if (SimCopterOriginalGame::IsOriginalGameRoot(OriginalGameRoot.Path))
	{
		return OriginalGameRoot.Path;
	}

	return SimCopterOriginalGame::ResolveRoot();
}

void ASimCopterHangar::OpenShell(APlayerController* PlayerController)
{
	if (bShellOpen || GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (Art == nullptr)
	{
		Art = NewObject<USimCopterHangarArt>(this, TEXT("HangarArt"));
	}
	Art->SetOriginalGameRoot(ResolveOriginalGameRoot());

	SimCopterHangarShop::FContext Context;
	Context.Hangar = this;
	Context.Missions = Cast<ASimCopterMissionSystemActor>(
		UGameplayStatics::GetActorOfClass(World, ASimCopterMissionSystemActor::StaticClass()));
	Context.Helicopter = SimCopterHelicopterParking::ResolveCurrentAircraft(World);

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		USimCopterCareerSubsystem* Career = GameInstance->GetSubsystem<USimCopterCareerSubsystem>();
		if (Career != nullptr)
		{
			// A city entered directly (PIE, or -game with no menu) never ran BeginSession, so the
			// books would otherwise be empty and every catalog row unpriced.
			Career->EnsurePricesLoaded(ResolveOriginalGameRoot());
			if (!Career->IsCareerOpen())
			{
				Career->BeginCareer();
			}

			// Whatever the player is actually flying is on the books by definition.
			if (const ASimCopterHelicopterPawn* Helicopter = Context.Helicopter.Get())
			{
				Career->SetHelicopterOwned(Helicopter->GetHelicopterTypeIndex(), true);
			}
		}
		Context.Career = Career;
	}

	TSharedPtr<SSimCopterHangarMenu> HangarMenu;
	ShellWidget = SAssignNew(HangarMenu, SSimCopterHangarMenu)
		.Art(Art)
		.Shop(Context)
		.OnDoneRequested(FSimpleDelegate::CreateUObject(this, &ASimCopterHangar::CloseShell));

	GEngine->GameViewport->AddViewportWidgetContent(ShellWidget.ToSharedRef(), 200);

	ShellController = PlayerController != nullptr ? PlayerController : UGameplayStatics::GetPlayerController(World, 0);
	if (APlayerController* Controller = ShellController.Get())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetWidgetToFocus(HangarMenu.IsValid() ? HangarMenu->GetInitialFocusWidget() : nullptr);
		Controller->SetInputMode(InputMode);
		Controller->bShowMouseCursor = true;
	}

	bShellOpen = true;
	UE_LOG(LogTemp, Display, TEXT("SimCopter hangar: shell opened."));
}

void ASimCopterHangar::CloseShell()
{
	if (!bShellOpen)
	{
		return;
	}

	if (GEngine != nullptr && GEngine->GameViewport != nullptr && ShellWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ShellWidget.ToSharedRef());
	}
	ShellWidget.Reset();

	// The hangar owns hangar.wav as a standalone, unlooped sound object (FUN_00449cb0).
	// Leaving before the long clip finishes must tear that screen-owned sound down immediately.
	if (USimCopterAudioSubsystem* Audio = USimCopterAudioSubsystem::Get(this))
	{
		Audio->StopStandaloneSounds();
	}

	if (APlayerController* Controller = ShellController.Get())
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->bShowMouseCursor = false;
	}
	ShellController.Reset();

	bShellOpen = false;

	PushPlayerOutOfDoorway();

	if (const UWorld* World = GetWorld())
	{
		ReopenAllowedTimeSeconds = World->GetTimeSeconds() + ReopenCooldownSeconds;
	}
}

void ASimCopterHangar::PushPlayerOutOfDoorway()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return;
	}

	// Forward is the doorway, so stepping the player along it takes them out of the building and
	// out of the trigger they were standing in. The distance is measured from the front wall,
	// which is DepthTiles/2 out from the actor's own origin.
	const FVector Forward = GetActorForwardVector();
	const FVector Current = Pawn->GetActorLocation();
	const float Push = (SimCopterHangarPlacement::DepthTiles * 0.5f + ExitPushTiles) * TileSizeCm;
	FVector Target = GetActorLocation() + Forward * Push;
	Target.Z = Current.Z;

	if (Pawn->TeleportTo(Target, Forward.Rotation(), /*bIsATest=*/false, /*bNoCheck=*/true))
	{
		return;
	}

	// Blocked (something parked in the doorway): settle for shoving them straight out from where
	// they stand, which still clears the trigger.
	Pawn->TeleportTo(Current + Forward * Push, Forward.Rotation(), /*bIsATest=*/false, /*bNoCheck=*/true);
}

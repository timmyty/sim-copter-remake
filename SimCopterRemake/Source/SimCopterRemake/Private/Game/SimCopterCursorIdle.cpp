#include "Game/SimCopterCursorIdle.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"

namespace SimCopterCursorIdle
{
class FGuard : public IInputProcessor
{
public:
	explicit FGuard(UGameInstance* InInstance) : Instance(InInstance) {}
	~FGuard() { Restore(); }
	void Restore()
	{
		if (bSuppressed && FSlateApplication::IsInitialized())
			FSlateApplication::Get().GetPlatformCursor()->Show(true);
		bSuppressed = false;
	}
	void Activity()
	{
		LastActivity = FPlatformTime::Seconds();
		Restore();
	}
	virtual void Tick(float Delta, FSlateApplication& Slate, TSharedRef<ICursor> Cursor) override
	{
		auto* GI = Instance.Get();
		auto* Viewport = GI ? GI->GetGameViewportClient() : nullptr;
		auto* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
		const auto Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
		// Never hide the editor/desktop cursor or keep it hidden after changing worlds.
		if (!PC || !Widget || !Slate.IsActive() || !Widget->GetCachedGeometry().IsUnderLocation(Slate.GetCursorPos())) { Activity(); return; }
		const bool bMenu = PC->IsPaused() || Viewport->IgnoreInput();
		if (LastController != PC || LastPawn.Get() != PC->GetPawn() || bWasMenu != bMenu || bWasVisible != PC->bShowMouseCursor)
		{
			Activity();
			LastController = PC; LastPawn = PC->GetPawn();
			bWasMenu = bMenu; bWasVisible = PC->bShowMouseCursor;
		}
		// Suppress only the native cursor. Changing bShowMouseCursor would overwrite the
		// pawn/menu's intended state and could reveal the on-foot cursor after leaving a menu.
		if (!PC->bShowMouseCursor) { Activity(); return; }
		if (!bSuppressed && HasTimedOut(FPlatformTime::Seconds(), LastActivity))
		{
			bSuppressed = true;
			Cursor->Show(false);
		}
	}
	virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		if (!Event.GetCursorDelta().IsNearlyZero()) Activity();
		return false;
	}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override { Activity(); return false; }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent&, const FPointerEvent*) override { Activity(); return false; }
private:
	TWeakObjectPtr<UGameInstance> Instance;
	TWeakObjectPtr<APlayerController> LastController;
	TWeakObjectPtr<APawn> LastPawn;
	bool bWasMenu = false;
	bool bWasVisible = false;
	bool bSuppressed = false;
	double LastActivity = FPlatformTime::Seconds();
};

TSharedPtr<IInputProcessor> Register(UGameInstance* Instance)
{
	if (!FSlateApplication::IsInitialized() || IsRunningCommandlet()) return nullptr;
	auto Guard = MakeShared<FGuard>(Instance);
	FSlateApplication::Get().RegisterInputPreProcessor(Guard);
	return Guard;
}
void Unregister(TSharedPtr<IInputProcessor>& Guard)
{
	if (Guard && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Guard);
	Guard.Reset();
}
}

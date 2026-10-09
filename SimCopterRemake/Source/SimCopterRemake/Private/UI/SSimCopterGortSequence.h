#pragma once
#include "Widgets/SCompoundWidget.h"

class USimCopterAudioSubsystem;
class UAudioComponent;

class SSimCopterGortSequence : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterGortSequence) : _Art(nullptr), _Audio(nullptr) {}
		SLATE_ARGUMENT(const FSlateBrush*, Art)
		SLATE_ARGUMENT(USimCopterAudioSubsystem*, Audio)
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual ~SSimCopterGortSequence() override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	FText GetSubtitle(int32 Speaker) const;
private:
	double ElapsedSeconds = 0;
	int32 CurrentCue = INDEX_NONE;
	bool bClosed = false;
	FSimpleDelegate OnClosed;
	TWeakObjectPtr<USimCopterAudioSubsystem> Audio;
	TWeakObjectPtr<UAudioComponent> Voice;
	void StopVoice();
	void Finish();
};

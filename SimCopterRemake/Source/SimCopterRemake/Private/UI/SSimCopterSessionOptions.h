#pragma once

#include "SSimCopterNavigableMenu.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class USimCopterSettings;

// Offered on every New/Open path, independent of previous launches or existing config.
class SSimCopterSessionOptions : public SSimCopterNavigableMenu
{
public:
	SLATE_BEGIN_ARGS(SSimCopterSessionOptions) {}
		SLATE_ARGUMENT(USimCopterSettings*, Settings)
		SLATE_EVENT(FSimpleDelegate, OnContinue)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
	FSimpleDelegate OnBack;
};

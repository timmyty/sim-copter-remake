#include "UI/SimCopterControllerHelp.h"

namespace SimCopterControllerHelp
{
EStyle DetectStyle(const FString& Identity)
{
	for (const TCHAR* Name : {TEXT("PlayStation"), TEXT("DualShock"), TEXT("DualSense"), TEXT("Sony"),
		TEXT("PS4"), TEXT("PS5"), TEXT("VID_054C"), TEXT("VID:054C")})
		if (Identity.Contains(Name, ESearchCase::IgnoreCase)) return EStyle::PlayStation;
	// XInput translation can conceal the physical brand; use the layout exposed by the driver.
	return EStyle::Xbox;
}

TArray<FPrompt> BuildPrompts(const FState& State)
{
	TArray<FPrompt> Rows;
	const auto Add = [&Rows](const TCHAR* Icon, FString Label) { Rows.Add({Icon, MoveTemp(Label)}); };
	if (State.bKeyboard)
	{
		if (State.bExpanded)
		{
			if (State.Context == EContext::OnFoot)
			{
				Add(TEXT(""), TEXT("WASD: move | Mouse: look | Shift: toggle sprint | Space: jump"));
				Add(TEXT(""), TEXT("F: board | Walk up: carry | G: put down | P: parachute"));
				Add(TEXT(""), TEXT("RMB: aim taser | LMB: fire"));
			}
			else
			{
				Add(TEXT(""), TEXT("W/S: forward/back | A/D: turn | Space/Ctrl: climb/descend"));
				Add(TEXT(""), TEXT("Shift: toggle sprint | RMB + mouse: look | Wheel: zoom | C: camera"));
				Add(TEXT(""), TEXT("F: exit | N: tools/dispatch | +/-: map zoom"));
			}
			Add(TEXT(""), TEXT("Ctrl+P: Photo Mode | F12: take photo in Photo Mode"));
		}
		Add(TEXT(""), State.bExpanded ? TEXT("F1: Hide controls") :
			FString::Printf(TEXT("Shift: Sprint %s | F1: Controls | Ctrl+P: Photo Mode"), State.bSprintEnabled ? TEXT("ON") : TEXT("OFF")));
		return Rows;
	}
	if (State.bExpanded)
	{
		switch (State.Context)
		{
		case EContext::Camera:
			Add(TEXT("ls"), TEXT("Forward / back; slide left / right"));
			Add(TEXT("rs"), TEXT("Up / down: zoom camera"));
			Add(TEXT("rt"), TEXT("Raise aircraft in camera framing"));
			Add(TEXT("lt"), TEXT("Lower aircraft in camera framing"));
			Add(TEXT("x"), TEXT("Toggle spotlight"));
			if (State.bCanExit) Add(TEXT("y"), TEXT("Exit helicopter"));
			Add(TEXT("r3"), TEXT("Release to return to tools"));
			break;
		case EContext::ToolWheel:
			Add(TEXT("rs"), TEXT("Select tool / remote taser / passengers"));
			Add(TEXT("a"), TEXT("Activate selected option"));
			Add(TEXT("x"), TEXT("Switch to Dispatch"));
			Add(TEXT("b"), TEXT("Cancel"));
			break;
		case EContext::DispatchWheel:
			Add(TEXT("rs"), TEXT("Select service / Recall All"));
			Add(TEXT("a"), TEXT("Confirm selected option"));
			Add(TEXT("x"), TEXT("Switch to Tools"));
			Add(TEXT("b"), TEXT("Cancel"));
			break;
		case EContext::Passengers:
			Add(TEXT("horizontal"), TEXT("Select passenger"));
			if (State.bHasPassengers) Add(TEXT("a"), TEXT("Choose drop action"));
			Add(TEXT("b"), TEXT("Back"));
			break;
		case EContext::ConfirmPassenger:
			Add(TEXT("horizontal"), TEXT("Drop / Cancel"));
			Add(TEXT("a"), TEXT("Confirm"));
			Add(TEXT("b"), TEXT("Back"));
			break;
		case EContext::OnFoot:
			Add(TEXT("ls"), TEXT("Move / strafe; walk up to carry"));
			Add(TEXT("rs"), TEXT("Look / aim"));
			Add(TEXT("lt"), TEXT("Hold to aim taser"));
			Add(TEXT("rt"), TEXT("Fire taser"));
			Add(TEXT("a"), TEXT("Jump"));
			Add(TEXT("b"), TEXT("Toggle parachute while falling"));
			if (State.bCanBoard) Add(TEXT("y"), TEXT("Enter helicopter"));
			if (State.bCarryingPerson) Add(TEXT("x"), TEXT("Put down passenger"));
			break;
		case EContext::RemoteTaser:
			Add(TEXT("rs"), TEXT("Aim remote taser"));
			Add(TEXT("lt"), TEXT("Hold for 4x scope zoom"));
			Add(TEXT("a"), TEXT("Fire remote taser"));
			Add(TEXT("rt"), TEXT("Fire remote taser"));
			Add(TEXT("x"), TEXT("Options / select Fly Helicopter"));
			if (State.bCanExit) Add(TEXT("y"), TEXT("Exit helicopter"));
			break;
		default:
			Add(TEXT("ls"), TEXT("Forward / back; turn left / right"));
			Add(TEXT("rs"), TEXT("Look / orbit camera"));
			Add(TEXT("rt"), TEXT("Climb / start engine"));
			Add(TEXT("lt"), TEXT("Descend / stop engine (B also)"));
			Add(TEXT("r3"), TEXT("Hold for camera adjustments"));
			if (State.bHasTool)
			{
				Add(TEXT("a"), State.bMegaphone ? FString(TEXT("Broadcast: ")) + State.Message : State.ToolAction);
				if (State.bMegaphone) Add(TEXT("horizontal"), TEXT("Change message"));
				else if (State.bRope) Add(TEXT("vertical"), TEXT("Raise / lower"));
			}
			Add(TEXT("x"), TEXT("Open Tools / switch Dispatch"));
			Add(TEXT("lb"), TEXT("LB / RB: turn left / right"));
			if (State.bCanExit) Add(TEXT("y"), TEXT("Exit helicopter"));
			break;
		}
	}
	if (State.bExpanded && (State.Context == EContext::Flight || State.Context == EContext::Camera || State.Context == EContext::RemoteTaser))
	{
		Add(TEXT("r3"), TEXT("+ D-pad up / down: minimap zoom"));
		Add(TEXT("r3"), TEXT("+ D-pad left / right: map mission"));
		Add(TEXT("l3"), TEXT("+ D-pad up: toggle mission markers"));
		Add(TEXT("l3"), TEXT("+ D-pad down: toggle service markers"));
	}
	// Always the final row: collapsing help never hides the way to bring it back.
	if (State.bSprintAvailable)
	{
		Add(TEXT("l3"), State.bSprintEnabled ? TEXT("Sprint Mode: ON (click to stop)") : TEXT("Sprint Mode: OFF (click to sprint)"));
		Add(TEXT("l3"), State.bExpanded ? TEXT("R3 + L3: Hide help") : TEXT("R3 + L3: Show help"));
	}
	else Add(TEXT("l3"), State.bExpanded ? TEXT("Hide help") : TEXT("Show help"));
	return Rows;
}
}

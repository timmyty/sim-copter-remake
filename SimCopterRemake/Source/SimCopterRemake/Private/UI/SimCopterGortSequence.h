#pragma once
#include "CoreMinimal.h"

namespace SimCopterGort
{
struct FCue
{
	double StartSeconds;
	int32 Speaker; // 0 = left, 1 = right
	int32 Voice;   // One-based alNN.wav number; not the subtitle index.
	const TCHAR* Subtitle;
};

TConstArrayView<FCue> GetCues();
int32 FindCue(double ElapsedSeconds);
// The original waits indefinitely after the final line. Give it reading time,
// then release this modal screen and its pause instead of trapping the player.
constexpr double EndSeconds = 186.0;
constexpr double FadeSeconds = .5;
}

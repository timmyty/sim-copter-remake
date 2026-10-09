#include "UI/SimCopterGortSequence.h"

namespace SimCopterGort
{
// SCHOOK: AlienEndingSetup 0x00445800, strings 964..999.
// Paired text/sound records share a start. The first twenty lines have 4-second
// delays; the original bonus conversation follows a 150-second pause.
// User-requested remake change: shorten ONLY that pause to ten seconds.
// Keep the nonsequential voices and repeated left speaker at lines 13..15.
static const FCue Cues[] = {
	{4, 0, 1, TEXT("That human contains excessive flying abilities.")},
	{8, 1, 2, TEXT("Yes, it is very well.")},
	{12, 0, 3, TEXT("Our noble mission is now in mere jeopardy.")},
	{16, 1, 4, TEXT("That carbon-based biped has much sincerity of purpose.")},
	{20, 0, 5, TEXT("Permit me to recognize the situation.")},
	{24, 1, 6, TEXT("Yes, we shall both recognize rapidly.")},
	{28, 0, 7, TEXT("Recognition has engulfed me now.")},
	{32, 1, 8, TEXT("Please illustrate me also.")},
	{36, 0, 9, TEXT("Perhaps it can be re-trained.")},
	{40, 1, 10, TEXT("That primitive pilot-creature?")},
	{44, 0, 11, TEXT("Yes, it might be persuaded to join our cause.")},
	{48, 1, 12, TEXT("Join us? It is nominal?")},
	{52, 0, 13, TEXT("Just recognize the potentialities.")},
	{56, 0, 15, TEXT("It contains the instincts of dglorath.")},
	{60, 0, 17, TEXT("It brims with the strategy of clof-gor-thatn.")},
	{64, 0, 19, TEXT("It chiefly lacks our superior technology.")},
	{68, 1, 14, TEXT("I oscillate out of phase with you.")},
	{72, 0, 15, TEXT("Modulate if you must, I see our destiny now.")},
	{76, 1, 16, TEXT("I desire that you have true recognition.")},
	{80, 0, 17, TEXT("Trust me...")},
	{90, 0, 1, TEXT("What hasn't the user clicked on this screen yet?")},
	{96, 1, 2, TEXT("Why did Paul agree to program this screen in the first place?")},
	{102, 0, 3, TEXT("I think because he finished his bug list before the release date.")},
	{108, 1, 4, TEXT("Do you ever watch American TV?")},
	{114, 0, 5, TEXT("Yes, but I don't admit it to dglorath.")},
	{120, 1, 6, TEXT("Did you ever notice how on the news they always pronounce 'harassment' funny?")},
	{126, 0, 7, TEXT("This is similar to the issue of the pronouncement of the planet Uranus.")},
	{132, 1, 8, TEXT("Yeah, but they pronounce 'abutted' they way you'd expect.")},
	{138, 0, 9, TEXT("Why does this game have so many Easter eggs?")},
	{144, 1, 10, TEXT("Well, since it isn't a violent shoot-em up, they had to put in some entertainment for the teenagers.")},
	{150, 0, 11, TEXT("What programming language is this game written in?")},
	{156, 1, 12, TEXT("60% C++, 30% C, and 10% assembler.")},
	{162, 0, 13, TEXT("It's a good thing we don't run Windows on our computers.")},
	{168, 1, 14, TEXT("I thought Windows was a virus.")},
	{174, 0, 15, TEXT("Well, it is. It's just disguised as an operating system.")},
	{180, 1, 16, TEXT("An installer under Windows is defined as a program that takes a directory of files in one nice neat place and spreads them all over your computer with no hope of ever knowing where they all went.")}
};

TConstArrayView<FCue> GetCues() { return MakeArrayView(Cues); }

int32 FindCue(double ElapsedSeconds)
{
	if (!FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds >= EndSeconds) return INDEX_NONE;
	for (int32 Index = UE_ARRAY_COUNT(Cues) - 1; Index >= 0; --Index)
		if (ElapsedSeconds >= Cues[Index].StartSeconds) return Index;
	return INDEX_NONE;
}
}

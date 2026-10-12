#include "Ground/SimCopterBaseballGame.h"

void FSimCopterBaseballScore::Apply(ESimCopterBaseballPlay Play)
{
	if (bFinal) return;
	const int32 Player = Batter();
	bool bComplete = false;
	if (Play == ESimCopterBaseballPlay::Foul) { Strikes = FMath::Min(2, Strikes + 1); return; }
	if (Play == ESimCopterBaseballPlay::Ball)
	{
		if (++Balls < 4) return;
		// Only forced runners advance on a walk, including a bases-loaded run.
		if (Bases[0] != INDEX_NONE)
		{
			if (Bases[1] != INDEX_NONE)
			{
				if (Bases[2] != INDEX_NONE) ++Runs[BattingTeam];
				Bases[2] = Bases[1];
			}
			Bases[1] = Bases[0];
		}
		Bases[0] = Player;
		bComplete = true;
	}
	else if (Play == ESimCopterBaseballPlay::Strike)
	{
		if (++Strikes < 3) return;
		++Outs; bComplete = true;
	}
	else if (Play == ESimCopterBaseballPlay::GroundOut || Play == ESimCopterBaseballPlay::FlyOut)
	{
		++Outs; bComplete = true;
	}
	else
	{
		const int32 Advance = int32(Play) - int32(ESimCopterBaseballPlay::Single) + 1;
		for (int32 I = 2; I >= 0; --I)
		{
			if (Bases[I] == INDEX_NONE) continue;
			if (I + Advance >= 3) ++Runs[BattingTeam];
			else Bases[I + Advance] = Bases[I];
			Bases[I] = INDEX_NONE;
		}
		if (Advance == 4) ++Runs[BattingTeam];
		else Bases[Advance - 1] = Player;
		bComplete = true;
	}
	if (!bComplete) return;
	// On a non-home-run walk-off, scoring stops at the winning run.
	if (BattingTeam==1 && Inning>=9 && Play!=ESimCopterBaseballPlay::HomeRun && Runs[1]>Runs[0])
		Runs[1]=Runs[0]+1;
	Balls = Strikes = 0;
	NextBatter[BattingTeam] = (NextBatter[BattingTeam] + 1) % 9;
	// Walk-offs and an unnecessary bottom of the ninth end immediately. Ties play extras.
	if (BattingTeam == 1 && Inning >= 9 && Runs[1] > Runs[0]) { bFinal = true; return; }
	if (Outs < 3) return;
	if (Inning >= 9 && ((BattingTeam == 0 && Runs[1] > Runs[0]) ||
		(BattingTeam == 1 && Runs[0] != Runs[1]))) { bFinal = true; return; }
	Outs = 0;
	for (int32& Runner : Bases) Runner = INDEX_NONE;
	if (BattingTeam == 1) ++Inning;
	BattingTeam = 1 - BattingTeam;
}

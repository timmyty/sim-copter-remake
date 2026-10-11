#include "Audio/SimCopterAudioSubsystem.h"

FSimCopterPcmClip USimCopterAudioSubsystem::BuildBoneCrack()
{
	// A short, dry compound fracture/crunch cue. Generated PCM ships with the code and
	// needs no additional original-game file or cooked asset. No loop or shared voice slot.
	FSimCopterPcmClip Clip;
	Clip.SampleRate=22050; Clip.Channels=1; Clip.Duration=.32f;
	const int32 Count=FMath::RoundToInt(Clip.SampleRate*Clip.Duration);
	Clip.Pcm16.SetNumUninitialized(Count*2);
	FRandomStream Random(7319);
	float LowPass=0;
	for (int32 I=0; I<Count; ++I)
	{
		const float T=float(I)/Clip.SampleRate;
		const float Noise=Random.FRandRange(-1.f,1.f);
		LowPass=FMath::Lerp(LowPass,Noise,.16f);
		float Crack=0;
		for (float Start : {0.f,.027f,.063f,.103f})
		{
			const float Age=T-Start;
			if (Age>=0) Crack+=(Noise-.6f*LowPass)*FMath::Exp(-Age*125.f)*(.6f-Start*2.f);
		}
		const float Body=LowPass*.7f*FMath::Exp(-T*17.f)+.16f*FMath::Sin(2*PI*115*T)*FMath::Exp(-T*35.f);
		const int16 Sample=int16(FMath::Clamp(Crack+Body,-.95f,.95f)*22000);
		FMemory::Memcpy(Clip.Pcm16.GetData()+I*2,&Sample,2);
	}
	return Clip;
}

void USimCopterAudioSubsystem::PlayPilotFallInjury(bool bSevere, const FVector& Location)
{
	// Original event 30 is the pain grunt (grnthit1/2), rather than an exertion voice.
	PlayVoiceEvent(INDEX_NONE,30,Location,0,true);
	if (bSevere) PlayIndependentVoice(BuildBoneCrack(),Location,0,true,INDEX_NONE);
}

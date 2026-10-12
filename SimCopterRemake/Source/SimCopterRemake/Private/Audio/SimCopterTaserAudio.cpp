#include "Audio/SimCopterAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

FSimCopterPcmClip USimCopterAudioSubsystem::BuildTaserSound(ESimCopterTaserSound Kind)
{
	FSimCopterPcmClip Clip;
	Clip.SampleRate=22050; Clip.Channels=1;
	const bool bLoop=Kind==ESimCopterTaserSound::Charge || Kind==ESimCopterTaserSound::Seek;
	Clip.Duration=bLoop ? 1.f : Kind==ESimCopterTaserSound::Release ? .28f : .42f;
	const int32 Count=FMath::RoundToInt(Clip.SampleRate*Clip.Duration);
	Clip.Pcm16.SetNumUninitialized(Count*2);
	FRandomStream Noise(9182+int32(Kind));
	for (int32 I=0; I<Count; ++I)
	{
		const float T=float(I)/Clip.SampleRate;
		float Signal=0;
		switch (Kind)
		{
		case ESimCopterTaserSound::Charge:
			Signal=(.32f*FMath::Sin(2*PI*110*T)+.16f*FMath::Sin(2*PI*220*T)+.08f*FMath::Sin(2*PI*660*T))*(.85f+.15f*FMath::Cos(2*PI*4*T)); break;
		case ESimCopterTaserSound::Seek:
			Signal=.3f*FMath::Sin(2*PI*460*T+3*FMath::Sin(2*PI*6*T))+.08f*FMath::Sin(2*PI*920*T); break;
		case ESimCopterTaserSound::Release:
			Signal=(.5f*FMath::Sin(2*PI*(1050*T-1400*T*T))+.18f*Noise.FRandRange(-1,1))*FMath::Exp(-T*12); break;
		case ESimCopterTaserSound::Impact:
			Signal=(.45f*Noise.FRandRange(-1,1)*FMath::Exp(-T*16)+.35f*FMath::Sin(2*PI*85*T)*FMath::Exp(-T*11))*(.7f+.3f*FMath::Cos(2*PI*38*T)); break;
		}
		const float Envelope=bLoop ? 1.f : FMath::Min(T/.004f,1.f)*FMath::Min((Clip.Duration-T)/.012f,1.f);
		const int16 Sample=int16(FMath::Clamp(Signal*Envelope,-.95f,.95f)*28000);
		FMemory::Memcpy(Clip.Pcm16.GetData()+I*2,&Sample,2);
	}
	return Clip;
}

UAudioComponent* USimCopterAudioSubsystem::PlayTaserSound(ESimCopterTaserSound Kind, USceneComponent* Emitter)
{
	if (!GetWorld() || !bSoundsAvailable || !Emitter) return nullptr;
	const bool bLoop=Kind==ESimCopterTaserSound::Charge || Kind==ESimCopterTaserSound::Seek;
	const FSimCopterPcmClip Clip=BuildTaserSound(Kind);
	auto* Component=NewObject<UAudioComponent>(this);
	Component->bAutoActivate=false; Component->bAutoDestroy=false; Component->bIsUISound=false;
	Component->bAllowSpatialization=true; Component->bOverrideAttenuation=true;
	auto& Attenuation=Component->AttenuationOverrides;
	Attenuation.bAttenuate=true; Attenuation.bSpatialize=true;
	Attenuation.DistanceAlgorithm=EAttenuationDistanceModel::Linear;
	Attenuation.AttenuationShape=EAttenuationShape::Sphere;
	Attenuation.AttenuationShapeExtents=FVector(80); Attenuation.FalloffDistance=2400;
	Component->SetSound(MakeWave(Clip,bLoop,this));
	Component->SetVolumeMultiplier(VolumeIndexToGain(MasterVolume)*(bLoop ? .42f : .75f));
	Component->RegisterComponentWithWorld(GetWorld());
	if (bLoop) Component->AttachToComponent(Emitter,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	else Component->SetWorldLocation(Emitter->GetComponentLocation());
	Component->Play();
	if (bLoop) AttachedVoiceLoopComponents.Add(Component);
	else
	{
		auto& Sound=OneShots.AddDefaulted_GetRef(); Sound.Component=Component;
		Sound.EndTime=FPlatformTime::Seconds()+Clip.Duration; Sound.VolumeIndex=8750;
	}
	return Component;
}

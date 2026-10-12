#pragma once
#include "Formats/MaxisProceduralMeshBuilder.h"

namespace SimCopterPilotEquipment
{
	void BuildTaser(FMaxisMeshSection& Out, float Deployment = 0.0f);
	void BuildTaserCircuits(FMaxisMeshSection& Out, float Deployment, float Energy, float Phase);
	void BuildTaserOrb(FMaxisMeshSection& Out, float Radius, float Phase, float Energy = 1.0f, bool bCore = true);
	void BuildParachute(FMaxisMeshSection& Out);
}

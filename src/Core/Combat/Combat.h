#pragma once

#include <vector>

#include "Core/Combat/CbtEncounter.h"
#include "Core/Combat/CbtEvent.h"
#include "Nexus/Nexus.h"

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	uint32_t GetTargetID();

	Encounter_t GetCurrentEncounter();
}
 
#pragma once

#include <vector>

#include "Core/Combat/CbtEncounter.h"
#include "Core/Combat/CbtEvent.h"
#include "Nexus/Nexus.h"

#define EV_CCCCCCCC_COMBAT     "EV_CCCCCCCC_COMBAT"
#define EV_CCCCCCCC_COMBAT_END "EV_CCCCCCCC_COMBAT_END"

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	Encounter_t GetCurrentEncounter();
}
 
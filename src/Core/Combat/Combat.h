#pragma once

#include "Nexus/Nexus.h"

#include "CbtEncounter.h"

#define EV_CMX_COMBAT     "CMX::CombatEvent"
#define EV_CMX_COMBAT_END "CMX::CombatEnd"

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	bool IsRegistered();

	bool IsActive();

	Encounter_t GetCurrentEncounter();
}
 
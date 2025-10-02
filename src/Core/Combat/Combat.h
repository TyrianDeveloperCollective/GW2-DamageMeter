#pragma once

#include <vector>

#include "Nexus/Nexus.h"
#include "CbtEncounter.h"

#define EV_CMX_COMBAT "CMX::CombatEvent"

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	bool IsRegistered();

	bool IsActive();

	Encounter_t* GetCurrentEncounter();

	std::vector<Encounter_t*> GetHistory();
}
 
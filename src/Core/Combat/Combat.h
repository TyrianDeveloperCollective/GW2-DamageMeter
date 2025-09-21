#pragma once

#include <vector>

#include "Nexus/Nexus.h"
#include "CbtEvent.h"

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	uint32_t GetSelfID();

	uint32_t GetTargetID();

	std::vector<CombatEvent_t*> GetCombatEvents();
}
 
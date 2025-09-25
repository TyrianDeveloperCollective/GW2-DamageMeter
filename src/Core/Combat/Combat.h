#pragma once

#include "Core/Combat/CbtEncounter.h"
#include "Nexus/Nexus.h"

#define EV_PCM_COMBAT     "PCM::CombatEvent"
#define EV_PCM_COMBAT_END "PCM::CombatEnd"

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	bool IsRegistered();

	bool IsActive();

	Encounter_t GetCurrentEncounter();
}
 
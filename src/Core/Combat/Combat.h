#pragma once

#include "Nexus/Nexus.h"
#include "CbtEncounter.h"

#define EV_CMX_COMBAT "CMX::CombatEvent"

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy(void* = nullptr, void* = nullptr);

	bool IsRegistered();

	bool IsActive();

	Encounter_t* GetCurrentEncounter();
}

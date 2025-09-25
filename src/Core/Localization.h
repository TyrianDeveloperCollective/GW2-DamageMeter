#pragma once

#include "Nexus/Nexus.h"

enum class ETexts
{
	Barrier,
	Cleave,
	CombatMetrics,
	Damage,
	DisabledCombatTracker,
	DisabledInPvP,
	Duration,
	Heal,
	NoTargets,
	Incoming,
	Outgoing,
	Target,
	Total
};

namespace Localization
{
	void Init(AddonAPI_t* aApi);
}

const char* Translate(ETexts aID);

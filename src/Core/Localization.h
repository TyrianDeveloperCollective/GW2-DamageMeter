#pragma once

#include "Nexus/Nexus.h"

enum class ETexts
{
	CombatMetrics,
	DisabledInPvP,
	Duration
};

namespace Localization
{
	void Init(AddonAPI_t* aApi);
}

const char* Translate(ETexts aID);

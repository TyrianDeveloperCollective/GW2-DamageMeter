#pragma once

#include <vector>

#include "Nexus/Nexus.h"

enum class ECombatEventType
{
	Health,
	Down,
	Death
};

struct CombatEvent_t
{
	ECombatEventType Type;

	uint64_t         Time;
	uint32_t         Ms;

	uint32_t         SrcAgentID;
	uint32_t         DstAgentID;
	uint32_t         SkillID;

	float            Value;
	float            ValueAlt;

	uint32_t         IsConditionDamage : 1;
	uint32_t         IsCritical : 1;
	uint32_t         IsFumble : 1;
};

namespace Combat
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	uint32_t GetSelfID();

	std::vector<CombatEvent_t*> GetCombatEvents();
}

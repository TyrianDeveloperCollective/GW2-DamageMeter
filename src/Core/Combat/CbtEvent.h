#pragma once

#include <cstdint>

#include "CbtAgent.h"

enum class ECombatEventType
{
	Health,
	Down,
	Death
};

struct Skill_t
{
	uint32_t ID;
	char     Name[128];
};

struct CombatEvent_t
{
	ECombatEventType Type;

	uint64_t         Time;
	uint32_t         Ms;

	Agent_t*         SrcAgent;
	Agent_t*         DstAgent;
	Skill_t*         Skill;

	float            Value;
	float            ValueAlt;

	uint32_t         IsConditionDamage : 1;
	uint32_t         IsCritical        : 1;
	uint32_t         IsFumble          : 1;
};

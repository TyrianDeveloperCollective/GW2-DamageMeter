#pragma once

#include <cstdint>

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
	uint32_t         IsCritical        : 1;
	uint32_t         IsFumble          : 1;
};

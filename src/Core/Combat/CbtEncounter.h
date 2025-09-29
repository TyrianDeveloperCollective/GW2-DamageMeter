#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "CbtAgent.h"
#include "CbtEvent.h"

struct Encounter_t
{
	uint64_t                               TimeStart    = 0;
	uint64_t                               TimeEnd      = 0;

	uint32_t                               SelfID       = 0;

	std::unordered_map<uint32_t, Agent_t*> Agents;
	std::unordered_map<uint32_t, Skill_t*> Skills;
	std::vector<CombatEvent_t*>            CombatEvents;
};

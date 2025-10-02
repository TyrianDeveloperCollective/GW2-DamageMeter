#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "CbtAgent.h"
#include "CbtEvent.h"

struct Stats_t
{
	float Damage  = 0.f;
	float Heal    = 0.f;
	float Barrier = 0.f;
};

struct Encounter_t
{
	uint64_t                               TimeStart = 0;
	uint64_t                               TimeEnd   = 0;

	uint32_t                               TriggerID = 0;

	Agent_t*                               Self      = 0;
	Stats_t                                OutTarget = {};
	Stats_t                                OutCleave = {};
	Stats_t                                InTarget  = {};
	Stats_t                                InCleave  = {};

	std::unordered_map<uint32_t, Agent_t*> Agents;
	std::unordered_map<uint32_t, Skill_t*> Skills;
	std::vector<CombatEvent_t*>            CombatEvents;
};

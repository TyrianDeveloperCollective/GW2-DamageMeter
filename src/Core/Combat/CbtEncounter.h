#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "CbtEvent.h"

struct Encounter_t
{
	uint64_t                                  TimeStart;
	uint64_t                                  TimeEnd;

	uint32_t                                  SelfID;

	std::unordered_map<uint32_t, std::string> AgentNameLUT;
	std::vector<CombatEvent_t*>               CombatEvents;
};

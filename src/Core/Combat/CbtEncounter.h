#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "CbtAgent.h"
#include "CbtEvent.h"
#include "Util/src/Strings.h"

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

	inline std::string GetName()
	{
		std::string targetName;
		if (this->TriggerID)
		{
			targetName = this->Agents.at(TriggerID)->GetName();
		}
		else
		{
			for (CombatEvent_t* ev : this->CombatEvents)
			{
				if (ev->SrcAgent == this->Self && ev->DstAgent && ev->DstAgent != this->Self)
				{
					targetName = ev->DstAgent->GetName();
					break;
				}
			}
		}

		time_t time = this->TimeStart / 1000; // needs to be in seconds
		tm tm{};
		localtime_s(&tm, &time);

		return String::Format("%02d:%02d:%02d, %s (%s)", tm.tm_hour, tm.tm_min, tm.tm_sec, this->Duration().c_str(), targetName.c_str());
	}

	inline std::string Duration()
	{
		uint64_t cbtDurationMs = max(this->TimeEnd - this->TimeStart, 1000);
		float cbtDuration = cbtDurationMs / 1000.f;

		std::string durationStr;

		if (cbtDurationMs > 60000)
		{
			durationStr = String::Format("%um%.2fs", cbtDurationMs / 1000 / 60, fmod(cbtDuration, 60.f));
		}
		else
		{
			durationStr = String::Format("%.2fs", cbtDuration);
		}

		return durationStr;
	}
};

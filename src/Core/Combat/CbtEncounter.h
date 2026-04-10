#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <RealTime-API/RTAPI.hpp>

#include "Util/src/Strings.h"

struct Stats_t
{
	float Damage  = 0.f;
	float Heal    = 0.f;
	float Barrier = 0.f;
};

struct Encounter_t
{
	uint64_t                                TimeStart   = 0;
	uint64_t                                TimeEnd     = 0;
	decltype(RTAPI::Agent::Name) TriggerName = "";
	uint32_t                                SelfID      = 0;
	Stats_t                                 OutTarget   = {};
	Stats_t                                 OutCleave   = {};
	Stats_t                                 InTarget    = {};
	Stats_t                                 InCleave    = {};

	inline bool HasStarted() const
	{
		return TimeStart != 0;
	}

	inline std::string GetName()
	{
		std::string targetName = TriggerName;
		if (targetName.empty())
		{
			targetName = "SelfID=" + std::to_string(this->SelfID); 
		}

		time_t time = this->TimeStart / 1000; // needs to be in seconds
		tm tm{};
		localtime_s(&tm, &time);

		return String::Format("%02d:%02d:%02d, %s (%s)", tm.tm_hour, tm.tm_min, tm.tm_sec, this->Duration().c_str(), targetName.c_str());
	}

	inline std::string Duration() const
	{
		uint64_t cbtDurationMs = max(this->TimeEnd - this->TimeStart, 1000);
		float cbtDuration = cbtDurationMs / 1000.f;

		if (cbtDurationMs > 60000)
		{
			return String::Format("%um%.2fs", cbtDurationMs / 1000 / 60, fmod(cbtDuration, 60.f));
		}
		return String::Format("%.2fs", cbtDuration);
	}
};

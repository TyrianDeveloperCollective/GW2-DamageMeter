#pragma once

#include <string>

#include "GW2RE/Game/Agent/Agent.h"
#include "GW2RE/Game/Combat/SkillDef.h"
#include "Nexus/Nexus.h"

namespace TextCache
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	void Advance();

	///----------------------------------------------------------------------------------------------------
	/// GetAgentName:
	/// 	Retrieves the agent name from the cache.
	///----------------------------------------------------------------------------------------------------
	std::string GetAgentName(uint32_t aID);
	
	///----------------------------------------------------------------------------------------------------
	/// GetAgentName:
	/// 	Retrieves the agent name or queues it for resolving.
	///----------------------------------------------------------------------------------------------------
	std::string GetAgentName(GW2RE::CAgent aAgent);

	///----------------------------------------------------------------------------------------------------
	/// GetSkillName:
	/// 	Retrieves the skill name from the cache.
	///----------------------------------------------------------------------------------------------------
	std::string GetSkillName(uint32_t aID);

	///----------------------------------------------------------------------------------------------------
	/// GetSkillName:
	/// 	Retrieves the skill name or queues it for resolving.
	///----------------------------------------------------------------------------------------------------
	std::string GetSkillName(GW2RE::SkillDef_t* aSkill);
}

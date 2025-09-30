#pragma once

#include <cstdint>
#include <string>

enum class EAgentType
{
	Character,
	Gadget,
	AttackTarget
};

struct Agent_t
{
	uint32_t    ID;
	uint32_t    SpeciesID;
	EAgentType  Type;
	char        Name[128];

	bool        IsMinion;
	uint32_t    OwnerID;

	inline std::string GetName()
	{
		if (this->Name[0])
		{
			return this->Name;
		}

		switch (this->Type)
		{
			default:
				return "ag-" + std::to_string(this->ID);
			case EAgentType::Character:
				return "ch-" + std::to_string(this->ID);
			case EAgentType::Gadget:
				return "gd-" + std::to_string(this->ID);
			case EAgentType::AttackTarget:
				return "at-" + std::to_string(this->ID);
		}
	}
};

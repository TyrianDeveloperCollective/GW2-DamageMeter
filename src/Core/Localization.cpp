#include "Localization.h"

#include <string>

#define LANG_ID(aID) ("METER_" + std::to_string((uint32_t)aID)).c_str()

namespace Localization
{
	static AddonAPI_t* s_APIDefs = nullptr;
}

void Localization::Init(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;

	s_APIDefs->Localization_Set(LANG_ID(ETexts::CombatMetrics), "en", "Combat Metrics");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::CombatMetrics), "de", "Kampfstatistiken");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::DisabledInPvP), "en", "Disabled in PvP.");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::DisabledInPvP), "de", "Im PvP deaktiviert.");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Duration), "en", "Duration");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Duration), "de", "Dauer");
}

const char* Translate(ETexts aID)
{
	return Localization::s_APIDefs->Localization_Translate(LANG_ID(aID));
}

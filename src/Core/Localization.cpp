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

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Barrier), "en", "Barrier");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Barrier), "de", "Schild");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Cleave), "en", "Cleave");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Cleave), "de", "Spalten");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::CombatMetrics), "en", "Combat Metrics");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::CombatMetrics), "de", "Kampfstatistiken");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Damage), "en", "Damage");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Damage), "de", "Schaden");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::DisabledCombatTracker), "en", "Combat Tracker not registered.");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::DisabledCombatTracker), "de", "Kampfprotokoll nicht registriert.");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::DisabledInPvP), "en", "Disabled in PvP.");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::DisabledInPvP), "de", "Im PvP deaktiviert.");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Duration), "en", "Duration");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Duration), "de", "Dauer");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Heal), "en", "Healing");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Heal), "de", "Heilung");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::NoTargets), "en", "No targets.");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::NoTargets), "de", "Keine Gegner.");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Incoming), "en", "Incoming");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Incoming), "de", "Erhalten");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Outgoing), "en", "Outgoing");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Outgoing), "de", "Verteilt");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Target), "en", "Target");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Target), "de", "Ziel");

	s_APIDefs->Localization_Set(LANG_ID(ETexts::Total), "en", "Total");
	s_APIDefs->Localization_Set(LANG_ID(ETexts::Total), "de", "Gesamt");
}

const char* Translate(ETexts aID)
{
	return Localization::s_APIDefs->Localization_Translate(LANG_ID(aID));
}

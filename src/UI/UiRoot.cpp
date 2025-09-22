#include "UiRoot.h"

#include <mutex>

#include "imgui/imgui.h"

#include "Core/Combat/Combat.h"
#include "Core/Localization.h"
#include "Core/TextCache/TextCache.h"
#include "GW2RE/Game/Map/MapDef.h"
#include "GW2RE/Game/MissionContext.h"
#include "GW2RE/Game/PropContext.h"
#include "Util/src/Time.h"

namespace UiRoot
{
	struct DisplayEncounter_t
	{
		Encounter_t           Encounter    = {};
		std::vector<uint32_t> Skills;
		std::vector<uint32_t> Agents;

		float                 TotalDmg     = 0.f;
		float                 TotalHeal    = 0.f;
		float                 TotalBarrier = 0.f;
	};

	static AddonAPI_t*           s_APIDefs = nullptr;
	
	static std::mutex            s_Mutex;
	static DisplayEncounter_t    s_DisplayedEncounter;

	void OnCombatEvent();
}

void UiRoot::Create(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;

	ImGui::SetCurrentContext((ImGuiContext*)s_APIDefs->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))s_APIDefs->ImguiMalloc, (void(*)(void*, void*))s_APIDefs->ImguiFree);

	Localization::Init(s_APIDefs);

	s_APIDefs->GUI_Register(RT_Render, UiRoot::Render);
	s_APIDefs->GUI_Register(RT_OptionsRender, UiRoot::Options);

	s_APIDefs->Events_Subscribe(EV_CCCCCCCC_COMBAT, (EVENT_CONSUME)OnCombatEvent);
	s_APIDefs->Events_Subscribe(EV_CCCCCCCC_COMBAT_END, (EVENT_CONSUME)OnCombatEvent);
}

void UiRoot::Destroy()
{
	if (!s_APIDefs) { return; }

	s_APIDefs->Events_Unsubscribe(EV_CCCCCCCC_COMBAT, (EVENT_CONSUME)OnCombatEvent);
	s_APIDefs->Events_Unsubscribe(EV_CCCCCCCC_COMBAT_END, (EVENT_CONSUME)OnCombatEvent);

	s_APIDefs->GUI_Deregister(UiRoot::Render);
	s_APIDefs->GUI_Deregister(UiRoot::Options);
}

void UiRoot::Render()
{
	GW2RE::CPropContext propctx = GW2RE::CPropContext::Get();
	GW2RE::MissionContext_t* missionctx = propctx.GetMissionCtx();

	std::string wndName = Translate(ETexts::CombatMetrics);
	wndName.append("###RCGG_Meter");

	if (missionctx && missionctx->CurrentMap && missionctx->CurrentMap->PvP)
	{
		ImGui::Begin(wndName.c_str());
		ImGui::TextColored(ImVec4(0.675f, 0.349f, 0.349f, 1.0f), Translate(ETexts::DisabledInPvP));
		ImGui::End();
		return;
	}

	if (!Combat::IsRegistered())
	{
		ImGui::Begin(wndName.c_str());
		ImGui::TextColored(ImVec4(0.675f, 0.349f, 0.349f, 1.0f), Translate(ETexts::DisabledCombatTracker));
		ImGui::End();
		return;
	}

	if (ImGui::Begin(wndName.c_str()))
	{
		uint64_t cbtDurationMs = s_DisplayedEncounter.Encounter.TimeEnd - s_DisplayedEncounter.Encounter.TimeStart;

		float ms = (cbtDurationMs % 1000) / 1000.f;
		uint64_t s = (cbtDurationMs / 1000) % 60;
		uint64_t m = (cbtDurationMs / 1000) / 60;

		if (m > 0)
		{
			ImGui::Text("%s: %llum%.2fs", Translate(ETexts::Duration), m, s + ms);
		}
		else
		{
			ImGui::Text("%s: %.2fs", Translate(ETexts::Duration), s + ms);
		}

		ImGui::TextDisabled("D/s: %.0f", abs(s_DisplayedEncounter.TotalDmg) / (cbtDurationMs / 1000.f));
		ImGui::TextDisabled("Dmg: %.0f", abs(s_DisplayedEncounter.TotalDmg));
		ImGui::TextDisabled("H/s: %.0f", s_DisplayedEncounter.TotalHeal / (cbtDurationMs / 1000.f));
		ImGui::TextDisabled("Heal: %.0f", s_DisplayedEncounter.TotalHeal);
		ImGui::TextDisabled("B/s: %.0f", s_DisplayedEncounter.TotalBarrier / (cbtDurationMs / 1000.f));
		ImGui::TextDisabled("Barr: %.0f", s_DisplayedEncounter.TotalBarrier);

		ImGui::Separator();

		ImGui::TextDisabled("Agents");
		for (uint32_t id : s_DisplayedEncounter.Agents)
		{
			if (s_DisplayedEncounter.Encounter.SelfID == id)
			{
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0, 1.0f), TextCache::GetAgentName(id).c_str());
			}
			else
			{
				ImGui::Text(TextCache::GetAgentName(id).c_str());
			}
		}

		ImGui::Separator();

		ImGui::TextDisabled("Skills");
		for (uint32_t id : s_DisplayedEncounter.Skills)
		{
			ImGui::Text(TextCache::GetSkillName(id).c_str());
		}
	}
	ImGui::End();
}

void UiRoot::Options()
{
	
}

void UiRoot::OnCombatEvent()
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	Encounter_t currentEncounter = Combat::GetCurrentEncounter();

	if (currentEncounter.TimeStart != s_DisplayedEncounter.Encounter.TimeStart)
	{
		/* TODO: Store old display encounter. */
		s_DisplayedEncounter = {};
	}

	s_DisplayedEncounter.Encounter = currentEncounter;
	s_DisplayedEncounter.TotalDmg = 0;
	s_DisplayedEncounter.TotalHeal = 0;
	s_DisplayedEncounter.TotalBarrier = 0;

	for (CombatEvent_t* ev : s_DisplayedEncounter.Encounter.CombatEvents)
	{
		if (std::find(s_DisplayedEncounter.Agents.begin(), s_DisplayedEncounter.Agents.end(), ev->SrcAgentID) == s_DisplayedEncounter.Agents.end())
		{
			s_DisplayedEncounter.Agents.push_back(ev->SrcAgentID);
		}

		if (ev->SrcAgentID == s_DisplayedEncounter.Encounter.SelfID)
		{
			if (ev->Value < 0)
			{
				s_DisplayedEncounter.TotalDmg += ev->Value;
			}
			else if (ev->Value > 0)
			{
				s_DisplayedEncounter.TotalHeal += ev->Value;
			}
			else if (ev->ValueAlt > 0)
			{
				s_DisplayedEncounter.TotalBarrier += ev->ValueAlt;
			}
		}

		if (std::find(s_DisplayedEncounter.Agents.begin(), s_DisplayedEncounter.Agents.end(), ev->DstAgentID) == s_DisplayedEncounter.Agents.end())
		{
			s_DisplayedEncounter.Agents.push_back(ev->DstAgentID);
		}

		if (std::find(s_DisplayedEncounter.Skills.begin(), s_DisplayedEncounter.Skills.end(), ev->SkillID) == s_DisplayedEncounter.Skills.end())
		{
			s_DisplayedEncounter.Skills.push_back(ev->SkillID);
		}
	}
}

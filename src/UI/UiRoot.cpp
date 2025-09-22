#include "UiRoot.h"

#include <algorithm>
#include <mutex>

#include "imgui/imgui.h"

#include "Core/Combat/Combat.h"
#include "Core/Localization.h"
#include "Core/TextCache/TextCache.h"
#include "GW2RE/Game/Map/MapDef.h"
#include "GW2RE/Game/MissionContext.h"
#include "GW2RE/Game/PropContext.h"
#include "Util/src/Strings.h"

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

	static bool                  s_Incoming = false;
	static uint32_t              s_Target   = 0;

	void OnCombatEvent();
	void CalculateTotals();
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

void TooltipGeneric(const char* aFmt, ...)
{
	if (strlen(aFmt) == 0) { return; }

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		va_list args;
		va_start(args, aFmt);
		ImGui::TextV(aFmt, args);
		va_end(args);
		ImGui::EndTooltip();
	}
}

void UiRoot::Render()
{
	GW2RE::CPropContext propctx = GW2RE::CPropContext::Get();
	GW2RE::MissionContext_t* missionctx = propctx.GetMissionCtx();

	std::string wndName = Translate(ETexts::CombatMetrics);
	wndName.append("###RCGG_Meter");

	ImGuiWindowFlags wndFlags = ImGuiWindowFlags_AlwaysAutoResize;

	static float s_WndHeight = 150.f;
	float wndHeight = .0f;

	ImGui::SetNextWindowSize(ImVec2(.0f, s_WndHeight));

	if (missionctx && missionctx->CurrentMap && missionctx->CurrentMap->PvP)
	{
		ImGui::Begin(wndName.c_str(), 0, wndFlags);
		ImGui::TextColored(ImVec4(0.675f, 0.349f, 0.349f, 1.0f), Translate(ETexts::DisabledInPvP));
		ImGui::End();
		return;
	}

	if (!Combat::IsRegistered())
	{
		ImGui::Begin(wndName.c_str(), 0, wndFlags);
		ImGui::TextColored(ImVec4(0.675f, 0.349f, 0.349f, 1.0f), Translate(ETexts::DisabledCombatTracker));
		ImGui::End();
		return;
	}

	if (ImGui::Begin(wndName.c_str(), 0, wndFlags))
	{
		const std::lock_guard<std::mutex> lock(s_Mutex);
		if (ImGui::SmallButton(s_Incoming ? "incoming" : "outgoing"))
		{
			s_Incoming = !s_Incoming;
			CalculateTotals();
		}
		uint64_t cbtDurationMs = s_DisplayedEncounter.Encounter.TimeEnd - s_DisplayedEncounter.Encounter.TimeStart;

		/*float ms = (cbtDurationMs % 1000) / 1000.f;
		uint64_t s = (cbtDurationMs / 1000) % 60;
		uint64_t m = (cbtDurationMs / 1000) / 60;

		if (m > 0)
		{
			ImGui::Text("%s: %llum%.2fs", Translate(ETexts::Duration), m, s + ms);
		}
		else
		{
			ImGui::Text("%s: %.2fs", Translate(ETexts::Duration), s + ms);
		}*/

		ImGui::Separator();

		static float s_TextWidth = 50.f;
		float textWidth = .0f;
		float agentSelectorWidth = s_TextWidth + ((ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().WindowPadding.x) * 2);
		if (ImGui::BeginChild("Agents", ImVec2(agentSelectorWidth, .0f), false))
		{
			if (s_DisplayedEncounter.Agents.size() == 0)
			{
				ImGui::TextDisabled("No targets.");
				textWidth = max(textWidth, ImGui::CalcTextSize("No targets.").x);
			}
			else
			{
				if (ImGui::Selectable(s_Incoming ? "Self" : "Cleave", s_Target == 0))
				{
					s_Target = 0;
					CalculateTotals();
				}
				textWidth = max(textWidth, ImGui::CalcTextSize("Self").x);
			}

			for (uint32_t id : s_DisplayedEncounter.Agents)
			{
				if (id == s_DisplayedEncounter.Encounter.SelfID)
				{
					continue;
				}
				else if (id == 0)
				{
					continue;
				}
				else
				{
					std::string btnId = TextCache::GetAgentName(id);

					textWidth = max(textWidth, ImGui::CalcTextSize(btnId.c_str()).x);

					btnId.append("###");
					btnId.append(std::to_string(id));

					if (ImGui::Selectable(btnId.c_str(), s_Target == id))
					{
						if (s_Target != id)
						{
							s_Target = id;
						}
						else
						{
							s_Target = 0;
						}
						CalculateTotals();
					}
				}
			}

			s_TextWidth = textWidth;
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginGroup();
		{
			if (ImGui::BeginTable("Data", 3))
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Damage");

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s/s", String::FormatNumberDenominated(abs(s_DisplayedEncounter.TotalDmg) / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
				TooltipGeneric("%.0f", abs(s_DisplayedEncounter.TotalDmg));

				ImGui::TableSetColumnIndex(2);
				ImGui::Text(String::FormatNumberDenominated(abs(s_DisplayedEncounter.TotalDmg)).c_str());
				TooltipGeneric("%.0f", abs(s_DisplayedEncounter.TotalDmg));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Heal");

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s/s", String::FormatNumberDenominated(s_DisplayedEncounter.TotalHeal / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
				TooltipGeneric("%.0f", abs(s_DisplayedEncounter.TotalHeal));

				ImGui::TableSetColumnIndex(2);
				ImGui::Text(String::FormatNumberDenominated(s_DisplayedEncounter.TotalHeal).c_str());
				TooltipGeneric("%.0f", abs(s_DisplayedEncounter.TotalHeal));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Barrier");

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s/s", String::FormatNumberDenominated(s_DisplayedEncounter.TotalBarrier / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
				TooltipGeneric("%.0f", abs(s_DisplayedEncounter.TotalBarrier));

				ImGui::TableSetColumnIndex(2);
				ImGui::Text(String::FormatNumberDenominated(s_DisplayedEncounter.TotalBarrier).c_str());
				TooltipGeneric("%.0f", abs(s_DisplayedEncounter.TotalBarrier));
			}
			ImGui::EndTable();
		}
		ImGui::EndGroup();
		wndHeight = max(wndHeight, ImGui::GetCursorPosY());
		s_WndHeight = ceilf(wndHeight) + 1.f;

		//ImGui::Separator();

		//ImGui::TextDisabled("Skills");
		//for (uint32_t id : s_DisplayedEncounter.Skills)
		//{
		//	ImGui::Text(TextCache::GetSkillName(id).c_str());
		//}
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

	for (CombatEvent_t* ev : s_DisplayedEncounter.Encounter.CombatEvents)
	{
		if (std::find(s_DisplayedEncounter.Agents.begin(), s_DisplayedEncounter.Agents.end(), ev->SrcAgentID) == s_DisplayedEncounter.Agents.end())
		{
			s_DisplayedEncounter.Agents.push_back(ev->SrcAgentID);
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

	UiRoot::CalculateTotals();
}

void UiRoot::CalculateTotals()
{
	s_DisplayedEncounter.TotalDmg = 0;
	s_DisplayedEncounter.TotalHeal = 0;
	s_DisplayedEncounter.TotalBarrier = 0;

	for (CombatEvent_t* ev : s_DisplayedEncounter.Encounter.CombatEvents)
	{
		if (s_Incoming)
		{
			if (s_Target == 0) /* self */
			{
				/* if not self, skip */
				if (ev->DstAgentID != s_DisplayedEncounter.Encounter.SelfID) { continue; }
			}
			else /* specific agent */
			{
				if (ev->DstAgentID != s_Target) { continue; }
			}
		}
		else /* outgoing */
		{
			if (s_Target == 0) /* self */
			{
				/* if not self, skip */
				if (ev->SrcAgentID != s_DisplayedEncounter.Encounter.SelfID) { continue; }
			}
			else /* specific agent */
			{
				if (ev->SrcAgentID != s_Target) { continue; }
			}
		}

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
}

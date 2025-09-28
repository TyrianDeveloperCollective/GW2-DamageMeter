#include "UiRoot.h"

#include <algorithm>
#include <mutex>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "ImPos/imgui_positioning.h"

#include "Core/Combat/Combat.h"
#include "Core/Localization.h"
#include "Core/TextCache/TextCache.h"
#include "GW2RE/Game/Map/MapDef.h"
#include "GW2RE/Game/MissionContext.h"
#include "GW2RE/Game/PropContext.h"
#include "Targets.h"
#include "Util/src/Strings.h"

namespace UiRoot
{
	struct Stats_t
	{
		float Damage  = 0.f;
		float Heal    = 0.f;
		float Barrier = 0.f;
	};

	struct DisplayEncounter_t
	{
		Encounter_t           Encounter = {};
		std::vector<uint32_t> Skills;
		std::vector<uint32_t> Agents;

		Stats_t               Target    = {};
		Stats_t               Cleave    = {};
	};

	static AddonAPI_t*        s_APIDefs = nullptr;
	
	static std::mutex         s_Mutex;
	static DisplayEncounter_t s_DisplayedEncounter;

	static bool               s_Incoming = false;

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

	s_APIDefs->Events_Subscribe(EV_CMX_COMBAT, (EVENT_CONSUME)OnCombatEvent);
	s_APIDefs->Events_Subscribe(EV_CMX_COMBAT_END, (EVENT_CONSUME)OnCombatEvent);
}

void UiRoot::Destroy()
{
	if (!s_APIDefs) { return; }

	s_APIDefs->Events_Unsubscribe(EV_CMX_COMBAT, (EVENT_CONSUME)OnCombatEvent);
	s_APIDefs->Events_Unsubscribe(EV_CMX_COMBAT_END, (EVENT_CONSUME)OnCombatEvent);

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
	wndName.append("###CMX::Metrics");

	static ImGuiExt::Positioning_t s_Position{};

	ImGuiWindowFlags wndFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiExt::UpdatePosition("###CMX::Metrics");

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
		uint64_t cbtDurationMs = s_DisplayedEncounter.Encounter.TimeEnd - s_DisplayedEncounter.Encounter.TimeStart;
		float cbtDuration = max(cbtDurationMs, 1000) / 1000.f;

		std::string durationStr;

		if (cbtDurationMs > 60000)
		{
			durationStr = String::Format("%um%.2fs", cbtDurationMs / 1000 / 60, fmod(cbtDuration, 60.f));
		}
		else
		{
			durationStr = String::Format("%.2fs", cbtDuration);
		}

		if (ImGui::BeginTable("Data", 3))
		{
			ImGui::TableSetupColumn("##NULL", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn((std::string(Translate(ETexts::Target)) + "##Target").c_str(), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn((std::string(Translate(ETexts::Cleave)) + "##Cleave").c_str(), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();
			/* TODO: Configurable headers. */

			/* Damage row. */
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled(Translate(ETexts::Damage));

			/* DPS Target */
			ImGui::TableNextColumn();
			std::string dpsTarget = s_DisplayedEncounter.Target.Damage < 0.f
				? String::FormatNumberDenominated(abs(s_DisplayedEncounter.Target.Damage) / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(dpsTarget.c_str()).x);
			ImGui::Text(dpsTarget.c_str());
			TooltipGeneric("%.0f, %s", abs(s_DisplayedEncounter.Target.Damage), durationStr.c_str());

			/* DPS Cleave */
			ImGui::TableNextColumn();
			std::string dpsCleave = s_DisplayedEncounter.Cleave.Damage < 0.f
				? String::FormatNumberDenominated(abs(s_DisplayedEncounter.Cleave.Damage) / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(dpsCleave.c_str()).x);
			ImGui::Text(dpsCleave.c_str());
			TooltipGeneric("%.0f, %s", abs(s_DisplayedEncounter.Cleave.Damage), durationStr.c_str());

			/* Heal row. */
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled(Translate(ETexts::Heal));

			/* Heal Target */
			ImGui::TableNextColumn();
			std::string hpsTarget = s_DisplayedEncounter.Target.Heal > 0.f
				? String::FormatNumberDenominated(s_DisplayedEncounter.Target.Heal / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(hpsTarget.c_str()).x);
			ImGui::Text(hpsTarget.c_str());
			TooltipGeneric("%.0f, %s", s_DisplayedEncounter.Target.Heal, durationStr.c_str());

			/* Heal Cleave */
			ImGui::TableNextColumn();
			std::string hpsCleave = s_DisplayedEncounter.Cleave.Heal > 0.f
				? String::FormatNumberDenominated(s_DisplayedEncounter.Cleave.Heal / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(hpsCleave.c_str()).x);
			ImGui::Text(hpsCleave.c_str());
			TooltipGeneric("%.0f, %s", s_DisplayedEncounter.Cleave.Heal, durationStr.c_str());

			/* Barrier row. */
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled(Translate(ETexts::Barrier));

			/* Barrier Target */
			ImGui::TableNextColumn();
			std::string bpsTarget = s_DisplayedEncounter.Target.Barrier > 0.f
				? String::FormatNumberDenominated(s_DisplayedEncounter.Target.Barrier / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(bpsTarget.c_str()).x);
			ImGui::Text(bpsTarget.c_str());
			TooltipGeneric("%.0f, %s", s_DisplayedEncounter.Target.Barrier, durationStr.c_str());

			/* Barrier Cleave*/
			ImGui::TableNextColumn();
			std::string bpsCleave = s_DisplayedEncounter.Cleave.Barrier > 0.f
				? String::FormatNumberDenominated(s_DisplayedEncounter.Cleave.Barrier / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(bpsCleave.c_str()).x);
			ImGui::Text(bpsCleave.c_str());
			TooltipGeneric("%.0f, %s", s_DisplayedEncounter.Cleave.Barrier, durationStr.c_str());
		}
		ImGui::EndTable();
	}

	if (ImGui::BeginPopupContextWindow("###CMX::Metrics::CtxMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		const std::lock_guard<std::mutex> lock(s_Mutex);
		if (ImGui::Button(s_Incoming ? Translate(ETexts::Incoming) : Translate(ETexts::Outgoing)))
		{
			s_Incoming = !s_Incoming;
			CalculateTotals();
		}

		ImGui::EndPopup();
	}

	ImGuiExt::ContextMenuPosition("###CMX::Metrics::CtxMenu");

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
	s_DisplayedEncounter.Cleave = {};
	s_DisplayedEncounter.Target = {};

	//minions need to count towards your own dps

	for (CombatEvent_t* ev : s_DisplayedEncounter.Encounter.CombatEvents)
	{
		if (s_Incoming)
		{
			if (ev->DstAgentID != s_DisplayedEncounter.Encounter.SelfID) { continue; }
		}
		else /* outgoing */
		{
			if (ev->SrcAgentID != s_DisplayedEncounter.Encounter.SelfID) { continue; }
		}

		uint32_t id = s_Incoming ? ev->SrcAgentID : ev->DstAgentID;
		uint32_t species = s_DisplayedEncounter.Encounter.AgentSpeciesLUT.at(id);

		bool isPrimary = std::find(s_PrimaryTargets.begin(), s_PrimaryTargets.end(), species) != s_PrimaryTargets.end();
		bool isSecondary = std::find(s_SecondaryTargets.begin(), s_SecondaryTargets.end(), species) != s_SecondaryTargets.end();

		if (ev->Value < 0)
		{
			s_DisplayedEncounter.Cleave.Damage += ev->Value;

			if (isPrimary || isSecondary)
			{
				s_DisplayedEncounter.Target.Damage += ev->Value;
			}
		}
		else if (ev->Value > 0)
		{
			s_DisplayedEncounter.Cleave.Heal += ev->Value;

			if (isPrimary || isSecondary)
			{
				s_DisplayedEncounter.Target.Heal += ev->Value;
			}
		}
		else if (ev->ValueAlt > 0)
		{
			s_DisplayedEncounter.Cleave.Barrier += ev->ValueAlt;

			if (isPrimary || isSecondary)
			{
				s_DisplayedEncounter.Target.Barrier += ev->ValueAlt;
			}
		}
	}
}

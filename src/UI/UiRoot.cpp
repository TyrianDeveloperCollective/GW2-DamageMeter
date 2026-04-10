#include "UiRoot.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include "Nexus/Nexus.h"
#include "Targets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "ImPos/imgui_positioning.h"
#include <Core/Combat/CbtEncounter.h>
#include <string>
#include <array>
#include "RealTime-API/RTAPI.hpp"

#include "Core/Localization.h"
#include "Util/src/Strings.h"

using Encounters = std::array<Encounter_t, 10>;
namespace UiRoot
{
	static AddonAPI_t*                s_APIDefs            = nullptr;
	static NexusLinkData_t*           s_NexusLink          = nullptr;
	static RTAPI::RealTimeData*       s_RealTimeData       = nullptr;

	static std::mutex                 s_Mutex;
	static Encounters::const_iterator s_DisplayedEncounter = {};
	static Encounters::iterator       s_ActiveEncounter    = {};
	static Encounters                 s_History            = {};

	static bool                       s_Incoming           = false;

	void OnCombatEvent(const RTAPI::CombatEvent*);
}

void UiRoot::OnAddonLoaded(int* aSignature)
{
  if (!aSignature) { return; }
  
  if (*aSignature == RTAPI_SIG)
  {
    s_RealTimeData = static_cast<RTAPI::RealTimeData *>(s_APIDefs->DataLink_Get(DL_RTAPI));
  }
}

void UiRoot::OnAddonUnloaded(int* aSignature)
{
  if (!aSignature) { return; }

  if (*aSignature == RTAPI_SIG)
  {
    s_RealTimeData = nullptr;
  }
}

void UiRoot::Create(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;
	ImGui::SetCurrentContext((ImGuiContext*)s_APIDefs->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))s_APIDefs->ImguiMalloc, (void(*)(void*, void*))s_APIDefs->ImguiFree);

	Localization::Init(s_APIDefs);

	s_APIDefs->GUI_Register(RT_Render, UiRoot::Render);
	s_APIDefs->GUI_Register(RT_OptionsRender, UiRoot::Options);
	
	s_NexusLink = static_cast<NexusLinkData_t*>(s_APIDefs->DataLink_Get(DL_NEXUS_LINK));
	
	for(auto &encounter : s_History)
	{
		encounter = Encounter_t{};
	}
	s_DisplayedEncounter = s_History.begin();
	s_ActiveEncounter = s_History.begin();

    
	s_APIDefs->Events_Subscribe(EV_ADDON_LOADED, (EVENT_CONSUME)OnAddonLoaded);
	s_APIDefs->Events_Subscribe(EV_ADDON_UNLOADED, (EVENT_CONSUME)OnAddonUnloaded);
	s_APIDefs->Events_Subscribe(EV_RTAPI_COMBAT_EVENT, (EVENT_CONSUME)OnCombatEvent);
}

void UiRoot::Destroy()
{
	if (!s_APIDefs) { return; }

	s_APIDefs->Events_Unsubscribe(EV_RTAPI_COMBAT_EVENT, (EVENT_CONSUME)OnCombatEvent);
	s_APIDefs->Events_Unsubscribe(EV_ADDON_LOADED, (EVENT_CONSUME)OnAddonLoaded);
	s_APIDefs->Events_Unsubscribe(EV_ADDON_UNLOADED, (EVENT_CONSUME)OnAddonUnloaded);
	
	s_APIDefs->GUI_Deregister(UiRoot::Render);
	s_APIDefs->GUI_Deregister(UiRoot::Options);

	/* Make sure all pending callbacks are done */
	const std::lock_guard<std::mutex> lock(s_Mutex);
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
	if (!s_NexusLink || !s_NexusLink->IsGameplay)
	{
		return;
	}
    std::string wndName = Translate(ETexts::CombatMetrics);
	wndName.append("###CMX::Metrics");

	static ImGuiExt::Positioning_t s_Position{};

	ImGuiWindowFlags wndFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiExt::UpdatePosition("###CMX::Metrics");


	if (!s_RealTimeData)
	{
		ImGui::Begin(wndName.c_str(), 0, wndFlags);
		ImGui::TextColored(ImVec4(0.675f, 0.349f, 0.349f, 1.0f), Translate(ETexts::DisabledCombatTracker));
		ImGui::End();
		return;
	}

	if (s_RealTimeData->MapType == RTAPI::EMapType::PvP)
	{
		ImGui::Begin(wndName.c_str(), 0, wndFlags);
		ImGui::TextColored(ImVec4(0.675f, 0.349f, 0.349f, 1.0f), Translate(ETexts::DisabledInPvP));
		ImGui::End();
		return;
	}

	const std::lock_guard<std::mutex> lock(s_Mutex);
	if (ImGui::Begin(wndName.c_str(), 0, wndFlags))
	{
		uint64_t cbtDurationMs = max(s_DisplayedEncounter->TimeEnd - s_DisplayedEncounter->TimeStart, 1000);
		float cbtDuration = cbtDurationMs / 1000.f;

		std::string durationStr = s_DisplayedEncounter->Duration();

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

			const float* damageCleave = s_Incoming ? &s_DisplayedEncounter->InCleave.Damage : &s_DisplayedEncounter->OutCleave.Damage;
			const float* damageTarget = s_Incoming ? &s_DisplayedEncounter->InTarget.Damage : &s_DisplayedEncounter->OutTarget.Damage;

			const float* healCleave = s_Incoming ? &s_DisplayedEncounter->InCleave.Heal : &s_DisplayedEncounter->OutCleave.Heal;
			const float* healTarget = s_Incoming ? &s_DisplayedEncounter->InTarget.Heal : &s_DisplayedEncounter->OutTarget.Heal;

			const float* barrierCleave = s_Incoming ? &s_DisplayedEncounter->InCleave.Barrier : &s_DisplayedEncounter->OutCleave.Barrier;
			const float* barrierTarget = s_Incoming ? &s_DisplayedEncounter->InTarget.Barrier : &s_DisplayedEncounter->OutTarget.Barrier;

			/* DPS Target */
			ImGui::TableNextColumn();
			std::string dpsTarget = *damageTarget < 0.f
				? String::FormatNumberDenominated(abs(*damageTarget) / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(dpsTarget.c_str()).x);
			ImGui::Text(dpsTarget.c_str());
			TooltipGeneric("%.0f, %s", abs(*damageTarget), durationStr.c_str());

			/* DPS Cleave */
			ImGui::TableNextColumn();
			std::string dpsCleave = *damageCleave < 0.f
				? String::FormatNumberDenominated(abs(*damageCleave) / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(dpsCleave.c_str()).x);
			ImGui::Text(dpsCleave.c_str());
			TooltipGeneric("%.0f, %s", abs(*damageCleave), durationStr.c_str());

			/* Heal row. */
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled(Translate(ETexts::Heal));

			/* Heal Target */
			ImGui::TableNextColumn();
			std::string hpsTarget = *healTarget > 0.f
				? String::FormatNumberDenominated(*healTarget / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(hpsTarget.c_str()).x);
			ImGui::Text(hpsTarget.c_str());
			TooltipGeneric("%.0f, %s", *healTarget, durationStr.c_str());

			/* Heal Cleave */
			ImGui::TableNextColumn();
			std::string hpsCleave = *healCleave > 0.f
				? String::FormatNumberDenominated(*healCleave / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(hpsCleave.c_str()).x);
			ImGui::Text(hpsCleave.c_str());
			TooltipGeneric("%.0f, %s", *healCleave, durationStr.c_str());

			/* Barrier row. */
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled(Translate(ETexts::Barrier));

			/* Barrier Target */
			ImGui::TableNextColumn();
			std::string bpsTarget = *barrierTarget > 0.f
				? String::FormatNumberDenominated(*barrierTarget / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(bpsTarget.c_str()).x);
			ImGui::Text(bpsTarget.c_str());
			TooltipGeneric("%.0f, %s", *barrierTarget, durationStr.c_str());

			/* Barrier Cleave*/
			ImGui::TableNextColumn();
			std::string bpsCleave = *barrierCleave > 0.f
				? String::FormatNumberDenominated(*barrierCleave / cbtDuration) + "/s"
				: "-/s";
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(bpsCleave.c_str()).x);
			ImGui::Text(bpsCleave.c_str());
			TooltipGeneric("%.0f, %s", *barrierCleave, durationStr.c_str());
		}
		ImGui::EndTable();
	}

	if (ImGui::BeginPopupContextWindow("###CMX::Metrics::CtxMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::Button(s_Incoming ? Translate(ETexts::Incoming) : Translate(ETexts::Outgoing)))
		{
			s_Incoming = !s_Incoming;
		}

		if (ImGui::BeginMenu("History"))
		{
			bool hasHistory = false;
			for (auto encounter = s_History.begin(); encounter != s_History.end(); ++encounter)
			{
				if(!encounter->HasStarted())
				{
					continue;
				}
				const std::string name = encounter == s_ActiveEncounter ? "Current" : encounter->GetName();
				if (ImGui::Selectable(name.c_str()))
				{
					s_DisplayedEncounter = encounter;
				}
				hasHistory = true;
			}
			if(!hasHistory)
			{
				ImGui::Text("No history.");
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	ImGuiExt::ContextMenuPosition("###CMX::Metrics::CtxMenu");

	ImGui::End();
	
	auto inCombat = static_cast<uint32_t>(s_RealTimeData->CharacterState) & static_cast<uint32_t>(RTAPI::ECharacterState::IsInCombat);
	if(!inCombat && s_ActiveEncounter->HasStarted())
	{
		UiRoot::OnCombatEnd();
	}
}

void UiRoot::Options()
{
}

void UiRoot::OnCombatStart()
{
	auto lastEncounter = (s_ActiveEncounter == s_History.begin() ? s_History.end() : s_ActiveEncounter) - 1;
	if(s_DisplayedEncounter == lastEncounter)
	{
		s_DisplayedEncounter = s_ActiveEncounter;
	}
}

void UiRoot::OnCombatEnd()
{
	if ((s_ActiveEncounter->TimeEnd - s_ActiveEncounter->TimeStart) >= 5000)
	{
		++s_ActiveEncounter;
		if(s_ActiveEncounter == s_History.end())
		{
			s_ActiveEncounter = s_History.begin();
		}
	}
    *s_ActiveEncounter = Encounter_t{};
}

void UiRoot::OnCombatEvent(const RTAPI::CombatEvent* ev)
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	if (!s_ActiveEncounter->HasStarted())
	{
		UiRoot::OnCombatStart();
		s_ActiveEncounter->TimeStart = ev->Time;
	}

	s_ActiveEncounter->TimeEnd = ev->Time;

	/* Check for trigger name. */
	if (!s_ActiveEncounter->TriggerName[0])
	{
		if (ev->SrcAgent && ev->SrcAgent->ID)
		{
			if (std::find(s_PrimaryTargets.begin(), s_PrimaryTargets.end(), ev->SrcAgent->ID) != s_PrimaryTargets.end())
			{
				std::memcpy(s_ActiveEncounter->TriggerName, ev->SrcAgent->Name, sizeof(s_ActiveEncounter->TriggerName));
			}
		}
		else if (ev->DstAgent && ev->DstAgent->ID)
		{
			if (std::find(s_PrimaryTargets.begin(), s_PrimaryTargets.end(), ev->DstAgent->ID) != s_PrimaryTargets.end())
			{
				std::memcpy(s_ActiveEncounter->TriggerName, ev->DstAgent->Name, sizeof(s_ActiveEncounter->TriggerName));
			}
		}
	}

	/* Check for self ID. */
	if(!s_ActiveEncounter->SelfID)
	{
		if(ev->SrcAgent->IsSelf)
		{
			s_ActiveEncounter->SelfID = ev->SrcAgent->ID;
		}
		else if(ev->DstAgent->IsSelf)
		{
			s_ActiveEncounter->SelfID = ev->DstAgent->ID;
		}
	}

	/* Process stats. */
	{
		/*              hasSrc       &&  src is self          || src is owned minion */
		bool outgoing = ev->SrcAgent && (ev->SrcAgent->IsSelf || (ev->SrcAgent->IsMinion && ev->SrcAgent->OwnerID == s_ActiveEncounter->SelfID));
		bool incoming = ev->DstAgent && ev->DstAgent->IsSelf;

		if (outgoing && ev->DstAgent)
		{
			bool isTarget = std::find(s_PrimaryTargets.begin(), s_PrimaryTargets.end(), ev->DstAgent->SpeciesID) != s_PrimaryTargets.end()
				|| std::find(s_SecondaryTargets.begin(), s_SecondaryTargets.end(), ev->DstAgent->SpeciesID) != s_SecondaryTargets.end();

			if (ev->Value < 0)
			{
				/* Damage */
				s_ActiveEncounter->OutCleave.Damage += ev->Value;

				if (isTarget)
				{
					s_ActiveEncounter->OutTarget.Damage += ev->Value;
				}
			}
			else if (ev->Value > 0)
			{
				/* Heal */
				s_ActiveEncounter->OutCleave.Heal += ev->Value;

				if (isTarget)
				{
					s_ActiveEncounter->OutTarget.Heal += ev->Value;
				}
			}
			else if (ev->ValueAlt > 0)
			{
				/* Barrier */
				s_ActiveEncounter->OutCleave.Barrier += ev->ValueAlt;

				if (isTarget)
				{
					s_ActiveEncounter->OutTarget.Barrier += ev->ValueAlt;
				}
			}
		}
		else if (incoming && ev->SrcAgent)
		{
			bool isTarget = std::find(s_PrimaryTargets.begin(), s_PrimaryTargets.end(), ev->SrcAgent->SpeciesID) != s_PrimaryTargets.end()
				|| std::find(s_SecondaryTargets.begin(), s_SecondaryTargets.end(), ev->SrcAgent->SpeciesID) != s_SecondaryTargets.end();

			if (ev->Value < 0)
			{
				/* Damage */
				s_ActiveEncounter->InCleave.Damage += ev->Value;

				if (isTarget)
				{
					s_ActiveEncounter->InTarget.Damage += ev->Value;
				}
			}
			else if (ev->Value > 0)
			{
				/* Heal */
				s_ActiveEncounter->InCleave.Heal += ev->Value;

				if (isTarget)
				{
					s_ActiveEncounter->InTarget.Heal += ev->Value;
				}
			}
			else if (ev->ValueAlt > 0)
			{
				/* Barrier */
				s_ActiveEncounter->InCleave.Barrier += ev->ValueAlt;

				if (isTarget)
				{
					s_ActiveEncounter->InTarget.Barrier += ev->ValueAlt;
				}
			}
		}
	}
}

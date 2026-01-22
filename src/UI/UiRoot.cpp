#include "UiRoot.h"

#include <algorithm>
#include <mutex>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "ImPos/imgui_positioning.h"

#include "Core/Combat/Combat.h"
#include "Core/Localization.h"
#include "GW2RE/Game/Map/MapDef.h"
#include "GW2RE/Game/MissionContext.h"
#include "GW2RE/Game/PropContext.h"
#include "Util/src/Strings.h"

namespace UiRoot
{
	static AddonAPI_t*               s_APIDefs            = nullptr;
	static NexusLinkData_t*          s_NexusLink          = nullptr;

	static std::mutex                s_Mutex;
	static Encounter_t               s_NullEncounter      = {}; // Dummy encounter
	static Encounter_t*              s_DisplayedEncounter = &s_NullEncounter;
	static std::vector<Encounter_t*> s_History            = {};

	static bool                      s_Incoming           = false;

	void OnCombatEvent();
}

/* Small helper to properly delete collection entries. */
void DeleteEncounter(Encounter_t* aEncounter)
{
	for (auto [id, ag] : aEncounter->Agents)
	{
		delete ag;
	}
	aEncounter->Agents.clear();

	for (auto [id, sk] : aEncounter->Skills)
	{
		delete sk;
	}
	aEncounter->Skills.clear();

	for (auto ev : aEncounter->CombatEvents)
	{
		delete ev;
	}
	aEncounter->CombatEvents.clear();

	delete aEncounter;
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

	s_APIDefs->Events_Subscribe(EV_CMX_COMBAT, (EVENT_CONSUME)OnCombatEvent);
}

void UiRoot::Destroy()
{
	if (!s_APIDefs) { return; }

	s_APIDefs->Events_Unsubscribe(EV_CMX_COMBAT, (EVENT_CONSUME)OnCombatEvent);

	s_APIDefs->GUI_Deregister(UiRoot::Render);
	s_APIDefs->GUI_Deregister(UiRoot::Options);

	const std::lock_guard<std::mutex> lock(s_Mutex);
	for (Encounter_t* encounter : s_History)
	{
		DeleteEncounter(encounter);
	}
	s_History.clear();
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

	if (!missionctx || !missionctx->CurrentMap)
	{
		return;
	}

	if (!Combat::IsRegistered())
	{
		ImGui::Begin(wndName.c_str(), 0, wndFlags);
		ImGui::TextColored(ImVec4(0.675f, 0.349f, 0.349f, 1.0f), Translate(ETexts::DisabledCombatTracker));
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

			float* damageCleave = s_Incoming ? &s_DisplayedEncounter->InCleave.Damage : &s_DisplayedEncounter->OutCleave.Damage;
			float* damageTarget = s_Incoming ? &s_DisplayedEncounter->InTarget.Damage : &s_DisplayedEncounter->OutTarget.Damage;

			float* healCleave = s_Incoming ? &s_DisplayedEncounter->InCleave.Heal : &s_DisplayedEncounter->OutCleave.Heal;
			float* healTarget = s_Incoming ? &s_DisplayedEncounter->InTarget.Heal : &s_DisplayedEncounter->OutTarget.Heal;

			float* barrierCleave = s_Incoming ? &s_DisplayedEncounter->InCleave.Barrier : &s_DisplayedEncounter->OutCleave.Barrier;
			float* barrierTarget = s_Incoming ? &s_DisplayedEncounter->InTarget.Barrier : &s_DisplayedEncounter->OutTarget.Barrier;

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
			if (s_History.size() > 0)
			{
				for (int32_t i = s_History.size() - 1; i >= 0; i--)
				{
					Encounter_t* encounter = s_History[i];

					if (ImGui::Selectable(i == s_History.size() - 1 ? "Current" : encounter->GetName().c_str()))
					{
						s_DisplayedEncounter = encounter;
					}
				}
			}
			else
			{
				ImGui::Text("No history.");
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	ImGuiExt::ContextMenuPosition("###CMX::Metrics::CtxMenu");

	ImGui::End();
}

void UiRoot::Options()
{
	
}

void UiRoot::OnCombatEnd()
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	/* Only keep last 10 encounters. */
	while (s_History.size() > 10)
	{
		Encounter_t* delEncounter = s_History.front();

		/* Selecting the oldest encounter means you can keep more than 10 encounters in history, but who cares. */
		if (delEncounter != s_DisplayedEncounter)
		{
			DeleteEncounter(delEncounter);
			s_History.erase(s_History.begin());
		}
	}

	/* Reset displayed to null dummy. */
	s_DisplayedEncounter = &s_NullEncounter;

	if (!s_History.empty())
	{
		/* Assuming this is only executed after combat end. */
		Encounter_t* mostrecent = s_History.back();

		/* If less than 5 seconds duration, drop it. */
		if ((mostrecent->TimeEnd - mostrecent->TimeStart) < 5000)
		{
			DeleteEncounter(mostrecent);
			s_History.erase(s_History.end() - 1);
		}

		if (s_DisplayedEncounter == &s_NullEncounter && s_History.size() > 0)
		{
			s_DisplayedEncounter = s_History.back();
		}
	}
}

void UiRoot::OnCombatEvent()
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	Encounter_t* current = Combat::GetCurrentEncounter();

	if (current == nullptr) { return; }

	if (s_History.size() > 0 && s_DisplayedEncounter == s_History.back())
	{
		s_DisplayedEncounter = &s_NullEncounter;
	}

	auto it = std::find(s_History.begin(), s_History.end(), current);

	if (it == s_History.end())
	{
		s_History.push_back(current);
	}

	if (s_DisplayedEncounter == &s_NullEncounter)
	{
		s_DisplayedEncounter = current;
	}
}

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
#include "Targets.h"
#include "Util/src/Strings.h"

namespace UiRoot
{
	static AddonAPI_t*  s_APIDefs            = nullptr;
	
	static Encounter_t  s_NullEncounter      = {};
	static Encounter_t* s_DisplayedEncounter = &s_NullEncounter;

	static bool         s_Incoming           = false;

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

	s_APIDefs->Events_Subscribe(EV_CMX_COMBAT, (EVENT_CONSUME)OnCombatEvent);
}

void UiRoot::Destroy()
{
	if (!s_APIDefs) { return; }

	s_APIDefs->Events_Unsubscribe(EV_CMX_COMBAT, (EVENT_CONSUME)OnCombatEvent);

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
		uint64_t cbtDurationMs = s_DisplayedEncounter->TimeEnd - s_DisplayedEncounter->TimeStart;
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
	s_DisplayedEncounter = Combat::GetCurrentEncounter();
}

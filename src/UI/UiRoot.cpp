#include "UiRoot.h"

#include <mutex>

#include "imgui/imgui.h"

#include "Core/Combat/Combat.h"
#include "Core/TextCache/TextCache.h"
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

	void RefreshData();
}

void UiRoot::Create(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;

	ImGui::SetCurrentContext((ImGuiContext*)s_APIDefs->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))s_APIDefs->ImguiMalloc, (void(*)(void*, void*))s_APIDefs->ImguiFree);

	s_APIDefs->GUI_Register(RT_Render, UiRoot::Render);
	s_APIDefs->GUI_Register(RT_OptionsRender, UiRoot::Options);
}

void UiRoot::Destroy()
{
	s_APIDefs->GUI_Deregister(UiRoot::Render);
	s_APIDefs->GUI_Deregister(UiRoot::Options);
}

void UiRoot::Render()
{
	RefreshData();

	uint32_t targetId = Combat::GetTargetID();

	std::string wndName;
	wndName.append(
		targetId != 0
		? TextCache::GetAgentName(targetId)
		: "No target."
	);
	wndName.append("###RC_Meter");

	if (ImGui::Begin(wndName.c_str()))
	{
		uint64_t cbtDurationMs = s_DisplayedEncounter.Encounter.TimeEnd - s_DisplayedEncounter.Encounter.TimeStart;

		time_t timeStart = s_DisplayedEncounter.Encounter.TimeStart / 1000;
		tm tmStart{};
		localtime_s(&tmStart, &timeStart);
		char startTime[64];
		strftime(startTime, sizeof(startTime), "%H:%M:%S", &tmStart);

		time_t timeEnd = s_DisplayedEncounter.Encounter.TimeEnd / 1000;
		tm tmEnd{};
		localtime_s(&tmEnd, &timeEnd);
		char endTime[64];
		strftime(endTime, sizeof(endTime), "%H:%M:%S", &tmEnd);

		ImGui::Text("Combat start: %s.%u", startTime, s_DisplayedEncounter.Encounter.TimeStart % 1000);
		ImGui::Text("Combat end: %s.%u", endTime, s_DisplayedEncounter.Encounter.TimeEnd % 1000);
		ImGui::Text("Duration: %llu.%llus", cbtDurationMs / 1000, cbtDurationMs % 1000);

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

void UiRoot::RefreshData()
{
	static uint64_t s_LastRefresh = 0;

	uint64_t now = Time::GetTimestampMs();

	if (now - s_LastRefresh < 1000)
	{
		return;
	}

	s_LastRefresh = now;

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

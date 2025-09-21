#include "UiRoot.h"

#include <mutex>

#include "imgui/imgui.h"

#include "Core/Combat/Combat.h"
#include "Core/TextCache/TextCache.h"
#include "Util/src/Time.h"

namespace UiRoot
{
	static AddonAPI_t* s_APIDefs = nullptr;
	
	static std::mutex            s_Mutex;
	static std::vector<uint32_t> s_Agents;
	static std::vector<uint32_t> s_Skills;
	static float s_TotalDmg;
	static float s_TotalHeal;
	static float s_TotalBarrier;

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
		ImGui::TextDisabled("Dmg: %.0f", s_TotalDmg);
		ImGui::TextDisabled("Heal: %.0f", s_TotalHeal);
		ImGui::TextDisabled("Barrier: %.0f", s_TotalBarrier);

		ImGui::Separator();

		ImGui::TextDisabled("Agents");
		for (uint32_t id : s_Agents)
		{
			if (Combat::GetSelfID() == id)
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
		for (uint32_t id : s_Skills)
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

	std::vector<CombatEvent_t*> events = Combat::GetCombatEvents();

	const std::lock_guard<std::mutex> lock(s_Mutex);

	s_Agents.clear();
	s_Skills.clear();
	s_TotalDmg = 0;
	s_TotalHeal = 0;
	s_TotalBarrier = 0;

	uint32_t self = Combat::GetSelfID();

	for (CombatEvent_t* ev : events)
	{
		if (std::find(s_Agents.begin(), s_Agents.end(), ev->SrcAgentID) == s_Agents.end())
		{
			s_Agents.push_back(ev->SrcAgentID);
		}

		if (ev->SrcAgentID == self )
		{
			if (ev->Value < 0)
			{
				s_TotalDmg += ev->Value;
			}
			else if (ev->Value > 0)
			{
				s_TotalHeal += ev->Value;
			}
			else if (ev->ValueAlt > 0)
			{
				s_TotalBarrier += ev->ValueAlt;
			}
		}

		if (std::find(s_Agents.begin(), s_Agents.end(), ev->DstAgentID) == s_Agents.end())
		{
			s_Agents.push_back(ev->DstAgentID);
		}

		if (std::find(s_Skills.begin(), s_Skills.end(), ev->SkillID) == s_Skills.end())
		{
			s_Skills.push_back(ev->SkillID);
		}
	}
}

#include "UiRoot.h"

#include <algorithm>
#include <mutex>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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

	s_APIDefs->Events_Subscribe(EV_PCM_COMBAT, (EVENT_CONSUME)OnCombatEvent);
	s_APIDefs->Events_Subscribe(EV_PCM_COMBAT_END, (EVENT_CONSUME)OnCombatEvent);
}

void UiRoot::Destroy()
{
	if (!s_APIDefs) { return; }

	s_APIDefs->Events_Unsubscribe(EV_PCM_COMBAT, (EVENT_CONSUME)OnCombatEvent);
	s_APIDefs->Events_Unsubscribe(EV_PCM_COMBAT_END, (EVENT_CONSUME)OnCombatEvent);

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

enum class EPositioning
{
	Manual,
	Anchor,
	ScreenRelative
};

enum class EAnchorPoint
{
	TopLeft,
	TopRight,
	BottomRight,
	BottomLeft
};
std::string StringFrom(EAnchorPoint aAnchorPoint)
{
	switch (aAnchorPoint)
	{
		default:
		case EAnchorPoint::TopLeft:
		{
			return "Top Left";
		}
		case EAnchorPoint::TopRight:
		{
			return "Top Right";
		}
		case EAnchorPoint::BottomRight:
		{
			return "Bottom Right";
		}
		case EAnchorPoint::BottomLeft:
		{
			return "Bottom Left";
		}
	}
}

void AnchorPointCombo(std::string aLabel, EAnchorPoint* aAnchorPoint)
{
	if (ImGui::BeginCombo(aLabel.c_str(), StringFrom(*aAnchorPoint).c_str()))
	{
		if (ImGui::Selectable(StringFrom(EAnchorPoint::TopLeft).c_str(), *aAnchorPoint == EAnchorPoint::TopLeft))
		{
			*aAnchorPoint = EAnchorPoint::TopLeft;
		}
		if (ImGui::Selectable(StringFrom(EAnchorPoint::TopRight).c_str(), *aAnchorPoint == EAnchorPoint::TopRight))
		{
			*aAnchorPoint = EAnchorPoint::TopRight;
		}
		if (ImGui::Selectable(StringFrom(EAnchorPoint::BottomRight).c_str(), *aAnchorPoint == EAnchorPoint::BottomRight))
		{
			*aAnchorPoint = EAnchorPoint::BottomRight;
		}
		if (ImGui::Selectable(StringFrom(EAnchorPoint::BottomLeft).c_str(), *aAnchorPoint == EAnchorPoint::BottomLeft))
		{
			*aAnchorPoint = EAnchorPoint::BottomLeft;
		}

		ImGui::EndCombo();
	}
}

struct Positioning_t
{
	EPositioning Mode         = EPositioning::Manual;

	EAnchorPoint OriginCorner = EAnchorPoint::TopLeft;
	EAnchorPoint TargetCorner = EAnchorPoint::TopLeft;
	ImGuiID      TargetID     = 0;
	float        X            = 0.f;
	float        Y            = 0.f;
};

ImGuiWindowFlags AdvancePosition(Positioning_t& aPosition)
{
	Positioning_t& position = aPosition;

	ImGuiWindowFlags result = 0;

	switch (position.Mode)
	{
		default:
		case EPositioning::Manual: { break; }
		case EPositioning::Anchor:
		{
			ImVec2 pivot;
			switch (position.OriginCorner)
			{
				default:
				case EAnchorPoint::TopLeft:
					pivot = ImVec2(0.f, 0.f);
					break;
				case EAnchorPoint::TopRight:
					pivot = ImVec2(1.f, 0.f);
					break;
				case EAnchorPoint::BottomRight:
					pivot = ImVec2(1.f, 1.f);
					break;
				case EAnchorPoint::BottomLeft:
					pivot = ImVec2(0.f, 1.f);
					break;
			}
			ImVec2 pos;
			ImGuiWindow* targetWindow = ImGui::FindWindowByID(position.TargetID);

			if (!targetWindow)
			{
				break;
			}

			switch (position.TargetCorner)
			{
				default:
				case EAnchorPoint::TopLeft:
					pos = ImVec2(targetWindow->Pos.x + position.X, targetWindow->Pos.y + position.Y);
					break;
				case EAnchorPoint::TopRight:
					pos = ImVec2(targetWindow->Pos.x + targetWindow->Size.x + position.X, targetWindow->Pos.y + position.Y);
					break;
				case EAnchorPoint::BottomRight:
					pos = ImVec2(targetWindow->Pos.x + targetWindow->Size.x + position.X, targetWindow->Pos.y + targetWindow->Size.y + position.Y);
					break;
				case EAnchorPoint::BottomLeft:
					pos = ImVec2(targetWindow->Pos.x + position.X, targetWindow->Pos.y + targetWindow->Size.y + position.Y);
					break;
			}
			ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
			result |= ImGuiWindowFlags_NoMove;
			break;
		}
		case EPositioning::ScreenRelative:
		{
			ImVec2 pivot;
			ImVec2 pos;
			switch (position.TargetCorner)
			{
				default:
				case EAnchorPoint::TopLeft:
					pivot = ImVec2(0.f, 0.f);
					pos = ImVec2(0.f + position.X, 0.f + position.Y);
					break;
				case EAnchorPoint::TopRight:
					pivot = ImVec2(1.f, 0.f);
					pos = ImVec2(ImGui::GetIO().DisplaySize.x - position.X, 0.f + position.Y);
					break;
				case EAnchorPoint::BottomRight:
					pivot = ImVec2(1.f, 1.f);
					pos = ImVec2(ImGui::GetIO().DisplaySize.x - position.X, ImGui::GetIO().DisplaySize.y - position.Y);
					break;
				case EAnchorPoint::BottomLeft:
					pivot = ImVec2(0.f, 1.f);
					pos = ImVec2(0.f + position.X, ImGui::GetIO().DisplaySize.y - position.Y);
					break;
			}
			ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
			result |= ImGuiWindowFlags_NoMove;
			break;
		}
	}

	return result;
}

void ContextMenuPosition(Positioning_t& aPosition, std::string aContextMenuName)
{
	Positioning_t& position = aPosition;
	ImGuiWindow* currentWindow = ImGui::GetCurrentWindow();

	if (ImGui::BeginPopupContextWindow(aContextMenuName.c_str(), ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::BeginMenu("Position"))
		{
			if (ImGui::RadioButton("Manual", position.Mode == EPositioning::Manual))
			{
				position.Mode = EPositioning::Manual;
			}
			if (ImGui::RadioButton("Anchor to window", position.Mode == EPositioning::Anchor))
			{
				position.Mode = EPositioning::Anchor;
			}
			if (ImGui::RadioButton("Screen-relative", position.Mode == EPositioning::ScreenRelative))
			{
				position.Mode = EPositioning::ScreenRelative;
			}

			switch (position.Mode)
			{
				default:
				case EPositioning::Manual:
				{
					/* No additional options, nothing to do. */
					break;
				}
				case EPositioning::Anchor:
				{
					ImGui::Separator();

					AnchorPointCombo("Origin Anchor", &position.OriginCorner);
					AnchorPointCombo("Target Anchor", &position.TargetCorner);

					std::vector<ImGuiWindow*> targetWindows;

					/* GET_WINDOW_IDS */
					{
						for (ImGuiWindow* wnd : ImGui::GetCurrentContext()->Windows)
						{
							if (strcmp(wnd->Name, "Debug##Default") == 0) { continue; }
							if (strcmp(wnd->Name, currentWindow->Name) == 0) { continue; }
							if (wnd->ParentWindow != NULL) { continue; }
							if (!wnd->Active) { continue; }

							targetWindows.push_back(wnd);
						}
					}

					ImGuiWindow* targetWindow = ImGui::FindWindowByID(position.TargetID);

					if (ImGui::BeginCombo("Target Window", targetWindow ? targetWindow->Name : "(null)"))
					{
						for (ImGuiWindow* wnd : targetWindows)
						{
							char buffID[32];
							sprintf_s(buffID, sizeof(buffID), "%04x", wnd->ID);

							if (ImGui::Selectable(buffID))
							{
								position.TargetID = wnd->ID;
							}
						}
						ImGui::EndCombo();
					}

					ImGui::InputFloat("X", &position.X, 1.f, 50.f, "%.0f");
					ImGui::InputFloat("Y", &position.Y, 1.f, 50.f, "%.0f");

					/* RENDER_WINDOW_IDS */
					{
						ImDrawList* dl = ImGui::GetForegroundDrawList();

						bool canConnect = false;
						ImVec2 centerTarget;

						for (ImGuiWindow* wnd : targetWindows)
						{
							ImVec2 nameSz = ImGui::CalcTextSize(wnd->Name);

							ImColor highlight = wnd->ID == position.TargetID ? ImColor(1.f, 0.f, 0.f, 1.f) : ImColor(1.f, 1.f, 0.f, 1.f);

							/* Draw background for label. */
							dl->AddRectFilled(ImVec2(wnd->Pos.x, wnd->Pos.y), ImVec2(wnd->Pos.x + nameSz.x + 2.f, wnd->Pos.y + nameSz.y + 2.f), ImColor(0.f, 0.f, 0.f, 1.f));

							/* Draw outline for label. */
							dl->AddRect(ImVec2(wnd->Pos.x, wnd->Pos.y), ImVec2(wnd->Pos.x + nameSz.x + 2.f, wnd->Pos.y + nameSz.y + 2.f), highlight);

							/* Draw outline for window. */
							dl->AddRect(ImVec2(wnd->Pos.x, wnd->Pos.y), ImVec2(wnd->Pos.x + wnd->Size.x, wnd->Pos.y + wnd->Size.y), highlight);

							char buffID[32];
							sprintf_s(buffID, sizeof(buffID), "%04x", wnd->ID);

							/* Draw label text. */
							dl->AddText(ImVec2(wnd->Pos.x + 1.f, wnd->Pos.y + 1.f), highlight, buffID);

							if (wnd->ID == position.TargetID)
							{
								switch (position.TargetCorner)
								{
									default:
									case EAnchorPoint::TopLeft:
										centerTarget = ImVec2(wnd->Pos.x, wnd->Pos.y);
										break;
									case EAnchorPoint::TopRight:
										centerTarget = ImVec2(wnd->Pos.x + wnd->Size.x, wnd->Pos.y);
										break;
									case EAnchorPoint::BottomRight:
										centerTarget = ImVec2(wnd->Pos.x + wnd->Size.x, wnd->Pos.y + wnd->Size.y);
										break;
									case EAnchorPoint::BottomLeft:
										centerTarget = ImVec2(wnd->Pos.x, wnd->Pos.y + wnd->Size.y);
										break;
								}

								canConnect = true;
							}
						}

						ImVec2 centerOrigin;
						switch (position.OriginCorner)
						{
							default:
							case EAnchorPoint::TopLeft:
								centerOrigin = ImVec2(currentWindow->Pos.x, currentWindow->Pos.y);
								break;
							case EAnchorPoint::TopRight:
								centerOrigin = ImVec2(currentWindow->Pos.x + currentWindow->Size.x, currentWindow->Pos.y);
								break;
							case EAnchorPoint::BottomRight:
								centerOrigin = ImVec2(currentWindow->Pos.x + currentWindow->Size.x, currentWindow->Pos.y + currentWindow->Size.y);
								break;
							case EAnchorPoint::BottomLeft:
								centerOrigin = ImVec2(currentWindow->Pos.x, currentWindow->Pos.y + currentWindow->Size.y);
								break;
						}

						if (canConnect)
						{
							dl->AddLine(centerOrigin, centerTarget, ImColor(1.f, 1.f, 0.f, 1.f));
							dl->AddCircleFilled(centerTarget, 3.f, ImColor(1.f, 0.f, 0.f, 1.f));
						}

						dl->AddCircleFilled(centerOrigin, 3.f, ImColor(1.f, 0.f, 0.f, 1.f));
					}

					break;
				}
				case EPositioning::ScreenRelative:
				{
					ImGui::Separator();

					AnchorPointCombo("Screen Anchor", &position.TargetCorner);
					ImGui::InputFloat("X", &position.X, 1.f, 50.f, "%.0f");
					ImGui::InputFloat("Y", &position.Y, 1.f, 50.f, "%.0f");

					{
						ImDrawList* dl = ImGui::GetForegroundDrawList();

						ImVec2 centerOrigin;
						ImVec2 centerTarget;
						switch (position.TargetCorner)
						{
							default:
							case EAnchorPoint::TopLeft:
								centerOrigin = ImVec2(currentWindow->Pos.x, currentWindow->Pos.y);
								centerTarget = ImVec2(0.f, 0.f);
								break;
							case EAnchorPoint::TopRight:
								centerOrigin = ImVec2(currentWindow->Pos.x + currentWindow->Size.x, currentWindow->Pos.y);
								centerTarget = ImVec2(ImGui::GetIO().DisplaySize.x, 0.f);
								break;
							case EAnchorPoint::BottomRight:
								centerOrigin = ImVec2(currentWindow->Pos.x + currentWindow->Size.x, currentWindow->Pos.y + currentWindow->Size.y);
								centerTarget = ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
								break;
							case EAnchorPoint::BottomLeft:
								centerOrigin = ImVec2(currentWindow->Pos.x, currentWindow->Pos.y + currentWindow->Size.y);
								centerTarget = ImVec2(0.f, ImGui::GetIO().DisplaySize.y);
								break;
						}

						dl->AddLine(centerOrigin, centerTarget, ImColor(1.f, 1.f, 0.f, 1.f));
						dl->AddCircleFilled(centerTarget, 3.f, ImColor(1.f, 0.f, 0.f, 1.f));
						dl->AddCircleFilled(centerOrigin, 3.f, ImColor(1.f, 0.f, 0.f, 1.f));
					}

					break;
				}
			}

			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
}

void UiRoot::Render()
{
	GW2RE::CPropContext propctx = GW2RE::CPropContext::Get();
	GW2RE::MissionContext_t* missionctx = propctx.GetMissionCtx();

	std::string wndName = Translate(ETexts::CombatMetrics);
	wndName.append("###PCM::Metrics");

	static Positioning_t s_Position{};

	ImGuiWindowFlags wndFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | AdvancePosition(s_Position);

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

		if (ImGui::BeginTable("Data", 3))
		{
			ImGui::TableSetupColumn("###NULL", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn((std::string(Translate(ETexts::Target)) + "###Target").c_str(), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn((std::string(Translate(ETexts::Cleave)) + "###Cleave").c_str(), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			/* Damage row. */
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(Translate(ETexts::Damage));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s/s", String::FormatNumberDenominated(abs(s_DisplayedEncounter.Target.Damage) / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
			TooltipGeneric("%.0f", abs(s_DisplayedEncounter.Target.Damage));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s/s", String::FormatNumberDenominated(abs(s_DisplayedEncounter.Cleave.Damage) / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
			TooltipGeneric("%.0f", abs(s_DisplayedEncounter.Cleave.Damage));

			/* Heal row. */
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(Translate(ETexts::Heal));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s/s", String::FormatNumberDenominated(abs(s_DisplayedEncounter.Target.Heal) / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
			TooltipGeneric("%.0f", abs(s_DisplayedEncounter.Target.Heal));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s/s", String::FormatNumberDenominated(abs(s_DisplayedEncounter.Cleave.Heal) / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
			TooltipGeneric("%.0f", abs(s_DisplayedEncounter.Cleave.Heal));

			/* Barrier row. */
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(Translate(ETexts::Barrier));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s/s", String::FormatNumberDenominated(abs(s_DisplayedEncounter.Target.Barrier) / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
			TooltipGeneric("%.0f", abs(s_DisplayedEncounter.Target.Barrier));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s/s", String::FormatNumberDenominated(abs(s_DisplayedEncounter.Cleave.Barrier) / (max(cbtDurationMs, 1000) / 1000.f)).c_str());
			TooltipGeneric("%.0f", abs(s_DisplayedEncounter.Cleave.Barrier));
		}
		ImGui::EndTable();
	}

	if (ImGui::BeginPopupContextWindow("###PCM::Metrics::CtxMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		const std::lock_guard<std::mutex> lock(s_Mutex);
		if (ImGui::Button(s_Incoming ? Translate(ETexts::Incoming) : Translate(ETexts::Outgoing)))
		{
			s_Incoming = !s_Incoming;
			CalculateTotals();
		}

		ImGui::EndPopup();
	}

	ContextMenuPosition(s_Position, "###PCM::Metrics::CtxMenu");

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

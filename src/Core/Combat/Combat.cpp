#include "Combat.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>

#include "Core/Addon.h"
#include "Core/Combat/CbtEncounter.h"
#include "Core/TextCache/TextCache.h"
#include "GW2RE/Game/Agent/Agent.h"
#include "GW2RE/Game/Char/Character.h"
#include "GW2RE/Game/Char/ChCliContext.h"
#include "GW2RE/Game/Char/SpeciesDef.h"
#include "GW2RE/Game/Combat/SkillDef.h"
#include "GW2RE/Game/Combat/Tracker/CombatEvent.h"
#include "GW2RE/Game/Gadget/Gadget.h"
#include "GW2RE/Game/Gadget/GdAttackTarget.h"
#include "GW2RE/Game/Game/EventApi.h"
#include "GW2RE/Game/Map/MapDef.h"
#include "GW2RE/Game/MissionContext.h"
#include "GW2RE/Game/PropContext.h"
#include "GW2RE/Util/Hook.h"
#include "memtools/memtools.h"
#include "Util/src/Strings.h"
#include "Util/src/Time.h"

FUNC_HOOKCREATE  HookCreate  = nullptr;
FUNC_HOOKREMOVE  HookRemove  = nullptr;
FUNC_HOOKENABLE  HookEnable  = nullptr;
FUNC_HOOKDISABLE HookDisable = nullptr;

namespace Combat
{
	static AddonAPI_t*                               s_APIDefs           = nullptr;
	static std::time_t                               s_BootTime          = {};

	typedef uint64_t(__fastcall* FN_COMBATTRACKER)(GW2RE::CbtEvent_t*, uint32_t*);
	static GW2RE::Hook<FN_COMBATTRACKER>*            s_HookCombatTracker = nullptr;

	static bool                                      s_IsInCombat        = false;
	static Encounter_t                               s_CurrentEncounter  = {};

	/* Forward declare internal functions. */
	void ResolveAgentSpecies(GW2RE::Agent_t* aAgent);
	uint64_t __fastcall OnCombatEvent(GW2RE::CbtEvent_t*, uint32_t*);
	void __fastcall Advance(void*, void*);
}

void Combat::Create(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;

	/* Get system time offset for combat events. */
	s_BootTime = std::time(nullptr) - static_cast<std::time_t>(GetTickCount64() / 1000);

	/* Set auxillary functions for Hook struct. */
	HookCreate = (FUNC_HOOKCREATE)s_APIDefs->MinHook_Create;
	HookRemove = (FUNC_HOOKREMOVE)s_APIDefs->MinHook_Remove;
	HookEnable = (FUNC_HOOKENABLE)s_APIDefs->MinHook_Enable;
	HookDisable = (FUNC_HOOKDISABLE)s_APIDefs->MinHook_Disable;

	FN_COMBATTRACKER cbttracker = GW2RE::S_FnCombatTracker.Scan<FN_COMBATTRACKER>();

	if (!cbttracker)
	{
		s_APIDefs->Log(LOGL_CRITICAL, ADDON_NAME, "Combat tracker not registered.");
		return;
	}

	GW2RE::CEventApi::Register(GW2RE::EEngineEvent::EngineTick, Advance);

	s_HookCombatTracker = new GW2RE::Hook<FN_COMBATTRACKER>((FN_COMBATTRACKER)GW2RE::S_FnCombatTracker.Scan(), OnCombatEvent);
	s_HookCombatTracker->Enable();
}

void Combat::Destroy()
{
	if (!s_APIDefs) { return; }

	GW2RE::CEventApi::Deregister(GW2RE::EEngineEvent::EngineTick, Advance);

	if (s_HookCombatTracker) { GW2RE::DestroyHook(s_HookCombatTracker); }
}

bool Combat::IsRegistered()
{
	return s_HookCombatTracker != nullptr;
}

bool Combat::IsActive()
{
	return s_IsInCombat;
}

Encounter_t Combat::GetCurrentEncounter()
{
	return s_CurrentEncounter;
}

void Combat::ResolveAgentSpecies(GW2RE::Agent_t* aAgent)
{
	if (!aAgent)     { return; }
	if (!aAgent->ID) { return; }

	auto it = s_CurrentEncounter.AgentSpeciesLUT.find(aAgent->ID);

	if (it == s_CurrentEncounter.AgentSpeciesLUT.end())
	{
		GW2RE::CAgent ag = aAgent;
		switch (ag.GetType())
		{
			case GW2RE::EAgentType::Char:
			{
				GW2RE::CCharacter character = ag.GetCharacter();
				s_CurrentEncounter.AgentSpeciesLUT.emplace(aAgent->ID, character->SpeciesDef->ID);
				break;
			}
			case GW2RE::EAgentType::Gadget:
			{
				GW2RE::CGadget gadget = ag.GetGadget();
				s_CurrentEncounter.AgentSpeciesLUT.emplace(aAgent->ID, gadget.GetArcID());
				break;
			}
			case GW2RE::EAgentType::Gadget_Attack_Target:
			{
				GW2RE::CGadgetAttackTarget at = ag.GetGadgetAttackTarget();
				GW2RE::CGadget owner = at.GetOwner();
				s_CurrentEncounter.AgentSpeciesLUT.emplace(aAgent->ID, owner.GetArcID());
				break;
			}
		}
	}
}

uint64_t __fastcall Combat::OnCombatEvent(GW2RE::CbtEvent_t* aCombatEvent, uint32_t* a2)
{
	const std::lock_guard<std::mutex> lock(s_HookCombatTracker->Mutex);

	GW2RE::CCbtEv aCbtEv = aCombatEvent;

	GW2RE::CPropContext propctx = GW2RE::CPropContext::Get();
	GW2RE::MissionContext_t* missionctx = propctx.GetMissionCtx();

	/* If no active map, or active map is PvP, do not process. */
	if (!missionctx || (missionctx->CurrentMap && missionctx->CurrentMap->PvP))
	{
		return s_HookCombatTracker->OriginalFunction(aCombatEvent, a2);
	}

	/* Filter display events. */
	if (!aCbtEv || aCbtEv.IsDisplayedBuffDamage())
	{
		return s_HookCombatTracker->OriginalFunction(aCombatEvent, a2);
	}

	/* Filter out unwanted events. */
	ECombatEventType evType;
	switch (aCbtEv->EventType)
	{
		case GW2RE::ECbtEventType::Death:
		{
			evType = ECombatEventType::Death;
			break;
		}
		case GW2RE::ECbtEventType::Down:
		{
			evType = ECombatEventType::Down;
			break;
		}
		case GW2RE::ECbtEventType::HealthAdjustment:
		{
			evType = ECombatEventType::Health;
			break;
		}

		default:
		{
			/* Do not process other events. */
			return s_HookCombatTracker->OriginalFunction(aCombatEvent, a2);
		}
	}

	/* Allocate and assign new internal event type. */
	CombatEvent_t* ev     = new CombatEvent_t();
	ev->Type              = evType;

	ev->Time              = s_BootTime + (aCbtEv->SysTime / 1000);
	ev->Ms                = aCbtEv->SysTime % 1000;

	ev->SrcAgentID        = aCbtEv->SrcAgent ? aCbtEv->SrcAgent->ID : 0;
	ev->DstAgentID        = aCbtEv->DstAgent ? aCbtEv->DstAgent->ID : 0;
	ev->SkillID           = aCbtEv->SkillDef ? aCbtEv->SkillDef->ID : 0;

	ev->Value             = aCbtEv->Value;
	ev->ValueAlt          = aCbtEv->Value2;

	ev->IsConditionDamage = aCbtEv.IsConditionDamage();
	ev->IsCritical        = aCbtEv.IsCritical();
	ev->IsFumble          = aCbtEv.IsFumble();

	uint64_t now = Time::GetTimestampMs();

	/* Entered combat. */
	if (!s_IsInCombat || s_CurrentEncounter.TimeStart == 0)
	{
		s_IsInCombat = true;
		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Entered combat.");
		s_CurrentEncounter = {};
		s_CurrentEncounter.TimeStart = now;

		GW2RE::CPropContext propctx = GW2RE::CPropContext::Get();
		GW2RE::CCharCliContext cctx = propctx.GetCharCliCtx();
		GW2RE::CAgent agent = cctx.GetControlledAgent();
		s_CurrentEncounter.SelfID = agent.GetAgentId();
	}

	s_CurrentEncounter.TimeEnd = now;

	/* Store agent species. */
	ResolveAgentSpecies(aCbtEv->SrcAgent);
	ResolveAgentSpecies(aCbtEv->DstAgent);

	/* Store combat event. */
	s_CurrentEncounter.CombatEvents.push_back(ev);

	std::string srcName = TextCache::GetAgentName(aCbtEv->SrcAgent);
	std::string dstName = TextCache::GetAgentName(aCbtEv->DstAgent);
	std::string skillName = TextCache::GetSkillName(aCbtEv->SkillDef);

	s_APIDefs->Events_RaiseNotificationTargeted(ADDON_SIG, EV_PCM_COMBAT);
	
//#ifdef _DEBUG
	s_APIDefs->Log(
		LOGL_DEBUG,
		ADDON_NAME,
		String::Format(
			"<c=#00ff00>%s</c> hits <c=#ff0000>%s</c> using <c=#0000ff>%s</c> with %.0f (%.0f).",
			srcName.c_str(),
			dstName.c_str(),
			skillName.c_str(),
			ev->Value,
			ev->ValueAlt
		).c_str()
	);
//#endif

	return s_HookCombatTracker->OriginalFunction(aCombatEvent, a2);
}

void __fastcall Combat::Advance(void*, void*)
{
	GW2RE::CPropContext    propctx   = GW2RE::CPropContext::Get();
	GW2RE::CCharCliContext cctx      = propctx.GetCharCliCtx();
	GW2RE::CCharacter      character = cctx.GetOwnedCharacter();

	if (!character) { return; }

	bool isInCombat = (character->Flags & GW2RE::ECharacterFlags::IsInCombat) == GW2RE::ECharacterFlags::IsInCombat;

	if (!isInCombat && s_IsInCombat)
	{
		s_IsInCombat = false;
		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Left combat.");
		s_CurrentEncounter.AgentNameLUT = TextCache::GetAgentNames();
		s_APIDefs->Events_RaiseNotificationTargeted(ADDON_SIG, EV_PCM_COMBAT_END);
	}
}

#include "Combat.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>

#include "GW2RE/Game/Agent/Agent.h"
#include "GW2RE/Game/Agent/AgSelectionContext.h"
#include "GW2RE/Game/Char/Character.h"
#include "GW2RE/Game/Char/ChCliContext.h"
#include "GW2RE/Game/Combat/SkillDef.h"
#include "GW2RE/Game/Combat/Tracker/CombatEvent.h"
#include "GW2RE/Game/Game/EventApi.h"
#include "GW2RE/Game/PropContext.h"
#include "GW2RE/Util/Hook.h"
#include "GW2RE/Util/Validation.h"
#include "memtools/memtools.h"
#include "Util/src/Strings.h"

#include "Core/Addon.h"
#include "Core/TextCache/TextCache.h"

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

	static uint32_t                                  s_SelfID = 0;
	static std::vector<CombatEvent_t*>               s_Events;

	/* Forward declare internal functions. */
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

	std::string err = GW2RE::RunDiag();

	if (!err.empty())
	{
		s_APIDefs->Log(LOGL_CRITICAL, ADDON_NAME, err.c_str());
		return;
	}

	GW2RE::CEventApi::Register(GW2RE::EEvent::EngineTick, Advance);
	
	s_HookCombatTracker = new GW2RE::Hook<FN_COMBATTRACKER>((FN_COMBATTRACKER)GW2RE::S_FnCombatTracker.Scan(), OnCombatEvent);
	s_HookCombatTracker->Enable();
}

void Combat::Destroy()
{
	GW2RE::CEventApi::Deregister(GW2RE::EEvent::EngineTick, Advance);

	if (s_HookCombatTracker) { GW2RE::DestroyHook(s_HookCombatTracker); }
}

uint32_t Combat::GetSelfID()
{
	GW2RE::CPropContext propctx = GW2RE::CPropContext::Get();
	GW2RE::CCharCliContext cctx = propctx.GetCharCliCtx();
	GW2RE::CAgent agent = cctx.GetControlledAgent();
	return agent.GetAgentId();
}

uint32_t Combat::GetTargetID()
{
	GW2RE::CAgentSelectionContext agselctx = GW2RE::CAgentSelectionContext::Get();
	GW2RE::CAgent agent = agselctx.GetTarget();
	return agent.GetAgentId();
}

std::vector<CombatEvent_t*> Combat::GetCombatEvents()
{
	return s_Events;
}

uint64_t __fastcall Combat::OnCombatEvent(GW2RE::CbtEvent_t* aCombatEvent, uint32_t* a2)
{
	const std::lock_guard<std::mutex> lock(s_HookCombatTracker->Mutex);

	GW2RE::CCbtEv aCbtEv = aCombatEvent;

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

	/* Store combat event. */
	s_Events.push_back(ev);

	/* Kinda hacky way to ensure s_GameThread is set. */
	TextCache::Advance();

	std::string srcName = TextCache::GetAgentName(aCbtEv->SrcAgent);
	std::string dstName = TextCache::GetAgentName(aCbtEv->DstAgent);
	std::string skillName = TextCache::GetSkillName(aCbtEv->SkillDef);

	TextCache::Advance();

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

	return s_HookCombatTracker->OriginalFunction(aCombatEvent, a2);
}

void __fastcall Combat::Advance(void*, void*)
{
	GW2RE::CPropContext    propctx   = GW2RE::CPropContext::Get();
	GW2RE::CCharCliContext cctx      = propctx.GetCharCliCtx();
	GW2RE::CCharacter      character = cctx.GetOwnedCharacter();

	bool isInCombat = (character->Flags & GW2RE::ECharacterFlags::IsInCombat) == GW2RE::ECharacterFlags::IsInCombat;

	static bool s_IsInCombat = false;

	if (isInCombat != s_IsInCombat)
	{
		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, isInCombat ? "Entered combat." : "Left combat.");
		s_IsInCombat = isInCombat;
	}
}

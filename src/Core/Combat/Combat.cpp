#include "Combat.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>

#include "GW2RE/Game/Agent/Agent.h"
#include "GW2RE/Game/Char/Character.h"
#include "GW2RE/Game/Char/ChCliContext.h"
#include "GW2RE/Game/Char/ChKennel.h"
#include "GW2RE/Game/Char/SpeciesDef.h"
#include "GW2RE/Game/Combat/SkillDef.h"
#include "GW2RE/Game/Combat/Tracker/CombatEvent.h"
#include "GW2RE/Game/Gadget/Gadget.h"
#include "GW2RE/Game/Gadget/GdAttackTarget.h"
#include "GW2RE/Game/Game/EventApi.h"
#include "GW2RE/Game/Map/MapDef.h"
#include "GW2RE/Game/MissionContext.h"
#include "GW2RE/Game/Player/Player.h"
#include "GW2RE/Game/PropContext.h"
#include "GW2RE/Game/Text/TextApi.h"
#include "GW2RE/Util/Hook.h"
#include "memtools/memtools.h"

#include "CbtEncounter.h"
#include "Core/Addon.h"
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
	static GW2RE::CAgent                             s_SelfAgent         = nullptr;
	static Encounter_t                               s_CurrentEncounter  = {};

	/* Forward declare internal functions. */
	Agent_t* TrackAgent(GW2RE::Agent_t* aAgent);
	Skill_t* TrackSkill(GW2RE::SkillDef_t* aSkill);
	uint64_t __fastcall OnCombatEvent(GW2RE::CbtEvent_t*, uint32_t*);
	void __fastcall Advance(void*, void*);

	/* Text API */
	typedef GW2RE::CodedText(__fastcall* FN_RESOLVEHASH)(GW2RE::TextHash, ...);
	static FN_RESOLVEHASH                            s_ResolveHash = nullptr;

	typedef void(__fastcall* FN_DECODETEXT)(GW2RE::CodedText, GW2RE::FN_RECEIVETEXT, void*);
	static FN_DECODETEXT                             s_DecodeText = nullptr;

	void __fastcall ReceiveText(void*, const wchar_t*);
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

	s_ResolveHash = GW2RE::S_ResolveTextHash.Scan<FN_RESOLVEHASH>();
	s_DecodeText = GW2RE::S_DecodeText.Scan<FN_DECODETEXT>();

	FN_COMBATTRACKER cbttracker = GW2RE::S_FnCombatTracker.Scan<FN_COMBATTRACKER>();

	if (!cbttracker)
	{
		cbttracker = GW2RE::S_FnCombatTracker_Callsite.Scan<FN_COMBATTRACKER>();

		if (!cbttracker)
		{
			s_APIDefs->Log(LOGL_CRITICAL, ADDON_NAME, "Combat tracker not registered.");
			return;
		}
		else
		{
			s_APIDefs->Log(LOGL_INFO, ADDON_NAME, "Combat tracker registered through secondary entry point.");
		}
	}

	GW2RE::CEventApi::Register(GW2RE::EEngineEvent::EngineTick, Advance);

	s_HookCombatTracker = new GW2RE::Hook<FN_COMBATTRACKER>(cbttracker, OnCombatEvent);
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

Agent_t* Combat::TrackAgent(GW2RE::Agent_t* aAgent)
{
	if (!aAgent)     { return nullptr; }
	if (!aAgent->ID) { return nullptr; }

	auto it = s_CurrentEncounter.Agents.find(aAgent->ID);

	if (it != s_CurrentEncounter.Agents.end()) { return it->second; }
	
	it = s_CurrentEncounter.Agents.emplace(aAgent->ID, new Agent_t()).first;

	it->second->ID = aAgent->ID;

	GW2RE::CAgent    ag        = aAgent;
	GW2RE::CodedText codedName = nullptr;

	switch (ag.GetType())
	{
		case GW2RE::EAgentType::Char:
		{
			it->second->Type = EAgentType::Character;

			GW2RE::CCharacter character = ag.GetCharacter();
			it->second->SpeciesID = character->SpeciesDef->ID;

			if (character.IsPlayer())
			{
				GW2RE::CPlayer player = character.GetPlayer();

				/* No need for decoding, can grab raw text. */
				strcpy_s(it->second->Name, sizeof(it->second->Name), String::ToString(player.GetName()).c_str());
			}
			else
			{
				GW2RE::CCharacter master = character.GetMaster();
				if (master)
				{
					/* Recursive track master agent. */
					TrackAgent(master.GetAgent());

					it->second->IsMinion = true;
					it->second->OwnerID = master.GetAgentId();
				}
			}

			codedName = character.GetCodedName();
			break;
		}
		case GW2RE::EAgentType::Gadget:
		{
			it->second->Type = EAgentType::Gadget;

			GW2RE::CGadget gadget = ag.GetGadget();
			it->second->SpeciesID = gadget.GetArcID();

			it->second->IsMinion = gadget->Flags & 1;
			it->second->OwnerID = s_CurrentEncounter.SelfID;

			codedName = gadget.GetCodedName();
			break;
		}
		case GW2RE::EAgentType::Gadget_Attack_Target:
		{
			it->second->Type = EAgentType::AttackTarget;

			GW2RE::CGadgetAttackTarget at = ag.GetGadgetAttackTarget();
			GW2RE::CGadget owner = at.GetOwner();
			it->second->SpeciesID = owner.GetArcID();

			codedName = owner.GetCodedName();
			break;
		}
	}

	if (codedName)
	{
		s_DecodeText(codedName, ReceiveText, &it->second->Name[0]);
	}

	return it->second;
}

Skill_t* Combat::TrackSkill(GW2RE::SkillDef_t* aSkill)
{
	if (!aSkill)       { return nullptr; }
	if (!aSkill->ID)   { return nullptr; }
	if (!aSkill->Name) { return nullptr; }

	auto it = s_CurrentEncounter.Skills.find(aSkill->ID);

	if (it != s_CurrentEncounter.Skills.end()) { return it->second; }

	it = s_CurrentEncounter.Skills.emplace(aSkill->ID, new Skill_t()).first;

	it->second->ID = aSkill->ID;

	GW2RE::CodedText codedText = s_ResolveHash(aSkill->Name, GW2RE::ETextOperation::Terminate);
	s_DecodeText(codedText, ReceiveText, &it->second->Name);

	return it->second;
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

	ev->SrcAgent          = TrackAgent(aCbtEv->SrcAgent);
	ev->DstAgent          = TrackAgent(aCbtEv->DstAgent);
	ev->Skill             = TrackSkill(aCbtEv->SkillDef);

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
		s_SelfAgent = agent;
		s_CurrentEncounter.SelfID = agent.GetAgentId();
	}

	s_CurrentEncounter.TimeEnd = now;

	/* Store combat event. */
	s_CurrentEncounter.CombatEvents.push_back(ev);

	s_APIDefs->Events_RaiseNotification(EV_CMX_COMBAT);
	
	s_APIDefs->Log(
		LOGL_DEBUG,
		ADDON_NAME,
		String::Format(
			"[EV:%u] <c=#00ff00>%s</c> (%u) hits <c=#ff0000>%s</c> (%u) using <c=#0000ff>%s</c> (%u) with %.0f (%.0f).",
			ev->Type,
			ev->SrcAgent ? ev->SrcAgent->GetName().c_str() : "(null)",
			ev->SrcAgent ? ev->SrcAgent->ID : 0,
			ev->DstAgent ? ev->DstAgent->GetName().c_str() : "(null)",
			ev->DstAgent ? ev->DstAgent->ID : 0,
			ev->Skill ? ev->Skill->GetName().c_str() : "(null)",
			ev->Skill ? ev->Skill->ID : 0,
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

	if (!character) { return; }

	bool isInCombat = (character->Flags & GW2RE::ECharacterFlags::IsInCombat) == GW2RE::ECharacterFlags::IsInCombat;

	if (!isInCombat && s_IsInCombat)
	{
		s_IsInCombat = false;
		s_SelfAgent = nullptr;
		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Left combat.");
		s_APIDefs->Events_RaiseNotificationTargeted(ADDON_SIG, EV_CMX_COMBAT_END);
	}

	/*static uint32_t s_LastMapID = 0;

	GW2RE::MissionContext_t* missionctx = propctx.GetMissionCtx();

	if (missionctx && missionctx->CurrentMapID != s_LastMapID)
	{
		s_LastMapID = missionctx->CurrentMapID;

		s_IsInCombat = false;
		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Left combat.");
		s_APIDefs->Events_RaiseNotificationTargeted(ADDON_SIG, EV_CMX_COMBAT_END);
	}*/
}

void __fastcall Combat::ReceiveText(void* aPtr, const wchar_t* aWString)
{
	if (!aPtr) { return; }

	char* outStr = (char*)aPtr;

	strcpy_s(outStr, 128, String::ToString(aWString).c_str());
}

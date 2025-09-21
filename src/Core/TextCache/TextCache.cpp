#include "TextCache.h"

#include <mutex>

#include "GW2RE/Game/Agent/Agent.h"
#include "GW2RE/Game/Char/Character.h"
#include "GW2RE/Game/Gadget/Gadget.h"
#include "GW2RE/Game/Gadget/GdAttackTarget.h"
#include "GW2RE/Game/Game/EventApi.h"
#include "GW2RE/Game/MissionContext.h"
#include "GW2RE/Game/Player/Player.h"
#include "GW2RE/Game/PropContext.h"
#include "GW2RE/Game/Text/TextApi.h"
#include "Util/src/Strings.h"

namespace TextCache
{
	static AddonAPI_t*                               s_APIDefs    = nullptr;
	static uint32_t                                  s_GameThread = 0;

	void __fastcall Advance(void*, void*);

	typedef GW2RE::CodedText (__fastcall*FN_RESOLVEHASH)(GW2RE::TextHash, ...);
	static FN_RESOLVEHASH                            s_ResolveHash = nullptr;

	typedef void (__fastcall*FN_DECODETEXT)(GW2RE::CodedText, GW2RE::FN_RECEIVETEXT, void*);
	static FN_DECODETEXT                             s_DecodeText = nullptr;

	void __fastcall ReceiveText(void*, const wchar_t*);

	static std::mutex                                s_Mutex;
	static std::unordered_map<uint32_t, std::string> s_AgentNameLUT;
	static std::unordered_map<uint32_t, std::string> s_SkillLUT;
}

void TextCache::Create(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;

	s_ResolveHash = GW2RE::S_ResolveTextHash.Scan<FN_RESOLVEHASH>();
	s_DecodeText = GW2RE::S_DecodeText.Scan<FN_DECODETEXT>();

	GW2RE::CEventApi::Register(GW2RE::EEvent::EngineTick, Advance);
}

void TextCache::Destroy()
{
	GW2RE::CEventApi::Deregister(GW2RE::EEvent::EngineTick, Advance);
}

void TextCache::Advance(void*, void*)
{
	static uint32_t s_LastMapID = 0;

	if (s_GameThread == 0)
	{
		s_GameThread = GetCurrentThreadId();
	}

	GW2RE::CPropContext propctx = GW2RE::CPropContext::Get();
	GW2RE::MissionContext_t* missionctx = propctx.GetMissionCtx();

	if (missionctx && missionctx->CurrentMapID != s_LastMapID)
	{
		s_LastMapID = missionctx->CurrentMapID;

		FlushAgentNames();
	}
}

std::unordered_map<uint32_t, std::string> TextCache::GetAgentNames()
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	return s_AgentNameLUT;
}

void TextCache::FlushAgentNames()
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	s_AgentNameLUT.clear();
}

void __fastcall TextCache::ReceiveText(void* aPtr, const wchar_t* aWString)
{
	std::string* outStr = (std::string*)aPtr;
	outStr->assign(String::ToString(aWString));
}

std::string TextCache::GetAgentName(uint32_t aID)
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	auto it = s_AgentNameLUT.find(aID);

	if (it != s_AgentNameLUT.end())
	{
		return it->second;
	}
	
	return String::Format("((%s-%u))", "ag", aID);
}

std::string TextCache::GetAgentName(GW2RE::CAgent aAgent)
{
	std::string result = "(null)";

	if (!aAgent) { return result; }

	const std::lock_guard<std::mutex> lock(s_Mutex);

	auto it = s_AgentNameLUT.find(aAgent.GetAgentId());

	if (it != s_AgentNameLUT.end())
	{
		return it->second;
	}
	else
	{
		std::string prefix;
		switch (aAgent.GetType())
		{
			default:                                      { prefix = "ag"; break; }
			case GW2RE::EAgentType::Char:                 { prefix = "ch"; break; }
			case GW2RE::EAgentType::Gadget:               { prefix = "gd"; break; }
			case GW2RE::EAgentType::Gadget_Attack_Target: { prefix = "at"; break; }
		}
		result = String::Format("((%s-%u))", prefix.c_str(), aAgent.GetAgentId());
		s_AgentNameLUT.emplace(aAgent.GetAgentId(), result);
	}

	if (s_GameThread == GetCurrentThreadId())
	{
		GW2RE::CodedText codedName = nullptr;
		auto it = s_AgentNameLUT.find(aAgent.GetAgentId());

		switch (aAgent.GetType())
		{
			case GW2RE::EAgentType::Char:
			{
				GW2RE::CCharacter character = aAgent.GetCharacter();

				if (character.IsPlayer())
				{
					GW2RE::CPlayer player = character.GetPlayer();

					/* No need for decoding, can grab raw text. */
					it->second = String::ToString(player.GetName());
					return it->second;
				}

				codedName = character.GetCodedName();
				break;
			}
			case GW2RE::EAgentType::Gadget:
			{
				GW2RE::CGadget gadget = aAgent.GetGadget();

				codedName = gadget.GetCodedName();
				break;
			}
			case GW2RE::EAgentType::Gadget_Attack_Target:
			{
				GW2RE::CGadgetAttackTarget attacktarget = aAgent.GetGadgetAttackTarget();
				GW2RE::CGadget owner = attacktarget.GetOwner();

				codedName = owner.GetCodedName();
				break;
			}
		}

		s_DecodeText(codedName, TextCache::ReceiveText, &it->second);
	}

	return result;
}

std::string TextCache::GetSkillName(uint32_t aID)
{
	const std::lock_guard<std::mutex> lock(s_Mutex);

	auto it = s_SkillLUT.find(aID);

	if (it != s_SkillLUT.end())
	{
		return it->second;
	}

	return String::Format("((sk-%u))", aID);
}

std::string TextCache::GetSkillName(GW2RE::SkillDef_t* aSkill)
{
	std::string result = "(null)";

	if (!aSkill) { return result; }

	const std::lock_guard<std::mutex> lock(s_Mutex);

	auto it = s_SkillLUT.find(aSkill->ID);

	if (it != s_SkillLUT.end())
	{
		return it->second;
	}
	else
	{
		result = String::Format("((sk-%u))", aSkill->ID);
		s_SkillLUT.emplace(aSkill->ID, result);
	}

	if (s_GameThread == GetCurrentThreadId())
	{
		GW2RE::CodedText codedText = s_ResolveHash(aSkill->Name, 0);
		auto it = s_SkillLUT.find(aSkill->ID);
		s_DecodeText(codedText, TextCache::ReceiveText, &it->second);
	}

	return result;
}

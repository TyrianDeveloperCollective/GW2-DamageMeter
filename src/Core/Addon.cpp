#include "Addon.h"

#include "Remote.h"
#include "Version.h"

#include "Combat/Combat.h"
#include "GW2RE/Util/Validation.h"
#include "UI/UiRoot.h"

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
	static AddonDefinition_t s_AddonDef;

	s_AddonDef.Signature        = ADDON_SIG;
	s_AddonDef.APIVersion       = NEXUS_API_VERSION;
	s_AddonDef.Name             = ADDON_NAME;
	s_AddonDef.Version.Major    = V_MAJOR;
	s_AddonDef.Version.Minor    = V_MINOR;
	s_AddonDef.Version.Build    = V_BUILD;
	s_AddonDef.Version.Revision = V_REVISION;
	s_AddonDef.Author           = "Tyrian Developer Collective";
	s_AddonDef.Description      = "Personal DPS meter and combat metrics tool.";
	s_AddonDef.Load             = Addon::Load;
	s_AddonDef.Unload           = Addon::Unload;
	s_AddonDef.Flags            = AF_None;

	s_AddonDef.Provider         = UP_GitHub;
	s_AddonDef.UpdateLink       = REMOTE_URL;

	return &s_AddonDef;
}

namespace Addon
{
	static AddonAPI_t* s_APIDefs = nullptr;
}

void Addon::Load(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;

	std::string err = GW2RE::RunDiag();

	if (!err.empty())
	{
		s_APIDefs->Log(LOGL_CRITICAL, ADDON_NAME, "Cancelled load.");
		s_APIDefs->Log(LOGL_CRITICAL, ADDON_NAME, err.c_str());
		return;
	}

	Combat::Create(aApi);
	UiRoot::Create(aApi);
}

void Addon::Unload()
{
	Combat::Destroy();
	UiRoot::Destroy();
}

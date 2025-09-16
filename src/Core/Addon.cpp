#include "Addon.h"

#include "imgui/imgui.h"

#include "Remote.h"
#include "UI/UiRoot.h"
#include "Version.h"

#define ADDON_NAME "Damage Meter"

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
	static AddonDefinition_t s_AddonDef;

	s_AddonDef.Signature        = 0xCCCCCCCC;
	s_AddonDef.APIVersion       = NEXUS_API_VERSION;
	s_AddonDef.Name             = ADDON_NAME;
	s_AddonDef.Version.Major    = V_MAJOR;
	s_AddonDef.Version.Minor    = V_MINOR;
	s_AddonDef.Version.Build    = V_BUILD;
	s_AddonDef.Version.Revision = V_REVISION;
	s_AddonDef.Author           = "Tyrian Developer Collective";
	s_AddonDef.Description      = "Damage Meter and Combat Metrics.";
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

	void Load(AddonAPI_t* aApi)
	{
		s_APIDefs = aApi;

		UiRoot::Create(aApi);

		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Loaded.");
	}

	void Unload()
	{
		UiRoot::Destroy();

		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Unloaded.");
	}
}

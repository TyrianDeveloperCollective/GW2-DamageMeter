#include "Addon.h"
#include "Remote.h"
#include "Version.h"

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
	s_AddonDef.Load             = UiRoot::Create;
	s_AddonDef.Unload           = UiRoot::Destroy;
	s_AddonDef.Flags            = AF_None;

	s_AddonDef.Provider         = UP_GitHub;
	s_AddonDef.UpdateLink       = REMOTE_URL;

	return &s_AddonDef;
}

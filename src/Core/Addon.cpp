// dllmain.cpp : Defines the entry point for the DLL application.

#include "Addon.h"

#include "Remote.h"
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

	/* not necessary if hosted on Raidcore, but shown anyway for the example also useful as a backup resource */
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

		ImGui::SetCurrentContext((ImGuiContext*)s_APIDefs->ImguiContext);
		ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))s_APIDefs->ImguiMalloc, (void(*)(void*, void*))s_APIDefs->ImguiFree);

		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Loaded.");
	}

	void Unload()
	{
		s_APIDefs->Log(LOGL_DEBUG, ADDON_NAME, "Unloaded.");
	}
}

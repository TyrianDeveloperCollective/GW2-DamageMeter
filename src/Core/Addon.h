#pragma once

#include "Nexus/Nexus.h"

#define ADDON_SIG   0x4D455452
#define ADDON_NAME "Personal Combat Metrics"

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef();

namespace Addon
{
	void Load(AddonAPI_t* aApi);

	void Unload();
}

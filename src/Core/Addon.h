#pragma once

#include "Nexus/Nexus.h"

#define ADDON_SIG   0xCCCCCCCC
#define ADDON_NAME "Damage Meter"

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef();

namespace Addon
{
	void Load(AddonAPI_t* aApi);

	void Unload();
}

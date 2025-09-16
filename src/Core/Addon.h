#pragma once

#include "Nexus/Nexus.h"

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef();

namespace Addon
{
	void Load(AddonAPI_t* aApi);

	void Unload();
}

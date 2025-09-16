#pragma once

#include <windows.h>

#include "Nexus/Nexus.h"
#include "imgui/imgui.h"

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef();

namespace Addon
{
	void Load(AddonAPI_t* aApi);

	void Unload();
}

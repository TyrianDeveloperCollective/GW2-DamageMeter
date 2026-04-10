#pragma once

#include "Nexus/Nexus.h"

#define ADDON_SIG   0x434D5821
#define ADDON_NAME "Personal Combat Metrics"

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef();

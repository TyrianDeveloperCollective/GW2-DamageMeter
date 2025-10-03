#pragma once

#include "Nexus/Nexus.h"

namespace UiRoot
{
	void Create(AddonAPI_t* aApi);

	void Destroy();

	void Render();

	void Options();

	void OnCombatEnd();
}

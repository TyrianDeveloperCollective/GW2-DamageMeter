#include "Combat.h"

namespace Combat
{
	static AddonAPI_t* s_APIDefs = nullptr;
}

void Combat::Create(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;
}

void Combat::Destroy()
{

}

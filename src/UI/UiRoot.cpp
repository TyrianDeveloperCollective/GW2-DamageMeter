#include "UiRoot.h"

#include "imgui/imgui.h"

namespace UiRoot
{
	static AddonAPI_t* s_APIDefs = nullptr;
}

void UiRoot::Create(AddonAPI_t* aApi)
{
	s_APIDefs = aApi;

	ImGui::SetCurrentContext((ImGuiContext*)s_APIDefs->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))s_APIDefs->ImguiMalloc, (void(*)(void*, void*))s_APIDefs->ImguiFree);

	s_APIDefs->GUI_Register(RT_Render, UiRoot::Render);
	s_APIDefs->GUI_Register(RT_OptionsRender, UiRoot::Options);
}

void UiRoot::Destroy()
{
	s_APIDefs->GUI_Deregister(UiRoot::Render);
	s_APIDefs->GUI_Deregister(UiRoot::Options);
}

void UiRoot::Render()
{

}

void UiRoot::Options()
{

}

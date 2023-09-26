#pragma once
#include "imgui.h"

struct GLFWwindow;

// Backend API
IMGUI_IMPL_API bool     ImGui_ImplMBGL_Init();
IMGUI_IMPL_API void     ImGui_ImplMBGL_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplMBGL_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplMBGL_RenderDrawData(ImDrawData* draw_data);

// (Optional) Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool     ImGui_ImplMBGL_CreateFontsTexture();
//IMGUI_IMPL_API void     ImGui_ImplMBGL_DestroyFontsTexture();
IMGUI_IMPL_API bool     ImGui_ImplMBGL_CreateDeviceObjects();
//IMGUI_IMPL_API void     ImGui_ImplMBGL_DestroyDeviceObjects();

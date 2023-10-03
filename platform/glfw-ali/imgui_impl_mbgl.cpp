#include "imgui_impl_mbgl.h"
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/gl/defines.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/gl/vertex_array_extension.hpp>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/gl/defines.hpp>

using namespace mbgl::platform;

bool ImGui_ImplMBGL_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    // Setup backend capabilities flags
    io.BackendPlatformName = "imgui_impl_mbgl";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    return true;
}

void ImGui_ImplMBGL_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    int w, h;
    int display_w, display_h;
    w = 1024;
    h = 768;
    display_w = 1024;
    display_h = 768;
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)display_w / (float)w, (float)display_h / (float)h);

    io.DeltaTime = 0.001;
}


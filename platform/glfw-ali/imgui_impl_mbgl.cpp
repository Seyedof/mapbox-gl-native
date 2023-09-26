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
//#include <GLFW/glfw3.h>

using namespace mbgl::platform;

struct ImGui_ImplMBGL_Data
{
    GLuint          FontTexture;
    //GLuint          ShaderHandle;
    //GLint           AttribLocationTex;       // Uniforms location
    //GLint           AttribLocationProjMtx;
    //GLuint          AttribLocationVtxPos;    // Vertex attributes location
    //GLuint          AttribLocationVtxUV;
    //GLuint          AttribLocationVtxColor;
    unsigned int    VboHandle, ElementsHandle;
    GLsizeiptr      VertexBufferSize;
    GLsizeiptr      IndexBufferSize;
    bool            HasClipOrigin;
    bool            UseBufferSubData;

    //GLFWwindow*     Window;
    //float           Time = 0.0f;
    //GLFWcursor*             MouseCursors[ImGuiMouseCursor_COUNT];

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    //GLuint buffer = 0;
    GLuint a_pos = 0;
    GLuint a_uv = 0;
    GLuint a_color = 0;

    GLuint u_texture = 0;
    GLuint u_projMatrix = 0;

    ImGui_ImplMBGL_Data() { memset((void*)this, 0, sizeof(*this)); }
};

static ImGui_ImplMBGL_Data* ImGui_ImplMBGL_GetBackendData()
{
    if (!ImGui::GetCurrentContext()) {
        std::cout << "hello2" << std::endl;
    }
    return ImGui::GetCurrentContext() ? (ImGui_ImplMBGL_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

bool ImGui_ImplMBGL_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    // Setup backend capabilities flags
    ImGui_ImplMBGL_Data* bd = IM_NEW(ImGui_ImplMBGL_Data)();
    std::cout << "hello" << std::endl;
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_glfw";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

    //bd->Window = window;
    //bd->Time = 0.0;
//
    //io.SetClipboardTextFn = ImGui_ImplGlfw_SetClipboardText;
    //io.GetClipboardTextFn = ImGui_ImplGlfw_GetClipboardText;
    //io.ClipboardUserData = bd->Window;

    // GLFWerrorfun prev_error_callback = glfwSetErrorCallback(nullptr);
    // glfwSetErrorCallback(prev_error_callback);

    // Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
    // if (install_callbacks)
    //     ImGui_ImplGlfw_InstallCallbacks(window);

    //bd->ClientApi = client_api;

    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    io.BackendRendererUserData = (void*)bd;

    bd->UseBufferSubData = false;

    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    GLint current_texture;
    mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

    ImGui_ImplMBGL_CreateDeviceObjects();

    return true;
}

void ImGui_ImplMBGL_Shutdown()
{
/*    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    IM_DELETE(bd);*/
}

void ImGui_ImplMBGL_NewFrame()
{
    ImGui_ImplMBGL_Data* bd = ImGui_ImplMBGL_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplOpenGL3_Init()?");

    //if (!bd->program)
        //return;//ImGui_ImplMBGL_CreateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();
    //ImGui_ImplMBGL_Data* bd = ImGui_ImplMBGL_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplGlfw_InitForXXX()?");

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    // glfwGetWindowSize(bd->Window, &w, &h);
    // glfwGetFramebufferSize(bd->Window, &display_w, &display_h);
    w = 1024;
    h = 768;
    display_w = 1024;
    display_h = 768;
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)display_w / (float)w, (float)display_h / (float)h);

    // Setup time step
    // (Accept glfwGetTime() not returning a monotonically increasing value. Seems to happens on disconnecting peripherals and probably on VMs and Emscripten, see #6491, #6189, #6114, #3644)
    // double current_time = glfwGetTime();
    // if (current_time <= bd->Time)
    //     current_time = bd->Time + 0.00001f;
    // io.DeltaTime = bd->Time > 0.0 ? (float)(current_time - bd->Time) : (float)(1.0f / 60.0f);
    //bd->Time = current_time;
    io.DeltaTime = 0.001;

    //ImGui_ImplGlfw_UpdateMouseData();
    //ImGui_ImplGlfw_UpdateMouseCursor();
    if (!bd->program)
        return;//ImGui_ImplMBGL_CreateDeviceObjects();
}

bool ImGui_ImplMBGL_CreateDeviceObjects()
{
    ImGui_ImplMBGL_Data* bd = ImGui_ImplMBGL_GetBackendData();

    // Backup GL state
    GLint last_texture, last_array_buffer;
    mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    mbgl::platform::glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);

    std::cout << "shit3" << std::endl;

/*static const GLchar* vertexShaderSource = R"MBGL_SHADER(
attribute vec2 a_pos;
void main() {
     gl_Position = vec4(a_pos, 1, 1);
}
)MBGL_SHADER";*/

     static const GLchar* vertexShaderSource = R"MBGL_SHADER(
        attribute vec2 a_pos;
        attribute vec2 a_uv;
        attribute vec4 a_color;
        uniform mat4 ProjMtx;
        varying vec2 Frag_UV;
        varying vec4 Frag_Color;
        void main() {
             //gl_Position = vec4(a_pos, 1, 1);
             gl_Position = ProjMtx * vec4(a_pos.xy,1,1);
             Frag_UV = a_uv;
             Frag_Color = a_color;
         }
     )MBGL_SHADER";

    static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
        uniform sampler2D u_Texture;
        varying vec2 Frag_UV;
        varying vec4 Frag_Color;
        void main()
        {
            gl_FragColor = Frag_Color * texture2D(u_Texture, Frag_UV);
            //gl_FragColor = vec4(1,1,1,1);
        }
    )MBGL_SHADER";

    bd->program = MBGL_CHECK_ERROR(mbgl::platform::glCreateProgram());
    if (bd->program == 0) {
        std::cout << "shit4" << std::endl;
    }
    bd->vertexShader = MBGL_CHECK_ERROR(mbgl::platform::glCreateShader(GL_VERTEX_SHADER));
    bd->fragmentShader = MBGL_CHECK_ERROR(mbgl::platform::glCreateShader(GL_FRAGMENT_SHADER));

    MBGL_CHECK_ERROR(mbgl::platform::glShaderSource(bd->vertexShader, 1, &vertexShaderSource, nullptr));
    MBGL_CHECK_ERROR(mbgl::platform::glCompileShader(bd->vertexShader));
    MBGL_CHECK_ERROR(mbgl::platform::glAttachShader(bd->program, bd->vertexShader));
    MBGL_CHECK_ERROR(mbgl::platform::glShaderSource(bd->fragmentShader, 1, &fragmentShaderSource, nullptr));
    MBGL_CHECK_ERROR(mbgl::platform::glCompileShader(bd->fragmentShader));
    if (bd->fragmentShader == 0) {
        std::cout << "shit5" << std::endl;
    }
    MBGL_CHECK_ERROR(mbgl::platform::glAttachShader(bd->program, bd->fragmentShader));
    MBGL_CHECK_ERROR(mbgl::platform::glLinkProgram(bd->program));
    bd->a_pos = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(bd->program, "a_pos"));
    bd->a_uv = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(bd->program, "a_uv"));
    bd->a_color = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(bd->program, "a_color"));

    bd->u_texture = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(bd->program, "u_Texture"));
    bd->u_projMatrix = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(bd->program, "ProjMtx"));
    
    // Create buffers
    glGenBuffers(1, &bd->VboHandle);
    glGenBuffers(1, &bd->ElementsHandle);

    ImGui_ImplMBGL_CreateFontsTexture();

    // Restore modified GL state
    MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));
    MBGL_CHECK_ERROR(mbgl::platform::glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer));

    return true;
}

bool ImGui_ImplMBGL_CreateFontsTexture()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplMBGL_Data* bd = ImGui_ImplMBGL_GetBackendData();

    // Build texture atlas
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

    // Upload texture to graphics system
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    GLint last_texture;
    MBGL_CHECK_ERROR(mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture));
    MBGL_CHECK_ERROR(mbgl::platform::glGenTextures(1, &bd->FontTexture));
    MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, bd->FontTexture));
    MBGL_CHECK_ERROR(mbgl::platform::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    MBGL_CHECK_ERROR(mbgl::platform::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    MBGL_CHECK_ERROR(mbgl::platform::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)(intptr_t)bd->FontTexture);

    // Restore state
    MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));

    return true;
}

static void ImGui_ImplMBGL_SetupRenderState(ImDrawData* draw_data, int fb_width, int fb_height/*, GLuint vertex_array_object*/)
{
    ImGui_ImplMBGL_Data* bd = ImGui_ImplMBGL_GetBackendData();

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    mbgl::platform::glEnable(GL_BLEND);
    mbgl::platform::glBlendEquation(GL_FUNC_ADD);
    mbgl::platform::glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    mbgl::platform::glDisable(GL_CULL_FACE);
    mbgl::platform::glDisable(GL_DEPTH_TEST);
    mbgl::platform::glDisable(GL_STENCIL_TEST);
    mbgl::platform::glEnable(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    if (bd->GlVersion >= 310)
        mbgl::platform::glDisable(GL_PRIMITIVE_RESTART);
#endif
#ifdef IMGUI_IMPL_HAS_POLYGON_MODE
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    MBGL_CHECK_ERROR(mbgl::platform::glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height));
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
    };
    glUseProgram(bd->program);
    glUniform1i(bd->u_texture, 0);
    glUniformMatrix4fv(bd->u_projMatrix, 1, GL_FALSE, &ortho_projection[0][0]);

#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
        glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 and GL ES 3.0 may set that otherwise.
#endif

    // Bind vertex/index buffers and setup attributes for ImDrawVert
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, bd->VboHandle));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bd->ElementsHandle));
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(bd->a_pos));
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(bd->a_uv));
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(bd->a_color));
    MBGL_CHECK_ERROR(glVertexAttribPointer(bd->a_pos,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos)));
    MBGL_CHECK_ERROR(glVertexAttribPointer(bd->a_uv,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv)));
    MBGL_CHECK_ERROR(glVertexAttribPointer(bd->a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col)));
}

void ImGui_ImplMBGL_RenderDrawData(ImDrawData* draw_data)
{
    if (!draw_data) {
        return;
    }
    std::cout << "shit6" << std::endl;

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;


    ImGui_ImplMBGL_Data* bd = ImGui_ImplMBGL_GetBackendData();

    // Backup GL state
    GLenum last_active_texture; mbgl::platform::glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
    mbgl::platform::glActiveTexture(GL_TEXTURE0);
    GLuint last_program; mbgl::platform::glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&last_program);
    GLuint last_texture; mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    GLuint last_sampler; if (bd->GlVersion >= 330 || bd->GlProfileIsES3) { mbgl::platform::glGetIntegerv(GL_SAMPLER_BINDING, (GLint*)&last_sampler); } else { last_sampler = 0; }
#endif
    GLuint last_array_buffer; mbgl::platform::glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint*)&last_array_buffer);
#ifndef IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
    // This is part of VAO on OpenGL 3.0+ and OpenGL ES 3.0+.
    GLint last_element_array_buffer; mbgl::platform::glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
    // ImGui_ImplOpenGL3_VtxAttribState last_vtx_attrib_state_pos; last_vtx_attrib_state_pos.GetState(bd->a_pos);
    // ImGui_ImplOpenGL3_VtxAttribState last_vtx_attrib_state_uv; last_vtx_attrib_state_uv.GetState(bd->a_uv);
    // ImGui_ImplOpenGL3_VtxAttribState last_vtx_attrib_state_color; last_vtx_attrib_state_color.GetState(bd->a_color);
#endif
#ifdef IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
    GLuint last_vertex_array_object; mbgl::platform::glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint*)&last_vertex_array_object);
#endif
#ifdef IMGUI_IMPL_HAS_POLYGON_MODE
    GLint last_polygon_mode[2]; mbgl::platform::glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
#endif
    GLint last_viewport[4]; mbgl::platform::glGetIntegerv(GL_VIEWPORT, last_viewport);
    //GLint last_scissor_box[4]; mbgl::platform::glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    //GLenum last_blend_src_rgb; mbgl::platform::glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
    //GLenum last_blend_dst_rgb; mbgl::platform::glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha; mbgl::platform::glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha; mbgl::platform::glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb; mbgl::platform::glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
    //GLenum last_blend_equation_alpha; mbgl::platform::glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
    GLboolean last_enable_blend = mbgl::platform::glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = mbgl::platform::glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = mbgl::platform::glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_stencil_test = mbgl::platform::glIsEnabled(GL_STENCIL_TEST);
    GLboolean last_enable_scissor_test = mbgl::platform::glIsEnabled(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    GLboolean last_enable_primitive_restart = (bd->GlVersion >= 310) ? mbgl::platform::glIsEnabled(GL_PRIMITIVE_RESTART) : GL_FALSE;
#endif

    // Setup desired GL state
    // Recreate the VAO every time (this is to easily allow multiple GL contexts to be rendered to. VAO are not shared among GL contexts)
    // The renderer would actually work without any VAO bound, but then our VertexAttrib calls would overwrite the default one currently bound.
    //GLuint vertex_array_object = 0;
#ifdef IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
    MBGL_CHECK_ERROR(glGenVertexArrays(1, &vertex_array_object));
#endif
    ImGui_ImplMBGL_SetupRenderState(draw_data, fb_width, fb_height/*, vertex_array_object*/);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        // Upload vertex/index buffers
        // - OpenGL drivers are in a very sorry state nowadays....
        //   During 2021 we attempted to switch from glBufferData() to orphaning+glBufferSubData() following reports
        //   of leaks on Intel GPU when using multi-viewports on Windows.
        // - After this we kept hearing of various display corruptions issues. We started disabling on non-Intel GPU, but issues still got reported on Intel.
        // - We are now back to using exclusively glBufferData(). So bd->UseBufferSubData IS ALWAYS FALSE in this code.
        //   We are keeping the old code path for a while in case people finding new issues may want to test the bd->UseBufferSubData path.
        // - See https://github.com/ocornut/imgui/issues/4468 and please report any corruption issues.
        const GLsizeiptr vtx_buffer_size = (GLsizeiptr)cmd_list->VtxBuffer.Size * (int)sizeof(ImDrawVert);
        const GLsizeiptr idx_buffer_size = (GLsizeiptr)cmd_list->IdxBuffer.Size * (int)sizeof(ImDrawIdx);
        if (bd->UseBufferSubData)
        {
            if (bd->VertexBufferSize < vtx_buffer_size)
            {
                bd->VertexBufferSize = vtx_buffer_size;
                MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, bd->VertexBufferSize, nullptr, GL_STREAM_DRAW));
            }
            if (bd->IndexBufferSize < idx_buffer_size)
            {
                bd->IndexBufferSize = idx_buffer_size;
                MBGL_CHECK_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER, bd->IndexBufferSize, nullptr, GL_STREAM_DRAW));
            }
            MBGL_CHECK_ERROR(glBufferSubData(GL_ARRAY_BUFFER, 0, vtx_buffer_size, (const GLvoid*)cmd_list->VtxBuffer.Data));
            MBGL_CHECK_ERROR(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, idx_buffer_size, (const GLvoid*)cmd_list->IdxBuffer.Data));
        }
        else
        {
            MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, vtx_buffer_size, (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW));
            MBGL_CHECK_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_buffer_size, (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW));
        }

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ;//ImGui_ImplMBGL_SetupRenderState(draw_data, fb_width, fb_height/*, vertex_array_object*/);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                MBGL_CHECK_ERROR(mbgl::platform::glScissor((int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y)));

                // Bind texture, Draw
                MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->GetTexID()));
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_VTX_OFFSET
                if (bd->GlVersion >= 320)
                    MBGL_CHECK_ERROR(mbgl::platform::glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)pcmd->VtxOffset));
                else
#endif
                std::cout << "im drawing" << std::endl;
                MBGL_CHECK_ERROR(mbgl::platform::glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx))));
            }
        }
    }

    // Restore modified GL state
    // This "glIsProgram()" check is required because if the program is "pending deletion" at the time of binding backup, it will have been deleted by now and will cause an OpenGL error. See #6220.
    if (last_program == 0 || glIsProgram(last_program)) glUseProgram(last_program);
    mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
        glBindSampler(0, last_sampler);
#endif
    mbgl::platform::glActiveTexture(last_active_texture);
#ifdef IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
    glBindVertexArray(last_vertex_array_object);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
#ifndef IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    //last_vtx_attrib_state_pos.SetState(bd->a_pos);
    //last_vtx_attrib_state_uv.SetState(bd->a_uv);
    //last_vtx_attrib_state_color.SetState(bd->a_color);
#endif
    //glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    //mbgl::platform::glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) mbgl::platform::glEnable(GL_BLEND); else mbgl::platform::glDisable(GL_BLEND);
    if (last_enable_cull_face) mbgl::platform::glEnable(GL_CULL_FACE); else mbgl::platform::glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) mbgl::platform::glEnable(GL_DEPTH_TEST); else mbgl::platform::glDisable(GL_DEPTH_TEST);
    if (last_enable_stencil_test) mbgl::platform::glEnable(GL_STENCIL_TEST); else mbgl::platform::glDisable(GL_STENCIL_TEST);
    if (last_enable_scissor_test) mbgl::platform::glEnable(GL_SCISSOR_TEST); else mbgl::platform::glDisable(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    if (bd->GlVersion >= 310) { if (last_enable_primitive_restart) mbgl::platform::glEnable(GL_PRIMITIVE_RESTART); else mbgl::platform::glDisable(GL_PRIMITIVE_RESTART); }
#endif

#ifdef IMGUI_IMPL_HAS_POLYGON_MODE
    // Desktop OpenGL 3.0 and OpenGL 3.1 had separate polygon draw modes for front-facing and back-facing faces of polygons
    if (bd->GlVersion <= 310 || bd->GlProfileIsCompat)
    {
        glPolygonMode(GL_FRONT, (GLenum)last_polygon_mode[0]);
        glPolygonMode(GL_BACK, (GLenum)last_polygon_mode[1]);
    }
    else
    {
        glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
    }
#endif // IMGUI_IMPL_HAS_POLYGON_MODE

    mbgl::platform::glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    //mbgl::platform::glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
    (void)bd; // Not all compilation paths use this
}

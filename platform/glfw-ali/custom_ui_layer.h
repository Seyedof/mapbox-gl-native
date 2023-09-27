//#include <mbgl/test/util.hpp>
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
//#include "third-party/imgui/backends/imgui_impl_glfw.h"
//#include "third-party/imgui/backends/imgui_impl_opengl3.h"
#include "imgui_impl_mbgl.h"
#include "glfw_view.hpp"

//#include <mbgl/gfx/headless_frontend.hpp>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/gl/defines.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/style/layers/fill_layer.hpp>
#include <mbgl/style/style.hpp>
#include <mbgl/util/io.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/util/run_loop.hpp>
#include <iostream>

using namespace mbgl;
using namespace mbgl::style;
using namespace mbgl::platform;

// Note that custom layers need to draw geometry with a z value of 1 to take advantage of
// depth-based fragment culling.
// static const GLchar* vertexShaderSource = R"MBGL_SHADER(
// attribute vec2 a_pos;
// void main() {
//     gl_Position = vec4(a_pos, 1, 1);
// }
// )MBGL_SHADER";

// static const GLchar* vertexShaderSource = R"MBGL_SHADER(
//         attribute vec2 a_pos;
//         attribute vec2 a_uv;
//         attribute vec4 a_color;
//         // layout (location = 0) in vec2 Position;
//         // layout (location = 1) in vec2 UV;
//         // layout (location = 2) in vec4 Color;
//         uniform mat4 ProjMtx;
//         // out vec2 Frag_UV;
//         // out vec4 Frag_Color;
//         void main()
//         {
//             // Frag_UV = UV;
//             // Frag_Color = Color;
//             gl_Position = ProjMtx * vec4(a_pos,1,1);
//             gl_Position = vec4(a_pos, 1, 1);
//         }
//         )MBGL_SHADER";

// static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
// void main() {
//     gl_FragColor = vec4(0, 0.5, 0, 0.5);
// }
// )MBGL_SHADER";

// Not using any mbgl-specific stuff (other than a basic error-checking macro) in the
// layer implementation because it is intended to reflect how someone using custom layers
// might actually write their own implementation.

class CustomUiLayer : public mbgl::style::CustomLayerHost {
public:
    void initialize() override {
        {
            GLint current_texture;
            mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

            GLint last_texture, last_array_buffer;
            mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
            mbgl::platform::glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);

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

            program = MBGL_CHECK_ERROR(mbgl::platform::glCreateProgram());
            if (program == 0) {
                std::cout << "shit4" << std::endl;
            }
            vertexShader = MBGL_CHECK_ERROR(mbgl::platform::glCreateShader(GL_VERTEX_SHADER));
            fragmentShader = MBGL_CHECK_ERROR(mbgl::platform::glCreateShader(GL_FRAGMENT_SHADER));

            MBGL_CHECK_ERROR(mbgl::platform::glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr));
            MBGL_CHECK_ERROR(mbgl::platform::glCompileShader(vertexShader));
            MBGL_CHECK_ERROR(mbgl::platform::glAttachShader(program, vertexShader));
            MBGL_CHECK_ERROR(mbgl::platform::glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr));
            MBGL_CHECK_ERROR(mbgl::platform::glCompileShader(fragmentShader));
            if (fragmentShader == 0) {
                std::cout << "shit5" << std::endl;
            }
            MBGL_CHECK_ERROR(mbgl::platform::glAttachShader(program, fragmentShader));
            MBGL_CHECK_ERROR(mbgl::platform::glLinkProgram(program));
            a_pos = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_pos"));
            a_uv = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_uv"));
            a_color = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_color"));

            u_texture = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "u_Texture"));
            u_projMatrix = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "ProjMtx"));
            
            // Create buffers
            glGenBuffers(1, &VboHandle);
            glGenBuffers(1, &ElementsHandle);
            
            std::cout << "point3" << std::endl;

            {
                // Build texture atlas
                unsigned char* pixels;
                int width, height;
                auto& io = ImGui::GetIO();
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

                std::cout << "point2" << std::endl;

                // Upload texture to graphics system
                // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
                //GLint last_texture;
                MBGL_CHECK_ERROR(mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture));
                MBGL_CHECK_ERROR(mbgl::platform::glGenTextures(1, &FontTexture));
                MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, FontTexture));
                MBGL_CHECK_ERROR(mbgl::platform::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                MBGL_CHECK_ERROR(mbgl::platform::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

                MBGL_CHECK_ERROR(mbgl::platform::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));

                // Store our identifier
                io.Fonts->SetTexID((ImTextureID)(intptr_t)FontTexture);

                // Restore state
                MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));
            }

            // Restore modified GL state
            MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));
            MBGL_CHECK_ERROR(mbgl::platform::glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer));
            GLFWView::showUI = true;
        }

        // program = MBGL_CHECK_ERROR(glCreateProgram());
        //     if (program == 0) {
        //         std::cout<< "omg2" << std::endl;
        //     }

        // vertexShader = MBGL_CHECK_ERROR(glCreateShader(GL_VERTEX_SHADER));
        // fragmentShader = MBGL_CHECK_ERROR(glCreateShader(GL_FRAGMENT_SHADER));

        // MBGL_CHECK_ERROR(glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr));
        // MBGL_CHECK_ERROR(glCompileShader(vertexShader));
        // MBGL_CHECK_ERROR(glAttachShader(program, vertexShader));
        // MBGL_CHECK_ERROR(glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr));
        // MBGL_CHECK_ERROR(glCompileShader(fragmentShader));
        // MBGL_CHECK_ERROR(glAttachShader(program, fragmentShader));
        // MBGL_CHECK_ERROR(glLinkProgram(program));
        // a_pos = MBGL_CHECK_ERROR(glGetAttribLocation(program, "a_pos"));

        // GLfloat triangle[] = { 0, 0.5, 0.5, -0.5, -0.5, -0.5 };
        // MBGL_CHECK_ERROR(glGenBuffers(1, &buffer));
        // MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, buffer));
        // MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), triangle, GL_STATIC_DRAW));
    }

    void render(const mbgl::style::CustomLayerRenderParameters&) override {

        if (imguiDrawData) {
            std::cout << "has data" << std::endl;
            RenderDrawData(imguiDrawData);
        }
        RenderDrawData(ImGui::GetDrawData());
        std::cout << "no data" << std::endl;

        // MBGL_CHECK_ERROR(glUseProgram(program));
        // MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, buffer));
        // MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos));
        // MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 0, nullptr));
        // MBGL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_STRIP, 0, 3));
    }

    void contextLost() override {}

    void deinitialize() override {
         if (program) {
                MBGL_CHECK_ERROR(glDeleteBuffers(1, &buffer));
                MBGL_CHECK_ERROR(glDetachShader(program, vertexShader));
                MBGL_CHECK_ERROR(glDetachShader(program, fragmentShader));
                MBGL_CHECK_ERROR(glDeleteShader(vertexShader));
                MBGL_CHECK_ERROR(glDeleteShader(fragmentShader));
                MBGL_CHECK_ERROR(glDeleteProgram(program));
            }
    }

    void SetImGuiDrawData(ImDrawData* draw_data)
    {
        return;
        std::cout << "got data" << std::endl;
        imguiDrawData = draw_data;
        if (!draw_data) {
            std::cout << "got null data" << std::endl;
        }
        else {
            std::cout << "got good data" << std::endl;
        }
    }

    void SetupRenderState(ImDrawData* draw_data, int fb_width, int fb_height/*, GLuint vertex_array_object*/)
    {
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
        glUseProgram(program);
        glUniform1i(u_texture, 0);
        glUniformMatrix4fv(u_projMatrix, 1, GL_FALSE, &ortho_projection[0][0]);

    #ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
        if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
            glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 and GL ES 3.0 may set that otherwise.
    #endif

        // Bind vertex/index buffers and setup attributes for ImDrawVert
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, VboHandle));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ElementsHandle));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_uv));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_color));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos)));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_uv,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv)));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col)));
    }

    void RenderDrawData(ImDrawData* draw_data)
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
        SetupRenderState(draw_data, fb_width, fb_height/*, vertex_array_object*/);

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
            if (UseBufferSubData)
            {
                if (VertexBufferSize < vtx_buffer_size)
                {
                    VertexBufferSize = vtx_buffer_size;
                    MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, VertexBufferSize, nullptr, GL_STREAM_DRAW));
                }
                if (IndexBufferSize < idx_buffer_size)
                {
                    IndexBufferSize = idx_buffer_size;
                    MBGL_CHECK_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER, IndexBufferSize, nullptr, GL_STREAM_DRAW));
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
        //(void)bd; // Not all compilation paths use this
    }


    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint buffer = 0;
    GLuint a_pos = 0;

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

    //GLuint program = 0;
    //GLuint vertexShader = 0;
    //GLuint fragmentShader = 0;
    //GLuint buffer = 0;
    //GLuint a_pos = 0;
    GLuint a_uv = 0;
    GLuint a_color = 0;

    GLuint u_texture = 0;
    GLuint u_projMatrix = 0;

    ImDrawData* imguiDrawData = nullptr;
};

/*
TEST(CustomLayer, Basic) {
    if (gfx::Backend::GetType() != gfx::Backend::Type::OpenGL) {
        return;
    }

    util::RunLoop loop;

    HeadlessFrontend frontend { 1 };
    Map map(frontend, MapObserver::nullObserver(),
            MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()),
            ResourceOptions().withCachePath(":memory:").withAssetPath("test/fixtures/api/assets"));
    map.getStyle().loadJSON(util::read_file("test/fixtures/api/water.json"));
    map.jumpTo(CameraOptions().withCenter(LatLng { 37.8, -122.5 }).withZoom(10.0));
    map.getStyle().addLayer(std::make_unique<CustomLayer>(
        "custom",
        std::make_unique<TestLayer>()));

    auto layer = std::make_unique<FillLayer>("landcover", "mapbox");
    layer->setSourceLayer("landcover");
    layer->setFillColor(Color{ 1.0, 1.0, 0.0, 1.0 });
    map.getStyle().addLayer(std::move(layer));

    test::checkImage("test/fixtures/custom_layer/basic", frontend.render(map).image, 0.0006, 0.1);
}
*/
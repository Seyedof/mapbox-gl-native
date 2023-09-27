#include "glfw_view.hpp"
#include "tiny_gltf.h"

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
using namespace tinygltf;

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

class Custom3DLayer : public mbgl::style::CustomLayerHost {
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

            // Restore modified GL state
            MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));
            MBGL_CHECK_ERROR(mbgl::platform::glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer));

            std::string err;
            std::string warn;

            bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, "Duck.gltf");

            if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
            }

            if (!err.empty()) {
            printf("Err: %s\n", err.c_str());
            }
            assert(ret);

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

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint buffer = 0;
    GLuint a_pos = 0;

    unsigned int    VboHandle, ElementsHandle;
    GLsizeiptr      VertexBufferSize;
    GLsizeiptr      IndexBufferSize;
    bool            HasClipOrigin;
    bool            UseBufferSubData;

    Model model;
    TinyGLTF loader;
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
};
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

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
#include <mbgl/gl/vertex_array_extension.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

using namespace mbgl;
using namespace mbgl::style;
using namespace mbgl::platform;
using namespace tinygltf;

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

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
                attribute vec3 a_pos;
                attribute vec3 a_normal;
                attribute vec2 a_uv;
                uniform mat4 MVP;
                varying vec2 Frag_UV;
                varying vec3 Frag_Normal;
                void main() {
                    //gl_Position = vec4(a_pos, 1);
                    gl_Position = MVP * vec4(a_pos.xyz,1);
                    Frag_UV = vec2(1,1);
	                //Frag_Normal = normalize(mat3(MVP) * a_normal);
                    Frag_Normal = vec3(1,0,0);
                }
            )MBGL_SHADER";

            static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
                uniform sampler2D u_Texture;
                varying vec2 Frag_UV;
                varying vec3 Frag_Normal;
                void main()
                {
	                //float lum = max(dot(Frag_Normal, normalize(0, -1, 0)), 0.0);
                    //gl_FragColor = vec4((0.3 + 0.7 * lum) * vec3(1,1,1), 1.0) * texture2D(u_Texture, Frag_UV);
                    gl_FragColor = vec4(1,1,1,1);
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
            if (vertexShader == 0) {
                std::cout << "shit4" << std::endl;
            }
            if (fragmentShader == 0) {
                std::cout << "shit5" << std::endl;
            }
            MBGL_CHECK_ERROR(mbgl::platform::glAttachShader(program, fragmentShader));
            MBGL_CHECK_ERROR(mbgl::platform::glLinkProgram(program));
            a_pos = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_pos"));
            a_normal = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_normal"));
            a_uv = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_uv"));

            u_texture = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "u_Texture"));
            u_MVP = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "MVP"));
            
            std::cout << "point3" << std::endl;

            // Restore modified GL state
            MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));
            MBGL_CHECK_ERROR(mbgl::platform::glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer));

            std::string err;
            std::string warn;

            bool ret = loader.LoadASCIIFromFile(&model3D, &err, &warn, "/tmp/Duck.gltf");

            if (!warn.empty()) {
                printf("Warn: %s\n", warn.c_str());
            }

            if (!err.empty()) {
                printf("Err: %s\n", err.c_str());
            }
            assert(ret);

            modelVaoAndEbos = bindModel(model3D);
            
            GLFWView::show3D = true;
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

    void bindMesh(std::map<int, GLuint>& vbos, tinygltf::Model &model, tinygltf::Mesh &mesh) {
        for (size_t i = 0; i < model.bufferViews.size(); ++i) {
            const tinygltf::BufferView &bufferView = model.bufferViews[i];
            if (bufferView.target == 0) {  // TODO impl drawarrays
                std::cout << "WARN: bufferView.target is zero" << std::endl;
                continue;  // Unsupported bufferView.
            }

            const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
            std::cout << "bufferview.target " << bufferView.target << std::endl;

            GLuint vbo;
            glGenBuffers(1, &vbo);
            vbos[i] = vbo;
            glBindBuffer(bufferView.target, vbo);

            // Ali
        /*MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_normal));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_uv));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos,    3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (GLvoid*)(0)));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_normal, 3, GL_FLOAT, GL_TRUE,  sizeof(float) * 8, (GLvoid*)(3 * 4)));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_uv,     2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (GLvoid*)(6 * 4)));*/
            // Ali

            std::cout << "buffer.data.size = " << buffer.data.size()
                    << ", bufferview.byteOffset = " << bufferView.byteOffset
                    << std::endl;

            glBufferData(bufferView.target, bufferView.byteLength,
                        &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
        }

        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            tinygltf::Primitive primitive = mesh.primitives[i];
            tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

            for (auto &attrib : primitive.attributes) {
                tinygltf::Accessor accessor = model.accessors[attrib.second];
                int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

                int size = 1;
                if (accessor.type != TINYGLTF_TYPE_SCALAR) {
                    size = accessor.type;
                }

                int vaa = -1;
                if (attrib.first.compare("POSITION") == 0) vaa = 0;
                if (attrib.first.compare("NORMAL") == 0) vaa = 1;
                if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
                std::cout << vaa << std::endl;
                if (vaa > -1) {
                    glEnableVertexAttribArray(vaa);
                    glVertexAttribPointer(vaa, size, accessor.componentType,
                                            accessor.normalized ? GL_TRUE : GL_FALSE,
                                            byteStride, BUFFER_OFFSET(accessor.byteOffset));
                } else
                    std::cout << "vaa missing: " << attrib.first << std::endl;
                }

                if (model.textures.size() > 0) {
                    // fixme: Use material's baseColor
                    tinygltf::Texture &tex = model.textures[0];

                    if (tex.source > -1) {

                        GLuint texid;
                        mbgl::platform::glGenTextures(1, &texid);

                        tinygltf::Image &image = model.images[tex.source];

                        mbgl::platform::glBindTexture(GL_TEXTURE_2D, texid);
                        mbgl::platform::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                        mbgl::platform::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        mbgl::platform::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        mbgl::platform::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        mbgl::platform::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                        GLenum format = GL_RGBA;

                        if (image.component == 1) {
                            format = GL_RED;
                        } else if (image.component == 2) {
                            format = GL_RG;
                        } else if (image.component == 3) {
                            format = GL_RGB;
                        } else {
                            // ???
                        }

                        GLenum type = GL_UNSIGNED_BYTE;
                        if (image.bits == 8) {
                            // ok
                        } else if (image.bits == 16) {
                            type = GL_UNSIGNED_SHORT;
                        } else {
                            // ???
                        }

                        mbgl::platform::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                                                    format, type, &image.image.at(0));
                    }
            }
        }
    }

    // bind models
    void bindModelNodes(std::map<int, GLuint>& vbos, tinygltf::Model &model, tinygltf::Node &node) {
        if ((node.mesh >= 0) && (node.mesh < static_cast<int>(model.meshes.size()))) {
            bindMesh(vbos, model, model.meshes[node.mesh]);
        }

        for (size_t i = 0; i < node.children.size(); i++) {
            assert((node.children[i] >= 0) && (node.children[i] < static_cast<int>(model.nodes.size())));
            bindModelNodes(vbos, model, model.nodes[node.children[i]]);
        }
    }

    std::pair<GLuint, std::map<int, GLuint>> bindModel(tinygltf::Model &model) {
        std::map<int, GLuint> vbos;
        GLuint vao;
        //mbgl::gl::VertexArray::genVertexArrays(1, &vao);
        //mbgl::gl::extension::bindVertexArray(vao);
        vao = 100;

        const tinygltf::Scene &scene = model.scenes[model.defaultScene];
        for (size_t i = 0; i < scene.nodes.size(); ++i) {
            assert((scene.nodes[i] >= 0) && (scene.nodes[i] < static_cast<int>(model.nodes.size())));
            bindModelNodes(vbos, model, model.nodes[scene.nodes[i]]);
        }

        //glBindVertexArray(0);
        // cleanup vbos but do not delete index buffers yet
        for (auto it = vbos.cbegin(); it != vbos.cend();) {
            tinygltf::BufferView bufferView = model.bufferViews[it->first];
            if (bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
                glDeleteBuffers(1, &vbos[it->first]);
                vbos.erase(it++);
            }
            else {
                ++it;
            }
        }

        return {vao, vbos};
    }

    void drawMesh(const std::map<int, GLuint>& vbos, tinygltf::Model &model, tinygltf::Mesh &mesh) {
        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            tinygltf::Primitive primitive = mesh.primitives[i];
            tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

            continue;
            mbgl::platform::glDrawElements( primitive.mode, indexAccessor.count,
                            indexAccessor.componentType,
                            BUFFER_OFFSET(indexAccessor.byteOffset));
        }
    }
    
    void drawModelNodes(const std::pair<GLuint, std::map<int, GLuint>>& vaoAndEbos, tinygltf::Model &model, tinygltf::Node &node) {
        if ((node.mesh >= 0) && (node.mesh < static_cast<int>(model.meshes.size()))) {
            drawMesh(vaoAndEbos.second, model, model.meshes[node.mesh]);
        }
        for (size_t i = 0; i < node.children.size(); i++) {
            drawModelNodes(vaoAndEbos, model, model.nodes[node.children[i]]);
        }
    }

    void drawModel(const std::pair<GLuint, std::map<int, GLuint>>& vaoAndEbos, tinygltf::Model &model) {
        //glBindVertexArray(vaoAndEbos.first);

        const tinygltf::Scene &scene = model.scenes[model.defaultScene];
        for (size_t i = 0; i < scene.nodes.size(); ++i) {
            drawModelNodes(vaoAndEbos, model, model.nodes[scene.nodes[i]]);
        }

        //glBindVertexArray(0);
    }

    void SetupRenderState()
    {
        mbgl::platform::glEnable(GL_BLEND);
        mbgl::platform::glBlendEquation(GL_FUNC_ADD);
        mbgl::platform::glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        mbgl::platform::glDisable(GL_CULL_FACE);
        mbgl::platform::glDisable(GL_DEPTH_TEST);
        mbgl::platform::glDisable(GL_STENCIL_TEST);
        mbgl::platform::glEnable(GL_SCISSOR_TEST);

        int fb_width = 1024;
        int fb_height = 768;
        MBGL_CHECK_ERROR(mbgl::platform::glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height));
        // float L = 0;
        // float R = fb_width;
        // float T = 0;
        // float B = fb_height;

        // const float ortho_projection[4][4] =
        // {
        //     { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        //     { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        //     { 0.0f,         0.0f,        -1.0f,   0.0f },
        //     { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        // };
        glUseProgram(program);
        glUniform1i(u_texture, 0);
        //glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &ortho_projection[0][0]);
        //mbgl::platform::

        // Bind vertex/index buffers and setup attributes for ImDrawVert
        // MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, VboHandle));
        // MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ElementsHandle));
        /*
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_normal));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_uv));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos,    3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (GLvoid*)(0)));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_normal, 3, GL_FLOAT, GL_TRUE,  sizeof(float) * 8, (GLvoid*)(3 * 4)));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_uv,     2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (GLvoid*)(6 * 4)));*/
    }

    void render(const mbgl::style::CustomLayerRenderParameters& renderParams)  override {
        MBGL_CHECK_ERROR(glUseProgram(program));

        float MVP[16];
        for (int i = 0; i < 16; i++) {
            MVP[i] = renderParams.projectionMatrix[i];
            std::cout << renderParams.projectionMatrix[i] << " ,";
            if (((i+1) & 3) == 0)
                std::cout << std::endl;
        }
        std::cout << std::endl;

        std::cout << "width= " << renderParams.width << std::endl;
        std::cout << "height= " << renderParams.height << std::endl;
        std::cout << "lat= " << renderParams.latitude << std::endl;
        std::cout << "long= " << renderParams.longitude << std::endl;
        std::cout << "zoom= " << renderParams.zoom << std::endl;
        std::cout << "bearing= " << renderParams.bearing << std::endl;
        std::cout << "pitch= " << renderParams.pitch << std::endl;
        std::cout << "fov= " << renderParams.fieldOfView << std::endl;

        std::cout << std::endl;

        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_MVP, 1, GL_FALSE, MVP));

        SetupRenderState();
        drawModel(modelVaoAndEbos, model3D);
        // MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, buffer));
        // MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos));
        // MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 0, nullptr));
        // MBGL_CHECK_ERROR(glDrawArrays(GL_TRIANGLE_STRIP, 0, 3));
    }

    void contextLost() override {}

    void deinitialize() override {
        if (program) {
            // MBGL_CHECK_ERROR(glDeleteBuffers(1, &buffer));
            // MBGL_CHECK_ERROR(glDetachShader(program, vertexShader));
            // MBGL_CHECK_ERROR(glDetachShader(program, fragmentShader));
            // MBGL_CHECK_ERROR(glDeleteShader(vertexShader));
            // MBGL_CHECK_ERROR(glDeleteShader(fragmentShader));
            // MBGL_CHECK_ERROR(glDeleteProgram(program));
        }
    }

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;

    std::pair<GLuint, std::map<int, GLuint>> modelVaoAndEbos;

    Model model3D;
    TinyGLTF loader;

    GLuint a_pos = 0;
    GLuint a_uv = 0;
    GLuint a_normal = 0;

    GLuint u_texture = 0;
    GLuint u_MVP = 0;
};

#pragma GCC diagnostic pop

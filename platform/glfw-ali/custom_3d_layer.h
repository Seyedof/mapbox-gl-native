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
#include <mbgl/map/transform_state.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/style/layers/fill_layer.hpp>
#include <mbgl/style/style.hpp>
#include <mbgl/util/io.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/util/run_loop.hpp>
#include <mbgl/util/projection.hpp>
#include <mbgl/util/camera.hpp>
#include <mbgl/gl/vertex_array_extension.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>

using namespace mbgl;
using namespace mbgl::style;
using namespace mbgl::platform;
using namespace tinygltf;

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

LatLng gObjectLatLng{0.0, 0.0};

class Custom3DLayer : public mbgl::style::CustomLayerHost {
public:
    typedef struct {
        std::map<std::string, GLint> attribs;
        std::map<std::string, GLint> uniforms;
    } GLProgramState;

    void initialize() override {
        {
            GLfloat triangle[] = {  -0.5, -0.5, 0.0,
                                     0.0, 0.5, 0.0,
                                     0.5, -0.5, 0.0};
            MBGL_CHECK_ERROR(glGenBuffers(1, &mybuffer));
            MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, mybuffer));
            MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(GLfloat), triangle, GL_STATIC_DRAW));

            //alaki.reserve(1000);
            GLint current_texture;
            mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

            GLint last_texture, last_array_buffer;
            mbgl::platform::glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
            mbgl::platform::glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);

            static const GLchar* vertexShaderSource = R"MBGL_SHADER(
                attribute vec3 a_pos;
                attribute vec3 a_normal;
                attribute vec2 a_uv;
                uniform mat4 u_MVP;
                uniform mat4 u_WORLD;
                uniform mat4 u_LOCAL;
                varying vec2 Frag_UV;
                varying vec3 Frag_Normal;
                void main() {
                    gl_Position = u_MVP * u_WORLD * u_LOCAL * vec4(a_pos.xyz,1);
                    Frag_Normal = normalize(u_LOCAL * vec4(a_normal, 0)).xyz;
                    Frag_UV = a_uv;
                }
            )MBGL_SHADER";

            static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
                uniform sampler2D u_Texture;
                varying vec3 Frag_Normal;
                varying vec2 Frag_UV;
                void main()
                {
	                float n_dot_l = max(dot(-Frag_Normal, vec3(0, 0, -1)), 0.2);
                    gl_FragColor = vec4(n_dot_l, n_dot_l, n_dot_l, 1.0);
                    //gl_FragColor = vec4(Frag_Normal.xyz, 1.0);
                    //gl_FragColor = vec4(1, 0, 0, 0.8);
                }
            )MBGL_SHADER";

            program = MBGL_CHECK_ERROR(mbgl::platform::glCreateProgram());
            vertexShader = MBGL_CHECK_ERROR(mbgl::platform::glCreateShader(GL_VERTEX_SHADER));
            fragmentShader = MBGL_CHECK_ERROR(mbgl::platform::glCreateShader(GL_FRAGMENT_SHADER));

            MBGL_CHECK_ERROR(mbgl::platform::glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr));
            MBGL_CHECK_ERROR(mbgl::platform::glCompileShader(vertexShader));
            MBGL_CHECK_ERROR(mbgl::platform::glAttachShader(program, vertexShader));
            MBGL_CHECK_ERROR(mbgl::platform::glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr));
            MBGL_CHECK_ERROR(mbgl::platform::glCompileShader(fragmentShader));
            MBGL_CHECK_ERROR(mbgl::platform::glAttachShader(program, fragmentShader));
            MBGL_CHECK_ERROR(mbgl::platform::glLinkProgram(program));
            a_pos = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_pos"));
            a_normal = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_normal"));
            a_uv = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_uv"));

            u_texture = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "u_Texture"));
            u_MVP = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "u_MVP"));
            u_WORLD = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "u_WORLD"));
            u_LOCAL = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "u_LOCAL"));
            
            // Restore modified GL state
            MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));
            MBGL_CHECK_ERROR(mbgl::platform::glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer));

            std::string err;
            std::string warn;
            bool ret = loader.LoadASCIIFromFile(&model3D, &err, &warn, "./Duck.gltf");

            if (!warn.empty()) {
                printf("Warn: %s\n", warn.c_str());
            }

            if (!err.empty()) {
                printf("Err: %s\n", err.c_str());
            }
            assert(ret);

            SetupMeshState();
            
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

    size_t ComponentTypeByteSize(int type) {
        switch (type) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            case TINYGLTF_COMPONENT_TYPE_BYTE:
                return sizeof(char);
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            case TINYGLTF_COMPONENT_TYPE_SHORT:
                return sizeof(short);
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            case TINYGLTF_COMPONENT_TYPE_INT:
                return sizeof(int);
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return sizeof(float);
            case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                return sizeof(double);
            default:
                return 0;
        }
    }

    void SetupMeshState() {
        for (size_t i = 0; i < model3D.bufferViews.size(); i++) {
            const tinygltf::BufferView &bufferView = model3D.bufferViews[i];
            if (bufferView.target == 0) {
                std::cout << "WARN: bufferView.target is zero" << std::endl;
                continue;  // Unsupported bufferView.
            }

            int sparse_accessor = -1;
            for (size_t a_i = 0; a_i < model3D.accessors.size(); ++a_i) {
                const auto &accessor = model3D.accessors[a_i];
                if (accessor.bufferView == static_cast<int>(i)) {
                    std::cout << i << " is used by accessor " << a_i << std::endl;
                    if (accessor.sparse.isSparse) {
                        std::cout
                            << "WARN: this bufferView has at least one sparse accessor to "
                                "it. We are going to load the data as patched by this "
                                "sparse accessor, not the original data"
                            << std::endl;
                        sparse_accessor = a_i;
                        break;
                    }
                }
            }

            const tinygltf::Buffer &buffer = model3D.buffers[bufferView.buffer];
            GLuint vb;
            glGenBuffers(1, &vb);
            glBindBuffer(bufferView.target, vb);
            std::cout << "buffer.size= " << buffer.data.size()
                        << ", byteOffset = " << bufferView.byteOffset << std::endl;

            if (sparse_accessor < 0)
                glBufferData(bufferView.target, bufferView.byteLength,
                            &buffer.data.at(0) + bufferView.byteOffset,
                            GL_STATIC_DRAW);
            else {
                const auto accessor = model3D.accessors[sparse_accessor];
                // copy the buffer to a temporary one for sparse patching
                unsigned char *tmp_buffer = new unsigned char[bufferView.byteLength];
                memcpy(tmp_buffer, buffer.data.data() + bufferView.byteOffset, bufferView.byteLength);

                const size_t size_of_object_in_buffer = ComponentTypeByteSize(accessor.componentType);
                const size_t size_of_sparse_indices = ComponentTypeByteSize(accessor.sparse.indices.componentType);

                const auto &indices_buffer_view = model3D.bufferViews[accessor.sparse.indices.bufferView];
                const auto &indices_buffer = model3D.buffers[indices_buffer_view.buffer];

                const auto &values_buffer_view = model3D.bufferViews[accessor.sparse.values.bufferView];
                const auto &values_buffer = model3D.buffers[values_buffer_view.buffer];

                for (size_t sparse_index = 0; sparse_index < static_cast<size_t>(accessor.sparse.count); ++sparse_index) {
                    int index = 0;
                    switch (accessor.sparse.indices.componentType) {
                        case TINYGLTF_COMPONENT_TYPE_BYTE:
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            index = (int)*(
                                unsigned char *)(indices_buffer.data.data() +
                                                indices_buffer_view.byteOffset +
                                                accessor.sparse.indices.byteOffset +
                                                (sparse_index * size_of_sparse_indices));
                            break;
                        case TINYGLTF_COMPONENT_TYPE_SHORT:
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                            index = (int)*(
                                        unsigned short *)(indices_buffer.data.data() +
                                        indices_buffer_view.byteOffset +
                                        accessor.sparse.indices.byteOffset +
                                        (sparse_index * size_of_sparse_indices));
                            break;
                        case TINYGLTF_COMPONENT_TYPE_INT:
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                            index = (int)*(
                                    unsigned int *)(indices_buffer.data.data() +
                                    indices_buffer_view.byteOffset +
                                    accessor.sparse.indices.byteOffset +
                                    (sparse_index * size_of_sparse_indices));
                            break;
                    }
                    std::cout << "updating sparse data at index  : " << index << std::endl;
                    // index is now the target of the sparse index to patch in
                    const unsigned char *read_from =
                            values_buffer.data.data() +
                            (values_buffer_view.byteOffset +
                            accessor.sparse.values.byteOffset) +
                            (sparse_index * (size_of_object_in_buffer * accessor.type));

                    unsigned char *write_to = tmp_buffer + index * (size_of_object_in_buffer * accessor.type);

                    memcpy(write_to, read_from, size_of_object_in_buffer * accessor.type);
                }

                glBufferData(bufferView.target, bufferView.byteLength, tmp_buffer, GL_STATIC_DRAW);
                delete[] tmp_buffer;
            }
            glBindBuffer(bufferView.target, 0);

            bufferState[i] = vb;
        }

        glUseProgram(program);
        GLint vtloc = glGetAttribLocation(program, "a_pos");
        GLint nrmloc = glGetAttribLocation(program, "a_normal");
        GLint uvloc = glGetAttribLocation(program, "a_uv");

        programState.attribs["POSITION"] = vtloc;
        programState.attribs["NORMAL"] = nrmloc;
        programState.attribs["TEXCOORD_0"] = uvloc;
    }

    void DrawMesh(tinygltf::Model &model, const tinygltf::Mesh &mesh) {

        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive &primitive = mesh.primitives[i];

            if (primitive.indices < 0) return;

            // Assume TEXTURE_2D target for the texture object.
            // glBindTexture(GL_TEXTURE_2D, gMeshState[mesh.name].diffuseTex[i]);

            std::map<std::string, int>::const_iterator it(primitive.attributes.begin());
            std::map<std::string, int>::const_iterator itEnd(primitive.attributes.end());

            for (; it != itEnd; it++) {
                assert(it->second >= 0);
                const tinygltf::Accessor &accessor = model.accessors[it->second];
                glBindBuffer(GL_ARRAY_BUFFER, bufferState[accessor.bufferView]);
                int size = 1;
                if (accessor.type == TINYGLTF_TYPE_SCALAR) {
                    size = 1;
                } else if (accessor.type == TINYGLTF_TYPE_VEC2) {
                    size = 2;
                } else if (accessor.type == TINYGLTF_TYPE_VEC3) {
                    size = 3;
                } else if (accessor.type == TINYGLTF_TYPE_VEC4) {
                    size = 4;
                } else {
                    assert(0);
                }
                // it->first would be "POSITION", "NORMAL", "TEXCOORD_0", ...
                if ((it->first.compare("POSITION") == 0) ||
                    (it->first.compare("NORMAL") == 0) ||
                    (it->first.compare("TEXCOORD_0") == 0)) {
                    if (programState.attribs[it->first] >= 0) {
                    // Compute byteStride from Accessor + BufferView combination.
                        int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                        assert(byteStride != -1);
                        glVertexAttribPointer(programState.attribs[it->first], size,
                                                accessor.componentType,
                                                accessor.normalized ? GL_TRUE : GL_FALSE,
                                                byteStride, BUFFER_OFFSET(accessor.byteOffset));
                        glEnableVertexAttribArray(programState.attribs[it->first]);
                    }
                }
            }

            const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferState[indexAccessor.bufferView]);
            int mode = -1;
            if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
                mode = GL_TRIANGLES;
            } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
                mode = GL_TRIANGLE_STRIP;
            } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                mode = GL_TRIANGLE_FAN;
            } else if (primitive.mode == TINYGLTF_MODE_POINTS) {
                mode = GL_POINTS;
            } else if (primitive.mode == TINYGLTF_MODE_LINE) {
                mode = GL_LINES;
            } else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP) {
                mode = GL_LINE_LOOP;
            } else {
                assert(0);
            }
            mbgl::platform::glDrawElements(mode, indexAccessor.count, indexAccessor.componentType, BUFFER_OFFSET(indexAccessor.byteOffset));

            std::map<std::string, int>::const_iterator it2(primitive.attributes.begin());
            std::map<std::string, int>::const_iterator itEnd2(primitive.attributes.end());

            for (; it2 != itEnd2; it2++) {
                if ((it2->first.compare("POSITION") == 0) ||
                    (it2->first.compare("NORMAL") == 0) ||
                    (it2->first.compare("TEXCOORD_0") == 0)) {
                    if (programState.attribs[it2->first] >= 0) {
                        glDisableVertexAttribArray(programState.attribs[it2->first]);
                    }
                }
            }
        }
    }

    void QuatToAngleAxis(const std::vector<double> quaternion, double &outAngleDegrees, double *axis) {
        double qx = quaternion[0];
        double qy = quaternion[1];
        double qz = quaternion[2];
        double qw = quaternion[3];
        
        double angleRadians = 2 * acos(qw);
        if (angleRadians == 0.0) {
            outAngleDegrees = 0.0;
            axis[0] = 0.0;
            axis[1] = 0.0;
            axis[2] = 1.0;
            return;
        }

        double denom = sqrt(1-qw*qw);
        outAngleDegrees = angleRadians * 180.0 / M_PI;
        axis[0] = qx / denom;
        axis[1] = qy / denom;
        axis[2] = qz / denom;
    }

    void DrawNode(tinygltf::Model &model, const tinygltf::Node &node) {
        // Apply xform
        glPushMatrix();
        if (node.matrix.size() == 16) {
            // Use `matrix' attribute
            glMultMatrixd(node.matrix.data());
        } else {
            // Assume Trans x Rotate x Scale order
            if (node.translation.size() == 3) {
                glTranslated(node.translation[0], node.translation[1], node.translation[2]);
            }    

            if (node.rotation.size() == 4) {
                double angleDegrees;
                double axis[3];
                QuatToAngleAxis(node.rotation, angleDegrees, axis);
           
                glRotated(angleDegrees, axis[0], axis[1], axis[2]);
            }

            if (node.scale.size() == 3) {
                glScaled(node.scale[0], node.scale[1], node.scale[2]);
            }
        }

        if (node.mesh > -1) {
            assert(node.mesh < static_cast<int>(model.meshes.size()));
            GLfloat local[16];
            mbgl::platform::glGetFloatv(GL_MODELVIEW_MATRIX, local);
            MBGL_CHECK_ERROR(glUniformMatrix4fv(u_LOCAL, 1, GL_FALSE, (GLfloat*)local));

            DrawMesh(model, model.meshes[node.mesh]);
        }

        // Draw child nodes.
        for (size_t i = 0; i < node.children.size(); i++) {
            assert(node.children[i] < static_cast<int>(model.nodes.size()));
            DrawNode(model, model.nodes[node.children[i]]);
        }

        glPopMatrix();
    }

    void DrawModel(tinygltf::Model &model) {
        // If the glTF asset has at least one scene, and doesn't define a default one
        // just show the first one we can find
        assert(model.scenes.size() > 0);
        int scene_to_display = model.defaultScene > -1 ? model.defaultScene : 0;
        const tinygltf::Scene &scene = model.scenes[scene_to_display];
        for (size_t i = 0; i < scene.nodes.size(); i++) {
            DrawNode(model, model.nodes[scene.nodes[i]]);
        }
    }

    static double mercatorXfromLng(double lng) {
        return (180.0 + lng) / 360.0;
    }

    static double mercatorYfromLat(double lat) {
        return (180.0 - (180.0 / M_PI * std::log(std::tan(M_PI_4 + lat * M_PI / 360.0)))) / 360.0;
    }

    static vec3 toMercator(const LatLng& location, double altitudeMeters) {
        const double pixelsPerMeter = 1.0 / Projection::getMetersPerPixelAtLatitude(location.latitude(), 0.0);
        const double worldSize = Projection::worldSize(std::pow(2.0, 0.0));

        return {{mercatorXfromLng(location.longitude()),
                mercatorYfromLat(location.latitude()),
                altitudeMeters * pixelsPerMeter / worldSize}};
    }

    Point<double> worldPosFromLatLng(const LatLng& latLng) {
        double latitude = latLng.latitude();
        return {256.0 * latLng.longitude() / 180.0 + 256.0, -256.0 * latitude / 85.0 + 256.0f};
    }

    void render(const mbgl::style::CustomLayerRenderParameters& renderParams)  override {
        MBGL_CHECK_ERROR(glUseProgram(program));

        GLfloat MVP[16];
        for (int i = 0; i < 16; i++) {
            MVP[i] = renderParams.projectionMatrix[i];
        }

        // std::cout << "width= " << renderParams.width << std::endl;
        // std::cout << "height= " << renderParams.height << std::endl;
        // std::cout << "lat= " << renderParams.latitude << std::endl;
        // std::cout << "long= " << renderParams.longitude << std::endl;
        // std::cout << "zoom= " << renderParams.zoom << std::endl;
        // std::cout << "bearing= " << renderParams.bearing << std::endl;
        // std::cout << "pitch= " << renderParams.pitch << std::endl;
        // std::cout << "fov= " << renderParams.fieldOfView << std::endl;
        // std::cout << std::endl;

        //LatLng latLong {32.0, 52.0};
        //auto merc = toMercator(latLong, 0);
        // std::cout << merc[0] << " " << merc[1] << " " << merc[2] << std::endl;

        //LatLng latLong2 {renderParams.latitude, renderParams.longitude};
        //Point<double> p = mbgl::Projection::project(latLong2, pow(2.0, renderParams.zoom));
        //auto p = mbgl::Projection::projectedMetersForLatLng(gObjectLatLng);
        //std::cout << "Proj" << p.easting() << " " << p.northing() << std::endl;

        GLfloat world[4][4];
        memset(world, 0, 4 * 4 * sizeof(float));
        world[0][0] = 100;
        world[1][1] = -100;
        world[2][2] = 100;
        world[3][3] = 1;

        auto worldPos = worldPosFromLatLng(gObjectLatLng);
        //std::cout << "World " << worldPos.x << " " << worldPos.y << std::endl;

        world[3][0] = worldPos.x * pow(2.0, renderParams.zoom);
        world[3][1] = worldPos.y * pow(2.0, renderParams.zoom);
        world[3][2] = 0;

        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_WORLD, 1, GL_FALSE, (GLfloat*)world));
        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_MVP, 1, GL_FALSE, MVP));

        static auto t0 = std::chrono::system_clock::now();
        auto t1 = std::chrono::system_clock::now();
        auto dt = t1 - t0;
        float time = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() / 1000.0f;

        GLfloat rot[4][4];
        memset(rot, 0, 4 * 4 * sizeof(float));
        rot[0][0] = 1;
        rot[1][1] = 1;
        rot[2][2] = 1;
        rot[3][3] = 1;

        rot[0][0] = cos(time * 2.0f);
        rot[0][2] = -sin(time * 2.0f);
        rot[2][0] = sin(time * 2.0f);
        rot[2][2] = cos(time * 2.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glLoadMatrixf(&rot[0][0]);

        mbgl::platform::glEnable(GL_CULL_FACE);
        mbgl::platform::glCullFace(GL_BACK);  
        mbgl::platform::glEnable(GL_DEPTH_TEST);  
        mbgl::platform::glDepthFunc(GL_LESS);  
        DrawModel(model3D);

        #if 0
        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_WORLD, 1, GL_FALSE, (GLfloat*)world));
        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_MVP, 1, GL_FALSE, MVP));

        MBGL_CHECK_ERROR(glUseProgram(program));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, mybuffer));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
        mbgl::platform::glDisable(GL_CULL_FACE);  
        MBGL_CHECK_ERROR(mbgl::platform::glDrawArrays(GL_TRIANGLES, 0, 3)); 
        #endif
    }

    void contextLost() override {}

    void deinitialize() override {
        if (program) {
            // MBGL_CHECK_ERROR(glDeleteBuffers(1, &buffer));
            MBGL_CHECK_ERROR(glDetachShader(program, vertexShader));
            MBGL_CHECK_ERROR(glDetachShader(program, fragmentShader));
            MBGL_CHECK_ERROR(glDeleteShader(vertexShader));
            MBGL_CHECK_ERROR(glDeleteShader(fragmentShader));
            MBGL_CHECK_ERROR(glDeleteProgram(program));
        }
    }

    void Place3DModelAt(LatLng& latLng) {
        gObjectLatLng = latLng;
    }

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    
    std::map<int, GLuint> bufferState;
    GLProgramState programState;

    Model model3D;
    TinyGLTF loader;
    double olng = 0.0;
    double olat = 0.0;

    GLuint a_pos = 0;
    GLuint a_uv = 0;
    GLuint a_normal = 0;

    GLuint mybuffer = 0;

    GLuint u_texture = 0;
    GLuint u_MVP = 0;
    GLuint u_WORLD = 0;
    GLuint u_LOCAL = 0;
};

#pragma GCC diagnostic pop
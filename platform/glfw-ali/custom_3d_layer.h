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

class Custom3DLayer : public mbgl::style::CustomLayerHost {
public:
    typedef struct {
        std::map<std::string, GLint> attribs;
        std::map<std::string, GLint> uniforms;
    } GLProgramState;

    void initialize() override {
        {
            GLfloat triangle[] = {  -0.5, 0.0, -0.5, 
                                     0.0, 0.0, 0.5,
                                     0.5, 0.0, -0.5,};
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
                uniform mat4 MVP;
                uniform mat4 WORLD;
                varying vec2 Frag_UV;
                varying vec3 Frag_Normal;
                void main() {
                    gl_Position = MVP * WORLD * vec4(a_pos.xyz,1);
                    //gl_Position = gl_ModelViewProjectionMatrix * vec4(a_pos.xyz,1);
                    //gl_Position = gl_ModelViewMatrix * vec4(a_pos.xyz,1);
	                //Frag_Normal = normalize(mat3(MVP) * a_normal);
                    Frag_Normal = a_normal;
                    Frag_UV = a_uv;
                }
            )MBGL_SHADER";

            static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
                uniform sampler2D u_Texture;
                varying vec3 Frag_Normal;
                varying vec2 Frag_UV;
                void main()
                {
	                float n_dot_l = max(dot(Frag_Normal, vec3(0, 0, -1)), 0.2);
                    gl_FragColor = vec4(n_dot_l, n_dot_l, n_dot_l, 1.0);
                    //gl_FragColor = vec4(Frag_Normal.xyz, 1.0);
                    //gl_FragColor = vec4(1, 0, 0, 0.8);
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
            u_WORLD = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "WORLD"));
            
            std::cout << "point3" << std::endl;

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

            //modelVaoAndEbos = bindModel(model3D);
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

        // // GLint diffuseTexLoc = glGetUniformLocation(progId, "diffuseTex");
        // //GLint isCurvesLoc = glGetUniformLocation(program, "uIsCurves");

        programState.attribs["POSITION"] = vtloc;
        programState.attribs["NORMAL"] = nrmloc;
        programState.attribs["TEXCOORD_0"] = uvloc;
        // gGLProgramState.uniforms["diffuseTex"] = diffuseTexLoc;
        //gGLProgramState.uniforms["isCurvesLoc"] = isCurvesLoc;
    }

    void DrawMesh(tinygltf::Model &model, const tinygltf::Mesh &mesh) {
        if (programState.uniforms["isCurvesLoc"] >= 0) {
            glUniform1i(programState.uniforms["isCurvesLoc"], 0);
        }

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

    void build_rotmatrix(float m[4][4], const float q[4]) {
    m[0][0] = 1.0 - 2.0 * (q[1] * q[1] + q[2] * q[2]);
    m[0][1] = 2.0 * (q[0] * q[1] - q[2] * q[3]);
    m[0][2] = 2.0 * (q[2] * q[0] + q[1] * q[3]);
    m[0][3] = 0.0;

    m[1][0] = 2.0 * (q[0] * q[1] + q[2] * q[3]);
    m[1][1] = 1.0 - 2.0 * (q[2] * q[2] + q[0] * q[0]);
    m[1][2] = 2.0 * (q[1] * q[2] - q[0] * q[3]);
    m[1][3] = 0.0;

    m[2][0] = 2.0 * (q[2] * q[0] - q[1] * q[3]);
    m[2][1] = 2.0 * (q[1] * q[2] + q[0] * q[3]);
    m[2][2] = 1.0 - 2.0 * (q[1] * q[1] + q[0] * q[0]);
    m[2][3] = 0.0;

    m[3][0] = 0.0;
    m[3][1] = 0.0;
    m[3][2] = 0.0;
    m[3][3] = 1.0;
    }

    static inline float vdot(float a[3], float b[3]) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    static inline void vcross(float c[3], float a[3], float b[3]) {
        c[0] = a[1] * b[2] - a[2] * b[1];
        c[1] = a[2] * b[0] - a[0] * b[2];
        c[2] = a[0] * b[1] - a[1] * b[0];
    }

    static inline float vlength(float v[3]) {
        float len2 = vdot(v, v);
        if (std::abs(len2) > 1.0e-30) {
            return sqrt(len2);
        }
        return 0.0f;
    }

    static void vnormalize(float v[3]) {
        float len = vlength(v);
        if (std::abs(len) > 1.0e-30) {
            float inv_len = 1.0 / len;
            v[0] *= inv_len;
            v[1] *= inv_len;
            v[2] *= inv_len;
        }
    }

    void transpose(float m[4][4])
    {
        float tsrc[16];
        for (int i = 0; i < 4; i++) {
            tsrc[i] = m[i][0];
            tsrc[i + 4] = m[i][1];
            tsrc[i + 8] = m[i][2];
            tsrc[i + 12] = m[i][3];
        }
        memcpy(m, tsrc, 16 * 4);
    }

    void myPerspective(float m[4][4], float fov, float aspect, float Near, float Far)
    {
        float f = 1.0 / tan(fov / 2.0f);

        m[0][0] = f / aspect;
        m[0][1] = 0;
        m[0][2] = 0;
        m[0][3] = 0;

        m[1][0] = 0;
        m[1][1] = f;
        m[1][2] = 0;
        m[1][3] = 0;

        m[2][0] = 0;
        m[2][1] = 0;
        m[2][2] = (Far + Near) / (Near - Far);
        m[2][3] = 2.0 * Far * Near / (Near - Far);

        m[3][0] = 0;
        m[3][1] = 0;
        m[3][2] = -1;
        m[3][3] = 0;
    }

    void myLookAt(float m[4][4], float eye[3], float lookat[3], float up[3]) {
        float F[3];
        F[0] = lookat[0] - eye[0];
        F[1] = lookat[1] - eye[1];
        F[2] = lookat[2] - eye[2];
        vnormalize(F);

        float UP[3];
        UP[0] = up[0]; UP[1] = up[1]; UP[2] = up[2];
        vnormalize(UP);

        float s[3];
        vcross(s, F, UP);

        float S[3];
        S[0] = s[0]; S[1] = s[1]; S[2] = s[2];
        vnormalize(S);

        float u[3];
        vcross(u, S, F);

        m[0][0] = s[0];
        m[0][1] = s[1];
        m[0][2] = s[2];
        m[0][3] = 0;

        m[1][0] = u[0];
        m[1][1] = u[1];
        m[1][2] = u[2];
        m[1][3] = 0;

        m[2][0] = -F[0];
        m[2][1] = -F[1];
        m[2][2] = -F[2];
        m[2][3] = 0;

        //m[3][0] = -eye[0];
        //m[3][1] = -eye[1];
        //m[3][2] = -eye[2];
        //m[3][3] = 1;

        m[3][0] = 0;
        m[3][1] = 0;
        m[3][2] = 0;
        m[3][3] = 1;
    }

    void render(const mbgl::style::CustomLayerRenderParameters& renderParams)  override {
        MBGL_CHECK_ERROR(glUseProgram(program));

        GLfloat MVP[16];
        for (int i = 0; i < 16; i++) {
            MVP[i] = renderParams.projectionMatrix[i];
            //std::cout << renderParams.projectionMatrix[i] << " ,";
            // if (((i+1) & 3) == 0)
            //     std::cout << std::endl;
        }
        // std::cout << std::endl;

        std::cout << "width= " << renderParams.width << std::endl;
        std::cout << "height= " << renderParams.height << std::endl;
        std::cout << "lat= " << renderParams.latitude << std::endl;
        std::cout << "long= " << renderParams.longitude << std::endl;
        std::cout << "zoom= " << renderParams.zoom << std::endl;
        std::cout << "bearing= " << renderParams.bearing << std::endl;
        std::cout << "pitch= " << renderParams.pitch << std::endl;
        std::cout << "fov= " << renderParams.fieldOfView << std::endl;

         std::cout << std::endl;

        GLfloat world[4][4];
        memset(world, 0, 4 * 4 * 4);
        world[0][0] = 1;
        world[1][1] = -1;
        world[2][2] = 1;
        world[3][3] = 1;

        world[3][0] = 500;
        world[3][1] = 400;
        world[3][2] = 0;
        
        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_WORLD, 1, GL_FALSE, (GLfloat*)world));
        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_MVP, 1, GL_FALSE, MVP));


    {
        //float eye[3], lookat[3], up[3];
        // eye[0] = 0; eye[1] = 0; eye[2] = -5.0f;
        // lookat[0] = 0; lookat[1] = 0; lookat[2] = 0;
        //up[0] = 0; up[1] = 1; up[2] = 0;
        
        //lookat[0] = renderParams.longitude; lookat[2] = 0; lookat[1] = renderParams.latitude;
        //eye[0] = renderParams.longitude; eye[2] = 13.0f; eye[1] = renderParams.latitude;

        //lookat[0] = 0; lookat[1] = 0; lookat[2] = 0;
        //eye[0] = 0; eye[1] = 0; eye[2] = 13;

        if (0) {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            //gluPerspective(45.0, (float)w / (float)h, 0.1f, 1000.0f);

            float proj[16];
            mbgl::platform::glGetFloatv(GL_PROJECTION_MATRIX, proj);

            float proj2[4][4];
            myPerspective(proj2, renderParams.fieldOfView, (float)renderParams.width / (float)renderParams.height, 0.1f, 1000.0f);
            transpose(proj2);

            glMultMatrixf((GLfloat*)proj2);

            //glMatrixMode(GL_MODELVIEW);
            //glLoadIdentity();
        }

        // camera(define it in projection matrix)
        //glMatrixMode(GL_PROJECTION);
        //glPushMatrix();
        //gluLookAt(eye[0], eye[1], eye[2], lookat[0], lookat[1], lookat[2], up[0], up[1], up[2]);

        //float view2[4][4];
        //myLookAt(view2, eye, lookat, up);
        // glMultMatrixf(&view2[0][0]);
        // glTranslated(-eye[0], -eye[1], -eye[2]);

        // GLfloat view[16];
        // mbgl::platform::glGetFloatv(GL_PROJECTION_MATRIX, view);

        // glMatrixMode(GL_MODELVIEW);
        // glLoadIdentity();
        //glMultMatrixf(MVP);

        // GLfloat mat[4][4];
        // build_rotmatrix(mat, curr_quat);
        // glMultMatrixf(&mat[0][0]);

        //float scale = 1.0;
        //glScalef(scale, scale, scale);

        //glTranslatef(520, 320, 0);

        //GLfloat mvp[16];
        //mbgl::platform::glGetFloatv(GL_PROJECTION_MATRIX, mvp);

        DrawModel(model3D);

        //glMatrixMode(GL_PROJECTION);
        //glPopMatrix();
    }

    /*
    // glMatrixMode(GL_PROJECTION);
    // glLoadIdentity();

    // glMatrixMode(GL_MODELVIEW);
    // glLoadIdentity();
    // glMultMatrixf(MVP);

    //glScalef(1000.0, 1000.0, 1000.0);
    

    GLfloat view[4][4];
    //build_rotmatrix(mat, curr_quat);

    float eye[3], lookat[3], up[3];
    eye[0] = 0; eye[1] = 0; eye[2] = -5.0f;
    lookat[0] = 0; lookat[1] = 0; lookat[2] = 0;
    up[0] = 0; up[1] = 1; up[2] = 0;
    
    lookat[0] = renderParams.longitude; lookat[2] = renderParams.latitude; lookat[1] = 0;
    eye[0] = renderParams.longitude; eye[2] = renderParams.latitude; eye[1] = 10.0f;

    LookAt(view, eye, lookat, up);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMultMatrixf(MVP);
    //glLoadTransposeMatrixf(MVP);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    //glMultMatrixf(&world[0][0]);
    //glMultMatrixf(&view[0][0]);

    float scale = 100.0f;
    glScalef(scale, scale, scale);
    
        GLfloat mat[4][4];
        build_rotmatrix(mat, curr_quat);

        // camera(define it in projection matrix)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        
        float eye[3], lookat[3], up[3];
        eye[0] = 0; eye[1] = 0; eye[2] = -5.0f;
        lookat[0] = 0; lookat[1] = 0; lookat[2] = 0;
        up[0] = 0; up[1] = 1; up[2] = 0;
        
        LookAt(mat, eye, lookat, up);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMultMatrixf(&mat[0][0]);

        float scale = 1.0f;
        glScalef(scale, scale, scale); 

        ////DrawModel(model3D);

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        float proj[4][4];
        memset(proj, 0, 4 * 4 * 4);
        // proj[0][0] = 1;
        // proj[1][1] = 1;
        // proj[2][2] = 1;
        // proj[3][3] = 1;
        PerspectiveRH(proj, renderParams.fieldOfView, (float)renderParams.width / (float)renderParams.height, 0.01, 1000);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        //glMultMatrixf((GLfloat*)proj);
        //glMultMatrixf(MVP);
        //glLoadTransposeMatrixf(MVP);

        mbgl::matrix 
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMultMatrixf((GLfloat*)view);
        //glMultMatrixf((GLfloat*)world);
        //glLoadTransposeMatrixf((GLfloat*)world);
        //glMultMatrixf(MVP);
        //glLoadTransposeMatrixf(MVP);
        //scale = 100.0f;
        //glScalef(scale, scale, scale); 

        MBGL_CHECK_ERROR(glUseProgram(program));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, mybuffer));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos));
        MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
        mbgl::platform::glDisable(GL_CULL_FACE);  
        MBGL_CHECK_ERROR(mbgl::platform::glDrawArrays(GL_TRIANGLES, 0, 3)); */
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

    void Create3DModelAt(LatLng&) {
    }

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    
    std::map<int, GLuint> bufferState;
    GLProgramState programState;

    Model model3D;
    TinyGLTF loader;

    GLuint a_pos = 0;
    GLuint a_uv = 0;
    GLuint a_normal = 0;

    GLuint mybuffer = 0;

    GLuint u_texture = 0;
    GLuint u_MVP = 0;
    GLuint u_WORLD = 0;
    float curr_quat[4];
    float prev_quat[4];
};

#pragma GCC diagnostic pop
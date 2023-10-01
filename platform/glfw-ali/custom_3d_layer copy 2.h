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
    struct GLProgramState {
        std::map<std::string, GLint> attribs;
        std::map<std::string, GLint> uniforms;
    };

    struct ModelDesc {
        uint32_t id;
        Model* model3D;
        std::map<int, GLuint> bufferStates;
    };

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
                    //gl_Position = MVP * vec4(a_pos.xyz,1);
                    gl_Position = gl_ModelViewProjectionMatrix * vec4(a_pos.xyz,1);
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
            
            /*a_pos = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_pos"));
            a_normal = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_normal"));
            a_uv = MBGL_CHECK_ERROR(mbgl::platform::glGetAttribLocation(program, "a_uv"));

            u_texture = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "u_Texture"));*/
            u_MVP = MBGL_CHECK_ERROR(mbgl::platform::glGetUniformLocation(program, "MVP"));
            
            std::cout << "point3" << std::endl;

            // Restore modified GL state
            MBGL_CHECK_ERROR(mbgl::platform::glBindTexture(GL_TEXTURE_2D, last_texture));
            MBGL_CHECK_ERROR(mbgl::platform::glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer));

            LatLng latLng{};
            Create3DModelAt(latLng);

            glUseProgram(program);
            GLint vtloc = glGetAttribLocation(program, "a_pos");
            GLint nrmloc = glGetAttribLocation(program, "a_normal");
            GLint uvloc = glGetAttribLocation(program, "a_uv");

            // GLint diffuseTexLoc = glGetUniformLocation(progId, "diffuseTex");
            //GLint isCurvesLoc = glGetUniformLocation(program, "uIsCurves");

            programState.attribs["POSITION"] = vtloc;
            programState.attribs["NORMAL"] = nrmloc;
            programState.attribs["TEXCOORD_0"] = uvloc;
            // gGLProgramState.uniforms["diffuseTex"] = diffuseTexLoc;
            //gGLProgramState.uniforms["isCurvesLoc"] = isCurvesLoc;

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

static size_t ComponentTypeByteSize(int type) {
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

    void SetupMeshState(ModelDesc& model) {
        for (size_t i = 0; i < model.model3D->bufferViews.size(); i++) {
        const tinygltf::BufferView &bufferView = model.model3D->bufferViews[i];
        if (bufferView.target == 0) {
            std::cout << "WARN: bufferView.target is zero" << std::endl;
            continue;  // Unsupported bufferView.
        }

        int sparse_accessor = -1;
        for (size_t a_i = 0; a_i < model.model3D->accessors.size(); ++a_i) {
            const auto &accessor = model.model3D->accessors[a_i];
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

        const tinygltf::Buffer &buffer = model.model3D->buffers[bufferView.buffer];
        GLuint vb;
        std::cout << "Here 5" << std::endl;
        glGenBuffers(1, &vb);
        glBindBuffer(bufferView.target, vb);
        std::cout << "buffer.size= " << buffer.data.size()
                    << ", byteOffset = " << bufferView.byteOffset << std::endl;

        if (sparse_accessor < 0) {
            std::cout << "Here 6" << std::endl;
            glBufferData(bufferView.target, bufferView.byteLength,
                        &buffer.data.at(0) + bufferView.byteOffset,
                        GL_STATIC_DRAW);
        }
        else {
            std::cout << "Here 7" << std::endl;
            const auto accessor = model.model3D->accessors[sparse_accessor];
            // copy the buffer to a temporary one for sparse patching
            unsigned char *tmp_buffer = new unsigned char[bufferView.byteLength];
            memcpy(tmp_buffer, buffer.data.data() + bufferView.byteOffset,
                bufferView.byteLength);

            const size_t size_of_object_in_buffer =
                ComponentTypeByteSize(accessor.componentType);
            const size_t size_of_sparse_indices =
                ComponentTypeByteSize(accessor.sparse.indices.componentType);

            const auto &indices_buffer_view =
                model.model3D->bufferViews[accessor.sparse.indices.bufferView];
            const auto &indices_buffer = model.model3D->buffers[indices_buffer_view.buffer];

            const auto &values_buffer_view =
                model.model3D->bufferViews[accessor.sparse.values.bufferView];
            const auto &values_buffer = model.model3D->buffers[values_buffer_view.buffer];

            for (size_t sparse_index = 0; sparse_index < static_cast<size_t>(accessor.sparse.count); ++sparse_index) {
            int index = 0;
            // std::cout << "accessor.sparse.indices.componentType = " <<
            // accessor.sparse.indices.componentType << std::endl;
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
            std::cout << "updating sparse data at index  : " << index
                        << std::endl;
            // index is now the target of the sparse index to patch in
            const unsigned char *read_from =
                values_buffer.data.data() +
                (values_buffer_view.byteOffset +
                accessor.sparse.values.byteOffset) +
                (sparse_index * (size_of_object_in_buffer * accessor.type));

            /*
            std::cout << ((float*)read_from)[0] << "\n";
            std::cout << ((float*)read_from)[1] << "\n";
            std::cout << ((float*)read_from)[2] << "\n";
            */

            unsigned char *write_to =
                tmp_buffer + index * (size_of_object_in_buffer * accessor.type);

            memcpy(write_to, read_from, size_of_object_in_buffer * accessor.type);
            }

            // debug:
            /*for(size_t p = 0; p < bufferView.byteLength/sizeof(float); p++)
            {
            float* b = (float*)tmp_buffer;
            std::cout << "modified_buffer [" << p << "] = " << b[p] << '\n';
            }*/

            std::cout << "Here 8" << std::endl;
            glBufferData(bufferView.target, bufferView.byteLength, tmp_buffer,
                        GL_STATIC_DRAW);
            delete[] tmp_buffer;
        }
        std::cout << "Here 9" << std::endl;
        glBindBuffer(bufferView.target, 0);
        std::cout << "Here 10" << std::endl;

        model.bufferStates[i] = vb;
        }
        std::cout << "Here 11" << std::endl;
    }

    void DrawMesh(Model &model3D, std::map<int, GLuint> &bufferStates, const tinygltf::Mesh &mesh) {
    //// Skip curves primitive.
    // if (gCurvesMesh.find(mesh.name) != gCurvesMesh.end()) {
    //  return;
    //}

    // if (gGLProgramState.uniforms["diffuseTex"] >= 0) {
    //  glUniform1i(gGLProgramState.uniforms["diffuseTex"], 0);  // TEXTURE0
    //}

    if (programState.uniforms["isCurvesLoc"] >= 0) {
        glUniform1i(programState.uniforms["isCurvesLoc"], 0);
    }

    for (size_t i = 0; i < mesh.primitives.size(); i++) {
        const tinygltf::Primitive &primitive = mesh.primitives[i];

        if (primitive.indices < 0) return;

        // Assume TEXTURE_2D target for the texture object.
        // glBindTexture(GL_TEXTURE_2D, gMeshState[mesh.name].diffuseTex[i]);

        std::map<std::string, int>::const_iterator it(primitive.attributes.begin());
        std::map<std::string, int>::const_iterator itEnd(
            primitive.attributes.end());

        for (; it != itEnd; it++) {
        assert(it->second >= 0);
        const tinygltf::Accessor &accessor = model3D.accessors[it->second];
        glBindBuffer(GL_ARRAY_BUFFER, bufferStates[accessor.bufferView]);
        //CheckErrors("bind buffer");
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
            int byteStride =
                accessor.ByteStride(model3D.bufferViews[accessor.bufferView]);
            assert(byteStride != -1);
            glVertexAttribPointer(programState.attribs[it->first], size,
                                    accessor.componentType,
                                    accessor.normalized ? GL_TRUE : GL_FALSE,
                                    byteStride, BUFFER_OFFSET(accessor.byteOffset));
            //CheckErrors("vertex attrib pointer");
            glEnableVertexAttribArray(programState.attribs[it->first]);
            //CheckErrors("enable vertex attrib array");
            }
        }
        }

        const tinygltf::Accessor &indexAccessor =
            model3D.accessors[primitive.indices];
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                    bufferStates[indexAccessor.bufferView]);
        //CheckErrors("bind buffer");
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
        mbgl::platform::glDrawElements(mode, indexAccessor.count, indexAccessor.componentType,
                    BUFFER_OFFSET(indexAccessor.byteOffset));
        //CheckErrors("draw elements");

        {
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
    }

    static void QuatToAngleAxis(const std::vector<double> quaternion,
                    double &outAngleDegrees,
                    double *axis) {
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

    void DrawNode(tinygltf::Model &model, std::map<int, GLuint> &bufferStates, const tinygltf::Node &node) {
    // Apply xform
        glPushMatrix();
        if (node.matrix.size() == 16) {
            // Use `matrix' attribute
            glMultMatrixd(node.matrix.data());
        } else {
            // Assume Trans x Rotate x Scale order
            if (node.translation.size() == 3) {
            glTranslated(node.translation[0], node.translation[1],
                        node.translation[2]);
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

        // std::cout << "node " << node.name << ", Meshes " << node.meshes.size() <<
        // std::endl;

        // std::cout << it->first << std::endl;
        // FIXME(syoyo): Refactor.
        // DrawCurves(scene, it->second);
        if (node.mesh > -1) {
            assert(node.mesh < static_cast<int>(model.meshes.size()));
            DrawMesh(model, bufferStates, model.meshes[node.mesh]);
        }

        // Draw child nodes.
        for (size_t i = 0; i < node.children.size(); i++) {
            assert(node.children[i] < static_cast<int>(model.nodes.size()));
            DrawNode(model, bufferStates, model.nodes[node.children[i]]);
        }

        glPopMatrix();
    }

    void drawModel(ModelDesc& model) {
        assert(model.model3D->scenes.size() > 0);
        int scene_to_display = model.model3D->defaultScene > -1 ? model.model3D->defaultScene : 0;
        const tinygltf::Scene &scene = model.model3D->scenes[scene_to_display];
        for (size_t i = 0; i < scene.nodes.size(); i++) {
            DrawNode(*model.model3D, model.bufferStates, model.model3D->nodes[scene.nodes[i]]);
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

    void LookAt(float m[4][4], float eye[3], float lookat[3], float _up[3]) {

    float u[3], v[3];
    float look[3];
    look[0] = lookat[0] - eye[0];
    look[1] = lookat[1] - eye[1];
    look[2] = lookat[2] - eye[2];
    vnormalize(look);

    vcross(u, look, _up);
    vnormalize(u);

    vcross(v, u, look);
    vnormalize(v);

    #if 0
    m[0][0] = u[0];
    m[0][1] = v[0];
    m[0][2] = -look[0];
    m[0][3] = 0.0;

    m[1][0] = u[1];
    m[1][1] = v[1];
    m[1][2] = -look[1];
    m[1][3] = 0.0;

    m[2][0] = u[2];
    m[2][1] = v[2];
    m[2][2] = -look[2];
    m[2][3] = 0.0;

    m[3][0] = eye[0];
    m[3][1] = eye[1];
    m[3][2] = eye[2];
    m[3][3] = 1.0;
    #else
    m[0][0] = u[0];
    m[1][0] = v[0];
    m[2][0] = -look[0];
    m[3][0] = eye[0];

    m[0][1] = u[1];
    m[1][1] = v[1];
    m[2][1] = -look[1];
    m[3][1] = eye[1];

    m[0][2] = u[2];
    m[1][2] = v[2];
    m[2][2] = -look[2];
    m[3][2] = eye[2];

    m[0][3] = 0.0;
    m[1][3] = 0.0;
    m[2][3] = 0.0;
    m[3][3] = 1.0;

    #endif
    }

    void render(const mbgl::style::CustomLayerRenderParameters& renderParams)  override {
        return;
        MBGL_CHECK_ERROR(glUseProgram(program));

        float MVP[16];
        for (int i = 0; i < 16; i++) {
            MVP[i] = renderParams.projectionMatrix[i];
            //std::cout << renderParams.projectionMatrix[i] << " ,";
            // if (((i+1) & 3) == 0)
            //     std::cout << std::endl;
        }
        // std::cout << std::endl;

        // std::cout << "width= " << renderParams.width << std::endl;
        // std::cout << "height= " << renderParams.height << std::endl;
        // std::cout << "lat= " << renderParams.latitude << std::endl;
        // std::cout << "long= " << renderParams.longitude << std::endl;
        // std::cout << "zoom= " << renderParams.zoom << std::endl;
        // std::cout << "bearing= " << renderParams.bearing << std::endl;
        // std::cout << "pitch= " << renderParams.pitch << std::endl;
        // std::cout << "fov= " << renderParams.fieldOfView << std::endl;

        // std::cout << std::endl;

        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_MVP, 1, GL_FALSE, MVP));

        GLfloat mat[4][4];
        //build_rotmatrix(mat, curr_quat);

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
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

        /*GLfloat mat[4][4];
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
        glScalef(scale, scale, scale); */

        for (auto model : models) {
            drawModel(*model);
        }

        /*glMatrixMode(GL_PROJECTION);
        glPopMatrix();*/
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
        std::cout << "Create3DModel" << std::endl;

        ModelDesc* model = new ModelDesc;
        model->model3D = new Model;
        TinyGLTF loader;
        std::string err;
        std::string warn;

        bool ret = loader.LoadASCIIFromFile(model->model3D, &err, &warn, "/tmp/Duck.gltf");
        std::cout << "Here 1" << std::endl;

        if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
        }

        if (!err.empty()) {
            printf("Err: %s\n", err.c_str());
        }
        assert(ret);

        std::cout << "Here 2" << std::endl;
        SetupMeshState(*model);
        std::cout << "Here 3" << std::endl;

        models.push_back(model);
        std::cout << "Here 4" << std::endl;
    }

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    
    std::vector<ModelDesc*> models;

    //std::map<int, GLuint> bufferState;
    GLProgramState programState;

    //Model model3D;
    //TinyGLTF loader;

    GLuint a_pos = 0;
    GLuint a_uv = 0;
    GLuint a_normal = 0;

    GLuint u_texture = 0;
    GLuint u_MVP = 0;
    float curr_quat[4];
    float prev_quat[4];
};

#pragma GCC diagnostic pop
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_search_module(GLFW glfw3-ali REQUIRED)

add_executable(
    mbgl-glfw-ali
    ${PROJECT_SOURCE_DIR}/platform/glfw-ali/main.cpp
    ${PROJECT_SOURCE_DIR}/platform/glfw-ali/glfw_view.cpp
    ${PROJECT_SOURCE_DIR}/platform/glfw-ali/glfw_renderer_frontend.cpp
    ${PROJECT_SOURCE_DIR}/platform/glfw-ali/settings_json.cpp
    ${PROJECT_SOURCE_DIR}/platform/glfw-ali/test_writer.cpp
    ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/map/map_snapshotter.cpp
)

set_property(
    SOURCE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/glfw_view.cpp
    PROPERTY COMPILE_DEFINITIONS MAPBOX_PUCK_ASSETS_PATH=\"${PROJECT_SOURCE_DIR}/platform/glfw-ali/assets/\"
)

if(MBGL_WITH_OPENGL)
    target_sources(
        mbgl-glfw-ali
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/glfw_gl_backend.cpp
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/third-party/imgui/imgui.cpp
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/third-party/imgui/imgui_widgets.cpp
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/third-party/imgui/imgui_tables.cpp
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/third-party/imgui/imgui_draw.cpp
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/third-party/imgui/imgui_demo.cpp
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/imgui_impl_mbgl.cpp
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/tinygltf/tiny_gltf.h
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/tinygltf/stb_image.h
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/tinygltf/stb_image.h
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/tinygltf/stb_image_write.h
        PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/tinygltf/json.hpp
        )
    target_compile_definitions(
        mbgl-glfw-ali
        PRIVATE MBGL_RENDER_BACKEND_OPENGL=1
    )
endif()

target_include_directories(
    mbgl-glfw-ali
    PRIVATE
        ${GLFW_INCLUDE_DIRS}
        # For /platform/default/src/mbgl/map/map_snapshotter.hpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src
    PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/third-party/imgui
    PRIVATE ${PROJECT_SOURCE_DIR}/platform/glfw-ali/third-party/tinygltf
)

# Use target_link_directories when we move away from CMake 3.10.
target_link_libraries(
    mbgl-glfw-ali
    PRIVATE $<$<BOOL:${GLFW_LIBRARY_DIRS}>:-L${GLFW_LIBRARY_DIRS}>
)

target_link_libraries(
    mbgl-glfw-ali
    PRIVATE
        ${GLFW_LIBRARIES}
        Mapbox::Base::Extras::args
        Mapbox::Base::Extras::filesystem
        Mapbox::Base::Extras::rapidjson
        Mapbox::Map
        mbgl-compiler-options
        Mapbox::Base::cheap-ruler-cpp
)

if(MBGL_WITH_OPENGL)
    target_link_libraries(
        mbgl-glfw-ali
        PRIVATE OpenGL::GL
    )
endif()

set_property(TARGET mbgl-glfw-ali PROPERTY FOLDER Executables)

install(TARGETS mbgl-glfw-ali RUNTIME DESTINATION bin)

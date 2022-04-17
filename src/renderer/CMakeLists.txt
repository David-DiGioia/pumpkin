set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/public/vulkan_renderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/public/mesh_types.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/swapchain.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/swapchain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/mesh.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/mesh.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/memory_allocator.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/memory_allocator.cpp"
)

add_library(Renderer "${SOURCES}")

target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")

target_link_libraries(Renderer PRIVATE Common)

macro(COMPILE_SHADER shader_name)
    SET(GLSL "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_name}")
    SET(SPIRV "${PROJECT_BINARY_DIR}/src/shaders/${shader_name}.spv")

    add_custom_command(
        PRE_BUILD
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${GLSL} -o ${SPIRV} --target-env spirv1.6
        DEPENDS ${GLSL} # If GLSL changes, this command will be run to compile them again.
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    list(APPEND GLSL_FILES ${GLSL})
endmacro()

COMPILE_SHADER("default.frag")
COMPILE_SHADER("default.vert")

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
    SOURCES ${GLSL_FILES}
)

add_dependencies(Renderer Shaders)

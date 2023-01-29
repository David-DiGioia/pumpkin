set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/public/vulkan_renderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_backend.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_backend.cpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/descriptor_set.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/descriptor_set.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer_types.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer_types.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ray_tracing.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ray_tracing.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer_constants.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/render_object.h"
)

add_library(Renderer "${SOURCES}")

target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")

target_link_libraries(Renderer PRIVATE Common)

macro(COMPILE_SHADER shader_name)
    SET(GLSL "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_name}")
    SET(SPIRV "${PROJECT_BINARY_DIR}/src/shaders/${shader_name}.spv")

    # The variable CMAKE_BUILD_TYPE is only set for single-configuration setups which visual studio is not (since you can select build type in VS).
    # So we use a generator expression to determine if we pass -g to the glsl compiler.
    add_custom_command(
        PRE_BUILD
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${GLSL} -o ${SPIRV} --target-env spirv1.6 $<$<CONFIG:Debug>:-g>
        DEPENDS ${GLSL} # If GLSL changes, this command will be run to compile them again.
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    list(APPEND GLSL_FILES ${GLSL})
endmacro()

macro(HEADER_SHADER shader_name)
    SET(GLSL "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_name}")
    list(APPEND GLSL_FILES ${GLSL})
endmacro()

COMPILE_SHADER("default.frag")
COMPILE_SHADER("default.vert")
COMPILE_SHADER("default.rgen")
COMPILE_SHADER("default.rmiss")
COMPILE_SHADER("shadow.rmiss")
COMPILE_SHADER("default.rchit")
HEADER_SHADER("common.glsl")

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
    SOURCES ${GLSL_FILES}
)

add_dependencies(Renderer Shaders)

if (EDITOR_ENABLED_OPTION)
    target_compile_definitions(Renderer PRIVATE EDITOR_ENABLED=1)
endif()

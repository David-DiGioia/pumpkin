set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/public/vulkan_renderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/swapchain.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/swapchain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/default.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/default.frag"
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
    SET(SPIRV "${PROJECT_BINARY_DIR}/shaders/${shader_name}.spv")

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND glslangvalidator.exe -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL} # If GLSL changes, this command will be run to compile them again.
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endmacro()

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(Renderer Shaders)

#add_custom_command(TARGET Renderer PRE_BUILD
#    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Renderer>/shaders/"
#    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:Renderer>/shaders/"
#)

COMPILE_SHADER("default.frag")
COMPILE_SHADER("default.vert")

MESSAGE(${SPIRV_BINARY_FILES})


set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_ray_tracing.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_ray_tracing.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/project_config.h"
)

add_executable(raytracing "${SOURCES}")

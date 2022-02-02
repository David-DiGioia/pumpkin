cmake_minimum_required (VERSION 3.8)

project ("raytracing" VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable (raytracing
    "src/vulkan_ray_tracing.cpp"
    "include/vulkan_ray_tracing.h"
    "include/project_config.h"
)

target_include_directories(raytracing PUBLIC "${PROJECT_SOURCE_DIR}/include")

configure_file(
    "${PROJECT_SOURCE_DIR}/include/project_config.h.in"
    "${PROJECT_SOURCE_DIR}/include/project_config.h"
)

# Set rayracing as startup project.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT raytracing)

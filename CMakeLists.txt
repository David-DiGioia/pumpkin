cmake_minimum_required (VERSION 3.8)

project ("VRT" VERSION 1.0)

add_executable (VRT "src/vulkan-ray-tracing.cpp" "src/vulkan-ray-tracing.h")

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VRT)

configure_file("project-config.h.in" "project-config.h")

target_include_directories(VRT PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          )

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

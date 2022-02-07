set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/project_config.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/log.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/log.h"
)

add_executable(raytracing "${SOURCES}")

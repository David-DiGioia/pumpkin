set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/public/vulkan_renderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vulkan_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.cpp"
)

add_library(Renderer "${SOURCES}")

target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")

target_link_libraries(Renderer PRIVATE Common)

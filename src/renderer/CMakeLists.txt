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
)

add_library(Renderer "${SOURCES}")

target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")

target_link_libraries(Renderer PRIVATE Common)

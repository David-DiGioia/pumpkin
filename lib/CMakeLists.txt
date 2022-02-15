# If Vulkan SDK is installed, this command will make the Vulkan symbols
# used below available to us. Use cmake gui to see all the available symbols.
find_package(Vulkan REQUIRED FATAL_ERROR)

add_subdirectory(volk)

target_include_directories (Renderer PRIVATE
    "${Vulkan_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.3.6.bin.WIN64/include"
)

# Public, so will be accessible from main app too.
target_include_directories (Renderer PUBLIC
    # Header only
    "${CMAKE_CURRENT_SOURCE_DIR}/glm" 
)

target_link_libraries(Renderer PRIVATE
    "${Vulkan_LIBRARY}"
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.3.6.bin.WIN64/lib-vc2022/glfw3.lib"
    volk_headers
)


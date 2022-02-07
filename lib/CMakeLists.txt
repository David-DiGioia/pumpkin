# If Vulkan SDK is installed, this command will make the Vulkan symbols
# used below available to us. Use cmake gui to see all the available symbols.
find_package(Vulkan REQUIRED FATAL_ERROR)

add_subdirectory(volk)

target_include_directories (raytracing PUBLIC
    "${Vulkan_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.3.6.bin.WIN64/include"

    # Header only
    "${CMAKE_CURRENT_SOURCE_DIR}/glm" 
)

target_link_libraries(raytracing
    "${Vulkan_LIBRARY}"
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.3.6.bin.WIN64/lib-vc2022/glfw3.lib"
    volk_headers
)


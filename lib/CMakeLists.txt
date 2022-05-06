# If Vulkan SDK is installed, this command will make the Vulkan symbols
# used below available to us. Use cmake gui to see all the available symbols.
find_package(Vulkan REQUIRED FATAL_ERROR)

add_subdirectory(volk)
add_subdirectory(imgui)

# System keyword ignores warnings from these libraries.
target_include_directories (Renderer SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.3.6.bin.WIN64/include"
    "${Vulkan_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/include"

    # Header only.
    "${CMAKE_CURRENT_SOURCE_DIR}/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/tinygltf"
)

set(SFML_LIBS_D
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/sfml-audio-s-d.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/sfml-system-s-d.lib"
)

set(SFML_LIBS_R
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/sfml-audio-s.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/sfml-system-s.lib"
)

set(SFML_LIBS
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/openal32.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/flac.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/vorbisenc.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/vorbisfile.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/vorbis.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/lib/ogg.lib"
)

# Link libraries only in debug build.
target_link_libraries(Renderer PUBLIC debug
    "${SFML_LIBS_D}"
)

# Link libraries only in release build.
target_link_libraries(Renderer PUBLIC optimized
    "${SFML_LIBS_R}"
)

# Link libraries in all builds.
target_link_libraries(Renderer PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw-3.3.6.bin.WIN64/lib-vc2022/glfw3.lib"
    "${Vulkan_LIBRARY}"
    "${SFML_LIBS}"
    volk_headers
    Imgui
)

target_compile_definitions(Renderer PUBLIC
    "SFML_STATIC"
)

# Copy the dll that SFML is dependent on into the bin directories of the exe projects.
set(SFML_NEEDED_DLL "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/bin/openal32.dll")
file(COPY "${SFML_NEEDED_DLL}" DESTINATION "${Editor_BINARY_DIR}")
file(COPY "${SFML_NEEDED_DLL}" DESTINATION "${Editor_BINARY_DIR_D}")
file(COPY "${SFML_NEEDED_DLL}" DESTINATION "${Bootstrap_BINARY_DIR}")
file(COPY "${SFML_NEEDED_DLL}" DESTINATION "${Bootstrap_BINARY_DIR_D}")

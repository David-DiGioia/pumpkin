# Copy the imgui font to the build directory.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/persans.ttf"
    "${PROJECT_BINARY_DIR}/src/editor/persans.ttf"
    COPYONLY
)

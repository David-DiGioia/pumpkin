set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/project_config.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

add_executable(Pumpkin "${SOURCES}")

target_include_directories(Pumpkin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(renderer)
add_subdirectory(common)

target_link_libraries(Pumpkin
    Renderer
    Common
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/project_config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/project_config.h"
)

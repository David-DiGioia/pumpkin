set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/editor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/editor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gui.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/gui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/input.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/input.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/camera_controller.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/camera_controller.cpp"
)

add_executable(Editor "${SOURCES}")

target_include_directories(Editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Editor
    Pumpkin
    Common
)

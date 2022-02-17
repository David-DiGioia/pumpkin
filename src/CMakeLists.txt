set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.cpp"
)

add_executable(Pumpkin "${SOURCES}")

target_include_directories(Pumpkin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(renderer)
add_subdirectory(common)

target_link_libraries(Pumpkin
    Renderer
    Common
)

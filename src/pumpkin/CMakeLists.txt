set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.cpp"
)

add_library(Pumpkin "${SOURCES}")

target_include_directories(Pumpkin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Pumpkin PUBLIC
    Renderer
    Common
)

target_compile_definitions(Pumpkin PRIVATE EDITOR_ENABLED=1)

# Bootstrap version (editor disabled) of Pumpkin target.

add_library(Pumpkin_B "${SOURCES}")

target_include_directories(Pumpkin_B PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Pumpkin_B PUBLIC
    Renderer_B
    Common
)

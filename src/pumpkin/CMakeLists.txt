set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/audio.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/audio.cpp"
)

add_library(Pumpkin "${SOURCES}")

target_include_directories(Pumpkin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Pumpkin PUBLIC
    Renderer
    Common
)

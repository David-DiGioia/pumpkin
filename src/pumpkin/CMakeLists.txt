set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/particles.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/particles.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/mpm.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/mpm.cpp"

)

add_library(Pumpkin "${SOURCES}")

target_include_directories(Pumpkin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Pumpkin PUBLIC
    Renderer
    Common
)

if (EDITOR_ENABLED_OPTION)
    target_compile_definitions(Pumpkin PUBLIC EDITOR_ENABLED=1)
endif()

set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/voxels.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/voxels.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/xpbd.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/xpbd.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/rigid_body.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/rigid_body.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/physics.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/physics.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/constraint.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/terrain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/terrain.h"
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

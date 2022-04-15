set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/pumpkin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene.cpp"
)

add_executable(Pumpkin "${SOURCES}")

target_include_directories(Pumpkin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(renderer)
add_subdirectory(common)

target_link_libraries(Pumpkin
    Renderer
    Common
)

# Disable unscoped enum warning.
if (MSVC)
    set_target_properties(Pumpkin PROPERTIES COMPILE_FLAGS "/wd26812")
    set_target_properties(Renderer PROPERTIES COMPILE_FLAGS "/wd26812")
    set_target_properties(Common PROPERTIES COMPILE_FLAGS "/wd26812")
endif()

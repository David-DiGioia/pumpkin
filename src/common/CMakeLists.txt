set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/public/cmake_config.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/public/project_config.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/public/logger.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/public/string_util.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/string_util.cpp"
)

add_library(Common "${SOURCES}")

target_include_directories(Common PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(Common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/public/cmake_config.h"
)

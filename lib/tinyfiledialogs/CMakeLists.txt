set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tinyfiledialogs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tinyfiledialogs.c"
)

add_library(Tinyfiledialogs "${SOURCES}")

target_include_directories(Tinyfiledialogs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

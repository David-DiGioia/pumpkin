set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/TracyClient.cpp"
)

add_library(Tracy "${SOURCES}")

target_include_directories(Tracy PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(Tracy PUBLIC TRACY_ENABLE)
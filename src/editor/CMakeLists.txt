set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

add_executable(Editor "${SOURCES}")

target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public")

target_link_libraries(Editor
    Pumpkin
    Common
)

set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

add_executable(Bootstrap "${SOURCES}")

target_include_directories(Bootstrap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Bootstrap
    Pumpkin
)

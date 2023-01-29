add_subdirectory(renderer)
add_subdirectory(common)
add_subdirectory(pumpkin)
add_subdirectory(editor)
add_subdirectory(bootstrap)

# Disable unscoped enum warning.
if (MSVC)
    set_target_properties(Pumpkin PROPERTIES COMPILE_FLAGS "/wd26812")
    set_target_properties(Renderer PROPERTIES COMPILE_FLAGS "/wd26812")
    set_target_properties(Common PROPERTIES COMPILE_FLAGS "/wd26812")
    set_target_properties(Editor PROPERTIES COMPILE_FLAGS "/wd26812")
    set_target_properties(Bootstrap PROPERTIES COMPILE_FLAGS "/wd26812")
endif()

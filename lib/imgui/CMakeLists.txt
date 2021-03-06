set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/imconfig.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_internal.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imstb_rectpack.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imstb_textedit.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imstb_truetype.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_glfw.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_vulkan.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_win32.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/implot/implot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/implot/implot.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/implot/implot_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/implot/implot_internal.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/implot/implot_items.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/curve_editor/curve_v122.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imsequencer/imsequencer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imsequencer/imsequencer.cpp"
)

add_library(Imgui "${SOURCES}")

target_include_directories(Imgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../glfw-3.3.6.bin.WIN64/include"
    "${Vulkan_INCLUDE_DIR}"
)

target_link_libraries(Imgui PUBLIC
    "${Vulkan_LIBRARY}"
    volk_headers
)

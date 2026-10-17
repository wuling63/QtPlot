# qtplot_c.dll：C 介面，給 C#、Python 這類只認 C ABI 的語言用。
#
# qtplot 本身維持靜態函式庫，整個連進這顆 DLL 裡 —— 對外只露出 qtplot_c.h 的 C 函式，
# 不匯出任何 C++ 類別，所以不會有「DLL 兩邊 STL／Qt 版本對不上」的問題。
add_library(qtplot_c SHARED
    qtplot_c.h
    qtplot_c.cpp
)

target_include_directories(qtplot_c PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtplot_c PRIVATE qtplot)
target_compile_definitions(qtplot_c
    PRIVATE QTPLOT_C_BUILD QTPLOT_C_VERSION="${PROJECT_VERSION}")
# 非 Windows 平台只匯出標了 QP_API 的符號
set_target_properties(qtplot_c PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

add_executable(QtPlotDemo WIN32
    main.cpp
    MainWindow.h   MainWindow.cpp
    Recipe.h       Recipe.cpp
    SharedAxesWindow.h  SharedAxesWindow.cpp
    recipes/Quickstart.cpp
    recipes/PlotTypes.cpp
    recipes/AxesLayout.cpp
    recipes/Layout.cpp
    recipes/MoreTypes.cpp
    recipes/Diagnostics.cpp
    recipes/Styling.cpp
    recipes/Polar.cpp
    recipes/Statistics.cpp
    recipes/Analysis.cpp
    recipes/Finance.cpp
    recipes/Field.cpp
    recipes/Live.cpp
    recipes/Specialty.cpp
    recipes/Interaction.cpp
    recipes/AxisCustom.cpp
    recipes/Decorations.cpp
    recipes/Selection.cpp
    recipes/Advanced.cpp
    recipes/Supplement.cpp
    recipes/Supplement2.cpp
    recipes/Supplement3.cpp
    recipes/Supplement4.cpp
    recipes.qrc
)

target_include_directories(QtPlotDemo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(QtPlotDemo PRIVATE qtplot)

# --export 模式要把訊息印到主控台，所以保留 stdout（WIN32 子系統預設沒有）
set_target_properties(QtPlotDemo PROPERTIES OUTPUT_NAME "QtPlotDemo")

# ── 把 Qt 的執行期檔案複製到執行檔旁邊 ──
#
# 沒有這一步，執行前得先把 D:/Qt/.../bin 加進 PATH，而且少了 platforms/qwindows.dll
# 會直接跳「could not find or load the Qt platform plugin windows」而不是缺 DLL 的錯誤，
# 不知道的人很難查。windeployqt 會一併處理 DLL、平台外掛、影像格式與樣式。
#
# 它會跳過已是最新的檔案，所以每次建置都跑不會變慢。
# 不想自動部署就用 -DQTPLOT_DEPLOY=OFF。
option(QTPLOT_DEPLOY "建置後自動把 Qt 執行期檔案複製到執行檔旁邊" ON)

if(WIN32 AND QTPLOT_DEPLOY)
    # windeployqt 和 qmake 放在同一個 bin 目錄，從 qmake 的位置回推最可靠
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXE
        NAMES windeployqt windeployqt6
        HINTS "${_qt_bin}"
        NO_DEFAULT_PATH)

    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET QtPlotDemo POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}"
                    $<IF:$<CONFIG:Debug>,--debug,--release>
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-compiler-runtime
                    "$<TARGET_FILE:QtPlotDemo>"
            COMMENT "部署 Qt 執行期檔案到輸出目錄"
            VERBATIM)

        # windeployqt 會一併帶上 Direct3D 的著色器編譯器（13 MB），
        # 但這個程式完全走 QPainter 的軟體光柵化，用不到 D3D。
        # 已實測刪掉之後 GUI 與 --export 都正常，所以直接清掉。
        add_custom_command(TARGET QtPlotDemo POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f
                    "$<TARGET_FILE_DIR:QtPlotDemo>/dxcompiler.dll"
                    "$<TARGET_FILE_DIR:QtPlotDemo>/dxil.dll"
            COMMENT "移除用不到的 Direct3D 著色器編譯器"
            VERBATIM)
    else()
        message(WARNING "找不到 windeployqt，執行 QtPlotDemo 前要自己把 Qt 的 bin 加進 PATH。")
    endif()
endif()

# 數值自我檢查。圖對不對靠 --export 看圖，數字對不對靠這個。
add_executable(qtplot_selftest selftest.cpp)
target_link_libraries(qtplot_selftest PRIVATE qtplot)

# 主控台程式，不要 WIN32 子系統（要印結果）
set_target_properties(qtplot_selftest PROPERTIES WIN32_EXECUTABLE OFF)

if(WIN32 AND QTPLOT_DEPLOY AND WINDEPLOYQT_EXE)
    add_custom_command(TARGET qtplot_selftest POST_BUILD
        COMMAND "${WINDEPLOYQT_EXE}"
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime
                "$<TARGET_FILE:qtplot_selftest>"
        COMMENT "部署 Qt 執行期檔案（selftest）"
        VERBATIM)
endif()

# ── ctest ──
#   ctest --test-dir build --output-on-failure
# selftest：數值檢查，回傳失敗項數。
# soak：449 支範例輪流畫進同一張圖，檢查換圖後有沒有殘留，回傳 0 表示乾淨。
add_test(NAME selftest COMMAND qtplot_selftest)
add_test(NAME soak COMMAND QtPlotDemo --soak)
# 主視窗的「語言」選單：九種語言輪流切一遍
add_test(NAME demo_languages COMMAND QtPlotDemo --check-languages)
set_tests_properties(selftest soak demo_languages PROPERTIES TIMEOUT 300)

# QTPLOT_DEPLOY=OFF 時執行檔旁邊沒有 Qt 的 DLL，改從 Qt 安裝目錄找
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.22 AND QT6_INSTALL_PREFIX)
    set_tests_properties(selftest soak demo_languages PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
endif()

# C 介面的檢查。用純 C 編譯，確保 qtplot_c.h 沒有混進 C++ 語法。
if(TARGET qtplot_c)
    enable_language(C)
    add_executable(qtplot_capi_test capi_test.c)
    target_link_libraries(qtplot_capi_test PRIVATE qtplot_c)
    if(MSVC)
        # /utf-8：測試字串裡有中文；/wd4996：fopen 之類的「不安全」警告
        target_compile_options(qtplot_capi_test PRIVATE /utf-8 /wd4996)
    endif()
    add_test(NAME capi COMMAND qtplot_capi_test "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(capi PROPERTIES TIMEOUT 60)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.22 AND QT6_INSTALL_PREFIX)
        set_tests_properties(capi PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()
endif()

# C# 包裝（bindings/csharp）的端到端檢查：C# → qtplot_c.dll → Qt。
# 機器上有 dotnet 才跑；沒有就整段略過，不影響其他測試。
find_program(DOTNET_EXE dotnet)
option(QTPLOT_TEST_CSHARP "ctest 也跑 C# 包裝的檢查（需要 .NET 9 SDK）" ON)
if(TARGET qtplot_c AND DOTNET_EXE AND QTPLOT_TEST_CSHARP)
    add_test(NAME csharp
        COMMAND "${DOTNET_EXE}" run
                --project "${PROJECT_SOURCE_DIR}/bindings/csharp/Demo/Demo.csproj"
                -c Release -p:Platform=x64
                "-p:QtPlotNativeDir=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                -- --smoke "${CMAKE_CURRENT_BINARY_DIR}")
    # 第一次要還原套件、編譯，給寬一點
    set_tests_properties(csharp PROPERTIES TIMEOUT 300)
endif()

# PlotWidget 的互動檢查（翻譯、列印、動畫、手勢）。要 QApplication 與 Qt Test 的觸控模擬。
find_package(Qt6 QUIET COMPONENTS Test)
if(TARGET Qt6::Test)
    add_executable(qtplot_widget_test widget_test.cpp)
    target_link_libraries(qtplot_widget_test PRIVATE qtplot Qt6::Test)
    set_target_properties(qtplot_widget_test PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME widget COMMAND qtplot_widget_test "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(widget PROPERTIES TIMEOUT 60)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.22 AND QT6_INSTALL_PREFIX)
        set_tests_properties(widget PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()
endif()

# 效能量測（不判對錯，看數字用）：build\bin\qtplot_bench.exe
add_executable(qtplot_bench bench.cpp)
target_link_libraries(qtplot_bench PRIVATE qtplot)
set_target_properties(qtplot_bench PROPERTIES WIN32_EXECUTABLE OFF)

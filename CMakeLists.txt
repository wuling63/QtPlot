cmake_minimum_required(VERSION 3.21)

project(QtPlot
    VERSION 0.1.0
    DESCRIPTION "Qt/QPainter 互動式繪圖函式庫與 Cookbook 示範"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 這台機器上的 Qt。想換版本用 -DCMAKE_PREFIX_PATH=... 覆寫。
if(NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    foreach(_qt "D:/Qt/6.11.1/msvc2022_64" "D:/Qt/6.10.3/msvc2022_64")
        if(EXISTS "${_qt}/lib/cmake/Qt6/Qt6Config.cmake")
            set(CMAKE_PREFIX_PATH "${_qt}")
            break()
        endif()
    endforeach()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport LinguistTools)

# MSVC 的 <cmath> 要有這個巨集才會定義 M_PI
add_compile_definitions(_USE_MATH_DEFINES)

message(STATUS "QtPlot: Qt ${Qt6_VERSION} @ ${CMAKE_PREFIX_PATH}")

if(MSVC)
    # /utf-8：原始碼含中文字串，沒有這個旗標 MSVC 會用 ANSI 代碼頁誤判。
    add_compile_options(/utf-8 /W4 /permissive- /wd4100)
    # QPainter 的浮點座標轉換很多，關掉 double->float 的雜訊警告
    add_compile_options(/wd4244 /wd4267)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ctest 要在最上層啟用，子目錄的 add_test 才會被收進來
enable_testing()

add_subdirectory(src/qtplot)

# C 介面 DLL（C# 用）。不需要就 -DQTPLOT_BUILD_CAPI=OFF。
option(QTPLOT_BUILD_CAPI "建置 qtplot_c.dll（C 介面）" ON)
if(QTPLOT_BUILD_CAPI)
    add_subdirectory(src/qtplot_c)
endif()

add_subdirectory(demo)
add_subdirectory(tests)

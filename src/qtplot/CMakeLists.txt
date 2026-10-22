add_library(qtplot STATIC
    Core.h            Core.cpp
    Axis.h            Axis.cpp
    Plottable.h
    Plottables.h      Plottables.cpp
    Bars3D.h          Bars3D.cpp
    Chart3D.h         Chart3D.cpp
    Business.h        Business.cpp
    Sunburst.h        Sunburst.cpp
    Flow.h            Flow.cpp
    Isometric.h       Isometric.cpp
    Multivariate.h    Multivariate.cpp
    Polar.h           Polar.cpp
    Overlays.h        Overlays.cpp
    RadialCharts.h    RadialCharts.cpp
    Statistics.h      Statistics.cpp
    Inspection.h      Inspection.cpp
    Analysis.h        Analysis.cpp
    Spectral.h        Spectral.cpp
    Finance.h         Finance.cpp
    Field.h           Field.cpp
    Live.h            Live.cpp
    Annotations.h     Annotations.cpp
    Decorations.h     Decorations.cpp
    Selection.cpp
    Smoothing.cpp
    Specialty.h       Specialty.cpp
    Plot.h            Plot.cpp
    Multiplot.h       Multiplot.cpp
    PlotWidget.h      PlotWidget.cpp
    LegendStyleDialog.h LegendStyleDialog.cpp
    DataGen.h         DataGen.cpp
    Decimation.h      Decimation.cpp
    Translation.h     Translation.cpp
)

target_include_directories(qtplot PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(qtplot PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg      # Plot::saveSvg 用 QSvgGenerator
    Qt6::PrintSupport  # PlotWidget::print 與預覽列印
    # savePdf 用 QPdfWriter，它在 Qt6::Gui 裡，上面已經連了
)

# ── 翻譯 ──
# 原始字串是繁體中文。改了介面文字之後跑一次：
#   cmake --build build --target update_translations
# 會把新字串加進 i18n/qtplot_*.ts，用 Qt Linguist 填好翻譯再建置即可。
# 要加新語言：在下面的 TS_FILES 加一個 qtplot_<代碼>.ts（例如 qtplot_de.ts），跑 update_translations 再翻。
# .qm 編進函式庫的資源（:/qtplot/i18n/），使用端不必另外帶檔案。
qt_add_translations(qtplot
    TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_en.ts"
             "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_zh_CN.ts"
             "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_ja.ts"
             "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_ko.ts"
             "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_fr.ts"
             "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_de.ts"
             "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_it.ts"
             "${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtplot_es.ts"
    SOURCE_TARGETS qtplot
    LUPDATE_OPTIONS -no-obsolete -locations none
    QM_FILES_OUTPUT_VARIABLE qtplot_qm_files)
qt_add_resources(qtplot qtplot_translations
    PREFIX "/qtplot/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${qtplot_qm_files})

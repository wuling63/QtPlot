// qtplot_c.cpp — qtplot_c.h 的實作。
//
// 每個匯出函式都包在 guard() 裡：C++ 例外不能穿過 C ABI（穿過去就是未定義行為，
// 在 C# 那邊通常是整個行程直接消失），所以一律在這裡接住，轉成回傳碼與 qp_last_error()。
#include "qtplot_c.h"

#include "qtplot/Plot.h"
#include "qtplot/Translation.h"

#include <QApplication>
#include <QByteArray>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <exception>
#include <memory>
#include <string>

using namespace qtplot;

struct QpPlot {
    Plot plot;
};

namespace {

thread_local std::string g_lastError;

int fail(int code, const char* message) {
    g_lastError = message;
    return code;
}

// 行程裡沒有 Qt application 就建一個。量字型、畫 QImage 都要它。
//
// 刻意不在 DLL 卸載時刪掉：宿主（C#）結束時 DLL 的卸載順序沒有保證，
// 在那時候解構 QApplication 很容易踩到已經卸載的 Qt 外掛。行程結束時作業系統會回收。
void ensureApp() {
    if (QCoreApplication::instance()) return;
    static int argc = 1;
    static char arg0[] = "qtplot_c";
    static char* argv[] = {arg0, nullptr};
    new QApplication(argc, argv);
}

// 把一個匯出函式的本體包起來：補 Qt application、清錯誤、接住例外。
template <typename F>
int guard(F&& body) {
    try {
        ensureApp();
        g_lastError.clear();
        return body();
    } catch (const std::exception& e) {
        return fail(QP_ERR_INTERNAL, e.what());
    } catch (...) {
        return fail(QP_ERR_INTERNAL, "unknown C++ exception");
    }
}

// 回傳指標的版本：失敗時回傳 nullptr，原因一樣放在 qp_last_error()。
template <typename F>
QpItem* guardItem(QpPlot* plot, F&& body) {
    QpItem* result = nullptr;
    guard([&]() -> int {
        if (!plot) return fail(QP_ERR_NULL, "plot is NULL");
        IPlottable* p = body(plot->plot);
        if (!p) return g_lastError.empty() ? fail(QP_ERR_ARG, "invalid argument") : QP_ERR_ARG;
        result = reinterpret_cast<QpItem*>(p);
        return QP_OK;
    });
    return result;
}

QColor toColor(uint32_t argb) {
    if (argb == QP_AUTO_COLOR) return QColor();  // 無效色 = 交給 Plot 的調色盤
    return QColor::fromRgba(argb);
}

QString toString(const char* utf8) { return utf8 ? QString::fromUtf8(utf8) : QString(); }

QVector<double> toVector(const double* data, int count) {
    return QVector<double>(data, data + count);
}

bool checkArray(const double* data, int count) {
    if (!data) return fail(QP_ERR_NULL, "data pointer is NULL"), false;
    if (count <= 0) return fail(QP_ERR_ARG, "count must be > 0"), false;
    return true;
}

Axis* axisOf(Plot& plot, int axis) {
    switch (axis) {
    case QP_AXIS_BOTTOM: return plot.bottomAxis();
    case QP_AXIS_LEFT: return plot.leftAxis();
    case QP_AXIS_TOP: return plot.topAxis();
    case QP_AXIS_RIGHT: return plot.rightAxis();
    default: return nullptr;
    }
}

// item 必須是這張圖目前擁有的圖元。C# 那邊很容易在 clear() 之後還拿著舊的 handle，
// 直接 reinterpret_cast 回去用就是懸空指標；這裡逐一比對，圖元數量一般只有幾十個。
IPlottable* resolve(QpPlot* plot, QpItem* item) {
    if (!plot || !item) return nullptr;
    auto* p = reinterpret_cast<IPlottable*>(item);
    const QVector<IPlottable*> all = plot->plot.plottables();
    return all.contains(p) ? p : nullptr;
}

#define QP_REQUIRE_PLOT()                                              \
    do {                                                               \
        if (!plot) return fail(QP_ERR_NULL, "plot is NULL");           \
    } while (0)

#define QP_RESOLVE(var)                                                \
    QP_REQUIRE_PLOT();                                                 \
    IPlottable* var = resolve(plot, item);                             \
    if (!var) return fail(QP_ERR_BAD_ITEM, "item does not belong to this plot (removed or cleared?)")

// 有 line 成員的圖元
LineStyle* lineOf(IPlottable* p) {
    if (auto* s = dynamic_cast<Scatter*>(p)) return &s->line;
    if (auto* s = dynamic_cast<Signal*>(p)) return &s->line;
    if (auto* s = dynamic_cast<SignalXY*>(p)) return &s->line;
    if (auto* s = dynamic_cast<AxisLine*>(p)) return &s->line;
    if (auto* s = dynamic_cast<DataStreamer*>(p)) return &s->line;
    if (auto* s = dynamic_cast<DataLogger*>(p)) return &s->line;
    return nullptr;
}

// 有 marker 成員的圖元
MarkerStyle* markerOf(IPlottable* p) {
    if (auto* s = dynamic_cast<Scatter*>(p)) return &s->marker;
    if (auto* s = dynamic_cast<Markers*>(p)) return &s->marker;
    if (auto* s = dynamic_cast<Signal*>(p)) return &s->marker;
    if (auto* s = dynamic_cast<SignalXY*>(p)) return &s->marker;
    return nullptr;
}

}  // namespace

// ───────────────────────────── 基本 ─────────────────────────────

extern "C" {

const char* qp_version(void) { return QTPLOT_C_VERSION; }

const char* qp_last_error(void) { return g_lastError.c_str(); }

int qp_set_language(const char* code) {
    return guard([&]() -> int { return setLanguage(toString(code)) ? 1 : 0; });
}

const char* qp_available_languages(void) {
    static std::string list;
    guard([&]() -> int {
        list = availableLanguages().join(QLatin1Char(',')).toStdString();
        return QP_OK;
    });
    return list.c_str();
}

QpPlot* qp_plot_create(void) {
    QpPlot* result = nullptr;
    guard([&]() -> int {
        result = new QpPlot();
        return QP_OK;
    });
    return result;
}

void qp_plot_destroy(QpPlot* plot) {
    guard([&]() -> int {
        delete plot;
        return QP_OK;
    });
}

int qp_plot_clear(QpPlot* plot) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        plot->plot.clear();
        return QP_OK;
    });
}

int qp_plot_reset(QpPlot* plot) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        plot->plot.reset();
        return QP_OK;
    });
}

int qp_plot_count(QpPlot* plot) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        return plot->plot.plottableCount();
    });
}

// ───────────────────────────── 外觀 ─────────────────────────────

int qp_set_title(QpPlot* plot, const char* text) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        plot->plot.title.text = toString(text);
        return QP_OK;
    });
}

int qp_set_axis_label(QpPlot* plot, int axis, const char* text) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        Axis* a = axisOf(plot->plot, axis);
        if (!a) return fail(QP_ERR_ARG, "unknown axis");
        a->label.text = toString(text);
        return QP_OK;
    });
}

int qp_set_style(QpPlot* plot, int style) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        switch (style) {
        case QP_STYLE_LIGHT: plot->plot.setStyle(PlotStyle::light()); break;
        case QP_STYLE_DARK: plot->plot.setStyle(PlotStyle::dark()); break;
        case QP_STYLE_HARDCOPY: plot->plot.setStyle(PlotStyle::hardcopy()); break;
        default: return fail(QP_ERR_ARG, "unknown style");
        }
        return QP_OK;
    });
}

int qp_set_legend(QpPlot* plot, int visible, int corner) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        Qt::Alignment a;
        switch (corner) {
        case QP_CORNER_TOP_LEFT: a = Qt::AlignLeft | Qt::AlignTop; break;
        case QP_CORNER_TOP_RIGHT: a = Qt::AlignRight | Qt::AlignTop; break;
        case QP_CORNER_BOTTOM_LEFT: a = Qt::AlignLeft | Qt::AlignBottom; break;
        case QP_CORNER_BOTTOM_RIGHT: a = Qt::AlignRight | Qt::AlignBottom; break;
        default: return fail(QP_ERR_ARG, "unknown corner");
        }
        plot->plot.legend.isVisible = visible != 0;
        plot->plot.legend.alignment = a;
        return QP_OK;
    });
}

int qp_set_grid(QpPlot* plot, int visible) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        plot->plot.grid.isVisible = visible != 0;
        return QP_OK;
    });
}

int qp_set_antialias(QpPlot* plot, int enabled) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        plot->plot.antiAlias = enabled != 0;
        return QP_OK;
    });
}

int qp_set_font(QpPlot* plot, const char* family, double scale) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (family && *family) plot->plot.setFontFamily(toString(family));
        if (scale > 0) plot->plot.scaleFontSizes(scale);
        return QP_OK;
    });
}

// ───────────────────────────── 圖例樣式 ─────────────────────────────

int qp_set_legend_layout(QpPlot* plot, int placement, int horizontal, int columns) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        Legend& lg = plot->plot.legend;
        switch (placement) {
        case QP_LEGEND_INSIDE: lg.placement = Legend::Placement::Inside; break;
        case QP_LEGEND_OUTSIDE_RIGHT: lg.placement = Legend::Placement::OutsideRight; break;
        case QP_LEGEND_OUTSIDE_BOTTOM:
            lg.placement = Legend::Placement::OutsideBottom;
            lg.alignment = Qt::AlignHCenter | Qt::AlignTop;  // 底下的圖例預設置中
            break;
        default: return fail(QP_ERR_ARG, "unknown legend placement");
        }
        lg.horizontal = horizontal != 0;
        if (columns > 0) lg.columns = columns;
        return QP_OK;
    });
}

int qp_set_legend_border(QpPlot* plot, uint32_t argb, double width, int pattern) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (pattern < QP_LINE_SOLID || pattern > QP_LINE_DASH_DOT_DOT) return fail(QP_ERR_ARG, "unknown line pattern");
        LineStyle& b = plot->plot.legend.border;
        if (!(width > 0)) {
            b.pattern = LinePattern::None;
            return QP_OK;
        }
        if (argb != QP_AUTO_COLOR) b.color = QColor::fromRgba(argb);
        b.width = width;
        b.pattern = static_cast<LinePattern>(pattern);
        return QP_OK;
    });
}

int qp_set_legend_background(QpPlot* plot, uint32_t argb) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        plot->plot.legend.background = FillStyle(QColor::fromRgba(argb));  // 0 = 透明 = 不塗
        return QP_OK;
    });
}

int qp_set_legend_symbols(QpPlot* plot, int filled, uint32_t outlineArgb, double outlineWidth) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        Legend& lg = plot->plot.legend;
        lg.fillSymbols = filled != 0;
        lg.symbolOutline = (outlineWidth > 0)
                               ? LineStyle(outlineArgb ? QColor::fromRgba(outlineArgb) : QColor(0, 0, 0), outlineWidth)
                               : LineStyle::none();
        return QP_OK;
    });
}

int qp_set_legend_font(QpPlot* plot, const char* family, double size, int bold, int italic, uint32_t argb) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        FontStyle& f = plot->plot.legend.font;
        if (family && *family) {
            f.family = toString(family);
            plot->plot.legend.title.font.family = f.family;
        }
        if (size > 0) f.size = size;
        if (bold >= 0) {
            f.weight = FontStyle::Auto;
            f.bold = bold != 0;
        }
        if (italic >= 0) f.italic = italic != 0;
        if (argb != QP_AUTO_COLOR) f.color = QColor::fromRgba(argb);
        return QP_OK;
    });
}

// ───────────────────────────── 座標軸與刻度 ─────────────────────────────

#define QP_AXIS(var)                                                   \
    QP_REQUIRE_PLOT();                                                 \
    Axis* var = axisOf(plot->plot, axis);                              \
    if (!var) return fail(QP_ERR_ARG, "unknown axis")

int qp_set_axis_color(QpPlot* plot, int axis, uint32_t argb) {
    return guard([&]() -> int {
        QP_AXIS(a);
        a->setColor(QColor::fromRgba(argb));
        return QP_OK;
    });
}

int qp_set_axis_ticks(QpPlot* plot, int axis, int direction, double majorLength, double minorLength, int mirror) {
    return guard([&]() -> int {
        QP_AXIS(a);
        if (direction < QP_TICKS_OUTSIDE || direction > QP_TICKS_CROSS) return fail(QP_ERR_ARG, "unknown tick direction");
        a->tickDirection = static_cast<TickDirection>(direction);
        if (majorLength >= 0) a->majorTickLength = majorLength;
        if (minorLength >= 0) a->minorTickLength = minorLength;
        a->mirrorTicks = mirror != 0;
        return QP_OK;
    });
}

int qp_set_axis_tick_marker(QpPlot* plot, int axis, int shape, double size, uint32_t argb) {
    return guard([&]() -> int {
        QP_AXIS(a);
        if (shape < QP_MARKER_NONE || shape > QP_MARKER_HORIZONTAL_BAR) return fail(QP_ERR_ARG, "unknown marker shape");
        const QColor c = argb ? QColor::fromRgba(argb) : a->majorTickLine.color;
        a->majorTickMarker = MarkerStyle(static_cast<MarkerShape>(shape), std::max(0.0, size), c);
        return QP_OK;
    });
}

int qp_set_axis_log(QpPlot* plot, int axis, int enabled, int labelFormat, int labelMinor) {
    return guard([&]() -> int {
        QP_AXIS(a);
        if (labelFormat < QP_LOG_LABEL_AUTO || labelFormat > QP_LOG_LABEL_E) return fail(QP_ERR_ARG, "unknown log label format");
        a->logScale = enabled != 0;
        if (a->logScale) {
            auto lg = std::make_shared<LogTickGenerator>();
            lg->labelFormat = static_cast<LogTickGenerator::LabelFormat>(labelFormat);
            lg->labelMinorTicks = labelMinor != 0;
            a->tickGenerator = lg;
        } else if (dynamic_cast<LogTickGenerator*>(a->tickGenerator.get())) {
            a->tickGenerator = std::make_shared<AutomaticTickGenerator>();  // 關掉時換回線性刻度
        }
        return QP_OK;
    });
}

// ───────────────────────────── 視野 ─────────────────────────────

int qp_set_limits(QpPlot* plot, double xMin, double xMax, double yMin, double yMax) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (!(xMax > xMin) || !(yMax > yMin)) return fail(QP_ERR_ARG, "max must be greater than min");
        plot->plot.setAxisLimits(xMin, xMax, yMin, yMax);
        return QP_OK;
    });
}

int qp_get_limits(QpPlot* plot, double* xMin, double* xMax, double* yMin, double* yMax) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        const CoordinateRange x = plot->plot.bottomAxis()->range;
        const CoordinateRange y = plot->plot.leftAxis()->range;
        if (xMin) *xMin = x.min;
        if (xMax) *xMax = x.max;
        if (yMin) *yMin = y.min;
        if (yMax) *yMax = y.max;
        return QP_OK;
    });
}

int qp_autoscale(QpPlot* plot, double margin) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        // 使用者主動要求自動範圍，所以連 setAxisLimits 設過的軸也重算
        plot->plot.autoScale(margin >= 0 ? margin : 0.05, true);
        return QP_OK;
    });
}

int qp_pixel_to_coord(QpPlot* plot, double px, double py, double* x, double* y) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        const PixelRect d = plot->plot.lastDataRect();
        if (d.isEmpty()) return fail(QP_ERR_ARG, "plot has not been rendered yet");
        if (x) *x = plot->plot.bottomAxis()->getCoordinate(px, d);
        if (y) *y = plot->plot.leftAxis()->getCoordinate(py, d);
        return QP_OK;
    });
}

int qp_coord_to_pixel(QpPlot* plot, double x, double y, double* px, double* py) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        const PixelRect d = plot->plot.lastDataRect();
        if (d.isEmpty()) return fail(QP_ERR_ARG, "plot has not been rendered yet");
        if (px) *px = plot->plot.bottomAxis()->getPixel(x, d);
        if (py) *py = plot->plot.leftAxis()->getPixel(y, d);
        return QP_OK;
    });
}

int qp_pan(QpPlot* plot, double dxPx, double dyPx) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        Plot& p = plot->plot;
        const PixelRect d = p.lastDataRect();
        if (d.isEmpty()) return fail(QP_ERR_ARG, "plot has not been rendered yet");
        // 走 Axis::pan：反向軸的方向自然就對了，對數軸也會在對數空間平移
        auto shift = [&](Axis* a, Axis* mirror, double from, double to) {
            if (a->locked) return;
            a->pan(from, to, d);
            mirror->range = a->range;
        };
        shift(p.bottomAxis(), p.topAxis(), d.left, d.left + dxPx);
        shift(p.leftAxis(), p.rightAxis(), d.top, d.top + dyPx);
        return QP_OK;
    });
}

int qp_zoom(QpPlot* plot, double factor, double px, double py) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (!(factor > 0)) return fail(QP_ERR_ARG, "factor must be > 0");
        Plot& p = plot->plot;
        const PixelRect d = p.lastDataRect();
        if (d.isEmpty()) return fail(QP_ERR_ARG, "plot has not been rendered yet");
        // 和 PlotWidget::zoomAt 同一套：游標下的資料點縮放後仍在同一個像素上
        Axis* bx = p.bottomAxis();
        Axis* by = p.leftAxis();
        if (!bx->locked) {
            bx->zoom(factor, bx->getCoordinate(px, d));
            p.topAxis()->range = bx->range;
        }
        if (!by->locked) {
            by->zoom(factor, by->getCoordinate(py, d));
            p.rightAxis()->range = by->range;
        }
        return QP_OK;
    });
}

// ───────────────────────────── 圖元 ─────────────────────────────

QpItem* qp_add_scatter(QpPlot* plot, const double* xs, const double* ys, int count, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (!checkArray(xs, count) || !checkArray(ys, count)) return nullptr;
        return p.addScatter(toVector(xs, count), toVector(ys, count), toColor(argb));
    });
}

QpItem* qp_add_markers(QpPlot* plot, const double* xs, const double* ys, int count, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (!checkArray(xs, count) || !checkArray(ys, count)) return nullptr;
        return p.addMarkers(toVector(xs, count), toVector(ys, count), toColor(argb));
    });
}

QpItem* qp_add_signal(QpPlot* plot, const double* ys, int count, double period, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (!checkArray(ys, count)) return nullptr;
        if (period == 0) return fail(QP_ERR_ARG, "period must not be 0"), nullptr;
        return p.addSignal(toVector(ys, count), period, toColor(argb));
    });
}

QpItem* qp_add_signal_xy(QpPlot* plot, const double* xs, const double* ys, int count, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (!checkArray(xs, count) || !checkArray(ys, count)) return nullptr;
        return p.addSignalXY(toVector(xs, count), toVector(ys, count), toColor(argb));
    });
}

QpItem* qp_add_bars(QpPlot* plot, const double* values, int count, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (!checkArray(values, count)) return nullptr;
        return p.addBars(toVector(values, count), toColor(argb));
    });
}

QpItem* qp_add_histogram(QpPlot* plot, const double* values, int count, int binCount, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (!checkArray(values, count)) return nullptr;
        return p.addHistogram(toVector(values, count), std::max(0, binCount), toColor(argb));
    });
}

QpItem* qp_add_heatmap(QpPlot* plot, const double* values, int rows, int cols, int colormap) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (rows <= 0 || cols <= 0) return fail(QP_ERR_ARG, "rows and cols must be > 0"), nullptr;
        if (!checkArray(values, rows * cols)) return nullptr;
        if (colormap < QP_COLORMAP_VIRIDIS || colormap > QP_COLORMAP_COOLWARM)
            return fail(QP_ERR_ARG, "unknown colormap"), nullptr;
        Heatmap* h = p.addHeatmap(toVector(values, rows * cols), rows, cols);
        h->colormap = Colormap(static_cast<Colormap::Preset>(colormap));
        return h;
    });
}

QpItem* qp_add_hline(QpPlot* plot, double y, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* { return p.addHorizontalLine(y, toColor(argb)); });
}

QpItem* qp_add_vline(QpPlot* plot, double x, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* { return p.addVerticalLine(x, toColor(argb)); });
}

QpItem* qp_add_text(QpPlot* plot, const char* text, double x, double y) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (!text) return fail(QP_ERR_NULL, "text is NULL"), nullptr;
        return p.addText(toString(text), x, y);
    });
}

QpItem* qp_add_streamer(QpPlot* plot, int capacity, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (capacity <= 0) return fail(QP_ERR_ARG, "capacity must be > 0"), nullptr;
        return p.addDataStreamer(capacity, toColor(argb));
    });
}

QpItem* qp_add_logger(QpPlot* plot, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* { return p.addDataLogger(toColor(argb)); });
}

QpItem* qp_add_live_spectrum(QpPlot* plot, int fftSize, double sampleRate, uint32_t argb) {
    return guardItem(plot, [&](Plot& p) -> IPlottable* {
        if (fftSize < 8) return fail(QP_ERR_ARG, "fftSize must be >= 8"), nullptr;
        if (!(sampleRate > 0)) return fail(QP_ERR_ARG, "sampleRate must be > 0"), nullptr;
        return p.addLiveSpectrum(fftSize, sampleRate, toColor(argb));
    });
}

// ───────────────────────────── 圖元屬性 ─────────────────────────────

int qp_item_remove(QpPlot* plot, QpItem* item) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        plot->plot.remove(p);
        return QP_OK;
    });
}

int qp_item_set_visible(QpPlot* plot, QpItem* item, int visible) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        p->isVisible = visible != 0;
        return QP_OK;
    });
}

int qp_item_set_legend(QpPlot* plot, QpItem* item, const char* text) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        p->legendText = toString(text);
        return QP_OK;
    });
}

int qp_item_set_line(QpPlot* plot, QpItem* item, uint32_t argb, double width) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        LineStyle* line = lineOf(p);
        if (!line) return fail(QP_ERR_WRONG_TYPE, "this item has no line style");
        if (argb != QP_AUTO_COLOR) line->color = QColor::fromRgba(argb);
        line->width = std::max(0.0, width);
        return QP_OK;
    });
}

int qp_item_set_marker(QpPlot* plot, QpItem* item, int shape, double size, uint32_t argb) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        MarkerStyle* m = markerOf(p);
        if (!m) return fail(QP_ERR_WRONG_TYPE, "this item has no marker style");
        if (shape < QP_MARKER_NONE || shape > QP_MARKER_HORIZONTAL_BAR) return fail(QP_ERR_ARG, "unknown marker shape");
        m->shape = static_cast<MarkerShape>(shape);
        m->size = std::max(0.0, size);
        if (argb != QP_AUTO_COLOR) {
            m->fillColor = QColor::fromRgba(argb);
            m->outline.color = m->fillColor;
        }
        return QP_OK;
    });
}

int qp_item_set_data(QpPlot* plot, QpItem* item, const double* xs, const double* ys, int count) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        if (!checkArray(ys, count)) return QP_ERR_ARG;
        if (auto* s = dynamic_cast<Signal*>(p)) {
            s->ys = toVector(ys, count);
            return QP_OK;
        }
        if (!checkArray(xs, count)) return QP_ERR_ARG;
        if (auto* sc = dynamic_cast<Scatter*>(p)) {
            sc->xs = toVector(xs, count);
            sc->ys = toVector(ys, count);
        } else if (auto* mk = dynamic_cast<Markers*>(p)) {
            mk->xs = toVector(xs, count);
            mk->ys = toVector(ys, count);
        } else if (auto* sx = dynamic_cast<SignalXY*>(p)) {
            sx->xs = toVector(xs, count);
            sx->ys = toVector(ys, count);
        } else {
            return fail(QP_ERR_WRONG_TYPE, "set_data supports scatter, markers, signal and signal_xy");
        }
        return QP_OK;
    });
}

int qp_item_set_color_range(QpPlot* plot, QpItem* item, double min, double max) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        auto* h = dynamic_cast<Heatmap*>(p);
        if (!h) return fail(QP_ERR_WRONG_TYPE, "color range only applies to heatmaps");
        h->colorRange = (max > min) ? CoordinateRange(min, max) : CoordinateRange();
        return QP_OK;
    });
}

int qp_item_append(QpPlot* plot, QpItem* item, const double* xs, const double* ys, int count) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        if (!checkArray(ys, count)) return QP_ERR_ARG;
        if (auto* s = dynamic_cast<DataStreamer*>(p)) {
            s->addRange(toVector(ys, count));
            return QP_OK;
        }
        if (auto* s = dynamic_cast<DataLogger*>(p)) {
            if (xs) s->addRange(toVector(xs, count), toVector(ys, count));
            else s->addRange(toVector(ys, count));
            return QP_OK;
        }
        if (auto* s = dynamic_cast<LiveSpectrum*>(p)) {
            s->addRange(toVector(ys, count));  // 頻譜只看樣本，xs 忽略
            return QP_OK;
        }
        return fail(QP_ERR_WRONG_TYPE, "append only applies to streamer, logger and live spectrum");
    });
}

int qp_spectrum_set_analysis(QpPlot* plot, QpItem* item, int window, double averaging, int peakHold,
                             double peakHoldDecayDb) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        auto* s = dynamic_cast<LiveSpectrum*>(p);
        if (!s) return fail(QP_ERR_WRONG_TYPE, "this item is not a live spectrum");
        if (window < QP_WINDOW_RECTANGULAR || window > QP_WINDOW_FLAT_TOP) return fail(QP_ERR_ARG, "unknown window");
        s->window = static_cast<WindowFunction>(window);
        s->averaging = std::clamp(averaging, 0.0, 0.99);
        s->showPeakHold = peakHold != 0;
        s->peakHoldDecayDb = std::max(0.0, peakHoldDecayDb);
        return QP_OK;
    });
}

int qp_spectrum_set_display(QpPlot* plot, QpItem* item, double minDb, double maxDb, int peakLabels, uint32_t fillArgb) {
    return guard([&]() -> int {
        QP_RESOLVE(p);
        auto* s = dynamic_cast<LiveSpectrum*>(p);
        if (!s) return fail(QP_ERR_WRONG_TYPE, "this item is not a live spectrum");
        if (!(maxDb > minDb)) return fail(QP_ERR_ARG, "maxDb must be greater than minDb");
        s->minDb = minDb;
        s->maxDb = maxDb;
        s->peakLabels = std::max(0, peakLabels);
        s->fill = FillStyle(QColor::fromRgba(fillArgb));
        return QP_OK;
    });
}

// ───────────────────────────── 頻譜計算 ─────────────────────────────

int qp_welch(const double* samples, int count, double sampleRate, int segmentSize, double overlap, int window,
             double* outFreq, double* outDb, int capacity) {
    return guard([&]() -> int {
        if (!checkArray(samples, count)) return count <= 0 ? QP_ERR_ARG : QP_ERR_NULL;
        if (count < 2) return fail(QP_ERR_ARG, "need at least 2 samples");
        if (!(sampleRate > 0)) return fail(QP_ERR_ARG, "sampleRate must be > 0");
        if (window < QP_WINDOW_RECTANGULAR || window > QP_WINDOW_FLAT_TOP) return fail(QP_ERR_ARG, "unknown window");
        const QVector<double> v = toVector(samples, count);
        const WindowFunction w = static_cast<WindowFunction>(window);
        const Periodogram pg = (segmentSize > 0 && segmentSize < count) ? welch(v, sampleRate, segmentSize, overlap, w)
                                                                        : periodogram(v, sampleRate, w);
        const int bins = static_cast<int>(pg.frequencies.size());
        if (outFreq && outDb && capacity >= bins) {
            std::copy(pg.frequencies.begin(), pg.frequencies.end(), outFreq);
            std::copy(pg.decibels.begin(), pg.decibels.end(), outDb);
        }
        return bins;
    });
}

int qp_find_peaks(const double* freqs, const double* db, int count, int maxPeaks, double minProminenceDb,
                  double minSpacingHz, double minFrequency, double* outFreq, double* outDb) {
    return guard([&]() -> int {
        if (!checkArray(freqs, count) || !checkArray(db, count)) return QP_ERR_ARG;
        if (maxPeaks <= 0) return 0;
        if (!outFreq || !outDb) return fail(QP_ERR_NULL, "output pointer is NULL");
        Periodogram pg;
        pg.frequencies = toVector(freqs, count);
        pg.decibels = toVector(db, count);
        for (double d : pg.decibels) pg.amplitude.append(std::pow(10.0, d / 20));  // 只是為了讓 amplitude 欄位一致
        const QVector<SpectralPeak> peaks = findSpectralPeaks(pg, maxPeaks, minProminenceDb, minSpacingHz, minFrequency);
        for (int i = 0; i < peaks.size(); ++i) {
            outFreq[i] = peaks[i].frequency;
            outDb[i] = peaks[i].decibels;
        }
        return static_cast<int>(peaks.size());
    });
}

// ───────────────────────────── 輸出 ─────────────────────────────

int qp_render_bgra(QpPlot* plot, int width, int height, uint8_t* buffer, int stride) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (!buffer) return fail(QP_ERR_NULL, "buffer is NULL");
        if (width <= 0 || height <= 0) return fail(QP_ERR_ARG, "width and height must be > 0");
        if (stride < width * 4) return fail(QP_ERR_ARG, "stride must be at least width * 4");
        // 直接包住呼叫端的記憶體畫，不另外配置再複製
        QImage img(buffer, width, height, stride, QImage::Format_ARGB32_Premultiplied);
        img.fill(plot->plot.style.figureBackground);
        QPainter painter(&img);
        plot->plot.render(painter, QRectF(0, 0, width, height));
        return QP_OK;
    });
}

int qp_save_png(QpPlot* plot, const char* path, int width, int height, double scale) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (!path) return fail(QP_ERR_NULL, "path is NULL");
        if (width <= 0 || height <= 0 || !(scale > 0)) return fail(QP_ERR_ARG, "width, height and scale must be > 0");
        if (!plot->plot.savePng(toString(path), width, height, scale)) return fail(QP_ERR_IO, "failed to write PNG");
        return QP_OK;
    });
}

int qp_save_svg(QpPlot* plot, const char* path, int width, int height) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (!path) return fail(QP_ERR_NULL, "path is NULL");
        if (width <= 0 || height <= 0) return fail(QP_ERR_ARG, "width and height must be > 0");
        if (!plot->plot.saveSvg(toString(path), width, height)) return fail(QP_ERR_IO, "failed to write SVG");
        return QP_OK;
    });
}

int qp_save_pdf(QpPlot* plot, const char* path, int width, int height) {
    return guard([&]() -> int {
        QP_REQUIRE_PLOT();
        if (!path) return fail(QP_ERR_NULL, "path is NULL");
        if (width <= 0 || height <= 0) return fail(QP_ERR_ARG, "width and height must be > 0");
        if (!plot->plot.savePdf(toString(path), width, height)) return fail(QP_ERR_IO, "failed to write PDF");
        return QP_OK;
    });
}

uint64_t qp_data_version(QpPlot* plot) {
    if (!plot) return 0;
    uint64_t sum = 0;
    for (const IPlottable* p : plot->plot.plottables()) sum += p->dataVersion();
    return sum;
}

double qp_last_render_ms(QpPlot* plot) { return plot ? plot->plot.lastRenderMs() : 0.0; }

}  // extern "C"

/* qtplot_c.h — QtPlot 的 C 介面（給 C#、Python、Delphi 這類只認 C ABI 的語言用）。
 *
 * C++ 類別不能跨 DLL 給別的語言用，所以這裡把 Plot 包成「不透明的指標 + 一組 C 函式」：
 *
 *     QpPlot* p = qp_plot_create();
 *     QpItem* s = qp_add_scatter(p, xs, ys, n, QP_AUTO_COLOR);
 *     qp_item_set_legend(p, s, "量測值");
 *     qp_save_png(p, "out.png", 800, 600, 1.0);
 *     qp_plot_destroy(p);
 *
 * 約定：
 *   - 字串一律 UTF-8，以 '\0' 結尾。
 *   - 顏色是 0xAARRGGBB。QP_AUTO_COLOR（0）代表「用調色盤的下一個顏色」——
 *     完全透明的黑色當資料色沒有意義，所以拿它來當「自動」。
 *   - 回傳 int 的函式：QP_OK（0）成功，負值失敗，原因用 qp_last_error() 取。
 *   - 陣列參數都會被複製一份，呼叫完就可以釋放或改寫自己的陣列。
 *   - QpItem* 屬於建立它的 QpPlot。qp_plot_clear()／qp_plot_reset()／qp_item_remove()
 *     之後就失效；傳失效的 item 進來會回傳 QP_ERR_BAD_ITEM，不會當掉。
 *   - 同一個 QpPlot 不能同時被兩條執行緒使用。第一次呼叫任何函式時，若行程裡還沒有
 *     Qt 的 application 物件，會自動建一個（宿主本身是 Qt 程式的話就沿用宿主的）。
 */
#ifndef QTPLOT_C_H
#define QTPLOT_C_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(QTPLOT_C_BUILD)
#    define QP_API __declspec(dllexport)
#  else
#    define QP_API __declspec(dllimport)
#  endif
#  define QP_CALL __cdecl
#else
#  define QP_API __attribute__((visibility("default")))
#  define QP_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QpPlot QpPlot;
typedef struct QpItem QpItem;

#define QP_AUTO_COLOR 0u

/* 回傳碼 */
enum {
    QP_OK = 0,
    QP_ERR_NULL = -1,        /* 必要的指標是 NULL */
    QP_ERR_ARG = -2,         /* 參數不合法（長度 <= 0、列數乘行數對不上……） */
    QP_ERR_BAD_ITEM = -3,    /* item 不屬於這張圖，或已經被移除 */
    QP_ERR_WRONG_TYPE = -4,  /* 這個 item 沒有這項屬性（例如對長條圖設標記） */
    QP_ERR_IO = -5,          /* 存檔失敗 */
    QP_ERR_INTERNAL = -6     /* C++ 端丟出例外 */
};

/* 座標軸 */
enum { QP_AXIS_BOTTOM = 0, QP_AXIS_LEFT = 1, QP_AXIS_TOP = 2, QP_AXIS_RIGHT = 3 };

/* 整張圖的配色 */
enum { QP_STYLE_LIGHT = 0, QP_STYLE_DARK = 1, QP_STYLE_HARDCOPY = 2 };

/* 圖例位置 */
enum {
    QP_CORNER_TOP_LEFT = 0, QP_CORNER_TOP_RIGHT = 1,
    QP_CORNER_BOTTOM_LEFT = 2, QP_CORNER_BOTTOM_RIGHT = 3
};

/* 標記形狀，數值與 qtplot::MarkerShape 相同 */
enum {
    QP_MARKER_NONE = 0,
    QP_MARKER_FILLED_CIRCLE, QP_MARKER_OPEN_CIRCLE,
    QP_MARKER_FILLED_SQUARE, QP_MARKER_OPEN_SQUARE,
    QP_MARKER_FILLED_TRIANGLE_UP, QP_MARKER_OPEN_TRIANGLE_UP,
    QP_MARKER_FILLED_TRIANGLE_DOWN, QP_MARKER_OPEN_TRIANGLE_DOWN,
    QP_MARKER_FILLED_DIAMOND, QP_MARKER_OPEN_DIAMOND,
    QP_MARKER_PLUS, QP_MARKER_ASTERISK, QP_MARKER_HASHTAG, QP_MARKER_EKS,
    QP_MARKER_VERTICAL_BAR, QP_MARKER_HORIZONTAL_BAR
};

/* 色階，數值與 qtplot::Colormap::Preset 相同 */
enum {
    QP_COLORMAP_VIRIDIS = 0, QP_COLORMAP_TURBO, QP_COLORMAP_JET, QP_COLORMAP_GRAYSCALE,
    QP_COLORMAP_BLUES, QP_COLORMAP_HOT, QP_COLORMAP_COOLWARM
};

/* 線型，數值與 qtplot::LinePattern 相同 */
enum {
    QP_LINE_SOLID = 0, QP_LINE_DASHED, QP_LINE_DENSELY_DASHED, QP_LINE_DOTTED,
    QP_LINE_DASH_DOT, QP_LINE_DASH_DOT_DOT
};

/* 圖例放在哪裡 */
enum { QP_LEGEND_INSIDE = 0, QP_LEGEND_OUTSIDE_RIGHT = 1, QP_LEGEND_OUTSIDE_BOTTOM = 2 };

/* 刻度線的方向 */
enum { QP_TICKS_OUTSIDE = 0, QP_TICKS_INSIDE = 1, QP_TICKS_CROSS = 2 };

/* 對數刻度的文字：自動、一般數字（1000）、上標（10³）、e 記號（1e3） */
enum { QP_LOG_LABEL_AUTO = 0, QP_LOG_LABEL_PLAIN, QP_LOG_LABEL_SUPERSCRIPT, QP_LOG_LABEL_E };

/* 窗函數，數值與 qtplot::WindowFunction 相同 */
enum {
    QP_WINDOW_RECTANGULAR = 0, QP_WINDOW_HANN, QP_WINDOW_HAMMING, QP_WINDOW_BLACKMAN,
    QP_WINDOW_BLACKMAN_HARRIS, QP_WINDOW_FLAT_TOP
};

/* ───────────── 基本 ───────────── */

/* "0.1.0" 這樣的字串，指向靜態記憶體，不必釋放。 */
QP_API const char* QP_CALL qp_version(void);
/* 這條執行緒上一個失敗呼叫的原因（UTF-8）。沒有錯誤時是空字串。 */
QP_API const char* QP_CALL qp_last_error(void);

/* 換 QtPlot 介面文字的語言（預設軸名之類）："en" "zh_CN" "ja" "ko" "fr" "de" "it" "es" "zh_TW"，
 * NULL 或空字串代表跟著系統。影響整個行程裡所有的圖，已經建好的圖下次畫時就會換。
 * 回傳 1 = 載入了翻譯，0 = 原始語言（繁體中文）或沒有那個語言。 */
QP_API int QP_CALL qp_set_language(const char* code);
/* 可以選的語言代碼，以逗號分隔（"de,en,es,…"），指向靜態記憶體，不必釋放。 */
QP_API const char* QP_CALL qp_available_languages(void);

QP_API QpPlot* QP_CALL qp_plot_create(void);
QP_API void    QP_CALL qp_plot_destroy(QpPlot* plot);
/* 只清圖元，標題、軸名、樣式保留。 */
QP_API int QP_CALL qp_plot_clear(QpPlot* plot);
/* 倒回剛建立的狀態。 */
QP_API int QP_CALL qp_plot_reset(QpPlot* plot);
QP_API int QP_CALL qp_plot_count(QpPlot* plot);

/* ───────────── 外觀 ───────────── */

QP_API int QP_CALL qp_set_title(QpPlot* plot, const char* text);
QP_API int QP_CALL qp_set_axis_label(QpPlot* plot, int axis, const char* text);
QP_API int QP_CALL qp_set_style(QpPlot* plot, int style);
QP_API int QP_CALL qp_set_legend(QpPlot* plot, int visible, int corner);
QP_API int QP_CALL qp_set_grid(QpPlot* plot, int visible);
QP_API int QP_CALL qp_set_antialias(QpPlot* plot, int enabled);
/* family 傳 NULL 或空字串代表不改；scale <= 0 代表不改。 */
QP_API int QP_CALL qp_set_font(QpPlot* plot, const char* family, double scale);

/* ───────────── 圖例樣式 ───────────── */

/* 擺法：placement 是 QP_LEGEND_*；horizontal 非 0 時橫向排；columns <= 0 代表不改。 */
QP_API int QP_CALL qp_set_legend_layout(QpPlot* plot, int placement, int horizontal, int columns);
/* 外框。width <= 0 代表不畫外框；pattern 是 QP_LINE_*。 */
QP_API int QP_CALL qp_set_legend_border(QpPlot* plot, uint32_t argb, double width, int pattern);
/* 底色。argb = 0（完全透明）代表不塗滿。 */
QP_API int QP_CALL qp_set_legend_background(QpPlot* plot, uint32_t argb);
/* 色塊符號：filled 為 0 時只畫框；outlineWidth > 0 時另外加一圈 outlineArgb 的外框（argb = 0 用黑色）。 */
QP_API int QP_CALL qp_set_legend_symbols(QpPlot* plot, int filled, uint32_t outlineArgb, double outlineWidth);
/* 圖例的字。family 傳 NULL 或空字串、size <= 0、argb = 0、bold／italic 傳負值，都代表那一項不改。 */
QP_API int QP_CALL qp_set_legend_font(QpPlot* plot, const char* family, double size, int bold, int italic, uint32_t argb);

/* ───────────── 座標軸與刻度 ───────────── */

/* 一次把整根軸（框線、刻度線、刻度數字、軸標題）換成同一個顏色。 */
QP_API int QP_CALL qp_set_axis_color(QpPlot* plot, int axis, uint32_t argb);
/* 刻度線：direction 是 QP_TICKS_*；長度傳負值代表不改；mirror 非 0 時繪圖區對面也畫一份。 */
QP_API int QP_CALL qp_set_axis_ticks(QpPlot* plot, int axis, int direction, double majorLength, double minorLength, int mirror);
/* 主刻度上的標記（QP_MARKER_*，QP_MARKER_NONE 代表不畫）。argb = 0 用軸的刻度線顏色。 */
QP_API int QP_CALL qp_set_axis_tick_marker(QpPlot* plot, int axis, int shape, double size, uint32_t argb);
/* 對數軸：資料用原始數值，軸以 log10 換算。labelFormat 是 QP_LOG_LABEL_*；labelMinor 非 0 時次刻度也印字。
 * 開了之後要呼叫 qp_autoscale 重算範圍（自動範圍只看正的部分）。 */
QP_API int QP_CALL qp_set_axis_log(QpPlot* plot, int axis, int enabled, int labelFormat, int labelMinor);

/* ───────────── 視野 ───────────── */

QP_API int QP_CALL qp_set_limits(QpPlot* plot, double xMin, double xMax, double yMin, double yMax);
QP_API int QP_CALL qp_get_limits(QpPlot* plot, double* xMin, double* xMax, double* yMin, double* yMax);
/* margin 是兩端各留的比例（0.05 = 5%）。 */
QP_API int QP_CALL qp_autoscale(QpPlot* plot, double margin);

/* 互動用。像素座標是「上一次 qp_render*」的畫布座標，所以至少要先畫過一次。 */
QP_API int QP_CALL qp_pixel_to_coord(QpPlot* plot, double px, double py, double* x, double* y);
QP_API int QP_CALL qp_coord_to_pixel(QpPlot* plot, double x, double y, double* px, double* py);
/* 拖曳平移：滑鼠移動了 (dxPx, dyPx) 像素，內容跟著走。 */
QP_API int QP_CALL qp_pan(QpPlot* plot, double dxPx, double dyPx);
/* 以像素 (px, py) 為定點縮放。factor < 1 放大（視野變小），> 1 縮小。 */
QP_API int QP_CALL qp_zoom(QpPlot* plot, double factor, double px, double py);

/* ───────────── 圖元 ───────────── */

QP_API QpItem* QP_CALL qp_add_scatter(QpPlot* plot, const double* xs, const double* ys, int count, uint32_t argb);
/* 只有點、沒有連線的散點。 */
QP_API QpItem* QP_CALL qp_add_markers(QpPlot* plot, const double* xs, const double* ys, int count, uint32_t argb);
/* 等間距的大量資料（每像素抽樣），x = i * period。 */
QP_API QpItem* QP_CALL qp_add_signal(QpPlot* plot, const double* ys, int count, double period, uint32_t argb);
/* 不等間距的大量資料。xs 必須遞增。 */
QP_API QpItem* QP_CALL qp_add_signal_xy(QpPlot* plot, const double* xs, const double* ys, int count, uint32_t argb);
/* 長條圖，第 i 根在 x = i。 */
QP_API QpItem* QP_CALL qp_add_bars(QpPlot* plot, const double* values, int count, uint32_t argb);
/* binCount <= 0 自動決定。 */
QP_API QpItem* QP_CALL qp_add_histogram(QpPlot* plot, const double* values, int count, int binCount, uint32_t argb);
/* values 是 row-major，大小 rows * cols。 */
QP_API QpItem* QP_CALL qp_add_heatmap(QpPlot* plot, const double* values, int rows, int cols, int colormap);
QP_API QpItem* QP_CALL qp_add_hline(QpPlot* plot, double y, uint32_t argb);
QP_API QpItem* QP_CALL qp_add_vline(QpPlot* plot, double x, uint32_t argb);
QP_API QpItem* QP_CALL qp_add_text(QpPlot* plot, const char* text, double x, double y);
/* 即時資料：固定容量的循環緩衝，新資料從右邊進來。 */
QP_API QpItem* QP_CALL qp_add_streamer(QpPlot* plot, int capacity, uint32_t argb);
/* 即時資料：只增不減，視野自動跟著資料走。 */
QP_API QpItem* QP_CALL qp_add_logger(QpPlot* plot, uint32_t argb);
/* 即時 FFT 頻譜（頻譜分析儀）。樣本用 qp_item_append 餵（xs 傳 NULL），頻譜自己算，
 * 每收到 fftSize/2 個新樣本更新一次。和 streamer 一樣可以從背景執行緒餵。 */
QP_API QpItem* QP_CALL qp_add_live_spectrum(QpPlot* plot, int fftSize, double sampleRate, uint32_t argb);

/* ───────────── 圖元屬性 ───────────── */

QP_API int QP_CALL qp_item_remove(QpPlot* plot, QpItem* item);
QP_API int QP_CALL qp_item_set_visible(QpPlot* plot, QpItem* item, int visible);
/* NULL 或空字串代表不進圖例。 */
QP_API int QP_CALL qp_item_set_legend(QpPlot* plot, QpItem* item, const char* text);
/* 線的顏色與粗細。width <= 0 代表不畫線。適用：scatter、signal、signal_xy、hline、vline、streamer、logger。 */
QP_API int QP_CALL qp_item_set_line(QpPlot* plot, QpItem* item, uint32_t argb, double width);
/* 標記。適用：scatter、markers、signal、signal_xy。 */
QP_API int QP_CALL qp_item_set_marker(QpPlot* plot, QpItem* item, int shape, double size, uint32_t argb);
/* 換掉資料（長度可以不同）。適用：scatter、markers、signal_xy（xs、ys）與 signal（只看 ys，xs 傳 NULL）。 */
QP_API int QP_CALL qp_item_set_data(QpPlot* plot, QpItem* item, const double* xs, const double* ys, int count);
/* 熱圖的色階範圍。min >= max 代表恢復自動。 */
QP_API int QP_CALL qp_item_set_color_range(QpPlot* plot, QpItem* item, double min, double max);
/* 餵資料給 streamer（xs 傳 NULL）或 logger（xs 傳 NULL 代表 x 接續遞增）。
 * 這個函式可以從背景執行緒呼叫（擷取卡、相機的執行緒），和 UI 執行緒的畫圖之間有鎖保護；
 * 但同一時間 UI 執行緒不能在加入或移除圖元。UI 執行緒用 qp_data_version() 判斷要不要重畫。 */
QP_API int QP_CALL qp_item_append(QpPlot* plot, QpItem* item, const double* xs, const double* ys, int count);
/* 即時頻譜的分析設定：window 是 QP_WINDOW_*；averaging 是指數平均的權重 0～0.99（平均功率）；
 * peakHold 非 0 時畫峰值保持，peakHoldDecayDb 是每次更新往下掉幾 dB（0 = 一直保持）。 */
QP_API int QP_CALL qp_spectrum_set_analysis(QpPlot* plot, QpItem* item, int window, double averaging,
                                            int peakHold, double peakHoldDecayDb);
/* 即時頻譜的顯示：Y 軸 minDb～maxDb；peakLabels 是自動標出幾個峰（0 = 不標）；
 * fillArgb 是曲線下方的填色（0 = 不填）。 */
QP_API int QP_CALL qp_spectrum_set_display(QpPlot* plot, QpItem* item, double minDb, double maxDb,
                                           int peakLabels, uint32_t fillArgb);

/* ───────────── 頻譜計算（不需要 QpPlot） ───────────── */

/* Welch 平均功率譜。segmentSize <= 0 或 >= count 時整段做一次 FFT。overlap 0～0.95。
 * 回傳頻率格數；outFreq（Hz）與 outDb（20·log10 振幅）有給、而且 capacity 夠大時才寫入，
 * 所以可以先傳 NULL 問要多大。失敗回傳負值。 */
QP_API int QP_CALL qp_welch(const double* samples, int count, double sampleRate, int segmentSize, double overlap,
                            int window, double* outFreq, double* outDb, int capacity);
/* 找頻譜峰值（依高度由大到小）。freqs 必須等間距遞增。minProminenceDb 是比附近雜訊底高出多少才算；
 * minSpacingHz 是兩個峰至少隔多遠；minFrequency 以下的不算。頻率經過拋物線內插，比格距準。
 * 結果寫進 outFreq／outDb（至少 maxPeaks 格），回傳找到幾個。 */
QP_API int QP_CALL qp_find_peaks(const double* freqs, const double* db, int count, int maxPeaks,
                                 double minProminenceDb, double minSpacingHz, double minFrequency,
                                 double* outFreq, double* outDb);

/* ───────────── 輸出 ───────────── */

/* 畫進呼叫端配置的緩衝區。格式是 premultiplied BGRA（每像素 4 bytes，
 * 等同 Windows 的 Format32bppPArgb）。stride 是一列的位元組數，至少 width * 4。
 * 這個函式直接畫在你的記憶體上，不會多複製一份。 */
QP_API int QP_CALL qp_render_bgra(QpPlot* plot, int width, int height, uint8_t* buffer, int stride);
/* scale > 1 輸出高解析度（例如 2 = 兩倍像素，字與線等比放大）。 */
QP_API int QP_CALL qp_save_png(QpPlot* plot, const char* path, int width, int height, double scale);
QP_API int QP_CALL qp_save_svg(QpPlot* plot, const char* path, int width, int height);
QP_API int QP_CALL qp_save_pdf(QpPlot* plot, const char* path, int width, int height);
/* 即時資料的版本號：streamer／logger 每進一批資料就變。和上次讀到的不同才需要重畫。 */
QP_API uint64_t QP_CALL qp_data_version(QpPlot* plot);
/* 上一次 render 花了幾毫秒。 */
QP_API double QP_CALL qp_last_render_ms(QpPlot* plot);

#ifdef __cplusplus
}
#endif

#endif /* QTPLOT_C_H */

/* capi_test.c — qtplot_c.dll 的檢查。
 *
 * 刻意寫成 C 而不是 C++：qtplot_c.h 要能被純 C 編譯器吃下去，
 * 這也是 C# 的 P/Invoke 看到的那一層。這支程式能編過，就代表標頭裡沒有混進 C++ 語法。
 */
#include "qtplot_c.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

static void check(int ok, const char* what) {
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        ++g_failures;
        if (qp_last_error()[0]) printf("        last error: %s\n", qp_last_error());
    }
}

static int nearly(double a, double b) { return fabs(a - b) < 1e-9 * (fabs(a) + fabs(b) + 1); }

int main(int argc, char** argv) {
    enum { N = 100, W = 400, H = 300 };
    double xs[N], ys[N];
    int i;
    QpPlot* p;
    QpItem* scatter;
    QpItem* bars;
    QpItem* streamer;
    uint8_t* pixels;
    double x0, x1, y0, y1, nx0, nx1, ny0, ny1, cx, cy, px, py;
    char pngPath[1024];

    for (i = 0; i < N; ++i) {
        xs[i] = i * 0.1;
        ys[i] = sin(xs[i]);
    }

    printf("qtplot_c %s\n", qp_version());
    check(strlen(qp_version()) > 0, "qp_version 有值");

    /* 語言 */
    check(strstr(qp_available_languages(), "de") != NULL && strstr(qp_available_languages(), "ja") != NULL,
          "qp_available_languages 列出德文、日文");
    check(qp_set_language("fr") == 1, "qp_set_language(\"fr\") 載入翻譯");
    check(qp_set_language("zh_TW") == 0, "qp_set_language(\"zh_TW\") 是原始語言");
    check(qp_set_language("xx") == 0, "沒有的語言回傳 0");
    check(qp_set_language(NULL) >= 0, "NULL = 跟著系統");
    qp_set_language("zh_TW");

    p = qp_plot_create();
    check(p != NULL, "建立 plot");

    scatter = qp_add_scatter(p, xs, ys, N, QP_AUTO_COLOR);
    check(scatter != NULL, "加入 scatter");
    check(qp_item_set_legend(p, scatter, "sin(x) 正弦") == QP_OK, "設圖例文字（UTF-8 中文）");
    check(qp_item_set_line(p, scatter, 0xFFD62728u, 2.0) == QP_OK, "設線色與線寬");
    check(qp_item_set_marker(p, scatter, QP_MARKER_OPEN_SQUARE, 5, QP_AUTO_COLOR) == QP_OK, "設標記");
    check(qp_set_title(p, "C API 測試") == QP_OK, "設標題");
    check(qp_set_axis_label(p, QP_AXIS_BOTTOM, "時間 (s)") == QP_OK, "設軸名");
    check(qp_set_legend(p, 1, QP_CORNER_TOP_RIGHT) == QP_OK, "開圖例");

    bars = qp_add_bars(p, ys, 10, 0x8000FF00u);
    check(bars != NULL, "加入長條");
    check(qp_item_set_marker(p, bars, QP_MARKER_FILLED_CIRCLE, 5, 0) == QP_ERR_WRONG_TYPE,
          "對長條設標記回傳 WRONG_TYPE");
    check(qp_last_error()[0] != '\0', "失敗時 qp_last_error 有訊息");
    check(qp_plot_count(p) == 2, "圖元數 = 2");

    /* 參數檢查 */
    check(qp_add_scatter(p, NULL, ys, N, 0) == NULL, "xs 為 NULL 時回傳 NULL");
    check(qp_add_heatmap(p, ys, 3, 0, QP_COLORMAP_VIRIDIS) == NULL, "熱圖 cols = 0 被拒絕");
    check(qp_set_limits(p, 1, 0, 0, 1) == QP_ERR_ARG, "min > max 被拒絕");
    check(qp_set_title(NULL, "x") == QP_ERR_NULL, "plot 為 NULL 回傳 ERR_NULL");

    /* 還沒畫過，互動函式要拒絕 */
    check(qp_pan(p, 10, 0) == QP_ERR_ARG, "畫之前 qp_pan 被拒絕");

    /* 畫到自己的緩衝區 */
    check(qp_autoscale(p, 0.05) == QP_OK, "autoscale");
    pixels = (uint8_t*)calloc((size_t)W * H, 4);
    check(qp_render_bgra(p, W, H, pixels, W * 4) == QP_OK, "render 到 BGRA 緩衝區");
    {
        /* 白底上畫了東西：至少有一定比例的像素不是純白 */
        int nonWhite = 0;
        for (i = 0; i < W * H; ++i) {
            const uint8_t* px4 = pixels + i * 4;
            if (px4[0] != 255 || px4[1] != 255 || px4[2] != 255) ++nonWhite;
        }
        check(nonWhite > W * H / 100, "緩衝區裡確實畫了東西");
        check(pixels[3] == 255, "背景不透明（alpha = 255）");
    }
    check(qp_render_bgra(p, W, H, pixels, W * 4 - 1) == QP_ERR_ARG, "stride 太小被拒絕");
    check(qp_last_render_ms(p) > 0, "有記錄 render 時間");

    /* 座標換算來回一致 */
    check(qp_coord_to_pixel(p, 3.0, 0.5, &px, &py) == QP_OK, "coord_to_pixel");
    check(qp_pixel_to_coord(p, px, py, &cx, &cy) == QP_OK, "pixel_to_coord");
    check(nearly(cx, 3.0) && nearly(cy, 0.5), "座標 → 像素 → 座標 來回一致");

    /* 平移：往右拖，視野往左移（x 變小），寬度不變 */
    qp_get_limits(p, &x0, &x1, &y0, &y1);
    check(qp_pan(p, 50, 0) == QP_OK, "qp_pan");
    qp_get_limits(p, &nx0, &nx1, &ny0, &ny1);
    check(nx0 < x0 && nearly(nx1 - nx0, x1 - x0), "往右拖時 x 範圍左移且寬度不變");
    check(nearly(ny0, y0) && nearly(ny1, y1), "水平拖曳不動 y");

    /* 縮放：定點不動，範圍變一半 */
    qp_get_limits(p, &x0, &x1, &y0, &y1);
    check(qp_pixel_to_coord(p, 200, 150, &cx, &cy) == QP_OK, "取縮放定點");
    check(qp_zoom(p, 0.5, 200, 150) == QP_OK, "qp_zoom");
    qp_get_limits(p, &nx0, &nx1, &ny0, &ny1);
    check(nearly(nx1 - nx0, (x1 - x0) * 0.5), "x 範圍變一半");
    check(nearly((cx - nx0) / (nx1 - nx0), (cx - x0) / (x1 - x0)), "定點在 x 方向的相對位置不變");

    /* 換資料 */
    check(qp_item_set_data(p, scatter, xs, ys, N / 2) == QP_OK, "scatter 換資料");
    check(qp_item_set_data(p, bars, xs, ys, 5) == QP_ERR_WRONG_TYPE, "長條不能 set_data");

    /* 即時資料 */
    streamer = qp_add_streamer(p, 50, QP_AUTO_COLOR);
    check(streamer != NULL, "加入 streamer");
    check(qp_item_append(p, streamer, NULL, ys, N) == QP_OK, "streamer append");
    check(qp_item_append(p, scatter, NULL, ys, N) == QP_ERR_WRONG_TYPE, "scatter 不能 append");

    /* 存檔 */
    {
        const char* dir = (argc > 1) ? argv[1] : ".";
        snprintf(pngPath, sizeof pngPath, "%s/capi_test.png", dir);
        check(qp_save_png(p, pngPath, W, H, 2.0) == QP_OK, "存 PNG（2 倍解析度）");
        {
            FILE* f = fopen(pngPath, "rb");
            long size = 0;
            if (f) {
                fseek(f, 0, SEEK_END);
                size = ftell(f);
                fclose(f);
                remove(pngPath);
            }
            check(size > 1000, "PNG 檔有內容");
        }
        check(qp_save_png(p, "Z:/no/such/dir/x.png", W, H, 1.0) == QP_ERR_IO, "寫不進去的路徑回傳 ERR_IO");
    }

    /* 移除與失效的 handle */
    check(qp_item_remove(p, bars) == QP_OK, "移除長條");
    check(qp_item_set_visible(p, bars, 0) == QP_ERR_BAD_ITEM, "移除後的 handle 回傳 BAD_ITEM");
    check(qp_plot_clear(p) == QP_OK, "clear");
    check(qp_plot_count(p) == 0, "clear 後圖元數 = 0");
    check(qp_item_set_legend(p, scatter, "x") == QP_ERR_BAD_ITEM, "clear 後舊 handle 回傳 BAD_ITEM，不會當掉");

    /* 別張圖的 handle */
    {
        QpPlot* other = qp_plot_create();
        QpItem* line = qp_add_hline(other, 0.5, QP_AUTO_COLOR);
        check(qp_item_set_visible(p, line, 0) == QP_ERR_BAD_ITEM, "拿別張圖的 item 回傳 BAD_ITEM");
        qp_plot_destroy(other);
    }

    free(pixels);
    qp_plot_destroy(p);
    qp_plot_destroy(NULL); /* 傳 NULL 不會當掉 */

    printf(g_failures ? "\n%d 項失敗\n" : "\n全部通過\n", g_failures);
    return g_failures;
}

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

    /* 圖例樣式、座標軸與刻度 */
    {
        QpItem* s2 = qp_add_scatter(p, xs, ys, N, QP_AUTO_COLOR);
        qp_item_set_legend(p, s2, "sin");
        qp_set_legend(p, 1, QP_CORNER_TOP_RIGHT);
        check(qp_set_legend_layout(p, QP_LEGEND_OUTSIDE_BOTTOM, 1, 0) == QP_OK, "圖例放到下方、橫排");
        check(qp_set_legend_layout(p, 9, 0, 0) == QP_ERR_ARG, "不認得的擺法回傳 ERR_ARG");
        check(qp_set_legend_border(p, 0xFFC81E1Eu, 2.0, QP_LINE_DASHED) == QP_OK, "圖例外框：紅色虛線");
        check(qp_set_legend_border(p, 0, 1.0, 42) == QP_ERR_ARG, "不認得的線型回傳 ERR_ARG");
        check(qp_set_legend_background(p, 0) == QP_OK, "圖例底色：不塗");
        check(qp_set_legend_symbols(p, 0, 0, 0) == QP_OK, "色塊只畫框");
        check(qp_set_legend_font(p, "Georgia", 12, -1, 1, 0) == QP_OK, "圖例字型：Georgia 斜體");

        check(qp_set_axis_color(p, QP_AXIS_LEFT, 0xFF1F77B4u) == QP_OK, "整根左軸換顏色");
        check(qp_set_axis_color(p, 7, 0xFF000000u) == QP_ERR_ARG, "不認得的軸回傳 ERR_ARG");
        check(qp_set_axis_ticks(p, QP_AXIS_BOTTOM, QP_TICKS_INSIDE, 8, -1, 1) == QP_OK, "下軸刻度朝內、鏡像");
        check(qp_set_axis_ticks(p, QP_AXIS_BOTTOM, 5, 8, 4, 0) == QP_ERR_ARG, "不認得的方向回傳 ERR_ARG");
        check(qp_set_axis_tick_marker(p, QP_AXIS_BOTTOM, QP_MARKER_FILLED_CIRCLE, 6, 0) == QP_OK, "主刻度畫圓點");

        /* 對數軸：y = 10^(x/2)，自動範圍之後 1 和 10 在畫面上的距離 = 10 和 100 的距離 */
        {
            double lx[40], ly[40], p1, p10, p100, dummy;
            int k;
            QpItem* logItem;
            qp_plot_clear(p);
            for (k = 0; k < 40; ++k) {
                lx[k] = k * 0.2;
                ly[k] = pow(10.0, lx[k] / 2);
            }
            logItem = qp_add_scatter(p, lx, ly, 40, QP_AUTO_COLOR);
            check(logItem != NULL, "加入對數資料");
            check(qp_set_axis_log(p, QP_AXIS_LEFT, 1, QP_LOG_LABEL_SUPERSCRIPT, 0) == QP_OK, "左軸改成對數軸");
            qp_autoscale(p, 0.05);
            qp_get_limits(p, &x0, &x1, &y0, &y1);
            check(y0 > 0 && y0 < 1 && y1 > 10000, "對數軸的自動範圍是正的");
            qp_render_bgra(p, W, H, pixels, W * 4);
            qp_coord_to_pixel(p, 0, 1, &dummy, &p1);
            qp_coord_to_pixel(p, 0, 10, &dummy, &p10);
            qp_coord_to_pixel(p, 0, 100, &dummy, &p100);
            check(fabs((p1 - p10) - (p10 - p100)) < 1e-6, "每個十的次方一樣長");
            qp_zoom(p, 0.5, W / 2.0, p10);
            qp_get_limits(p, &nx0, &nx1, &ny0, &ny1);
            check(fabs(log10(ny1 / ny0) - log10(y1 / y0) / 2) < 1e-6, "qp_zoom 在對數空間縮放");
            check(qp_set_axis_log(p, QP_AXIS_LEFT, 0, QP_LOG_LABEL_AUTO, 0) == QP_OK, "關掉對數軸");
        }
        qp_plot_clear(p);
    }

    /* 頻譜：Welch、找峰值、即時頻譜 */
    {
        enum { M = 4096, SEG = 512 };
        static double sig[M], freq[SEG / 2 + 1], db[SEG / 2 + 1];
        double pf[4], pd[4];
        int k, bins, found;
        QpItem* live;
        for (k = 0; k < M; ++k) /* 125 Hz（落在 512 點的第 64 格）與 −20 dB 的 312.5 Hz */
            sig[k] = sin(2 * 3.14159265358979 * 125 * k / 1000.0) + 0.1 * sin(2 * 3.14159265358979 * 312.5 * k / 1000.0);

        bins = qp_welch(sig, M, 1000, SEG, 0.5, QP_WINDOW_HANN, NULL, NULL, 0);
        check(bins == SEG / 2 + 1, "qp_welch 先問要多大");
        check(qp_welch(sig, M, 1000, SEG, 0.5, QP_WINDOW_HANN, freq, db, bins) == bins, "qp_welch 寫入結果");
        check(nearly(freq[64], 125) && fabs(db[64]) < 0.1, "125 Hz 振幅 1（0 dB）");
        check(qp_welch(sig, M, 0, SEG, 0.5, QP_WINDOW_HANN, NULL, NULL, 0) == QP_ERR_ARG, "取樣率 0 回傳 ERR_ARG");

        found = qp_find_peaks(freq, db, bins, 4, 15, 5, 0, pf, pd);
        check(found == 2, "找到兩個峰");
        check(found >= 2 && fabs(pf[0] - 125) < 0.5 && fabs(pf[1] - 312.5) < 0.5 && fabs(pd[1] + 20) < 0.6,
              "峰的頻率與高度正確");

        live = qp_add_live_spectrum(p, 1024, 1000, QP_AUTO_COLOR);
        check(live != NULL, "加入即時頻譜");
        check(qp_add_live_spectrum(p, 4, 1000, 0) == NULL, "FFT 長度太小回傳 NULL");
        check(qp_spectrum_set_analysis(p, live, QP_WINDOW_HANN, 0.5, 1, 0.3) == QP_OK, "即時頻譜：平均與峰值保持");
        check(qp_spectrum_set_display(p, live, -100, 10, 2, 0x401F77B4u) == QP_OK, "即時頻譜：顯示範圍、標峰、填色");
        check(qp_spectrum_set_display(p, live, 0, -10, 0, 0) == QP_ERR_ARG, "maxDb <= minDb 回傳 ERR_ARG");
        {
            uint64_t v0 = qp_data_version(p);
            check(qp_item_append(p, live, NULL, sig, M) == QP_OK, "餵樣本給即時頻譜");
            check(qp_data_version(p) != v0, "餵了之後資料版本變了");
        }
        check(qp_render_bgra(p, W, H, pixels, W * 4) == QP_OK, "畫出即時頻譜");
        check(qp_spectrum_set_display(p, scatter, -100, 0, 0, 0) == QP_ERR_BAD_ITEM, "失效的 handle 回傳 BAD_ITEM");
        qp_plot_clear(p);
    }

    free(pixels);
    qp_plot_destroy(p);
    qp_plot_destroy(NULL); /* 傳 NULL 不會當掉 */

    printf(g_failures ? "\n%d 項失敗\n" : "\n全部通過\n", g_failures);
    return g_failures;
}

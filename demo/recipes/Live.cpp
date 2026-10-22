// Live.cpp — 即時資料的範例。
//
// 這幾支用 QTPLOT_RECIPE_LIVE：frame == 0 是建立，之後每隔一段時間加一。
// 在 Cookbook 裡會真的跑起來；匯出 PNG 時則先跑 240 個 frame 再存檔，
// 兩邊是同一份程式。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

#include <cmath>

using namespace qtplot;

namespace {

/// 取得圖上唯一的 DataLogger／DataStreamer（frame > 0 時要拿它來加資料）。
template <typename T>
T* firstOf(Plot& plot) {
    for (IPlottable* p : plot.plottables())
        if (auto* t = dynamic_cast<T*>(p)) return t;
    return nullptr;
}

/// 一個看起來像感測器讀值的訊號：慢速漂移 + 週期 + 雜訊。
double sensorValue(int frame) {
    const double t = frame * 0.05;
    return 20 + 3 * std::sin(t * 0.3) + 0.8 * std::sin(t * 2.7) +
           (std::sin(frame * 12.9898) * 43758.5453 - std::floor(std::sin(frame * 12.9898) * 43758.5453) - 0.5);
}

}  // namespace

QTPLOT_RECIPE_LIVE("即時", "累積記錄", "DataLogger：顯示全部",
                   "資料持續累積，視野跟著變寬。Full 模式適合「我要看完整歷史」的場合。"
                   "紅點標出最新一筆。", 30) {
    if (frame == 0) {
        auto* log = plot.addDataLogger();
        log->view = LiveView::Full;
        log->legendText = RT("感測器讀值");
        plot.legend.isVisible = true;
        plot.leftAxis()->label.text = RT("溫度 (°C)");
        plot.bottomAxis()->label.text = RT("取樣點");
        return;
    }
    if (auto* log = firstOf<DataLogger>(plot)) log->add(sensorValue(frame));
}

QTPLOT_RECIPE_LIVE("即時", "累積記錄", "DataLogger：連續捲動",
                   "Slide 模式：固定寬度，永遠貼著最新的資料。"
                   "Y 軸只看「可視範圍內」的資料 —— 否則早就捲出去的尖峰會一直把軸撐著，"
                   "最近的變化反而被壓扁。", 30) {
    if (frame == 0) {
        auto* log = plot.addDataLogger();
        log->view = LiveView::Slide;
        log->viewWidth = 150;
        log->legendText = RT("Slide（連續捲動）");
        plot.legend.isVisible = true;
        plot.leftAxis()->label.text = RT("溫度 (°C)");
        return;
    }
    if (auto* log = firstOf<DataLogger>(plot)) {
        // 第 200 筆附近插一個尖峰，看看它捲出去之後 Y 軸會不會縮回來
        const double spike = (frame > 200 && frame < 206) ? 12.0 : 0.0;
        log->add(sensorValue(frame) + spike);
    }
}

QTPLOT_RECIPE_LIVE("即時", "累積記錄", "DataLogger：跳頁",
                   "Jump 模式：一樣是固定寬度，但畫面一格一格換，不是連續滑動。"
                   "要盯著讀數值時 Jump 比較不暈；看趨勢的話 Slide 比較順。"
                   "jumpFraction 控制每次往前跳多遠。", 30) {
    if (frame == 0) {
        auto* log = plot.addDataLogger(QColor(214, 39, 40));
        log->view = LiveView::Jump;
        log->viewWidth = 150;
        log->jumpFraction = 0.6;
        log->legendText = RT("Jump（整段跳）");
        plot.legend.isVisible = true;
        plot.leftAxis()->label.text = RT("溫度 (°C)");
        return;
    }
    if (auto* log = firstOf<DataLogger>(plot)) log->add(sensorValue(frame));
}

QTPLOT_RECIPE_LIVE("即時", "波形", "DataStreamer：掃描模式",
                   "固定長度的循環緩衝，記憶體用量不會成長。"
                   "Wipe 像示波器：寫入點從左掃到右，掃到底就回到左邊覆蓋舊的，"
                   "紅線標出目前的寫入位置。畫面本身不動，所以看得清波形的細節。", 20) {
    if (frame == 0) {
        // 容量刻意設小一點，匯出的靜態圖才來得及繞過一輪，看得到寫入點的紅線
        auto* s = plot.addDataStreamer(150);
        s->mode = DataStreamer::Mode::Wipe;
        s->legendText = RT("訊號");
        plot.legend.isVisible = true;
        plot.bottomAxis()->label.text = RT("緩衝位置");
        return;
    }
    if (auto* s = firstOf<DataStreamer>(plot)) {
        const double t = frame * 0.08;
        s->add(std::sin(t) + 0.35 * std::sin(t * 5.3) + 0.12 * std::sin(t * 17));
    }
}

QTPLOT_RECIPE_LIVE("即時", "波形", "DataStreamer：捲動模式",
                   "Scroll 像紙帶記錄器：最新的永遠在右邊，整段往左移。"
                   "比 Wipe 直覺，代價是每一幀整張圖都在動，細節比較難看。", 20) {
    if (frame == 0) {
        auto* s = plot.addDataStreamer(300, QColor(44, 160, 44));
        s->mode = DataStreamer::Mode::Scroll;
        s->legendText = RT("訊號");
        plot.legend.isVisible = true;
        plot.bottomAxis()->label.text = RT("取樣點");
        return;
    }
    if (auto* s = firstOf<DataStreamer>(plot)) {
        const double t = frame * 0.08;
        s->add(std::sin(t) * (1 + 0.3 * std::sin(t * 0.17)));
    }
}

QTPLOT_RECIPE_LIVE("即時", "波形", "多軌波形",
                   "三個 DataStreamer 各自加上固定偏移，疊在同一張圖上，"
                   "就是多軌的記錄器畫面。", 20) {
    if (frame == 0) {
        const QStringList names{RT("通道 1"), RT("通道 2"),
                                RT("通道 3")};
        for (int i = 0; i < 3; ++i) {
            auto* s = plot.addDataStreamer(150);
            s->mode = DataStreamer::Mode::Wipe;
            s->legendText = names[i];
            s->autoScaleY = false;  // 偏移是固定的，讓 Y 軸別跟著跳
            s->wipeMarker = (i == 0) ? s->wipeMarker : LineStyle::none();
        }
        plot.leftAxis()->setRange(-1.5, 7.5);
        plot.legend.isVisible = true;
        return;
    }
    const double t = frame * 0.08;
    int ch = 0;
    for (IPlottable* p : plot.plottables()) {
        if (auto* s = dynamic_cast<DataStreamer*>(p)) {
            const double offset = ch * 3.0;
            switch (ch) {
            case 0: s->add(offset + std::sin(t)); break;
            case 1: s->add(offset + ((std::fmod(t, 2.0) < 1.0) ? 0.8 : -0.8)); break;
            default: s->add(offset + 0.9 * std::sin(t * 3.1) * std::exp(-std::fmod(t, 3.0))); break;
            }
            ++ch;
        }
    }
}

QTPLOT_RECIPE_LIVE("即時", "頻譜", "即時 FFT 頻譜（峰值保持）",
                   "LiveSpectrum 是頻譜分析儀的畫面：樣本用 addRange() 餵進去（擷取卡的執行緒也可以），"
                   "每收到半個 FFT 長度就重算一次。\n"
                   "藍線是平均後的頻譜（averaging = 0.5，平均的是功率）：移動中的音在平均之下會拖出一條尾巴。"
                   "紅色虛線是峰值保持：掃過的頻率會留下痕跡，每次更新往下掉 0.4 dB；"
                   "三角形標出最高的峰（peakLabels = 3，最多三個）。\n"
                   "1 kHz 的固定音只有 −40 dB，一樣會被標出來。", 30) {
    const double fs = 8000;
    const int block = 512;
    if (frame == 0) {
        auto* s = plot.addLiveSpectrum(1024, fs);
        s->averaging = 0.5;
        s->showPeakHold = true;
        s->peakHoldDecayDb = 0.4;
        s->peakLabels = 3;
        s->peakMinFrequency = 50;
        s->minDb = -110;
        s->maxDb = 10;
        s->fill = FillStyle(QColor(31, 119, 180, 45));
        s->legendText = RT("平均頻譜");
        s->peakHoldLegendText = RT("峰值保持");
        plot.legend.isVisible = true;
        plot.bottomAxis()->label.text = RT("頻率（Hz）");
        plot.leftAxis()->label.text = RT("振幅（dB）");
        return;
    }
    auto* s = firstOf<LiveSpectrum>(plot);
    if (!s) return;
    // 一個在 700～2300 Hz 之間慢慢來回掃的音（帶二次諧波）、1 kHz 的弱音與白雜訊。
    // 掃太快的話，一個 FFT 長度裡頻率就走了好幾格，峰會被抹成一片平台。
    // 相位用樣本序號直接算，不必在 frame 之間記狀態
    QVector<double> v(block);
    for (int i = 0; i < block; ++i) {
        const qint64 idx = static_cast<qint64>(frame - 1) * block + i;
        const double t = idx / fs;
        const double sweepPhase = 2 * kPi * (1500 * t - 800 / (2 * kPi * 0.03) * std::cos(2 * kPi * 0.03 * t));
        const double h = std::sin(idx * 12.9898) * 43758.5453;  // 確定性的雜訊（每次跑都一樣）
        const double noise = (h - std::floor(h) - 0.5) * 0.02;
        v[i] = 0.5 * std::sin(sweepPhase) + 0.08 * std::sin(2 * sweepPhase) +
               0.01 * std::sin(2 * kPi * 1000 * t) + noise;
    }
    s->addRange(v);
}

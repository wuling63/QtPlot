// Live.h — 即時資料：持續累積的 DataLogger、固定長度循環的 DataStreamer 與即時頻譜 LiveSpectrum。
//
// 這兩個圖元和其他的不一樣：它們會「自己調整軸」。資料一直進來，視野要跟著走，
// 這件事必須在版面計算「之前」做完 —— 否則刻度是用上一幀的範圍算的，
// 畫面會慢一拍，而且刻度與資料對不上。IPlottable::beforeLayout() 就是這個時機。
//
// 執行緒：add*()、clear()、setCapacity()、setY() 可以從任何執行緒呼叫（例如擷取卡、相機的執行緒），
// 和 GUI 執行緒的重畫之間有鎖保護。背景執行緒不能直接叫 widget 重畫，
// 請用 PlotWidget::setAutoRefresh(間隔毫秒)：GUI 執行緒定時檢查，有新資料才重畫。
// 其他成員（樣式、view、period……）一樣只能在 GUI 執行緒改。
#pragma once

#include "Plottable.h"
#include "Spectral.h"

#include <QMutex>

#include <atomic>

namespace qtplot {

/// 視野怎麼跟著新資料走。
enum class LiveView {
    Full,    ///< 永遠顯示全部資料（視野越來越寬）
    Jump,    ///< 固定寬度，資料超出右邊界時整段往前跳一格
    Slide,   ///< 固定寬度，永遠貼著最新的資料連續捲動
    Manual   ///< 不碰軸，使用者自己管
};

/// 持續累積的資料。只會變長，不會丟掉舊的。
///
/// 適合記錄型的用途（溫度、負載、感測器讀值）。要固定記憶體用量請改用 DataStreamer。
class DataLogger : public IPlottable {
public:
    /// X 自動算成 count * period + xOffset。
    void add(double y);
    /// 任意 X。X 必須遞增 —— 繪製時會依賴這一點做可視範圍裁切。
    void add(double x, double y);
    void addRange(const QVector<double>& ys);
    /// 一次加入一批 (x, y)。
    void addRange(const QVector<double>& xs, const QVector<double>& ys);
    /// 改掉已經記下來的某一筆（例如最新一筆是「暫定值」，之後要修正）。超出範圍就不做事。
    void setY(int index, double y);
    void clear();

    int count() const;
    /// 直接拿內部陣列。背景執行緒還在寫的時候不能用 —— 請改用 snapshot()。
    const QVector<double>& xs() const { return m_xs; }
    const QVector<double>& ys() const { return m_ys; }
    /// 複製一份目前的資料（有上鎖，任何執行緒都能叫）。
    void snapshot(QVector<double>& xs, QVector<double>& ys) const;
    /// 最新一筆的值。沒有資料時回傳 NaN。
    double latest() const;

    double period = 1.0;
    double xOffset = 0.0;

    LiveView view = LiveView::Full;
    /// Jump / Slide 用的可視寬度（資料座標單位）。
    double viewWidth = 100;
    /// Jump 模式跳一次往前留多少（佔 viewWidth 的比例）。
    double jumpFraction = 0.6;
    /// Y 軸跟著「可視範圍內」的資料自動調整。
    bool autoScaleY = true;
    double yPadding = 0.1;

    LineStyle line{QColor(31, 119, 180), 1.5};
    MarkerStyle marker;
    /// 在最新一筆資料上畫一個標記，一眼看出目前在哪。
    MarkerStyle latestMarker{MarkerShape::FilledCircle, 7, QColor(214, 39, 40)};

    void beforeLayout() override;
    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    quint64 dataVersion() const override { return m_version.load(std::memory_order_relaxed); }

private:
    void addLocked(double x, double y);

    mutable QMutex m_mutex;
    std::atomic<quint64> m_version{1};
    QVector<double> m_xs;
    QVector<double> m_ys;
    double m_viewRight = 0;  ///< Jump 模式目前的右邊界
};

/// 固定長度的循環緩衝。新資料蓋掉最舊的，記憶體用量不會成長。
///
/// 適合波形顯示（示波器、音訊、心電圖）。
class DataStreamer : public IPlottable {
public:
    explicit DataStreamer(int capacity = 1000);

    void add(double y);
    void addRange(const QVector<double>& ys);
    void clear();
    void setCapacity(int capacity);

    int capacity() const;
    /// 目前緩衝內有幾筆（最多等於 capacity）。
    int size() const;
    /// 總共加入過幾筆（不會因為覆蓋而減少）。
    qint64 totalAdded() const;
    double latest() const;

    enum class Mode {
        Wipe,   ///< 像示波器：寫入點從左掃到右，掃到底就回到左邊重新覆蓋
        Scroll  ///< 像紙帶記錄器：最新的永遠在右邊，整段往左移
    };
    Mode mode = Mode::Wipe;

    double period = 1.0;
    double xOffset = 0.0;

    /// Y 軸跟著緩衝內的資料走。
    bool autoScaleY = true;
    double yPadding = 0.1;
    /// Y 軸只放大不縮小 —— 波形突然變小時軸不會一直抖。
    bool growOnly = true;

    LineStyle line{QColor(31, 119, 180), 1.5};
    /// Wipe 模式下，寫入點附近留一段空白，才看得出新舊的交界。
    int wipeGap = 4;
    /// 在寫入點畫一條垂直線。
    LineStyle wipeMarker{QColor(214, 39, 40, 160), 1.0};

    void beforeLayout() override;
    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    quint64 dataVersion() const override { return m_version.load(std::memory_order_relaxed); }

private:
    void addLocked(double y);
    void resetLocked(int capacity);

    mutable QMutex m_mutex;
    std::atomic<quint64> m_version{1};
    QVector<double> m_data;
    qint64 m_count = 0;  ///< 總共加入過幾筆
    CoordinateRange m_seenY;
};

/// 有自己 X 值的循環緩衝。
///
/// DataStreamer 的 X 是由索引算出來的（等間距）。資料的時間戳不規則時
/// （事件驅動、網路延遲、感測器丟包）就要用這個，自己帶 X 進來。
class DataStreamerXY : public IPlottable {
public:
    explicit DataStreamerXY(int capacity = 1000);

    void add(double x, double y);
    void clear();
    void setCapacity(int capacity);

    /// 一次加入一批 (x, y)，只上一次鎖。
    void addRange(const QVector<double>& xs, const QVector<double>& ys);

    int capacity() const;
    int size() const;
    qint64 totalAdded() const;
    /// 最新一筆。沒有資料時回傳 isFinite() == false 的座標。
    Coordinates latest() const;

    /// 可視寬度（資料座標）。<= 0 代表顯示緩衝裡的全部。
    double viewWidth = 0;
    bool autoScaleY = true;
    double yPadding = 0.1;
    LineStyle line{QColor(31, 119, 180), 1.5};
    MarkerStyle marker;

    void beforeLayout() override;
    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    quint64 dataVersion() const override { return m_version.load(std::memory_order_relaxed); }

private:
    /// 由舊到新取出緩衝內容（有上鎖）。
    void collect(QVector<double>& xs, QVector<double>& ys) const;
    void resetLocked(int capacity);

    mutable QMutex m_mutex;
    std::atomic<quint64> m_version{1};
    QVector<double> m_xs;
    QVector<double> m_ys;
    qint64 m_count = 0;
};

/// 即時 FFT 頻譜（頻譜分析儀的畫面）。
///
/// 樣本從 add()／addRange() 餵進來（任何執行緒都可以，例如擷取卡的回呼）。
/// 每收到 updateInterval 個新樣本，就拿最新的 fftSize 個樣本做一次頻譜，
/// 再依 averaging 做指數平均、更新峰值保持。FFT 在 add 的那個執行緒裡算，
/// 所以平均與峰值保持跟畫面多久重畫一次無關 —— 視窗被擋住、重畫變慢，量測結果也一樣。
///
/// 軸：autoAxes 開著時 X 固定是 0～Nyquist、Y 是 minDb～maxDb。
/// 要讓使用者自己縮放就關掉，或把軸設成 locked。
class LiveSpectrum : public IPlottable {
public:
    explicit LiveSpectrum(int fftSize = 1024, double sampleRate = 1000);

    // ── 餵資料（任何執行緒） ──
    void add(double sample);
    void addRange(const QVector<double>& samples);
    /// 清掉緩衝、平均與峰值保持。
    void clear();
    /// 只清掉峰值保持（換了待測物之後用）。
    void resetPeakHold();

    /// 改 FFT 長度（至少 8）。會清掉緩衝與平均。頻率格距 = sampleRate / fftSize。
    void setFftSize(int n);
    int fftSize() const;
    /// 改取樣率（Hz）。會清掉緩衝與平均。
    void setSampleRate(double fs);
    double sampleRate() const;

    // ── 讀結果（任何執行緒，回傳的是複本） ──
    /// 平均後的頻譜。還沒收滿 fftSize 個樣本前是空的。segments 是至今算了幾次。
    Periodogram spectrum() const;
    /// 每一格看過的最高 dB（和 spectrum().frequencies 對齊）。
    QVector<double> peakHold() const;
    /// 至今算了幾次頻譜。
    qint64 frameCount() const;

    // ── 分析設定（只能在 GUI 執行緒改；改了從下一次更新開始生效） ──
    WindowFunction window = WindowFunction::Hann;
    /// dB 的 0 點（例如 1 V 就是 dBV）。
    double reference = 1.0;
    /// 指數平均的權重，0（不平均）～0.99。0.8 大約等於平均最近 10 次。
    /// 平均的是功率，不是 dB。
    double averaging = 0.0;
    /// 每收到這麼多新樣本算一次。<= 0 時用 fftSize / 2（50% 重疊）。
    int updateInterval = 0;
    /// 峰值保持每次更新往下掉幾 dB。0 是一直保持，直到 resetPeakHold()。
    double peakHoldDecayDb = 0;

    // ── 外觀 ──
    double minDb = -120;
    double maxDb = 0;
    bool autoAxes = true;
    LineStyle line{QColor(31, 119, 180), 1.2};
    /// 曲線下方的填色（預設不填）。
    FillStyle fill;
    bool showPeakHold = false;
    LineStyle peakHoldLine{QColor(214, 39, 40, 170), 1.0, LinePattern::Dashed};
    /// 峰值保持在圖例裡的名稱（空字串不進圖例）。
    QString peakHoldLegendText;

    /// 自動標出最高的幾個峰（0 代表不標）。條件見 findSpectralPeaks()。
    int peakLabels = 0;
    double peakMinProminenceDb = 10;
    /// 兩個標記至少隔這麼遠（Hz）。<= 0 時用 Nyquist 的 2%，免得同一個峰的裙邊被標兩次。
    double peakMinSpacingHz = 0;
    /// 低於這個頻率的峰不標（避開直流與漂移）。
    double peakMinFrequency = 0;
    MarkerStyle peakMarker{MarkerShape::FilledTriangleDown, 8, QColor(214, 39, 40)};
    FontStyle peakFont;

    void beforeLayout() override;
    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    quint64 dataVersion() const override { return m_version.load(std::memory_order_relaxed); }
    void visitFonts(const FontVisitor& visit) override { visit(peakFont); }

private:
    void resetLocked();
    void addLocked(double sample);
    void computeLocked();

    mutable QMutex m_mutex;
    std::atomic<quint64> m_version{1};
    int m_fftSize = 1024;
    double m_sampleRate = 1000;
    QVector<double> m_buffer;  ///< 循環緩衝，長度 = fftSize
    qint64 m_count = 0;        ///< 總共收過幾個樣本
    int m_sinceUpdate = 0;     ///< 上次算頻譜之後又收了幾個
    qint64 m_frames = 0;
    QVector<double> m_frequencies;
    QVector<double> m_avgPower;
    QVector<double> m_peakDb;
};

}  // namespace qtplot

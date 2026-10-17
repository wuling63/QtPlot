// Live.h — 即時資料：持續累積的 DataLogger 與固定長度循環的 DataStreamer。
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

}  // namespace qtplot

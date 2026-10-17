// Plottable.h — 圖元介面。
//
// 要新增一種圖，只需繼承 IPlottable 並實作 axisLimits() 與 render()。
// 不要在圖元裡存像素座標或直接讀 Plot 的狀態：需要的東西都由 RenderPack 傳進來。
#pragma once

#include "Axis.h"
#include "Core.h"

namespace qtplot {

class Plot;

/// 命中測試要怎麼比距離。
enum class HitMode {
    /// 二維距離最近的點。游標在哪裡就抓哪裡，適合散點。
    NearestXY,
    /// 只比 X。游標的垂直位置不影響結果，所以沿著曲線掃過去時
    /// 抓到的一定是「這個 X 上的那一點」，不會因為手抖跳到鄰近的點。
    /// 時間序列用這個比較順。
    NearestX
};

/// 命中測試的結果。
struct HitResult {
    /// 命中的資料點索引。< 0 代表沒命中。
    int index = -1;
    /// 該點的資料座標。
    Coordinates point;
    /// 游標到該點的像素距離。
    double distancePx = 0;
    /// 命中的圖元。
    class IPlottable* plottable = nullptr;

    bool isHit() const { return index >= 0; }
};

/// 圖例裡的一列。
struct LegendItem {
    QString text;
    LineStyle line;
    MarkerStyle marker;
    FillStyle fill;

    /// 這一列是哪個圖元的。由 Plot 在收集時填入，圖元自己不必管。
    /// 點圖例切換顯示要靠它找回對應的圖元。
    class IPlottable* source = nullptr;
    /// 該圖元目前是隱藏的（這一列要畫成灰的）。
    bool dimmed = false;
};

/// 一次 render 傳給圖元的全部上下文。
struct RenderPack {
    QPainter* painter = nullptr;
    PixelRect figureRect;  ///< 整張圖的像素範圍
    PixelRect dataRect;    ///< 繪圖區（扣掉軸與標題之後）
    Plot* plot = nullptr;
    double scaleFactor = 1.0;  ///< 高 DPI 或輸出大圖時的線寬／字級倍率
};

/// 所有圖元的共同介面。
class IPlottable {
public:
    virtual ~IPlottable() = default;

    bool isVisible = true;
    QString legendText;  ///< 空字串代表不進圖例

    /// 綁定的軸。nullptr 時由 Plot 在加入時填入預設的 bottom / left。
    Axis* xAxis = nullptr;
    Axis* yAxis = nullptr;

    /// 在版面計算之前呼叫，每次 render 一次。
    ///
    /// 需要「依資料調整自己綁的軸」的圖元（即時資料）在這裡做。放在 render() 裡
    /// 做會慢一拍：版面與刻度是用上一幀的範圍算的，資料與刻度會對不上。
    /// 一般圖元不需要覆寫。
    virtual void beforeLayout() {}

    /// 資料的版本號，資料一變就加一。0 代表這種圖元不追蹤（一般圖元都是）。
    ///
    /// 給 PlotWidget::setAutoRefresh() 用：即時資料從背景執行緒餵進來時，
    /// 背景執行緒不能碰 widget（QWidget 只能在 GUI 執行緒用），
    /// 改由 GUI 執行緒定時看版本號有沒有變，變了才重畫。
    virtual quint64 dataVersion() const { return 0; }

    /// 這個圖元佔用的資料範圍。沒有資料就回傳預設建構的 AxisLimits（hasData() == false）。
    virtual AxisLimits axisLimits() const = 0;

    /// 畫自己。painter 已經被裁切到 dataRect，座標換算請用 transform()。
    virtual void render(RenderPack& rp) = 0;

    /// 要不要裁切到繪圖區。畫在繪圖區「外面」的東西（色階圖例）要回傳 false，
    /// 否則整個會被裁掉。
    virtual bool clipToDataArea() const { return true; }

    /// 這個圖元要在繪圖區的某一邊外側佔多少像素。版面會替它留出來。
    /// 預設 0 代表不佔空間。
    virtual double outsideMargin(Edge edge) const {
        Q_UNUSED(edge);
        return 0;
    }

    /// 版面計算時告訴這個圖元：同一邊在它之前已經被佔掉多少像素（座標軸、
    /// 其他色階圖例）。畫在外側的圖元要把自己往外推這麼多，否則會疊在一起。
    virtual void setOutsideOffset(Edge edge, double px) {
        Q_UNUSED(edge);
        Q_UNUSED(px);
    }

    /// 圖例項目。預設回傳一列（legendText 為空則不回傳）。
    virtual QVector<LegendItem> legendItems() const { return {}; }

    /// 把自己身上每一個 FontStyle 交給 visit。
    ///
    /// Plot::visitFonts() 靠這個把整張圖的文字一次改掉（換字型、調字級、轉角度）。
    /// 沒有文字的圖元不必覆寫。
    ///
    /// 新增圖元時如果忘了實作，不會有任何編譯錯誤 —— 只是那個圖元的文字從此
    /// 不受整體設定影響，而且只有畫出來才看得見。tools/check_fonts.py 專門查這件事。
    virtual void visitFonts(const FontVisitor& visit) { Q_UNUSED(visit); }

    // ───────────── 互動 ─────────────

    /// 找出離 target 最近的資料點。
    ///
    /// 距離要在「像素」空間算，不是資料空間：兩軸的數量級常常差很多，
    /// 用資料距離的話，數值大的那一軸會完全主導，游標明明貼著某個點
    /// 卻命中另一個離很遠的。預設回傳沒命中（沒有離散資料點的圖元）。
    virtual HitResult hitTest(Coordinates target, const Transform& t,
                              HitMode mode = HitMode::NearestXY) const {
        Q_UNUSED(target);
        Q_UNUSED(t);
        Q_UNUSED(mode);
        return {};
    }

    /// 找出落在資料矩形內的點（框選用）。預設沒有可選的點。
    virtual QVector<int> pointsInRect(const CoordinateRect& rect) const {
        Q_UNUSED(rect);
        return {};
    }
    /// 取出第 index 筆資料點。pointsInRect 回傳的索引配這個用。
    virtual Coordinates pointAt(int index) const {
        Q_UNUSED(index);
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()};
    }

    /// 需不需要在滑鼠移動時收到通知（跟著游標跑的十字線要）。
    virtual bool wantsCursorUpdates() const { return false; }
    /// 滑鼠在繪圖區移動時由 PlotWidget 呼叫。
    /// 給的是 Plot 而不是現成的命中結果 —— 不同的圖元可能要用不同的比法。
    virtual void onCursorMoved(Coordinates at, class Plot& plot) {
        Q_UNUSED(at);
        Q_UNUSED(plot);
    }

    /// 這個圖元能不能拖。預設不行。
    virtual bool isDraggable() const { return false; }
    /// 把圖元平移一段資料距離。isDraggable() 為 true 時才會被呼叫。
    virtual void dragBy(Coordinates delta) { Q_UNUSED(delta); }
    /// 抓住之後、第一次 dragBy 之前呼叫一次。
    ///
    /// 可以只拖「一部分」的圖元（線段的端點、區間的邊、矩形的角、某一個資料點）
    /// 在這裡記下抓的是哪一部分，之後的 dragBy 就只動那一部分。
    virtual void beginDrag(Coordinates at, const Transform& t, double tolerancePx) {
        Q_UNUSED(at);
        Q_UNUSED(t);
        Q_UNUSED(tolerancePx);
    }
    /// 放開滑鼠時呼叫。
    virtual void endDrag() {}
    /// 游標在這個資料座標上時，算不算抓住了這個圖元。
    /// tolerancePx 是容許的像素距離。
    virtual bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
        Q_UNUSED(at);
        Q_UNUSED(t);
        Q_UNUSED(tolerancePx);
        return false;
    }

    /// 建立這個圖元該用的座標換算（依它綁的軸）。
    Transform transform(const RenderPack& rp) const {
        Transform t;
        t.rect = rp.dataRect;
        t.xr = xAxis ? xAxis->range : CoordinateRange(0, 1);
        t.yr = yAxis ? yAxis->range : CoordinateRange(0, 1);
        // 反向軸也要帶進來。漏掉的話刻度會翻、資料不翻，兩者對不起來。
        t.xInverted = xAxis && xAxis->inverted;
        t.yInverted = yAxis && yAxis->inverted;
        t.xBreaks = (xAxis && xAxis->hasBreaks()) ? &xAxis->breaks() : nullptr;
        t.yBreaks = (yAxis && yAxis->hasBreaks()) ? &yAxis->breaks() : nullptr;
        return t;
    }
};

}  // namespace qtplot

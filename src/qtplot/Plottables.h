// Plottables.h — 第一批圖元。
//
// 每一種圖都是一個小類別，狀態就是使用者設的樣式加上資料；
// 不快取像素座標，所以縮放、改軸、換主題都不需要通知圖元。
#pragma once

#include "Decimation.h"
#include "Plottable.h"

namespace qtplot {

// ───────────────────────── 散點與線 ─────────────────────────

/// 逐點資料標籤的設定。Markers 與 Scatter 共用。
///
/// 標籤只在點不多的時候有意義：幾十個點標得出來，幾千個點標出來只會糊成一片，
/// 所以預設是關的，而且超過 maxLabels 個點就自動不標。
struct PointLabels {
    /// 每個點的文字。空字串的那幾點不標。數量少於資料點數時，後面的點就沒有標籤。
    QStringList texts;
    FontStyle font;
    /// 相對於資料點的像素偏移。預設往右上，避開標記本身。
    double offsetX = 7;
    double offsetY = -7;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignBottom;
    /// 資料點超過這個數量就不標（標了也讀不到）。
    int maxLabels = 200;

    bool isVisible() const { return !texts.isEmpty() && font.color.alpha() > 0; }

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) {
        visit(font);
    }
};

/// 任意 (x, y) 點序列。線與標記都可開關，是最通用的一種圖。
class Scatter : public IPlottable {
public:
    QVector<double> xs;
    QVector<double> ys;
    LineStyle line{QColor(31, 119, 180), 1.5};
    MarkerStyle marker{MarkerShape::FilledCircle, 6, QColor(31, 119, 180)};
    /// 逐點的資料標籤。預設不畫。
    PointLabels pointLabels;
    /// 階梯線：Horizontal 先走水平再垂直，Vertical 反之。
    enum class StepStyle { None, Horizontal, Vertical };
    StepStyle step = StepStyle::None;

    /// 線段之間怎麼連。
    enum class Interpolation {
        Straight,        ///< 直線相連
        Smooth,          ///< Catmull–Rom 平滑曲線
        SmoothMonotone   ///< 單調平滑：平滑但保證不會超出原始資料的範圍
    };
    Interpolation interpolation = Interpolation::Straight;
    /// Smooth 的張力。0 最鬆（接近直線），1 最彎。超過 0.5 容易過衝。
    double smoothTension = 0.5;

    /// 只畫這個索引範圍（兩端都含）。負值代表不限制。
    /// 資料很長但只想看其中一段時，比另外複製一份子陣列省事也省記憶體。
    int renderFirst = -1;
    int renderLast = -1;
    /// 填到這個 Y 值（FillY 的簡化版）。NaN 代表不填。
    double fillToY = std::numeric_limits<double>::quiet_NaN();
    FillStyle fill;
    /// 跳過非有限值（NaN 會把線斷開，而不是畫到無限遠）。
    bool skipNonFinite = true;

    /// 畫的時候把每個點換算成 (x·xScale + xOffset, y·yScale + yOffset)。
    /// 原始資料不動 —— 換單位（秒→毫秒）或把幾條曲線錯開時不用另外複製陣列。
    double xScale = 1.0;
    double yScale = 1.0;
    double xOffset = 0.0;
    double yOffset = 0.0;
    /// fillToY 以上與以下各用不同的顏色（例如盈虧）。無效色代表用 fill。
    QColor fillAboveColor;
    QColor fillBelowColor;
    /// 先畫標記再畫線（線會壓在標記上）。
    bool markersBelowLine = false;

    /// 套用了 scale/offset 之後實際畫出來的座標。
    QVector<double> displayXs() const;
    QVector<double> displayYs() const;

    // ── 拖曳個別的點 ──
    /// 可以用滑鼠拖動個別的資料點（拖的是原始資料，放開後 xs/ys 就是新值）。
    bool draggablePoints = false;
    /// 拖點時允許動哪個方向。只想調整數值、不能改時間時把 dragX 關掉。
    bool dragX = true;
    bool dragY = true;
    /// 目前（或最後一次）被拖的點。沒有就是 -1。
    int draggedIndex() const { return m_dragIndex; }

    bool isDraggable() const override { return draggablePoints; }
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;
    void beginDrag(Coordinates at, const Transform& t, double tolerancePx) override;
    void dragBy(Coordinates delta) override;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    HitResult hitTest(Coordinates target, const Transform& t,
                      HitMode mode = HitMode::NearestXY) const override;
    QVector<int> pointsInRect(const CoordinateRect& rect) const override;
    Coordinates pointAt(int index) const override;

private:
    int m_dragIndex = -1;
};

/// 只畫標記，不連線。
class Markers : public IPlottable {
public:
    QVector<double> xs;
    QVector<double> ys;
    MarkerStyle marker{MarkerShape::FilledCircle, 7, QColor(31, 119, 180)};
    /// 逐點的資料標籤。預設不畫。
    PointLabels pointLabels;

    // ── 拖曳個別的點 ──
    /// 可以用滑鼠拖動個別的資料點（拖的是原始資料，放開後 xs/ys 就是新值）。
    bool draggablePoints = false;
    /// 拖點時允許動哪個方向。只想調整數值、不能改時間時把 dragX 關掉。
    bool dragX = true;
    bool dragY = true;
    /// 目前（或最後一次）被拖的點。沒有就是 -1。
    int draggedIndex() const { return m_dragIndex; }

    bool isDraggable() const override { return draggablePoints; }
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;
    void beginDrag(Coordinates at, const Transform& t, double tolerancePx) override;
    void dragBy(Coordinates delta) override;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    HitResult hitTest(Coordinates target, const Transform& t,
                      HitMode mode = HitMode::NearestXY) const override;
    QVector<int> pointsInRect(const CoordinateRect& rect) const override;
    Coordinates pointAt(int index) const override;

private:
    int m_dragIndex = -1;
};

/// 等間距取樣的訊號。X 由 index * period + offset 算出，不存 X 陣列。
/// 點數遠多於像素時會做 min/max 抽樣，百萬點也能即時重畫。
class Signal : public IPlottable {
public:
    QVector<double> ys;
    double period = 1.0;
    double xOffset = 0.0;
    double yOffset = 0.0;
    /// 數值的倍率：畫的是 y·yScale + yOffset。換單位（V→mV）時不用複製資料。
    double yScale = 1.0;
    /// 轉成垂直方向：索引往上長、數值往右。
    /// 井深剖面、層序地層、頻譜瀑布圖這類資料本來就該直著看。
    bool vertical = false;
    /// 只畫這個索引範圍（兩端都含）。負值代表不限制。
    int renderFirst = -1;
    int renderLast = -1;
    LineStyle line{QColor(31, 119, 180), 1.5};
    MarkerStyle marker;  ///< 預設不畫；放大到每點間距足夠時才自動顯示
    /// 每像素點數超過這個值就改用 min/max 抽樣。
    double decimationThreshold = 2.0;
    /// 點間距大於這個像素數時自動顯示標記（0 代表不自動）。
    double autoMarkerSpacingPx = 8.0;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    HitResult hitTest(Coordinates target, const Transform& t,
                      HitMode mode = HitMode::NearestXY) const override;
    QVector<int> pointsInRect(const CoordinateRect& rect) const override;
    Coordinates pointAt(int index) const override;

private:
    /// 最小／最大值索引與它對應的那份資料（和 ys 共用記憶體，見 Plottables.cpp 的 validIndex）。
    const MinMaxIndex* minMaxIndex() const;
    mutable QVector<double> m_indexed;
    mutable MinMaxIndex m_index;
};

/// X 已排序的 (x, y) 序列。會做可視範圍裁切與抽樣，適合長時間序列。
class SignalXY : public IPlottable {
public:
    QVector<double> xs;  ///< 必須遞增；沒排序的資料請用 Scatter
    QVector<double> ys;
    /// 畫的是 (x + xOffset, y·yScale + yOffset)。
    double xOffset = 0.0;
    double yOffset = 0.0;
    double yScale = 1.0;
    /// 轉成垂直方向：xs 對到 Y 軸、ys 對到 X 軸。
    bool vertical = false;
    /// 只畫這個索引範圍（兩端都含）。負值代表不限制。
    int renderFirst = -1;
    int renderLast = -1;
    LineStyle line{QColor(31, 119, 180), 1.5};
    MarkerStyle marker;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    HitResult hitTest(Coordinates target, const Transform& t,
                      HitMode mode = HitMode::NearestXY) const override;
    QVector<int> pointsInRect(const CoordinateRect& rect) const override;
    Coordinates pointAt(int index) const override;

private:
    /// 最小／最大值索引與它對應的那份資料（和 ys 共用記憶體，見 Plottables.cpp 的 validIndex）。
    const MinMaxIndex* minMaxIndex() const;
    mutable QVector<double> m_indexed;
    mutable MinMaxIndex m_index;
};

/// y = f(x)，每個像素行求值一次，所以縮放後解析度自動跟上。
class FunctionPlot : public IPlottable {
public:
    std::function<double(double)> function;
    LineStyle line{QColor(214, 39, 40), 1.5};
    /// 求值範圍。預設跟著 X 軸可視範圍走。
    CoordinateRange xLimits;
    /// 每像素求值幾次（>1 可讓尖銳的函數平滑一些）。
    double samplesPerPixel = 1.0;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
};

/// 兩條曲線之間的填色帶（信賴區間、最大最小值範圍）。
class FillY : public IPlottable {
public:
    QVector<double> xs;
    QVector<double> ys1;
    QVector<double> ys2;
    FillStyle fill{QColor(31, 119, 180, 80)};
    LineStyle line{QColor(31, 119, 180, 160), 1.0};

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
};

// ───────────────────────── 長條與統計 ─────────────────────────

/// 一根長條。
struct Bar {
    double position = 0;      ///< 沿著「類別軸」的位置
    double value = 0;         ///< 長條頂端
    double valueBase = 0;     ///< 長條底端（堆疊圖用）
    double error = 0;         ///< 誤差線的半長，0 代表不畫
    double size = 0.8;        ///< 寬度（類別軸的資料單位）
    QColor fillColor = QColor(31, 119, 180);
    QColor borderColor = QColor(0, 0, 0, 0);
    QString label;            ///< 長條的標籤文字

    /// 標籤放在哪裡。
    enum class LabelPlacement {
        Outside,      ///< 長條外側（正值在頂端之外，負值在底端之外）
        Center,       ///< 長條正中央（堆疊長條的各段百分比用這個）
        InsideStart   ///< 長條內、靠起點那一端（排名長條把數值寫在條內用這個）
    };
    LabelPlacement labelPlacement = LabelPlacement::Outside;
    /// 標籤顏色。無效時用 valueLabelFont 的顏色（深色長條內的字要改成白色）。
    QColor labelColor;
    /// 完整的填色（花紋、漸層）。有設就取代 fillColor。
    FillStyle fill;
    /// 這根長條的框線粗細。<= 0 代表用 BarPlot::borderLine 的。
    double borderWidth = 0;
};

/// 長條圖。horizontal = true 時長條朝右。
class BarPlot : public IPlottable {
public:
    QVector<Bar> bars;
    bool horizontal = false;
    LineStyle borderLine{QColor(0, 0, 0, 0), 1.0};
    FontStyle valueLabelFont;
    /// 自動把資料範圍含進 0（長條圖從 0 起算才不會誤讀）。
    bool includeZero = true;
    /// 先畫完全部長條，再畫全部標籤。長條互相重疊（浮動長條、很寬的長條）時，
    /// 標籤才不會被後面的長條蓋住。
    bool labelsOnTop = false;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 由數值序列建一組等距長條（位置 0, 1, 2, ...）。
    static QVector<Bar> fromValues(const QVector<double>& values, QColor color);

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(valueLabelFont);
    }
};

/// 誤差線（可單獨用，也可疊在 Scatter 上）。
class ErrorBar : public IPlottable {
public:
    QVector<double> xs;
    QVector<double> ys;
    QVector<double> yErrorsNegative;
    QVector<double> yErrorsPositive;
    QVector<double> xErrorsNegative;
    QVector<double> xErrorsPositive;
    LineStyle line{QColor(0, 0, 0), 1.0};
    double capSize = 3;  ///< 端帽的半長（像素）

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};

// ───────────────────────── 熱圖 ─────────────────────────

/// 2D 陣列熱圖。資料以 row-major 存放，第 0 列預設畫在上方。
class Heatmap : public IPlottable {
public:
    QVector<double> values;  ///< 大小必須是 rows * cols
    int rows = 0;
    int cols = 0;
    Colormap colormap{Colormap::Viridis};
    /// 色階範圍。未設定（invalid）時用資料的 min/max。
    CoordinateRange colorRange;
    /// 熱圖佔的資料範圍。未設定時用 (0, cols, 0, rows)。
    CoordinateRect extent{0, 0, 0, 0};
    bool smooth = false;       ///< true 用雙線性插值放大
    /// 上下翻轉（第 0 列畫到下面）。
    bool flipVertically = false;
    /// 左右翻轉（第 0 行畫到右邊）。
    bool flipHorizontally = false;
    /// NaN 的像素畫成透明（false 則畫成色階最低色）。
    bool transparentNaN = true;

    /// 整張熱圖的不透明度（0–1）。疊在影像或另一張圖上時調低。
    double opacity = 1.0;
    /// 每一格各自的不透明度（0–1），大小必須是 rows * cols；空的話不用。
    ///
    /// 用來把「可信度」疊進顏色裡：樣本少的格子調淡，讀者自然不會太相信它。
    QVector<double> alphaMap;
    /// extent 指的是「最外圈格子的中心」而不是熱圖的外緣。
    ///
    /// 資料是在格點上取樣時（第 i 格就是 x = i 的量測值），格子的中心應該對準刻度；
    /// 不開的話整張圖會偏半格，刻度對到的是格子的邊。
    bool cellCentered = false;

    double value(int row, int col) const {
        return (row >= 0 && row < rows && col >= 0 && col < cols)
                   ? values.at(static_cast<qsizetype>(row) * cols + col)
                   : std::numeric_limits<double>::quiet_NaN();
    }
    CoordinateRect effectiveExtent() const;
    CoordinateRange dataRange() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};

// ───────────────────── 參考線與區間 ─────────────────────

/// 橫跨整張圖的水平或垂直參考線。
class AxisLine : public IPlottable {
public:
    bool vertical = false;  ///< true 為垂直線（固定 X）
    double position = 0;
    LineStyle line{QColor(0, 0, 0), 1.5};
    Label text;             ///< 印在線端的標籤
    /// 參與自動範圍計算（false 時不會把視野拉到這條線）。
    bool includeInAxisLimits = true;

    /// 線只畫這一段（另一軸的資料座標）。無效代表橫跨整張圖。
    CoordinateRange extent;

    /// 標籤放在哪裡。End 是垂直線的上端、水平線的右端。
    enum class TextPosition {
        InsideEnd,       ///< 繪圖區內、線的尾端（預設）
        InsideStart,     ///< 繪圖區內、線的起點
        InsideCenter,    ///< 繪圖區內、線的中段
        OnAxis,          ///< 壓在座標軸上的色塊（像游標讀數），垂直線在下方 X 軸、水平線在左側 Y 軸
        OnOppositeAxis   ///< 同上，但放在對面那根軸（上方／右側）
    };
    TextPosition textPosition = TextPosition::InsideEnd;
    /// OnAxis 色塊的底色。不可見時用線的顏色。
    FillStyle textBackground;
    /// 標籤和線的距離（像素）。
    double textOffset = 3;

    /// 可以用滑鼠拖動這條線。
    bool draggable = false;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    /// 壓在座標軸上的標籤在繪圖區外面，不能被裁掉。
    bool clipToDataArea() const override {
        return textPosition != TextPosition::OnAxis && textPosition != TextPosition::OnOppositeAxis;
    }
    bool isDraggable() const override { return draggable; }
    void dragBy(Coordinates delta) override { position += vertical ? delta.x : delta.y; }
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(text.font);
    }
};

/// 橫跨整張圖的帶狀區間。
class AxisSpan : public IPlottable {
public:
    bool vertical = false;  ///< true 為垂直帶（限制 X 範圍）
    double start = 0;
    double end = 1;
    FillStyle fill{QColor(255, 0, 0, 50)};
    LineStyle line{QColor(0, 0, 0, 0), 1.0};
    bool includeInAxisLimits = true;
    /// 可以用滑鼠拖動整個區間。
    bool draggable = false;
    /// 可以抓住兩側的邊改變區間的寬度。
    bool resizable = false;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    bool isDraggable() const override { return draggable || resizable; }
    void beginDrag(Coordinates at, const Transform& t, double tolerancePx) override;
    void dragBy(Coordinates delta) override;
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;

private:
    /// 0 整段、1 start 那一邊、2 end 那一邊、-1 不動
    int m_dragPart = 0;
};

/// 十字線。可以固定在某個座標，也可以跟著游標跑並吸附到最近的資料點。
class Crosshair : public IPlottable {
public:
    Coordinates position;
    LineStyle lineStyle{QColor(255, 0, 0, 180), 1.0, LinePattern::Dashed};
    bool showHorizontal = true;
    bool showVertical = true;
    bool showLabels = true;
    FontStyle labelFont;
    QColor labelBackground = QColor(255, 0, 0);

    // ── 跟著游標 ──
    /// 跟著滑鼠移動。需要 PlotWidget（匯出靜態圖時沒有游標，位置維持不變）。
    bool followCursor = false;
    /// 吸附到最近的資料點，而不是停在游標的實際位置。
    /// 讀數值時這個很重要 —— 游標停在兩點之間讀到的是內插出來的假值。
    bool snapToData = false;
    HitMode snapMode = HitMode::NearestXY;
    /// 吸附的最大距離（像素）。超過就不吸附，回到跟著游標。0 代表不限。
    double snapTolerancePx = 0;
    /// 在吸附到的點上畫一個標記。
    MarkerStyle snapMarker{MarkerShape::OpenCircle, 13, QColor(214, 39, 40)};

    // ── 讀數 ──
    /// 在角落顯示 Index / X / Y。
    bool showReadout = false;
    Qt::Alignment readoutCorner = Qt::AlignLeft | Qt::AlignTop;
    FontStyle readoutFont;
    FillStyle readoutBackground{QColor(255, 255, 255, 230)};
    LineStyle readoutBorder{QColor(0, 0, 0, 90), 1.0};

    /// 目前吸附到的資料點索引。沒吸附到就是 -1。
    int snappedIndex() const { return m_snappedIndex; }
    /// 目前吸附到的圖元。
    IPlottable* snappedPlottable() const { return m_snappedPlottable; }

    bool wantsCursorUpdates() const override { return followCursor; }
    void onCursorMoved(Coordinates at, class Plot& plot) override;

    AxisLimits axisLimits() const override { return AxisLimits(); }
    void render(RenderPack& rp) override;

private:
    int m_snappedIndex = -1;
    IPlottable* m_snappedPlottable = nullptr;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(readoutFont);
    }
};

// ───────────────────── 形狀與標註 ─────────────────────

/// 資料座標上的一段文字。
class Text : public IPlottable {
public:
    Coordinates position;
    Label label;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignBottom;
    FillStyle background;
    LineStyle border{QColor(0, 0, 0, 0), 1.0};
    double padding = 3;
    /// 像素偏移，讓文字不要壓在資料點上。
    double offsetX = 0;
    double offsetY = 0;
    /// 可以用滑鼠拖動。
    bool draggable = false;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    bool isDraggable() const override { return draggable; }
    void dragBy(Coordinates delta) override {
        position.x += delta.x;
        position.y += delta.y;
    }
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(label.font);
    }
};

/// 固定在繪圖區角落的文字（不隨縮放移動）。
class Annotation : public IPlottable {
public:
    Label label;
    Qt::Alignment corner = Qt::AlignLeft | Qt::AlignTop;
    double marginX = 8;
    double marginY = 8;
    FillStyle background{QColor(255, 255, 255, 220)};
    LineStyle border{QColor(0, 0, 0, 60), 1.0};
    double padding = 5;

    AxisLimits axisLimits() const override { return AxisLimits(); }
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(label.font);
    }
};

/// 箭頭的頭長什麼樣子。
enum class ArrowHead {
    Triangle,  ///< 三角形（filledHead 決定實心或空心）
    Open,      ///< 只有兩撇的 V 形
    Barbed,    ///< 尾端內凹的倒鉤
    Diamond,   ///< 菱形
    Circle,    ///< 圓點
    None       ///< 不畫頭，只剩線
};

/// 箭頭。
class Arrow : public IPlottable {
public:
    Coordinates tip;
    Coordinates base;
    LineStyle line{QColor(0, 0, 0), 2.0};
    double headLength = 12;  ///< 像素
    double headWidth = 8;    ///< 像素
    bool filledHead = true;
    ArrowHead headShape = ArrowHead::Triangle;
    /// 兩端都畫頭（雙向箭頭，標示距離或區間用）。
    bool doubleEnded = false;
    /// 可以用滑鼠拖動整支箭頭。
    bool draggable = false;
    /// 可以分別拖動兩個端點。headShape 設成 None 就是一條可以拉的線段（量距離、畫趨勢線）。
    bool draggableEndpoints = false;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    bool isDraggable() const override { return draggable || draggableEndpoints; }
    void beginDrag(Coordinates at, const Transform& t, double tolerancePx) override;
    void dragBy(Coordinates delta) override;
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;

private:
    /// 0 整支、1 尖端、2 尾端、-1 不動
    int m_dragPart = 0;
};

/// 矩形、橢圓、多邊形共用一個類別，用 shape 切換。
class Shape : public IPlottable {
public:
    enum class Kind { Rectangle, Ellipse, Polygon };
    Kind kind = Kind::Rectangle;
    /// Rectangle / Ellipse 用 rect；Polygon 用 points。
    CoordinateRect rect;
    QVector<Coordinates> points;
    FillStyle fill{QColor(31, 119, 180, 80)};
    LineStyle line{QColor(31, 119, 180), 1.5};
    /// 可以用滑鼠拖動整個形狀。
    bool draggable = false;
    /// 矩形與橢圓可以抓住邊或角改變大小（旋轉過的不行）。
    bool resizable = false;
    /// 繞自己的中心旋轉幾度（逆時針）。
    ///
    /// 旋轉是在「資料座標」上做的，所以兩軸比例不同時旋轉過的圓會變成斜橢圓 ——
    /// 這是正確的（資料空間的旋轉本來就該這樣），要看起來像旋轉請配
    /// plot.equalAxisScale = true。
    double rotationDegrees = 0;

    /// 形狀的外框頂點（已套用旋轉）。橢圓會取樣成多邊形。
    QVector<Coordinates> outline(int ellipseSegments = 180) const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    bool isDraggable() const override { return draggable || resizable; }
    void beginDrag(Coordinates at, const Transform& t, double tolerancePx) override;
    void dragBy(Coordinates delta) override;
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;

private:
    /// 抓到哪幾個邊（1 左、2 右、4 下、8 上）。0 代表整個平移，-1 代表不動。
    int m_dragEdges = 0;
};

/// 氣泡圖的一個泡泡。
struct Bubble {
    double x = 0;
    double y = 0;
    /// 泡泡的大小。怎麼換算成半徑由 BubbleChart::areaProportional 決定。
    double size = 1;
    QString label;     ///< 印在泡泡上的文字
    QColor fillColor;  ///< 無效色代表由調色盤依序指定
};

/// 氣泡圖：散點圖再加上第三個維度（泡泡大小）。
///
/// 預設讓「面積」正比於 size，不是直徑 —— 直徑正比的話，size 差兩倍的兩個泡泡
/// 面積會差四倍，讀者會把差距讀成平方。這是氣泡圖最常見的錯誤。
class BubbleChart : public IPlottable {
public:
    QVector<Bubble> bubbles;

    /// size 最大的那個泡泡畫多大（直徑，像素）。
    double maxDiameterPx = 56;
    /// size 最小的那個泡泡至少要多大（直徑，像素）。太小會看不見。
    double minDiameterPx = 10;
    /// true 時面積正比於 size（正確的讀法）；false 時直徑正比。
    bool areaProportional = true;

    double fillOpacity = 0.65;
    LineStyle border{QColor(0, 0, 0, 120), 1.0};
    FontStyle labelFont;
    /// 泡泡小於這個直徑（像素）就不印標籤。
    double minLabelDiameterPx = 26;

    // ── 大小圖例 ──
    /// 畫一組同心圓說明泡泡大小代表什麼。
    ///
    /// 氣泡圖沒有這個就沒辦法讀出絕對值 —— 讀者只知道「這顆比那顆大」，
    /// 說不出大多少。一般的圖例做不到：它只有一個固定大小的色塊。
    bool showSizeLegend = false;
    /// 圖例要列出哪幾個 size。留空則自動取最大、中間、最小三個。
    QVector<double> sizeLegendValues;
    /// 圖例畫在繪圖區的哪個角落。
    Qt::Alignment sizeLegendCorner = Qt::AlignRight | Qt::AlignTop;
    double sizeLegendMarginPx = 14;
    QString sizeLegendTitle;
    FontStyle sizeLegendFont;
    LineStyle sizeLegendLine{QColor(0, 0, 0, 150), 1.0, LinePattern::Dotted};

    /// size 換算成直徑（像素）。圖例和泡泡本身共用同一條換算，才不會對不上。
    double diameterFor(double size) const;
    /// 資料裡 size 的範圍。
    CoordinateRange sizeRange() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    // 預設引數不會從基礎類別繼承，覆寫時要自己再寫一次
    HitResult hitTest(Coordinates target, const Transform& t,
                      HitMode mode = HitMode::NearestXY) const override;
    Coordinates pointAt(int index) const override;

    /// 由三組數列建立泡泡，顏色依序取自調色盤。
    static QVector<Bubble> fromArrays(const QVector<double>& xs, const QVector<double>& ys,
                                      const QVector<double>& sizes, const Palette& palette);

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(sizeLegendFont);
    }
};

/// 扇形：橢圓內部，由一段圓周弧與圓心（或一段內弧）圍出來的區域。
///
/// 一個類別涵蓋四種常見形狀，差別只在內半徑給不給、兩個方向的半徑等不等：
///
///   | 形狀         | radiusX/Y | innerRadiusX/Y |
///   | 圓扇形       | 相等      | 0              |
///   | 橢圓扇形     | 不等      | 0              |
///   | 環狀扇形     | 相等      | > 0            |
///   | 環狀橢圓扇形 | 不等      | > 0            |
///
/// sweepAngle 給 360 就是完整的橢圓或圓環。
class Sector : public IPlottable {
public:
    Coordinates center{0, 0};
    /// 外半徑。兩者不等就是橢圓。
    double radiusX = 1.0;
    double radiusY = 1.0;
    /// 內半徑。> 0 時中間會空出一個洞（環狀扇形）。
    double innerRadiusX = 0.0;
    double innerRadiusY = 0.0;

    /// 起始角（度）。0 指向 +X，逆時針為正。
    double startAngle = 0;
    /// 掃過的角度（度）。負值代表順時針。
    double sweepAngle = 90;

    FillStyle fill{QColor(31, 119, 180, 80)};
    LineStyle line{QColor(0, 0, 0), 1.5};
    QString legendText;

    /// 沿著扇形的中線弧排的文字（逐字彎曲）。空字串代表不畫。
    ///
    /// 環狀圖外圈的「分類帶」就是這個用法。文字長的時候一定要逐字排 ——
    /// 整段轉同一個角度的話，兩端會離開弧線翹起來。
    Label arcLabel;
    /// 文字落在內外半徑之間的哪裡。0 貼內緣、1 貼外緣、0.5 在正中間。
    double arcLabelRadiusFraction = 0.5;

    /// 把兩個方向的外半徑一起設成同一個值（正圓）。
    void setRadius(double r) { radiusX = radiusY = r; }
    void setInnerRadius(double r) { innerRadiusX = innerRadiusY = r; }

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(arcLabel.font);
    }
};

/// 圓弧／橢圓弧：沿著橢圓邊緣的一段開放曲線，不填色也不連回圓心。
///
/// 和 sweepAngle = 360 的 Sector 外框的差別是它可以只畫一部分，
/// 而且兩端是開放的 —— 封起來會多出一條弦。
class Arc : public IPlottable {
public:
    Coordinates center{0, 0};
    double radiusX = 1.0;
    double radiusY = 1.0;
    /// 起始角（度）。0 指向 +X，逆時針為正 —— 和 Sector 一致。
    double startAngle = 0;
    double sweepAngle = 180;

    LineStyle line{QColor(0, 0, 0), 2.0};
    QString legendText;

    void setRadius(double r) { radiusX = radiusY = r; }

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
};

// ───────────────────── 地平線圖 ─────────────────────

/// 地平線圖：把一條時間序列「折疊」成幾層，用顏色深淺代替高度。
///
/// 用途是在很矮的高度裡塞進一條序列，好幾條疊起來比較形狀。
/// 原理是把值域切成 bands 層，每層都畫在同一個高度裡，愈高的層顏色愈深；
/// 負值鏡射回來、換一組顏色。
///
/// 代價要知道：讀者沒辦法直接讀出數值，只能比較「深淺」與「形狀」。
/// 要讀數值請用一般的折線圖。層數超過 3～4 層之後，顏色也分不出來了。
class Horizon : public IPlottable {
public:
    QVector<double> xs;
    QVector<double> ys;

    /// 畫在這個資料矩形裡。高度就是「一層」的高度。
    CoordinateRect extent{0, 1, 0, 1};
    /// 切成幾層。
    int bands = 3;
    /// 一層代表多少數值。<= 0 代表自動取 max|y − baseline| / bands。
    double bandValue = 0;
    /// 基準線。
    double baseline = 0;

    /// 正值的顏色，由淺到深。不足的話用最後一個。
    QVector<QColor> positiveColors{QColor(200, 225, 240), QColor(110, 175, 215),
                                   QColor(30, 110, 170)};
    /// 負值的顏色，由淺到深。
    QVector<QColor> negativeColors{QColor(250, 215, 200), QColor(240, 145, 110),
                                   QColor(200, 60, 40)};

    /// 實際用的一層高度（已套用自動計算）。
    double resolvedBandValue() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};

// ───────────────────── 堆疊輔助 ─────────────────────

/// 一條堆疊帶的上下界。兩個陣列長度相同，和原始的 x 一一對應。
struct StackedBand {
    QVector<double> lower;
    QVector<double> upper;
};

/// 把多組數列堆疊成帶狀區間：正值往上疊、負值往下疊，兩邊各自累積。
///
/// 為什麼不能直接用「累積和」：資料裡有負值的時候，累積和會讓那一層把
/// 它上面所有層一起往下拉，層與層之間還會互相穿越 —— 畫出來的面積
/// 不再是各層的值，整張圖就讀錯了。正負分開累積才對：
/// 每個 x 各自維護一個「正的累積」和一個「負的累積」，
/// 正值從正累積往上長，負值從負累積往下長。
///
/// 回傳的順序和輸入相同。長度不一致時以最短的為準。
QVector<StackedBand> stackSigned(const QVector<QVector<double>>& series);

/// 堆疊的基準線怎麼放。
enum class StackOffset {
    Zero,      ///< 基準線固定在 0（一般的堆疊面積圖）
    Centered,  ///< 每一欄都對稱於中線（ThemeRiver）
    Wiggle,    ///< 讓各層的「斜率變動」最小（Byron–Wattenberg 的河流圖）
    Expand     ///< 每一欄正規化成 1（百分比堆疊）
};

/// 河流圖（streamgraph）用的堆疊：和 stackSigned 的差別只在基準線。
///
/// 全部的值都當成非負（負值視為 0）—— 河流圖的前提是「總量」有意義，
/// 有正有負的資料請用 stackSigned。
///
/// Wiggle 是 Byron–Wattenberg 那篇提的做法：基準線的每一步位移取
///   −(1/(n+1)) · Σ_j (n−j)·Δf_j
/// 讓厚的層盡量少上下擺動。直接用 0 當基準線的話，最下面那層的起伏會把
/// 上面所有層一起推上推下，讀者會把那個起伏誤讀成上層自己的變化。
QVector<StackedBand> stackStream(const QVector<QVector<double>>& series,
                                 StackOffset offset = StackOffset::Wiggle);

// ───────────────────── 直方圖輔助 ─────────────────────

/// 把資料分箱。回傳的 bin 中心與計數可直接餵給 BarPlot。
struct Histogram {
    QVector<double> binCenters;
    QVector<double> binEdges;
    QVector<double> counts;
    double binWidth = 1;

    /// binCount <= 0 時用 Sturges 公式自動決定箱數。
    static Histogram fromValues(const QVector<double>& values, int binCount = 0);
    /// 範圍固定在 [min, max]、切成 binCount 箱。
    ///
    /// 要比較兩組資料的分布就一定要用這個 —— 各自自動分箱的話箱子對不齊，
    /// 疊起來比的是兩把不同的尺。範圍外的值 includeOutliers 為 true 時併進頭尾兩箱，否則丟掉。
    static Histogram fromRange(const QVector<double>& values, double min, double max,
                               int binCount, bool includeOutliers = false);
    /// 固定箱寬：從 min 開始每 binWidth 一箱，直到蓋住 max。
    static Histogram fromBinWidth(const QVector<double>& values, double min, double max,
                                  double binWidth, bool includeOutliers = false);
    /// 轉成機率密度（總面積為 1）。
    QVector<double> densities() const;
    /// 每一箱佔總數的比例（總和為 1）。
    QVector<double> probabilities() const;
    /// 累積次數（最後一箱等於總數）。
    QVector<double> cumulativeCounts() const;
    QVector<Bar> toBars(QColor color) const;
};

}  // namespace qtplot

// Core.h — 座標型別、樣式、調色盤。
//
// 幾何約定（與 ScottPlot 相同，不要改）：
//   * 資料座標 Coordinates：y 向上為正，就是使用者給的數值。
//   * 像素座標 Pixel：y 向下為正，Qt 的慣例，原點在左上。
//   * PixelRect 的 top < bottom（因為 y 向下），CoordinateRect 的 bottom < top。
// 兩者的轉換一律走 Transform，不要在圖元裡自己算。
#pragma once

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace qtplot {

/// π。標頭裡不要用 M_PI —— 那個巨集需要 _USE_MATH_DEFINES，
/// 外部引用這個標頭的專案不一定有設。
constexpr double kPi = 3.14159265358979323846;

// ───────────────────────────── 幾何 ─────────────────────────────

/// 資料座標上的一點。
struct Coordinates {
    double x = 0;
    double y = 0;
    Coordinates() = default;
    Coordinates(double x_, double y_) : x(x_), y(y_) {}
    bool isFinite() const { return std::isfinite(x) && std::isfinite(y); }
};

/// 極座標轉直角。angleDeg 以 +X 軸為 0°，逆時針為正。
inline Coordinates fromPolar(double radius, double angleDeg) {
    const double rad = angleDeg * kPi / 180.0;
    return {radius * std::cos(rad), radius * std::sin(rad)};
}

/// 單一軸上的區間。預設是「無效」（min > max），方便用 expand() 逐點累積。
struct CoordinateRange {
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();

    CoordinateRange() = default;
    CoordinateRange(double lo, double hi) : min(lo), max(hi) {}

    /// 逐位元相等。用在「視野有沒有變」這種比較 ——
    /// 那裡要的就是「一模一樣」，不是「差不多」，所以不設容差。
    bool operator==(const CoordinateRange& o) const { return min == o.min && max == o.max; }
    bool operator!=(const CoordinateRange& o) const { return !(*this == o); }

    bool isValid() const { return min <= max && std::isfinite(min) && std::isfinite(max); }
    bool hasArea() const { return isValid() && max > min; }
    double span() const { return max - min; }
    double center() const { return (min + max) / 2.0; }

    void expand(double v) {
        if (!std::isfinite(v)) return;
        min = std::min(min, v);
        max = std::max(max, v);
    }
    void expand(const CoordinateRange& o) {
        if (!o.isValid()) return;
        expand(o.min);
        expand(o.max);
    }
    /// 兩端各外推 span 的 fraction 倍。span 為 0 時改用絕對量，避免原地不動。
    void pad(double fraction) {
        if (!isValid()) return;
        double s = span();
        double amount = (s > 0) ? s * fraction : (std::abs(max) > 0 ? std::abs(max) * fraction : 1.0);
        min -= amount;
        max += amount;
    }
    /// 保證有寬度：單點資料（span == 0）會讓 Transform 除以零。
    void ensureArea(double minSpan = 1.0) {
        if (!isValid()) {
            min = -minSpan / 2;
            max = minSpan / 2;
            return;
        }
        if (span() <= 0) {
            double c = center();
            min = c - minSpan / 2;
            max = c + minSpan / 2;
        }
    }
    void translate(double delta) { min += delta; max += delta; }
    /// 以 about 為定點縮放。frac > 1 是放大視野（zoom out）。
    void zoom(double frac, double about) {
        min = about + (min - about) * frac;
        max = about + (max - about) * frac;
    }
};

/// 資料空間的矩形。注意 bottom < top。
struct CoordinateRect {
    double left = 0, right = 0, bottom = 0, top = 0;
    CoordinateRect() = default;
    CoordinateRect(double l, double r, double b, double t)
        : left(l), right(r), bottom(b), top(t) {}
    double width() const { return right - left; }
    double height() const { return top - bottom; }
    CoordinateRange xRange() const { return {left, right}; }
    CoordinateRange yRange() const { return {bottom, top}; }
};

/// 像素空間的矩形。注意 top < bottom（Qt 的 y 向下）。
struct PixelRect {
    double left = 0, right = 0, top = 0, bottom = 0;
    PixelRect() = default;
    PixelRect(double l, double r, double t, double b)
        : left(l), right(r), top(t), bottom(b) {}
    static PixelRect fromQRectF(const QRectF& r) {
        return {r.left(), r.right(), r.top(), r.bottom()};
    }
    double width() const { return right - left; }
    double height() const { return bottom - top; }
    bool isEmpty() const { return width() <= 0 || height() <= 0; }
    QRectF toQRectF() const { return QRectF(left, top, width(), height()); }
    PixelRect shrunk(double dl, double dr, double dt, double db) const {
        return {left + dl, right - dr, top + dt, bottom - db};
    }
    bool contains(double x, double y) const {
        return x >= left && x <= right && y >= top && y <= bottom;
    }
};

/// 一個圖元佔用的資料範圍。兩軸都用「無效」起始，代表沒有資料。
struct AxisLimits {
    CoordinateRange x;
    CoordinateRange y;
    AxisLimits() = default;
    AxisLimits(CoordinateRange xr, CoordinateRange yr) : x(xr), y(yr) {}
    AxisLimits(double l, double r, double b, double t) : x(l, r), y(b, t) {}
    bool hasData() const { return x.isValid() && y.isValid(); }
    void expand(const AxisLimits& o) {
        x.expand(o.x);
        y.expand(o.y);
    }
    void expand(Coordinates c) {
        x.expand(c.x);
        y.expand(c.y);
    }
};

/// 座標軸上被截掉的一段 [from, to]。
struct AxisBreak {
    double from = 0;
    double to = 0;
    bool operator==(const AxisBreak& o) const { return from == o.from && to == o.to; }
};

/// 截斷軸的換算：值 ↔ 沿軸的比例。
///
/// 被截掉的區段不佔長度，每個「完全落在可視範圍內」的截斷再留 gapPx 像素的空隙
/// 畫截斷記號。比例的 0 是 range.min 那端、1 是 range.max 那端（還沒套反向）。
/// 落在截斷區裡的值一律對到空隙的正中間 —— 不能丟掉，不然折線穿過截斷時會斷開。
///
/// breaks 必須已經排序且互不重疊；Axis::addBreak() 保證這一點。
struct AxisBreaks {
    QVector<AxisBreak> breaks;
    double gapPx = 10;

    bool isEmpty() const { return breaks.isEmpty(); }
    double toFraction(double v, const CoordinateRange& r, double lengthPx) const;
    double fromFraction(double f, const CoordinateRange& r, double lengthPx) const;
    /// 可視範圍內每一段沒被截掉的區間（刻度要分段產生）。
    QVector<CoordinateRange> visibleSegments(const CoordinateRange& r) const;
    /// 可視範圍內每個截斷空隙的位置（比例，0–1），畫截斷記號用。
    QVector<std::pair<double, double>> gapFractions(const CoordinateRange& r, double lengthPx) const;
};

/// 資料座標與像素座標的雙向轉換。每次 render 由 Plot 依該圖元綁的軸建立。
/// 對數軸的換算：把範圍的兩端取 log10。範圍不是正的時候給一個看得見的預設，
/// 不讓 log10(0) = −∞ 一路傳下去把整張圖變成 NaN。
inline void logBounds(const CoordinateRange& r, double& lo, double& hi) {
    hi = (r.max > 0) ? std::log10(r.max) : 1.0;
    lo = (r.min > 0) ? std::log10(r.min) : hi - 3.0;
    if (!(hi > lo)) hi = lo + 1.0;
}
/// 對數軸上 v 落在範圍的哪個比例。非正值畫到繪圖區外很遠的地方（線會往下衝出去，不會消失）。
inline double logFraction(double v, const CoordinateRange& r) {
    double lo, hi;
    logBounds(r, lo, hi);
    if (!(v > 0)) return -10.0;
    return (std::log10(v) - lo) / (hi - lo);
}
/// logFraction 的反函數。
inline double logFromFraction(double f, const CoordinateRange& r) {
    double lo, hi;
    logBounds(r, lo, hi);
    return std::pow(10.0, lo + f * (hi - lo));
}

struct Transform {
    PixelRect rect;      ///< 繪圖區（像素）
    CoordinateRange xr;  ///< 綁定的 X 軸範圍
    CoordinateRange yr;  ///< 綁定的 Y 軸範圍
    /// 對應的軸是不是反向的（值由大到小）。
    ///
    /// 這兩個欄位必須和 Axis::inverted 一致：刻度是用 Axis::getPixel 畫的、
    /// 資料是用這裡畫的，兩邊各算各的就會上下顛倒 —— 看起來像資料錯了，
    /// 其實是刻度和資料用了不同的換算。
    bool xInverted = false;
    bool yInverted = false;
    /// 截斷軸。沒有截斷時是 nullptr，走下面的線性快速路徑 ——
    /// 百萬點的 Signal 每一點都要換算，這裡多一個分支判斷就好，不能多走一段迴圈。
    const AxisBreaks* xBreaks = nullptr;
    const AxisBreaks* yBreaks = nullptr;
    /// 對數軸（Axis::logScale）。對數軸不支援截斷，兩者都設時以對數為準。
    bool xLog = false;
    bool yLog = false;

    double px(double x) const {
        const double f = xLog      ? logFraction(x, xr)
                         : xBreaks ? xBreaks->toFraction(x, xr, rect.width())
                                   : (x - xr.min) / xr.span();
        return rect.left + (xInverted ? 1.0 - f : f) * rect.width();
    }
    double py(double y) const {
        const double f = yLog      ? logFraction(y, yr)
                         : yBreaks ? yBreaks->toFraction(y, yr, rect.height())
                                   : (y - yr.min) / yr.span();
        return rect.bottom - (yInverted ? 1.0 - f : f) * rect.height();
    }
    QPointF p(Coordinates c) const { return QPointF(px(c.x), py(c.y)); }
    QPointF p(double x, double y) const { return QPointF(px(x), py(y)); }

    double cx(double pixelX) const {
        double f = (pixelX - rect.left) / rect.width();
        if (xInverted) f = 1.0 - f;
        if (xLog) return logFromFraction(f, xr);
        return xBreaks ? xBreaks->fromFraction(f, xr, rect.width()) : xr.min + f * xr.span();
    }
    double cy(double pixelY) const {
        double f = (rect.bottom - pixelY) / rect.height();
        if (yInverted) f = 1.0 - f;
        if (yLog) return logFromFraction(f, yr);
        return yBreaks ? yBreaks->fromFraction(f, yr, rect.height()) : yr.min + f * yr.span();
    }
    Coordinates c(QPointF pt) const { return {cx(pt.x()), cy(pt.y())}; }

    /// 一個資料單位對應幾個像素。
    double scaleX() const { return rect.width() / xr.span(); }
    double scaleY() const { return rect.height() / yr.span(); }

    QRectF pixelRect(const CoordinateRect& r) const {
        double l = px(r.left), rt = px(r.right);
        double t = py(r.top), b = py(r.bottom);
        return QRectF(QPointF(std::min(l, rt), std::min(t, b)),
                      QPointF(std::max(l, rt), std::max(t, b)));
    }
};

// ───────────────────────────── 樣式 ─────────────────────────────

enum class LinePattern { Solid, Dashed, DenselyDashed, Dotted, DashDot, DashDotDot, None };

/// 線條樣式。width <= 0 或 pattern == None 都視為不畫。
struct LineStyle {
    QColor color = QColor(0, 0, 0);
    double width = 1.0;
    LinePattern pattern = LinePattern::Solid;
    Qt::PenCapStyle cap = Qt::FlatCap;
    Qt::PenJoinStyle join = Qt::MiterJoin;
    /// 手繪感：線會隨機抖動這麼多像素（0 代表筆直）。1～2 像素就很明顯了。
    ///
    /// 給「這是示意圖，不是精確資料」的場合用 —— 讀者看到手繪線就不會去讀小數點。
    /// 抖動是由線的位置決定的，同一張圖每次重畫都一樣，不會閃。
    double handDrawn = 0;
    /// 髮絲線：不管輸出放大幾倍，都畫成 width 個「裝置像素」寬。
    bool hairline = false;

    LineStyle() = default;
    LineStyle(QColor c, double w = 1.0, LinePattern p = LinePattern::Solid)
        : color(c), width(w), pattern(p) {}

    bool isVisible() const {
        return width > 0 && pattern != LinePattern::None && color.alpha() > 0;
    }
    QPen pen() const;
    static LineStyle none() {
        LineStyle s;
        s.pattern = LinePattern::None;
        return s;
    }
};

enum class MarkerShape {
    None,
    FilledCircle, OpenCircle,
    FilledSquare, OpenSquare,
    FilledTriangleUp, OpenTriangleUp,
    FilledTriangleDown, OpenTriangleDown,
    FilledDiamond, OpenDiamond,
    Plus, Asterisk, HashTag, Eks,
    VerticalBar, HorizontalBar
};
// 註：ScottPlot 的 Cross 就是這裡的 Plus（＋），Eks 是 ×。
// 原本兩個名字畫出同一個形狀，已把 Cross 拿掉以免挑了卻看不出差別。

/// 標記樣式。shape == None 視為不畫。
struct MarkerStyle {
    MarkerShape shape = MarkerShape::None;
    double size = 6.0;  ///< 外接方框的邊長（像素）
    QColor fillColor = QColor(0, 0, 0);
    LineStyle outline = LineStyle(QColor(0, 0, 0), 1.0);

    MarkerStyle() = default;
    MarkerStyle(MarkerShape s, double sz, QColor c)
        : shape(s), size(sz), fillColor(c), outline(c, 1.0) {}

    bool isVisible() const { return shape != MarkerShape::None && size > 0; }
    static MarkerStyle none() { return MarkerStyle(); }
};

enum class HatchPattern { None, Horizontal, Vertical, DiagonalUp, DiagonalDown, Checker, Dots };

/// 填色樣式。
/// 漸層填色的方向。
enum class GradientDirection {
    None,        ///< 不用漸層，填 color 或 hatch
    Horizontal,  ///< 沿 X 方向（左 → 右）
    Vertical     ///< 沿 Y 方向（下 → 上，和資料座標同向）
};

struct FillStyle {
    QColor color = QColor(0, 0, 0, 0);
    HatchPattern hatch = HatchPattern::None;
    QColor hatchColor = QColor(0, 0, 0, 80);

    /// 漸層方向。不是 None 時，gradientStops 取代 color 與 hatch。
    GradientDirection gradient = GradientDirection::None;
    /// 漸層的顏色節點，等距分布。至少要兩個，否則當成沒有漸層。
    ///
    /// 要用現成的色階就 Colormap(...).sampleColors(n)；要兩色漸層就直接給兩個顏色。
    QVector<QColor> gradientStops;

    FillStyle() = default;
    explicit FillStyle(QColor c) : color(c) {}

    bool hasGradient() const {
        return gradient != GradientDirection::None && gradientStops.size() >= 2;
    }
    bool isVisible() const {
        return color.alpha() > 0 || hatch != HatchPattern::None || hasGradient();
    }

    QBrush brush() const;
    /// 漸層需要知道要鋪在哪個像素範圍上，所以要傳要填的區域。
    /// 沒有設漸層時這個版本和無參數的完全一樣。
    QBrush brush(const QRectF& bounds) const;

    static FillStyle none() { return FillStyle(); }
    /// 便利建構：沿某個方向，從一組顏色漸層過去。
    static FillStyle linearGradient(GradientDirection dir, QVector<QColor> stops) {
        FillStyle f;
        f.gradient = dir;
        f.gradientStops = std::move(stops);
        return f;
    }
};

/// 字型樣式。size 是 point。
///
/// 圖上每一處文字都由某個 FontStyle 決定，而且每一處都獨立可改 ——
/// 想一次改掉整張圖請用 Plot::visitFonts() 或它上面那幾個捷徑。
struct FontStyle {
    QString family;  ///< 空字串代表用系統預設（中文環境下由 Qt 挑）
    double size = 11.0;

    /// 粗細。
    ///
    /// 大多數字型家族實際上只裝了 Normal 與 Bold 兩個字重，中間那幾級由 Qt 合成，
    /// 出來的樣子因字型而異。要穩定的結果就挑 Normal 或 Bold。
    enum Weight {
        Auto = -1,  ///< 看 bold 這個欄位（預設）
        Thin = 100, ExtraLight = 200, Light = 300, Normal = 400,
        Medium = 500, DemiBold = 600, Bold = 700, ExtraBold = 800, Black = 900
    };
    /// 預設 Auto，也就是沿用 bold。設成其他值之後就蓋過 bold。
    int weight = Auto;
    /// 粗體的簡寫。weight 是 Auto 時才有作用。
    bool bold = false;

    bool italic = false;
    bool underline = false;
    bool strikeOut = false;

    /// 字距的增減（像素）。負值收緊、正值放寬，0 代表用字型原本的。
    double letterSpacing = 0;
    /// 橫向拉伸，百分比。100 是原樣、50 最扁、200 最寬；0 代表不調整。
    ///
    /// 這是把字形橫向縮放出來的，不是換一套窄體 —— 幅度大了會變形。
    int stretch = 0;

    /// 旋轉角度（度，順時針）。
    ///
    /// drawText() 是繞 anchor 轉、再在轉過的座標系裡算對齊的。
    /// 所以「靠下對齊」這種邊緣對齊一旦轉了角度，就不再是螢幕上的下緣了。
    /// 置中對齊（AlignHCenter | AlignVCenter）轉不轉都一樣，
    /// 要把文字擺在某個方向就先用 measureText() 取旋轉後的外接框再外推。
    ///
    /// 例外是座標軸的刻度標籤：那裡要用 Axis::tickLabelRotation，因為版面
    /// 得先知道角度才留得出空間。（tickFont.rotation 也會被接受，但兩個都設時
    /// 以 tickLabelRotation 為準。）
    double rotation = 0;

    /// 多行文字的行距倍率。0 代表用字型原本的行距；1.5 就是一行半。
    double lineHeight = 0;

    QColor color = QColor(0, 0, 0);

    QFont font() const;

    /// 實際生效的字重（把 Auto 換算成 bold 對應的值）。
    int effectiveWeight() const {
        return (weight >= 0) ? weight : (bold ? static_cast<int>(Bold) : static_cast<int>(Normal));
    }
};

/// 一段要畫的文字加它的外觀。
struct Label {
    QString text;
    FontStyle font;
    bool isVisible() const { return !text.isEmpty() && font.color.alpha() > 0; }
};

/// 走訪字型設定用的回呼。Plot::visitFonts() 與各圖元的 visitFonts() 都收這個。
using FontVisitor = std::function<void(FontStyle&)>;

// ───────────────────────────── 調色盤 ─────────────────────────────

/// 自動配色用的顏色序列。index 會循環，所以圖元再多也不會取不到色。
class Palette {
public:
    enum Preset {
        Category10,    ///< matplotlib / D3 的預設十色
        Category20,
        Nord,
        OneHalf,
        Tsitsulin,     ///< 高對比，色盲友善
        Grayscale
    };

    explicit Palette(Preset preset = Category10);
    QColor color(int index) const;
    int count() const { return m_colors.size(); }
    const QVector<QColor>& colors() const { return m_colors; }
    static QStringList presetNames();
    static Palette fromPresetName(const QString& name);
    /// 用自己的一組顏色當調色盤（公司色、品牌色）。空的話退回 Category10。
    static Palette fromColors(const QVector<QColor>& colors);
    /// 順序倒過來的調色盤。
    Palette reversed() const;
    /// 每個顏色取反色（255 − r, g, b）。淺色主題的調色盤搬到深色底上時，
    /// 原本偏深的顏色會融進背景，取反色之後亮度也跟著反過來。
    Palette inverted() const;

private:
    QVector<QColor> m_colors;
};

/// 連續色階（Heatmap / Contour 用）。
class Colormap {
public:
    enum Preset { Viridis, Turbo, Jet, Grayscale, Blues, Hot, Coolwarm };

    explicit Colormap(Preset preset = Viridis);
    /// 用自訂的顏色節點建色階。至少要兩個顏色。
    /// 想要「從藍漸層到綠」這種兩色漸層，就傳 {blue, green}。
    static Colormap fromColors(const QVector<QColor>& stops);
    /// fraction 會被夾到 [0, 1]；NaN 回傳透明色。
    QColor lookup(double fraction) const;
    /// 把 value 依 [min, max] 正規化後取色。
    QColor lookup(double value, double min, double max) const;
    static QStringList presetNames();
    static Colormap fromPresetName(const QString& name);
    /// 從色階上等距取 count 個顏色，拿來餵給 FillStyle::gradientStops。
    QVector<QColor> sampleColors(int count) const;
    /// 頭尾倒過來的色階（深色代表小值時用）。
    Colormap reversed() const;
    /// 切成 count 個色塊的離散色階：同一段裡的值都拿到同一個顏色。
    ///
    /// 連續色階讀不出「這格到底是多少」，切成幾段之後才對得回圖例。
    Colormap stepped(int count) const;
    /// 色階的節點顏色。
    const QVector<QColor>& stops() const { return m_stops; }
    /// 離散的段數。0 代表連續。
    int steps() const { return m_steps; }

private:
    QVector<QColor> m_stops;
    int m_steps = 0;
};

// ───────────────────────── 繪圖輔助 ─────────────────────────

/// 產生一段環形扇形的像素路徑。rInner = 0 就是一般的扇形。
/// 弧線用線段逼近，所以兩軸比例不同（圓被壓成橢圓）時結果仍然正確 ——
/// 這點是 QPainter::drawPie 做不到的，它的角度參數在橢圓上不是幾何角度。
QPainterPath sectorPath(const struct Transform& t, Coordinates center,
                        double rOuter, double rInner, double startDeg, double sweepDeg);

/// 橢圓版：X 與 Y 方向各有自己的半徑。
///
/// 角度仍然是「參數角」而不是幾何角：點的位置是 (rx·cos θ, ry·sin θ)。
/// 兩者在 rx != ry 時並不相等，但參數角才是畫橢圓弧的通用寫法，
/// 也是 ScottPlot 等函式庫的約定 —— 起點落在 +X、逆時針。
QPainterPath sectorPath(const struct Transform& t, Coordinates center,
                        double rxOuter, double ryOuter, double rxInner, double ryInner,
                        double startDeg, double sweepDeg);

/// 產生一段「開放」的橢圓弧路徑：只有曲線本身，不連回圓心、也不封口。
/// rx == ry 就是圓弧。
QPainterPath arcPath(const struct Transform& t, Coordinates center, double rx, double ry,
                     double startDeg, double sweepDeg);

/// 把一段文字沿著圓弧排，每個字各自轉到該處的切線方向。
///
/// 和「整段文字轉同一個角度」的差別：弧長一長，整段轉同一個角度就會離開弧線，
/// 兩端翹起來。環圈圖外圈的分類帶一定要逐字排才貼得住。
///
/// centerDeg 是文字的中心角度，radius 是文字基線所在的半徑（資料單位）。
/// 文字在下半圈時預設會翻轉 180°，否則字是倒的；autoFlip 設 false 可以關掉。
///
/// 注意這裡假設兩軸的比例相同（equalAxisScale）。比例不同的話弧在畫面上是橢圓，
/// 字距會算不準 —— 環狀圖本來就該配 equalAxisScale。
void drawTextOnArc(QPainter& painter, const struct Transform& t, Coordinates center,
                   double radius, double centerDeg, const Label& label,
                   bool autoFlip = true);

/// 一段文字沿弧排會佔多少角度（度）。給版面判斷「放不放得下」用。
double arcTextSpanDegrees(const struct Transform& t, Coordinates center, double radius,
                          const Label& label);

/// 兩個顏色之間線性內插。fraction 0 回傳 a，1 回傳 b。
///
/// 在 sRGB 分量上直接內插 —— 和 ScottPlot 的 Color Interpolation 同一種做法。
/// 色彩學上「正確」的做法是先轉到線性光再插，但那樣中段會偏亮，
/// 和大多數繪圖庫的結果對不起來。
QColor blendColors(QColor a, QColor b, double fraction);

/// 從 a 漸層到 b 的 count 個顏色。
QVector<QColor> gradientColors(QColor a, QColor b, int count);

/// 畫一個標記。center 是像素座標。
void drawMarker(QPainter& painter, QPointF center, const MarkerStyle& style);

/// 畫一串標記，比逐點呼叫 drawMarker 少很多 QPen/QBrush 切換。
void drawMarkers(QPainter& painter, const QVector<QPointF>& centers, const MarkerStyle& style);

/// 以 alignment 指定的角落／邊為基準畫文字，回傳佔用的像素矩形。
QRectF drawText(QPainter& painter, QPointF anchor, const Label& label,
                Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop);

/// 量一段文字的尺寸（不畫）。
QSizeF measureText(const Label& label);

/// 把數值格式化成軸標籤用的字串（自動挑小數位數，大／小數值轉科學記號）。
///
/// axisMaxAbs 是整根軸上最大的絕對值。給了它，同一根軸的每個標籤就會用同一種
/// 格式 —— 否則會出現 900000 旁邊跟著 1e+06 這種前後不一致的刻度。
QString formatTickLabel(double value, double tickSpacing, double axisMaxAbs = 0);

}  // namespace qtplot

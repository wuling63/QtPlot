#include "Core.h"

#include <QFontMetricsF>
#include <QImage>
#include <QLinearGradient>
#include <QPaintEngine>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

#include <array>

namespace qtplot {

// ───────────────────────────── 截斷軸 ─────────────────────────────

namespace {

// 一次換算要用的版面：可視範圍內的截斷段、每資料單位幾像素、空隙寬度。
// 每次呼叫都重算 —— 截斷通常只有一兩個，這比快取再判斷失效便宜，也不會有過期的問題。
struct BreakLayout {
    struct Piece {
        double from, to;  // 已裁到可視範圍內
        bool gap;         // 完全落在範圍內才留空隙；壓在邊上的只是被吃掉，不畫記號
    };
    // 固定大小、放在堆疊上：這個結構每換算一個點就建一次，不能動態配置。
    // 同時看得到超過 8 個截斷的圖不存在，多出來的就當沒有。
    static constexpr int kMax = 8;
    std::array<Piece, kMax> pieces{};
    int count = 0;
    const Piece* begin() const { return pieces.data(); }
    const Piece* end() const { return pieces.data() + count; }
    double k = 1;     // 每資料單位的像素數
    double gap = 0;   // 每個空隙的像素數
    double length = 1;
    bool linear = true;
};

BreakLayout layoutBreaks(const AxisBreaks& b, const CoordinateRange& r, double lengthPx) {
    BreakLayout L;
    const double span = r.span();
    if (!(span > 0)) return L;
    double removed = 0;
    int gaps = 0;
    for (const AxisBreak& br : b.breaks) {
        const double lo = std::max(br.from, r.min), hi = std::min(br.to, r.max);
        if (hi <= lo) continue;
        const bool gap = br.from > r.min && br.to < r.max;
        if (L.count == BreakLayout::kMax) break;
        L.pieces[L.count++] = {lo, hi, gap};
        removed += hi - lo;
        gaps += gap ? 1 : 0;
    }
    const double kept = span - removed;
    if (L.count == 0 || !(kept > 0)) return L;  // 整段都被截掉：退回線性，至少不會除以零

    L.length = lengthPx > 0 ? lengthPx : 1;
    // 空隙最多吃掉一半的長度，繪圖區很小時資料才不會被擠沒
    L.gap = (gaps > 0 && lengthPx > 0) ? std::min(b.gapPx, L.length * 0.5 / gaps) : 0;
    L.k = (L.length - gaps * L.gap) / kept;
    L.linear = false;
    return L;
}

}  // namespace

double AxisBreaks::toFraction(double v, const CoordinateRange& r, double lengthPx) const {
    const BreakLayout L = layoutBreaks(*this, r, lengthPx);
    if (L.linear) return (v - r.min) / r.span();
    double pos = 0, cur = r.min;
    for (const auto& p : L) {
        if (v <= p.from) return (pos + (v - cur) * L.k) / L.length;
        pos += (p.from - cur) * L.k;
        const double g = p.gap ? L.gap : 0;
        if (v < p.to) return (pos + g / 2) / L.length;
        pos += g;
        cur = p.to;
    }
    return (pos + (v - cur) * L.k) / L.length;
}

double AxisBreaks::fromFraction(double f, const CoordinateRange& r, double lengthPx) const {
    const BreakLayout L = layoutBreaks(*this, r, lengthPx);
    if (L.linear) return r.min + f * r.span();
    const double t = f * L.length;
    double pos = 0, cur = r.min;
    for (const auto& p : L) {
        const double end = pos + (p.from - cur) * L.k;
        if (t <= end) return cur + (t - pos) / L.k;
        pos = end;
        const double g = p.gap ? L.gap : 0;
        // 點在空隙裡：靠哪一邊就算哪一邊的端點
        if (t < pos + g) return (t < pos + g / 2) ? p.from : p.to;
        pos += g;
        cur = p.to;
    }
    return cur + (t - pos) / L.k;
}

QVector<CoordinateRange> AxisBreaks::visibleSegments(const CoordinateRange& r) const {
    QVector<CoordinateRange> out;
    double cur = r.min;
    for (const AxisBreak& br : breaks) {
        const double lo = std::max(br.from, r.min), hi = std::min(br.to, r.max);
        if (hi <= lo) continue;
        if (lo > cur) out.append({cur, lo});
        cur = hi;
    }
    if (r.max > cur) out.append({cur, r.max});
    return out;
}

QVector<std::pair<double, double>> AxisBreaks::gapFractions(const CoordinateRange& r, double lengthPx) const {
    QVector<std::pair<double, double>> out;
    const BreakLayout L = layoutBreaks(*this, r, lengthPx);
    if (L.linear) return out;
    for (const auto& p : L) {
        if (!p.gap) continue;
        // 值剛好等於 from 時換算出來的就是空隙的起點
        const double start = toFraction(p.from, r, lengthPx);
        out.append({start, start + L.gap / L.length});
    }
    return out;
}

// ───────────────────────────── 樣式 ─────────────────────────────

QPen LineStyle::pen() const {
    if (!isVisible()) return QPen(Qt::NoPen);
    QPen p(color);
    p.setWidthF(width);
    p.setCosmetic(hairline);
    p.setCapStyle(cap);
    p.setJoinStyle(join);
    // 虛線間距用線寬的倍數指定，線變粗時虛線才不會糊成一團。
    switch (pattern) {
    case LinePattern::Solid:         p.setStyle(Qt::SolidLine); break;
    case LinePattern::Dashed:        p.setDashPattern({4, 3}); break;
    case LinePattern::DenselyDashed: p.setDashPattern({2, 1.5}); break;
    case LinePattern::Dotted:        p.setDashPattern({1, 3}); p.setCapStyle(Qt::RoundCap); break;
    case LinePattern::DashDot:       p.setDashPattern({5, 3, 1, 3}); break;
    case LinePattern::DashDotDot:    p.setDashPattern({5, 3, 1, 3, 1, 3}); break;
    case LinePattern::None:          p.setStyle(Qt::NoPen); break;
    }
    return p;
}

QBrush FillStyle::brush(const QRectF& bounds) const {
    if (!hasGradient()) return brush();

    // QLinearGradient 的座標是像素，而像素的 y 是往下長的，
    // 所以「垂直向上」的漸層要從 bottom 畫到 top，否則顏色會上下顛倒。
    QLinearGradient g;
    if (gradient == GradientDirection::Horizontal)
        g = QLinearGradient(bounds.left(), bounds.top(), bounds.right(), bounds.top());
    else
        g = QLinearGradient(bounds.left(), bounds.bottom(), bounds.left(), bounds.top());

    const int n = static_cast<int>(gradientStops.size());
    for (int i = 0; i < n; ++i) g.setColorAt(static_cast<double>(i) / (n - 1), gradientStops[i]);
    return QBrush(g);
}

QBrush FillStyle::brush() const {
    // 沒有範圍可用時，漸層退回成中間那個顏色 —— 總比完全不填好，
    // 而且圖例的小方塊就是走這條路
    if (hasGradient()) return QBrush(gradientStops[gradientStops.size() / 2]);
    if (hatch == HatchPattern::None) {
        return color.alpha() > 0 ? QBrush(color) : QBrush(Qt::NoBrush);
    }
    Qt::BrushStyle s = Qt::NoBrush;
    switch (hatch) {
    case HatchPattern::Horizontal:   s = Qt::HorPattern; break;
    case HatchPattern::Vertical:     s = Qt::VerPattern; break;
    case HatchPattern::DiagonalUp:   s = Qt::BDiagPattern; break;
    case HatchPattern::DiagonalDown: s = Qt::FDiagPattern; break;
    case HatchPattern::Checker:      s = Qt::CrossPattern; break;
    case HatchPattern::Dots:         s = Qt::Dense5Pattern; break;
    case HatchPattern::None:         break;
    }
    return QBrush(hatchColor, s);
}

QVector<QColor> Colormap::sampleColors(int count) const {
    QVector<QColor> out;
    if (count <= 0) return out;
    out.reserve(count);
    if (count == 1) {
        out.append(lookup(0.5));
        return out;
    }
    for (int i = 0; i < count; ++i) out.append(lookup(static_cast<double>(i) / (count - 1)));
    return out;
}

QFont FontStyle::font() const {
    QFont f;
    if (!family.isEmpty()) f.setFamily(family);
    f.setPointSizeF(size);
    // setWeight 而不是 setBold：setBold(true) 等於 setWeight(Bold)，
    // 中間那幾級用 setBold 表達不出來。
    f.setWeight(static_cast<QFont::Weight>(effectiveWeight()));
    f.setItalic(italic);
    f.setUnderline(underline);
    f.setStrikeOut(strikeOut);
    if (letterSpacing != 0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    if (stretch > 0) f.setStretch(stretch);

    // 註：旋轉過的文字在 Windows 上會有彩色邊緣（次像素反鋸齒）。
    // 試過 QFont::NoSubpixelAntialias，量過像素，完全沒有作用 ——
    // 目前無解，留著這行註解免得下一個人再試一次。
    return f;
}

// ───────────────────────────── 調色盤 ─────────────────────────────

namespace {

QVector<QColor> hexList(const QStringList& hexes) {
    QVector<QColor> out;
    out.reserve(hexes.size());
    for (const QString& h : hexes) out.append(QColor(h));
    return out;
}

}  // namespace

Palette::Palette(Preset preset) {
    switch (preset) {
    case Category10:
        m_colors = hexList({"#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
                            "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"});
        break;
    case Category20:
        m_colors = hexList({"#1f77b4", "#aec7e8", "#ff7f0e", "#ffbb78", "#2ca02c",
                            "#98df8a", "#d62728", "#ff9896", "#9467bd", "#c5b0d5",
                            "#8c564b", "#c49c94", "#e377c2", "#f7b6d2", "#7f7f7f",
                            "#c7c7c7", "#bcbd22", "#dbdb8d", "#17becf", "#9edae5"});
        break;
    case Nord:
        m_colors = hexList({"#5e81ac", "#bf616a", "#a3be8c", "#ebcb8b", "#b48ead",
                            "#88c0d0", "#d08770", "#8fbcbb"});
        break;
    case OneHalf:
        m_colors = hexList({"#383a42", "#e4564a", "#50a14f", "#c18402", "#0084bc",
                            "#a626a4", "#0897b3"});
        break;
    case Tsitsulin:
        m_colors = hexList({"#004488", "#bb5566", "#ddaa33", "#009988", "#ee7733",
                            "#33bbee", "#cc3311", "#aa4499", "#117733", "#882255"});
        break;
    case Grayscale:
        m_colors = hexList({"#1a1a1a", "#4d4d4d", "#808080", "#b3b3b3", "#333333",
                            "#666666", "#999999"});
        break;
    }
    if (m_colors.isEmpty()) m_colors.append(QColor(Qt::black));
}

Palette Palette::fromColors(const QVector<QColor>& colors) {
    Palette p;
    if (!colors.isEmpty()) p.m_colors = colors;
    return p;
}

Palette Palette::reversed() const {
    Palette p = *this;
    std::reverse(p.m_colors.begin(), p.m_colors.end());
    return p;
}

Palette Palette::inverted() const {
    Palette p = *this;
    for (QColor& c : p.m_colors) c = QColor(255 - c.red(), 255 - c.green(), 255 - c.blue(), c.alpha());
    return p;
}

QColor Palette::color(int index) const {
    if (m_colors.isEmpty()) return QColor(Qt::black);
    // 負的 index 也要能用（圖元被移除後編號可能倒退）
    int i = index % m_colors.size();
    if (i < 0) i += m_colors.size();
    return m_colors.at(i);
}

QStringList Palette::presetNames() {
    return {"Category10", "Category20", "Nord", "OneHalf", "Tsitsulin", "Grayscale"};
}

Palette Palette::fromPresetName(const QString& name) {
    const QStringList names = presetNames();
    int i = names.indexOf(name);
    return Palette(i >= 0 ? static_cast<Preset>(i) : Category10);
}

Colormap::Colormap(Preset preset) {
    switch (preset) {
    case Viridis:
        m_stops = hexList({"#440154", "#482878", "#3e4989", "#31688e", "#26828e",
                           "#1f9e89", "#35b779", "#6ece58", "#b5de2b", "#fde725"});
        break;
    case Turbo:
        m_stops = hexList({"#30123b", "#4145ab", "#4675ed", "#39a2fc", "#1bcfd4",
                           "#24eca6", "#61fc6c", "#a4fc3b", "#d1e834", "#f3c63a",
                           "#fe9b2d", "#f36315", "#d93806", "#b11901", "#7a0402"});
        break;
    case Jet:
        m_stops = hexList({"#00007f", "#0000ff", "#007fff", "#00ffff", "#7fff7f",
                           "#ffff00", "#ff7f00", "#ff0000", "#7f0000"});
        break;
    case Grayscale:
        m_stops = hexList({"#000000", "#ffffff"});
        break;
    case Blues:
        m_stops = hexList({"#f7fbff", "#deebf7", "#c6dbef", "#9ecae1", "#6baed6",
                           "#4292c6", "#2171b5", "#08519c", "#08306b"});
        break;
    case Hot:
        m_stops = hexList({"#000000", "#7f0000", "#ff0000", "#ff7f00", "#ffff00", "#ffffff"});
        break;
    case Coolwarm:
        m_stops = hexList({"#3b4cc0", "#6788ee", "#9abbff", "#c9d7f0", "#edd1c2",
                           "#f7a789", "#e26952", "#b40426"});
        break;
    }
    if (m_stops.size() < 2) m_stops = {QColor(Qt::black), QColor(Qt::white)};
}

Colormap Colormap::fromColors(const QVector<QColor>& stops) {
    Colormap m;
    if (stops.size() >= 2) m.m_stops = stops;
    return m;
}

QColor blendColors(QColor a, QColor b, double fraction) {
    const double t = std::clamp(fraction, 0.0, 1.0);
    auto mix = [t](int x, int y) {
        return static_cast<int>(std::lround(x + (y - x) * t));
    };
    return QColor(mix(a.red(), b.red()), mix(a.green(), b.green()), mix(a.blue(), b.blue()),
                  mix(a.alpha(), b.alpha()));
}

QVector<QColor> gradientColors(QColor a, QColor b, int count) {
    QVector<QColor> out;
    if (count <= 0) return out;
    out.reserve(count);
    if (count == 1) {
        out.append(a);
        return out;
    }
    for (int i = 0; i < count; ++i) out.append(blendColors(a, b, i / (count - 1.0)));
    return out;
}

Colormap Colormap::reversed() const {
    Colormap m = *this;
    std::reverse(m.m_stops.begin(), m.m_stops.end());
    return m;
}

Colormap Colormap::stepped(int count) const {
    Colormap m = *this;
    m.m_steps = std::max(0, count);
    return m;
}

QColor Colormap::lookup(double fraction) const {
    if (std::isnan(fraction)) return QColor(0, 0, 0, 0);
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (m_steps >= 2) {
        // 取每一段的中心色，第一段與最後一段才會是色階的兩端
        const int k = std::min(m_steps - 1, static_cast<int>(fraction * m_steps));
        fraction = k / (m_steps - 1.0);
    }
    double pos = fraction * (m_stops.size() - 1);
    int i = static_cast<int>(std::floor(pos));
    if (i >= m_stops.size() - 1) return m_stops.last();
    double t = pos - i;
    const QColor& a = m_stops.at(i);
    const QColor& b = m_stops.at(i + 1);
    // 在 sRGB 上線性內插。色彩學上不是最正確，但和 matplotlib 的查表結果夠接近。
    return QColor(static_cast<int>(std::lround(a.red() + (b.red() - a.red()) * t)),
                  static_cast<int>(std::lround(a.green() + (b.green() - a.green()) * t)),
                  static_cast<int>(std::lround(a.blue() + (b.blue() - a.blue()) * t)),
                  static_cast<int>(std::lround(a.alpha() + (b.alpha() - a.alpha()) * t)));
}

QColor Colormap::lookup(double value, double min, double max) const {
    if (max <= min) return lookup(0.5);
    return lookup((value - min) / (max - min));
}

QStringList Colormap::presetNames() {
    return {"Viridis", "Turbo", "Jet", "Grayscale", "Blues", "Hot", "Coolwarm"};
}

Colormap Colormap::fromPresetName(const QString& name) {
    const QStringList names = presetNames();
    int i = names.indexOf(name);
    return Colormap(i >= 0 ? static_cast<Preset>(i) : Viridis);
}

// ───────────────────────── 標記繪製 ─────────────────────────

namespace {

/// 回傳形狀是否要填色（Open* 與線狀標記不填）。
bool shapeIsFilled(MarkerShape s) {
    switch (s) {
    case MarkerShape::FilledCircle:
    case MarkerShape::FilledSquare:
    case MarkerShape::FilledTriangleUp:
    case MarkerShape::FilledTriangleDown:
    case MarkerShape::FilledDiamond:
        return true;
    default:
        return false;
    }
}

void markerPath(QPainterPath& path, QPointF c, double r, MarkerShape shape) {
    switch (shape) {
    case MarkerShape::FilledCircle:
    case MarkerShape::OpenCircle:
        path.addEllipse(c, r, r);
        break;
    case MarkerShape::FilledSquare:
    case MarkerShape::OpenSquare:
        path.addRect(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
        break;
    case MarkerShape::FilledTriangleUp:
    case MarkerShape::OpenTriangleUp:
        path.moveTo(c.x(), c.y() - r);
        path.lineTo(c.x() + r, c.y() + r);
        path.lineTo(c.x() - r, c.y() + r);
        path.closeSubpath();
        break;
    case MarkerShape::FilledTriangleDown:
    case MarkerShape::OpenTriangleDown:
        path.moveTo(c.x(), c.y() + r);
        path.lineTo(c.x() + r, c.y() - r);
        path.lineTo(c.x() - r, c.y() - r);
        path.closeSubpath();
        break;
    case MarkerShape::FilledDiamond:
    case MarkerShape::OpenDiamond:
        path.moveTo(c.x(), c.y() - r);
        path.lineTo(c.x() + r, c.y());
        path.lineTo(c.x(), c.y() + r);
        path.lineTo(c.x() - r, c.y());
        path.closeSubpath();
        break;
    case MarkerShape::Plus:
        path.moveTo(c.x() - r, c.y());  path.lineTo(c.x() + r, c.y());
        path.moveTo(c.x(), c.y() - r);  path.lineTo(c.x(), c.y() + r);
        break;
    case MarkerShape::Eks:
        path.moveTo(c.x() - r, c.y() - r);  path.lineTo(c.x() + r, c.y() + r);
        path.moveTo(c.x() - r, c.y() + r);  path.lineTo(c.x() + r, c.y() - r);
        break;
    case MarkerShape::Asterisk: {
        path.moveTo(c.x(), c.y() - r);  path.lineTo(c.x(), c.y() + r);
        double d = r * 0.866;  // sin(60°)
        double h = r * 0.5;
        path.moveTo(c.x() - d, c.y() - h);  path.lineTo(c.x() + d, c.y() + h);
        path.moveTo(c.x() - d, c.y() + h);  path.lineTo(c.x() + d, c.y() - h);
        break;
    }
    case MarkerShape::HashTag: {
        double o = r * 0.45;
        path.moveTo(c.x() - r, c.y() - o);  path.lineTo(c.x() + r, c.y() - o);
        path.moveTo(c.x() - r, c.y() + o);  path.lineTo(c.x() + r, c.y() + o);
        path.moveTo(c.x() - o, c.y() - r);  path.lineTo(c.x() - o, c.y() + r);
        path.moveTo(c.x() + o, c.y() - r);  path.lineTo(c.x() + o, c.y() + r);
        break;
    }
    case MarkerShape::VerticalBar:
        path.moveTo(c.x(), c.y() - r);  path.lineTo(c.x(), c.y() + r);
        break;
    case MarkerShape::HorizontalBar:
        path.moveTo(c.x() - r, c.y());  path.lineTo(c.x() + r, c.y());
        break;
    case MarkerShape::None:
        break;
    }
}

}  // namespace

namespace {

/// 弧線用幾段線段逼近。每 2 度一段，整圈約 180 段，肉眼看不出是折線。
int arcSegments(double sweepDeg) {
    return std::clamp(static_cast<int>(std::ceil(std::abs(sweepDeg) / 2.0)), 2, 720);
}

}  // namespace

QPainterPath sectorPath(const Transform& t, Coordinates center, double rOuter, double rInner,
                        double startDeg, double sweepDeg) {
    return sectorPath(t, center, rOuter, rOuter, rInner, rInner, startDeg, sweepDeg);
}

QPainterPath sectorPath(const Transform& t, Coordinates center, double rxOuter, double ryOuter,
                        double rxInner, double ryInner, double startDeg, double sweepDeg) {
    QPainterPath path;
    if (rxOuter <= 0 || ryOuter <= 0 || sweepDeg == 0) return path;

    const int segments = arcSegments(sweepDeg);
    // 內半徑只要有一個方向是正的就當成環 —— 兩個方向只給一個通常是寫錯，
    // 但當成實心扇形畫出來會更難發現
    const bool donut = rxInner > 0 || ryInner > 0;

    auto pointAt = [&](double rx, double ry, double deg) {
        const double rad = deg * kPi / 180.0;
        return t.p(center.x + rx * std::cos(rad), center.y + ry * std::sin(rad));
    };

    // 外弧：起點 → 終點
    path.moveTo(pointAt(rxOuter, ryOuter, startDeg));
    for (int i = 1; i <= segments; ++i)
        path.lineTo(pointAt(rxOuter, ryOuter, startDeg + sweepDeg * i / segments));

    if (donut) {
        // 內弧：反向繞回來，中間就空出一個洞
        for (int i = segments; i >= 0; --i)
            path.lineTo(pointAt(rxInner, ryInner, startDeg + sweepDeg * i / segments));
    } else {
        path.lineTo(t.p(center.x, center.y));
    }
    path.closeSubpath();
    return path;
}

QPainterPath arcPath(const Transform& t, Coordinates center, double rx, double ry,
                     double startDeg, double sweepDeg) {
    QPainterPath path;
    if (rx <= 0 || ry <= 0 || sweepDeg == 0) return path;

    const int segments = arcSegments(sweepDeg);
    auto pointAt = [&](double deg) {
        const double rad = deg * kPi / 180.0;
        return t.p(center.x + rx * std::cos(rad), center.y + ry * std::sin(rad));
    };

    path.moveTo(pointAt(startDeg));
    for (int i = 1; i <= segments; ++i)
        path.lineTo(pointAt(startDeg + sweepDeg * i / segments));
    // 刻意不 closeSubpath()：圓弧是一條開放的曲線，封口會多一條弦
    return path;
}

void drawMarker(QPainter& painter, QPointF center, const MarkerStyle& style) {
    drawMarkers(painter, {center}, style);
}

void drawMarkers(QPainter& painter, const QVector<QPointF>& centers, const MarkerStyle& style) {
    if (!style.isVisible() || centers.isEmpty()) return;

    const double r = style.size / 2.0;
    const bool filled = shapeIsFilled(style.shape);

    // 點很多又是畫到點陣圖上時改用「蓋章」：先把一個標記畫成小圖，再一張張貼上去。
    // 全部標記組成一條 QPainterPath 再填，路徑填色的成本隨子路徑數暴增 ——
    // 十萬個圓要好幾秒。蓋章是每點一次小圖拷貝，快兩三個數量級。
    // 向量輸出（SVG、PDF、印表機）不能這樣做，那邊要的是真的形狀，所以只限點陣引擎。
    const QTransform& xf = painter.transform();
    const bool rasterTarget = painter.paintEngine() && painter.paintEngine()->type() == QPaintEngine::Raster;
    if (centers.size() > 400 && rasterTarget && xf.type() <= QTransform::TxScale) {
        const double scale = std::max(std::abs(xf.m11()), std::abs(xf.m22()));
        const double outlineW = style.outline.isVisible() ? style.outline.width : 1.0;
        // 小圖的中心放在整數裝置像素上，貼的時候把每個點的中心捨入到最近的裝置像素：
        // 兩邊對齊，標記才不會整批偏半個像素
        const int halfDev = static_cast<int>(std::ceil((r + outlineW + 1) * scale));
        const int px = halfDev * 2 + 1;
        QImage stamp(px, px, QImage::Format_ARGB32_Premultiplied);
        stamp.setDevicePixelRatio(scale);
        stamp.fill(Qt::transparent);
        {
            QPainter sp(&stamp);
            sp.setRenderHint(QPainter::Antialiasing, painter.testRenderHint(QPainter::Antialiasing));
            QPainterPath one;
            markerPath(one, QPointF(halfDev / scale, halfDev / scale), r, style.shape);
            if (filled) {
                sp.setBrush(QBrush(style.fillColor));
                sp.setPen(style.outline.isVisible() ? style.outline.pen() : QPen(Qt::NoPen));
            } else {
                sp.setBrush(Qt::NoBrush);
                LineStyle ol = style.outline;
                if (!ol.isVisible()) ol = LineStyle(style.fillColor, 1.0);
                sp.setPen(ol.pen());
            }
            sp.drawPath(one);
        }
        // 貼到整數裝置像素上：非整數位置會讓每次貼圖都重新取樣，慢很多
        const QTransform inv = xf.inverted();
        for (QPointF c : centers) {
            if (!std::isfinite(c.x()) || !std::isfinite(c.y())) continue;
            const QPointF dev = xf.map(c);
            const QPointF corner(std::round(dev.x()) - halfDev, std::round(dev.y()) - halfDev);
            painter.drawImage(inv.map(corner), stamp);
        }
        return;
    }

    QPainterPath path;
    for (QPointF c : centers) {
        if (!std::isfinite(c.x()) || !std::isfinite(c.y())) continue;
        markerPath(path, c, r, style.shape);
    }
    if (path.isEmpty()) return;

    painter.save();
    if (filled) {
        painter.setBrush(QBrush(style.fillColor));
        // 填色標記的外框若沒另外設色，就不畫外框，避免深色描邊讓小點變糊。
        painter.setPen(style.outline.isVisible() ? style.outline.pen() : QPen(Qt::NoPen));
    } else {
        painter.setBrush(Qt::NoBrush);
        LineStyle ol = style.outline;
        if (!ol.isVisible()) ol = LineStyle(style.fillColor, 1.0);
        painter.setPen(ol.pen());
    }
    painter.drawPath(path);
    painter.restore();
}

// ───────────────────────── 文字 ─────────────────────────

namespace {

/// 自訂行距時一行一行排；沒設行距（或只有一行）時回傳 false，交給 Qt 自己排。
bool customLineLayout(const Label& label, const QFontMetricsF& fm, QStringList& lines,
                      double& step, QSizeF& size) {
    if (label.font.lineHeight <= 0 || !label.text.contains('\n')) return false;
    lines = label.text.split('\n');
    step = fm.lineSpacing() * label.font.lineHeight;
    double w = 0;
    for (const QString& s : lines) w = std::max(w, fm.horizontalAdvance(s));
    size = QSizeF(w, fm.height() + (lines.size() - 1) * step);
    return true;
}

/// 在 box 裡逐行畫。每一行依 box 的水平對齊各自對齊。
void drawLines(QPainter& painter, const QRectF& box, const QStringList& lines, double step,
               const QFontMetricsF& fm, Qt::Alignment alignment) {
    for (int i = 0; i < lines.size(); ++i) {
        const double lw = fm.horizontalAdvance(lines[i]);
        double x = box.left();
        if (alignment & Qt::AlignHCenter)     x = box.center().x() - lw / 2;
        else if (alignment & Qt::AlignRight)  x = box.right() - lw;
        painter.drawText(QPointF(x, box.top() + i * step + fm.ascent()), lines[i]);
    }
}

}  // namespace

QSizeF measureText(const Label& label) {
    if (label.text.isEmpty()) return QSizeF(0, 0);
    QFontMetricsF fm(label.font.font());
    QRectF r = fm.boundingRect(QRectF(0, 0, 10000, 10000),
                               Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip,
                               label.text);
    QSizeF s(r.width(), r.height());
    {
        QStringList lines;
        double step = 0;
        QSizeF custom;
        if (customLineLayout(label, fm, lines, step, custom)) s = custom;
    }
    if (label.font.rotation != 0) {
        // 旋轉後的外接框
        double rad = label.font.rotation * M_PI / 180.0;
        double c = std::abs(std::cos(rad)), sn = std::abs(std::sin(rad));
        s = QSizeF(s.width() * c + s.height() * sn, s.width() * sn + s.height() * c);
    }
    return s;
}

QRectF drawText(QPainter& painter, QPointF anchor, const Label& label, Qt::Alignment alignment) {
    if (!label.isVisible()) return QRectF(anchor, QSizeF(0, 0));

    QFontMetricsF fm(label.font.font());
    QRectF textRect = fm.boundingRect(QRectF(0, 0, 10000, 10000),
                                      Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip,
                                      label.text);
    QStringList lines;
    double lineStep = 0;
    QSizeF customSize;
    const bool custom = customLineLayout(label, fm, lines, lineStep, customSize);
    if (custom) textRect = QRectF(QPointF(0, 0), customSize);
    const double w = textRect.width();
    const double h = textRect.height();
    // 多行文字在框內的水平對齊跟著錨點的對齊走
    const int textFlags = Qt::TextDontClip | Qt::AlignTop |
                          ((alignment & Qt::AlignHCenter) ? Qt::AlignHCenter
                           : (alignment & Qt::AlignRight) ? Qt::AlignRight : Qt::AlignLeft);

    double dx = 0, dy = 0;
    if (alignment & Qt::AlignHCenter)     dx = -w / 2;
    else if (alignment & Qt::AlignRight)  dx = -w;
    if (alignment & Qt::AlignVCenter)     dy = -h / 2;
    else if (alignment & Qt::AlignBottom) dy = -h;

    painter.save();
    painter.setFont(label.font.font());
    painter.setPen(QPen(label.font.color));
    if (label.font.rotation != 0) {
        painter.translate(anchor);
        painter.rotate(label.font.rotation);
        if (custom) drawLines(painter, QRectF(dx, dy, w, h), lines, lineStep, fm, alignment);
        else        painter.drawText(QRectF(dx, dy, w, h), textFlags, label.text);
        painter.restore();
        // 旋轉後的實際佔用框不好算，回傳以 anchor 為中心的外接框就夠版面用
        QSizeF s = measureText(label);
        return QRectF(anchor.x() - s.width() / 2, anchor.y() - s.height() / 2,
                      s.width(), s.height());
    }
    QRectF out(anchor.x() + dx, anchor.y() + dy, w, h);
    if (custom) drawLines(painter, out, lines, lineStep, fm, alignment);
    else        painter.drawText(out, textFlags, label.text);
    painter.restore();
    return out;
}

namespace {

/// 文字基線半徑換算成像素，以及每個字的寬度。
struct ArcTextMetrics {
    double radiusPx = 0;
    double totalWidthPx = 0;
    QVector<double> advances;
};

ArcTextMetrics measureArcText(const Transform& t, Coordinates center, double radius,
                              const Label& label) {
    ArcTextMetrics m;
    const QPointF c = t.p(center);
    const QPointF edge = t.p(center.x + radius, center.y);
    m.radiusPx = std::hypot(edge.x() - c.x(), edge.y() - c.y());

    QFontMetricsF fm(label.font.font());
    m.advances.reserve(label.text.size());
    for (const QChar& ch : label.text) {
        const double w = fm.horizontalAdvance(ch);
        m.advances.append(w);
        m.totalWidthPx += w;
    }
    return m;
}

}  // namespace

double arcTextSpanDegrees(const Transform& t, Coordinates center, double radius,
                          const Label& label) {
    const ArcTextMetrics m = measureArcText(t, center, radius, label);
    if (!(m.radiusPx > 0)) return 0;
    return m.totalWidthPx / m.radiusPx * 180.0 / kPi;
}

void drawTextOnArc(QPainter& painter, const Transform& t, Coordinates center, double radius,
                   double centerDeg, const Label& label, bool autoFlip) {
    if (!label.isVisible()) return;
    const ArcTextMetrics m = measureArcText(t, center, radius, label);
    if (!(m.radiusPx > 0) || m.advances.isEmpty()) return;

    // 下半圈的字如果不翻，讀起來是倒的
    const double norm = std::fmod(std::fmod(centerDeg, 360.0) + 360.0, 360.0);
    const bool flip = autoFlip && (norm > 180.0);

    const QPointF c = t.p(center);
    const double spanRad = m.totalWidthPx / m.radiusPx;
    // 文字以 centerDeg 為中心。θ 增加的方向在畫面上是往左，
    // 所以要從「中心角 + 半個跨度」開始往回排，字才會由左往右讀。
    const double dir = flip ? -1.0 : 1.0;
    double theta = centerDeg * kPi / 180.0 + dir * spanRad / 2;

    QFontMetricsF fm(label.font.font());
    // 讓字的視覺中心落在弧線上（基線要往下移半個字高）
    const double baselineY = (fm.ascent() - fm.descent()) / 2;

    painter.save();
    painter.setFont(label.font.font());
    painter.setPen(QPen(label.font.color));
    for (int i = 0; i < label.text.size(); ++i) {
        const double w = m.advances[i];
        const double half = (w / m.radiusPx) / 2;
        const double at = theta - dir * half;

        const QPointF p(c.x() + m.radiusPx * std::cos(at), c.y() - m.radiusPx * std::sin(at));
        painter.save();
        painter.translate(p);
        // 切線方向：θ 遞減的那一側。Qt 的 rotate 是順時針，所以是 90 − θ。
        painter.rotate(90.0 - at * 180.0 / kPi + (flip ? 180.0 : 0.0));
        painter.drawText(QPointF(-w / 2, baselineY), QString(label.text[i]));
        painter.restore();

        theta -= dir * (w / m.radiusPx);
    }
    painter.restore();
}

QString formatTickLabel(double value, double tickSpacing, double axisMaxAbs) {
    // 0 永遠印成 "0"，不要變成 "0.00" 或 "-0"
    if (value == 0 || std::abs(value) < tickSpacing * 1e-10) return QStringLiteral("0");

    // 用整根軸的量級決定格式，而不是這個值自己的量級
    const double absV = (axisMaxAbs > 0) ? axisMaxAbs : std::abs(value);
    // 太大／太小改用科學記號，否則標籤會長到擠爆版面
    if (absV >= 1e6 || absV < 1e-4) {
        QString s = QString::number(value, 'e', 2);
        // 1.00e+06 → 1e+06，把沒意義的尾零去掉
        s.replace(QRegularExpression(QStringLiteral("\\.?0+e")), QStringLiteral("e"));
        return s;
    }
    // 小數位數由刻度間距決定：間距 0.25 要兩位，間距 5 要零位。
    //
    // 不能用 ceil(-log10(spacing))：那對 0.25 算出 1 位，標籤就會把 0.25 印成
    // 「0.3」—— 刻度明明在 0.25。自動刻度的間距都是 1/2/5 × 10^n 所以看不出來，
    // 但極座標的環半徑這類自訂間距會踩到。改成找出間距本身需要幾位小數。
    int decimals = 0;
    if (tickSpacing > 0 && tickSpacing < 1) {
        for (int d = 1; d <= 10; ++d) {
            const double scaled = tickSpacing * std::pow(10.0, d);
            if (std::abs(scaled - std::round(scaled)) < 1e-6) {
                decimals = d;
                break;
            }
            decimals = d;
        }
    }
    return QString::number(value, 'f', decimals);
}

}  // namespace qtplot

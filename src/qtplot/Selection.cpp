// Selection.cpp — 框選與拖曳的實作（從 Plottables.cpp 拆出來，那個檔已經夠長了）。
#include "Plot.h"
#include "Plottables.h"

namespace qtplot {

namespace {

/// 把 (xs, ys) 裡落在矩形內的點索引挑出來。
QVector<int> indicesInRect(const QVector<double>& xs, const QVector<double>& ys,
                           const CoordinateRect& rect) {
    QVector<int> out;
    const qsizetype n = std::min(xs.size(), ys.size());
    // 使用者拖出來的框方向不定，先正規化再比
    const double x0 = std::min(rect.left, rect.right);
    const double x1 = std::max(rect.left, rect.right);
    const double y0 = std::min(rect.bottom, rect.top);
    const double y1 = std::max(rect.bottom, rect.top);
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        if (xs[i] < x0 || xs[i] > x1 || ys[i] < y0 || ys[i] > y1) continue;
        out.append(static_cast<int>(i));
    }
    return out;
}

Coordinates pointOf(const QVector<double>& xs, const QVector<double>& ys, int index) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (index < 0 || index >= std::min(xs.size(), ys.size())) return {nan, nan};
    return {xs[index], ys[index]};
}

}  // namespace

// ───────────────────────── 框選 ─────────────────────────

QVector<int> Scatter::pointsInRect(const CoordinateRect& rect) const {
    return indicesInRect(displayXs(), displayYs(), rect);
}
Coordinates Scatter::pointAt(int index) const { return pointOf(displayXs(), displayYs(), index); }

QVector<int> Markers::pointsInRect(const CoordinateRect& rect) const {
    return indicesInRect(xs, ys, rect);
}
Coordinates Markers::pointAt(int index) const { return pointOf(xs, ys, index); }

QVector<int> SignalXY::pointsInRect(const CoordinateRect& rect) const {
    QVector<double> dx(xs.size()), dy(ys.size());
    for (qsizetype i = 0; i < xs.size(); ++i) dx[i] = xs[i] + xOffset;
    for (qsizetype i = 0; i < ys.size(); ++i) dy[i] = ys[i] * yScale + yOffset;
    return indicesInRect(dx, dy, rect);
}
Coordinates SignalXY::pointAt(int index) const {
    const Coordinates c = pointOf(xs, ys, index);
    return {c.x + xOffset, c.y * yScale + yOffset};
}

QVector<int> Signal::pointsInRect(const CoordinateRect& rect) const {
    QVector<int> out;
    if (ys.isEmpty() || period == 0) return out;

    const double x0 = std::min(rect.left, rect.right);
    const double x1 = std::max(rect.left, rect.right);
    const double y0 = std::min(rect.bottom, rect.top);
    const double y1 = std::max(rect.bottom, rect.top);

    // X 是算出來的，所以索引範圍可以直接反推，不必掃過全部點
    const double i0f = (x0 - xOffset) / period;
    const double i1f = (x1 - xOffset) / period;
    qsizetype i0 = static_cast<qsizetype>(std::ceil(std::min(i0f, i1f)));
    qsizetype i1 = static_cast<qsizetype>(std::floor(std::max(i0f, i1f)));
    i0 = std::max<qsizetype>(0, i0);
    i1 = std::min<qsizetype>(ys.size() - 1, i1);

    for (qsizetype i = i0; i <= i1; ++i) {
        const double v = ys[i] * yScale + yOffset;
        if (!std::isfinite(v) || v < y0 || v > y1) continue;
        out.append(static_cast<int>(i));
    }
    return out;
}

Coordinates Signal::pointAt(int index) const {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (index < 0 || index >= ys.size()) return {nan, nan};
    return {xOffset + index * period, ys[index] * yScale + yOffset};
}

QVector<HitResult> Plot::pointsInRect(const CoordinateRect& rect) const {
    QVector<HitResult> out;
    for (const auto& p : m_plottables) {
        if (!p->isVisible) continue;
        for (int i : p->pointsInRect(rect)) {
            HitResult r;
            r.index = i;
            r.point = p->pointAt(i);
            r.plottable = p.get();
            out.append(r);
        }
    }
    return out;
}

// ───────────────────────── 拖曳 ─────────────────────────

namespace {

/// 離游標最近、且在容許距離內的點。沒有就回傳 -1。
int grabbedPoint(const QVector<double>& xs, const QVector<double>& ys, Coordinates at,
                 const Transform& t, double tolerancePx) {
    const QPointF p = t.p(at);
    int best = -1;
    double bestD = tolerancePx;
    const qsizetype n = std::min(xs.size(), ys.size());
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        const QPointF q = t.p(xs[i], ys[i]);
        const double d = std::hypot(q.x() - p.x(), q.y() - p.y());
        if (d <= bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

}  // namespace

// ───────────────────────── 拖曳個別的點 ─────────────────────────

bool Scatter::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    return draggablePoints &&
           grabbedPoint(displayXs(), displayYs(), at, t, tolerancePx + marker.size / 2) >= 0;
}

void Scatter::beginDrag(Coordinates at, const Transform& t, double tolerancePx) {
    m_dragIndex = grabbedPoint(displayXs(), displayYs(), at, t, tolerancePx + marker.size / 2);
}

void Scatter::dragBy(Coordinates delta) {
    if (m_dragIndex < 0 || m_dragIndex >= std::min(xs.size(), ys.size())) return;
    // 拖的是畫出來的位置，換回原始資料要除掉倍率
    if (dragX && xScale != 0) xs[m_dragIndex] += delta.x / xScale;
    if (dragY && yScale != 0) ys[m_dragIndex] += delta.y / yScale;
}

bool Markers::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    return draggablePoints && grabbedPoint(xs, ys, at, t, tolerancePx + marker.size / 2) >= 0;
}

void Markers::beginDrag(Coordinates at, const Transform& t, double tolerancePx) {
    m_dragIndex = grabbedPoint(xs, ys, at, t, tolerancePx + marker.size / 2);
}

void Markers::dragBy(Coordinates delta) {
    if (m_dragIndex < 0 || m_dragIndex >= std::min(xs.size(), ys.size())) return;
    if (dragX) xs[m_dragIndex] += delta.x;
    if (dragY) ys[m_dragIndex] += delta.y;
}

// ───────────────────────── 箭頭與線段 ─────────────────────────

void Arrow::beginDrag(Coordinates at, const Transform& t, double tolerancePx) {
    m_dragPart = draggable ? 0 : -1;
    if (!draggableEndpoints) return;
    const QPointF p = t.p(at), a = t.p(tip), b = t.p(base);
    const double dt = std::hypot(p.x() - a.x(), p.y() - a.y());
    const double db = std::hypot(p.x() - b.x(), p.y() - b.y());
    const double tol = tolerancePx + 4;
    if (dt <= tol && dt <= db) m_dragPart = 1;
    else if (db <= tol)        m_dragPart = 2;
}

void Arrow::dragBy(Coordinates delta) {
    if (m_dragPart == 0 || m_dragPart == 1) { tip.x += delta.x;  tip.y += delta.y; }
    if (m_dragPart == 0 || m_dragPart == 2) { base.x += delta.x; base.y += delta.y; }
}

bool Arrow::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    if (draggableEndpoints && !draggable) {
        // 只能拖端點時，抓線段中間不算
        const QPointF p = t.p(at), a = t.p(tip), b = t.p(base);
        const double tol = tolerancePx + 4;
        return std::hypot(p.x() - a.x(), p.y() - a.y()) <= tol ||
               std::hypot(p.x() - b.x(), p.y() - b.y()) <= tol;
    }
    // 點到線段的距離
    const QPointF a = t.p(base);
    const QPointF b = t.p(tip);
    const QPointF p = t.p(at);
    const double dx = b.x() - a.x(), dy = b.y() - a.y();
    const double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) return std::hypot(p.x() - a.x(), p.y() - a.y()) <= tolerancePx;
    // 投影參數夾在 [0,1]，才是「到線段」而不是「到整條直線」的距離
    const double s = std::clamp(((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / len2, 0.0, 1.0);
    const double cx = a.x() + s * dx, cy = a.y() + s * dy;
    return std::hypot(p.x() - cx, p.y() - cy) <= tolerancePx + line.width;
}

namespace {

/// 游標靠近矩形的哪幾個邊（1 左、2 右、4 下、8 上）。
int nearEdges(const CoordinateRect& rect, Coordinates at, const Transform& t, double tol) {
    const QPointF p = t.p(at);
    const double l = t.px(std::min(rect.left, rect.right)), r = t.px(std::max(rect.left, rect.right));
    const double b = t.py(std::min(rect.bottom, rect.top)), tp = t.py(std::max(rect.bottom, rect.top));
    const double top = std::min(b, tp), bottom = std::max(b, tp);
    // 只有在矩形附近才算（不然很遠的地方對齊某條邊也會被當成抓到）
    if (p.x() < l - tol || p.x() > r + tol || p.y() < top - tol || p.y() > bottom + tol) return 0;
    int edges = 0;
    if (std::abs(p.x() - l) <= tol) edges |= 1;
    else if (std::abs(p.x() - r) <= tol) edges |= 2;
    if (std::abs(p.y() - bottom) <= tol) edges |= 4;
    else if (std::abs(p.y() - top) <= tol) edges |= 8;
    return edges;
}

}  // namespace

void Shape::beginDrag(Coordinates at, const Transform& t, double tolerancePx) {
    m_dragEdges = draggable ? 0 : -1;
    if (!resizable || kind == Kind::Polygon || rotationDegrees != 0) return;
    // 先把 rect 正規化，「左」才一定是 left
    if (rect.left > rect.right) std::swap(rect.left, rect.right);
    if (rect.bottom > rect.top) std::swap(rect.bottom, rect.top);
    const int e = nearEdges(rect, at, t, tolerancePx);
    if (e != 0) m_dragEdges = e;
}

void Shape::dragBy(Coordinates delta) {
    if (m_dragEdges < 0) return;
    if (m_dragEdges > 0) {
        if (m_dragEdges & 1) rect.left += delta.x;
        if (m_dragEdges & 2) rect.right += delta.x;
        if (m_dragEdges & 4) rect.bottom += delta.y;
        if (m_dragEdges & 8) rect.top += delta.y;
        return;
    }
    rect.left += delta.x;
    rect.right += delta.x;
    rect.bottom += delta.y;
    rect.top += delta.y;
    for (Coordinates& c : points) {
        c.x += delta.x;
        c.y += delta.y;
    }
}

bool Shape::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    if (resizable && kind != Kind::Polygon && rotationDegrees == 0 &&
        nearEdges(rect, at, t, tolerancePx) != 0)
        return true;
    if (!draggable) return false;
    if (kind == Kind::Polygon) {
        if (points.size() < 3) return false;
        // 射線法：從該點往 +x 射一條線，和多邊形邊界交會奇數次就在裡面
        bool inside = false;
        for (int i = 0, j = static_cast<int>(points.size()) - 1; i < points.size(); j = i++) {
            const Coordinates& a = points[i];
            const Coordinates& b = points[j];
            if ((a.y > at.y) != (b.y > at.y) &&
                at.x < (b.x - a.x) * (at.y - a.y) / (b.y - a.y) + a.x)
                inside = !inside;
        }
        return inside;
    }

    const double x0 = std::min(rect.left, rect.right);
    const double x1 = std::max(rect.left, rect.right);
    const double y0 = std::min(rect.bottom, rect.top);
    const double y1 = std::max(rect.bottom, rect.top);

    if (kind == Kind::Ellipse) {
        const double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        const double rx = (x1 - x0) / 2, ry = (y1 - y0) / 2;
        if (rx <= 0 || ry <= 0) return false;
        const double nx = (at.x - cx) / rx, ny = (at.y - cy) / ry;
        return nx * nx + ny * ny <= 1.0;
    }

    // 矩形：容差換算成資料單位再比，邊緣才抓得到
    const double tolX = std::abs(t.cx(tolerancePx) - t.cx(0));
    const double tolY = std::abs(t.cy(tolerancePx) - t.cy(0));
    return at.x >= x0 - tolX && at.x <= x1 + tolX && at.y >= y0 - tolY && at.y <= y1 + tolY;
}

}  // namespace qtplot

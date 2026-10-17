// Smoothing.cpp — 平滑曲線、部分繪製、垂直方向、形狀旋轉。
//
// 這些都是「畫法」上的變化，和資料本身無關，所以從 Plottables.cpp 拆出來。
#include "Plottables.h"

#include <QPainterPath>
#include <QPolygonF>

namespace qtplot {

namespace {

/// Catmull–Rom 轉成三次貝茲的控制點。
///
/// 端點用重複點代替「前一點／後一點」，曲線在兩端才不會往外甩。
QPainterPath catmullRomPath(const QVector<QPointF>& pts, double tension) {
    QPainterPath path;
    if (pts.size() < 2) return path;
    path.moveTo(pts.first());
    if (pts.size() == 2) {
        path.lineTo(pts.last());
        return path;
    }

    const double t = std::clamp(tension, 0.0, 1.0) / 3.0;
    for (qsizetype i = 0; i < pts.size() - 1; ++i) {
        const QPointF& p0 = pts[std::max<qsizetype>(0, i - 1)];
        const QPointF& p1 = pts[i];
        const QPointF& p2 = pts[i + 1];
        const QPointF& p3 = pts[std::min<qsizetype>(pts.size() - 1, i + 2)];

        const QPointF c1(p1.x() + (p2.x() - p0.x()) * t, p1.y() + (p2.y() - p0.y()) * t);
        const QPointF c2(p2.x() - (p3.x() - p1.x()) * t, p2.y() - (p3.y() - p1.y()) * t);
        path.cubicTo(c1, c2, p2);
    }
    return path;
}

/// 單調三次內插（Fritsch–Carlson）。
///
/// 和 Catmull–Rom 的差別是它保證不會超出原始資料的範圍：
/// 資料從 0 漲到 1 再平下來，Catmull–Rom 會先衝過 1 再回來（overshoot），
/// 看起來像資料真的到過 1.05。量測資料不能這樣畫。
QPainterPath monotonePath(const QVector<QPointF>& pts) {
    QPainterPath path;
    const qsizetype n = pts.size();
    if (n < 2) return path;
    path.moveTo(pts.first());
    if (n == 2) {
        path.lineTo(pts.last());
        return path;
    }

    QVector<double> dx(n - 1), dy(n - 1), slope(n - 1);
    for (qsizetype i = 0; i < n - 1; ++i) {
        dx[i] = pts[i + 1].x() - pts[i].x();
        dy[i] = pts[i + 1].y() - pts[i].y();
        slope[i] = (std::abs(dx[i]) > 1e-12) ? dy[i] / dx[i] : 0.0;
    }

    QVector<double> m(n);
    m[0] = slope[0];
    m[n - 1] = slope[n - 2];
    for (qsizetype i = 1; i < n - 1; ++i) {
        // 斜率變號的地方切線設 0，曲線才不會在轉折處衝過頭
        m[i] = (slope[i - 1] * slope[i] <= 0) ? 0.0 : (slope[i - 1] + slope[i]) / 2;
    }
    // Fritsch–Carlson 的限制條件
    for (qsizetype i = 0; i < n - 1; ++i) {
        if (std::abs(slope[i]) < 1e-12) {
            m[i] = m[i + 1] = 0;
            continue;
        }
        const double a = m[i] / slope[i];
        const double b = m[i + 1] / slope[i];
        const double h = std::hypot(a, b);
        if (h > 3.0) {
            const double k = 3.0 / h;
            m[i] = k * a * slope[i];
            m[i + 1] = k * b * slope[i];
        }
    }

    for (qsizetype i = 0; i < n - 1; ++i) {
        const QPointF c1(pts[i].x() + dx[i] / 3, pts[i].y() + m[i] * dx[i] / 3);
        const QPointF c2(pts[i + 1].x() - dx[i] / 3, pts[i + 1].y() - m[i + 1] * dx[i] / 3);
        path.cubicTo(c1, c2, pts[i + 1]);
    }
    return path;
}

}  // namespace

/// 把一條路徑重新取樣，沿法線方向加上平滑的抖動，看起來像手畫的。
///
/// 抖動用幾個不同頻率的正弦疊起來，相位由路徑起點決定 —— 不用亂數，
/// 所以同一條線每次重畫的樣子都一樣。
QPainterPath handDrawnPath(const QPainterPath& src, double amplitude) {
    const double len = src.length();
    if (!(len > 1) || !(amplitude > 0)) return src;
    const QPointF s0 = src.pointAtPercent(0);
    const double phase = std::fmod(s0.x() * 0.137 + s0.y() * 0.071, 6.283);
    const int n = std::max(2, static_cast<int>(len / 4));
    QPainterPath out;
    for (int i = 0; i <= n; ++i) {
        const double pct = static_cast<double>(i) / n;
        const QPointF pt = src.pointAtPercent(pct);
        const double ang = src.angleAtPercent(pct) * 3.14159265358979 / 180.0;
        // 法線（Qt 的角度逆時針、y 向下）
        const QPointF normal(std::sin(ang), std::cos(ang));
        const double d = pct * len;
        const double w = amplitude * (0.55 * std::sin(d * 0.07 + phase) +
                                      0.3 * std::sin(d * 0.19 + phase * 2.1) +
                                      0.15 * std::sin(d * 0.53 + phase * 3.7));
        const QPointF q = pt + normal * w;
        if (i == 0) out.moveTo(q);
        else        out.lineTo(q);
    }
    return out;
}

/// 依 interpolation 把點序列畫成線。NaN 會把曲線斷開。
void drawInterpolated(QPainter& p, const QVector<QPointF>& pts, const LineStyle& line,
                      Scatter::Interpolation mode, double tension) {
    if (!line.isVisible() || pts.size() < 2) return;
    QPen pen = line.pen();
    if (line.handDrawn > 0) {
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
    }
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    QVector<QPointF> run;
    auto flush = [&]() {
        if (run.size() < 2) {
            run.clear();
            return;
        }
        QPainterPath path;
        switch (mode) {
        case Scatter::Interpolation::Straight:
            path.addPolygon(QPolygonF(run));
            break;
        case Scatter::Interpolation::Smooth:
            path = catmullRomPath(run, tension);
            break;
        case Scatter::Interpolation::SmoothMonotone:
            path = monotonePath(run);
            break;
        }
        if (line.handDrawn > 0) path = handDrawnPath(path, line.handDrawn);
        p.drawPath(path);
        run.clear();
    };

    for (const QPointF& pt : pts) {
        if (std::isfinite(pt.x()) && std::isfinite(pt.y())) run.append(pt);
        else flush();
    }
    flush();
}

/// 把 renderFirst / renderLast 夾成有效的索引範圍。
void clampRenderRange(int first, int last, qsizetype count, qsizetype& outFirst,
                      qsizetype& outLast) {
    outFirst = (first >= 0) ? std::min<qsizetype>(first, count - 1) : 0;
    outLast = (last >= 0) ? std::min<qsizetype>(last, count - 1) : count - 1;
    if (outLast < outFirst) std::swap(outFirst, outLast);
}

// ───────────────────────── Shape 的旋轉 ─────────────────────────

QVector<Coordinates> Shape::outline(int ellipseSegments) const {
    QVector<Coordinates> pts;

    switch (kind) {
    case Kind::Rectangle:
        pts = {{rect.left, rect.bottom},
               {rect.right, rect.bottom},
               {rect.right, rect.top},
               {rect.left, rect.top}};
        break;
    case Kind::Ellipse: {
        const double cx = (rect.left + rect.right) / 2;
        const double cy = (rect.bottom + rect.top) / 2;
        const double rx = (rect.right - rect.left) / 2;
        const double ry = (rect.top - rect.bottom) / 2;
        const int n = std::max(8, ellipseSegments);
        pts.reserve(n);
        for (int i = 0; i < n; ++i) {
            const double a = 2 * kPi * i / n;
            pts.append({cx + rx * std::cos(a), cy + ry * std::sin(a)});
        }
        break;
    }
    case Kind::Polygon:
        pts = points;
        break;
    }

    if (rotationDegrees != 0 && !pts.isEmpty()) {
        // 繞形狀自己的中心轉。用外接框的中心而不是頂點平均 ——
        // 多邊形的頂點分布不均時，平均會讓旋轉中心跑掉。
        CoordinateRange xr, yr;
        for (const Coordinates& c : pts) {
            xr.expand(c.x);
            yr.expand(c.y);
        }
        const double cx = xr.center(), cy = yr.center();
        const double rad = rotationDegrees * kPi / 180.0;
        const double cs = std::cos(rad), sn = std::sin(rad);
        for (Coordinates& c : pts) {
            const double dx = c.x - cx, dy = c.y - cy;
            c = {cx + dx * cs - dy * sn, cy + dx * sn + dy * cs};
        }
    }
    return pts;
}

}  // namespace qtplot

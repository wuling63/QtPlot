#include "Specialty.h"

#include <QFontMetricsF>
#include <QPolygonF>
#include <QSet>

namespace qtplot {

namespace {

/// 畫箭頭（和 Annotations.cpp 的那個分開，這裡要能換顏色）。
void arrowHead(QPainter& p, QPointF tip, QPointF from, double len, double width,
               const QColor& color) {
    const double dx = tip.x() - from.x(), dy = tip.y() - from.y();
    const double n = std::hypot(dx, dy);
    if (n < 1e-9) return;
    const double ux = dx / n, uy = dy / n;
    const QPointF base(tip.x() - ux * len, tip.y() - uy * len);
    const double hw = width / 2;
    QPolygonF head;
    head << tip << QPointF(base.x() - uy * hw, base.y() + ux * hw)
         << QPointF(base.x() + uy * hw, base.y() - ux * hw);
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(color));
    p.drawPolygon(head);
}

/// 把一條圓弧取樣成多邊形（在資料座標上取樣，所以兩軸比例不同也正確）。
QPolygonF arcPolyline(const Transform& t, Coordinates center, double radius, double startDeg,
                      double sweepDeg, int segments = 0) {
    QPolygonF out;
    const int n = (segments > 0) ? segments
                                 : std::clamp(static_cast<int>(std::abs(sweepDeg) / 2), 2, 720);
    out.reserve(n + 1);
    for (int i = 0; i <= n; ++i) {
        const Coordinates c = fromPolar(radius, startDeg + sweepDeg * i / n);
        out.append(t.p(center.x + c.x, center.y + c.y));
    }
    return out;
}

}  // namespace

// ───────────────────────── Delaunay ─────────────────────────

namespace {

/// 點是否落在三角形的外接圓內。回傳 false 也包含「退化三角形」的情況。
bool inCircumcircle(const Coordinates& p, const Coordinates& a, const Coordinates& b,
                    const Coordinates& c) {
    const double ax = a.x - p.x, ay = a.y - p.y;
    const double bx = b.x - p.x, by = b.y - p.y;
    const double cx = c.x - p.x, cy = c.y - p.y;

    // 先確認 a-b-c 是逆時針，不然行列式的正負號會反過來
    const double orient = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::abs(orient) < 1e-18) return false;  // 共線，沒有外接圓

    const double a2 = ax * ax + ay * ay;
    const double b2 = bx * bx + by * by;
    const double c2 = cx * cx + cy * cy;
    const double m0 = bx * cy - by * cx;
    const double m1 = ax * cy - ay * cx;
    const double m2 = ax * by - ay * bx;
    const double det = a2 * m0 - b2 * m1 + c2 * m2;

    // 不能和 0 直接比。點正好落在外接圓「上」是常見的退化情形
    //（例如一組共圓的取樣點），浮點誤差會讓判定在裡外之間搖擺，
    // 挖出來的洞就不是單連通的，補回去會長出互相重疊的三角形。
    // 門檻跟著運算量級縮放，把「幾乎在圓上」一律當成圓外，
    // 這樣退化情形會得到一個任意但一致的三角化。
    const double scale = (std::abs(a2) + std::abs(b2) + std::abs(c2)) *
                         (std::abs(m0) + std::abs(m1) + std::abs(m2));
    const double eps = 1e-12 * scale;
    return (orient > 0) ? (det > eps) : (det < -eps);
}

/// 無向邊，用來找「洞」的邊界。
struct TriEdge {  // 不能叫 Edge —— 那是軸的方位列舉 qtplot::Edge
    int u = 0;
    int v = 0;
    TriEdge(int a, int b) : u(std::min(a, b)), v(std::max(a, b)) {}
    bool operator==(const TriEdge& o) const { return u == o.u && v == o.v; }
};

size_t qHash(const TriEdge& e, size_t seed = 0) {
    return ::qHash(e.u, seed) ^ (::qHash(e.v, seed) << 1);
}

}  // namespace

QVector<Triangle> delaunay(const QVector<Coordinates>& points) {
    QVector<Triangle> out;
    const int n = static_cast<int>(points.size());
    if (n < 3) return out;

    // 共線的點沒有唯一的三角化，先擋掉
    {
        bool collinear = true;
        for (int i = 2; i < n && collinear; ++i) {
            const double cross = (points[1].x - points[0].x) * (points[i].y - points[0].y) -
                                 (points[1].y - points[0].y) * (points[i].x - points[0].x);
            if (std::abs(cross) > 1e-12) collinear = false;
        }
        if (collinear) return out;
    }

    // 超級三角形：大到把所有點包起來，最後再把碰到它的三角形丟掉
    CoordinateRange xr, yr;
    for (const Coordinates& p : points) {
        if (!p.isFinite()) continue;
        xr.expand(p.x);
        yr.expand(p.y);
    }
    if (!xr.isValid() || !yr.isValid()) return out;
    const double dMax = std::max(xr.span(), yr.span()) * 10 + 1;
    const double cx = xr.center(), cy = yr.center();

    QVector<Coordinates> pts = points;
    const int s0 = n, s1 = n + 1, s2 = n + 2;
    pts.append({cx - dMax, cy - dMax});
    pts.append({cx + dMax, cy - dMax});
    pts.append({cx, cy + dMax});

    QVector<Triangle> tris{{s0, s1, s2}};

    for (int i = 0; i < n; ++i) {
        if (!pts[i].isFinite()) continue;

        // 找出外接圓包含這個點的三角形，把它們拆掉
        QVector<Triangle> bad;
        QVector<Triangle> good;
        good.reserve(tris.size());
        for (const Triangle& t : tris) {
            if (inCircumcircle(pts[i], pts[t.a], pts[t.b], pts[t.c])) bad.append(t);
            else good.append(t);
        }
        if (bad.isEmpty()) continue;

        // 洞的邊界 = 只出現一次的邊
        QHash<TriEdge, int> edgeCount;
        for (const Triangle& t : bad) {
            edgeCount[TriEdge(t.a, t.b)]++;
            edgeCount[TriEdge(t.b, t.c)]++;
            edgeCount[TriEdge(t.c, t.a)]++;
        }
        tris = good;
        for (auto it = edgeCount.constBegin(); it != edgeCount.constEnd(); ++it) {
            if (it.value() != 1) continue;
            tris.append({it.key().u, it.key().v, i});
        }
    }

    // 丟掉用到超級三角形頂點的那些
    out.reserve(tris.size());
    for (const Triangle& t : tris) {
        if (t.a >= n || t.b >= n || t.c >= n) continue;
        out.append(t);
    }

    // 正規化並排序，輸出才是確定的。
    //
    // 洞的邊界是從 QHash 走訪出來的，而 Qt 的 QHash 每個行程用不同的亂數種子，
    // 走訪順序會變。三角形的「集合」不受影響，但順序會變 —— 反鋸齒把後畫的邊
    // 疊在先畫的上面，同一支程式跑兩次就會產生色階差 1 的 PNG。
    // 這會讓「匯出兩次比對雜湊」的回歸檢查每次都誤報。
    for (Triangle& t : out) {
        int v[3] = {t.a, t.b, t.c};
        std::sort(v, v + 3);
        t = {v[0], v[1], v[2]};
    }
    std::sort(out.begin(), out.end(), [](const Triangle& x, const Triangle& y) {
        if (x.a != y.a) return x.a < y.a;
        if (x.b != y.b) return x.b < y.b;
        return x.c < y.c;
    });
    return out;
}

// ───────────────────────────── TIN ─────────────────────────────

const QVector<Triangle>& TIN::triangles() const {
    if (!m_cached) {
        m_triangles = delaunay(points);
        m_cached = true;
    }
    return m_triangles;
}

AxisLimits TIN::axisLimits() const {
    AxisLimits lim;
    for (const Coordinates& c : points) lim.expand(c);
    return lim;
}

void TIN::render(RenderPack& rp) {
    const QVector<Triangle>& tris = triangles();
    if (tris.isEmpty() || points.size() != zs.size()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    CoordinateRange cr = colorRange;
    if (!cr.isValid()) {
        for (double z : zs)
            if (std::isfinite(z)) cr.expand(z);
    }

    for (const Triangle& tri : tris) {
        QPolygonF poly;
        poly << t.p(points[tri.a]) << t.p(points[tri.b]) << t.p(points[tri.c]);

        if (filled) {
            const double z = (zs[tri.a] + zs[tri.b] + zs[tri.c]) / 3;
            p.setBrush(QBrush(colormap.lookup(z, cr.min, cr.max)));
        } else {
            p.setBrush(Qt::NoBrush);
        }
        // 填色時也畫同色的邊，相鄰三角形之間才不會露出一條條反鋸齒的縫
        p.setPen(wireframe.isVisible() ? wireframe.pen()
                                       : (filled ? QPen(p.brush().color(), 0.5) : QPen(Qt::NoPen)));
        p.drawPolygon(poly);
    }

    if (marker.isVisible()) {
        QVector<QPointF> pts;
        pts.reserve(points.size());
        for (const Coordinates& c : points)
            if (c.isFinite()) pts.append(t.p(c));
        drawMarkers(p, pts, marker);
    }
    p.restore();
}

// ───────────────────────── TriangularAxis ─────────────────────────

Coordinates TriangularAxis::toCartesian(double a, double b, double c) {
    const double sum = a + b + c;
    if (!(sum > 0)) return {0, 0};
    const double nb = b / sum, nc = c / sum;
    // 底邊 (0,0)–(1,0)，頂點 (0.5, √3/2)
    return {nb + nc / 2, nc * std::sqrt(3.0) / 2};
}

AxisLimits TriangularAxis::axisLimits() const {
    // 留白給角落的標籤
    return AxisLimits(-0.12, 1.12, -0.12, std::sqrt(3.0) / 2 + 0.12);
}

void TriangularAxis::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const Coordinates c0 = toCartesian(1, 0, 0);  // 下左
    const Coordinates c1 = toCartesian(0, 1, 0);  // 下右
    const Coordinates c2 = toCartesian(0, 0, 1);  // 上

    if (background.isVisible()) {
        QPolygonF tri;
        tri << t.p(c0) << t.p(c1) << t.p(c2);
        p.setPen(Qt::NoPen);
        p.setBrush(background.brush(tri.boundingRect()));
        p.drawPolygon(tri);
    }

    // 內部格線：三個方向各 gridCount-1 條
    if (gridLine.isVisible() && gridCount > 1) {
        p.setPen(gridLine.pen());
        for (int i = 1; i < gridCount; ++i) {
            const double f = static_cast<double>(i) / gridCount;
            // 固定 a = f 的線、固定 b = f 的線、固定 c = f 的線
            p.drawLine(t.p(toCartesian(f, 1 - f, 0)), t.p(toCartesian(f, 0, 1 - f)));
            p.drawLine(t.p(toCartesian(1 - f, f, 0)), t.p(toCartesian(0, f, 1 - f)));
            p.drawLine(t.p(toCartesian(1 - f, 0, f)), t.p(toCartesian(0, 1 - f, f)));
        }
    }

    if (edgeStyles.isEmpty()) {
        if (edgeLine.isVisible()) {
            p.setPen(edgeLine.pen());
            p.setBrush(Qt::NoBrush);
            QPolygonF tri;
            tri << t.p(c0) << t.p(c1) << t.p(c2);
            p.drawPolygon(tri);
        }
    } else {
        const Coordinates ends[3][2] = {{c0, c1}, {c1, c2}, {c2, c0}};
        for (int e = 0; e < 3; ++e) {
            const LineStyle& ls = e < edgeStyles.size() ? edgeStyles[e] : edgeLine;
            if (!ls.isVisible()) continue;
            p.setPen(ls.pen());
            p.drawLine(t.p(ends[e][0]), t.p(ends[e][1]));
        }
    }

    // 邊的標題：沿著邊寫，往三角形外面推開（讓出刻度標籤的位置）
    if (!edgeTitles.isEmpty() && edgeTitleFont.color.alpha() > 0) {
        const Coordinates mids[3] = {{(c0.x + c1.x) / 2, (c0.y + c1.y) / 2},
                                     {(c1.x + c2.x) / 2, (c1.y + c2.y) / 2},
                                     {(c2.x + c0.x) / 2, (c2.y + c0.y) / 2}};
        const QPointF outward[3] = {QPointF(0, 1), QPointF(0.866, -0.5), QPointF(-0.866, -0.5)};
        const double rotations[3] = {0, 60, -60};
        QFontMetricsF fm(labelFont.font());
        const double push = fm.height() + 14;
        for (int e = 0; e < 3 && e < edgeTitles.size(); ++e) {
            if (edgeTitles[e].isEmpty()) continue;
            Label lb;
            lb.text = edgeTitles[e];
            lb.font = edgeTitleFont;
            lb.font.rotation = rotations[e];
            drawText(p, t.p(mids[e]) + outward[e] * push, lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }

    if (labelFont.color.alpha() > 0) {
        auto put = [&](Coordinates at, const QString& text, Qt::Alignment al, QPointF offset) {
            if (text.isEmpty()) return;
            Label lb;
            lb.text = text;
            lb.font = labelFont;
            drawText(p, t.p(at) + offset, lb, al);
        };
        put(c0, cornerLabels.value(0), Qt::AlignRight | Qt::AlignTop, QPointF(-4, 6));
        put(c1, cornerLabels.value(1), Qt::AlignLeft | Qt::AlignTop, QPointF(4, 6));
        put(c2, cornerLabels.value(2), Qt::AlignHCenter | Qt::AlignBottom, QPointF(0, -6));

        if (showTickLabels && gridCount > 1) {
            for (int i = 1; i < gridCount; ++i) {
                const double f = static_cast<double>(i) / gridCount;
                Label lb;
                lb.font = labelFont;
                lb.font.size = labelFont.size * 0.85;
                lb.text = QString::number(f * 100, 'g', 3);
                // 底邊：逆時針讀 B（往右遞增）、順時針讀 A（往左遞增）
                const Coordinates bottom = clockwise ? toCartesian(f, 1 - f, 0) : toCartesian(1 - f, f, 0);
                drawText(p, t.p(bottom) + QPointF(0, 4), lb, Qt::AlignHCenter | Qt::AlignTop);
                if (!tickLabelsOnAllEdges) continue;
                // 右邊：逆時針讀 C、順時針讀 B；左邊：逆時針讀 A、順時針讀 C
                const Coordinates right = clockwise ? toCartesian(0, f, 1 - f) : toCartesian(0, 1 - f, f);
                const Coordinates left = clockwise ? toCartesian(1 - f, 0, f) : toCartesian(f, 0, 1 - f);
                drawText(p, t.p(right) + QPointF(5, 0), lb, Qt::AlignLeft | Qt::AlignVCenter);
                drawText(p, t.p(left) + QPointF(-5, 0), lb, Qt::AlignRight | Qt::AlignVCenter);
            }
        }
    }
    p.restore();
}

// ───────────────────────── SmithChartAxis ─────────────────────────

Coordinates SmithChartAxis::toCartesian(double r, double x) {
    // Γ = (z - 1) / (z + 1)，z = r + jx
    const double denom = (r + 1) * (r + 1) + x * x;
    if (!(denom > 0)) return {0, 0};
    return {(r * r - 1 + x * x) / denom, 2 * x / denom};
}

Coordinates SmithChartAxis::coordinates(double r, double x) const {
    const Coordinates g = toCartesian(r, x);
    return {center.x + g.x * radius, center.y + g.y * radius};
}

AxisLimits SmithChartAxis::axisLimits() const {
    // 多留一成給圓周外側的電抗標籤
    const double rr = radius * 1.14;
    return AxisLimits(center.x - rr, center.x + rr, center.y - rr, center.y + rr);
}

void SmithChartAxis::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setBrush(Qt::NoBrush);

    /// 把反射係數平面上的圓換算成圖上的圓（套用 center / radius）。
    auto mapped = [&](Coordinates c, double rad) {
        return QPair<Coordinates, double>({center.x + c.x * radius, center.y + c.y * radius},
                                          rad * radius);
    };

    /// 畫一條等電阻圓。圓心 (r/(1+r), 0)、半徑 1/(1+r)，整個都在單位圓內。
    auto drawResistance = [&](double r) {
        if (r < 0) return;
        const auto m = mapped({r / (1 + r), 0}, 1.0 / (1 + r));
        p.drawPolyline(arcPolyline(t, m.first, m.second, 0, 360, 180));
    };

    /// 畫一條等電抗弧（單一正負號）。圓心 (1, 1/x)、半徑 |1/x|，裁到單位圓之內。
    auto drawReactance = [&](double xx) {
        if (std::abs(xx) < 1e-12) return;
        const Coordinates gc{1.0, 1.0 / xx};
        const auto m = mapped(gc, std::abs(1.0 / xx));
        // 與單位圓的交點就是 z = 0（Γ = -1 那一側）與開路點 Γ = 1。
        // 直接算兩個端點的角度再畫弧，比畫整圈再裁切乾淨。
        const Coordinates pEnd = toCartesian(0, xx);
        const Coordinates pStart{1.0, 0.0};
        const double a0 = std::atan2(pStart.y - gc.y, pStart.x - gc.x) * 180 / kPi;
        const double a1 = std::atan2(pEnd.y - gc.y, pEnd.x - gc.x) * 180 / kPi;
        double sweep = a1 - a0;
        // 取短弧：長弧會繞到單位圓外面
        while (sweep > 180) sweep -= 360;
        while (sweep < -180) sweep += 360;
        p.drawPolyline(arcPolyline(t, m.first, m.second, a0, sweep));
    };

    // 細格線先畫，主格線才蓋在上面
    if (minorGridLine.isVisible()) {
        p.setPen(minorGridLine.pen());
        for (double r : minorResistances) drawResistance(r);
        for (double x : minorReactances) {
            drawReactance(x);
            drawReactance(-x);
        }
    }

    if (gridLine.isVisible()) {
        p.setPen(gridLine.pen());
        for (double r : resistances) drawResistance(r);
        for (double x : reactances) {
            drawReactance(x);
            drawReactance(-x);
        }
        // 實軸（x = 0 那條等電抗「弧」退化成直線）
        p.drawLine(t.p(coordinates(0, 0)), t.p(center.x + radius, center.y));
    }

    // 等駐波比圓：以圓心為心、|Γ| 為半徑
    if (vswrLine.isVisible() && !vswrCircles.isEmpty()) {
        p.setPen(vswrLine.pen());
        for (double swr : vswrCircles) {
            if (!(swr > 1)) continue;  // VSWR < 1 沒有物理意義
            const double gamma = (swr - 1) / (swr + 1);
            p.drawPolyline(arcPolyline(t, center, gamma * radius, 0, 360, 180));
        }
    }

    if (outerLine.isVisible()) {
        p.setPen(outerLine.pen());
        p.drawPolyline(arcPolyline(t, center, radius, 0, 360, 240));
    }

    if (showLabels && labelFont.color.alpha() > 0) {
        for (double r : resistances) {
            if (r <= 0) continue;
            Label lb;
            lb.text = QString::number(r, 'g', 3);
            lb.font = labelFont;
            // 標在實軸上該等電阻圓與實軸的交點
            drawText(p, t.p(coordinates(r, 0)) + QPointF(0, 3), lb,
                     Qt::AlignHCenter | Qt::AlignTop);
        }
    }

    if (showReactanceLabels && labelFont.color.alpha() > 0) {
        for (double x : reactances) {
            for (double sign : {1.0, -1.0}) {
                const double xx = x * sign;
                // 等電抗弧與單位圓的交點就是 z = jx 那一點
                const Coordinates g = toCartesian(0, xx);
                Label lb;
                lb.text = (xx > 0) ? QStringLiteral("+j%1").arg(xx, 0, 'g', 3)
                                   : QStringLiteral("-j%1").arg(-xx, 0, 'g', 3);
                lb.font = labelFont;
                // 沿著半徑往外推一點，文字才不會壓在圓周上
                const QPointF anchor = t.p(coordinates(0, xx));
                const QPointF outward(g.x, -g.y);  // 像素空間的 y 是往下的
                drawText(p, anchor + outward * 12, lb, Qt::AlignHCenter | Qt::AlignVCenter);
            }
        }
    }
    p.restore();
}

// ───────────────────────── PhasorPlot ─────────────────────────

AxisLimits PhasorPlot::axisLimits() const {
    double maxMag = 0;
    for (const Phasor& ph : phasors) maxMag = std::max(maxMag, std::abs(ph.magnitude));
    if (!(maxMag > 0)) return AxisLimits();
    const double m = maxMag * 1.2;
    return AxisLimits(-m, m, -m, m);
}

void PhasorPlot::render(RenderPack& rp) {
    if (phasors.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QPointF origin = t.p(0, 0);

    for (const Phasor& ph : phasors) {
        const Coordinates c = ph.toCartesian();
        const QPointF tip = t.p(c);
        const QColor color = ph.color.isValid() ? ph.color : QColor(Qt::darkGray);

        if (showProjections && projectionLine.isVisible()) {
            p.setPen(projectionLine.pen());
            p.setBrush(Qt::NoBrush);
            p.drawLine(tip, t.p(c.x, 0));
            p.drawLine(tip, t.p(0, c.y));
        }

        // 線畫到箭頭底部，粗線才不會從尖端露出來
        const double dx = tip.x() - origin.x(), dy = tip.y() - origin.y();
        const double n = std::hypot(dx, dy);
        LineStyle ls(color, lineWidth);
        p.setPen(ls.pen());
        p.setBrush(Qt::NoBrush);
        if (n > headLength) {
            p.drawLine(origin, QPointF(tip.x() - dx / n * headLength, tip.y() - dy / n * headLength));
            arrowHead(p, tip, origin, headLength, headWidth, color);
        } else if (n > 0.5) {
            p.drawLine(origin, tip);
        }

        if (showLabels && !ph.label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = ph.label;
            lb.font = labelFont;
            lb.font.color = color;
            // 沿著相量方向再往外推一點，標籤才不會壓在箭頭上
            const Coordinates out = fromPolar(ph.magnitude * 1.08, ph.angleDegrees);
            drawText(p, t.p(out), lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> PhasorPlot::legendItems() const {
    QVector<LegendItem> out;
    for (const Phasor& ph : phasors) {
        if (ph.label.isEmpty()) continue;
        LegendItem it;
        it.text = ph.label;
        it.line = LineStyle(ph.color.isValid() ? ph.color : QColor(Qt::darkGray), lineWidth);
        out.append(it);
    }
    return out;
}

}  // namespace qtplot

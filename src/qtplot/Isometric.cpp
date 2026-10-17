#include "Isometric.h"

#include <QPolygonF>

#include <algorithm>

namespace qtplot {

Coordinates IsoProjection::project(double x, double y, double z) const {
    const double a = angleDegrees * kPi / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    return {origin.x + (x - y) * c, origin.y - (x + y) * s + z * heightScale};
}

namespace {

/// 畫一個四邊形。
void fillQuad(QPainter& p, const Transform& t, const QVector<Coordinates>& pts,
              const QColor& color, const LineStyle& edge) {
    QPolygonF poly;
    poly.reserve(pts.size());
    for (const Coordinates& c : pts) poly.append(t.p(c));
    p.setBrush(QBrush(color));
    p.setPen(edge.isVisible() ? edge.pen() : QPen(Qt::NoPen));
    p.drawPolygon(poly);
}

}  // namespace

// ───────────────────────── IsoBars ─────────────────────────

QVector<IsoBar> IsoBars::fromValues(const QVector<double>& values, const Palette& palette,
                                    double y) {
    QVector<IsoBar> out;
    out.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        IsoBar b;
        b.x = i;
        b.y = y;
        b.height = values[i];
        b.color = palette.color(i);
        out.append(b);
    }
    return out;
}

AxisLimits IsoBars::axisLimits() const {
    AxisLimits lim;
    if (bars.isEmpty()) return lim;

    // 八個角投影之後都要含進來。只取柱頂或柱底都會漏掉一側 ——
    // 等角投影下「最左」和「最右」分別來自不同的角。
    for (const IsoBar& b : bars) {
        for (double dx : {-b.sizeX / 2, b.sizeX / 2})
            for (double dy : {-b.sizeY / 2, b.sizeY / 2})
                for (double z : {0.0, b.height})
                    lim.expand(projection.project(b.x + dx, b.y + dy, z));
    }
    return lim;
}

void IsoBars::render(RenderPack& rp) {
    if (bars.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // ── 地面 ──
    if (showGround || groundFill.isVisible()) {
        CoordinateRange xr, yr;
        for (const IsoBar& b : bars) {
            xr.expand(b.x - b.sizeX / 2);
            xr.expand(b.x + b.sizeX / 2);
            yr.expand(b.y - b.sizeY / 2);
            yr.expand(b.y + b.sizeY / 2);
        }
        const double pad = 0.35;
        const double x0 = xr.min - pad, x1 = xr.max + pad;
        const double y0 = yr.min - pad, y1 = yr.max + pad;

        if (groundFill.isVisible()) {
            fillQuad(p, t,
                     {projection.project(x0, y0, 0), projection.project(x1, y0, 0),
                      projection.project(x1, y1, 0), projection.project(x0, y1, 0)},
                     groundFill.color, LineStyle::none());
        }
        if (showGround && groundLine.isVisible()) {
            p.setPen(groundLine.pen());
            p.setBrush(Qt::NoBrush);
            // 沿兩個方向各畫一組平行線，交出等角的菱形格
            for (double x = std::ceil(x0); x <= x1; x += 1.0)
                p.drawLine(t.p(projection.project(x, y0, 0)),
                           t.p(projection.project(x, y1, 0)));
            for (double y = std::ceil(y0); y <= y1; y += 1.0)
                p.drawLine(t.p(projection.project(x0, y, 0)),
                           t.p(projection.project(x1, y, 0)));
        }
    }

    // ── 柱子：由後往前畫 ──
    //
    // x + y 愈大代表愈靠近觀眾。照資料順序畫的話，後排的柱子會蓋住前排，
    // 立體感整個反過來。
    QVector<const IsoBar*> order;
    order.reserve(bars.size());
    for (const IsoBar& b : bars) order.append(&b);
    std::stable_sort(order.begin(), order.end(), [](const IsoBar* a, const IsoBar* b) {
        return IsoProjection::depth(a->x, a->y) < IsoProjection::depth(b->x, b->y);
    });

    for (const IsoBar* bp : order) {
        const IsoBar& b = *bp;
        if (!std::isfinite(b.height)) continue;
        const QColor base = b.color.isValid() ? b.color : QColor(90, 150, 210);
        const double hx = b.sizeX / 2, hy = b.sizeY / 2;
        const double h = b.height;

        // 看得到的是 +x 那一面（右）、+y 那一面（左）、以及頂面。
        // 另外三面被柱體自己擋住，不必畫。
        fillQuad(p, t,
                 {projection.project(b.x - hx, b.y + hy, 0),
                  projection.project(b.x + hx, b.y + hy, 0),
                  projection.project(b.x + hx, b.y + hy, h),
                  projection.project(b.x - hx, b.y + hy, h)},
                 base.darker(std::max(100, leftDarken)), edgeLine);

        fillQuad(p, t,
                 {projection.project(b.x + hx, b.y - hy, 0),
                  projection.project(b.x + hx, b.y + hy, 0),
                  projection.project(b.x + hx, b.y + hy, h),
                  projection.project(b.x + hx, b.y - hy, h)},
                 base.darker(std::max(100, rightDarken)), edgeLine);

        fillQuad(p, t,
                 {projection.project(b.x - hx, b.y - hy, h),
                  projection.project(b.x + hx, b.y - hy, h),
                  projection.project(b.x + hx, b.y + hy, h),
                  projection.project(b.x - hx, b.y + hy, h)},
                 base.lighter(std::max(100, topLighten)), edgeLine);

        if (!b.label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = b.label;
            lb.font = labelFont;
            drawText(p, t.p(projection.project(b.x, b.y, h)) + QPointF(0, -6), lb,
                     Qt::AlignHCenter | Qt::AlignBottom);
        }
    }
    p.restore();
}

QVector<LegendItem> IsoBars::legendItems() const {
    QVector<LegendItem> items;
    for (const IsoBar& b : bars) {
        if (b.label.isEmpty()) continue;
        LegendItem it;
        it.text = b.label;
        it.fill = FillStyle(b.color.isValid() ? b.color : QColor(90, 150, 210));
        items.append(it);
    }
    return items;
}

// ───────────────────────── IsoStack ─────────────────────────

double IsoStack::totalHeight() const {
    double sum = 0;
    for (const IsoLayer& l : layers)
        if (std::isfinite(l.height) && l.height > 0) sum += l.height;
    return sum;
}

AxisLimits IsoStack::axisLimits() const {
    AxisLimits lim;
    if (layers.isEmpty()) return lim;

    double maxR = 0;
    for (const IsoLayer& l : layers) maxR = std::max({maxR, l.bottomRadius, l.topRadius});
    const double h = totalHeight();
    // 底面的外接圓，上下兩端各取一次
    for (int i = 0; i < 16; ++i) {
        const double a = 2 * kPi * i / 16;
        const double dx = maxR * std::cos(a), dy = maxR * std::sin(a);
        lim.expand(projection.project(x + dx, y + dy, 0));
        lim.expand(projection.project(x + dx, y + dy, h));
    }
    return lim;
}

void IsoStack::render(RenderPack& rp) {
    if (layers.isEmpty()) return;
    const int n = std::max(3, sides);
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double rot = rotationDegrees * kPi / 180.0;
    auto ringPoint = [&](double radius, int i, double z) {
        const double a = rot + 2 * kPi * i / n;
        return projection.project(x + radius * std::cos(a), y + radius * std::sin(a), z);
    };

    double z0 = 0;
    for (const IsoLayer& layer : layers) {
        if (!std::isfinite(layer.height) || layer.height <= 0) continue;
        const double z1 = z0 + layer.height;
        const QColor base = layer.color.isValid() ? layer.color : QColor(90, 150, 210);

        // 側面拆成 n 個四邊形，依深度由後往前畫，再按法線方向上明暗。
        //
        // 不這樣做的話只有兩個選擇：整個側面同一個色（看起來是平的），
        // 或是自己去算輪廓線（圓錐、平截頭的輪廓不是多邊形的頂點，很麻煩）。
        struct Facet {
            int i;
            double depth;
        };
        QVector<Facet> facets;
        facets.reserve(n);
        for (int i = 0; i < n; ++i) {
            const double a = rot + 2 * kPi * (i + 0.5) / n;
            // 這一片的中心在世界座標的 x + y，就是它的深度
            facets.append({i, IsoProjection::depth(x + std::cos(a), y + std::sin(a))});
        }
        std::stable_sort(facets.begin(), facets.end(),
                         [](const Facet& a, const Facet& b) { return a.depth < b.depth; });

        const double lightRad = lightAngleDegrees * kPi / 180.0;
        const double lx = std::cos(lightRad), ly = std::sin(lightRad);
        for (const Facet& f : facets) {
            const double a = rot + 2 * kPi * (f.i + 0.5) / n;
            // 法線和光線方向的內積決定明暗（Lambert）
            const double lambert = std::cos(a) * lx + std::sin(a) * ly;
            const double k = (lambert + 1) / 2;  // 0..1
            const int factor = static_cast<int>(minLighten + (maxLighten - minLighten) * k);
            const QColor c = (factor >= 100) ? base.lighter(factor) : base.darker(10000 / factor);

            // 沒有指定邊線時，用「和這一片同色」的細筆描邊。
            // 不描的話，相鄰兩片之間會留下抗鋸齒造成的細白縫 ——
            // 圓柱上會看到一排豎線，像是有稜角。
            LineStyle seam = edgeLine;
            if (!seam.isVisible()) {
                seam.color = c;
                seam.width = 0.8;
            }
            fillQuad(p, t,
                     {ringPoint(layer.bottomRadius, f.i, z0),
                      ringPoint(layer.bottomRadius, f.i + 1, z0),
                      ringPoint(layer.topRadius, f.i + 1, z1),
                      ringPoint(layer.topRadius, f.i, z1)},
                     c, seam);
        }

        // 頂面
        if (layer.topRadius > 0) {
            QVector<Coordinates> top;
            top.reserve(n);
            for (int i = 0; i < n; ++i) top.append(ringPoint(layer.topRadius, i, z1));
            fillQuad(p, t, top, base.lighter(std::max(100, topLighten)), edgeLine);
        }

        if (showLabels && !layer.label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = layer.label;
            lb.font = labelFont;
            // 標在這一層的右側中段
            const Coordinates at =
                projection.project(x + layer.bottomRadius, y, (z0 + z1) / 2);
            drawText(p, t.p(at) + QPointF(labelGapPx, 0), lb,
                     Qt::AlignLeft | Qt::AlignVCenter);
        }
        z0 = z1;
    }
    p.restore();
}

QVector<LegendItem> IsoStack::legendItems() const {
    QVector<LegendItem> items;
    for (const IsoLayer& l : layers) {
        if (l.label.isEmpty()) continue;
        LegendItem it;
        it.text = l.label;
        it.fill = FillStyle(l.color.isValid() ? l.color : QColor(90, 150, 210));
        items.append(it);
    }
    return items;
}


// ───────────────────────── 立體曲面 ─────────────────────────

CoordinateRange Surface3D::zRange() const {
    CoordinateRange r;
    for (double v : values)
        if (std::isfinite(v)) r.expand(v);
    return r;
}

CoordinateRange Surface3D::effectiveColorRange() const {
    if (colorRange.isValid()) return colorRange;
    CoordinateRange r = zRange();
    r.ensureArea();
    return r;
}

namespace {

/// 格點的世界座標。
Coordinates gridPoint(const Surface3D& s, int row, int col) {
    const double fx = (s.cols > 1) ? static_cast<double>(col) / (s.cols - 1) : 0.0;
    const double fy = (s.rows > 1) ? static_cast<double>(row) / (s.rows - 1) : 0.0;
    return {s.xRange.min + fx * s.xRange.span(), s.yRange.min + fy * s.yRange.span()};
}

}  // namespace

AxisLimits Surface3D::axisLimits() const {
    AxisLimits lim;
    if (rows < 2 || cols < 2 || values.size() < static_cast<qsizetype>(rows) * cols) return lim;
    // 四個角加上最高與最低點都投一次就夠了 —— 投影是線性的，
    // 極值一定落在這幾個之中。
    const CoordinateRange zr = zRange();
    for (int r : {0, rows - 1})
        for (int c : {0, cols - 1})
            for (double z : {zr.isValid() ? zr.min : 0.0, zr.isValid() ? zr.max : 0.0}) {
                const Coordinates g = gridPoint(*this, r, c);
                lim.expand(projection.project(g.x, g.y, z));
            }
    return lim;
}

void Surface3D::render(RenderPack& rp) {
    if (rows < 2 || cols < 2 ||
        values.size() < static_cast<qsizetype>(rows) * cols)
        return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const CoordinateRange range = effectiveColorRange();
    const double lightRad = lightAngleDegrees * kPi / 180.0;
    const QPointF lightDir(std::cos(lightRad), std::sin(lightRad));

    auto screen = [&](int row, int col) {
        const Coordinates g = gridPoint(*this, row, col);
        const double z = value(row, col);
        return t.p(projection.project(g.x, g.y, std::isfinite(z) ? z : 0.0));
    };

    if (mode == Mode::Points) {
        QVector<QPointF> pts;
        pts.reserve(static_cast<qsizetype>(rows) * cols);
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (std::isfinite(value(r, c))) pts.append(screen(r, c));
        MarkerStyle m = pointMarker;
        m.size *= rp.scaleFactor;
        m.outline.width *= rp.scaleFactor;
        drawMarkers(p, pts, m);
        p.restore();
        return;
    }

    // 由遠而近畫。深度是 x + y，而格座標和世界座標同向，
    // 所以 (row + col) 大的後畫就對了。
    struct Quad { double depth; int row; int col; };
    QVector<Quad> quads;
    quads.reserve(static_cast<qsizetype>(rows - 1) * (cols - 1));
    for (int r = 0; r + 1 < rows; ++r)
        for (int c = 0; c + 1 < cols; ++c)
            quads.append({static_cast<double>(r + c), r, c});
    std::sort(quads.begin(), quads.end(), [](const Quad& a, const Quad& b) {
        if (a.depth != b.depth) return a.depth < b.depth;
        // 深度相同時照索引，畫出來的順序才不會隨實作而變
        if (a.row != b.row) return a.row < b.row;
        return a.col < b.col;
    });

    QPen meshPen = mesh.isVisible() ? mesh.pen() : QPen(Qt::NoPen);
    meshPen.setWidthF(meshPen.widthF() * rp.scaleFactor);

    for (const Quad& q : quads) {
        const double z00 = value(q.row, q.col);
        const double z01 = value(q.row, q.col + 1);
        const double z10 = value(q.row + 1, q.col);
        const double z11 = value(q.row + 1, q.col + 1);
        if (!std::isfinite(z00) || !std::isfinite(z01) || !std::isfinite(z10) ||
            !std::isfinite(z11))
            continue;

        QPolygonF poly;
        poly << screen(q.row, q.col) << screen(q.row, q.col + 1)
             << screen(q.row + 1, q.col + 1) << screen(q.row + 1, q.col);

        if (mode == Mode::Surface) {
            const double zMid = (z00 + z01 + z10 + z11) / 4.0;
            QColor fill = colormap.lookup(zMid, range.min, range.max);

            if (shading > 0) {
                // 用投影後的梯度當法線的代理：面朝光的那一側提亮、背光的壓暗。
                // 真正的法線要在世界座標算，但等角投影是線性的，
                // 畫面上的梯度方向和世界法線的水平分量一致，對打光來說夠了。
                const double dzdx = (z01 + z11 - z00 - z10) / 2.0;
                const double dzdy = (z10 + z11 - z00 - z01) / 2.0;
                const double len = std::hypot(dzdx, dzdy);
                double lambert = 0;
                if (len > 1e-12)
                    lambert = (dzdx * lightDir.x() + dzdy * lightDir.y()) / len;
                const double factor = 1.0 + shading * std::clamp(lambert, -1.0, 1.0);
                fill = (factor >= 1.0) ? fill.lighter(static_cast<int>(factor * 100))
                                       : fill.darker(static_cast<int>(100 / factor));
            }
            p.setBrush(QBrush(fill));
            p.setPen(meshPen);
            p.drawPolygon(poly);
        } else {
            p.setBrush(Qt::NoBrush);
            p.setPen(meshPen);
            p.drawPolygon(poly);
        }
    }
    p.restore();
}

}  // namespace qtplot

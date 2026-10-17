#include "Chart3D.h"

#include "PlotWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace qtplot {

namespace {

constexpr double kPi = 3.14159265358979323846;

void expandPoints(const QVector<Point3D>& pts, CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) {
    for (const Point3D& p : pts) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;
        x.expand(p.x);
        y.expand(p.y);
        z.expand(p.z);
    }
}

CoordinateRange rangeOfZ(const QVector<Point3D>& pts) {
    CoordinateRange r;
    for (const Point3D& p : pts)
        if (std::isfinite(p.z)) r.expand(p.z);
    return r;
}

/// 和 QColor::lighter()/darker() 一樣在 HSV 調亮度，色相才不會跑掉。
QColor shade(const QColor& c, double factor) {
    if (factor >= 1) return c.lighter(static_cast<int>(std::lround(factor * 100)));
    return c.darker(static_cast<int>(std::lround(100 / std::max(0.05, factor))));
}

/// 一個由遠而近排序的繪圖動作。
struct Primitive {
    double depth;
    int order;
    std::function<void(QPainter&)> draw;
};

/// 資料座標 → 置中的盒座標 → 旋轉 → 螢幕。
class Projector {
public:
    Projector(const Chart3D& chart, CoordinateRange xr, CoordinateRange yr, CoordinateRange zr)
        : m_x(xr), m_y(yr), m_z(zr), m_perspective(chart.perspective) {
        const double longest = std::max({chart.aspectX, chart.aspectY, chart.aspectZ, 1e-9});
        m_ax = chart.aspectX / longest;
        m_ay = chart.aspectY / longest;
        m_az = chart.aspectZ / longest;
        const double a = chart.azimuth * kPi / 180, e = chart.elevation * kPi / 180;
        m_cosA = std::cos(a);
        m_sinA = std::sin(a);
        m_cosE = std::cos(e);
        m_sinE = std::sin(e);
    }

    /// 回傳 (螢幕 x, 螢幕 y 向上, 深度：越大越遠)。輸入是盒座標（每軸 -0.5..0.5 乘上長寬比）。
    void projectBox(double x, double y, double z, double& sx, double& sy, double& depth) const {
        const double xr = x * m_cosA - y * m_sinA;
        const double yr = x * m_sinA + y * m_cosA;
        sx = xr;
        sy = yr * m_sinE + z * m_cosE;
        depth = yr * m_cosE - z * m_sinE;
        if (m_perspective) {
            const double f = 2.6 / (2.6 + depth);
            sx *= f;
            sy *= f;
        }
    }

    static double norm(double v, const CoordinateRange& r) {
        return r.span() > 0 ? (v - r.min) / r.span() - 0.5 : 0.0;
    }

    /// 資料座標 → 盒座標。
    void normalize(double x, double y, double z, double& bx, double& by, double& bz) const {
        bx = norm(x, m_x) * m_ax;
        by = norm(y, m_y) * m_ay;
        bz = norm(z, m_z) * m_az;
    }

    void fit(const PixelRect& rect, double margin) {
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
        for (double x : {-0.5, 0.5})
            for (double y : {-0.5, 0.5})
                for (double z : {-0.5, 0.5}) {
                    double sx, sy, d;
                    projectBox(x * m_ax, y * m_ay, z * m_az, sx, sy, d);
                    minX = std::min(minX, sx); maxX = std::max(maxX, sx);
                    minY = std::min(minY, sy); maxY = std::max(maxY, sy);
                }
        const double usable = 1 - 2 * std::clamp(margin, 0.0, 0.45);
        m_scale = std::min(rect.width() * usable / std::max(1e-9, maxX - minX),
                           rect.height() * usable / std::max(1e-9, maxY - minY));
        m_offX = (rect.left + rect.right) / 2 - (minX + maxX) / 2 * m_scale;
        m_offY = (rect.top + rect.bottom) / 2 + (minY + maxY) / 2 * m_scale;
    }

    QPointF toPixel(double sx, double sy) const { return {m_offX + sx * m_scale, m_offY - sy * m_scale}; }

    /// 盒座標（-0.5..0.5，未乘長寬比）→ 像素與深度。
    QPointF box(double x, double y, double z, double* depth = nullptr) const {
        double sx, sy, d;
        projectBox(x * m_ax, y * m_ay, z * m_az, sx, sy, d);
        if (depth) *depth = d;
        return toPixel(sx, sy);
    }

    /// 資料座標 → 像素與深度。
    QPointF data(double x, double y, double z, double* depth = nullptr) const {
        double bx, by, bz, sx, sy, d;
        normalize(x, y, z, bx, by, bz);
        projectBox(bx, by, bz, sx, sy, d);
        if (depth) *depth = d;
        return toPixel(sx, sy);
    }

    /// 某一軸離觀察者較遠的那一面（-0.5 或 +0.5）。背景的三面就畫在這裡。
    double farSide(int axis) const {
        auto depthAt = [&](double s) {
            double d = 0;
            if (axis == 0) box(s, 0, 0, &d);
            else if (axis == 1) box(0, s, 0, &d);
            else box(0, 0, s, &d);
            return d;
        };
        return depthAt(-0.5) > depthAt(0.5) ? -0.5 : 0.5;
    }

    const CoordinateRange& xr() const { return m_x; }
    const CoordinateRange& yr() const { return m_y; }
    const CoordinateRange& zr() const { return m_z; }

private:
    CoordinateRange m_x, m_y, m_z;
    bool m_perspective;
    double m_ax = 1, m_ay = 1, m_az = 1;
    double m_cosA = 1, m_sinA = 0, m_cosE = 1, m_sinE = 0;
    double m_scale = 1, m_offX = 0, m_offY = 0;
};

/// 從盒中心往外推的對齊方式：標籤永遠在盒子外側。
Qt::Alignment alignAwayFrom(QPointF p, QPointF center) {
    const double dx = p.x() - center.x(), dy = center.y() - p.y();
    const double a = std::atan2(dy, dx);
    const double c = std::cos(a), s = std::sin(a);
    if (c > 0.15) return Qt::AlignLeft | Qt::AlignVCenter;
    if (c < -0.15) return Qt::AlignRight | Qt::AlignVCenter;
    return s > 0 ? (Qt::AlignHCenter | Qt::AlignBottom) : (Qt::AlignHCenter | Qt::AlignTop);
}

QPointF pushAway(QPointF p, QPointF center, double distance) {
    const double dx = p.x() - center.x(), dy = p.y() - center.y();
    const double len = std::hypot(dx, dy);
    return len < 1e-6 ? p : QPointF(p.x() + dx / len * distance, p.y() + dy / len * distance);
}

}  // namespace

// ───────────── 序列 ─────────────

void Scatter3D::limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const {
    expandPoints(points, x, y, z);
}
CoordinateRange Scatter3D::colorRange() const { return colorByZ ? rangeOfZ(points) : CoordinateRange(); }
QVector<LegendItem> Scatter3D::legendItems() const {
    if (name.isEmpty()) return {};
    LegendItem item;
    item.text = name;
    item.marker = MarkerStyle(MarkerShape::FilledCircle, 8, color);
    return {item};
}

void Line3D::limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const {
    expandPoints(points, x, y, z);
}
CoordinateRange Line3D::colorRange() const { return colorByZ ? rangeOfZ(points) : CoordinateRange(); }
QVector<LegendItem> Line3D::legendItems() const {
    if (name.isEmpty()) return {};
    LegendItem item;
    item.text = name;
    item.line = LineStyle(color, width);
    return {item};
}

double SurfaceSeries3D::z(int row, int col) const {
    const qsizetype i = static_cast<qsizetype>(row) * xs.size() + col;
    return (row >= 0 && col >= 0 && col < xs.size() && i < zs.size())
               ? zs.at(i) : std::numeric_limits<double>::quiet_NaN();
}
void SurfaceSeries3D::limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& zr) const {
    for (double v : xs) if (std::isfinite(v)) x.expand(v);
    for (double v : ys) if (std::isfinite(v)) y.expand(v);
    for (double v : zs) if (std::isfinite(v)) zr.expand(v);
}
CoordinateRange SurfaceSeries3D::colorRange() const {
    if (!fill || fillColor.isValid()) return {};
    CoordinateRange r;
    for (double v : zs) if (std::isfinite(v)) r.expand(v);
    return r;
}

QVector<LegendItem> Waterfall3D::legendItems() const {
    QVector<LegendItem> items;
    for (int k = 0; k < curveNames.size(); ++k) {
        if (curveNames[k].isEmpty()) continue;
        LegendItem it;
        it.text = curveNames[k];
        it.fill = FillStyle(palette.color(k));
        it.line = LineStyle::none();
        items.append(it);
    }
    return items;
}

void Waterfall3D::limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const {
    const double half = std::max(0.0, thickness) / 2;
    for (double v : xs) if (std::isfinite(v)) x.expand(v);
    for (double v : ys) if (std::isfinite(v)) { y.expand(v - half); y.expand(v + half); }
    bool any = false;
    for (const auto& curve : zs)
        for (double v : curve)
            if (std::isfinite(v)) { z.expand(v); any = true; }
    if (any) z.expand(0.0);  // 曲線填到 0，0 一定要在盒子裡
}

double Columns3D::columnWidth() const {
    auto spacing = [&](bool useX) {
        QVector<double> v;
        for (const Point3D& c : columns) v.append(useX ? c.x : c.y);
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
        double best = 1;
        bool found = false;
        for (int i = 1; i < v.size(); ++i) {
            const double d = v[i] - v[i - 1];
            if (!found || d < best) { best = d; found = true; }
        }
        return best;
    };
    return std::min(spacing(true), spacing(false)) * widthFraction;
}
void Columns3D::limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const {
    if (columns.isEmpty()) return;
    const double h = columnWidth() / 2;
    for (const Point3D& c : columns) {
        x.expand(c.x - h); x.expand(c.x + h);
        y.expand(c.y - h); y.expand(c.y + h);
        z.expand(c.z);
    }
    z.expand(0.0);
}
CoordinateRange Columns3D::colorRange() const { return colorByZ ? rangeOfZ(columns) : CoordinateRange(); }
QVector<LegendItem> Columns3D::legendItems() const {
    if (name.isEmpty()) return {};
    LegendItem item;
    item.text = name;
    item.fill = FillStyle(color);
    return {item};
}

// ───────────── Chart3D ─────────────

Chart3D::Chart3D() {
    tickFont.size = 8.5;
    tickFont.color = QColor(70, 70, 70);
    axisLabelFont.size = 10;
    axisLabelFont.bold = true;
    axisLabelFont.color = QColor(40, 40, 40);
}

void Chart3D::effectiveLimits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const {
    CoordinateRange dx, dy, dz;
    for (const auto& s : series)
        if (s->isVisible) s->limits(dx, dy, dz);
    auto finish = [](CoordinateRange manual, CoordinateRange data) {
        if (manual.isValid() && manual.span() > 0) return manual;
        if (!data.isValid()) return CoordinateRange(0, 1);
        if (data.span() <= 0) return CoordinateRange(data.min - 0.5, data.max + 0.5);
        return data;
    };
    x = finish(xRange, dx);
    y = finish(yRange, dy);
    z = finish(zRange, dz);
}

CoordinateRange Chart3D::effectiveColorRange() const {
    if (colorRange.isValid() && colorRange.span() > 0) return colorRange;
    CoordinateRange r;
    for (const auto& s : series) {
        if (!s->isVisible) continue;
        const CoordinateRange c = s->colorRange();
        if (c.isValid()) r.expand(c);
    }
    if (!r.isValid()) return {0, 1};
    if (r.span() <= 0) return {r.min, r.min + 1};
    return r;
}

QVector<double> Chart3D::niceTicks(const CoordinateRange& range, int target) {
    const double span = range.span();
    if (!(span > 0)) return {range.min};
    const double raw = span / std::max(1, target);
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    double step = 10 * mag;
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (m * mag >= raw) { step = m * mag; break; }
    QVector<double> ticks;
    for (double t = std::ceil(range.min / step - 1e-9) * step; t <= range.max + step * 1e-9; t += step)
        ticks.append(std::abs(t) < step * 1e-9 ? 0.0 : t);
    return ticks;
}

AxisLimits Chart3D::axisLimits() const {
    AxisLimits l;
    l.expand(Coordinates{0, 0});
    l.expand(Coordinates{1, 1});
    return l;
}

QVector<LegendItem> Chart3D::legendItems() const {
    QVector<LegendItem> items;
    for (const auto& s : series)
        if (s->isVisible) items += s->legendItems();
    return items;
}

void Chart3D::render(RenderPack& rp) {
    CoordinateRange xr, yr, zr;
    effectiveLimits(xr, yr, zr);
    Projector proj(*this, xr, yr, zr);
    proj.fit(rp.dataRect, margin);
    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const double sf = rp.scaleFactor;

    const QVector<double> xt = niceTicks(xr), yt = niceTicks(yr), zt = niceTicks(zr);
    const double fx = proj.farSide(0), fy = proj.farSide(1), fz = proj.farSide(2);

    // ── 背後三面：底色、格線、外框 ──
    if (showBox) {
        struct Pane { int axis; double side; };
        for (const Pane& pane : {Pane{0, fx}, Pane{1, fy}, Pane{2, fz}}) {
            auto at = [&](double u, double v) {
                if (pane.axis == 0) return proj.box(pane.side, u, v);
                if (pane.axis == 1) return proj.box(u, pane.side, v);
                return proj.box(u, v, pane.side);
            };
            QPolygonF quad;
            quad << at(-0.5, -0.5) << at(0.5, -0.5) << at(0.5, 0.5) << at(-0.5, 0.5);
            p.setPen(Qt::NoPen);
            p.setBrush(paneColor);
            p.drawPolygon(quad);

            if (showGrid) {
                p.setPen(QPen(gridColor, 1.0 * sf));
                const QVector<double>& ut = pane.axis == 0 ? yt : xt;
                const CoordinateRange& ur = pane.axis == 0 ? yr : xr;
                const QVector<double>& vt = pane.axis == 2 ? yt : zt;
                const CoordinateRange& vr = pane.axis == 2 ? yr : zr;
                for (double t : ut) {
                    const double u = Projector::norm(t, ur);
                    p.drawLine(at(u, -0.5), at(u, 0.5));
                }
                for (double t : vt) {
                    const double v = Projector::norm(t, vr);
                    p.drawLine(at(-0.5, v), at(0.5, v));
                }
            }
            p.setPen(QPen(boxEdgeColor, 1.0 * sf));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(quad);
        }
    }

    // ── 收集所有圖形，由遠而近畫 ──
    const CoordinateRange cr = effectiveColorRange();
    auto mapColor = [&](double v) { return colormap.lookup(v, cr.min, cr.max); };
    std::vector<Primitive> prims;
    int order = 0;

    for (const auto& sp : series) {
        if (!sp->isVisible) continue;

        if (auto* s = dynamic_cast<SurfaceSeries3D*>(sp.get())) {
            const int nx = static_cast<int>(s->xs.size()), ny = static_cast<int>(s->ys.size());
            for (int j = 0; j + 1 < ny; ++j) {
                for (int i = 0; i + 1 < nx; ++i) {
                    const double z00 = s->z(j, i), z01 = s->z(j, i + 1), z11 = s->z(j + 1, i + 1), z10 = s->z(j + 1, i);
                    if (!std::isfinite(z00) || !std::isfinite(z01) || !std::isfinite(z11) || !std::isfinite(z10)) continue;
                    double d0, d1, d2, d3;
                    QPolygonF quad;
                    quad << proj.data(s->xs[i], s->ys[j], z00, &d0) << proj.data(s->xs[i + 1], s->ys[j], z01, &d1)
                         << proj.data(s->xs[i + 1], s->ys[j + 1], z11, &d2) << proj.data(s->xs[i], s->ys[j + 1], z10, &d3);
                    QColor fill = s->fillColor.isValid() ? s->fillColor : mapColor((z00 + z01 + z11 + z10) / 4);
                    if (s->lighting) {
                        // 在盒座標算法線，光從左上方來
                        double ax, ay, az, bx, by, bz, cx, cy, cz;
                        proj.normalize(s->xs[i], s->ys[j], z00, ax, ay, az);
                        proj.normalize(s->xs[i + 1], s->ys[j], z01, bx, by, bz);
                        proj.normalize(s->xs[i], s->ys[j + 1], z10, cx, cy, cz);
                        const double ux = bx - ax, uy = by - ay, uz = bz - az;
                        const double vx = cx - ax, vy = cy - ay, vz = cz - az;
                        const double nx3 = uy * vz - uz * vy, ny3 = uz * vx - ux * vz, nz3 = ux * vy - uy * vx;
                        const double len = std::sqrt(nx3 * nx3 + ny3 * ny3 + nz3 * nz3);
                        const double light = len > 0 ? (-0.4 * nx3 + 0.3 * ny3 + 0.87 * nz3) / len : 1;
                        fill = shade(fill, 0.72 + 0.3 * std::abs(light));
                    }
                    const bool doFill = s->fill;
                    const QColor wire = s->wireColor;
                    const double ww = s->wireWidth * sf;
                    prims.push_back({(d0 + d1 + d2 + d3) / 4, order++, [=](QPainter& pp) {
                        if (doFill) {
                            pp.setBrush(fill);
                            pp.setPen(QPen(ww > 0 ? wire : fill, std::max(ww, 0.5)));
                        } else {
                            pp.setBrush(Qt::NoBrush);
                            pp.setPen(QPen(wire, std::max(ww, 1.0)));
                        }
                        pp.drawPolygon(quad);
                    }});
                }
            }
        } else if (auto* tw = dynamic_cast<Waterfall3D*>(sp.get()); tw && tw->thickness > 0) {
            // 有厚度：緞帶（只有頂面）或實心板（後面、頂面、前面）
            const double baseZ = std::clamp(0.0, zr.min, zr.max);
            const bool plusYFar = proj.farSide(1) > 0;
            for (int k = 0; k < std::min(tw->ys.size(), tw->zs.size()); ++k) {
                const int n = static_cast<int>(std::min(tw->xs.size(), tw->zs[k].size()));
                if (n < 2) continue;
                const double y0 = tw->ys[k] - tw->thickness / 2, y1 = tw->ys[k] + tw->thickness / 2;
                const double nearY = plusYFar ? y0 : y1, farY = plusYFar ? y1 : y0;
                QColor fill = tw->palette.color(k);
                fill.setAlpha(tw->fillAlpha);
                QColor top = shade(fill, 1.18);
                top.setAlpha(tw->fillAlpha);
                const bool solid = tw->fillToFloor;

                auto face = [&](double y) {
                    QPolygonF poly;
                    poly << proj.data(tw->xs[0], y, baseZ);
                    for (int i = 0; i < n; ++i) poly << proj.data(tw->xs[i], y, tw->zs[k][i]);
                    poly << proj.data(tw->xs[n - 1], y, baseZ);
                    return poly;
                };
                struct Strip { double depth; QPolygonF quad; };
                QVector<Strip> strips;
                double depthSum = 0;
                for (int i = 0; i + 1 < n; ++i) {
                    double d0, d1, d2, d3;
                    QPolygonF q;
                    q << proj.data(tw->xs[i], y0, tw->zs[k][i], &d0) << proj.data(tw->xs[i + 1], y0, tw->zs[k][i + 1], &d1)
                      << proj.data(tw->xs[i + 1], y1, tw->zs[k][i + 1], &d2) << proj.data(tw->xs[i], y1, tw->zs[k][i], &d3);
                    strips.append({(d0 + d1 + d2 + d3) / 4, q});
                    depthSum += (d0 + d1 + d2 + d3) / 4;
                }
                std::sort(strips.begin(), strips.end(), [](const Strip& a, const Strip& b) { return a.depth > b.depth; });
                const QPolygonF farFace = face(farY), nearFace = face(nearY);
                const QColor edge = tw->edgeColor;
                const double ew = tw->edgeWidth * sf;
                const QColor stripEdge = shade(fill, 0.85);
                prims.push_back({depthSum / (n - 1), order++, [=](QPainter& pp) {
                    if (solid) {
                        pp.setPen(Qt::NoPen);
                        pp.setBrush(fill);
                        pp.drawPolygon(farFace);
                    }
                    pp.setPen(QPen(stripEdge, 0.6 * sf));
                    for (const Strip& st : strips) {
                        pp.setBrush(solid ? top : fill);
                        pp.drawPolygon(st.quad);
                    }
                    if (solid) {
                        pp.setPen(Qt::NoPen);
                        pp.setBrush(fill);
                        pp.drawPolygon(nearFace);
                        pp.setPen(QPen(edge, ew));
                        pp.setBrush(Qt::NoBrush);
                        pp.drawPolyline(nearFace.mid(1, n));
                    }
                }});
            }
        } else if (auto* w = dynamic_cast<Waterfall3D*>(sp.get())) {
            const double baseZ = std::clamp(0.0, zr.min, zr.max);
            for (int k = 0; k < std::min(w->ys.size(), w->zs.size()); ++k) {
                const int n = static_cast<int>(std::min(w->xs.size(), w->zs[k].size()));
                if (n < 2) continue;
                QPolygonF curve, poly;
                double depth = 0;
                for (int i = 0; i < n; ++i) {
                    double d;
                    curve << proj.data(w->xs[i], w->ys[k], w->zs[k][i], &d);
                    depth += d;
                }
                poly << proj.data(w->xs[0], w->ys[k], baseZ) << curve << proj.data(w->xs[n - 1], w->ys[k], baseZ);
                QColor fill = w->palette.color(k);
                fill.setAlpha(w->fillAlpha);
                const QColor edge = w->edgeColor;
                const double ew = w->edgeWidth * sf;
                prims.push_back({depth / n, order++, [=](QPainter& pp) {
                    pp.setPen(Qt::NoPen);
                    pp.setBrush(fill);
                    pp.drawPolygon(poly);
                    pp.setPen(QPen(edge, ew));
                    pp.setBrush(Qt::NoBrush);
                    pp.drawPolyline(curve);
                }});
            }
        } else if (auto* c = dynamic_cast<Columns3D*>(sp.get())) {
            // 規則格網上整根柱一起排序就正確；只畫朝向觀察者的三面
            const double h = c->columnWidth() / 2;
            const double baseZ = std::clamp(0.0, zr.min, zr.max);
            const bool showLeft = proj.farSide(0) > 0, showFront = proj.farSide(1) > 0;
            for (const Point3D& col : c->columns) {
                const QColor color = c->colorByZ ? mapColor(col.z) : c->color;
                const double x0 = col.x - h, x1 = col.x + h, y0 = col.y - h, y1 = col.y + h;
                const double xs = showLeft ? x0 : x1, ys = showFront ? y0 : y1;
                QPolygonF front, side, top;
                front << proj.data(x0, ys, baseZ) << proj.data(x1, ys, baseZ) << proj.data(x1, ys, col.z) << proj.data(x0, ys, col.z);
                side << proj.data(xs, y0, baseZ) << proj.data(xs, y1, baseZ) << proj.data(xs, y1, col.z) << proj.data(xs, y0, col.z);
                top << proj.data(x0, y0, col.z) << proj.data(x1, y0, col.z) << proj.data(x1, y1, col.z) << proj.data(x0, y1, col.z);
                double depth;
                proj.data(col.x, col.y, (baseZ + col.z) / 2, &depth);
                const QColor edge = c->edgeColor;
                const double ew = 0.8 * sf;
                prims.push_back({depth, order++, [=](QPainter& pp) {
                    pp.setPen(QPen(edge, ew));
                    pp.setBrush(shade(color, 0.85)); pp.drawPolygon(front);
                    pp.setBrush(shade(color, 0.7));  pp.drawPolygon(side);
                    pp.setBrush(shade(color, 1.1));  pp.drawPolygon(top);
                }});
            }
        } else if (auto* l = dynamic_cast<Line3D*>(sp.get())) {
            for (int i = 0; i + 1 < l->points.size(); ++i) {
                const Point3D a = l->points[i], b = l->points[i + 1];
                double da, db;
                const QPointF pa = proj.data(a.x, a.y, a.z, &da), pb = proj.data(b.x, b.y, b.z, &db);
                const QColor color = l->colorByZ ? mapColor((a.z + b.z) / 2) : l->color;
                const double lw = l->width * sf;
                prims.push_back({(da + db) / 2, order++, [=](QPainter& pp) {
                    QPen pen(color, lw);
                    pen.setCapStyle(Qt::RoundCap);
                    pp.setPen(pen);
                    pp.drawLine(pa, pb);
                }});
            }
        } else if (auto* sc = dynamic_cast<Scatter3D*>(sp.get())) {
            for (const Point3D& pt : sc->points) {
                double d;
                const QPointF px = proj.data(pt.x, pt.y, pt.z, &d);
                const QColor color = sc->colorByZ ? mapColor(pt.z) : sc->color;
                if (sc->dropLines) {
                    const QPointF floor = proj.data(pt.x, pt.y, zr.min);
                    const QColor dc = sc->dropLineColor;
                    prims.push_back({d + 1e-6, order++, [=](QPainter& pp) {
                        pp.setPen(QPen(dc, 1.0 * sf));
                        pp.drawLine(floor, px);
                    }});
                }
                MarkerStyle m(MarkerShape::FilledCircle, sc->markerSize * sf, color);
                m.outline = LineStyle(QColor(255, 255, 255, 180), 0.6 * sf);
                prims.push_back({d, order++, [=](QPainter& pp) { drawMarker(pp, px, m); }});
            }
        }
    }

    std::stable_sort(prims.begin(), prims.end(), [](const Primitive& a, const Primitive& b) {
        if (a.depth != b.depth) return a.depth > b.depth;
        return a.order < b.order;
    });
    for (const Primitive& pr : prims) pr.draw(p);

    // ── 刻度與軸名 ──
    if (showBox) {
        const QPointF center = proj.box(0, 0, 0);
        const double floor = elevation >= 0 ? -0.5 : 0.5;
        auto labels = [&](const QVector<double>& ticks, const CoordinateRange& range,
                          const std::function<QPointF(double)>& at, const QString& title,
                          const std::function<QString(double)>& formatter) {
            Label lb;
            lb.font = tickFont;
            for (double t : ticks) {
                const QPointF px = pushAway(at(Projector::norm(t, range)), center, 6);
                lb.text = formatter ? formatter(t) : QString::number(t, 'g', 6);
                drawText(p, px, lb, alignAwayFrom(px, center));
            }
            Label tl;
            tl.text = title;
            tl.font = axisLabelFont;
            const QPointF m = pushAway(at(0), center, 34);
            drawText(p, m, tl, alignAwayFrom(m, center));
        };
        labels(xt, xr, [&](double u) { return proj.box(u, -fy, floor); }, xLabel, xTickFormatter);
        labels(yt, yr, [&](double u) { return proj.box(-fx, u, floor); }, yLabel, yTickFormatter);

        // Z 標在螢幕上最左邊那條垂直邊
        double bestX = 1e300, ex = -0.5, ey = -0.5;
        for (double x : {-0.5, 0.5})
            for (double y : {-0.5, 0.5}) {
                const double sx = proj.box(x, y, 0).x();
                if (sx < bestX) { bestX = sx; ex = x; ey = y; }
            }
        labels(zt, zr, [&](double u) { return proj.box(ex, ey, u); }, zLabel, zTickFormatter);
    }

    p.restore();
}

// ───────────── 滑鼠旋轉 ─────────────

namespace {

class RotationFilter : public QObject {
public:
    RotationFilter(PlotWidget* widget, Chart3D* chart) : QObject(widget), m_widget(widget), m_chart(chart) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* e = static_cast<QMouseEvent*>(event);
            if (e->button() != Qt::LeftButton) return false;
            m_dragging = true;
            m_start = e->position();
            m_startAzimuth = m_chart->azimuth;
            m_startElevation = m_chart->elevation;
            return true;
        }
        case QEvent::MouseMove: {
            if (!m_dragging) return false;
            auto* e = static_cast<QMouseEvent*>(event);
            const QPointF d = e->position() - m_start;
            m_chart->azimuth = m_startAzimuth - d.x() * 0.5;
            m_chart->elevation = std::clamp(m_startElevation + d.y() * 0.4, -89.0, 89.0);
            m_widget->refresh();
            return true;
        }
        case QEvent::MouseButtonRelease:
            if (!m_dragging) return false;
            m_dragging = false;
            return true;
        case QEvent::Wheel: {
            auto* e = static_cast<QWheelEvent*>(event);
            const int steps = e->angleDelta().y() > 0 ? 1 : (e->angleDelta().y() < 0 ? -1 : 0);
            m_chart->elevation = std::clamp(m_chart->elevation + steps * 5.0, -89.0, 89.0);
            m_widget->refresh();
            return true;
        }
        default:
            return false;
        }
    }

private:
    PlotWidget* m_widget;
    Chart3D* m_chart;
    bool m_dragging = false;
    QPointF m_start;
    double m_startAzimuth = 0, m_startElevation = 0;
};

}  // namespace

void enableMouseRotation(PlotWidget& widget, Chart3D* chart) {
    if (!chart) return;
    widget.panEnabled = false;
    widget.zoomEnabled = false;
    widget.rectangleZoomEnabled = false;
    widget.installEventFilter(new RotationFilter(&widget, chart));
}

}  // namespace qtplot

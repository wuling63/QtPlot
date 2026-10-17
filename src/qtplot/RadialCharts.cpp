#include "RadialCharts.h"

#include <QLocale>

#include "Translation.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace qtplot {

namespace {

/// 圓外的標籤要往外長：右半邊靠左對齊、左半邊靠右對齊、正上下置中。
Qt::Alignment outwardAlignment(double radians) {
    const double c = std::cos(radians), s = std::sin(radians);
    if (c > 0.15) return Qt::AlignLeft | Qt::AlignVCenter;
    if (c < -0.15) return Qt::AlignRight | Qt::AlignVCenter;
    return s > 0 ? (Qt::AlignHCenter | Qt::AlignBottom) : (Qt::AlignHCenter | Qt::AlignTop);
}

/// 每資料單位多少像素。取兩軸較小者，圓才不會被拉成橢圓。
double pixelsPerUnit(const Transform& t, Coordinates c) {
    const QPointF o = t.p(c);
    const QPointF x = t.p(c.x + 1, c.y), y = t.p(c.x, c.y + 1);
    return std::min(std::abs(x.x() - o.x()), std::abs(y.y() - o.y()));
}

QPointF polar(QPointF c, double radians, double r) {
    return {c.x() + r * std::cos(radians), c.y() - r * std::sin(radians)};
}

/// 從 a0 到 a1（弧度）的環形帶狀區域。
QPolygonF band(QPointF c, double a0, double a1, double rIn, double rOut) {
    const int steps = std::max(2, static_cast<int>(std::abs(a1 - a0) * 180 / kPi * 1.5));
    QPolygonF poly;
    for (int k = 0; k <= steps; ++k) poly << polar(c, a0 + (a1 - a0) * k / steps, rOut);
    for (int k = steps; k >= 0; --k) poly << polar(c, a0 + (a1 - a0) * k / steps, rIn);
    return poly;
}

}  // namespace

// ───────────────────────────── RadialBarChart ─────────────────────────────

QColor RadialBarChart::color(int index) const {
    if (index < colors.size() && colors[index].isValid()) return colors[index];
    return barColor.isValid() ? barColor : palette.color(index);
}

void RadialBarChart::ring(int index, double& middle, double& thickness) const {
    const int n = std::max<int>(1, static_cast<int>(values.size()));
    const double inner = std::clamp(innerRadiusFraction, 0.0, 0.95);
    const double pitch = (1 - inner) / n;
    const int r = firstOutermost ? index : n - 1 - index;
    middle = 1 - pitch * (r + 0.5);
    thickness = pitch * (1 - std::clamp(ringGapFraction, 0.0, 0.9));
}

QVector<double> RadialBarChart::sweeps() const {
    QVector<double> v;
    for (double x : values) v.append(std::isfinite(x) ? std::max(0.0, x) : 0.0);
    QVector<double> out(v.size(), 0.0);
    if (v.isEmpty()) return out;

    if (!equalArcLength) {
        const double mx = (std::isfinite(maxValue) && maxValue > 0) ? maxValue
                                                                     : std::max(*std::max_element(v.begin(), v.end()), 1e-300);
        for (int i = 0; i < v.size(); ++i) out[i] = sweepAngle * std::min(1.0, v[i] / mx);
        return out;
    }

    // 弧長 = 數值 × k；k 取「讓每一條都不超過 sweepAngle」的最大值
    QVector<double> radii;
    for (int i = 0; i < v.size(); ++i) {
        double m, th;
        ring(i, m, th);
        radii.append(m);
    }
    double k = std::numeric_limits<double>::max();
    for (int i = 0; i < v.size(); ++i)
        if (v[i] > 0) k = std::min(k, sweepAngle * radii[i] / v[i]);
    if (std::isfinite(maxValue) && maxValue > 0)
        k = sweepAngle * *std::max_element(radii.begin(), radii.end()) / maxValue;
    if (k == std::numeric_limits<double>::max()) k = 0;
    for (int i = 0; i < v.size(); ++i) out[i] = std::min(sweepAngle, k * v[i] / radii[i]);
    return out;
}

AxisLimits RadialBarChart::axisLimits() const {
    AxisLimits l;
    l.expand(Coordinates{center.x - radius * 1.05, center.y - radius * 1.05});
    l.expand(Coordinates{center.x + radius * 1.05, center.y + radius * 1.05});
    return l;
}

QVector<LegendItem> RadialBarChart::legendItems() const {
    QVector<LegendItem> items;
    if (!showInLegend) return items;
    for (int i = 0; i < values.size(); ++i) {
        LegendItem it;
        it.text = i < labels.size() ? labels[i] : QString();
        it.fill = FillStyle(color(i));
        it.line = LineStyle::none();
        items.append(it);
    }
    return items;
}

void RadialBarChart::render(RenderPack& rp) {
    if (values.isEmpty()) return;
    const Transform t = transform(rp);
    const QPointF c = t.p(center);
    const double r = radius * pixelsPerUnit(t, center);
    const double dir = clockwise ? -1 : 1;
    const double start = startAngle * kPi / 180;
    const QVector<double> sw = sweeps();

    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    auto drawBar = [&](double sweepDeg, double middle, double thickness, const QColor& col) {
        if (sweepDeg <= 0) return;
        const double a1 = start + dir * sweepDeg * kPi / 180;
        p.setBrush(col);
        p.drawPolygon(band(c, start, a1, r * (middle - thickness / 2), r * (middle + thickness / 2)));
        if (roundCaps) {
            const double cap = r * thickness / 2;
            for (double a : {start, a1}) p.drawEllipse(polar(c, a, r * middle), cap, cap);
        }
    };

    for (int i = 0; i < values.size(); ++i) {
        double middle, thickness;
        ring(i, middle, thickness);
        if (showTracks) drawBar(sweepAngle, middle, thickness, trackColor);
        drawBar(sw[i], middle, thickness, color(i));
    }

    if (showLabels) {
        // 標籤放在每條起點的「前面」（和畫的方向相反的那一側）
        const double back = start - dir * kPi / 2;
        Label lb;
        lb.font = labelFont;
        for (int i = 0; i < values.size(); ++i) {
            double middle, thickness;
            ring(i, middle, thickness);
            const QString name = i < labels.size() ? labels[i] : QString();
            lb.text = showValues ? labelFormat.arg(name, displayLocale().toString(values[i], 'f', valueDecimals)) : name;
            if (lb.text.trimmed().isEmpty()) continue;
            const QPointF anchor = polar(c, start, r * middle);
            const double offset = r * thickness / 2 + 6;
            const QPointF at(anchor.x() + std::cos(back) * offset, anchor.y() - std::sin(back) * offset);
            drawText(p, at, lb, outwardAlignment(back));
        }
    }

    if (centerLabel.isVisible()) drawText(p, c, centerLabel, Qt::AlignHCenter | Qt::AlignVCenter);
    p.restore();
}

// ───────────────────────────── CircularHeatmap ─────────────────────────────

QColor CircularHeatmap::cell(int ring, int sector) const {
    const qsizetype i = static_cast<qsizetype>(ring) * sectors + sector;
    return (i >= 0 && i < cells.size()) ? cells[i] : QColor();
}

double CircularHeatmap::sectorAngle(double sector) const {
    const double step = std::clamp(sweepAngle, 1.0, 360.0) / std::max(1, sectors);
    return startAngle + (clockwise ? -1 : 1) * step * sector;
}

AxisLimits CircularHeatmap::axisLimits() const {
    const double pad = 1.35 + separatorOverhang;
    AxisLimits l;
    l.expand(Coordinates{center.x - radius * pad, center.y - radius * pad});
    l.expand(Coordinates{center.x + radius * pad, center.y + radius * pad});
    return l;
}

void CircularHeatmap::render(RenderPack& rp) {
    if (rings <= 0 || sectors <= 0) return;
    const Transform t = transform(rp);
    const QPointF c = t.p(center);
    const double r = radius * pixelsPerUnit(t, center);
    const double inner = std::clamp(innerRadiusFraction, 0.0, 0.95);
    const double pitch = (1 - inner) / rings;
    auto rad = [&](double sector) { return sectorAngle(sector) * kPi / 180; };

    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    for (int ring = 0; ring < rings; ++ring) {
        const double rOut = r * (1 - pitch * ring), rIn = r * (1 - pitch * (ring + 1));
        for (int s = 0; s < sectors; ++s) {
            const QColor col = cell(ring, s);
            const QPolygonF poly = band(c, rad(s), rad(s + 1), rIn, rOut);
            p.setBrush(col.isValid() ? col : emptyColor);
            p.setPen(cellBorder.isVisible() ? cellBorder.pen() : QPen(Qt::NoPen));
            p.drawPolygon(poly);

            const qsizetype i = static_cast<qsizetype>(ring) * sectors + s;
            if (i < striped.size() && striped[i]) {
                // 幾條沿半徑方向的細線
                p.setPen(QPen(stripeColor, 1.2));
                for (int k = 1; k <= 3; ++k) {
                    const double a = rad(s + k / 4.0);
                    p.drawLine(polar(c, a, rIn), polar(c, a, rOut));
                }
            }
        }
    }

    if (!cellLabels.isEmpty()) {
        Label cl;
        cl.font = cellLabelFont;
        for (int ring = 0; ring < rings; ++ring)
            for (int s = 0; s < sectors; ++s) {
                const qsizetype i = static_cast<qsizetype>(ring) * sectors + s;
                if (i >= cellLabels.size() || cellLabels[i].isEmpty()) continue;
                cl.text = cellLabels[i];
                if (autoContrastCellLabels) {
                    const QColor bg = cell(ring, s).isValid() ? cell(ring, s) : emptyColor;
                    const double lum = 0.299 * bg.redF() + 0.587 * bg.greenF() + 0.114 * bg.blueF();
                    cl.font.color = lum > 0.6 ? QColor(0, 0, 0) : QColor(255, 255, 255);
                }
                drawText(p, polar(c, rad(s + 0.5), r * (1 - pitch * (ring + 0.5))), cl,
                         Qt::AlignHCenter | Qt::AlignVCenter);
            }
    }

    Label lb;
    lb.font = sectorLabelFont;
    for (int s = 0; s < sectors && s < sectorLabels.size(); ++s) {
        lb.text = sectorLabels[s];
        const double a = rad(s + 0.5);
        drawText(p, polar(c, a, r * 1.03), lb, outwardAlignment(a));
    }

    p.setPen(separatorLine.isVisible() ? separatorLine.pen() : QPen(Qt::NoPen));
    for (int s : separators) {
        const double a = rad(s);
        p.drawLine(polar(c, a, r * inner), polar(c, a, r * (1 + separatorOverhang)));
    }

    Label gl;
    gl.font = groupLabelFont;
    for (const GroupLabel& g : groupLabels) {
        gl.text = g.text;
        const double a = rad(g.sector);
        drawText(p, polar(c, a, r * 1.12), gl, outwardAlignment(a));
    }

    if (centerLabel.isVisible()) drawText(p, c, centerLabel, Qt::AlignHCenter | Qt::AlignVCenter);
    p.restore();
}

}  // namespace qtplot

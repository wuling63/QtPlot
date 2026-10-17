#include "Multivariate.h"

#include <QPolygonF>

#include <algorithm>

namespace qtplot {

QVector<CoordinateRange> ParallelCoordinates::resolvedRanges() const {
    QVector<CoordinateRange> out(axes.size());
    for (int d = 0; d < axes.size(); ++d) {
        if (axes[d].range.isValid() && axes[d].range.hasArea()) {
            out[d] = axes[d].range;
            continue;
        }
        CoordinateRange r;
        for (const QVector<double>& row : rows)
            if (d < row.size()) r.expand(row[d]);
        // 全部一樣大時給它一點寬度，否則除以零
        if (r.isValid() && !r.hasArea()) r = CoordinateRange(r.min - 0.5, r.max + 0.5);
        out[d] = r;
    }
    return out;
}

double ParallelCoordinates::axisX(int dim) const {
    const int n = static_cast<int>(axes.size());
    if (n <= 1) return (extent.left + extent.right) / 2;
    return extent.left + extent.width() * dim / (n - 1);
}

double ParallelCoordinates::valueToY(int dim, double value) const {
    if (dim < 0 || dim >= axes.size()) return extent.bottom;
    const QVector<CoordinateRange> ranges = resolvedRanges();
    const CoordinateRange& r = ranges[dim];
    if (!r.isValid() || !r.hasArea()) return (extent.bottom + extent.top) / 2;
    double f = (value - r.min) / r.span();
    if (axes[dim].inverted) f = 1.0 - f;
    return extent.bottom + extent.height() * f;
}

AxisLimits ParallelCoordinates::axisLimits() const {
    if (axes.isEmpty()) return AxisLimits();
    return AxisLimits(extent.xRange(), extent.yRange());
}

void ParallelCoordinates::render(RenderPack& rp) {
    const int n = static_cast<int>(axes.size());
    if (n == 0 || rows.isEmpty()) return;

    const QVector<CoordinateRange> ranges = resolvedRanges();
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 線先畫，軸才蓋在上面。反過來的話成千上百條線會把軸整個糊掉。
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < rows.size(); ++i) {
        const QVector<double>& row = rows[i];
        QPolygonF poly;
        poly.reserve(n);
        bool broken = false;
        for (int d = 0; d < n && d < row.size(); ++d) {
            if (!std::isfinite(row[d])) {
                // 缺值讓這一筆整條不畫。只跳過那一段的話，線會從相鄰兩軸直接
                // 連過去，看起來像一筆完整的資料 —— 那是在編造。
                broken = true;
                break;
            }
            poly.append(t.p(axisX(d), valueToY(d, row[d])));
        }
        if (broken || poly.size() < 2) continue;

        LineStyle ls = lineStyle;
        if (i < rowColors.size() && rowColors[i].isValid()) {
            QColor c = rowColors[i];
            c.setAlpha(lineStyle.color.alpha());
            ls.color = c;
        }
        if (!ls.isVisible()) continue;
        p.setPen(ls.pen());
        p.drawPolyline(poly);
    }

    // ── 軸 ──
    for (int d = 0; d < n; ++d) {
        const double x = axisX(d);
        if (axisLine.isVisible()) {
            p.setPen(axisLine.pen());
            p.drawLine(t.p(x, extent.bottom), t.p(x, extent.top));
        }

        if (!axes[d].label.isEmpty() && axisLabelFont.color.alpha() > 0) {
            Label lb;
            lb.text = axes[d].label;
            lb.font = axisLabelFont;
            drawText(p, t.p(x, extent.top) + QPointF(0, -axisLabelGapPx), lb,
                     Qt::AlignHCenter | Qt::AlignBottom);
        }

        if (tickFont.color.alpha() == 0 || !ranges[d].isValid()) continue;
        const int ticks = std::max(2, axes[d].tickCount);
        for (int k = 0; k < ticks; ++k) {
            const double f = static_cast<double>(k) / (ticks - 1);
            const double value = ranges[d].min + ranges[d].span() * f;
            Label lb;
            lb.text = formatTickLabel(value, ranges[d].span() / (ticks - 1),
                                      std::abs(ranges[d].max));
            lb.font = tickFont;
            // 第一根軸的刻度放左邊，其餘放右邊 —— 全部放同一側的話，
            // 相鄰兩軸的數字會擠在一起
            const bool left = (d == 0);
            drawText(p, t.p(x, valueToY(d, value)) + QPointF(left ? -tickGapPx : tickGapPx, 0),
                     lb, (left ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> ParallelCoordinates::legendItems() const {
    QVector<LegendItem> items;
    QStringList seen;
    for (int i = 0; i < rowGroups.size(); ++i) {
        const QString& g = rowGroups[i];
        if (g.isEmpty() || seen.contains(g)) continue;
        seen.append(g);
        LegendItem it;
        it.text = g;
        LineStyle ls = lineStyle;
        if (i < rowColors.size() && rowColors[i].isValid()) ls.color = rowColors[i];
        ls.width = 3;
        it.line = ls;
        items.append(it);
    }
    return items;
}

}  // namespace qtplot

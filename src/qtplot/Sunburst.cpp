#include "Sunburst.h"

#include <functional>

namespace qtplot {

double SunburstNode::total() const {
    if (children.isEmpty()) return std::isfinite(value) && value > 0 ? value : 0;
    double sum = 0;
    for (const SunburstNode& c : children) sum += c.total();
    return sum;
}

int SunburstNode::depth() const {
    int best = 0;
    for (const SunburstNode& c : children) best = std::max(best, c.depth());
    return best + 1;
}

int Sunburst::depth() const {
    int best = 0;
    for (const SunburstNode& n : roots) best = std::max(best, n.depth());
    return best;
}

AxisLimits Sunburst::axisLimits() const {
    if (roots.isEmpty()) return AxisLimits();
    const double r = radius * 1.02;
    return AxisLimits(CoordinateRange(center.x - r, center.x + r),
                      CoordinateRange(center.y - r, center.y + r));
}

namespace {

/// 角度正規化到 [0, 360)。
double norm360(double deg) {
    double d = std::fmod(deg, 360.0);
    if (d < 0) d += 360.0;
    return d;
}

}  // namespace

void Sunburst::render(RenderPack& rp) {
    const int levels = depth();
    if (levels <= 0) return;

    double grandTotal = 0;
    for (const SunburstNode& n : roots) grandTotal += n.total();
    if (!(grandTotal > 0)) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double dir = clockwise ? -1.0 : 1.0;
    const double rInner = radius * std::clamp(innerFraction, 0.0, 0.9);
    const double ringWidth = (radius - rInner) / levels;

    // 以「畫一層」為單位遞迴。每一層自己知道角度區間，子節點再把它分掉。
    std::function<void(const QVector<SunburstNode>&, int, double, double, QColor)> drawLevel =
        [&](const QVector<SunburstNode>& nodes, int level, double startDeg, double spanDeg,
            QColor inherited) {
            double total = 0;
            for (const SunburstNode& n : nodes) total += n.total();
            if (!(total > 0)) return;

            const double r0 = rInner + ringWidth * level;
            const double r1 = r0 + ringWidth;
            double cursor = startDeg;

            for (int i = 0; i < nodes.size(); ++i) {
                const SunburstNode& n = nodes.at(i);
                const double v = n.total();
                if (!(v > 0)) continue;
                const double sweep = spanDeg * (v / total);

                QColor c = n.fillColor;
                if (!c.isValid()) {
                    // 最內圈取調色盤；往外每層在父節點的顏色上調亮，
                    // 同一個分支就看得出是一家人
                    c = inherited.isValid() ? inherited.lighter(100 + lightenPerLevel)
                                            : palette.color(i);
                }

                QPainterPath path = sectorPath(t, center, r1, r0, cursor, sweep);
                p.setBrush(QBrush(c));
                p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
                p.drawPath(path);

                // ── 標籤 ──
                if (!n.label.isEmpty() && labelFont.color.alpha() > 0) {
                    const double rMid = (r0 + r1) / 2;
                    const double midDeg = cursor + sweep / 2;
                    // 弧長用「中間半徑」算：扇形窄的時候文字本來就擺不下
                    const double arcPx =
                        std::abs(sweep) * kPi / 180.0 * rMid * std::abs(t.px(1) - t.px(0));

                    Label lb;
                    lb.text = n.label;
                    lb.font = labelFont;

                    // 舊介面相容：沒動過 labelOrientation 時照 rotateLabels 走
                    LabelOrientation orient = labelOrientation;
                    if (orient == LabelOrientation::Tangential && !rotateLabels)
                        orient = LabelOrientation::Horizontal;

                    const double textWidth = measureText(lb).width();
                    const double ringThicknessPx = std::abs(t.px(r1) - t.px(r0));
                    const Coordinates off = fromPolar(rMid, midDeg);
                    const QPointF at = t.p(center.x + off.x, center.y + off.y);

                    // 放不放得下，要看文字沿著哪個方向排：
                    // 沿弧排就比弧長，沿半徑排就比環的厚度。用錯方向的話，
                    // 外圈的長標籤會被誤判成放得下，然後整段溢出去壓到隔壁。
                    switch (orient) {
                    case LabelOrientation::Radial: {
                        if (ringThicknessPx < textWidth + 6) break;
                        const double a = norm360(midDeg);
                        // 左半圈的字要轉 180°，否則是倒著讀的
                        lb.font.rotation = (a > 90 && a < 270) ? -(a + 180) : -a;
                        drawText(p, at, lb, Qt::AlignHCenter | Qt::AlignVCenter);
                        break;
                    }
                    case LabelOrientation::Curved: {
                        const double textSpanDeg = arcTextSpanDegrees(t, center, rMid, lb);
                        if (textSpanDeg > std::abs(sweep) || arcPx < minLabelArcPx) break;
                        drawTextOnArc(p, t, center, rMid, midDeg, lb);
                        break;
                    }
                    case LabelOrientation::Tangential: {
                        if (arcPx < std::max(minLabelArcPx, textWidth + 4)) break;
                        const double a = norm360(midDeg);
                        lb.font.rotation = (a > 90 && a < 270) ? -(a - 180) : -a;
                        drawText(p, at, lb, Qt::AlignHCenter | Qt::AlignVCenter);
                        break;
                    }
                    case LabelOrientation::Horizontal:
                        if (arcPx < std::max(minLabelArcPx, textWidth + 4)) break;
                        drawText(p, at, lb, Qt::AlignHCenter | Qt::AlignVCenter);
                        break;
                    }
                }

                if (!n.children.isEmpty())
                    drawLevel(n.children, level + 1, cursor, sweep, c);
                cursor += sweep;
            }
        };

    drawLevel(roots, 0, startAngle, dir * 360.0, QColor());
    p.restore();
}

QVector<LegendItem> Sunburst::legendItems() const {
    QVector<LegendItem> items;
    // 只列最內圈。往外每層都列的話，圖例會比圖還大。
    for (int i = 0; i < roots.size(); ++i) {
        const SunburstNode& n = roots.at(i);
        if (n.label.isEmpty()) continue;
        LegendItem it;
        it.text = n.label;
        // 自動配色的節點沒有存下顏色，圖例要照同一條規則再取一次，
        // 否則色塊會全部變成灰的
        it.fill = FillStyle(n.fillColor.isValid() ? n.fillColor : palette.color(i));
        it.line = LineStyle::none();
        items.append(it);
    }
    return items;
}


// ───────────────────────── Treemap ─────────────────────────

double TreemapNode::total() const {
    if (children.isEmpty()) return (std::isfinite(value) && value > 0) ? value : 0;
    double sum = 0;
    for (const TreemapNode& c : children) sum += c.total();
    return sum;
}

double Treemap::grandTotal() const {
    double sum = 0;
    for (const TreemapNode& n : roots) sum += n.total();
    return sum;
}

AxisLimits Treemap::axisLimits() const {
    if (roots.isEmpty()) return AxisLimits();
    return AxisLimits(extent.xRange(), extent.yRange());
}

namespace {

/// squarify 的「最差長寬比」：一列裡最扁的那個矩形有多扁。
///
/// row 是這一列各塊的面積，length 是這一列鋪開的方向上的長度。
/// 回傳值愈接近 1 愈好（正方形）。
double worstAspect(const QVector<double>& row, double length) {
    if (row.isEmpty() || length <= 0) return std::numeric_limits<double>::max();
    double sum = 0, lo = std::numeric_limits<double>::max(), hi = 0;
    for (double v : row) {
        sum += v;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    if (sum <= 0 || lo <= 0) return std::numeric_limits<double>::max();
    const double s2 = sum * sum;
    const double l2 = length * length;
    return std::max(l2 * hi / s2, s2 / (l2 * lo));
}

/// 把一列已經決定好的區塊鋪進矩形的一側，並回傳剩下的矩形。
///
/// 沿著「短邊」鋪：那是 squarified 的關鍵，沿長邊鋪會得到一排細長條。
CoordinateRect layoutRow(const QVector<double>& row, const CoordinateRect& rect,
                         double scale, QVector<CoordinateRect>& out) {
    double sum = 0;
    for (double v : row) sum += v;
    if (sum <= 0) return rect;

    const double w = rect.width(), h = rect.height();
    const bool horizontal = w >= h;   // 短邊是高 → 這一列橫著鋪
    const double length = horizontal ? h : w;
    const double thickness = (sum * scale) / length;

    double cursor = horizontal ? rect.bottom : rect.left;
    for (double v : row) {
        const double span = (v * scale) / thickness;
        if (horizontal)
            out.append(CoordinateRect(rect.left, rect.left + thickness, cursor, cursor + span));
        else
            out.append(CoordinateRect(cursor, cursor + span, rect.top - thickness, rect.top));
        cursor += span;
    }

    if (horizontal) return CoordinateRect(rect.left + thickness, rect.right, rect.bottom, rect.top);
    return CoordinateRect(rect.left, rect.right, rect.bottom, rect.top - thickness);
}

/// squarified treemap：把 values（由大到小）鋪進 rect。
QVector<CoordinateRect> squarify(const QVector<double>& values, CoordinateRect rect) {
    QVector<CoordinateRect> out;
    double sum = 0;
    for (double v : values) sum += v;
    if (!(sum > 0) || rect.width() <= 0 || rect.height() <= 0) return out;

    // 把「數值」換算成「面積」的倍率
    const double scale = rect.width() * rect.height() / sum;

    QVector<double> row;
    int i = 0;
    while (i < values.size()) {
        const double length = std::min(rect.width(), rect.height());
        QVector<double> candidate = row;
        candidate.append(values[i] * scale);

        if (row.isEmpty() || worstAspect(candidate, length) <= worstAspect(row, length)) {
            row = candidate;
            ++i;
        } else {
            rect = layoutRow(row, rect, 1.0, out);
            row.clear();
        }
    }
    if (!row.isEmpty()) layoutRow(row, rect, 1.0, out);
    return out;
}

CoordinateRect shrink(const CoordinateRect& r, double by) {
    const double maxBy = std::min(r.width(), r.height()) / 2 - 1e-9;
    const double d = std::min(by, std::max(0.0, maxBy));
    return CoordinateRect(r.left + d, r.right - d, r.bottom + d, r.top - d);
}

}  // namespace

QVector<Treemap::Cell> Treemap::layout() const {
    QVector<Cell> out;
    if (roots.isEmpty()) return out;

    const double shortSide = std::min(extent.width(), extent.height());
    const double pad = shortSide * std::clamp(paddingFraction, 0.0, 0.1);
    const double groupPad = shortSide * std::clamp(groupPaddingFraction, 0.0, 0.2);

    std::function<void(const QVector<TreemapNode>&, CoordinateRect, int, QColor)> place =
        [&](const QVector<TreemapNode>& nodes, CoordinateRect box, int depth, QColor inherited) {
            // 由大到小排是 squarified 的前提：由小到大鋪會把大塊擠成長條
            QVector<int> order;
            QVector<double> values;
            for (int i = 0; i < nodes.size(); ++i) {
                const double v = nodes[i].total();
                if (v > 0) order.append(i);
            }
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                return nodes[a].total() > nodes[b].total();
            });
            for (int i : order) values.append(nodes[i].total());

            const QVector<CoordinateRect> rects = squarify(values, box);
            for (int k = 0; k < rects.size() && k < order.size(); ++k) {
                const TreemapNode& n = nodes[order[k]];
                QColor c = n.fillColor;
                if (!c.isValid())
                    c = inherited.isValid() ? inherited.lighter(100 + lightenPerLevel)
                                            : palette.color(k);

                const CoordinateRect cell = shrink(rects[k], pad / 2);
                out.append({cell, depth, n.children.isEmpty(), n.label, n.total(), c});

                if (!n.children.isEmpty())
                    place(n.children, shrink(cell, groupPad), depth + 1, c);
            }
        };

    place(roots, extent, 0, QColor());
    return out;
}

void Treemap::render(RenderPack& rp) {
    const QVector<Cell> cells = layout();
    if (cells.isEmpty()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    for (const Cell& c : cells) {
        const QRectF box = t.pixelRect(c.rect);
        // 群組只畫外框，裡面交給子層填 —— 填了的話子層會疊在上面，
        // 半透明的子層顏色會被父層汙染
        p.setBrush(c.isLeaf ? QBrush(c.color) : Qt::NoBrush);
        p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
        p.drawRect(box);
    }

    for (const Cell& c : cells) {
        if (c.label.isEmpty()) continue;
        const QRectF box = t.pixelRect(c.rect);
        const FontStyle& font = c.isLeaf ? labelFont : groupLabelFont;
        if (font.color.alpha() == 0) continue;
        if (std::abs(box.width() * box.height()) < minLabelAreaPx) continue;

        Label lb;
        lb.text = (showValues && c.isLeaf)
                      ? QStringLiteral("%1\n%2").arg(c.label).arg(c.value, 0, 'g', 4)
                      : c.label;
        lb.font = font;
        const QSizeF sz = measureText(lb);
        if (sz.width() + 6 > std::abs(box.width())) continue;
        if (sz.height() + 4 > std::abs(box.height())) continue;

        // 葉節點置中、群組靠左上 —— 群組的中間是子層的地盤
        if (c.isLeaf)
            drawText(p, box.center(), lb, Qt::AlignHCenter | Qt::AlignVCenter);
        else
            drawText(p, box.topLeft() + QPointF(4, 3), lb, Qt::AlignLeft | Qt::AlignTop);
    }
    p.restore();
}

QVector<LegendItem> Treemap::legendItems() const {
    QVector<LegendItem> items;
    for (int i = 0; i < roots.size(); ++i) {
        if (roots[i].label.isEmpty()) continue;
        LegendItem it;
        it.text = roots[i].label;
        it.fill = FillStyle(roots[i].fillColor.isValid() ? roots[i].fillColor
                                                        : palette.color(i));
        it.line = LineStyle::none();
        items.append(it);
    }
    return items;
}

}  // namespace qtplot

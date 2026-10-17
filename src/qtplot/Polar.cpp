#include "Polar.h"

#include <QFontMetricsF>

namespace qtplot {

namespace {

/// 順時針時角度要往負的方向走。
inline double dirOf(bool clockwise) { return clockwise ? -1.0 : 1.0; }

/// 把角度正規化到 [0, 360)，只用來挑文字對齊方式。
double normalizeDeg(double deg) {
    double d = std::fmod(deg, 360.0);
    if (d < 0) d += 360.0;
    return d;
}

/// 依標籤所在的角度決定對齊方式，文字才不會壓到圓周。
Qt::Alignment alignmentForAngle(double deg) {
    const double d = normalizeDeg(deg);
    Qt::Alignment h;
    if (d < 85 || d > 275)        h = Qt::AlignLeft;
    else if (d > 95 && d < 265)   h = Qt::AlignRight;
    else                          h = Qt::AlignHCenter;
    Qt::Alignment v;
    if (d > 5 && d < 175)         v = Qt::AlignBottom;
    else if (d > 185 && d < 355)  v = Qt::AlignTop;
    else                          v = Qt::AlignVCenter;
    return h | v;
}

/// 圓（或被壓成的橢圓）佔的資料範圍。
AxisLimits circleLimits(Coordinates center, double radius) {
    AxisLimits lim;
    lim.x.expand(center.x - radius);
    lim.x.expand(center.x + radius);
    lim.y.expand(center.y - radius);
    lim.y.expand(center.y + radius);
    return lim;
}

QColor withAlpha(QColor c, int alpha) {
    c.setAlpha(alpha);
    return c;
}

}  // namespace

// ───────────────────────────── PolarAxis ─────────────────────────────

AxisLimits PolarAxis::axisLimits() const {
    // 多留一成給放射線標籤，不然標籤會被裁掉
    return circleLimits(center, radius * 1.1);
}

Coordinates PolarAxis::coordinates(double r, double angleDeg) const {
    const Coordinates c = fromPolar(r, startAngle + dirOf(clockwise) * angleDeg);
    return {center.x + c.x, center.y + c.y};
}

QVector<PolarSpoke> PolarAxis::resolvedSpokes() const {
    if (!spokes.isEmpty()) return spokes;

    const int n = spokeLabels.isEmpty() ? std::max(1, spokeCount)
                                        : static_cast<int>(spokeLabels.size());
    QVector<PolarSpoke> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        PolarSpoke sp;
        sp.angle = 360.0 * i / n;
        if (i < spokeLabels.size()) sp.label = spokeLabels.at(i);
        sp.lengthFraction = spokeLengthFraction;
        out.append(sp);
    }
    return out;
}

void PolarAxis::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QVector<PolarSpoke> spokeList = resolvedSpokes();

    // 一圈的外形：圓（用多邊形逼近）或以放射線為頂點的多邊形
    const bool asPolygon = polygonRings && spokeList.size() >= 3;
    auto ringShape = [&](double r) {
        QPolygonF poly;
        if (asPolygon)
            for (const PolarSpoke& sp : spokeList) poly.append(t.p(coordinates(r, sp.angle)));
        else
            for (int i = 0; i <= 180; ++i) poly.append(t.p(coordinates(r, i * 2.0)));
        return poly;
    };

    // ── 底色 ──
    if (background.isVisible()) {
        const QPolygonF disc = ringShape(radius);
        p.setPen(Qt::NoPen);
        p.setBrush(background.brush(disc.boundingRect()));
        p.drawPolygon(disc);
    }

    // ── 同心圓 ──
    QVector<double> rings = ringRadii;
    if (rings.isEmpty()) {
        for (int i = 1; i <= std::max(1, ringCount); ++i)
            rings.append(radius * i / std::max(1, ringCount));
    }
    p.setBrush(Qt::NoBrush);
    for (int ri = 0; ri < rings.size(); ++ri) {
        const double r = rings[ri];
        const LineStyle& ls = ri < ringStyles.size() ? ringStyles[ri] : ringLine;
        if (r <= 0 || !ls.isVisible()) continue;
        p.setPen(ls.pen());
        // 用多邊形逼近，兩軸比例不同時才會正確地變成橢圓
        p.drawPolygon(ringShape(r));
    }

    // ── 放射線 ──
    for (const PolarSpoke& sp : spokeList) {
        const LineStyle& ls = (sp.line.color.alpha() > 0) ? sp.line : spokeLine;
        if (!ls.isVisible()) continue;
        const double len = radius * std::clamp(sp.lengthFraction, 0.0, 1.0);
        if (len <= 0) continue;
        p.setPen(ls.pen());
        p.drawLine(t.p(center), t.p(coordinates(len, sp.angle)));
    }

    // ── 同心圓的半徑標籤 ──
    if (showRingLabels && ringLabelFont.color.alpha() > 0) {
        for (int i = 0; i < rings.size(); ++i) {
            const double r = rings[i];
            if (r <= 0) continue;
            const QString text = (i < ringLabels.size())
                                     ? ringLabels.at(i)
                                     : QString::number(r, 'g', 3);
            if (text.isEmpty()) continue;
            Label lb;
            lb.text = text;
            lb.font = ringLabelFont;
            // 貼著圓周的內側放。放外側的話，多根放射線的標籤會跟它擠在一起。
            drawText(p, t.p(coordinates(r, ringLabelAngle)), lb,
                     Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    // ── 放射線標籤 ──
    for (const PolarSpoke& sp : spokeList) {
        const QColor color = sp.labelColor.isValid() ? sp.labelColor : labelFont.color;
        if (color.alpha() == 0) continue;
        const QString text =
            sp.label.isEmpty() ? QStringLiteral("%1°").arg(normalizeDeg(sp.angle), 0, 'f', 0)
                               : sp.label;
        if (text.isEmpty()) continue;

        // 標籤永遠貼在最外圈，不受 lengthFraction 影響 ——
        // 短放射線的標籤跟著縮進去的話，一圈文字會參差不齊
        QPointF anchor = t.p(coordinates(radius, sp.angle));
        // 往外推幾個像素，標籤才不會壓在圓周上
        const double absDeg = startAngle + dirOf(clockwise) * sp.angle;
        const Coordinates unit = fromPolar(1.0, absDeg);
        anchor += QPointF(unit.x, -unit.y) * labelPaddingPx;

        Label lb;
        lb.text = text;
        lb.font = labelFont;
        lb.font.color = color;
        drawText(p, anchor, lb, alignmentForAngle(absDeg));
    }

    p.restore();
}

// ───────────────────────────── Pie ─────────────────────────────

double Pie::total() const {
    double sum = 0;
    for (const PieSlice& s : slices)
        if (s.value > 0 && std::isfinite(s.value)) sum += s.value;
    return sum;
}

AxisLimits Pie::axisLimits() const {
    double maxExplode = 0;
    double maxRadius = radius;
    for (const PieSlice& s : slices) {
        maxExplode = std::max(maxExplode, s.explode);
        // 有的塊可以比整體半徑大，範圍要跟著撐開
        if (s.radiusFraction > 0) maxRadius = std::max(maxRadius, radius * s.radiusFraction);
    }
    const double outer = maxRadius + maxExplode;
    // 標籤畫在圓外時也要留空間
    double factor = std::max(1.08, labelDistance + 0.08);
    // 引線拉得更遠：水平段加上文字本身。文字的寬度是像素、這裡算的是資料單位，
    // 所以只能估 —— 標籤很長時請自己 setAxisLimits。
    if (useLeaderLines)
        factor = std::max(factor, 1.1 + std::max(0.05, leaderLengthFraction) + 0.45);

    // 壓扁之後垂直方向本來就比較矮，範圍要跟著縮，不然圖的上下會空一大片。
    // 但引線標籤是沿著垂直方向排開的，所以留一個下限。
    const double tilt = std::max(std::clamp(tiltFraction, 0.05, 1.0), 0.45);
    const double rx = outer * factor;
    const double ry = outer * factor * tilt;
    return AxisLimits(CoordinateRange(center.x - rx, center.x + rx),
                      CoordinateRange(center.y - ry, center.y + ry));
}

QStringList Pie::sliceTexts() const {
    const double sum = total();
    QStringList out;
    out.reserve(slices.size());
    for (const PieSlice& s : slices) {
        const double fraction = (sum > 0) ? s.value / sum : 0;
        QString text;
        switch (labelMode) {
        case LabelMode::Value:   text = QString::number(s.value, 'g', 4); break;
        case LabelMode::Percent: text = QStringLiteral("%1%").arg(fraction * 100, 0, 'f', 1); break;
        case LabelMode::Label:   text = s.label; break;
        case LabelMode::LabelAndPercent:
            text = s.label.isEmpty()
                       ? QStringLiteral("%1%").arg(fraction * 100, 0, 'f', 1)
                       : QStringLiteral("%1\n%2%").arg(s.label).arg(fraction * 100, 0, 'f', 1);
            break;
        case LabelMode::None: break;
        }
        out.append(text);
    }
    return out;
}

namespace {

/// 一塊扇形畫出來需要的所有幾何。
struct SliceGeometry {
    int index = 0;
    Coordinates center;   ///< 已套用 explode
    double startDeg = 0;
    double sweepDeg = 0;
    double midDeg = 0;
    double fraction = 0;
    double outerRadius = 1;  ///< 這一塊自己的外半徑（已套用 radiusFraction）
};

/// 把標籤的目標 y 座標推開，讓相鄰兩個至少隔 spacing 像素。
///
/// 做法是先按 y 排序，再從上往下掃一遍把太近的往下推，接著從下往上掃回來。
/// 只掃一趟的話，被推到最下面的那些會擠出繪圖區；掃回來才會把整串往回壓。
void spreadLabels(QVector<double>& ys, const QVector<double>& heights, double spacing,
                  double limitTop, double limitBottom) {
    const qsizetype n = ys.size();
    if (n < 2) return;

    for (qsizetype i = 1; i < n; ++i) {
        const double minY = ys[i - 1] + (heights[i - 1] + heights[i]) / 2 + spacing;
        if (ys[i] < minY) ys[i] = minY;
    }
    // 回掃：超出下緣就整串往上擠
    if (ys[n - 1] + heights[n - 1] / 2 > limitBottom)
        ys[n - 1] = limitBottom - heights[n - 1] / 2;
    for (qsizetype i = n - 2; i >= 0; --i) {
        const double maxY = ys[i + 1] - (heights[i + 1] + heights[i]) / 2 - spacing;
        if (ys[i] > maxY) ys[i] = maxY;
    }
    if (ys[0] - heights[0] / 2 < limitTop) ys[0] = limitTop + heights[0] / 2;
}

}  // namespace

void Pie::render(RenderPack& rp) {
    const double sum = total();
    if (sum <= 0 || slices.isEmpty()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double dir = dirOf(clockwise);
    const double tilt = std::clamp(tiltFraction, 0.05, 1.0);
    const double rInner = radius * std::clamp(donutFraction, 0.0, 0.95);

    // 幾何先全部算好。立體圓餅要依「前後」重排繪製順序，一邊算一邊畫做不到。
    QVector<SliceGeometry> geoms;
    double cursor = startAngle;
    for (int i = 0; i < slices.size(); ++i) {
        const PieSlice& s = slices.at(i);
        if (!(s.value > 0) || !std::isfinite(s.value)) continue;

        SliceGeometry g;
        g.index = i;
        g.fraction = s.value / sum;
        g.outerRadius = (s.radiusFraction > 0) ? radius * s.radiusFraction : radius;
        g.startDeg = cursor;
        g.sweepDeg = dir * g.fraction * 360.0;
        g.midDeg = cursor + g.sweepDeg / 2;
        g.center = center;
        if (s.explode != 0) {
            // explode 沿著中線往外推。壓扁的圖連推的方向也要跟著壓，
            // 否則前後兩塊推出去的距離看起來會不一樣。
            const Coordinates off = fromPolar(s.explode, g.midDeg);
            g.center.x += off.x;
            g.center.y += off.y * tilt;
        }
        geoms.append(g);
        cursor += g.sweepDeg;
    }

    auto radii = [&](double rOuter) {
        return QPair<double, double>(rOuter, rOuter * tilt);
    };

    if (depthPx > 0) {
        // 由後往前畫。螢幕上「下面」是前面，對應 sin(θ) 最小的方向，
        // 所以 sin 大的（後面）先畫。
        QVector<const SliceGeometry*> order;
        for (const SliceGeometry& g : geoms) order.append(&g);
        std::stable_sort(order.begin(), order.end(),
                         [](const SliceGeometry* a, const SliceGeometry* b) {
                             return std::sin(a->midDeg * kPi / 180.0) >
                                    std::sin(b->midDeg * kPi / 180.0);
                         });

        const QPointF down(0, depthPx);
        for (const SliceGeometry* gp : order) {
            const SliceGeometry& g = *gp;
            const PieSlice& s = slices.at(g.index);
            const QColor base = s.fillColor.isValid() ? s.fillColor : QColor(Qt::gray);
            p.setPen(Qt::NoPen);

            const auto r = radii(g.outerRadius);
            auto pointAt = [&](double deg, double rx, double ry) {
                const double a = deg * kPi / 180.0;
                return t.p(g.center.x + rx * std::cos(a), g.center.y + ry * std::sin(a));
            };

            // 兩側的平面（扇形被切開的那兩刀）。爆炸開來時看得到。
            p.setBrush(QBrush(base.darker(150)));
            for (double deg : {g.startDeg, g.startDeg + g.sweepDeg}) {
                QPolygonF wall;
                const QPointF outer = pointAt(deg, r.first, r.second);
                const QPointF inner = (rInner > 0)
                                          ? pointAt(deg, rInner, rInner * tilt)
                                          : t.p(g.center);
                wall << inner << outer << outer + down << inner + down;
                p.drawPolygon(wall);
            }

            // 外弧的側壁：只有前半圈（sin < 0）看得到
            const int steps = std::max(4, static_cast<int>(std::abs(g.sweepDeg)));
            QVector<QPointF> front;
            for (int k = 0; k <= steps; ++k) {
                const double deg = g.startDeg + g.sweepDeg * k / steps;
                if (std::sin(deg * kPi / 180.0) > 0) {
                    // 碰到後半圈就把目前這段收掉，前後兩段才不會被接成一片
                    if (front.size() >= 2) {
                        QPolygonF wall;
                        for (const QPointF& q : front) wall << q;
                        for (int j = front.size() - 1; j >= 0; --j) wall << front[j] + down;
                        p.setBrush(QBrush(base.darker(135)));
                        p.drawPolygon(wall);
                    }
                    front.clear();
                    continue;
                }
                front.append(pointAt(deg, r.first, r.second));
            }
            if (front.size() >= 2) {
                QPolygonF wall;
                for (const QPointF& q : front) wall << q;
                for (int j = front.size() - 1; j >= 0; --j) wall << front[j] + down;
                p.setBrush(QBrush(base.darker(135)));
                p.drawPolygon(wall);
            }
        }
    }

    // ── 頂面 ──
    for (const SliceGeometry& g : geoms) {
        const PieSlice& s = slices.at(g.index);
        QPainterPath path = sectorPath(t, g.center, g.outerRadius, g.outerRadius * tilt,
                                       rInner, rInner * tilt, g.startDeg, g.sweepDeg);
        if (s.fill.isVisible() && s.fill.hatch != HatchPattern::None && !s.fill.hasGradient()) {
            // 花紋要有底色：先鋪 fillColor 再疊花紋
            p.setBrush(QBrush(s.fill.color.alpha() > 0 ? s.fill.color
                              : (s.fillColor.isValid() ? s.fillColor : QColor(Qt::gray))));
            p.setPen(Qt::NoPen);
            p.drawPath(path);
            FillStyle hatchOnly = s.fill;
            hatchOnly.color = QColor(0, 0, 0, 0);
            p.setBrush(hatchOnly.brush());
        } else if (s.fill.isVisible()) {
            p.setBrush(s.fill.brush(path.boundingRect()));
        } else {
            p.setBrush(QBrush(s.fillColor.isValid() ? s.fillColor : QColor(Qt::gray)));
        }
        p.setPen(sliceBorder.isVisible() ? sliceBorder.pen() : QPen(Qt::NoPen));
        p.drawPath(path);
    }

    // ── 標籤 ──
    const QStringList texts = sliceTexts();
    if (labelMode != LabelMode::None && labelFont.color.alpha() > 0) {
        if (useLeaderLines) {
            // 左右各排一列。依扇形中線落在哪一側決定，再把同一側的上下推開。
            struct Entry {
                const SliceGeometry* g;
                QPointF anchor;   ///< 引線在圓周上的起點
                double y = 0;     ///< 標籤的目標 y（會被推動）
                double height = 0;
                QString text;
            };
            QVector<Entry> left, right;
            for (const SliceGeometry& g : geoms) {
                const QString text = texts.value(g.index);
                if (text.isEmpty()) continue;
                const double a = g.midDeg * kPi / 180.0;
                Entry e;
                e.g = &g;
                e.anchor = t.p(g.center.x + radius * std::cos(a),
                               g.center.y + radius * tilt * std::sin(a));
                e.y = e.anchor.y();
                e.text = text;
                Label probe;
                probe.text = text;
                probe.font = labelFont;
                e.height = measureText(probe).height();
                (std::cos(a) < 0 ? left : right).append(e);
            }

            const double elbow = radius * std::max(0.05, leaderLengthFraction);
            for (int side = 0; side < 2; ++side) {
                QVector<Entry>& list = (side == 0) ? left : right;
                if (list.isEmpty()) continue;
                std::stable_sort(list.begin(), list.end(),
                                 [](const Entry& a, const Entry& b) { return a.y < b.y; });

                QVector<double> ys, hs;
                for (const Entry& e : list) {
                    ys.append(e.y);
                    hs.append(e.height);
                }
                spreadLabels(ys, hs, leaderMinSpacingPx, rp.dataRect.top + 2,
                             rp.dataRect.bottom - 2);

                const double textX = t.px(center.x + (side == 0 ? -1 : 1) * (radius + elbow));
                for (int i = 0; i < list.size(); ++i) {
                    const Entry& e = list[i];
                    // 三段式引線：先沿半徑出來一小段，再水平拉到文字旁邊
                    const double a = e.g->midDeg * kPi / 180.0;
                    const QPointF mid = t.p(e.g->center.x + radius * 1.1 * std::cos(a),
                                            e.g->center.y + radius * tilt * 1.1 * std::sin(a));
                    const QPointF knee(mid.x(), ys[i]);
                    if (leaderLine.isVisible()) {
                        p.setPen(leaderLine.pen());
                        p.setBrush(Qt::NoBrush);
                        p.drawLine(e.anchor, mid);
                        p.drawLine(mid, knee);
                        p.drawLine(knee, QPointF(textX, ys[i]));
                    }
                    Label lb;
                    lb.text = e.text;
                    lb.font = labelFont;
                    drawText(p, QPointF(textX + (side == 0 ? -3 : 3), ys[i]), lb,
                             (side == 0 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
                }
            }
        } else {
            for (const SliceGeometry& g : geoms) {
                const QString text = texts.value(g.index);
                if (text.isEmpty()) continue;
                const double rLabel = rInner + (g.outerRadius - rInner) * labelDistance;
                const double a = g.midDeg * kPi / 180.0;
                Label lb;
                lb.text = text;
                lb.font = labelFont;
                // 標籤在圓內就置中；畫到圓外就依角度靠邊，才不會壓到扇形
                const Qt::Alignment al = (labelDistance <= 1.0)
                                             ? (Qt::AlignHCenter | Qt::AlignVCenter)
                                             : alignmentForAngle(g.midDeg);
                drawText(p, t.p(g.center.x + rLabel * std::cos(a),
                                g.center.y + rLabel * tilt * std::sin(a)),
                         lb, al);
            }
        }
    }

    if (rInner > 0 && centerLabel.isVisible())
        drawText(p, t.p(center.x, center.y), centerLabel, Qt::AlignHCenter | Qt::AlignVCenter);

    p.restore();
}

QVector<LegendItem> Pie::legendItems() const {
    QVector<LegendItem> out;
    for (const PieSlice& s : slices) {
        const QString text = s.legendText.isEmpty() ? s.label : s.legendText;
        if (text.isEmpty()) continue;
        LegendItem it;
        it.text = text;
        it.fill = FillStyle(s.fillColor.isValid() ? s.fillColor : QColor(Qt::gray));
        if (s.fill.isVisible()) {
            it.fill = s.fill;
            if (it.fill.color.alpha() == 0 && s.fillColor.isValid()) it.fill.color = s.fillColor;
        }
        it.line = LineStyle::none();
        out.append(it);
    }
    return out;
}

// ───────────────────────────── Coxcomb ─────────────────────────────

AxisLimits Coxcomb::axisLimits() const { return circleLimits(center, radius * 1.15); }

QVector<double> Coxcomb::sliceTotals() const {
    QVector<double> out;
    out.reserve(slices.size());
    for (int i = 0; i < slices.size(); ++i) {
        if (i < stacks.size()) {
            double sum = 0;
            for (double v : stacks[i])
                if (std::isfinite(v) && v > 0) sum += v;
            out.append(sum);
        } else {
            const double v = slices[i].value;
            out.append((std::isfinite(v) && v > 0) ? v : 0.0);
        }
    }
    return out;
}

void Coxcomb::render(RenderPack& rp) {
    if (slices.isEmpty()) return;

    const QVector<double> totals = sliceTotals();
    double maxValue = 0;
    for (double v : totals) maxValue = std::max(maxValue, v);
    if (!(maxValue > 0)) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double dir = dirOf(clockwise);
    const double slot = dir * 360.0 / slices.size();
    const double widthFrac = std::clamp(sectorWidthFraction, 0.05, 1.0);
    const double rHole = radius * std::clamp(donutFraction, 0.0, 0.9);
    const Palette palette(Palette::Category10);

    /// 累計值換算成半徑。
    ///
    /// 半徑正比於數值會讓面積變成平方關係，視覺上誇大大的值；取平方根才是
    /// 面積正比，這也是 Nightingale 原圖的做法。挖洞時從洞的邊緣起算，
    /// 不然小的值會整個藏進洞裡。
    auto radiusFor = [&](double value) {
        const double frac = std::clamp(value / maxValue, 0.0, 1.0);
        const double shaped = areaProportional ? std::sqrt(frac) : frac;
        return rHole + (radius - rHole) * shaped;
    };

    for (int i = 0; i < slices.size(); ++i) {
        if (!(totals[i] > 0)) continue;
        const PieSlice& s = slices.at(i);

        // 空隙平均分到兩側，扇形才會落在自己那一格的正中間
        const double slotStart = startAngle + slot * i;
        const double sweep = slot * widthFrac;
        const double start = slotStart + (slot - sweep) / 2;

        p.setPen(sliceBorder.isVisible() ? sliceBorder.pen() : QPen(Qt::NoPen));

        if (i < stacks.size() && !stacks[i].isEmpty()) {
            double cumulative = 0;
            for (int k = 0; k < stacks[i].size(); ++k) {
                const double v = stacks[i][k];
                if (!std::isfinite(v) || v <= 0) continue;
                const double rIn = radiusFor(cumulative);
                cumulative += v;
                const double rOut = radiusFor(cumulative);
                const QColor c = (k < stackColors.size() && stackColors[k].isValid())
                                     ? stackColors[k]
                                     : palette.color(k);
                p.setBrush(QBrush(c));
                p.drawPath(sectorPath(t, center, rOut, rIn, start, sweep));
            }
        } else {
            const QPainterPath wedge = sectorPath(t, center, radiusFor(totals[i]), rHole, start, sweep);
            p.setBrush(QBrush(s.fillColor.isValid() ? s.fillColor : QColor(Qt::gray)));
            p.drawPath(wedge);
            if (s.fill.isVisible()) {
                // 花紋疊在底色上
                p.setBrush(s.fill.brush(wedge.boundingRect()));
                p.drawPath(wedge);
            }
        }

        const double mid = start + sweep / 2;
        if (showLabels && !s.label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = s.label;
            lb.font = labelFont;
            if (labelsFollowValue) {
                // 尖端再往外推幾個像素
                const Coordinates tip = fromPolar(radiusFor(totals[i]), mid);
                const Coordinates unit = fromPolar(1.0, mid);
                const QPointF at = t.p(center.x + tip.x, center.y + tip.y) + QPointF(unit.x, -unit.y) * 6.0;
                if (radialLabels) {
                    const bool right = std::cos(mid * kPi / 180) >= 0;
                    lb.font.rotation = -mid + (right ? 0 : 180);
                    drawText(p, at, lb, (right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
                } else {
                    drawText(p, at, lb, alignmentForAngle(mid));
                }
            } else {
                const Coordinates lc = fromPolar(radius * labelDistance, mid);
                const QPointF at = t.p(center.x + lc.x, center.y + lc.y);
                if (radialLabels) {
                    const bool right = std::cos(mid * kPi / 180) >= 0;
                    lb.font.rotation = -mid + (right ? 0 : 180);
                    drawText(p, at, lb, (right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
                } else {
                    drawText(p, at, lb, alignmentForAngle(mid));
                }
            }
        }

        // 數值貼著扇形自己的尖端，不是貼在最外圈 ——
        // 貼最外圈的話，短的那幾格數字會離自己的扇形很遠，對不起來
        if (showValueLabels && valueLabelFont.color.alpha() > 0) {
            const Coordinates tip = fromPolar(radiusFor(totals[i]), mid);
            const Coordinates unit = fromPolar(1.0, mid);
            Label lb;
            lb.text = QString::number(totals[i], 'g', 3) + valueLabelSuffix;
            lb.font = valueLabelFont;
            drawText(p, t.p(center.x + tip.x, center.y + tip.y) +
                            QPointF(unit.x, -unit.y) * valueLabelGapPx,
                     lb, alignmentForAngle(mid));
        }
    }

    if (centerLabel.isVisible())
        drawText(p, t.p(center), centerLabel, Qt::AlignHCenter | Qt::AlignVCenter);

    p.restore();
}

QVector<LegendItem> Coxcomb::legendItems() const {
    QVector<LegendItem> out;
    // 堆疊模式下要列的是「段」而不是「格」：每一格都由同一組段疊成，
    // 列格的話圖例會和顏色對不起來
    if (!stacks.isEmpty() && !stackLabels.isEmpty()) {
        const Palette palette(Palette::Category10);
        for (int k = 0; k < stackLabels.size(); ++k) {
            if (stackLabels[k].isEmpty()) continue;
            LegendItem it;
            it.text = stackLabels[k];
            it.fill = FillStyle((k < stackColors.size() && stackColors[k].isValid())
                                    ? stackColors[k]
                                    : palette.color(k));
            it.line = LineStyle::none();
            out.append(it);
        }
        return out;
    }
    for (const PieSlice& s : slices) {
        const QString text = s.legendText.isEmpty() ? s.label : s.legendText;
        if (text.isEmpty()) continue;
        LegendItem it;
        it.text = text;
        it.fill = FillStyle(s.fillColor.isValid() ? s.fillColor : QColor(Qt::gray));
        if (s.fill.isVisible()) {
            it.fill = s.fill;
            if (it.fill.color.alpha() == 0 && s.fillColor.isValid()) it.fill.color = s.fillColor;
        }
        it.line = LineStyle::none();
        out.append(it);
    }
    return out;
}

// ───────────────────────────── Radar ─────────────────────────────

int Radar::dimensionCount() const {
    int n = static_cast<int>(axisLabels.size());
    for (const RadarSeries& s : series) n = std::max(n, static_cast<int>(s.values.size()));
    return n;
}

AxisLimits Radar::axisLimits() const { return circleLimits(center, radius * 1.25); }

void Radar::render(RenderPack& rp) {
    const int dims = dimensionCount();
    if (dims < 3 || series.isEmpty()) return;  // 少於三個維度畫不成多邊形

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // ── 正規化用的上限 ──
    QVector<double> scale(dims, 1.0);
    if (independentAxes) {
        for (int d = 0; d < dims; ++d) {
            double m = 0;
            for (const RadarSeries& s : series)
                if (d < s.values.size() && std::isfinite(s.values[d]))
                    m = std::max(m, std::abs(s.values[d]));
            scale[d] = (m > 0) ? m : 1.0;
        }
    } else {
        double m = maxValue;
        if (!(m > 0)) {
            for (const RadarSeries& s : series)
                for (double v : s.values)
                    if (std::isfinite(v)) m = std::max(m, std::abs(v));
        }
        scale.fill(m > 0 ? m : 1.0);
    }

    const double dir = dirOf(true);  // 雷達圖一律順時針，和一般讀法一致
    auto angleOf = [&](int d) { return 90.0 + dir * 360.0 * d / dims; };

    // ── 每一組資料 ──
    for (const RadarSeries& s : series) {
        QPolygonF poly;
        poly.reserve(dims);
        for (int d = 0; d < dims; ++d) {
            const double v = (d < s.values.size() && std::isfinite(s.values[d])) ? s.values[d] : 0.0;
            const double r = radius * std::clamp(v / scale[d], 0.0, 1.0);
            const Coordinates c = fromPolar(r, angleOf(d));
            poly.append(t.p(center.x + c.x, center.y + c.y));
        }
        if (poly.size() < 3) continue;

        const QColor line = s.lineColor.isValid() ? s.lineColor : QColor(Qt::darkGray);
        if (filled) {
            p.setBrush(QBrush(s.fillColor.isValid() ? s.fillColor : withAlpha(line, 60)));
        } else {
            p.setBrush(Qt::NoBrush);
        }
        p.setPen(LineStyle(line, lineWidth).pen());
        p.drawPolygon(poly);

        if (marker.isVisible()) {
            MarkerStyle m = marker;
            m.fillColor = line;
            m.outline.color = line;
            drawMarkers(p, QVector<QPointF>(poly.begin(), poly.end()), m);
        }
    }

    // ── 維度標籤 ──
    if (labelFont.color.alpha() > 0) {
        for (int d = 0; d < dims && d < axisLabels.size(); ++d) {
            const double deg = angleOf(d);
            const Coordinates c = fromPolar(radius * 1.06, deg);
            Label lb;
            lb.text = axisLabels.at(d);
            lb.font = labelFont;
            drawText(p, t.p(center.x + c.x, center.y + c.y), lb, alignmentForAngle(deg));
        }
    }
    p.restore();
}

QVector<LegendItem> Radar::legendItems() const {
    QVector<LegendItem> out;
    for (const RadarSeries& s : series) {
        if (s.legendText.isEmpty()) continue;
        LegendItem it;
        it.text = s.legendText;
        const QColor line = s.lineColor.isValid() ? s.lineColor : QColor(Qt::darkGray);
        it.line = LineStyle(line, lineWidth);
        if (filled) it.fill = FillStyle(s.fillColor.isValid() ? s.fillColor : withAlpha(line, 60));
        out.append(it);
    }
    return out;
}

// ───────────────────────────── RadialGauge ─────────────────────────────

AxisLimits RadialGauge::axisLimits() const { return circleLimits(center, outerRadius * 1.1); }

void RadialGauge::render(RenderPack& rp) {
    if (values.isEmpty() || !(maximum > 0)) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double dir = dirOf(clockwise);
    const int n = static_cast<int>(values.size());

    // Sequential：n 個同心環；Stacked：全部疊在最外圈的一個環上
    const int ringCount = (mode == Mode::Stacked) ? 1 : n;
    const double span = outerRadius - innerRadius;
    const double ringPitch = span / ringCount;
    const double ringThickness = ringPitch * (1.0 - std::clamp(gaugeSpacing, 0.0, 0.9));

    double stackedCursor = startAngle;
    const double fullSweep = std::clamp(sweepAngle, 1.0, 360.0);

    for (int i = 0; i < n; ++i) {
        const double v = values.at(i);
        if (!std::isfinite(v)) continue;

        // 第 0 個畫在最外圈，視覺上比較符合「第一項最重要」
        int ringIndex = (mode == Mode::Stacked) ? 0 : i;
        if (reverseOrder && mode != Mode::Stacked) ringIndex = ringCount - 1 - i;
        const double rOuter = outerRadius - ringIndex * ringPitch;
        const double rInner = rOuter - ringThickness;
        if (rInner <= 0) continue;

        // 負值往反方向畫
        const double frac = std::clamp(v / maximum, -1.0, 1.0);
        const double sweep = dir * frac * fullSweep;
        const double start = (mode == Mode::Sequential) ? startAngle : stackedCursor;

        const QColor color = (i < colors.size() && colors.at(i).isValid())
                                 ? colors.at(i)
                                 : QColor(Qt::gray);

        // 底環：只有 Sequential 畫，Stacked 的底環會被各段蓋掉沒意義
        if (showBackground && mode != Mode::Stacked) {
            p.setBrush(QBrush(backgroundColor));
            p.setPen(Qt::NoPen);
            p.drawPath(sectorPath(t, center, rOuter, rInner, startAngle, dir * fullSweep));
        }

        if (frac != 0) {
            p.setBrush(QBrush(color));
            p.setPen(Qt::NoPen);
            p.drawPath(sectorPath(t, center, rOuter, rInner, start, sweep));

            // 圓頭：在兩端各補一個半圓，弧看起來才不是切齊的方頭
            if (roundCaps) {
                const double rMid = (rOuter + rInner) / 2;
                const double capR = (rOuter - rInner) / 2;
                for (double deg : {start, start + sweep}) {
                    const Coordinates c = fromPolar(rMid, deg);
                    QPolygonF cap;
                    for (int k = 0; k <= 36; ++k) {
                        const Coordinates e = fromPolar(capR, k * 10.0);
                        cap.append(t.p(center.x + c.x + e.x, center.y + c.y + e.y));
                    }
                    p.drawPolygon(cap);
                }
            }
        }

        // ── 標籤 ──
        if (showLabels && labelFont.color.alpha() > 0) {
            const double rMid = (rOuter + rInner) / 2;
            if (i < labels.size() && !labels.at(i).isEmpty()) {
                Label lb;
                lb.text = labels.at(i);
                lb.font = labelFont;

                // 名稱畫在自己這段弧的中點。Stacked 模式每段的起點都不同，
                // 固定畫在 startAngle 會讓所有名稱疊在同一個點上。
                // 弧太短塞不下文字時改畫到弧的外側，否則白字會落到淺色底環上看不見。
                const double rPix = std::abs(t.px(center.x + rMid) - t.px(center.x));
                const double arcPix = std::abs(sweep) * kPi / 180.0 * rPix;
                const bool onArc = arcPix > measureText(lb).width() * 1.25;

                double deg;
                if (onArc) {
                    deg = start + sweep / 2;
                    // 深色填滿配白字、淺色填滿配黑字，兩種底色都讀得到
                    lb.font.color = (color.lightness() < 140) ? QColor(Qt::white) : QColor(Qt::black);
                } else {
                    // 沿著弧退到起點「之前」，留在自己這一環的底環上。
                    // 不能往徑向外推 —— 那會把文字推到隔壁那一環的弧上面。
                    const double offsetDeg =
                        (measureText(lb).width() / 2 + 8) / std::max(rPix, 1.0) * 180.0 / kPi;
                    deg = start - dir * offsetDeg;
                }
                // 半徑一律用環的中線，標籤才不會跨到相鄰的環
                const Coordinates c = fromPolar(rMid, deg);
                drawText(p, t.p(center.x + c.x, center.y + c.y), lb,
                         onArc ? (Qt::AlignHCenter | Qt::AlignVCenter) : alignmentForAngle(deg));
            }
            if (valueMode != ValueMode::None && frac != 0) {
                const Coordinates c =
                    fromPolar(rMid, start + sweep * std::clamp(valueLabelPosition, 0.0, 1.0));
                Label lb;
                lb.text = (valueMode == ValueMode::Percent)
                              ? QStringLiteral("%1%").arg(frac * 100, 0, 'f', 0)
                              : QString::number(v, 'g', 4);
                lb.font = valueFont;
                if (valueFontFraction > 0) {
                    // 環的像素厚度換成 point（螢幕上 1pt ≈ 1.33px）
                    const double thickPx = std::abs(t.px(center.x + ringThickness) - t.px(center.x));
                    lb.font.size = std::max(4.0, thickPx * valueFontFraction * 0.75);
                }
                drawText(p, t.p(center.x + c.x, center.y + c.y), lb,
                         Qt::AlignHCenter | Qt::AlignVCenter);
            }
        }

        if (mode != Mode::Sequential) stackedCursor += sweep;
    }
    p.restore();
}

QVector<LegendItem> RadialGauge::legendItems() const {
    QVector<LegendItem> out;
    for (int i = 0; i < labels.size(); ++i) {
        if (labels.at(i).isEmpty()) continue;
        LegendItem it;
        it.text = labels.at(i);
        it.fill = FillStyle((i < colors.size() && colors.at(i).isValid()) ? colors.at(i)
                                                                         : QColor(Qt::gray));
        it.line = LineStyle::none();
        out.append(it);
    }
    return out;
}

}  // namespace qtplot

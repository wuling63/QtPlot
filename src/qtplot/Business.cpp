#include "Business.h"

#include <QCoreApplication>
#include <QPolygonF>

namespace qtplot {

// ───────────────────────── Funnel ─────────────────────────

QVector<FunnelStage> Funnel::fromValues(const QVector<double>& values, const QStringList& labels,
                                        const Palette& palette) {
    QVector<FunnelStage> out;
    out.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        FunnelStage s;
        s.value = values[i];
        if (i < labels.size()) s.label = labels[i];
        s.fillColor = palette.color(i);
        out.append(s);
    }
    return out;
}

AxisLimits Funnel::axisLimits() const {
    if (stages.isEmpty()) return AxisLimits();
    // 有說明文字時右邊要留一倍寬的位置給它
    bool hasNotes = false;
    for (const FunnelStage& st : stages) hasNotes = hasNotes || !st.description.isEmpty();
    if (hasNotes)
        return AxisLimits(CoordinateRange(extent.left, extent.right + extent.width()), extent.yRange());
    return AxisLimits(extent.xRange(), extent.yRange());
}

void Funnel::render(RenderPack& rp) {
    const int n = static_cast<int>(stages.size());
    if (n == 0) return;

    double maxValue = 0;
    for (const FunnelStage& s : stages)
        if (std::isfinite(s.value)) maxValue = std::max(maxValue, std::abs(s.value));
    if (widthMode == WidthMode::Value && !(maxValue > 0)) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double cx = (extent.left + extent.right) / 2;
    const double halfSpan = (extent.right - extent.left) / 2;

    /// 第 k 條水平分界線（k = 0..n）上的半寬，以 halfSpan 的比例表示。
    ///
    /// 用「分界線」而不是「每段一個寬度」，相鄰兩段才會嚴絲合縫地接上 ——
    /// 各段各自算寬度的話，接縫處會差一截，看起來像斷掉的階梯。
    auto halfWidthAt = [&](int k) {
        if (widthMode == WidthMode::Linear) {
            // 線性收斂到一點：第 0 條最寬，第 n 條為 0
            return 1.0 - static_cast<double>(k) / n;
        }
        // 依數值：分界線取相鄰兩段的值，兩端則沿用最靠近的那一段
        const double a = stages[std::clamp(k - 1, 0, n - 1)].value;
        const double b = stages[std::clamp(k, 0, n - 1)].value;
        const double v = (k == 0) ? b : (k == n ? a : (a + b) / 2);
        return std::abs(v) / maxValue;
    };

    const double stageHeight = (extent.top - extent.bottom) / n;
    const double gap = stageHeight * std::clamp(gapFraction, 0.0, 0.5);

    for (int i = 0; i < n; ++i) {
        const FunnelStage& st = stages[i];
        // inverted 時最寬的在下面，等於把整個圖上下翻過來
        const int kTop = inverted ? (n - i) : i;
        const int kBottom = inverted ? (n - i - 1) : (i + 1);

        const double yTop = extent.top - stageHeight * i - gap / 2;
        const double yBottom = extent.top - stageHeight * (i + 1) + gap / 2;
        const double hwTop = halfSpan * halfWidthAt(kTop);
        const double hwBottom = halfSpan * halfWidthAt(kBottom);

        QPolygonF quad;
        quad << t.p(cx - hwTop, yTop) << t.p(cx + hwTop, yTop) << t.p(cx + hwBottom, yBottom)
             << t.p(cx - hwBottom, yBottom);
        const QColor fill = st.fillColor.isValid() ? st.fillColor : QColor(120, 120, 120);

        double labelShift = 0;
        if (cone3D) {
            // 像素空間裡的半寬與橢圓高
            const QPointF topC = t.p(cx, yTop), botC = t.p(cx, yBottom);
            const double hwT = std::abs(quad[1].x() - quad[0].x()) / 2;
            const double hwB = std::abs(quad[2].x() - quad[3].x()) / 2;
            const double ratio = std::clamp(ellipseRatio, 0.0, 0.5);
            const double ryT = hwT * ratio, ryB = hwB * ratio;

            // 身體：左邊往下、沿底緣橢圓的前半圈、右邊往上
            QPolygonF body;
            body << quad[0];
            for (int k = 0; k <= 40; ++k) {
                const double a = kPi * k / 40;
                body << QPointF(botC.x() - hwB * std::cos(a), botC.y() + ryB * std::sin(a));
            }
            body << quad[1];
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPolygon(body);

            if (cap == CapStyle::Hollow) {
                p.setBrush(fill.lighter(125));
                p.drawEllipse(topC, hwT, ryT);
                p.setBrush(fill.darker(220));
                p.drawEllipse(QPointF(topC.x(), topC.y() + ryT * 0.12), hwT * 0.94, ryT * 0.8);
            } else {
                p.setBrush(fill.lighter(120));
                p.drawEllipse(topC, hwT, ryT);
            }
            labelShift = ryT / 2;
        } else {
            p.setBrush(QBrush(fill));
            p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
            p.drawPolygon(quad);
        }

        if (!st.label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = showValues ? QStringLiteral("%1  %2").arg(st.label).arg(st.value)
                                 : st.label;
            lb.font = labelFont;
            drawText(p, t.p(cx, (yTop + yBottom) / 2) + QPointF(0, labelShift), lb,
                     Qt::AlignHCenter | Qt::AlignVCenter);
        }

        if (!st.description.isEmpty()) {
            Label note;
            note.text = st.description;
            note.font = descriptionFont;
            drawText(p, t.p(extent.right + extent.width() * descriptionGap, (yTop + yBottom) / 2), note,
                     Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> Funnel::legendItems() const {
    QVector<LegendItem> items;
    for (const FunnelStage& s : stages) {
        if (s.label.isEmpty()) continue;
        LegendItem it;
        it.text = s.label;
        it.fill = FillStyle(s.fillColor);
        items.append(it);
    }
    return items;
}

// ───────────────────────── Waterfall ─────────────────────────

QVector<double> Waterfall::cumulative() const {
    QVector<double> out;
    out.reserve(steps.size());
    double running = 0;
    for (const WaterfallStep& s : steps) {
        // isTotal 只影響「從哪裡畫」，不影響算術。兩者綁在一起的話，
        // 期初餘額（既是總計、又要讓累計從它開始）就寫不出來。
        if (std::isfinite(s.delta)) running += s.delta;
        out.append(running);
    }
    return out;
}

AxisLimits Waterfall::axisLimits() const {
    AxisLimits lim;
    if (steps.isEmpty()) return lim;

    const QVector<double> cum = cumulative();
    lim.expand({-0.5 - barSize / 2, 0});  // 瀑布圖一定要看得到 0 這條基準
    for (int i = 0; i < steps.size(); ++i) {
        const double top = cum[i];
        const double base = steps[i].isTotal ? 0.0 : (i == 0 ? 0.0 : cum[i - 1]);
        lim.expand({i - barSize / 2, top});
        lim.expand({i + barSize / 2, base});
    }
    return lim;
}

void Waterfall::render(RenderPack& rp) {
    if (steps.isEmpty()) return;
    const QVector<double> cum = cumulative();
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 連接線：從前一根的頂端拉到這一根的起點
    if (connectorLine.isVisible() && steps.size() > 1) {
        p.setPen(connectorLine.pen());
        for (int i = 0; i + 1 < steps.size(); ++i) {
            const double y = cum[i];
            p.drawLine(t.p(i - barSize / 2, y), t.p(i + 1 + barSize / 2, y));
        }
    }

    for (int i = 0; i < steps.size(); ++i) {
        const WaterfallStep& s = steps[i];
        const double top = cum[i];
        const double base = s.isTotal ? 0.0 : (i == 0 ? 0.0 : cum[i - 1]);

        QColor c = s.fillColor;
        if (!c.isValid()) c = s.isTotal ? totalColor : (top >= base ? riseColor : fallColor);

        const QRectF r(t.p(i - barSize / 2, std::max(top, base)),
                       t.p(i + barSize / 2, std::min(top, base)));
        p.setBrush(QBrush(c));
        p.setPen(borderLine.isVisible() ? borderLine.pen() : QPen(Qt::NoPen));
        p.drawRect(r);

        const bool up = top >= base;
        if (showValueLabels && valueFont.color.alpha() > 0) {
            Label lb;
            // 總計那根印累計值，增減那幾根印變化量（正數補上 + 號）
            const double shown = s.isTotal ? top : s.delta;
            lb.text = (!s.isTotal && shown > 0) ? QStringLiteral("+%1").arg(shown)
                                                : QStringLiteral("%1").arg(shown);
            lb.font = valueFont;

            // 印在長條裡面要先確認塞得下。塞不下還硬印的話，文字會從
            // 細長條的上下溢出去，壓到相鄰的長條。
            const bool fits = valueLabelInside &&
                              measureText(lb).height() + 4 <= std::abs(r.height());
            if (fits) {
                drawText(p, QPointF(r.center().x(), r.center().y()), lb,
                         Qt::AlignHCenter | Qt::AlignVCenter);
            } else {
                // 放在長條外側：往上長的放上面、往下長的放下面
                drawText(p, t.p(i, up ? std::max(top, base) : std::min(top, base)), lb,
                         Qt::AlignHCenter | (up ? Qt::AlignBottom : Qt::AlignTop));
            }
        }

        // 累計值：永遠標在長條的「頂端」上方，和增減量分開看
        if (showCumulativeLabels && cumulativeFont.color.alpha() > 0) {
            Label lb;
            lb.text = QStringLiteral("%1").arg(top);
            lb.font = cumulativeFont;
            drawText(p, t.p(i, std::max(top, base)), lb, Qt::AlignHCenter | Qt::AlignBottom);
        }
    }
    p.restore();
}

// ───────────────────────── Bullet ─────────────────────────

AxisLimits Bullet::axisLimits() const {
    AxisLimits lim;
    if (rows.isEmpty()) return lim;

    for (int i = 0; i < rows.size(); ++i) {
        const BulletRow& r = rows[i];
        lim.expand({0, i - rowHeight / 2});
        lim.expand({0, i + rowHeight / 2});
        lim.expand({r.value, static_cast<double>(i)});
        lim.expand({r.target, static_cast<double>(i)});
        for (double b : r.ranges) lim.expand({b, static_cast<double>(i)});
    }
    return lim;
}

void Bullet::render(RenderPack& rp) {
    if (rows.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(Qt::NoPen);

    for (int i = 0; i < rows.size(); ++i) {
        const BulletRow& row = rows[i];
        const double yTop = i + rowHeight / 2;
        const double yBottom = i - rowHeight / 2;

        // 背景分級帶：由外而內畫，愈裡面愈深。
        // 反過來畫的話，後畫的淺色會把深色整個蓋掉。
        QVector<double> bounds = row.ranges;
        std::sort(bounds.begin(), bounds.end());
        for (int k = static_cast<int>(bounds.size()) - 1; k >= 0; --k) {
            const double shade = 1.0 - static_cast<double>(k + 1) / (bounds.size() + 1);
            QColor c = rangeColor.lighter(static_cast<int>(100 + 90 * (1.0 - shade)));
            p.setBrush(QBrush(c));
            p.drawRect(QRectF(t.p(0, yTop), t.p(bounds[k], yBottom)));
        }

        // 實際值：中間一根比較細的長條
        const double vh = rowHeight * std::clamp(valueHeightFraction, 0.05, 1.0) / 2;
        p.setBrush(QBrush(row.valueColor));
        p.drawRect(QRectF(t.p(0, i + vh), t.p(row.value, i - vh)));

        // 目標：一條垂直記號
        if (targetLine.isVisible()) {
            const double th = rowHeight * std::clamp(targetHeightFraction, 0.05, 1.0) / 2;
            p.setPen(targetLine.pen());
            p.drawLine(t.p(row.target, i + th), t.p(row.target, i - th));
            p.setPen(Qt::NoPen);
        }
    }
    p.restore();
}


// ───────────────────────── Dumbbell ─────────────────────────

AxisLimits Dumbbell::axisLimits() const {
    AxisLimits lim;
    for (int i = 0; i < rows.size(); ++i) {
        const DumbbellRow& r = rows[i];
        const double cat = i;
        if (horizontal) {
            lim.expand({r.from, cat});
            lim.expand({r.to, cat});
        } else {
            lim.expand({cat, r.from});
            lim.expand({cat, r.to});
        }
    }
    // 類別軸兩端各留半格，端點的標記才不會被切掉
    if (lim.hasData() && !rows.isEmpty()) {
        if (horizontal) {
            lim.expand({lim.x.min, -0.5});
            lim.expand({lim.x.max, rows.size() - 0.5});
        } else {
            lim.expand({-0.5, lim.y.min});
            lim.expand({rows.size() - 0.5, lim.y.max});
        }
    }
    return lim;
}

void Dumbbell::render(RenderPack& rp) {
    if (rows.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    auto at = [&](double value, int index) {
        return horizontal ? t.p(value, index) : t.p(index, value);
    };

    // 連線先畫，兩端的標記才蓋在上面把線頭藏起來
    if (connector.isVisible()) {
        p.setPen(connector.pen());
        for (int i = 0; i < rows.size(); ++i) p.drawLine(at(rows[i].from, i), at(rows[i].to, i));
    }

    for (int i = 0; i < rows.size(); ++i) {
        MarkerStyle mf = fromMarker, mt = toMarker;
        if (rows[i].fromColor.isValid()) mf.fillColor = rows[i].fromColor;
        if (rows[i].toColor.isValid()) mt.fillColor = rows[i].toColor;
        drawMarker(p, at(rows[i].from, i), mf);
        drawMarker(p, at(rows[i].to, i), mt);
    }

    if (showValues && valueFont.color.alpha() > 0) {
        for (int i = 0; i < rows.size(); ++i) {
            // 數字放在「外側」：起點小就放左邊、終點大就放右邊。
            // 固定放同一側的話，兩端靠得近時數字會疊在一起。
            const bool fromIsLower = rows[i].from <= rows[i].to;
            auto place = [&](double value, bool lower) {
                Label lb;
                lb.text = QString::number(value, 'g', 4);
                lb.font = valueFont;
                const QPointF base = at(value, i);
                if (horizontal)
                    drawText(p, base + QPointF(lower ? -valueGapPx : valueGapPx, 0), lb,
                             (lower ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
                else
                    drawText(p, base + QPointF(0, lower ? valueGapPx : -valueGapPx), lb,
                             Qt::AlignHCenter | (lower ? Qt::AlignTop : Qt::AlignBottom));
            };
            place(rows[i].from, fromIsLower);
            place(rows[i].to, !fromIsLower);
        }
    }

    if (showDelta && deltaFont.color.alpha() > 0) {
        for (int i = 0; i < rows.size(); ++i) {
            const double d = rows[i].to - rows[i].from;
            Label lb;
            lb.text = (d >= 0) ? QStringLiteral("+%1").arg(d, 0, 'g', 3)
                               : QStringLiteral("%1").arg(d, 0, 'g', 3);
            lb.font = deltaFont;
            const QPointF mid = at((rows[i].from + rows[i].to) / 2, i);
            drawText(p, mid + QPointF(0, horizontal ? -valueGapPx : 0), lb,
                     Qt::AlignHCenter | (horizontal ? Qt::AlignBottom : Qt::AlignVCenter));
        }
    }
    p.restore();
}

QVector<LegendItem> Dumbbell::legendItems() const {
    QVector<LegendItem> items;
    if (!fromLegendText.isEmpty()) {
        LegendItem it;
        it.text = fromLegendText;
        it.marker = fromMarker;
        it.line = LineStyle::none();
        items.append(it);
    }
    if (!toLegendText.isEmpty()) {
        LegendItem it;
        it.text = toLegendText;
        it.marker = toMarker;
        it.line = LineStyle::none();
        items.append(it);
    }
    return items;
}

// ───────────────────────── Gantt ─────────────────────────

int Gantt::rowCount() const {
    int maxRow = -1;
    for (const GanttTask& t : tasks) maxRow = std::max(maxRow, t.row);
    return maxRow + 1;
}

AxisLimits Gantt::axisLimits() const {
    AxisLimits lim;
    if (tasks.isEmpty()) return lim;
    for (const GanttTask& t : tasks) {
        lim.expand({t.start, t.row - barHeight / 2});
        lim.expand({t.end, t.row + barHeight / 2});
    }
    return lim;
}

void Gantt::render(RenderPack& rp) {
    if (tasks.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    auto barRect = [&](const GanttTask& task) {
        return QRectF(t.p(std::min(task.start, task.end), task.row + barHeight / 2),
                      t.p(std::max(task.start, task.end), task.row - barHeight / 2));
    };

    // 先畫相依箭頭，長條才蓋在上面 —— 反過來的話箭頭會橫過長條中間
    if (dependencyLine.isVisible()) {
        p.setPen(dependencyLine.pen());
        p.setBrush(QBrush(dependencyLine.color));
        for (int i = 0; i < tasks.size(); ++i) {
            const GanttTask& to = tasks.at(i);
            for (int from : to.dependsOn) {
                if (from < 0 || from >= tasks.size()) continue;
                const QRectF a = barRect(tasks.at(from));
                const QRectF b = barRect(to);
                // 三段折線：從前一項的右緣出來、垂直走到這一項的那一列、再接進左緣
                const QPointF p1(a.right(), a.center().y());
                const QPointF p3(b.left(), b.center().y());
                const double midX = std::max(p1.x() + 6, p3.x() - 12);
                p.setPen(dependencyLine.pen());
                p.drawLine(p1, QPointF(midX, p1.y()));
                p.drawLine(QPointF(midX, p1.y()), QPointF(midX, p3.y()));
                p.drawLine(QPointF(midX, p3.y()), p3);

                QPolygonF head;
                head << p3 << QPointF(p3.x() - arrowHeadPx, p3.y() - arrowHeadPx / 2)
                     << QPointF(p3.x() - arrowHeadPx, p3.y() + arrowHeadPx / 2);
                p.setPen(Qt::NoPen);
                p.drawPolygon(head);
            }
        }
    }

    for (const GanttTask& task : tasks) {
        const QRectF r = barRect(task);
        const QColor fill = task.fillColor.isValid() ? task.fillColor : QColor(66, 133, 244);

        p.setBrush(QBrush(fill));
        p.setPen(borderLine.isVisible() ? borderLine.pen() : QPen(Qt::NoPen));
        if (cornerRadiusPx > 0) p.drawRoundedRect(r, cornerRadiusPx, cornerRadiusPx);
        else                    p.drawRect(r);

        // 完成度：長條左側疊一段深色。用裁切而不是另外畫一個圓角矩形，
        // 右邊的圓角才不會跑到長條中間。
        const double progress = std::clamp(task.progress, 0.0, 1.0);
        if (progress > 0) {
            p.save();
            QRectF clip = r;
            clip.setWidth(r.width() * progress);
            p.setClipRect(clip);
            p.setBrush(QBrush(fill.darker(std::max(100, progressDarken))));
            p.setPen(Qt::NoPen);
            if (cornerRadiusPx > 0) p.drawRoundedRect(r, cornerRadiusPx, cornerRadiusPx);
            else                    p.drawRect(r);
            p.restore();
        }

        if (showTaskLabels && !task.label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = task.label;
            lb.font = labelFont;
            // 長條塞得下就放裡面，塞不下就擺到右邊，不要讓文字溢出去壓到別的列
            const double textWidth = measureText(lb).width();
            if (textWidth + 8 <= r.width())
                drawText(p, r.center(), lb, Qt::AlignHCenter | Qt::AlignVCenter);
            else
                drawText(p, QPointF(r.right() + 5, r.center().y()), lb,
                         Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    p.restore();
}

// ───────────────────────── Mosaic ─────────────────────────

QVector<double> Mosaic::columnTotals() const {
    QVector<double> out;
    out.reserve(columns.size());
    for (const MosaicColumn& c : columns) {
        double sum = 0;
        for (double v : c.values)
            if (std::isfinite(v) && v > 0) sum += v;
        out.append(sum);
    }
    return out;
}

double Mosaic::grandTotal() const {
    double sum = 0;
    for (double v : columnTotals()) sum += v;
    return sum;
}

AxisLimits Mosaic::axisLimits() const {
    if (columns.isEmpty()) return AxisLimits();
    return AxisLimits(extent.xRange(), extent.yRange());
}

void Mosaic::render(RenderPack& rp) {
    const QVector<double> totals = columnTotals();
    const double grand = grandTotal();
    if (!(grand > 0)) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const Palette palette(Palette::Category10);
    const double totalWidth = extent.right - extent.left;
    const double gap = totalWidth * std::clamp(columnGapFraction, 0.0, 0.05);
    // 間隔吃掉的寬度要先扣掉，剩下的才按比例分給各欄 ——
    // 不扣的話欄寬的比例會被間隔稀釋，寬度就不再正比於總量
    const double usable = totalWidth - gap * std::max(0, static_cast<int>(columns.size()) - 1);

    double cursorX = extent.left;
    for (int c = 0; c < columns.size(); ++c) {
        if (!(totals[c] > 0)) continue;
        const double w = usable * (totals[c] / grand);
        const double left = cursorX;
        const double right = left + w;
        cursorX = right + gap;

        // 欄內由下往上堆。各段的高度是它在這一欄裡的比例，所以每欄都填滿整個高度。
        double cursorY = extent.bottom;
        for (int k = 0; k < columns[c].values.size(); ++k) {
            const double v = columns[c].values[k];
            if (!std::isfinite(v) || v <= 0) continue;
            const double h = (extent.top - extent.bottom) * (v / totals[c]);
            const QRectF box(t.p(left, cursorY + h), t.p(right, cursorY));
            cursorY += h;

            const QColor fill = (k < segmentColors.size() && segmentColors[k].isValid())
                                    ? segmentColors[k]
                                    : palette.color(k);
            p.setBrush(QBrush(fill));
            p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
            p.drawRect(box);

            // 方塊太小就不標：硬塞的話文字會溢出到相鄰的方塊上
            if (labelFont.color.alpha() > 0 && k < columns[c].segmentLabels.size() &&
                std::abs(box.width() * box.height()) >= minLabelAreaPx) {
                Label lb;
                lb.text = columns[c].segmentLabels[k];
                lb.font = labelFont;
                if (measureText(lb).width() + 6 <= std::abs(box.width()))
                    drawText(p, box.center(), lb, Qt::AlignHCenter | Qt::AlignVCenter);
            }
        }

        if (columnLabelFont.color.alpha() == 0) continue;
        const double midX = (left + right) / 2;
        if (showColumnLabels && !columns[c].label.isEmpty()) {
            Label lb;
            lb.text = columns[c].label;
            lb.font = columnLabelFont;
            drawText(p, t.p(midX, extent.bottom) + QPointF(0, 5), lb,
                     Qt::AlignHCenter | Qt::AlignTop);
        }
        if (showColumnTotals) {
            Label lb;
            // 用定點而不是 'g'：'g' 在位數多的時候會跳成科學記號（2.951e+05），
            // 金額印成那樣沒人看得懂
            lb.text = QStringLiteral("%1\n%2%")
                          .arg(totals[c], 0, 'f', 0)
                          .arg(totals[c] / grand * 100, 0, 'f', 0);
            lb.font = columnLabelFont;
            // 欄太窄就只留百分比，不然相鄰兩欄的數字會疊在一起
            if (measureText(lb).width() > std::abs(t.px(right) - t.px(left)))
                lb.text = QStringLiteral("%1%").arg(totals[c] / grand * 100, 0, 'f', 0);
            drawText(p, t.p(midX, extent.top) + QPointF(0, -5), lb,
                     Qt::AlignHCenter | Qt::AlignBottom);
        }
    }
    p.restore();
}

QVector<LegendItem> Mosaic::legendItems() const {
    QVector<LegendItem> items;
    const Palette palette(Palette::Category10);
    for (int k = 0; k < segmentNames.size(); ++k) {
        if (segmentNames[k].isEmpty()) continue;
        LegendItem it;
        it.text = segmentNames[k];
        it.fill = FillStyle((k < segmentColors.size() && segmentColors[k].isValid())
                                ? segmentColors[k]
                                : palette.color(k));
        items.append(it);
    }
    return items;
}

// ───────────────────────── Venn ─────────────────────────

QVector<Coordinates> Venn::circleCenters() const {
    QVector<Coordinates> out;
    const int n = static_cast<int>(sets.size());
    if (n == 2) {
        // 兩個圓左右擺，圓心距取半徑 —— 交集大約佔每個圓的三分之一，
        // 這是示意圖最常見的比例
        const double d = radius * 1.0;
        out.append({center.x - d / 2, center.y});
        out.append({center.x + d / 2, center.y});
    } else if (n >= 3) {
        // 三個圓排成正三角形，兩兩的圓心距相同
        const double d = radius * 1.1;
        for (int i = 0; i < 3; ++i) {
            // 從正上方開始，逆時針。這樣 A 在上、B 在左下、C 在右下
            const double deg = 90.0 + 120.0 * i;
            const Coordinates off = fromPolar(d / std::sqrt(3.0), deg);
            out.append({center.x + off.x, center.y + off.y});
        }
    } else if (n == 1) {
        out.append(center);
    }
    return out;
}

Coordinates Venn::regionAnchor(int mask) const {
    const QVector<Coordinates> cs = circleCenters();
    const int n = static_cast<int>(cs.size());
    if (n == 0 || mask == 0) return center;

    QVector<int> members;
    for (int i = 0; i < n; ++i)
        if (mask & (1 << i)) members.append(i);
    if (members.isEmpty()) return center;

    // 全部圓的交集就在正中間
    if (members.size() == n) return center;

    // 參與的圓取形心，再往「沒參與的那些圓」的反方向推開一點，
    // 文字才會落在真正只屬於這個區塊的那一塊地方
    Coordinates sum{0, 0};
    for (int i : members) {
        sum.x += cs[i].x;
        sum.y += cs[i].y;
    }
    Coordinates anchor{sum.x / members.size(), sum.y / members.size()};

    Coordinates away{0, 0};
    int others = 0;
    for (int i = 0; i < n; ++i) {
        if (mask & (1 << i)) continue;
        away.x += cs[i].x;
        away.y += cs[i].y;
        ++others;
    }
    if (others > 0) {
        away.x /= others;
        away.y /= others;
        const double dx = anchor.x - away.x, dy = anchor.y - away.y;
        const double len = std::hypot(dx, dy);
        if (len > 1e-9) {
            // 推的距離取圓半徑的一半；再多就跑到圓外面去了
            const double push = radius * 0.5;
            anchor.x += dx / len * push;
            anchor.y += dy / len * push;
        }
    }
    return anchor;
}

AxisLimits Venn::axisLimits() const {
    if (sets.isEmpty()) return AxisLimits();
    AxisLimits lim;
    for (const Coordinates& c : circleCenters()) {
        lim.expand({c.x - radius, c.y - radius});
        lim.expand({c.x + radius, c.y + radius});
    }
    // 集合名稱畫在圓外，要留空間
    const double pad = radius * 0.22;
    lim.expand({lim.x.min - pad, lim.y.min - pad});
    lim.expand({lim.x.max + pad, lim.y.max + pad});
    return lim;
}

void Venn::render(RenderPack& rp) {
    const QVector<Coordinates> cs = circleCenters();
    if (cs.isEmpty()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 圓用半透明疊在一起：重疊處自然會變深，不必分別算交集的多邊形。
    // 真的去算交集區域要做圓弧的布林運算，為了一張示意圖不值得。
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (int i = 0; i < cs.size(); ++i) {
        QColor c = sets[i].fillColor.isValid() ? sets[i].fillColor : QColor(120, 120, 120);
        c.setAlphaF(std::clamp(fillOpacity, 0.0, 1.0));
        p.setBrush(QBrush(c));
        p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
        const QPointF a = t.p(cs[i].x - radius, cs[i].y + radius);
        const QPointF b = t.p(cs[i].x + radius, cs[i].y - radius);
        p.drawEllipse(QRectF(a, b));
    }

    // 區塊的數字
    if (countLabelFont.color.alpha() > 0) {
        for (auto it = regionLabels.constBegin(); it != regionLabels.constEnd(); ++it) {
            if (it.value().isEmpty()) continue;
            Label lb;
            lb.text = it.value();
            lb.font = countLabelFont;
            drawText(p, t.p(regionAnchor(it.key())), lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }

    // 集合名稱放在圓外側（沿著「圓心離整體中心」的方向推出去）
    if (setLabelFont.color.alpha() > 0) {
        for (int i = 0; i < cs.size(); ++i) {
            if (sets[i].label.isEmpty()) continue;
            double dx = cs[i].x - center.x, dy = cs[i].y - center.y;
            const double len = std::hypot(dx, dy);
            if (len < 1e-9) { dx = 0; dy = 1; }
            else            { dx /= len; dy /= len; }
            Label lb;
            lb.text = sets[i].label;
            lb.font = setLabelFont;
            drawText(p, t.p(cs[i].x + dx * radius * 0.92, cs[i].y + dy * radius * 0.92), lb,
                     Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> Venn::legendItems() const {
    QVector<LegendItem> items;
    for (const VennSet& s : sets) {
        if (s.label.isEmpty()) continue;
        LegendItem it;
        it.text = s.label;
        it.fill = FillStyle(s.fillColor);
        items.append(it);
    }
    return items;
}


// ───────────────────────── 魚骨圖 ─────────────────────────

QVector<FishboneCategory> Fishbone::sixM() {
    return {{QCoreApplication::translate("qtplot", "人", "fishbone 6M: Man"), {}, QColor()},
            {QCoreApplication::translate("qtplot", "機", "fishbone 6M: Machine"), {}, QColor()},
            {QCoreApplication::translate("qtplot", "料", "fishbone 6M: Material"), {}, QColor()},
            {QCoreApplication::translate("qtplot", "法", "fishbone 6M: Method"), {}, QColor()},
            {QCoreApplication::translate("qtplot", "環", "fishbone 6M: Environment"), {}, QColor()},
            {QCoreApplication::translate("qtplot", "測", "fishbone 6M: Measurement"), {}, QColor()}};
}

AxisLimits Fishbone::axisLimits() const {
    return AxisLimits(extent.left, extent.right, extent.bottom, extent.top);
}

namespace {

/// 畫一個有外框的文字方塊，回傳它佔的像素矩形。
QRectF drawBoxedText(QPainter& p, QPointF center, const Label& lb, const FillStyle& fill,
                     const LineStyle& border, double padX, double padY) {
    const QSizeF sz = measureText(lb);
    const QRectF box(center.x() - sz.width() / 2 - padX, center.y() - sz.height() / 2 - padY,
                     sz.width() + 2 * padX, sz.height() + 2 * padY);
    p.setBrush(fill.brush(box));
    p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
    p.drawRect(box);
    drawText(p, center, lb, Qt::AlignHCenter | Qt::AlignVCenter);
    return box;
}

}  // namespace

void Fishbone::render(RenderPack& rp) {
    if (categories.isEmpty() && effect.isEmpty()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double scale = rp.scaleFactor;
    auto pen = [&](const LineStyle& ls) {
        QPen q = ls.pen();
        q.setWidthF(q.widthF() * scale);
        return q;
    };

    const QRectF box = t.pixelRect(extent);
    const double headWidth = box.width() * std::clamp(headFraction, 0.05, 0.5);
    const double spineRight = box.right() - headWidth;
    const double spineLeft = box.left();

    // ── 把類別分到上下兩側 ──
    QVector<int> upper, lower;
    for (int i = 0; i < categories.size(); ++i) {
        if (alternateSides && (i % 2 == 1))
            lower.append(i);
        else
            upper.append(i);
    }

    // 脊椎的高度要看骨頭排在哪幾側：兩側都有才擺中間。
    // 全部排在上面卻還把脊椎放中間的話，下半張圖整個空著，
    // 骨頭卻只分到一半的高度，白白擠成一團。
    const bool bothSides = !upper.isEmpty() && !lower.isEmpty();
    const double margin = box.height() * 0.06;
    const double yc = bothSides ? box.center().y()
                                : (lower.isEmpty() ? box.bottom() - margin : box.top() + margin);
    const double availableHeight = bothSides ? box.height() / 2 : box.height() - margin;

    // ── 脊椎 ──
    p.setPen(pen(spine));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(spineLeft, yc), QPointF(spineRight, yc));

    // ── 魚頭 ──
    if (!effect.isEmpty()) {
        Label lb;
        lb.text = effect;
        lb.font = effectFont;
        lb.font.size *= scale;
        drawBoxedText(p, QPointF(spineRight + headWidth / 2, yc), lb, headFill, headBorder,
                      7 * scale, 5 * scale);
    }

    if (categories.isEmpty()) {
        p.restore();
        return;
    }

    const double theta = std::clamp(boneAngleDegrees, 15.0, 85.0) * kPi / 180.0;
    const double boneLen =
        availableHeight * std::clamp(boneLengthFraction, 0.2, 1.0) / std::sin(theta);
    const double causeLen = box.width() * std::clamp(causeLineFraction, 0.02, 0.3);

    // 骨頭往左斜，所以最左邊那一根的尖端會再往左伸出 boneLen·cos θ；
    // 原因的橫線還要再往左拉 causeLen，文字又接在橫線的左端。
    // 三段都算進去，最左邊那一根的文字才不會被切掉 ——
    // 只算骨頭和橫線的話，長一點的原因名會直接消失在圖外。
    double widestCausePx = 0;
    for (const FishboneCategory& c : categories) {
        for (const FishboneCause& cause : c.causes) {
            Label lb;
            lb.text = cause.text;
            lb.font = causeFont;
            lb.font.size *= scale;
            widestCausePx = std::max(widestCausePx, measureText(lb).width());
        }
    }
    const double tipOverhang = boneLen * std::cos(theta);
    const double usableLeft = spineLeft + tipOverhang + causeLen + widestCausePx + 6 * scale;
    const double usableRight = spineRight - box.width() * 0.03;

    auto placeSide = [&](const QVector<int>& side, int sign) {
        if (side.isEmpty()) return;
        const int n = side.size();
        // 由右往左排：第一個類別離魚頭最近，和閱讀順序一致
        const double span = usableRight - usableLeft;
        for (int k = 0; k < n; ++k) {
            const double f = (n == 1) ? 0.5 : static_cast<double>(k) / (n - 1);
            const double ax = usableRight - f * span;
            const QPointF anchor(ax, yc);
            // 螢幕的 y 向下，所以 sign = +1（上方）要減
            const QPointF dir(-std::cos(theta), -sign * std::sin(theta));
            const QPointF tip = anchor + boneLen * dir;

            const int idx = side[k];
            const FishboneCategory& cat = categories[idx];
            const QColor color = cat.color.isValid() ? cat.color : palette.color(idx);

            LineStyle bl = boneLine;
            bl.color = color;
            p.setPen(pen(bl));
            p.setBrush(Qt::NoBrush);
            p.drawLine(anchor, tip);

            // 類別名稱
            if (!cat.name.isEmpty()) {
                Label lb;
                lb.text = cat.name;
                lb.font = categoryFont;
                lb.font.size *= scale;
                const QPointF at = tip + QPointF(0, -sign * (measureText(lb).height() / 2 + 4 * scale));
                if (boxCategoryLabels) {
                    FillStyle fill(QColor(color.red(), color.green(), color.blue(), 45));
                    LineStyle border(color, 1.2);
                    drawBoxedText(p, at, lb, fill, border, 6 * scale, 3 * scale);
                } else {
                    drawText(p, at, lb, Qt::AlignHCenter | Qt::AlignVCenter);
                }
            }

            // 原因：沿著骨頭等距掛上去，每一條往左拉一條橫線
            const int m = cat.causes.size();
            for (int j = 0; j < m; ++j) {
                const double frac = (m == 1) ? 0.55
                                             : 0.28 + 0.60 * static_cast<double>(j) / (m - 1);
                const QPointF at = anchor + frac * boneLen * dir;
                LineStyle cl = causeLine;
                cl.color = color.darker(115);
                p.setPen(pen(cl));
                p.drawLine(at, at + QPointF(-causeLen, 0));

                Label lb;
                lb.text = cat.causes[j].text;
                lb.font = causeFont;
                lb.font.size *= scale;
                drawText(p, at + QPointF(-causeLen - 3 * scale, 0), lb,
                         Qt::AlignRight | Qt::AlignVCenter);

                // 第二層：從橫線上再長出和主骨平行的小刺
                if (!showSubCauses) continue;
                const QStringList& subs = cat.causes[j].subCauses;
                for (int q = 0; q < subs.size(); ++q) {
                    const double sf = (subs.size() == 1)
                                          ? 0.5
                                          : 0.25 + 0.5 * static_cast<double>(q) / (subs.size() - 1);
                    const QPointF sp = at + QPointF(-causeLen * sf, 0);
                    const QPointF stip = sp + boneLen * 0.16 * dir;
                    p.drawLine(sp, stip);

                    Label sl;
                    sl.text = subs[q];
                    sl.font = subCauseFont;
                    sl.font.size *= scale;
                    // 往左展開，不要置中：小刺和主骨是平行的，置中的話文字有一半
                    // 會壓在主骨上，而主骨是整張圖最粗的那條線，壓上去就看不清了。
                    drawText(p, stip + QPointF(-3 * scale, 0), sl,
                             Qt::AlignRight | Qt::AlignVCenter);
                }
            }
        }
    };

    placeSide(upper, +1);
    placeSide(lower, -1);
    p.restore();
}

QVector<LegendItem> Fishbone::legendItems() const {
    QVector<LegendItem> items;
    for (int i = 0; i < categories.size(); ++i) {
        if (categories[i].name.isEmpty()) continue;
        LegendItem it;
        it.text = categories[i].name;
        it.line = LineStyle(categories[i].color.isValid() ? categories[i].color
                                                         : palette.color(i),
                            2.0);
        items.append(it);
    }
    return items;
}


// ───────────────────────── 柏拉圖 ─────────────────────────

double Pareto::total() const {
    double t = 0;
    for (const ParetoItem& it : items)
        if (std::isfinite(it.value) && it.value > 0) t += it.value;
    return t;
}

QVector<ParetoItem> Pareto::sortedItems() const {
    QVector<ParetoItem> v;
    v.reserve(items.size());
    for (const ParetoItem& it : items)
        if (std::isfinite(it.value) && it.value > 0) v.append(it);
    if (sortDescending)
        std::stable_sort(v.begin(), v.end(), [](const ParetoItem& a, const ParetoItem& b) {
            return a.value > b.value;
        });
    return v;
}

QVector<double> Pareto::cumulativePercents() const {
    const QVector<ParetoItem> v = sortedItems();
    const double t = total();
    QVector<double> out;
    out.reserve(v.size());
    double run = 0;
    for (const ParetoItem& it : v) {
        run += it.value;
        out.append(t > 0 ? run / t * 100.0 : 0.0);
    }
    return out;
}

int Pareto::itemsToReach(double percent) const {
    const QVector<double> cum = cumulativePercents();
    for (int i = 0; i < cum.size(); ++i)
        if (cum[i] >= percent - 1e-9) return i + 1;
    return cum.size();
}

AxisLimits Pareto::axisLimits() const {
    AxisLimits lim;
    const QVector<ParetoItem> v = sortedItems();
    if (v.isEmpty()) return lim;
    lim.x.expand(-0.7);
    lim.x.expand(v.size() - 0.3);
    lim.y.expand(0);
    for (const ParetoItem& it : v) lim.y.expand(it.value);
    return lim;
}

void Pareto::render(RenderPack& rp) {
    const QVector<ParetoItem> v = sortedItems();
    if (v.isEmpty()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double half = std::clamp(barWidth, 0.05, 1.0) / 2;
    const double baseY = t.py(0);
    for (int i = 0; i < v.size(); ++i) {
        const QRectF r(QPointF(t.px(i - half), t.py(v[i].value)),
                       QPointF(t.px(i + half), baseY));
        QColor fill = v[i].color.isValid() ? v[i].color
                                           : (colorByItem ? palette.color(i) : uniformColor);
        p.setBrush(QBrush(fill));
        QPen pen = barBorder.isVisible() ? barBorder.pen() : QPen(Qt::NoPen);
        if (barBorder.isVisible()) pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        p.drawRect(r.normalized());

        if (showValueLabels) {
            Label lb;
            lb.text = QString::number(v[i].value, 'f', 0);
            lb.font = valueFont;
            lb.font.size *= rp.scaleFactor;
            const QSizeF ext = measureText(lb);
            drawText(p, QPointF(r.center().x(), t.py(v[i].value) - 3 * rp.scaleFactor
                                                    - ext.height() / 2),
                     lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }

    // ── 累積折線 ──
    //
    // 折線綁在另一根軸上（0–100%），所以要自己建一個 Transform，
    // 不能用 transform(rp) —— 那個換算出來的是左軸的數量。
    if (cumulativeAxis) {
        Transform ct = t;
        ct.yr = cumulativeAxis->range;
        ct.yInverted = cumulativeAxis->inverted;
        ct.yBreaks = cumulativeAxis->hasBreaks() ? &cumulativeAxis->breaks() : nullptr;

        if (referencePercent > 0 && referenceLine.isVisible()) {
            QPen pen = referenceLine.pen();
            pen.setWidthF(pen.widthF() * rp.scaleFactor);
            p.setPen(pen);
            const double y = ct.py(referencePercent);
            p.drawLine(QPointF(t.rect.left, y), QPointF(t.rect.right, y));
        }

        const QVector<double> cum = cumulativePercents();
        QVector<QPointF> pts;
        pts.reserve(cum.size());
        for (int i = 0; i < cum.size(); ++i) pts.append(ct.p(i, cum[i]));

        if (cumulativeLine.isVisible() && pts.size() >= 2) {
            QPen pen = cumulativeLine.pen();
            pen.setWidthF(pen.widthF() * rp.scaleFactor);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(pts.constData(), static_cast<int>(pts.size()));
        }
        if (cumulativeMarker.isVisible()) {
            MarkerStyle m = cumulativeMarker;
            m.size *= rp.scaleFactor;
            m.outline.width *= rp.scaleFactor;
            drawMarkers(p, pts, m);
        }
    }
    p.restore();
}

QVector<LegendItem> Pareto::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = FillStyle(uniformColor);
    it.line = LineStyle::none();
    return {it};
}

// ───────────────────────── 鬆餅圖 ─────────────────────────

QVector<int> Waffle::cellAssignment() const {
    const int total = std::max(1, rows) * std::max(1, columns);
    QVector<int> cells(total, -1);
    double sum = 0;
    for (const WaffleCategory& c : categories)
        if (std::isfinite(c.value) && c.value > 0) sum += c.value;
    if (!(sum > 0)) return cells;

    // 最大餘額法。先取整數部分，剩下的格子依小數部分由大到小補。
    // 直接四捨五入的話格子總數會對不上 —— 十個 3.33% 可以湊出 30 格也可以湊出 40 格。
    QVector<int> quota(categories.size(), 0);
    QVector<QPair<double, int>> remainder;
    int used = 0;
    for (int i = 0; i < categories.size(); ++i) {
        const double v = (std::isfinite(categories[i].value) && categories[i].value > 0)
                             ? categories[i].value
                             : 0.0;
        const double exact = v / sum * total;
        quota[i] = static_cast<int>(std::floor(exact));
        used += quota[i];
        remainder.append({exact - quota[i], i});
    }
    std::sort(remainder.begin(), remainder.end(),
              [](const QPair<double, int>& a, const QPair<double, int>& b) {
                  if (a.first != b.first) return a.first > b.first;
                  return a.second < b.second;  // 餘額相同時照索引，結果才穩定
              });
    for (int k = 0; used < total && k < remainder.size(); ++k) {
        ++quota[remainder[k].second];
        ++used;
    }

    int cursor = 0;
    for (int i = 0; i < categories.size(); ++i)
        for (int k = 0; k < quota[i] && cursor < total; ++k) cells[cursor++] = i;
    return cells;
}

AxisLimits Waffle::axisLimits() const {
    return AxisLimits(extent.left, extent.right, extent.bottom, extent.top);
}

void Waffle::render(RenderPack& rp) {
    const int nr = std::max(1, rows);
    const int nc = std::max(1, columns);
    const QVector<int> cells = cellAssignment();

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QRectF box = t.pixelRect(extent);
    const double cw = box.width() / nc;
    const double ch = box.height() / nr;
    const double gapX = cw * std::clamp(gapFraction, 0.0, 0.5);
    const double gapY = ch * std::clamp(gapFraction, 0.0, 0.5);
    const double radius = std::min(cw - gapX, ch - gapY) * std::clamp(cornerFraction, 0.0, 0.5);

    for (int i = 0; i < cells.size(); ++i) {
        const int col = i % nc;
        const int rowFromStart = i / nc;
        // 由下往上填：第 0 格要落在最後一列
        const int row = fillFromBottom ? (nr - 1 - rowFromStart) : rowFromStart;
        const QRectF cell(box.left() + col * cw + gapX / 2, box.top() + row * ch + gapY / 2,
                          cw - gapX, ch - gapY);

        QColor fill = emptyColor;
        if (cells[i] >= 0 && cells[i] < categories.size()) {
            const WaffleCategory& c = categories[cells[i]];
            fill = c.color.isValid() ? c.color : palette.color(cells[i]);
        }
        p.setBrush(QBrush(fill));
        QPen pen = border.isVisible() ? border.pen() : QPen(Qt::NoPen);
        if (border.isVisible()) pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        if (radius > 0.5)
            p.drawRoundedRect(cell, radius, radius);
        else
            p.drawRect(cell);
    }
    p.restore();
}

QVector<LegendItem> Waffle::legendItems() const {
    QVector<LegendItem> out;
    double sum = 0;
    for (const WaffleCategory& c : categories)
        if (c.value > 0) sum += c.value;
    for (int i = 0; i < categories.size(); ++i) {
        if (categories[i].label.isEmpty()) continue;
        LegendItem it;
        it.text = (sum > 0) ? QStringLiteral("%1 %2%")
                                  .arg(categories[i].label)
                                  .arg(categories[i].value / sum * 100, 0, 'f', 0)
                            : categories[i].label;
        it.fill = FillStyle(categories[i].color.isValid() ? categories[i].color
                                                         : palette.color(i));
        it.line = LineStyle::none();
        out.append(it);
    }
    return out;
}

// ───────────────────────── 斜率圖與排名圖 ─────────────────────────

QVector<QVector<double>> Slope::ranks() const {
    const int periods = periodLabels.size();
    QVector<QVector<double>> out(series.size(), QVector<double>(periods, 0.0));
    for (int t = 0; t < periods; ++t) {
        QVector<QPair<double, int>> column;
        for (int i = 0; i < series.size(); ++i) {
            const double v = (t < series[i].values.size()) ? series[i].values[t]
                                                           : std::numeric_limits<double>::quiet_NaN();
            column.append({v, i});
        }
        // 值大的名次小（第 1 名）。值相同時照索引，名次才不會隨輸入順序變。
        std::sort(column.begin(), column.end(),
                  [](const QPair<double, int>& a, const QPair<double, int>& b) {
                      if (std::isnan(a.first) != std::isnan(b.first)) return !std::isnan(a.first);
                      if (a.first != b.first) return a.first > b.first;
                      return a.second < b.second;
                  });
        for (int k = 0; k < column.size(); ++k) out[column[k].second][t] = k + 1;
    }
    return out;
}

QVector<QVector<double>> Slope::plotValues() const {
    if (useRanks) return ranks();
    QVector<QVector<double>> out;
    out.reserve(series.size());
    for (const SlopeSeries& s : series) out.append(s.values);
    return out;
}

AxisLimits Slope::axisLimits() const {
    AxisLimits lim;
    if (series.isEmpty() || periodLabels.isEmpty()) return lim;
    lim.x.expand(0);
    lim.x.expand(periodLabels.size() - 1);
    const QVector<QVector<double>> vals = plotValues();
    for (const QVector<double>& row : vals)
        for (double v : row) lim.y.expand(v);
    return lim;
}

void Slope::render(RenderPack& rp) {
    if (series.isEmpty() || periodLabels.size() < 2) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QVector<QVector<double>> vals = plotValues();
    bool anyHighlight = false;
    for (const SlopeSeries& s : series)
        if (s.highlight) anyHighlight = true;

    for (int i = 0; i < series.size(); ++i) {
        const SlopeSeries& s = series[i];
        QColor color = s.color.isValid() ? s.color : palette.color(i);
        // 有東西被強調時，其餘的淡化。全部一樣濃的話，強調就沒有意義了。
        if (anyHighlight && !s.highlight) color.setAlpha(std::clamp(dimAlpha, 0, 255));

        QVector<QPointF> pts;
        for (int k = 0; k < periodLabels.size(); ++k) {
            if (k >= vals[i].size() || !std::isfinite(vals[i][k])) continue;
            pts.append(t.p(k, vals[i][k]));
        }
        if (pts.size() < 2) continue;

        LineStyle ls = line;
        ls.color = color;
        if (s.highlight) ls.width *= 1.8;
        QPen pen = ls.pen();
        pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(pts.constData(), static_cast<int>(pts.size()));

        if (marker.isVisible()) {
            MarkerStyle m = marker;
            m.fillColor = color;
            m.outline.color = color;
            m.size *= rp.scaleFactor;
            if (s.highlight) m.size *= 1.25;
            m.outline.width *= rp.scaleFactor;
            drawMarkers(p, pts, m);
        }

        if (showEndLabels && !s.label.isEmpty()) {
            auto endLabel = [&](int k, Qt::Alignment align, double dx) {
                if (k >= vals[i].size() || !std::isfinite(vals[i][k])) return;
                Label lb;
                lb.text = s.label;
                if (showEndValues && !useRanks)
                    lb.text += QStringLiteral("  %1").arg(s.values.value(k), 0, 'f', 0);
                else if (showEndValues)
                    lb.text += QStringLiteral("  %1").arg(static_cast<int>(vals[i][k]));
                lb.font = labelFont;
                lb.font.size *= rp.scaleFactor;
                lb.font.color = color.alpha() < 255 ? color : lb.font.color;
                if (s.highlight) lb.font.bold = true;
                drawText(p, t.p(k, vals[i][k]) + QPointF(dx, 0), lb,
                         align | Qt::AlignVCenter);
            };
            const double gap = labelGapPx * rp.scaleFactor;
            endLabel(0, Qt::AlignRight, -gap);
            endLabel(periodLabels.size() - 1, Qt::AlignLeft, gap);
        }
    }
    p.restore();
}

QVector<LegendItem> Slope::legendItems() const {
    QVector<LegendItem> out;
    for (int i = 0; i < series.size(); ++i) {
        if (series[i].label.isEmpty()) continue;
        LegendItem it;
        it.text = series[i].label;
        LineStyle ls = line;
        ls.color = series[i].color.isValid() ? series[i].color : palette.color(i);
        it.line = ls;
        out.append(it);
    }
    return out;
}

}  // namespace qtplot

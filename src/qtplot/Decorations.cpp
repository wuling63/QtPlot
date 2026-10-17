#include "Decorations.h"

#include "Chart3D.h"

#include "Field.h"

#include "Field.h"
#include "Plottables.h"
#include "Specialty.h"

#include <QFontMetricsF>

namespace qtplot {

// ───────────────────────────── Colorbar ─────────────────────────────

Colorbar::Colorbar() { tickGenerator = std::make_shared<AutomaticTickGenerator>(); }

void Colorbar::follow(const Heatmap& heatmap) {
    colormap = heatmap.colormap;
    range = heatmap.dataRange();
}

void Colorbar::follow(const ContourLines& contour) {
    colormap = contour.colormap;
    const QVector<double> levels = contour.effectiveLevels();
    if (contour.colorRange.isValid()) range = contour.colorRange;
    else if (levels.size() > 1) range = CoordinateRange(levels.first(), levels.last());
    else range = contour.dataRange();
}

void Colorbar::follow(const Hexbin& hexbin) {
    colormap = hexbin.colormap;
    if (hexbin.countRange.isValid() && hexbin.countRange.hasArea()) {
        range = hexbin.countRange;
    } else {
        // 和 Hexbin::render 用同一個預設：0 到最大計數
        range = CoordinateRange(0, std::max(1, hexbin.maxCount()));
    }
}

void Colorbar::follow(const Chart3D& chart) {
    colormap = chart.colormap;
    range = chart.effectiveColorRange();
}

void Colorbar::follow(const TIN& tin) {
    colormap = tin.colormap;
    if (tin.colorRange.isValid()) {
        range = tin.colorRange;
    } else {
        CoordinateRange r;
        for (double z : tin.zs)
            if (std::isfinite(z)) r.expand(z);
        range = r;
    }
}

double Colorbar::outsideMargin(Edge e) const {
    if (e != edge || !range.isValid()) return 0;

    double total = gap + thickness + tickLength + 2;
    if (tickFont.color.alpha() > 0) {
        QFontMetricsF fm(tickFont.font());
        if (isVertical()) {
            // 最寬的刻度標籤。長度用上一次畫出來的值 —— 版面跑兩遍會收斂。
            double widest = 0;
            if (tickGenerator) {
                for (const Tick& t : tickGenerator->generate(range, m_lastLengthPx, tickFont, true))
                    if (t.isMajor && !t.label.isEmpty())
                        widest = std::max(widest,
                                          static_cast<double>(fm.horizontalAdvance(t.label)));
            }
            total += widest + 3;
        } else {
            total += fm.height() + 3;
        }
    }
    if (label.isVisible()) {
        const QSizeF s = measureText(label);
        total += (isVertical() ? s.width() : s.height()) + 4;
    }
    return total;
}

void Colorbar::setOutsideOffset(Edge e, double px) {
    if (e == edge) m_offsetPx = px;
}

void Colorbar::render(RenderPack& rp) {
    if (!range.isValid()) return;
    QPainter& p = *rp.painter;
    const PixelRect& d = rp.dataRect;
    p.save();

    // 色帶的位置：貼著繪圖區外側 gap 像素
    QRectF bar;
    if (isVertical()) {
        const double full = d.height();
        const double len = full * std::clamp(lengthFraction, 0.05, 1.0);
        const double top = d.top + (full - len) / 2;
        const double x = (edge == Edge::Right) ? (d.right + m_offsetPx + gap)
                                              : (d.left - m_offsetPx - gap - thickness);
        bar = QRectF(x, top, thickness, len);
    } else {
        const double full = d.width();
        const double len = full * std::clamp(lengthFraction, 0.05, 1.0);
        const double left = d.left + (full - len) / 2;
        const double y = (edge == Edge::Bottom) ? (d.bottom + m_offsetPx + gap)
                                                : (d.top - m_offsetPx - gap - thickness);
        bar = QRectF(left, y, len, thickness);
    }

    // 色帶本身：用一張細長的影像讓 Qt 做內插，比逐列畫矩形平滑
    const int steps = 256;
    QImage strip(isVertical() ? 1 : steps, isVertical() ? steps : 1,
                 QImage::Format_ARGB32_Premultiplied);
    for (int i = 0; i < steps; ++i) {
        // 垂直色帶的最大值在上面，而影像的第 0 列在上面，所以要反過來
        const double f = isVertical() ? (1.0 - i / (steps - 1.0)) : (i / (steps - 1.0));
        const QColor c = colormap.lookup(f);
        if (isVertical()) strip.setPixelColor(0, i, c);
        else              strip.setPixelColor(i, 0, c);
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(bar, strip);

    if (border.isVisible()) {
        p.setPen(border.pen());
        p.setBrush(Qt::NoBrush);
        p.drawRect(bar);
    }

    // 刻度：和座標軸共用同一套產生器，刻度才會落在整齊的數字上
    m_lastLengthPx = isVertical() ? bar.height() : bar.width();
    const QVector<Tick> ticks =
        tickGenerator ? tickGenerator->generate(range, m_lastLengthPx, tickFont, isVertical())
                      : QVector<Tick>();
    double labelExtent = 0;
    QFontMetricsF fm(tickFont.font());

    p.setPen(border.isVisible() ? border.pen() : QPen(QColor(0, 0, 0)));
    for (const Tick& tk : ticks) {
        if (!tk.isMajor || tk.label.isEmpty()) continue;
        if (tk.position < range.min || tk.position > range.max) continue;
        const double f = (tk.position - range.min) / range.span();
        const QString text = tk.label;

        if (isVertical()) {
            const double y = bar.bottom() - f * bar.height();
            const bool right = (edge == Edge::Right);
            const double x0 = right ? bar.right() : bar.left();
            const double x1 = right ? (x0 + tickLength) : (x0 - tickLength);
            p.drawLine(QPointF(x0, y), QPointF(x1, y));
            if (tickFont.color.alpha() > 0) {
                Label lb;
                lb.text = text;
                lb.font = tickFont;
                drawText(p, QPointF(x1 + (right ? 3 : -3), y), lb,
                         (right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
                labelExtent = std::max(labelExtent, static_cast<double>(fm.horizontalAdvance(text)));
            }
        } else {
            const double x = bar.left() + f * bar.width();
            const bool bottom = (edge == Edge::Bottom);
            const double y0 = bottom ? bar.bottom() : bar.top();
            const double y1 = bottom ? (y0 + tickLength) : (y0 - tickLength);
            p.drawLine(QPointF(x, y0), QPointF(x, y1));
            if (tickFont.color.alpha() > 0) {
                Label lb;
                lb.text = text;
                lb.font = tickFont;
                drawText(p, QPointF(x, y1 + (bottom ? 3 : -3)), lb,
                         Qt::AlignHCenter | (bottom ? Qt::AlignTop : Qt::AlignBottom));
                labelExtent = std::max(labelExtent, static_cast<double>(fm.height()));
            }
        }
    }

    // 說明文字：排在刻度標籤外側
    if (label.isVisible()) {
        Label lb = label;
        if (isVertical() && lb.font.rotation == 0) lb.font.rotation = -90;
        const QSizeF sz = measureText(lb);
        if (isVertical()) {
            const bool right = (edge == Edge::Right);
            const double x = right ? (bar.right() + tickLength + 6 + labelExtent + sz.width() / 2)
                                   : (bar.left() - tickLength - 6 - labelExtent - sz.width() / 2);
            drawText(p, QPointF(x, bar.center().y()), lb, Qt::AlignHCenter | Qt::AlignVCenter);
        } else {
            const bool bottom = (edge == Edge::Bottom);
            const double y = bottom ? (bar.bottom() + tickLength + 6 + labelExtent)
                                    : (bar.top() - tickLength - 6 - labelExtent);
            drawText(p, QPointF(bar.center().x(), y), lb,
                     Qt::AlignHCenter | (bottom ? Qt::AlignTop : Qt::AlignBottom));
        }
    }
    p.restore();
}

// ───────────────────────────── ImageMarker ─────────────────────────────

AxisLimits ImageMarker::axisLimits() const {
    AxisLimits lim;
    const qsizetype n = std::min(xs.size(), ys.size());
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        lim.x.expand(xs[i]);
        lim.y.expand(ys[i]);
    }
    return lim;
}

void ImageMarker::render(RenderPack& rp) {
    if (image.isNull()) return;
    const qsizetype n = std::min(xs.size(), ys.size());
    if (n == 0) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    p.setOpacity(std::clamp(opacity, 0.0, 1.0));

    const double w = (width > 0) ? width : image.width();
    const double h = (height > 0) ? height : image.height();
    const int step = std::max(1, stride);

    for (qsizetype i = 0; i < n; i += step) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        const QPointF c = t.p(xs[i], ys[i]);
        const QRectF target = centered ? QRectF(c.x() - w / 2, c.y() - h / 2, w, h)
                                       : QRectF(c.x(), c.y(), w, h);
        p.drawImage(target, image);
    }
    p.restore();
}

HitResult ImageMarker::hitTest(Coordinates target, const Transform& t, HitMode mode) const {
    HitResult best;
    const qsizetype n = std::min(xs.size(), ys.size());
    if (n == 0) return best;
    const QPointF pTarget = t.p(target);
    double bestSq = std::numeric_limits<double>::max();
    for (qsizetype i = 0; i < n; i += std::max(1, stride)) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        const QPointF p = t.p(xs[i], ys[i]);
        const double dx = p.x() - pTarget.x();
        const double dy = p.y() - pTarget.y();
        const double d2 = (mode == HitMode::NearestX) ? dx * dx : dx * dx + dy * dy;
        if (d2 < bestSq) {
            bestSq = d2;
            best.index = static_cast<int>(i);
            best.point = {xs[i], ys[i]};
        }
    }
    if (best.index >= 0) {
        const QPointF p = t.p(best.point);
        best.distancePx = std::hypot(p.x() - pTarget.x(), p.y() - pTarget.y());
        best.plottable = const_cast<ImageMarker*>(this);
    }
    return best;
}

// ───────────────────────────── FloatingAxis ─────────────────────────────

FloatingAxis::FloatingAxis() {
    tickGenerator = std::make_shared<AutomaticTickGenerator>();
    tickFont.size = 9.5;
}

AxisLimits FloatingAxis::axisLimits() const {
    // 軸本身不決定範圍，只標記它所在的那一條線
    AxisLimits lim;
    if (horizontal) lim.y.expand(position);
    else            lim.x.expand(position);
    return lim;
}

void FloatingAxis::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const CoordinateRange along = extent.isValid() ? extent : (horizontal ? t.xr : t.yr);
    if (!along.hasArea()) {
        p.restore();
        return;
    }

    // 軸線
    if (line.isVisible()) {
        p.setPen(line.pen());
        if (horizontal)
            p.drawLine(t.p(along.min, position), t.p(along.max, position));
        else
            p.drawLine(t.p(position, along.min), t.p(position, along.max));
    }

    const double lengthPx = horizontal ? std::abs(t.px(along.max) - t.px(along.min))
                                       : std::abs(t.py(along.max) - t.py(along.min));
    const QVector<Tick> ticks =
        tickGenerator ? tickGenerator->generate(along, lengthPx, tickFont, !horizontal)
                      : QVector<Tick>();

    const double dir = (labelSide >= 0) ? 1.0 : -1.0;
    for (const Tick& tk : ticks) {
        if (tk.position < along.min || tk.position > along.max) continue;
        // 0 的刻度和軸線重疊，標了反而擋住另一根軸
        if (tk.isMajor && std::abs(tk.position) < 1e-12 && std::abs(position) < 1e-12) continue;

        const QPointF at = horizontal ? t.p(tk.position, position) : t.p(position, tk.position);
        const double len = tk.isMajor ? tickLength : tickLength * 0.5;
        if (tickLine.isVisible()) {
            p.setPen(tickLine.pen());
            // 水平軸的刻度往下（像素 y 增加），垂直軸的刻度往左
            const QPointF off = horizontal ? QPointF(0, len * dir) : QPointF(-len * dir, 0);
            p.drawLine(at, at + off);
        }
        if (showTickLabels && tk.isMajor && !tk.label.isEmpty() && tickFont.color.alpha() > 0) {
            Label lb;
            lb.text = tk.label;
            lb.font = tickFont;
            if (horizontal) {
                drawText(p, at + QPointF(0, (len + 3) * dir), lb,
                         Qt::AlignHCenter | (dir > 0 ? Qt::AlignTop : Qt::AlignBottom));
            } else {
                drawText(p, at + QPointF(-(len + 3) * dir, 0), lb,
                         (dir > 0 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
            }
        }
    }

    if (label.isVisible()) {
        if (horizontal) {
            drawText(p, t.p(along.max, position) + QPointF(-4, -6), label,
                     Qt::AlignRight | Qt::AlignBottom);
        } else {
            Label lb = label;
            if (lb.font.rotation == 0) lb.font.rotation = -90;
            drawText(p, t.p(position, along.max) + QPointF(-8, 6), lb,
                     Qt::AlignHCenter | Qt::AlignTop);
        }
    }
    p.restore();
}

}  // namespace qtplot

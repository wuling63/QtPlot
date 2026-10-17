#include "Overlays.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace qtplot {

// ───────────────────────── TrendLine ─────────────────────────

double TrendLine::slope() const {
    const double dx = point2.x - point1.x;
    return dx != 0 ? (point2.y - point1.y) / dx : std::numeric_limits<double>::infinity();
}

AxisLimits TrendLine::axisLimits() const {
    AxisLimits lim;
    lim.x.expand(point1.x);
    lim.x.expand(point2.x);
    lim.y.expand(point1.y);
    lim.y.expand(point2.y);
    return lim;
}

void TrendLine::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    const QPointF a = t.p(point1), b = t.p(point2);
    const double len = std::hypot(b.x() - a.x(), b.y() - a.y());
    if (line.isVisible() && len > 1e-9) {
        // 往兩端延伸到比繪圖區對角線還長，剩下的交給裁切
        const double reach = (rp.dataRect.width() + rp.dataRect.height()) * 2;
        const QPointF u((b.x() - a.x()) / len, (b.y() - a.y()) / len);
        p.setPen(line.pen());
        p.drawLine(a - u * reach, b + u * reach);
    }
    if (handleMarker.isVisible()) drawMarkers(p, {a, b}, handleMarker);
}

QVector<LegendItem> TrendLine::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

bool TrendLine::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    const QPointF p = t.p(at), a = t.p(point1), b = t.p(point2);
    const double tol = tolerancePx + handleMarker.size / 2;
    if (std::hypot(p.x() - a.x(), p.y() - a.y()) <= tol) return true;
    if (std::hypot(p.x() - b.x(), p.y() - b.y()) <= tol) return true;
    // 到整條直線（不是線段）的距離
    const double len = std::hypot(b.x() - a.x(), b.y() - a.y());
    if (len < 1e-9) return false;
    const double d = std::abs((b.x() - a.x()) * (a.y() - p.y()) - (a.x() - p.x()) * (b.y() - a.y())) / len;
    return d <= tolerancePx + line.width;
}

void TrendLine::beginDrag(Coordinates at, const Transform& t, double tolerancePx) {
    const QPointF p = t.p(at), a = t.p(point1), b = t.p(point2);
    const double tol = tolerancePx + handleMarker.size / 2;
    const double da = std::hypot(p.x() - a.x(), p.y() - a.y());
    const double db = std::hypot(p.x() - b.x(), p.y() - b.y());
    m_dragPart = 0;
    if (da <= tol && da <= db) m_dragPart = 1;
    else if (db <= tol)        m_dragPart = 2;
}

void TrendLine::dragBy(Coordinates delta) {
    if (m_dragPart != 2) { point1.x += delta.x; point1.y += delta.y; }
    if (m_dragPart != 1) { point2.x += delta.x; point2.y += delta.y; }
}

// ───────────────────────────── LabeledLine ─────────────────────────────

AxisLimits LabeledLine::axisLimits() const {
    AxisLimits l;
    l.expand(start);
    l.expand(end);
    return l;
}

QVector<LegendItem> LabeledLine::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

void LabeledLine::render(RenderPack& rp) {
    const Transform t = transform(rp);
    const QPointF p1 = t.p(start), p2 = t.p(end);
    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (line.isVisible()) {
        QPen pen = line.pen();
        pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        p.drawLine(p1, p2);
    }

    const double dx = p2.x() - p1.x(), dy = p2.y() - p1.y();
    if (!text.isEmpty() && dx * dx + dy * dy >= 1) {
        // 螢幕上的角度（順時針，Y 向下）；轉到 -90..90 之間，字才不會倒過來
        double angle = std::atan2(dy, dx) * 180 / kPi;
        if (angle > 90) angle -= 180;
        if (angle < -90) angle += 180;

        const double f = std::clamp(textPosition, 0.0, 1.0);
        const double rad = angle * kPi / 180;
        // 轉過的座標系裡「往上」就是線的上方
        const QPointF at(p1.x() + dx * f + std::sin(rad) * textOffsetPx,
                         p1.y() + dy * f - std::cos(rad) * textOffsetPx);

        Label lb;
        lb.text = text;
        lb.font = font;
        lb.font.rotation = angle;
        if (textUsesLineColor) lb.font.color = line.color;
        drawText(p, at, lb, Qt::AlignHCenter | (textOffsetPx >= 0 ? Qt::AlignBottom : Qt::AlignTop));
    }
    p.restore();
}

// ───────────────────────────── ColorGrid ─────────────────────────────

AxisLimits ColorGrid::axisLimits() const {
    if (rows <= 0 || cols <= 0) return AxisLimits();
    return AxisLimits(extent.xRange(), extent.yRange());
}

void ColorGrid::render(RenderPack& rp) {
    if (rows <= 0 || cols <= 0 || colors.size() < static_cast<qsizetype>(rows) * cols) return;

    // 整張先轉成一張影像再縮放：格子之間不會有接縫，也比逐格畫矩形快得多
    QImage img(cols, rows, QImage::Format_ARGB32_Premultiplied);
    for (int r = 0; r < rows; ++r) {
        auto* scan = reinterpret_cast<QRgb*>(img.scanLine(r));
        for (int c = 0; c < cols; ++c) {
            const QColor& col = colors[static_cast<qsizetype>(r) * cols + c];
            scan[c] = col.isValid() ? qPremultiply(col.rgba()) : qRgba(0, 0, 0, 0);
        }
    }

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    p.drawImage(t.pixelRect(extent), img);
    p.restore();
}

}  // namespace qtplot

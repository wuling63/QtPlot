#include "Bars3D.h"

#include <QLinearGradient>
#include <QPolygonF>

namespace qtplot {

QVector<Bar3D> Bars3D::fromValues(const QVector<double>& values, const Palette& palette) {
    QVector<Bar3D> out;
    out.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        Bar3D b;
        b.position = i;
        b.value = values[i];
        b.fillColor = palette.color(i);
        out.append(b);
    }
    return out;
}

AxisLimits Bars3D::axisLimits() const {
    AxisLimits lim;
    if (bars.isEmpty()) return lim;

    for (const Bar3D& b : bars) {
        lim.expand({b.position - b.size / 2, b.value});
        lim.expand({b.position + b.size / 2, b.valueBase});
    }
    if (includeZero) lim.expand({lim.x.center(), 0});

    // 擠出的部分要留空間。像素量在這裡換不成資料單位（還不知道繪圖區多大），
    // 所以用比例估一個 —— 寧可多留一點，被切掉比留白難看。
    const double pad = std::clamp(depthPadFraction, 0.0, 1.0);
    if (lim.hasData()) {
        const double dx = lim.x.span() * pad;
        const double dy = lim.y.span() * pad;
        lim.expand({lim.x.max + dx, lim.y.max + dy});
    }
    return lim;
}

void Bars3D::render(RenderPack& rp) {
    if (bars.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double rad = depthAngleDegrees * kPi / 180.0;
    // 像素空間的 y 往下長，所以「往上」是負的
    const QPointF push(depthPx * std::cos(rad), -depthPx * std::sin(rad));

    // 由左往右畫。擠出是往右上推的，所以右邊那根會蓋住左邊那根的側面 ——
    // 這正是遮擋關係該有的樣子，順序反了會看到側面浮在前面。
    QVector<const Bar3D*> ordered;
    ordered.reserve(bars.size());
    for (const Bar3D& b : bars) ordered.append(&b);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Bar3D* a, const Bar3D* b) { return a->position < b->position; });

    const bool drawEdges = edgeLine.isVisible();
    for (const Bar3D* bp : ordered) {
        const Bar3D& b = *bp;
        if (!std::isfinite(b.value) || !std::isfinite(b.position)) continue;

        const double xL = t.px(b.position - b.size / 2);
        const double xR = t.px(b.position + b.size / 2);
        const double yTop = t.py(b.value);
        const double yBase = t.py(b.valueBase);

        const QPointF frontTL(xL, yTop), frontTR(xR, yTop);
        const QPointF frontBL(xL, yBase), frontBR(xR, yBase);

        p.setPen(drawEdges ? edgeLine.pen() : QPen(Qt::NoPen));

        // 側面（右）：先畫，讓前面的邊蓋過接縫
        QPolygonF side;
        side << frontTR << frontTR + push << frontBR + push << frontBR;
        p.setBrush(QBrush(b.fillColor.darker(std::max(100, sideDarken))));
        p.drawPolygon(side);

        // 頂面
        QPolygonF top;
        top << frontTL << frontTR << frontTR + push << frontTL + push;
        p.setBrush(QBrush(b.fillColor.lighter(std::max(100, topLighten))));
        p.drawPolygon(top);

        // 前面：由下往上打亮一點，看起來才有受光的感覺
        const QRectF front(QPointF(xL, std::min(yTop, yBase)),
                           QPointF(xR, std::max(yTop, yBase)));
        const double g = std::clamp(frontGradient, 0.0, 1.0);
        if (g > 0) {
            QLinearGradient grad(front.left(), front.bottom(), front.left(), front.top());
            grad.setColorAt(0.0, b.fillColor.darker(static_cast<int>(100 + 25 * g)));
            grad.setColorAt(1.0, b.fillColor.lighter(static_cast<int>(100 + 45 * g)));
            p.setBrush(QBrush(grad));
        } else {
            p.setBrush(QBrush(b.fillColor));
        }
        p.drawRect(front);

        // 長條上的文字：貼著底端放，長條再高也不會跑到看不見的地方
        if (!b.label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = b.label;
            lb.font = labelFont;
            const double frac = std::clamp(labelPositionFraction, 0.0, 1.0);
            const double y = yBase + (yTop - yBase) * frac;
            drawText(p, QPointF((xL + xR) / 2, y), lb, Qt::AlignHCenter | Qt::AlignBottom);
        }
    }

    p.restore();
}

}  // namespace qtplot

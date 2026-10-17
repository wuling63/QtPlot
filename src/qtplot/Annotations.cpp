#include "Annotations.h"

#include <QFontMetricsF>
#include <QPainterPath>
#include <QPolygonF>

namespace qtplot {

namespace {

/// 依 alignment 把一個尺寸擺到 anchor 旁邊，回傳像素矩形。
QRectF placeBox(QPointF anchor, QSizeF size, Qt::Alignment align) {
    QRectF box(anchor, size);
    if (align & Qt::AlignRight) box.moveLeft(anchor.x() - size.width());
    if (align & Qt::AlignHCenter) box.moveLeft(anchor.x() - size.width() / 2);
    if (align & Qt::AlignBottom) box.moveTop(anchor.y() - size.height());
    if (align & Qt::AlignVCenter) box.moveTop(anchor.y() - size.height() / 2);
    return box;
}

/// 從盒子中心往 towards 方向射出去，求與盒子邊界的交點。
/// 引線要從文字框的邊緣出發，不是從中心穿出來。
QPointF boxEdgeToward(const QRectF& box, QPointF towards) {
    const QPointF c = box.center();
    const double dx = towards.x() - c.x();
    const double dy = towards.y() - c.y();
    if (std::abs(dx) < 1e-9 && std::abs(dy) < 1e-9) return c;

    const double hw = box.width() / 2;
    const double hh = box.height() / 2;
    // 取能同時留在兩個方向界內的最大比例
    double t = std::numeric_limits<double>::max();
    if (std::abs(dx) > 1e-9) t = std::min(t, hw / std::abs(dx));
    if (std::abs(dy) > 1e-9) t = std::min(t, hh / std::abs(dy));
    return QPointF(c.x() + dx * t, c.y() + dy * t);
}

void drawArrowHead(QPainter& p, QPointF tip, QPointF from, double len, double width,
                   const QColor& color) {
    const double dx = tip.x() - from.x();
    const double dy = tip.y() - from.y();
    const double n = std::hypot(dx, dy);
    if (n < 1e-9) return;
    const double ux = dx / n, uy = dy / n;
    const QPointF base(tip.x() - ux * len, tip.y() - uy * len);
    const double hw = width / 2;
    QPolygonF head;
    head << tip << QPointF(base.x() - uy * hw, base.y() + ux * hw)
         << QPointF(base.x() + uy * hw, base.y() - ux * hw);
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(color));
    p.drawPolygon(head);
}

/// 沿著矩形邊界走 s 像素之後到達的點。
///
/// 起點是左上角，順時針（上 → 右 → 下 → 左）。s 會自動繞回去，所以可以傳負的
/// 或超過一圈的值。
///
/// 為什麼不用 QPainterPath::pointAtPercent 加上掃一圈找方向：那樣也能動（實測在
/// 200×40 的框上根部寬誤差約 2%，來自圓角那四段 Bézier 的參數不等於弧長），
/// 但每次重畫都要取樣 360 個點才能定位根部。矩形的弧長有閉式解，直接算就好。
QPointF rectPointAtLength(const QRectF& r, double s) {
    const double w = r.width(), h = r.height();
    const double perimeter = 2 * (w + h);
    if (perimeter <= 0) return r.center();
    s = std::fmod(std::fmod(s, perimeter) + perimeter, perimeter);

    if (s <= w) return QPointF(r.left() + s, r.top());
    s -= w;
    if (s <= h) return QPointF(r.right(), r.top() + s);
    s -= h;
    if (s <= w) return QPointF(r.right() - s, r.bottom());
    s -= w;
    return QPointF(r.left(), r.bottom() - s);
}

/// 上面那條路徑的反函數：邊界上的一點走了多少像素。
double rectLengthAt(const QRectF& r, QPointF q) {
    const double w = r.width(), h = r.height();
    // 判斷落在哪一條邊時留一點容差，交點算出來通常只是「非常接近」而不是剛好在邊上
    const double eps = 1e-6;
    if (std::abs(q.y() - r.top()) < eps) return std::clamp(q.x() - r.left(), 0.0, w);
    if (std::abs(q.x() - r.right()) < eps) return w + std::clamp(q.y() - r.top(), 0.0, h);
    if (std::abs(q.y() - r.bottom()) < eps) return w + h + std::clamp(r.right() - q.x(), 0.0, w);
    return 2 * w + h + std::clamp(r.bottom() - q.y(), 0.0, h);
}

}  // namespace

// ───────────────────────────── Tooltip ─────────────────────────────

AxisLimits Tooltip::axisLimits() const {
    AxisLimits lim;
    lim.expand(tip);
    lim.expand(labelPosition);
    return lim;
}

QPolygonF Tooltip::tailPolygon(const QRectF& box, QPointF tip, double tailWidth) {
    // 尖端已經在框裡面的話就不長尾巴 —— 硬要畫會變成一團戳回自己的三角形
    if (box.contains(tip)) return QPolygonF();

    const double shortSide = std::min(box.width(), box.height());
    const double baseWidth = std::clamp(tailWidth, 0.0, 1.0) * shortSide;
    if (baseWidth <= 0.5) return QPolygonF();

    // 根部的中點是「框心射向 tip」與框線的交點；兩個根部點再沿著框線
    // 各退半個根寬。沿邊界量而不是沿直線量，兩點就一定落在框線上；
    // 尖端在斜角方向時，尾巴會自然地跨過轉角。
    const double mid = rectLengthAt(box, boxEdgeToward(box, tip));
    QPolygonF tail;
    tail << rectPointAtLength(box, mid - baseWidth / 2) << tip
         << rectPointAtLength(box, mid + baseWidth / 2);
    return tail;
}

void Tooltip::render(RenderPack& rp) {
    if (!label.isVisible()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QPointF pTip = t.p(tip);
    const QPointF pLabel = t.p(labelPosition);

    QSizeF sz = measureText(label);
    QRectF box = placeBox(pLabel, sz, Qt::AlignHCenter | Qt::AlignVCenter);
    box.adjust(-padding, -padding, padding, padding);

    const double radius = std::clamp(cornerRadius, 0.0, std::min(box.width(), box.height()) / 2);
    QPainterPath body;
    body.addRoundedRect(box, radius, radius);

    QPainterPath shape = body;
    const QPolygonF tail = tailPolygon(box, pTip, tailWidth);
    if (!tail.isEmpty()) {
        QPainterPath tailPath;
        tailPath.addPolygon(tail);
        tailPath.closeSubpath();
        // 聯集成單一外框，框與尾巴交界的那段線才不會畫出來
        shape = body.united(tailPath);
    }

    p.setBrush(background.brush());
    p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
    p.drawPath(shape);

    drawText(p, box.center(), label, Qt::AlignHCenter | Qt::AlignVCenter);
    p.restore();
}

// ───────────────────────────── Callout ─────────────────────────────

AxisLimits Callout::axisLimits() const {
    AxisLimits lim;
    lim.expand(target);
    lim.expand(textPosition);
    return lim;
}

void Callout::render(RenderPack& rp) {
    if (!label.isVisible()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QPointF pTarget = t.p(target);
    const QPointF pText = t.p(textPosition);

    QSizeF sz = measureText(label);
    QRectF box = placeBox(pText, sz, Qt::AlignHCenter | Qt::AlignVCenter);
    box.adjust(-padding, -padding, padding, padding);

    // 引線：從文字框邊緣拉到目標點
    if (leader.isVisible()) {
        const QPointF from = boxEdgeToward(box, pTarget);
        p.setPen(leader.pen());
        p.setBrush(Qt::NoBrush);
        if (arrowHead) {
            // 線只畫到箭頭底部，粗線才不會從尖端露出來
            const double dx = pTarget.x() - from.x(), dy = pTarget.y() - from.y();
            const double n = std::hypot(dx, dy);
            if (n > headLength) {
                p.drawLine(from, QPointF(pTarget.x() - dx / n * headLength,
                                         pTarget.y() - dy / n * headLength));
            }
            drawArrowHead(p, pTarget, from, headLength, headWidth, leader.color);
        } else {
            p.drawLine(from, pTarget);
        }
    }

    if (background.isVisible() || border.isVisible()) {
        p.setBrush(background.brush());
        p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
        p.drawRect(box);
    }
    drawText(p, box.center(), label, Qt::AlignHCenter | Qt::AlignVCenter);
    p.restore();
}

// ───────────────────────────── Bracket ─────────────────────────────

AxisLimits Bracket::axisLimits() const {
    AxisLimits lim;
    lim.expand(start);
    lim.expand(end);
    return lim;
}

void Bracket::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(line.pen());
    p.setBrush(Qt::NoBrush);

    const QPointF a = t.p(start);
    const QPointF b = t.p(end);

    // 鬚的方向：水平括號的鬚垂直伸出，垂直括號的鬚水平伸出
    const QPointF tip = horizontal ? QPointF(0, tipLength) : QPointF(tipLength, 0);

    QPolygonF path;
    path << (a + tip) << a << b << (b + tip);
    p.drawPolyline(path);

    if (label.isVisible()) {
        const QPointF mid((a.x() + b.x()) / 2, (a.y() + b.y()) / 2);
        // 文字放在鬚的反方向（括號開口的另一側）
        Qt::Alignment align;
        QPointF anchor = mid;
        if (horizontal) {
            const bool tipDown = tipLength > 0;
            anchor += QPointF(0, tipDown ? -labelGap : labelGap);
            align = Qt::AlignHCenter | (tipDown ? Qt::AlignBottom : Qt::AlignTop);
        } else {
            const bool tipRight = tipLength > 0;
            anchor += QPointF(tipRight ? -labelGap : labelGap, 0);
            align = (tipRight ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
        }
        drawText(p, anchor, label, align);
    }
    p.restore();
}

// ───────────────────────────── ScaleBar ─────────────────────────────

void ScaleBar::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(line.pen());
    p.setBrush(Qt::NoBrush);

    const PixelRect& d = rp.dataRect;
    // 以資料單位換算出的像素長度
    const double wPx = (width > 0) ? std::abs(t.px(width) - t.px(0)) : 0;
    const double hPx = (height > 0) ? std::abs(t.py(height) - t.py(0)) : 0;

    // 角落：corner 決定直角往哪一邊開
    const bool right = (corner & Qt::AlignRight);
    const bool bottom = (corner & Qt::AlignBottom);
    const double cornerX = right ? (d.right - marginX) : (d.left + marginX);
    const double cornerY = bottom ? (d.bottom - marginY) : (d.top + marginY);

    const double xEnd = right ? (cornerX - wPx) : (cornerX + wPx);
    const double yEnd = bottom ? (cornerY - hPx) : (cornerY + hPx);

    if (wPx > 0) p.drawLine(QPointF(cornerX, cornerY), QPointF(xEnd, cornerY));
    if (hPx > 0) p.drawLine(QPointF(cornerX, cornerY), QPointF(cornerX, yEnd));

    if (font.color.alpha() > 0) {
        // 標籤一律放在角落的「內側」。放外側的話，貼在繪圖區邊緣的比例尺
        // 會把標籤畫到裁切範圍之外，文字就被切掉了。
        if (wPx > 0) {
            Label lb;
            lb.text = widthLabel.isEmpty() ? QString::number(width, 'g', 4) : widthLabel;
            lb.font = font;
            drawText(p, QPointF((cornerX + xEnd) / 2, cornerY + (bottom ? -labelGap : labelGap)),
                     lb, Qt::AlignHCenter | (bottom ? Qt::AlignBottom : Qt::AlignTop));
        }
        if (hPx > 0) {
            Label lb;
            lb.text = heightLabel.isEmpty() ? QString::number(height, 'g', 4) : heightLabel;
            lb.font = font;
            lb.font.rotation = -90;
            drawText(p, QPointF(cornerX + (right ? -labelGap : labelGap), (cornerY + yEnd) / 2),
                     lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
    p.restore();
}

// ───────────────────────────── ImageRect ─────────────────────────────

CoordinateRect ImageRect::fittedRect(const Transform& t) const {
    if (!keepAspectRatio || image.isNull() || image.width() <= 0 || image.height() <= 0)
        return rect;

    // 長寬比是像素空間的概念，所以要換算到像素再比
    const double boxW = std::abs(t.px(rect.right) - t.px(rect.left));
    const double boxH = std::abs(t.py(rect.top) - t.py(rect.bottom));
    if (boxW <= 0 || boxH <= 0) return rect;

    const double imgAspect = static_cast<double>(image.width()) / image.height();
    const double boxAspect = boxW / boxH;

    CoordinateRect out = rect;
    if (imgAspect > boxAspect) {
        // 影像比較寬 → 上下留白
        const double scale = boxAspect / imgAspect;
        const double h = rect.height() * scale;
        const double cy = (rect.bottom + rect.top) / 2;
        out.bottom = cy - h / 2;
        out.top = cy + h / 2;
    } else {
        const double scale = imgAspect / boxAspect;
        const double w = rect.width() * scale;
        const double cx = (rect.left + rect.right) / 2;
        out.left = cx - w / 2;
        out.right = cx + w / 2;
    }
    return out;
}

AxisLimits ImageRect::axisLimits() const {
    AxisLimits lim;
    lim.x.expand(rect.left);
    lim.x.expand(rect.right);
    lim.y.expand(rect.bottom);
    lim.y.expand(rect.top);
    return lim;
}

void ImageRect::render(RenderPack& rp) {
    if (image.isNull()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    p.setOpacity(std::clamp(opacity, 0.0, 1.0));

    const QRectF target = t.pixelRect(fittedRect(t));
    p.drawImage(target, image);

    p.setOpacity(1.0);
    if (border.isVisible()) {
        p.setPen(border.pen());
        p.setBrush(Qt::NoBrush);
        p.drawRect(target);
    }
    p.restore();
}

}  // namespace qtplot

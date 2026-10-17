#include "Plottables.h"
#include "Decimation.h"

#include "Plot.h"  // onCursorMoved 要呼叫 Plot::hitTest

#include <QFontMetricsF>
#include <QImage>
#include <QPainterPath>
#include <QPolygonF>

namespace qtplot {

namespace {

/// 畫逐點的資料標籤。Markers 與 Scatter 共用。
void drawPointLabels(QPainter& p, const PointLabels& labels, const QVector<double>& xs,
                     const QVector<double>& ys, const Transform& t) {
    if (!labels.isVisible()) return;
    const qsizetype n = std::min(xs.size(), ys.size());
    // 點太多就整個不標。標出來只會糊成一片，而且畫幾千段文字很慢。
    if (n > labels.maxLabels) return;

    for (qsizetype i = 0; i < n && i < labels.texts.size(); ++i) {
        const QString& text = labels.texts.at(static_cast<int>(i));
        if (text.isEmpty()) continue;
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        Label lb;
        lb.text = text;
        lb.font = labels.font;
        drawText(p, t.p(xs[i], ys[i]) + QPointF(labels.offsetX, labels.offsetY), lb,
                 labels.alignment);
    }
}

}  // namespace


/// 這幾個在 Smoothing.cpp。
void drawInterpolated(QPainter& p, const QVector<QPointF>& pts, const LineStyle& line,
                      Scatter::Interpolation mode, double tension);
void clampRenderRange(int first, int last, qsizetype count, qsizetype& outFirst,
                      qsizetype& outLast);

namespace {

/// 兩個陣列可能長度不同（使用者改了其中一個），一律取較短的。
qsizetype pairCount(const QVector<double>& a, const QVector<double>& b) {
    return std::min(a.size(), b.size());
}

AxisLimits limitsOf(const QVector<double>& xs, const QVector<double>& ys) {
    AxisLimits lim;
    const qsizetype n = pairCount(xs, ys);
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        lim.x.expand(xs[i]);
        lim.y.expand(ys[i]);
    }
    return lim;
}

/// 畫一條折線，遇到非有限點就斷開（分成多段 polyline）。
void drawBrokenPolyline(QPainter& painter, const QVector<QPointF>& pts, const LineStyle& line) {
    if (!line.isVisible() || pts.size() < 2) return;
    painter.setPen(line.pen());
    painter.setBrush(Qt::NoBrush);
    QPolygonF run;
    for (const QPointF& p : pts) {
        if (std::isfinite(p.x()) && std::isfinite(p.y())) {
            run.append(p);
        } else {
            if (run.size() >= 2) painter.drawPolyline(run);
            run.clear();
        }
    }
    if (run.size() >= 2) painter.drawPolyline(run);
    else if (run.size() == 1 && pts.size() == 1) { /* 單點交給標記畫 */ }
}

/// 把點序列展開成階梯線。
QVector<QPointF> makeSteps(const QVector<QPointF>& pts, Scatter::StepStyle style) {
    if (style == Scatter::StepStyle::None || pts.size() < 2) return pts;
    QVector<QPointF> out;
    out.reserve(pts.size() * 2);
    out.append(pts.first());
    for (qsizetype i = 1; i < pts.size(); ++i) {
        const QPointF& a = pts[i - 1];
        const QPointF& b = pts[i];
        if (style == Scatter::StepStyle::Horizontal) out.append(QPointF(b.x(), a.y()));
        else                                         out.append(QPointF(a.x(), b.y()));
        out.append(b);
    }
    return out;
}

/// 在 (xs, ys) 裡找離 target 最近的點。距離在像素空間算 ——
/// 兩軸的數量級常常差很多，用資料距離的話數值大的那一軸會完全主導。
HitResult nearestPoint(const QVector<double>& xs, const QVector<double>& ys,
                       Coordinates target, const Transform& t, HitMode mode) {
    HitResult best;
    const qsizetype n = pairCount(xs, ys);
    if (n == 0) return best;
    const QPointF pTarget = t.p(target);
    double bestSq = std::numeric_limits<double>::max();
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        const QPointF p = t.p(xs[i], ys[i]);
        const double dx = p.x() - pTarget.x();
        const double dy = p.y() - pTarget.y();
        // NearestX：游標的垂直位置不參與比較
        const double d2 = (mode == HitMode::NearestX) ? dx * dx : dx * dx + dy * dy;
        if (d2 < bestSq) {
            bestSq = d2;
            best.index = static_cast<int>(i);
            best.point = {xs[i], ys[i]};
        }
    }
    if (best.index >= 0) {
        // 距離一律回報真正的二維距離，否則 maxDistancePx 的門檻會失去意義
        const QPointF p = t.p(best.point);
        best.distancePx = std::hypot(p.x() - pTarget.x(), p.y() - pTarget.y());
    }
    return best;
}

}  // namespace

// ───────────────────────────── Scatter ─────────────────────────────

QVector<double> Scatter::displayXs() const {
    if (xScale == 1.0 && xOffset == 0.0) return xs;
    QVector<double> out(xs.size());
    for (qsizetype i = 0; i < xs.size(); ++i) out[i] = xs[i] * xScale + xOffset;
    return out;
}

QVector<double> Scatter::displayYs() const {
    if (yScale == 1.0 && yOffset == 0.0) return ys;
    QVector<double> out(ys.size());
    for (qsizetype i = 0; i < ys.size(); ++i) out[i] = ys[i] * yScale + yOffset;
    return out;
}

AxisLimits Scatter::axisLimits() const {
    AxisLimits lim = limitsOf(displayXs(), displayYs());
    if (std::isfinite(fillToY) && lim.hasData()) lim.y.expand(fillToY);
    return lim;
}

void Scatter::render(RenderPack& rp) {
    const Transform t = transform(rp);
    const QVector<double> dxs = displayXs();
    const QVector<double> dys = displayYs();
    const qsizetype n = pairCount(dxs, dys);
    if (n == 0) return;

    qsizetype from = 0, to = n - 1;
    clampRenderRange(renderFirst, renderLast, n, from, to);

    QVector<QPointF> pts;
    pts.reserve(to - from + 1);
    for (qsizetype i = from; i <= to; ++i) {
        if (skipNonFinite && (!std::isfinite(dxs[i]) || !std::isfinite(dys[i]))) {
            pts.append(QPointF(qQNaN(), qQNaN()));  // 斷點
            continue;
        }
        pts.append(t.p(dxs[i], dys[i]));
    }

    QPainter& p = *rp.painter;
    const QVector<QPointF> linePts = makeSteps(pts, step);

    // 填色：先填再畫線，線才不會被填色蓋住
    if (fill.isVisible() && std::isfinite(fillToY) && linePts.size() >= 2) {
        QPolygonF poly;
        poly.reserve(linePts.size() + 2);
        for (const QPointF& pt : linePts)
            if (std::isfinite(pt.x()) && std::isfinite(pt.y())) poly.append(pt);
        if (poly.size() >= 2) {
            const double yBase = t.py(fillToY);
            poly.append(QPointF(poly.last().x(), yBase));
            poly.append(QPointF(poly.first().x(), yBase));
            p.save();
            p.setPen(Qt::NoPen);
            if (fillAboveColor.isValid() || fillBelowColor.isValid()) {
                // 上下兩半各自裁切再填，交界正好落在基準線上
                const QRectF area = rp.dataRect.toQRectF();
                const QRectF above(area.left(), area.top() - 1, area.width(), yBase - area.top() + 1);
                const QRectF below(area.left(), yBase, area.width(), area.bottom() - yBase + 1);
                const QColor ca = fillAboveColor.isValid() ? fillAboveColor : fill.color;
                const QColor cb = fillBelowColor.isValid() ? fillBelowColor : fill.color;
                p.setClipRect(above, Qt::IntersectClip);
                p.setBrush(ca);
                p.drawPolygon(poly);
                p.restore();
                p.save();
                p.setPen(Qt::NoPen);
                p.setClipRect(below, Qt::IntersectClip);
                p.setBrush(cb);
                p.drawPolygon(poly);
            } else {
                // 漸層要鋪在「填色區域」上，不是整個繪圖區 ——
                // 用繪圖區的話，曲線只佔畫面一半時會只看到色階的一半
                p.setBrush(fill.brush(poly.boundingRect()));
                p.drawPolygon(poly);
            }
            p.restore();
        }
    }

    auto drawPointMarkers = [&] {
        if (!marker.isVisible()) return;
        QVector<QPointF> valid;
        valid.reserve(pts.size());
        for (const QPointF& pt : pts)
            if (std::isfinite(pt.x()) && std::isfinite(pt.y())) valid.append(pt);
        drawMarkers(p, valid, marker);
    };

    if (markersBelowLine) drawPointMarkers();
    p.save();
    drawInterpolated(p, linePts, line, step == StepStyle::None ? interpolation
                                                              : Interpolation::Straight,
                     smoothTension);
    p.restore();
    if (!markersBelowLine) drawPointMarkers();

    drawPointLabels(p, pointLabels, dxs, dys, t);
}

QVector<LegendItem> Scatter::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    it.marker = marker;
    return {it};
}

// ───────────────────────────── Markers ─────────────────────────────

AxisLimits Markers::axisLimits() const { return limitsOf(xs, ys); }

void Markers::render(RenderPack& rp) {
    const Transform t = transform(rp);
    const qsizetype n = pairCount(xs, ys);
    QVector<QPointF> pts;
    pts.reserve(n);
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        pts.append(t.p(xs[i], ys[i]));
    }
    drawMarkers(*rp.painter, pts, marker);
    drawPointLabels(*rp.painter, pointLabels, xs, ys, t);
}

QVector<LegendItem> Markers::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = LineStyle::none();
    it.marker = marker;
    return {it};
}

// ───────────────────────────── Signal ─────────────────────────────

namespace {
/// 少於這個點數就直接掃，不建索引（建索引要多 1/16 的記憶體，小資料划不來）。
constexpr qsizetype kMinMaxIndexThreshold = 1 << 14;

/// 確認快取的索引還對得上 data；對不上就重建。
///
/// cached 和 data 共用同一塊記憶體（QVector 的隱式共用）。使用者一改資料
/// （ys[i] = …、append、整個換掉）QVector 就會脫離共用，constData() 指標跟著變 ——
/// 不必叫使用者「改完記得通知」，也不必每張都掃一遍比對內容。
const MinMaxIndex* validIndex(const QVector<double>& data, QVector<double>& cached, MinMaxIndex& index) {
    if (data.size() < kMinMaxIndexThreshold) {
        if (!index.isEmpty()) {
            index.clear();
            cached = QVector<double>();
        }
        return nullptr;
    }
    if (cached.constData() != data.constData() || cached.size() != data.size() || index.size() != data.size()) {
        cached = data;
        index.build(cached.constData(), cached.size());
    }
    return &index;
}
}  // namespace

const MinMaxIndex* Signal::minMaxIndex() const { return validIndex(ys, m_indexed, m_index); }
const MinMaxIndex* SignalXY::minMaxIndex() const { return validIndex(ys, m_indexed, m_index); }

AxisLimits Signal::axisLimits() const {
    AxisLimits lim;
    if (ys.isEmpty()) return lim;
    CoordinateRange along, value;
    along.expand(xOffset);
    along.expand(xOffset + (ys.size() - 1) * period);
    // 有索引就直接查（五千萬點時這裡原本要掃一遍）
    double lo, hi;
    const MinMaxIndex* idx = minMaxIndex();
    if (idx ? idx->query(ys.constData(), 0, ys.size(), lo, hi) : scanMinMax(ys.constData(), 0, ys.size(), lo, hi)) {
        value.expand(lo * yScale + yOffset);
        value.expand(hi * yScale + yOffset);
    }
    // 垂直方向時索引對到 Y、數值對到 X
    if (vertical) {
        lim.x = value;
        lim.y = along;
    } else {
        lim.x = along;
        lim.y = value;
    }
    return lim;
}

void Signal::render(RenderPack& rp) {
    // 一律用 const 參照讀：非 const 的 ys[i] 會讓 QVector 脫離共用，索引快取就白建了
    const QVector<double>& Y = ys;
    if (Y.isEmpty() || period == 0) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;

    // 垂直方向時「沿著軸」的是 Y，可視範圍要看 Y 軸
    const CoordinateRange alongRange = vertical ? t.yr : t.xr;
    // 索引→像素點。垂直時交換兩個座標。
    auto pointAt = [&](qsizetype i, double v) {
        const double along = xOffset + i * period;
        const double value = v * yScale + yOffset;
        return vertical ? t.p(value, along) : t.p(along, value);
    };

    // 只處理落在可視範圍內的 index，點再多也不會整串掃過
    const double iMinF = (alongRange.min - xOffset) / period;
    const double iMaxF = (alongRange.max - xOffset) / period;
    qsizetype i0 = static_cast<qsizetype>(std::floor(std::min(iMinF, iMaxF))) - 1;
    qsizetype i1 = static_cast<qsizetype>(std::ceil(std::max(iMinF, iMaxF))) + 1;
    i0 = std::max<qsizetype>(0, i0);
    i1 = std::min<qsizetype>(Y.size() - 1, i1);
    // 使用者指定的索引範圍再夾一次
    qsizetype rf = 0, rl = Y.size() - 1;
    clampRenderRange(renderFirst, renderLast, Y.size(), rf, rl);
    i0 = std::max(i0, rf);
    i1 = std::min(i1, rl);
    if (i1 <= i0) return;

    const qsizetype visibleCount = i1 - i0 + 1;
    // 抽樣看的是資料「實際佔了」多少像素：只佔半個畫面時不能拿整個寬度來算
    auto alongPx = [&](double along) { return vertical ? t.py(along) : t.px(along); };
    const double pxFrom = alongPx(xOffset + i0 * period), pxTo = alongPx(xOffset + i1 * period);
    const double pointsPerPixel = visibleCount / std::max(1.0, std::abs(pxTo - pxFrom));
    const double* data = Y.constData();

    if (pointsPerPixel > decimationThreshold) {
        // 每個像素行畫一條最小到最大的直線。點很多時查索引，不必每張都掃過全部點。
        const MinMaxIndex* idx = minMaxIndex();
        const auto cols = buildEnvelope(
            pxFrom, pxTo, i0, i1,
            [&](double e) {
                const double fi = ((vertical ? t.cy(e) : t.cx(e)) - xOffset) / period;
                return static_cast<qsizetype>(std::clamp(std::ceil(fi), double(i0), double(i1 + 1)));
            },
            [&](qsizetype a, qsizetype b, double& lo, double& hi) {
                if (!(idx ? idx->query(data, a, b, lo, hi) : scanMinMax(data, a, b, lo, hi))) return false;
                lo = lo * yScale + yOffset;
                hi = hi * yScale + yOffset;
                return true;
            },
            [&](double v) { return vertical ? t.px(v) : t.py(v); });
        drawEnvelope(p, cols, line, vertical);
        return;  // 這麼密的時候標記只會糊成一片，不畫
    }

    QVector<QPointF> pts;
    pts.reserve(visibleCount);
    for (qsizetype i = i0; i <= i1; ++i) {
        const double v = data[i];
        pts.append(std::isfinite(v) ? pointAt(i, v) : QPointF(qQNaN(), qQNaN()));
    }

    p.save();
    drawBrokenPolyline(p, pts, line);
    p.restore();

    // 標記：使用者明確設了就畫；沒設但點距夠寬時自動顯示
    MarkerStyle m = marker;
    if (!m.isVisible() && autoMarkerSpacingPx > 0 && pointsPerPixel <= decimationThreshold) {
        const double spacingPx = std::abs(period * (vertical ? t.scaleY() : t.scaleX()));
        if (spacingPx >= autoMarkerSpacingPx) {
            m = MarkerStyle(MarkerShape::FilledCircle, std::min(6.0, spacingPx / 2), line.color);
        }
    }
    if (m.isVisible() && pointsPerPixel <= decimationThreshold) {
        QVector<QPointF> valid;
        valid.reserve(pts.size());
        for (const QPointF& pt : pts)
            if (std::isfinite(pt.x()) && std::isfinite(pt.y())) valid.append(pt);
        drawMarkers(p, valid, m);
    }
}

QVector<LegendItem> Signal::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    it.marker = marker;
    return {it};
}

// ───────────────────────────── SignalXY ─────────────────────────────

AxisLimits SignalXY::axisLimits() const {
    AxisLimits lim = limitsOf(xs, ys);
    if (lim.hasData()) {
        lim.x = CoordinateRange(lim.x.min + xOffset, lim.x.max + xOffset);
        const double a = lim.y.min * yScale + yOffset, b = lim.y.max * yScale + yOffset;
        lim.y = CoordinateRange(std::min(a, b), std::max(a, b));
    }
    if (vertical) std::swap(lim.x, lim.y);  // xs 對到 Y、ys 對到 X
    return lim;
}

void SignalXY::render(RenderPack& rp) {
    // 一律用 const 參照讀：非 const 的 Y[i] 會讓 QVector 脫離共用，索引快取就白建了
    const QVector<double>& X = xs;
    const QVector<double>& Y = ys;
    const qsizetype n = pairCount(X, Y);
    if (n == 0) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;

    // 垂直方向時 xs 對到 Y 軸
    const CoordinateRange alongRange = vertical ? t.yr : t.xr;
    auto pointAt = [&](double along, double value) {
        along += xOffset;
        value = value * yScale + yOffset;
        return vertical ? t.p(value, along) : t.p(along, value);
    };

    // xs 遞增，所以用二分搜尋找可視範圍
    auto lower = std::lower_bound(X.begin(), X.begin() + n, alongRange.min - xOffset);
    auto upper = std::upper_bound(X.begin(), X.begin() + n, alongRange.max - xOffset);
    qsizetype i0 = std::max<qsizetype>(0, (lower - X.begin()) - 1);
    qsizetype i1 = std::min<qsizetype>(n - 1, (upper - X.begin()));
    if (i1 <= i0) {
        i0 = 0;
        i1 = n - 1;
    }
    qsizetype rf = 0, rl = n - 1;
    clampRenderRange(renderFirst, renderLast, n, rf, rl);
    i0 = std::max(i0, rf);
    i1 = std::min(i1, rl);
    if (i1 <= i0) return;

    const qsizetype visibleCount = i1 - i0 + 1;
    auto alongPx = [&](double along) { return vertical ? t.py(along) : t.px(along); };
    const double pxFrom = alongPx(X[i0] + xOffset), pxTo = alongPx(X[i1] + xOffset);
    const double pointsPerPixel = visibleCount / std::max(1.0, std::abs(pxTo - pxFrom));

    QVector<QPointF> pts;
    if (pointsPerPixel > 2.0) {
        // 每個像素行畫一條最小到最大的直線。行的邊界用二分搜尋找，
        // 不假設 X 等間距（原本照索引平均切，X 疏密不均時會切錯行）。
        const MinMaxIndex* idx = minMaxIndex();
        const double* data = Y.constData();
        const auto cols = buildEnvelope(
            pxFrom, pxTo, i0, i1,
            [&](double e) {
                const double x = (vertical ? t.cy(e) : t.cx(e)) - xOffset;
                return static_cast<qsizetype>(std::lower_bound(X.begin() + i0, X.begin() + i1 + 1, x) - X.begin());
            },
            [&](qsizetype a, qsizetype b, double& lo, double& hi) {
                if (!(idx ? idx->query(data, a, b, lo, hi) : scanMinMax(data, a, b, lo, hi))) return false;
                lo = lo * yScale + yOffset;
                hi = hi * yScale + yOffset;
                return true;
            },
            [&](double v) { return vertical ? t.px(v) : t.py(v); });
        drawEnvelope(p, cols, line, vertical);
        return;
    } else {
        pts.reserve(visibleCount);
        for (qsizetype i = i0; i <= i1; ++i) {
            pts.append((std::isfinite(X[i]) && std::isfinite(Y[i]))
                           ? pointAt(X[i], Y[i])
                           : QPointF(qQNaN(), qQNaN()));
        }
    }

    p.save();
    drawBrokenPolyline(p, pts, line);
    p.restore();

    if (marker.isVisible() && pointsPerPixel <= 2.0) {
        QVector<QPointF> valid;
        for (const QPointF& pt : pts)
            if (std::isfinite(pt.x()) && std::isfinite(pt.y())) valid.append(pt);
        drawMarkers(p, valid, marker);
    }
}

QVector<LegendItem> SignalXY::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    it.marker = marker;
    return {it};
}

// ───────────────────────────── FunctionPlot ─────────────────────────────

AxisLimits FunctionPlot::axisLimits() const {
    // 函數在任何 X 都有值，所以不主動限制範圍；只有使用者設了 xLimits 才回報。
    AxisLimits lim;
    if (xLimits.isValid()) {
        lim.x = xLimits;
        // Y 範圍由取樣決定，這裡粗略掃 200 點
        if (function) {
            for (int i = 0; i <= 200; ++i) {
                const double x = xLimits.min + xLimits.span() * i / 200.0;
                const double y = function(x);
                if (std::isfinite(y)) lim.y.expand(y);
            }
        }
    }
    return lim;
}

void FunctionPlot::render(RenderPack& rp) {
    if (!function) return;
    const Transform t = transform(rp);

    CoordinateRange xr = xLimits.isValid() ? xLimits : t.xr;
    // 可視範圍與求值範圍的交集，看不到的地方不用算
    const double from = std::max(xr.min, t.xr.min);
    const double to = std::min(xr.max, t.xr.max);
    if (!(to > from)) return;

    const int samples = std::max(2, static_cast<int>(rp.dataRect.width() * samplesPerPixel));
    QVector<QPointF> pts;
    pts.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        const double x = from + (to - from) * i / (samples - 1.0);
        const double y = function(x);
        pts.append(std::isfinite(y) ? t.p(x, y) : QPointF(qQNaN(), qQNaN()));
    }

    rp.painter->save();
    drawBrokenPolyline(*rp.painter, pts, line);
    rp.painter->restore();
}

QVector<LegendItem> FunctionPlot::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

// ───────────────────────────── FillY ─────────────────────────────

AxisLimits FillY::axisLimits() const {
    AxisLimits lim;
    const qsizetype n = std::min(xs.size(), std::min(ys1.size(), ys2.size()));
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i])) continue;
        lim.x.expand(xs[i]);
        lim.y.expand(ys1[i]);
        lim.y.expand(ys2[i]);
    }
    return lim;
}

void FillY::render(RenderPack& rp) {
    const Transform t = transform(rp);
    const qsizetype n = std::min(xs.size(), std::min(ys1.size(), ys2.size()));
    if (n < 2) return;

    QPolygonF poly;
    poly.reserve(n * 2);
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys1[i])) continue;
        poly.append(t.p(xs[i], ys1[i]));
    }
    for (qsizetype i = n - 1; i >= 0; --i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys2[i])) continue;
        poly.append(t.p(xs[i], ys2[i]));
    }
    if (poly.size() < 3) return;

    QPainter& p = *rp.painter;
    p.save();
    p.setPen(line.isVisible() ? line.pen() : QPen(Qt::NoPen));
    p.setBrush(fill.brush());
    p.drawPolygon(poly);
    p.restore();
}

QVector<LegendItem> FillY::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = fill;
    it.line = line;
    return {it};
}

// ───────────────────────────── BarPlot ─────────────────────────────

QVector<Bar> BarPlot::fromValues(const QVector<double>& values, QColor color) {
    QVector<Bar> out;
    out.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i) {
        Bar b;
        b.position = static_cast<double>(i);
        b.value = values[i];
        b.fillColor = color;
        out.append(b);
    }
    return out;
}

AxisLimits BarPlot::axisLimits() const {
    AxisLimits lim;
    for (const Bar& b : bars) {
        const double half = b.size / 2;
        CoordinateRange along(b.position - half, b.position + half);
        CoordinateRange value;
        value.expand(b.value + b.error);
        value.expand(b.value - b.error);
        value.expand(b.valueBase);
        if (includeZero) value.expand(0.0);
        if (horizontal) {
            lim.x.expand(value);
            lim.y.expand(along);
        } else {
            lim.x.expand(along);
            lim.y.expand(value);
        }
    }
    return lim;
}

void BarPlot::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    QFontMetricsF fm(valueLabelFont.font());

    // 第 0 輪畫長條（labelsOnTop 關著時順便畫標籤），第 1 輪只畫標籤
    for (int pass = 0; pass < 2; ++pass)
    for (const Bar& b : bars) {
        if (pass == 1 && !labelsOnTop) break;
        const double half = b.size / 2;
        QRectF r;
        if (horizontal) {
            r = QRectF(QPointF(t.px(std::min(b.valueBase, b.value)), t.py(b.position + half)),
                       QPointF(t.px(std::max(b.valueBase, b.value)), t.py(b.position - half)));
        } else {
            r = QRectF(QPointF(t.px(b.position - half), t.py(std::max(b.valueBase, b.value))),
                       QPointF(t.px(b.position + half), t.py(std::min(b.valueBase, b.value))));
        }
        r = r.normalized();

        if (pass == 0) {
            if (b.fill.hatch != HatchPattern::None && !b.fill.hasGradient()) {
                // 花紋的筆刷只有線條、沒有底色，先鋪底色再疊花紋
                const QColor base = b.fill.color.alpha() > 0 ? b.fill.color : b.fillColor;
                p.fillRect(r, base);
                p.setBrush(b.fill.brush());
            } else {
                p.setBrush(b.fill.isVisible() ? b.fill.brush(r) : QBrush(b.fillColor));
            }
            LineStyle border = borderLine;
            if (b.borderColor.alpha() > 0) border.color = b.borderColor;
            if (b.borderWidth > 0) border.width = b.borderWidth;
            p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
            p.drawRect(r);

            // 誤差線
            if (b.error > 0) {
                p.setPen(QPen(QColor(0, 0, 0), 1.0));
                if (horizontal) {
                    const double y = t.py(b.position);
                    const double x1 = t.px(b.value - b.error), x2 = t.px(b.value + b.error);
                    p.drawLine(QPointF(x1, y), QPointF(x2, y));
                    p.drawLine(QPointF(x1, y - 3), QPointF(x1, y + 3));
                    p.drawLine(QPointF(x2, y - 3), QPointF(x2, y + 3));
                } else {
                    const double x = t.px(b.position);
                    const double y1 = t.py(b.value - b.error), y2 = t.py(b.value + b.error);
                    p.drawLine(QPointF(x, y1), QPointF(x, y2));
                    p.drawLine(QPointF(x - 3, y1), QPointF(x + 3, y1));
                    p.drawLine(QPointF(x - 3, y2), QPointF(x + 3, y2));
                }
            }
        }  // pass == 0
        if ((pass == 1) != labelsOnTop) continue;

        // 數值標籤：畫在長條外側，正負值各往自己的方向長
        if (!b.label.isEmpty()) {
            Label lb;
            lb.text = b.label;
            lb.font = valueLabelFont;
            const double gap = 3 + (b.error > 0 ? std::abs(t.py(b.value) - t.py(b.value + b.error)) : 0);
            const bool pos = b.value >= b.valueBase;

            // 用「標籤的中心」定位，不是用它靠長條的那一邊。
            //
            // drawText 是繞 anchor 旋轉、再在轉過的座標系裡算對齊的，所以
            // 「貼齊下緣」這種對齊一旦轉了角度就不再是螢幕上的下緣 ——
            // 直式的數值標籤會整段倒進長條裡。置中則是轉不轉都一樣，
            // 把中心放到「外推半個外接框」的位置就對了。
            // 沒有旋轉時這和原本的寫法完全等價。
            if (b.labelColor.isValid()) lb.font.color = b.labelColor;
            const QSizeF ext = measureText(lb);
            QPointF at;
            if (b.labelPlacement == Bar::LabelPlacement::Center) {
                at = r.center();
            } else if (b.labelPlacement == Bar::LabelPlacement::InsideStart) {
                // 起點是 valueBase 那一端：正值的左／下緣，負值的右／上緣
                const double pad = 4;
                if (horizontal)
                    at = QPointF(pos ? r.left() + pad + ext.width() / 2 : r.right() - pad - ext.width() / 2,
                                 r.center().y());
                else
                    at = QPointF(r.center().x(), pos ? r.bottom() - pad - ext.height() / 2
                                                     : r.top() + pad + ext.height() / 2);
            } else if (horizontal) {
                at = QPointF(pos ? r.right() + gap + ext.width() / 2
                                 : r.left() - gap - ext.width() / 2,
                             r.center().y());
            } else {
                at = QPointF(r.center().x(), pos ? r.top() - gap - ext.height() / 2
                                                 : r.bottom() + gap + ext.height() / 2);
            }
            drawText(p, at, lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> BarPlot::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = FillStyle(bars.isEmpty() ? QColor(31, 119, 180) : bars.first().fillColor);
    if (!bars.isEmpty() && bars.first().fill.isVisible()) it.fill = bars.first().fill;
    it.line = LineStyle::none();
    if (!bars.isEmpty() && (bars.first().borderColor.alpha() > 0 || borderLine.isVisible())) {
        it.line = borderLine;
        if (bars.first().borderColor.alpha() > 0) it.line.color = bars.first().borderColor;
    }
    return {it};
}

// ───────────────────────────── ErrorBar ─────────────────────────────

AxisLimits ErrorBar::axisLimits() const {
    AxisLimits lim;
    const qsizetype n = pairCount(xs, ys);
    auto at = [](const QVector<double>& v, qsizetype i) {
        return i < v.size() ? v[i] : 0.0;
    };
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        lim.x.expand(xs[i] - at(xErrorsNegative, i));
        lim.x.expand(xs[i] + at(xErrorsPositive, i));
        lim.y.expand(ys[i] - at(yErrorsNegative, i));
        lim.y.expand(ys[i] + at(yErrorsPositive, i));
    }
    return lim;
}

void ErrorBar::render(RenderPack& rp) {
    const Transform t = transform(rp);
    const qsizetype n = pairCount(xs, ys);
    if (n == 0 || !line.isVisible()) return;

    auto at = [](const QVector<double>& v, qsizetype i) {
        return i < v.size() ? v[i] : 0.0;
    };

    QPainter& p = *rp.painter;
    p.save();
    p.setPen(line.pen());
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        const double cx = t.px(xs[i]);
        const double cy = t.py(ys[i]);

        const double yn = at(yErrorsNegative, i), yp = at(yErrorsPositive, i);
        if (yn != 0 || yp != 0) {
            const double y1 = t.py(ys[i] - yn), y2 = t.py(ys[i] + yp);
            p.drawLine(QPointF(cx, y1), QPointF(cx, y2));
            if (capSize > 0) {
                p.drawLine(QPointF(cx - capSize, y1), QPointF(cx + capSize, y1));
                p.drawLine(QPointF(cx - capSize, y2), QPointF(cx + capSize, y2));
            }
        }
        const double xn = at(xErrorsNegative, i), xp = at(xErrorsPositive, i);
        if (xn != 0 || xp != 0) {
            const double x1 = t.px(xs[i] - xn), x2 = t.px(xs[i] + xp);
            p.drawLine(QPointF(x1, cy), QPointF(x2, cy));
            if (capSize > 0) {
                p.drawLine(QPointF(x1, cy - capSize), QPointF(x1, cy + capSize));
                p.drawLine(QPointF(x2, cy - capSize), QPointF(x2, cy + capSize));
            }
        }
    }
    p.restore();
}

// ───────────────────────────── Heatmap ─────────────────────────────

CoordinateRect Heatmap::effectiveExtent() const {
    CoordinateRect e = (extent.width() != 0 || extent.height() != 0) ? extent : CoordinateRect(0, cols, 0, rows);
    if (cellCentered && cols > 1 && rows > 1) {
        // extent 是頭尾兩格的中心 —— 往外各推半格才是熱圖的外緣
        const double hx = e.width() / (cols - 1) / 2;
        const double hy = e.height() / (rows - 1) / 2;
        e = CoordinateRect(e.left - hx, e.right + hx, e.bottom - hy, e.top + hy);
    } else if (cellCentered && (extent.width() == 0 && extent.height() == 0)) {
        e = CoordinateRect(-0.5, cols - 0.5, -0.5, rows - 0.5);
    }
    return e;
}

CoordinateRange Heatmap::dataRange() const {
    if (colorRange.isValid()) return colorRange;
    CoordinateRange r;
    for (double v : values)
        if (std::isfinite(v)) r.expand(v);
    return r;
}

AxisLimits Heatmap::axisLimits() const {
    if (rows <= 0 || cols <= 0) return AxisLimits();
    const CoordinateRect e = effectiveExtent();
    return AxisLimits(e.xRange(), e.yRange());
}

void Heatmap::render(RenderPack& rp) {
    if (rows <= 0 || cols <= 0 || values.size() < static_cast<qsizetype>(rows) * cols) return;

    const CoordinateRange cr = dataRange();
    // 先把資料轉成 QImage，再讓 Qt 做縮放 —— 比逐格 drawRect 快一個數量級
    QImage img(cols, rows, QImage::Format_ARGB32_Premultiplied);
    for (int r = 0; r < rows; ++r) {
        const int srcRow = flipVertically ? (rows - 1 - r) : r;
        auto* scan = reinterpret_cast<QRgb*>(img.scanLine(r));
        for (int c = 0; c < cols; ++c) {
            const int srcCol = flipHorizontally ? (cols - 1 - c) : c;
            const double v = values.at(static_cast<qsizetype>(srcRow) * cols + srcCol);
            if (std::isnan(v) && transparentNaN) {
                scan[c] = qRgba(0, 0, 0, 0);
            } else {
                const QColor col = colormap.lookup(v, cr.min, cr.max);
                double a = col.alphaF() * std::clamp(opacity, 0.0, 1.0);
                const qsizetype idx = static_cast<qsizetype>(srcRow) * cols + srcCol;
                if (alphaMap.size() == values.size() && std::isfinite(alphaMap.at(idx)))
                    a *= std::clamp(alphaMap.at(idx), 0.0, 1.0);
                // 影像格式是預乘 alpha，半透明的顏色要先乘上去
                scan[c] = qPremultiply(qRgba(col.red(), col.green(), col.blue(), static_cast<int>(std::lround(a * 255))));
            }
        }
    }

    const Transform t = transform(rp);
    const QRectF target = t.pixelRect(effectiveExtent());

    QPainter& p = *rp.painter;
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    p.drawImage(target, img);
    p.restore();
}

// ───────────────────────────── AxisLine ─────────────────────────────

AxisLimits AxisLine::axisLimits() const {
    AxisLimits lim;
    if (!includeInAxisLimits) return lim;
    // 只回報自己所在的那一軸；另一軸留空，否則會把視野壓成一條線
    if (vertical) lim.x.expand(position);
    else          lim.y.expand(position);
    return lim;
}

void AxisLine::render(RenderPack& rp) {
    if (!line.isVisible()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(line.pen());

    // 線段的起訖（像素）。extent 沒設就是整個繪圖區。
    double a0, a1;
    if (vertical) {
        a0 = rp.dataRect.bottom;
        a1 = rp.dataRect.top;
        if (extent.isValid()) {
            a0 = std::clamp(t.py(extent.min), static_cast<double>(rp.dataRect.top), static_cast<double>(rp.dataRect.bottom));
            a1 = std::clamp(t.py(extent.max), static_cast<double>(rp.dataRect.top), static_cast<double>(rp.dataRect.bottom));
        }
    } else {
        a0 = rp.dataRect.left;
        a1 = rp.dataRect.right;
        if (extent.isValid()) {
            a0 = std::clamp(t.px(extent.min), static_cast<double>(rp.dataRect.left), static_cast<double>(rp.dataRect.right));
            a1 = std::clamp(t.px(extent.max), static_cast<double>(rp.dataRect.left), static_cast<double>(rp.dataRect.right));
        }
    }

    const double c = vertical ? t.px(position) : t.py(position);
    const bool outside = vertical ? (c < rp.dataRect.left - 1 || c > rp.dataRect.right + 1)
                                  : (c < rp.dataRect.top - 1 || c > rp.dataRect.bottom + 1);
    if (outside) { p.restore(); return; }
    if (vertical) p.drawLine(QPointF(c, a0), QPointF(c, a1));
    else          p.drawLine(QPointF(a0, c), QPointF(a1, c));

    if (text.isVisible()) {
        const double off = textOffset;
        if (textPosition == TextPosition::OnAxis || textPosition == TextPosition::OnOppositeAxis) {
            // 壓在軸上的色塊：置中對齊線，貼著繪圖區外緣
            const bool opp = textPosition == TextPosition::OnOppositeAxis;
            const QSizeF sz = measureText(text);
            const double pad = 3;
            QRectF box;
            if (vertical) {
                const double y = opp ? rp.dataRect.top - sz.height() - 2 * pad : rp.dataRect.bottom;
                box = QRectF(c - sz.width() / 2 - pad, y, sz.width() + 2 * pad, sz.height() + 2 * pad);
            } else {
                const double x = opp ? rp.dataRect.right : rp.dataRect.left - sz.width() - 2 * pad;
                box = QRectF(x, c - sz.height() / 2 - pad, sz.width() + 2 * pad, sz.height() + 2 * pad);
            }
            p.setPen(Qt::NoPen);
            p.setBrush(textBackground.isVisible() ? textBackground.brush(box) : QBrush(line.color));
            p.drawRect(box);
            drawText(p, box.center(), text, Qt::AlignHCenter | Qt::AlignVCenter);
        } else {
            // 一律用「標籤中心」定位：文字可能轉了角度，邊緣對齊轉過之後就不是螢幕上的邊緣
            const QSizeF ext = measureText(text);
            const double hw = ext.width() / 2, hh = ext.height() / 2;
            QPointF at;
            if (vertical) {
                const double top = std::min(a0, a1), bottom = std::max(a0, a1);
                const double x = c + off + hw;
                if (textPosition == TextPosition::InsideStart)       at = QPointF(x, bottom - off - hh);
                else if (textPosition == TextPosition::InsideCenter) at = QPointF(x, (top + bottom) / 2);
                else                                                 at = QPointF(x, top + off + hh);
            } else {
                const double left = std::min(a0, a1), right = std::max(a0, a1);
                const double y = c - off - hh;
                if (textPosition == TextPosition::InsideStart)       at = QPointF(left + off + hw, y);
                else if (textPosition == TextPosition::InsideCenter) at = QPointF((left + right) / 2, y);
                else                                                 at = QPointF(right - off - hw, y);
            }
            drawText(p, at, text, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> AxisLine::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

// ───────────────────────────── AxisSpan ─────────────────────────────

AxisLimits AxisSpan::axisLimits() const {
    AxisLimits lim;
    if (!includeInAxisLimits) return lim;
    if (vertical) {
        lim.x.expand(start);
        lim.x.expand(end);
    } else {
        lim.y.expand(start);
        lim.y.expand(end);
    }
    return lim;
}

void AxisSpan::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QRectF r;
    if (vertical) {
        const double x1 = t.px(std::min(start, end)), x2 = t.px(std::max(start, end));
        r = QRectF(QPointF(x1, rp.dataRect.top), QPointF(x2, rp.dataRect.bottom));
    } else {
        const double y1 = t.py(std::max(start, end)), y2 = t.py(std::min(start, end));
        r = QRectF(QPointF(rp.dataRect.left, y1), QPointF(rp.dataRect.right, y2));
    }
    QPainter& p = *rp.painter;
    p.save();
    p.setBrush(fill.brush());
    p.setPen(line.isVisible() ? line.pen() : QPen(Qt::NoPen));
    p.drawRect(r.normalized());
    p.restore();
}

QVector<LegendItem> AxisSpan::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = fill;
    it.line = LineStyle::none();
    return {it};
}

// ───────────────────────────── Crosshair ─────────────────────────────

void Crosshair::onCursorMoved(Coordinates at, Plot& plot) {
    if (!followCursor) return;
    m_snappedIndex = -1;
    m_snappedPlottable = nullptr;

    if (!snapToData || !at.isFinite()) {
        position = at;
        return;
    }
    // 吸附：找最近的資料點。找不到（或太遠）就回到跟著游標，
    // 不要硬吸到一個離很遠的點 —— 那會讀到完全不相干的數值。
    const HitResult hit = plot.hitTest(at, snapTolerancePx, snapMode);
    if (hit.isHit()) {
        position = hit.point;
        m_snappedIndex = hit.index;
        m_snappedPlottable = hit.plottable;
    } else {
        position = at;
    }
}

void Crosshair::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(lineStyle.pen());

    const double x = t.px(position.x);
    const double y = t.py(position.y);

    if (showVertical && x >= rp.dataRect.left && x <= rp.dataRect.right)
        p.drawLine(QPointF(x, rp.dataRect.top), QPointF(x, rp.dataRect.bottom));
    if (showHorizontal && y >= rp.dataRect.top && y <= rp.dataRect.bottom)
        p.drawLine(QPointF(rp.dataRect.left, y), QPointF(rp.dataRect.right, y));

    if (showLabels) {
        FontStyle f = labelFont;
        f.color = QColor(Qt::white);
        auto chip = [&](QPointF anchor, const QString& s, Qt::Alignment al) {
            Label lb;
            lb.text = s;
            lb.font = f;
            QSizeF sz = measureText(lb);
            QRectF box(anchor, sz);
            if (al & Qt::AlignRight)  box.moveLeft(anchor.x() - sz.width());
            if (al & Qt::AlignHCenter) box.moveLeft(anchor.x() - sz.width() / 2);
            if (al & Qt::AlignBottom) box.moveTop(anchor.y() - sz.height());
            if (al & Qt::AlignVCenter) box.moveTop(anchor.y() - sz.height() / 2);
            box.adjust(-3, -1, 3, 1);
            p.setPen(Qt::NoPen);
            p.setBrush(QBrush(labelBackground));
            p.drawRect(box);
            drawText(p, box.center(), lb, Qt::AlignHCenter | Qt::AlignVCenter);
        };
        if (showVertical && x >= rp.dataRect.left && x <= rp.dataRect.right)
            chip(QPointF(x, rp.dataRect.bottom + 2), formatTickLabel(position.x, t.xr.span() / 100),
                 Qt::AlignHCenter | Qt::AlignTop);
        if (showHorizontal && y >= rp.dataRect.top && y <= rp.dataRect.bottom)
            chip(QPointF(rp.dataRect.left - 2, y), formatTickLabel(position.y, t.yr.span() / 100),
                 Qt::AlignRight | Qt::AlignVCenter);
    }

    // 吸附到的資料點上畫一個圈，一眼看出讀的是哪一點
    if (m_snappedIndex >= 0 && snapMarker.isVisible() &&
        rp.dataRect.contains(x, y))
        drawMarker(p, QPointF(x, y), snapMarker);

    // 角落的讀數
    if (showReadout && readoutFont.color.alpha() > 0) {
        Label lb;
        lb.font = readoutFont;
        lb.text = (m_snappedIndex >= 0)
                      ? QStringLiteral("Index = %1\nX = %2\nY = %3")
                            .arg(m_snappedIndex)
                            .arg(position.x, 0, 'g', 6)
                            .arg(position.y, 0, 'g', 6)
                      : QStringLiteral("X = %1\nY = %2")
                            .arg(position.x, 0, 'g', 6)
                            .arg(position.y, 0, 'g', 6);

        const QSizeF sz = measureText(lb);
        const double pad = 6, margin = 10;
        double rx = rp.dataRect.left + margin;
        double ry = rp.dataRect.top + margin;
        if (readoutCorner & Qt::AlignRight) rx = rp.dataRect.right - margin - sz.width() - pad * 2;
        if (readoutCorner & Qt::AlignBottom) ry = rp.dataRect.bottom - margin - sz.height() - pad * 2;

        const QRectF box(rx, ry, sz.width() + pad * 2, sz.height() + pad * 2);
        p.setBrush(readoutBackground.brush());
        p.setPen(readoutBorder.isVisible() ? readoutBorder.pen() : QPen(Qt::NoPen));
        p.drawRect(box);
        drawText(p, box.center(), lb, Qt::AlignHCenter | Qt::AlignVCenter);
    }
    p.restore();
}

// ───────────────────────────── Text ─────────────────────────────

AxisLimits Text::axisLimits() const {
    AxisLimits lim;
    lim.expand(position);
    return lim;
}

void Text::render(RenderPack& rp) {
    if (!label.isVisible()) return;
    const Transform t = transform(rp);
    QPointF anchor = t.p(position) + QPointF(offsetX, offsetY);

    QPainter& p = *rp.painter;
    p.save();
    if (background.isVisible() || border.isVisible()) {
        QSizeF sz = measureText(label);
        QRectF box(anchor, sz);
        if (alignment & Qt::AlignRight)   box.moveLeft(anchor.x() - sz.width());
        if (alignment & Qt::AlignHCenter) box.moveLeft(anchor.x() - sz.width() / 2);
        if (alignment & Qt::AlignBottom)  box.moveTop(anchor.y() - sz.height());
        if (alignment & Qt::AlignVCenter) box.moveTop(anchor.y() - sz.height() / 2);
        box.adjust(-padding, -padding, padding, padding);
        p.setBrush(background.brush());
        p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
        p.drawRect(box);
    }
    drawText(p, anchor, label, alignment);
    p.restore();
}

// ───────────────────────────── Annotation ─────────────────────────────

void Annotation::render(RenderPack& rp) {
    if (!label.isVisible()) return;
    const QSizeF sz = measureText(label);
    const PixelRect& d = rp.dataRect;

    double x = d.left + marginX;
    double y = d.top + marginY;
    if (corner & Qt::AlignRight)   x = d.right - marginX - sz.width();
    if (corner & Qt::AlignHCenter) x = (d.left + d.right) / 2 - sz.width() / 2;
    if (corner & Qt::AlignBottom)  y = d.bottom - marginY - sz.height();
    if (corner & Qt::AlignVCenter) y = (d.top + d.bottom) / 2 - sz.height() / 2;

    QPainter& p = *rp.painter;
    p.save();
    QRectF box(x, y, sz.width(), sz.height());
    box.adjust(-padding, -padding, padding, padding);
    if (background.isVisible() || border.isVisible()) {
        p.setBrush(background.brush());
        p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
        p.drawRect(box);
    }
    drawText(p, QPointF(x, y), label, Qt::AlignLeft | Qt::AlignTop);
    p.restore();
}

// ───────────────────────────── Arrow ─────────────────────────────

AxisLimits Arrow::axisLimits() const {
    AxisLimits lim;
    lim.expand(tip);
    lim.expand(base);
    return lim;
}

void Arrow::render(RenderPack& rp) {
    const Transform t = transform(rp);
    const QPointF pTip = t.p(tip);
    const QPointF pBase = t.p(base);

    const double dx = pTip.x() - pBase.x();
    const double dy = pTip.y() - pBase.y();
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) return;
    const double ux = dx / len, uy = dy / len;

    QPainter& p = *rp.painter;
    p.save();
    const double hw = headWidth / 2;

    // 在 at 畫一個朝 (dirX, dirY) 的頭，回傳線該畫到哪裡（粗線不能從尖端露出來）
    auto drawHead = [&](QPointF at, double dirX, double dirY) -> QPointF {
        const QPointF back(at.x() - dirX * headLength, at.y() - dirY * headLength);
        const QPointF left(back.x() - dirY * hw, back.y() + dirX * hw);
        const QPointF right(back.x() + dirY * hw, back.y() - dirX * hw);
        auto fillOrStroke = [&](bool fill) {
            if (fill) { p.setBrush(QBrush(line.color)); p.setPen(Qt::NoPen); }
            else { p.setBrush(Qt::NoBrush); p.setPen(line.pen()); }
        };
        switch (headShape) {
        case ArrowHead::None:
            return at;
        case ArrowHead::Open: {
            QPen pen = line.pen();
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(QPolygonF() << left << at << right);
            return at;
        }
        case ArrowHead::Barbed: {
            // 尾端往尖端凹進去 40%
            const QPointF notch(at.x() - dirX * headLength * 0.6, at.y() - dirY * headLength * 0.6);
            fillOrStroke(filledHead);
            p.drawPolygon(QPolygonF() << at << left << notch << right);
            return notch;
        }
        case ArrowHead::Diamond: {
            const QPointF mid(at.x() - dirX * headLength / 2, at.y() - dirY * headLength / 2);
            fillOrStroke(filledHead);
            p.drawPolygon(QPolygonF() << at << QPointF(mid.x() - dirY * hw, mid.y() + dirX * hw)
                                      << back << QPointF(mid.x() + dirY * hw, mid.y() - dirX * hw));
            return back;
        }
        case ArrowHead::Circle: {
            const double r = std::max(hw, headLength / 2);
            const QPointF c(at.x() - dirX * r, at.y() - dirY * r);
            fillOrStroke(filledHead);
            p.drawEllipse(c, r, r);
            return QPointF(c.x() - dirX * r, c.y() - dirY * r);
        }
        case ArrowHead::Triangle:
        default:
            fillOrStroke(filledHead);
            p.drawPolygon(QPolygonF() << at << left << right);
            return back;
        }
    };

    const bool roomForHeads = len > headLength * (doubleEnded ? 2 : 1);
    const QPointF lineEnd = drawHead(pTip, ux, uy);
    const QPointF lineStart = doubleEnded ? drawHead(pBase, -ux, -uy) : pBase;
    if (line.isVisible()) {
        p.setPen(line.pen());
        p.setBrush(Qt::NoBrush);
        p.drawLine(roomForHeads ? lineStart : pBase, roomForHeads ? lineEnd : pTip);
    }
    p.restore();
}

QVector<LegendItem> Arrow::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

// ───────────────────────────── Shape ─────────────────────────────

AxisLimits Shape::axisLimits() const {
    AxisLimits lim;
    // 用 outline() 而不是原始的 rect —— 旋轉過的形狀會超出原本的外接框
    for (const Coordinates& c : outline(72)) lim.expand(c);
    return lim;
}

void Shape::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setBrush(fill.brush());
    p.setPen(line.isVisible() ? line.pen() : QPen(Qt::NoPen));

    // 沒旋轉的矩形／橢圓交給 Qt 畫（比較快也比較平滑）；
    // 一旦要旋轉就只能自己取樣頂點 —— 旋轉是在資料空間做的，
    // QPainter 的 rotate 轉的是像素空間，兩軸比例不同時結果會不一樣。
    if (rotationDegrees == 0 && kind == Kind::Rectangle) {
        p.drawRect(t.pixelRect(rect));
    } else if (rotationDegrees == 0 && kind == Kind::Ellipse) {
        p.drawEllipse(t.pixelRect(rect));
    } else {
        const QVector<Coordinates> pts = outline();
        QPolygonF poly;
        poly.reserve(pts.size());
        for (const Coordinates& c : pts)
            if (c.isFinite()) poly.append(t.p(c));
        if (poly.size() >= 2) p.drawPolygon(poly);
    }
    p.restore();
}

QVector<LegendItem> Shape::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = fill;
    it.line = line;
    return {it};
}

// ───────────────────────────── BubbleChart ─────────────────────────────

QVector<Bubble> BubbleChart::fromArrays(const QVector<double>& xs, const QVector<double>& ys,
                                        const QVector<double>& sizes, const Palette& palette) {
    QVector<Bubble> out;
    const qsizetype n = std::min({xs.size(), ys.size(), sizes.size()});
    out.reserve(n);
    for (qsizetype i = 0; i < n; ++i)
        out.append({xs[i], ys[i], sizes[i], QString(), palette.color(static_cast<int>(i))});
    return out;
}

CoordinateRange BubbleChart::sizeRange() const {
    CoordinateRange r;
    for (const Bubble& b : bubbles)
        if (std::isfinite(b.size)) r.expand(b.size);
    return r;
}

double BubbleChart::diameterFor(double size) const {
    const CoordinateRange r = sizeRange();
    if (!r.hasArea()) return (minDiameterPx + maxDiameterPx) / 2;
    const double f = std::clamp((size - r.min) / r.span(), 0.0, 1.0);
    if (!areaProportional) return minDiameterPx + (maxDiameterPx - minDiameterPx) * f;
    // 面積正比：先在面積上內插，再開根號回到直徑。
    // 直接在直徑上內插的話，size 差兩倍的兩個泡泡面積會差四倍。
    const double aMin = minDiameterPx * minDiameterPx;
    const double aMax = maxDiameterPx * maxDiameterPx;
    return std::sqrt(aMin + (aMax - aMin) * f);
}

AxisLimits BubbleChart::axisLimits() const {
    AxisLimits lim;
    for (const Bubble& b : bubbles) lim.expand({b.x, b.y});
    // 泡泡的半徑是像素，換不成資料單位，所以只能留一個比例的餘裕。
    // 最大的泡泡貼在邊上時仍可能被切到一點，視野要緊請自己 setAxisLimits。
    if (lim.hasData()) {
        const double padX = lim.x.span() * 0.08 + 1e-12;
        const double padY = lim.y.span() * 0.08 + 1e-12;
        lim.expand({lim.x.min - padX, lim.y.min - padY});
        lim.expand({lim.x.max + padX, lim.y.max + padY});
    }
    return lim;
}

void BubbleChart::render(RenderPack& rp) {
    if (bubbles.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 大的先畫，小的才不會被蓋住。原本的順序要留著，所以排索引不排資料。
    QVector<int> order;
    order.reserve(bubbles.size());
    for (int i = 0; i < bubbles.size(); ++i) order.append(i);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return bubbles[a].size > bubbles[b].size; });

    for (int i : order) {
        const Bubble& b = bubbles.at(i);
        if (!std::isfinite(b.x) || !std::isfinite(b.y)) continue;
        const double d = diameterFor(b.size);
        const QPointF c = t.p(b.x, b.y);

        QColor fill = b.fillColor.isValid() ? b.fillColor : QColor(31, 119, 180);
        fill.setAlphaF(std::clamp(fillOpacity, 0.0, 1.0));
        p.setBrush(QBrush(fill));
        p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
        p.drawEllipse(c, d / 2, d / 2);

        if (!b.label.isEmpty() && labelFont.color.alpha() > 0 && d >= minLabelDiameterPx) {
            Label lb;
            lb.text = b.label;
            lb.font = labelFont;
            drawText(p, c, lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }

    // ── 大小圖例：幾個同心圓，底部對齊 ──
    if (showSizeLegend && sizeLegendFont.color.alpha() > 0) {
        QVector<double> values = sizeLegendValues;
        if (values.isEmpty()) {
            const CoordinateRange r = sizeRange();
            if (r.hasArea()) values = {r.max, (r.min + r.max) / 2, r.min};
        }
        // 由大到小排，大的先畫才不會把小的蓋住
        std::sort(values.begin(), values.end(), [](double a, double b) { return a > b; });

        if (!values.isEmpty()) {
            const double biggest = diameterFor(values.first());
            const PixelRect& d = rp.dataRect;
            const double m = sizeLegendMarginPx;

            // 標題佔的高度要先算出來，不然最大的那個圓會被標題壓到
            double titleHeight = 0;
            if (!sizeLegendTitle.isEmpty()) {
                Label probe;
                probe.text = sizeLegendTitle;
                probe.font = sizeLegendFont;
                probe.font.bold = true;
                titleHeight = measureText(probe).height() + 6;
            }
            const double leader = 10;

            // 引線往「繪圖區內側」拉。靠右放卻把數字往右寫的話，
            // 圖元是被裁切到繪圖區的，文字會直接被切掉。
            const bool onLeft = (sizeLegendCorner & Qt::AlignLeft) != 0;
            const double side = onLeft ? 1.0 : -1.0;

            // 底部對齊的同心圓：圓心在同一條垂直線上，但底緣都貼著 baseY。
            // 這是大小圖例的慣例 —— 圓心對齊的話，小圓會浮在大圓中間，
            // 不容易比出直徑。
            const double cx = onLeft ? (d.left + m + biggest / 2)
                                     : (d.right - m - biggest / 2);
            const double baseY = (sizeLegendCorner & Qt::AlignBottom)
                                     ? (d.bottom - m)
                                     : (d.top + m + titleHeight + biggest);

            p.setBrush(Qt::NoBrush);
            for (double v : values) {
                const double dia = diameterFor(v);
                p.setPen(sizeLegendLine.isVisible() ? sizeLegendLine.pen() : QPen(Qt::NoPen));
                p.drawEllipse(QPointF(cx, baseY - dia / 2), dia / 2, dia / 2);

                // 從圓的頂端往內側拉一條線，數字標在線的尾端
                const QPointF top(cx, baseY - dia);
                const double tx = cx + side * (biggest / 2 + leader);
                if (sizeLegendLine.isVisible()) p.drawLine(top, QPointF(tx, top.y()));

                Label lb;
                lb.text = QString::number(v, 'g', 3);
                lb.font = sizeLegendFont;
                drawText(p, QPointF(tx + side * 3, top.y()),
                         lb, (onLeft ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
            }
            if (!sizeLegendTitle.isEmpty()) {
                Label lb;
                lb.text = sizeLegendTitle;
                lb.font = sizeLegendFont;
                lb.font.bold = true;
                drawText(p, QPointF(cx, baseY - biggest - 6), lb,
                         Qt::AlignHCenter | Qt::AlignBottom);
            }
        }
    }
    p.restore();
}

QVector<LegendItem> BubbleChart::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = FillStyle(bubbles.isEmpty() || !bubbles.first().fillColor.isValid()
                            ? QColor(31, 119, 180)
                            : bubbles.first().fillColor);
    return {it};
}

HitResult BubbleChart::hitTest(Coordinates target, const Transform& t, HitMode mode) const {
    HitResult best;
    const QPointF tp = t.p(target);
    for (int i = 0; i < bubbles.size(); ++i) {
        const Bubble& b = bubbles.at(i);
        if (!std::isfinite(b.x) || !std::isfinite(b.y)) continue;
        const QPointF q = t.p(b.x, b.y);
        const double dx = q.x() - tp.x();
        const double dy = q.y() - tp.y();
        const double dist = (mode == HitMode::NearestX) ? std::abs(dx) : std::hypot(dx, dy);
        if (!best.isHit() || dist < best.distancePx) {
            best.index = i;
            best.point = {b.x, b.y};
            best.distancePx = dist;
        }
    }
    return best;
}

Coordinates BubbleChart::pointAt(int index) const {
    if (index < 0 || index >= bubbles.size())
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()};
    return {bubbles[index].x, bubbles[index].y};
}

// ───────────────────────────── Sector / Arc ─────────────────────────────

namespace {

/// 扇形或弧線涵蓋的資料範圍。
///
/// 不能只取四個端點的外接框：掃過 +X 的弧，最右邊是 0° 那一點而不是任何端點。
/// 所以除了兩個端點之外，還要把落在掃掠範圍內的 0/90/180/270 這四個極值點算進去。
AxisLimits ellipseArcLimits(Coordinates center, double rx, double ry, double startDeg,
                            double sweepDeg, bool includeCenter) {
    AxisLimits lim;
    if (includeCenter) lim.expand(center);

    auto add = [&](double deg) {
        const double rad = deg * kPi / 180.0;
        lim.expand({center.x + rx * std::cos(rad), center.y + ry * std::sin(rad)});
    };
    add(startDeg);
    add(startDeg + sweepDeg);

    const double lo = std::min(startDeg, startDeg + sweepDeg);
    const double hi = std::max(startDeg, startDeg + sweepDeg);
    for (double q = std::floor(lo / 90.0) * 90.0; q <= hi + 1e-9; q += 90.0)
        if (q >= lo - 1e-9) add(q);

    return lim;
}

}  // namespace

AxisLimits Sector::axisLimits() const {
    // 外圈就夠了 —— 內圈一定在外圈裡面。圓心只有實心扇形才算進去：
    // 環狀扇形的圓心是挖空的，把它算進來範圍會無謂地變大。
    const bool solid = innerRadiusX <= 0 && innerRadiusY <= 0;
    return ellipseArcLimits(center, radiusX, radiusY, startAngle, sweepAngle, solid);
}

void Sector::render(RenderPack& rp) {
    const Transform t = transform(rp);
    const QPainterPath path =
        sectorPath(t, center, radiusX, radiusY, innerRadiusX, innerRadiusY, startAngle,
                   sweepAngle);
    if (path.isEmpty()) return;

    QPainter& p = *rp.painter;
    p.save();
    p.setBrush(fill.brush());
    p.setPen(line.isVisible() ? line.pen() : QPen(Qt::NoPen));
    p.drawPath(path);

    if (arcLabel.isVisible()) {
        // 用 X 方向的半徑。兩軸比例不同時弧是橢圓、字距會算不準，
        // 但環狀圖本來就該配 equalAxisScale。
        const double inner = std::max(innerRadiusX, 0.0);
        const double f = std::clamp(arcLabelRadiusFraction, 0.0, 1.0);
        const double r = inner + (radiusX - inner) * f;
        drawTextOnArc(p, t, center, r, startAngle + sweepAngle / 2, arcLabel);
    }
    p.restore();
}

QVector<LegendItem> Sector::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = fill;
    it.line = line;
    return {it};
}

AxisLimits Arc::axisLimits() const {
    return ellipseArcLimits(center, radiusX, radiusY, startAngle, sweepAngle, false);
}

void Arc::render(RenderPack& rp) {
    if (!line.isVisible()) return;
    const Transform t = transform(rp);
    const QPainterPath path = arcPath(t, center, radiusX, radiusY, startAngle, sweepAngle);
    if (path.isEmpty()) return;

    QPainter& p = *rp.painter;
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setPen(line.pen());
    p.drawPath(path);
    p.restore();
}

QVector<LegendItem> Arc::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

// ───────────────────────────── 堆疊輔助 ─────────────────────────────

QVector<StackedBand> stackSigned(const QVector<QVector<double>>& series) {
    QVector<StackedBand> out;
    if (series.isEmpty()) return out;

    qsizetype n = series.first().size();
    for (const QVector<double>& s : series) n = std::min(n, s.size());
    if (n <= 0) return out;

    out.resize(series.size());
    for (StackedBand& b : out) {
        b.lower.resize(n);
        b.upper.resize(n);
    }

    QVector<double> posSum(n, 0.0), negSum(n, 0.0);
    for (int k = 0; k < series.size(); ++k) {
        for (qsizetype i = 0; i < n; ++i) {
            const double v = series[k][i];
            if (!std::isfinite(v)) {
                // NaN 讓這一段斷開，但不影響累積 —— 把它當成 0 會讓後面的層位移
                out[k].lower[i] = std::numeric_limits<double>::quiet_NaN();
                out[k].upper[i] = std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            if (v >= 0) {
                out[k].lower[i] = posSum[i];
                posSum[i] += v;
                out[k].upper[i] = posSum[i];
            } else {
                out[k].upper[i] = negSum[i];
                negSum[i] += v;
                out[k].lower[i] = negSum[i];
            }
        }
    }
    return out;
}

// ───────────────────────────── Horizon ─────────────────────────────

double Horizon::resolvedBandValue() const {
    if (bandValue > 0) return bandValue;
    double maxAbs = 0;
    const qsizetype n = std::min(xs.size(), ys.size());
    for (qsizetype i = 0; i < n; ++i)
        if (std::isfinite(ys[i])) maxAbs = std::max(maxAbs, std::abs(ys[i] - baseline));
    const int b = std::max(1, bands);
    return (maxAbs > 0) ? maxAbs / b : 1.0;
}

AxisLimits Horizon::axisLimits() const {
    if (xs.isEmpty()) return AxisLimits();
    return AxisLimits(extent.xRange(), extent.yRange());
}

void Horizon::render(RenderPack& rp) {
    const qsizetype n = std::min(xs.size(), ys.size());
    if (n < 2) return;
    const int b = std::max(1, bands);
    const double h = resolvedBandValue();
    if (!(h > 0)) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(Qt::NoPen);

    // 一層一層畫，由淺到深。深的畫在上面，所以高的地方看起來最深。
    for (int k = 0; k < b; ++k) {
        for (int sign = 0; sign < 2; ++sign) {
            const bool positive = (sign == 0);
            const QVector<QColor>& ramp = positive ? positiveColors : negativeColors;
            if (ramp.isEmpty()) continue;
            const QColor c = ramp[std::min(k, static_cast<int>(ramp.size()) - 1)];

            QPolygonF poly;
            poly.reserve(n + 2);
            poly.append(t.p(xs[0], extent.bottom));
            bool any = false;
            for (qsizetype i = 0; i < n; ++i) {
                double amount = 0;
                if (std::isfinite(ys[i])) {
                    const double v = ys[i] - baseline;
                    // 這一層只負責 |v| 落在 [k·h, (k+1)·h] 的那一段
                    if ((positive && v > 0) || (!positive && v < 0)) {
                        amount = std::clamp(std::abs(v) - k * h, 0.0, h);
                        if (amount > 0) any = true;
                    }
                }
                // 一層的厚度對應整個 extent 的高度 —— 這就是「折疊」
                poly.append(t.p(xs[i], extent.bottom + amount / h * extent.height()));
            }
            poly.append(t.p(xs[n - 1], extent.bottom));
            if (!any) continue;
            p.setBrush(QBrush(c));
            p.drawPolygon(poly);
        }
    }
    p.restore();
}

QVector<StackedBand> stackStream(const QVector<QVector<double>>& series, StackOffset offset) {
    QVector<StackedBand> out;
    if (series.isEmpty()) return out;

    qsizetype m = series.first().size();
    for (const QVector<double>& s : series) m = std::min(m, s.size());
    if (m <= 0) return out;
    const int n = static_cast<int>(series.size());

    // 先取出非負的值。河流圖的前提是總量有意義，負值在這裡沒有定義。
    QVector<QVector<double>> v(n, QVector<double>(m, 0.0));
    for (int j = 0; j < n; ++j)
        for (qsizetype i = 0; i < m; ++i) {
            const double x = series[j][i];
            v[j][i] = (std::isfinite(x) && x > 0) ? x : 0.0;
        }

    QVector<double> total(m, 0.0);
    for (int j = 0; j < n; ++j)
        for (qsizetype i = 0; i < m; ++i) total[i] += v[j][i];

    if (offset == StackOffset::Expand) {
        for (int j = 0; j < n; ++j)
            for (qsizetype i = 0; i < m; ++i)
                v[j][i] = (total[i] > 0) ? v[j][i] / total[i] : 0.0;
        for (qsizetype i = 0; i < m; ++i) total[i] = (total[i] > 0) ? 1.0 : 0.0;
    }

    QVector<double> base(m, 0.0);
    switch (offset) {
    case StackOffset::Zero:
    case StackOffset::Expand:
        break;
    case StackOffset::Centered:
        for (qsizetype i = 0; i < m; ++i) base[i] = -total[i] / 2;
        break;
    case StackOffset::Wiggle: {
        // Byron–Wattenberg：每一步的位移是各層增量的加權和，
        // 權重讓「下面的層」影響大一點，因為它們會推動上面全部的層。
        for (qsizetype i = 1; i < m; ++i) {
            double sum = 0;
            for (int j = 0; j < n; ++j) sum += (n - j) * (v[j][i] - v[j][i - 1]);
            base[i] = base[i - 1] - sum / (n + 1);
        }
        // 整條平移，讓最低點落在 0，圖才不會跑到視野外
        double lowest = std::numeric_limits<double>::max();
        for (qsizetype i = 0; i < m; ++i) lowest = std::min(lowest, base[i]);
        for (qsizetype i = 0; i < m; ++i) base[i] -= lowest;
        break;
    }
    }

    out.resize(n);
    for (StackedBand& b : out) {
        b.lower.resize(m);
        b.upper.resize(m);
    }
    QVector<double> cursor = base;
    for (int j = 0; j < n; ++j)
        for (qsizetype i = 0; i < m; ++i) {
            out[j].lower[i] = cursor[i];
            cursor[i] += v[j][i];
            out[j].upper[i] = cursor[i];
        }
    return out;
}

// ───────────────────────────── Histogram ─────────────────────────────

Histogram Histogram::fromRange(const QVector<double>& values, double min, double max,
                               int binCount, bool includeOutliers) {
    Histogram h;
    if (!(max > min) || binCount <= 0) return h;
    h.binWidth = (max - min) / binCount;
    h.counts.fill(0.0, binCount);
    for (int i = 0; i <= binCount; ++i) h.binEdges.append(min + i * h.binWidth);
    for (int i = 0; i < binCount; ++i) h.binCenters.append(min + (i + 0.5) * h.binWidth);
    for (double v : values) {
        if (!std::isfinite(v)) continue;
        if ((v < min || v > max) && !includeOutliers) continue;
        const int bin = std::clamp(static_cast<int>(std::floor((v - min) / h.binWidth)), 0, binCount - 1);
        h.counts[bin] += 1.0;
    }
    return h;
}

Histogram Histogram::fromBinWidth(const QVector<double>& values, double min, double max,
                                  double binWidth, bool includeOutliers) {
    if (!(binWidth > 0) || !(max > min)) return Histogram();
    const int bins = std::max(1, static_cast<int>(std::ceil((max - min) / binWidth - 1e-9)));
    return fromRange(values, min, min + bins * binWidth, bins, includeOutliers);
}

QVector<double> Histogram::probabilities() const {
    double total = 0;
    for (double c : counts) total += c;
    QVector<double> out;
    for (double c : counts) out.append(total > 0 ? c / total : 0.0);
    return out;
}

QVector<double> Histogram::cumulativeCounts() const {
    QVector<double> out;
    double run = 0;
    for (double c : counts) out.append(run += c);
    return out;
}

Histogram Histogram::fromValues(const QVector<double>& values, int binCount) {
    Histogram h;
    CoordinateRange r;
    qsizetype finiteCount = 0;
    for (double v : values) {
        if (!std::isfinite(v)) continue;
        r.expand(v);
        ++finiteCount;
    }
    if (finiteCount == 0) return h;

    if (binCount <= 0) {
        // Sturges：1 + log2(n)。資料少時比固定箱數穩。
        binCount = std::max(1, static_cast<int>(std::ceil(1 + std::log2(static_cast<double>(finiteCount)))));
    }
    r.ensureArea(1.0);
    h.binWidth = r.span() / binCount;
    h.counts.resize(binCount);
    h.counts.fill(0.0);
    h.binEdges.reserve(binCount + 1);
    h.binCenters.reserve(binCount);
    for (int i = 0; i <= binCount; ++i) h.binEdges.append(r.min + i * h.binWidth);
    for (int i = 0; i < binCount; ++i) h.binCenters.append(r.min + (i + 0.5) * h.binWidth);

    for (double v : values) {
        if (!std::isfinite(v)) continue;
        int bin = static_cast<int>((v - r.min) / h.binWidth);
        bin = std::clamp(bin, 0, binCount - 1);  // 最大值正好落在右邊界
        h.counts[bin] += 1.0;
    }
    return h;
}

QVector<double> Histogram::densities() const {
    double total = 0;
    for (double c : counts) total += c;
    QVector<double> out;
    out.reserve(counts.size());
    const double denom = total * binWidth;
    for (double c : counts) out.append(denom > 0 ? c / denom : 0.0);
    return out;
}

QVector<Bar> Histogram::toBars(QColor color) const {
    QVector<Bar> out;
    out.reserve(counts.size());
    for (qsizetype i = 0; i < counts.size(); ++i) {
        Bar b;
        b.position = binCenters.value(i);
        b.value = counts[i];
        b.size = binWidth;
        b.fillColor = color;
        out.append(b);
    }
    return out;
}


// ───────────────────── 命中測試與拖曳 ─────────────────────

HitResult Scatter::hitTest(Coordinates target, const Transform& t, HitMode mode) const {
    HitResult r = nearestPoint(displayXs(), displayYs(), target, t, mode);
    r.plottable = const_cast<Scatter*>(this);
    return r;
}

HitResult Markers::hitTest(Coordinates target, const Transform& t, HitMode mode) const {
    HitResult r = nearestPoint(xs, ys, target, t, mode);
    r.plottable = const_cast<Markers*>(this);
    return r;
}

HitResult Signal::hitTest(Coordinates target, const Transform& t, HitMode mode) const {
    // Signal 的 X 是算出來的，不管哪種模式都是直接反推索引
    Q_UNUSED(mode);
    HitResult r;
    if (ys.isEmpty() || period == 0) return r;
    // X 是算出來的，直接反推索引就好，不必掃過全部點
    const qsizetype i = std::clamp<qsizetype>(
        static_cast<qsizetype>(std::llround((target.x - xOffset) / period)), 0, ys.size() - 1);
    r.index = static_cast<int>(i);
    r.point = {xOffset + i * period, ys[i] * yScale + yOffset};
    const QPointF a = t.p(r.point);
    const QPointF b = t.p(target);
    r.distancePx = std::hypot(a.x() - b.x(), a.y() - b.y());
    r.plottable = const_cast<Signal*>(this);
    return r;
}

HitResult SignalXY::hitTest(Coordinates target, const Transform& t, HitMode mode) const {
    HitResult r;
    const qsizetype n = pairCount(xs, ys);
    if (n == 0) return r;
    // xs 遞增，用二分搜尋找附近再比較左右兩點
    auto it = std::lower_bound(xs.begin(), xs.begin() + n, target.x - xOffset);
    const qsizetype hi = std::clamp<qsizetype>(it - xs.begin(), 0, n - 1);
    const qsizetype lo = std::max<qsizetype>(0, hi - 1);
    const QPointF pTarget = t.p(target);
    double bestSq = std::numeric_limits<double>::max();
    for (qsizetype i = lo; i <= hi; ++i) {
        const Coordinates c{xs[i] + xOffset, ys[i] * yScale + yOffset};
        const QPointF p = t.p(c);
        const double dx = p.x() - pTarget.x();
        const double dy = p.y() - pTarget.y();
        const double d2 = (mode == HitMode::NearestX) ? dx * dx : dx * dx + dy * dy;
        if (d2 < bestSq) {
            bestSq = d2;
            r.index = static_cast<int>(i);
            r.point = c;
        }
    }
    const QPointF pBest = t.p(r.point);
    r.distancePx = std::hypot(pBest.x() - pTarget.x(), pBest.y() - pTarget.y());
    r.plottable = const_cast<SignalXY*>(this);
    return r;
}

bool AxisLine::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    const double d = vertical ? std::abs(t.px(at.x) - t.px(position))
                              : std::abs(t.py(at.y) - t.py(position));
    return d <= tolerancePx;
}

bool AxisSpan::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    const double v = vertical ? at.x : at.y;
    if (resizable) {
        auto px = [&](double c) { return vertical ? t.px(c) : t.py(c); };
        const double pv = px(v);
        if (std::abs(pv - px(start)) <= tolerancePx || std::abs(pv - px(end)) <= tolerancePx)
            return true;
    }
    return draggable && v >= std::min(start, end) && v <= std::max(start, end);
}

void AxisSpan::beginDrag(Coordinates at, const Transform& t, double tolerancePx) {
    m_dragPart = draggable ? 0 : -1;
    if (!resizable) return;
    auto px = [&](double c) { return vertical ? t.px(c) : t.py(c); };
    const double pv = px(vertical ? at.x : at.y);
    const double ds = std::abs(pv - px(start)), de = std::abs(pv - px(end));
    if (ds <= tolerancePx && ds <= de) m_dragPart = 1;
    else if (de <= tolerancePx)        m_dragPart = 2;
}

void AxisSpan::dragBy(Coordinates delta) {
    const double d = vertical ? delta.x : delta.y;
    if (m_dragPart == 0 || m_dragPart == 1) start += d;
    if (m_dragPart == 0 || m_dragPart == 2) end += d;
}

bool Text::isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const {
    // 以文字框的範圍判斷，不是以錨點 —— 使用者抓的是看得到的那個框
    const QPointF anchor = t.p(position) + QPointF(offsetX, offsetY);
    const QSizeF sz = measureText(label);
    QRectF box(anchor, sz);
    if (alignment & Qt::AlignRight) box.moveLeft(anchor.x() - sz.width());
    if (alignment & Qt::AlignHCenter) box.moveLeft(anchor.x() - sz.width() / 2);
    if (alignment & Qt::AlignBottom) box.moveTop(anchor.y() - sz.height());
    if (alignment & Qt::AlignVCenter) box.moveTop(anchor.y() - sz.height() / 2);
    box.adjust(-tolerancePx, -tolerancePx, tolerancePx, tolerancePx);
    return box.contains(t.p(at));
}

}  // namespace qtplot

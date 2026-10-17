#include "Field.h"
#include "Translation.h"

#include <QLocale>
#include <QMap>
#include <QPolygonF>

#include <QHash>
#include <QPolygonF>

namespace qtplot {

namespace {

/// 在格子的一條邊上線性內插出等值點。
Coordinates lerpEdge(double xa, double ya, double va, double xb, double yb, double vb,
                     double level) {
    const double denom = vb - va;
    // 兩端值相同時取中點，避免除以零
    const double t = (std::abs(denom) < 1e-30) ? 0.5 : (level - va) / denom;
    const double c = std::clamp(t, 0.0, 1.0);
    return {xa + (xb - xa) * c, ya + (yb - ya) * c};
}

/// 把點量化成整數鍵，用來把零散線段接成折線。
/// 容差取格距的千分之一：太大會把不相干的線接在一起，太小會接不起來。
struct PointKey {
    qint64 x = 0;
    qint64 y = 0;
    bool operator==(const PointKey& o) const { return x == o.x && y == o.y; }
};

size_t qHash(const PointKey& k, size_t seed = 0) {
    return ::qHash(k.x, seed) ^ (::qHash(k.y, seed) << 1);
}

PointKey keyOf(Coordinates c, double tol) {
    return {static_cast<qint64>(std::llround(c.x / tol)),
            static_cast<qint64>(std::llround(c.y / tol))};
}

/// 把線段接成折線。線段來自相鄰的格子，端點會精確重合，所以用端點查表就接得起來。
QVector<QVector<Coordinates>> joinSegments(const QVector<QPair<Coordinates, Coordinates>>& segs,
                                           double tol) {
    QVector<QVector<Coordinates>> out;
    if (segs.isEmpty()) return out;

    // 端點 → 還沒用掉的線段索引
    QHash<PointKey, QVector<int>> index;
    for (int i = 0; i < segs.size(); ++i) {
        index[keyOf(segs[i].first, tol)].append(i);
        index[keyOf(segs[i].second, tol)].append(i);
    }
    QVector<bool> used(segs.size(), false);

    auto takeNext = [&](const Coordinates& at, int exclude) -> int {
        const auto it = index.constFind(keyOf(at, tol));
        if (it == index.constEnd()) return -1;
        for (int i : *it)
            if (i != exclude && !used[i]) return i;
        return -1;
    };

    for (int start = 0; start < segs.size(); ++start) {
        if (used[start]) continue;
        used[start] = true;

        QVector<Coordinates> line;
        line.append(segs[start].first);
        line.append(segs[start].second);

        // 往後接
        int prev = start;
        for (;;) {
            const int next = takeNext(line.last(), prev);
            if (next < 0) break;
            used[next] = true;
            const auto& s = segs[next];
            // 接上去的那一端要是「另一端」
            line.append(keyOf(s.first, tol) == keyOf(line.last(), tol) ? s.second : s.first);
            prev = next;
        }
        // 往前接
        prev = start;
        for (;;) {
            const int next = takeNext(line.first(), prev);
            if (next < 0) break;
            used[next] = true;
            const auto& s = segs[next];
            line.prepend(keyOf(s.first, tol) == keyOf(line.first(), tol) ? s.second : s.first);
            prev = next;
        }
        out.append(line);
    }
    return out;
}

}  // namespace

// ───────────────────── marching squares ─────────────────────

QVector<QVector<Coordinates>> marchingSquares(const QVector<double>& values, int rows, int cols,
                                              double level, const CoordinateRect& extent) {
    QVector<QVector<Coordinates>> out;
    if (rows < 2 || cols < 2 || values.size() < static_cast<qsizetype>(rows) * cols) return out;

    const double dx = (cols > 1) ? extent.width() / (cols - 1) : 0;
    const double dy = (rows > 1) ? extent.height() / (rows - 1) : 0;
    auto xAt = [&](int c) { return extent.left + c * dx; };
    auto yAt = [&](int r) { return extent.bottom + r * dy; };
    auto at = [&](int r, int c) { return values[static_cast<qsizetype>(r) * cols + c]; };

    QVector<QPair<Coordinates, Coordinates>> segs;

    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            // 角落：bl = (r,c), br = (r,c+1), tr = (r+1,c+1), tl = (r+1,c)
            const double vbl = at(r, c), vbr = at(r, c + 1);
            const double vtr = at(r + 1, c + 1), vtl = at(r + 1, c);
            // 任一角是 NaN 就跳過這個格子 —— 缺資料的地方不該憑空生出輪廓
            if (!std::isfinite(vbl) || !std::isfinite(vbr) || !std::isfinite(vtr) ||
                !std::isfinite(vtl))
                continue;

            const double x0 = xAt(c), x1 = xAt(c + 1);
            const double y0 = yAt(r), y1 = yAt(r + 1);

            int idx = 0;
            if (vbl > level) idx |= 1;
            if (vbr > level) idx |= 2;
            if (vtr > level) idx |= 4;
            if (vtl > level) idx |= 8;
            if (idx == 0 || idx == 15) continue;

            // 四條邊上的等值點
            const Coordinates eBottom = lerpEdge(x0, y0, vbl, x1, y0, vbr, level);
            const Coordinates eRight = lerpEdge(x1, y0, vbr, x1, y1, vtr, level);
            const Coordinates eTop = lerpEdge(x0, y1, vtl, x1, y1, vtr, level);
            const Coordinates eLeft = lerpEdge(x0, y0, vbl, x0, y1, vtl, level);

            // 不要叫 emit —— 那是 Qt 的巨集
            auto addSeg = [&](const Coordinates& a, const Coordinates& b) {
                segs.append({a, b});
            };

            switch (idx) {
            case 1: case 14: addSeg(eLeft, eBottom); break;
            case 2: case 13: addSeg(eBottom, eRight); break;
            case 3: case 12: addSeg(eLeft, eRight); break;
            case 4: case 11: addSeg(eRight, eTop); break;
            case 6: case 9:  addSeg(eBottom, eTop); break;
            case 7: case 8:  addSeg(eLeft, eTop); break;
            case 5:
            case 10: {
                // 鞍點：兩種接法都成立。用格子中心的平均值決定哪一種，
                // 隨便挑的話相鄰格子會接出互相穿越的線。
                const double center = (vbl + vbr + vtr + vtl) / 4;
                const bool centerAbove = center > level;
                if (idx == 5) {
                    if (centerAbove) { addSeg(eLeft, eTop); addSeg(eBottom, eRight); }
                    else             { addSeg(eLeft, eBottom); addSeg(eRight, eTop); }
                } else {
                    if (centerAbove) { addSeg(eLeft, eBottom); addSeg(eRight, eTop); }
                    else             { addSeg(eLeft, eTop); addSeg(eBottom, eRight); }
                }
                break;
            }
            default: break;
            }
        }
    }

    const double tol = std::max(std::abs(dx), std::abs(dy)) * 1e-3;
    return joinSegments(segs, tol > 0 ? tol : 1e-9);
}

// ───────────────────────── ContourLines ─────────────────────────

CoordinateRect ContourLines::latticeExtentFor(const CoordinateRect& heatmapExtent, int rows,
                                              int cols) {
    if (rows < 2 || cols < 2) return heatmapExtent;
    const double halfX = heatmapExtent.width() / cols / 2;
    const double halfY = heatmapExtent.height() / rows / 2;
    return CoordinateRect(heatmapExtent.left + halfX, heatmapExtent.right - halfX,
                          heatmapExtent.bottom + halfY, heatmapExtent.top - halfY);
}

CoordinateRect ContourLines::effectiveExtent() const {
    if (extent.width() != 0 || extent.height() != 0) return extent;
    return CoordinateRect(0, std::max(1, cols - 1), 0, std::max(1, rows - 1));
}

CoordinateRange ContourLines::dataRange() const {
    CoordinateRange r;
    for (double v : values)
        if (std::isfinite(v)) r.expand(v);
    return r;
}

QVector<double> ContourLines::effectiveLevels() const {
    if (!levels.isEmpty()) return levels;
    QVector<double> out;
    const CoordinateRange r = dataRange();
    const int n = std::max(1, levelCount);
    if (!r.hasArea()) return out;
    // 不取頭尾：等於最小值或最大值的等值線要嘛沒有線、要嘛貼著邊界，沒有資訊
    for (int i = 1; i <= n; ++i) out.append(r.min + r.span() * i / (n + 1.0));
    return out;
}

AxisLimits ContourLines::axisLimits() const {
    if (rows < 2 || cols < 2) return AxisLimits();
    const CoordinateRect e = effectiveExtent();
    return AxisLimits(e.xRange(), e.yRange());
}

void ContourLines::render(RenderPack& rp) {
    if (rows < 2 || cols < 2) return;
    const QVector<double> lv = effectiveLevels();
    if (lv.isEmpty()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    CoordinateRange cr = colorRange;
    if (!cr.isValid() && lv.size() > 1) cr = CoordinateRange(lv.first(), lv.last());

    CoordinateRect e = effectiveExtent();
    if (!firstRowAtBottom) {
        // 第 0 列要畫在上方 → 把 y 的對應上下翻轉
        std::swap(e.bottom, e.top);
    }

    for (int li = 0; li < lv.size(); ++li) {
        const double level = lv.at(li);
        const QVector<QVector<Coordinates>> lines =
            marchingSquares(values, rows, cols, level, e);
        if (lines.isEmpty()) continue;

        LineStyle ls = line;
        if (useColormap) ls.color = colormap.lookup(level, cr.min, cr.max);
        p.setPen(ls.pen());
        p.setBrush(Qt::NoBrush);

        const QVector<Coordinates>* longest = nullptr;
        for (const QVector<Coordinates>& poly : lines) {
            if (poly.size() < 2) continue;
            QPolygonF pts;
            pts.reserve(poly.size());
            for (const Coordinates& c : poly) pts.append(t.p(c));
            p.drawPolyline(pts);
            if (!longest || poly.size() > longest->size()) longest = &poly;
        }

        // 標籤畫在最長那條線的中點，太短的就不標
        if (showLabels && longest && longest->size() >= static_cast<qsizetype>(minPointsForLabel) &&
            labelFont.color.alpha() > 0) {
            // 每一層的標籤沿著線錯開，相鄰等值的標籤才不會疊在一起
            const double frac = 0.3 + 0.4 * ((li % 3) / 3.0);
            const qsizetype mid =
                std::clamp<qsizetype>(static_cast<qsizetype>(longest->size() * frac), 1,
                                      longest->size() - 2);
            const QPointF a = t.p(longest->at(mid - 1));
            const QPointF b = t.p(longest->at(mid + 1));
            Label lb;
            lb.text = QString::number(level, 'g', std::max(1, labelPrecision));
            lb.font = labelFont;
            if (useColormap) lb.font.color = ls.color;
            if (rotateLabels) {
                double deg = std::atan2(b.y() - a.y(), b.x() - a.x()) * 180.0 / kPi;
                // 文字永遠保持可讀的方向，不要上下顛倒
                if (deg > 90) deg -= 180;
                if (deg < -90) deg += 180;
                lb.font.rotation = deg;
            }
            // 先用底色把線蓋掉一小段，文字才不會疊在線上
            const QPointF c = t.p(longest->at(mid));
            const QSizeF sz = measureText(lb);
            p.save();
            p.setPen(Qt::NoPen);
            p.setBrush(QBrush(labelBackground));
            p.drawRect(QRectF(c.x() - sz.width() / 2 - 1, c.y() - sz.height() / 2,
                              sz.width() + 2, sz.height()));
            p.restore();
            drawText(p, c, lb, Qt::AlignHCenter | Qt::AlignVCenter);
            p.setPen(ls.pen());
        }
    }
    p.restore();
}

QVector<LegendItem> ContourLines::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

// ───────────────────────── VectorField ─────────────────────────

AxisLimits VectorField::axisLimits() const {
    AxisLimits lim;
    for (const Coordinates& c : positions) lim.expand(c);
    // 箭頭會從格點往外伸，留一點邊
    if (lim.x.isValid()) {
        lim.x.pad(0.05);
        lim.y.pad(0.05);
    }
    return lim;
}

void VectorField::render(RenderPack& rp) {
    const qsizetype n = std::min(positions.size(), vectors.size());
    if (n == 0) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    double maxMag = 0;
    for (qsizetype i = 0; i < n; ++i) maxMag = std::max(maxMag, vectors[i].magnitude());
    if (!(maxMag > 0)) {
        p.restore();
        return;
    }

    // 自動縮放：最長的箭頭剛好等於格點間距，箭頭才不會互相穿過
    double s = scale;
    if (!(s > 0)) {
        double minSpacing = std::numeric_limits<double>::max();
        for (qsizetype i = 1; i < n; ++i) {
            const double d = std::hypot(positions[i].x - positions[i - 1].x,
                                        positions[i].y - positions[i - 1].y);
            if (d > 1e-12) minSpacing = std::min(minSpacing, d);
        }
        if (minSpacing == std::numeric_limits<double>::max()) minSpacing = 1.0;
        s = minSpacing / maxMag;
    }

    CoordinateRange cr = colorRange;
    if (!cr.isValid()) cr = CoordinateRange(0, maxMag);

    for (qsizetype i = 0; i < n; ++i) {
        const Coordinates& pos = positions[i];
        const Vector2& v = vectors[i];
        const double mag = v.magnitude();
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !(mag > 0)) continue;

        // normalize：長度一律取最大值，大小改由顏色表示
        const double len = normalize ? maxMag : mag;
        const double dx = v.x / mag * len * s;
        const double dy = v.y / mag * len * s;

        Coordinates base = pos;
        switch (anchor) {
        case Anchor::Base:   break;
        case Anchor::Center: base = {pos.x - dx / 2, pos.y - dy / 2}; break;
        case Anchor::Tip:    base = {pos.x - dx, pos.y - dy}; break;
        }
        const Coordinates tip{base.x + dx, base.y + dy};

        LineStyle ls = line;
        if (useColormap) ls.color = colormap.lookup(mag, cr.min, cr.max);

        QPointF pBase = t.p(base);
        QPointF pTip = t.p(tip);
        if (maxArrowLengthPx > 0) {
            // 以對齊點為中心縮短，錨點才不會跑掉
            const double full = std::hypot(pTip.x() - pBase.x(), pTip.y() - pBase.y());
            if (full > maxArrowLengthPx) {
                const double k = maxArrowLengthPx / full;
                const QPointF a = t.p(pos);
                pBase = a + (pBase - a) * k;
                pTip = a + (pTip - a) * k;
            }
        }
        const double px = pTip.x() - pBase.x();
        const double py = pTip.y() - pBase.y();
        const double plen = std::hypot(px, py);
        if (plen < 0.5) continue;  // 短到看不見就別畫
        const double ux = px / plen, uy = py / plen;

        // 箭頭太短時頭部要跟著縮，不然整支箭都是頭
        const double head = std::min(headLength, plen * 0.6);
        const double hw = headWidth / 2 * (head / std::max(headLength, 1e-9));
        const QPointF headBase(pTip.x() - ux * head, pTip.y() - uy * head);

        p.setPen(ls.pen());
        p.drawLine(pBase, headBase);

        QPolygonF arrow;
        arrow << pTip << QPointF(headBase.x() - uy * hw, headBase.y() + ux * hw)
              << QPointF(headBase.x() + uy * hw, headBase.y() - ux * hw);
        p.setPen(Qt::NoPen);
        p.setBrush(QBrush(ls.color));
        p.drawPolygon(arrow);
    }
    p.restore();
}

QVector<LegendItem> VectorField::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}

VectorField VectorField::fromFunction(const std::function<Vector2(double, double)>& f,
                                      const CoordinateRect& area, int nx, int ny) {
    VectorField field;
    if (!f || nx < 1 || ny < 1) return field;
    field.positions.reserve(static_cast<qsizetype>(nx) * ny);
    field.vectors.reserve(static_cast<qsizetype>(nx) * ny);
    for (int j = 0; j < ny; ++j) {
        const double y = (ny == 1) ? area.bottom
                                   : area.bottom + area.height() * j / (ny - 1.0);
        for (int i = 0; i < nx; ++i) {
            const double x = (nx == 1) ? area.left : area.left + area.width() * i / (nx - 1.0);
            field.positions.append({x, y});
            field.vectors.append(f(x, y));
        }
    }
    return field;
}

// ───────────────────────── sampleField ─────────────────────────

QVector<double> sampleField(const std::function<double(double, double)>& f,
                            const CoordinateRect& area, int rows, int cols) {
    QVector<double> out;
    if (!f || rows < 1 || cols < 1) return out;
    out.resize(static_cast<qsizetype>(rows) * cols);
    for (int r = 0; r < rows; ++r) {
        const double y = (rows == 1) ? area.bottom
                                     : area.bottom + area.height() * r / (rows - 1.0);
        for (int c = 0; c < cols; ++c) {
            const double x = (cols == 1) ? area.left : area.left + area.width() * c / (cols - 1.0);
            out[static_cast<qsizetype>(r) * cols + c] = f(x, y);
        }
    }
    return out;
}


QVector<double> gridScattered(const QVector<Coordinates>& points, const QVector<double>& values,
                              const CoordinateRect& area, int rows, int cols, double power) {
    const qsizetype n = std::min(points.size(), values.size());
    return sampleField(
        [&](double x, double y) {
            double num = 0, den = 0;
            for (qsizetype i = 0; i < n; ++i) {
                if (!std::isfinite(values[i])) continue;
                const double d2 = (points[i].x - x) * (points[i].x - x) +
                                  (points[i].y - y) * (points[i].y - y);
                if (d2 < 1e-24) return values[i];  // 正好落在量測點上
                const double w = std::pow(d2, -power / 2);
                num += w * values[i];
                den += w;
            }
            return den > 0 ? num / den : std::numeric_limits<double>::quiet_NaN();
        },
        area, rows, cols);
}

// ───────────────────────── Hexbin ─────────────────────────

namespace {

/// 六角格的幾何：給「格距」dx（相鄰兩欄的水平距離），回傳列距。
///
/// 正六邊形的中心排成交錯的格子：同一列的間距是 dx，
/// 相鄰兩列錯開 dx/2，列距是 dx * sqrt(3)/2。
double hexRowStep(double dx) { return dx * std::sqrt(3.0) / 2.0; }

}  // namespace

QVector<HexCell> Hexbin::bins() const {
    QVector<HexCell> out;
    const qsizetype n = std::min(xs.size(), ys.size());
    if (n == 0 || columns < 1) return out;

    CoordinateRect box = extent;
    if (!(box.width() > 0) || !(box.height() > 0)) {
        CoordinateRange xr, yr;
        for (qsizetype i = 0; i < n; ++i) {
            if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
            xr.expand(xs[i]);
            yr.expand(ys[i]);
        }
        if (!xr.hasArea() || !yr.hasArea()) return out;
        box = CoordinateRect(xr.min, xr.max, yr.min, yr.max);
    }

    const double dx = box.width() / columns;
    const double dy = hexRowStep(dx);
    if (!(dx > 0) || !(dy > 0)) return out;

    // 用 QMap 而不是 QHash：QHash 的走訪順序每個行程都不一樣，
    // 輸出的 PNG 會因此不可重現。
    QMap<QPair<int, int>, int> counts;
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;

        // 候選格：直接取整得到的那一格，以及上下錯開的那一格。
        // 六角格的歸屬不能只看矩形區間 —— 邊界是斜的，
        // 只取整的話靠近交界的點會被分到錯的格子，畫出來會有鋸齒狀的條紋。
        const double fy = (ys[i] - box.bottom) / dy;
        const int row0 = static_cast<int>(std::floor(fy));
        QPair<int, int> best{0, 0};
        double bestDist = std::numeric_limits<double>::max();
        for (int row : {row0, row0 + 1}) {
            const double offset = (row & 1) ? dx / 2 : 0.0;
            const int col = static_cast<int>(
                std::round((xs[i] - box.left - offset) / dx));
            const double cx = box.left + offset + col * dx;
            const double cy = box.bottom + row * dy;
            const double d = (xs[i] - cx) * (xs[i] - cx) + (ys[i] - cy) * (ys[i] - cy);
            if (d < bestDist) {
                bestDist = d;
                best = {col, row};
            }
        }
        ++counts[best];
    }

    out.reserve(counts.size());
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        const int col = it.key().first, row = it.key().second;
        const double offset = (row & 1) ? dx / 2 : 0.0;
        out.append({box.left + offset + col * dx, box.bottom + row * dy, it.value()});
    }
    return out;
}

int Hexbin::maxCount() const {
    int m = 0;
    for (const HexCell& c : bins()) m = std::max(m, c.count);
    return m;
}

AxisLimits Hexbin::axisLimits() const {
    AxisLimits lim;
    if (extent.width() > 0 && extent.height() > 0)
        return AxisLimits(extent.xRange(), extent.yRange());
    const qsizetype n = std::min(xs.size(), ys.size());
    for (qsizetype i = 0; i < n; ++i)
        if (std::isfinite(xs[i]) && std::isfinite(ys[i])) lim.expand({xs[i], ys[i]});
    return lim;
}

void Hexbin::render(RenderPack& rp) {
    const QVector<HexCell> cells = bins();
    if (cells.isEmpty()) return;

    CoordinateRange range = countRange;
    if (!range.isValid() || !range.hasArea()) {
        int m = 0;
        for (const HexCell& c : cells) m = std::max(m, c.count);
        range = CoordinateRange(0, std::max(1, m));
    }

    // 格距要從資料推回來，才能和 bins() 用的一致
    CoordinateRect box = extent;
    if (!(box.width() > 0)) {
        CoordinateRange xr, yr;
        for (const HexCell& c : cells) {
            xr.expand(c.x);
            yr.expand(c.y);
        }
        box = CoordinateRect(xr.min, xr.max, yr.min, yr.max);
        const AxisLimits lim = axisLimits();
        if (lim.hasData()) box = CoordinateRect(lim.x.min, lim.x.max, lim.y.min, lim.y.max);
    }
    const double dx = box.width() / std::max(1, columns);
    const double r = dx / std::sqrt(3.0) * std::clamp(sizeFraction, 0.1, 1.0);

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));

    for (const HexCell& c : cells) {
        if (c.count < minCount) continue;
        QPolygonF hex;
        // 尖端朝上的正六邊形：頂點在 90°、150°、…
        for (int k = 0; k < 6; ++k) {
            const double a = kPi / 2 + k * kPi / 3;
            hex.append(t.p(c.x + r * std::cos(a), c.y + r * std::sin(a)));
        }
        p.setBrush(QBrush(colormap.lookup(c.count, range.min, range.max)));
        p.drawPolygon(hex);
    }
    p.restore();
}


// ───────────────────────── 日曆熱圖 ─────────────────────────

QDate CalendarHeatmap::effectiveFirst() const {
    if (firstDate.isValid()) return firstDate;
    QDate first;
    for (const CalendarValue& v : values)
        if (v.date.isValid() && (!first.isValid() || v.date < first)) first = v.date;
    return first;
}

QDate CalendarHeatmap::effectiveLast() const {
    if (lastDate.isValid()) return lastDate;
    QDate last;
    for (const CalendarValue& v : values)
        if (v.date.isValid() && (!last.isValid() || v.date > last)) last = v.date;
    return last;
}

CoordinateRange CalendarHeatmap::effectiveColorRange() const {
    if (colorRange.isValid()) return colorRange;
    CoordinateRange r;
    for (const CalendarValue& v : values) r.expand(v.value);
    if (!r.hasArea()) r = CoordinateRange(r.isValid() ? r.min : 0.0,
                                          r.isValid() ? r.min + 1.0 : 1.0);
    return r;
}

namespace {

/// 這一天是該週的第幾天（0 起算）。
int weekdayIndex(const QDate& d, bool mondayFirst) {
    // QDate::dayOfWeek() 是 1=週一 … 7=週日
    const int dow = d.dayOfWeek();
    return mondayFirst ? dow - 1 : dow % 7;
}

/// 含 d 的那一週的第一天。
QDate weekStart(const QDate& d, bool mondayFirst) {
    return d.addDays(-weekdayIndex(d, mondayFirst));
}

}  // namespace

int CalendarHeatmap::weekCount() const {
    const QDate first = effectiveFirst();
    const QDate last = effectiveLast();
    if (!first.isValid() || !last.isValid() || last < first) return 0;
    const QDate a = weekStart(first, weekStartsMonday);
    const QDate b = weekStart(last, weekStartsMonday);
    return static_cast<int>(a.daysTo(b) / 7) + 1;
}

QPoint CalendarHeatmap::cellFor(const QDate& date) const {
    const QDate first = effectiveFirst();
    const QDate last = effectiveLast();
    if (!date.isValid() || !first.isValid() || date < first || date > last) return QPoint(-1, -1);
    const QDate origin = weekStart(first, weekStartsMonday);
    const int week = static_cast<int>(origin.daysTo(weekStart(date, weekStartsMonday)) / 7);
    return QPoint(week, weekdayIndex(date, weekStartsMonday));
}

AxisLimits CalendarHeatmap::axisLimits() const {
    return AxisLimits(extent.left, extent.right, extent.bottom, extent.top);
}

void CalendarHeatmap::render(RenderPack& rp) {
    const QDate first = effectiveFirst();
    const QDate last = effectiveLast();
    const int weeks = weekCount();
    if (weeks <= 0) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QRectF box = t.pixelRect(extent);
    const double cw = box.width() / weeks;
    const double ch = box.height() / 7.0;
    const double gapX = cw * std::clamp(gapFraction, 0.0, 0.5);
    const double gapY = ch * std::clamp(gapFraction, 0.0, 0.5);
    const double radius = std::min(cw - gapX, ch - gapY) * std::clamp(cornerFraction, 0.0, 0.5);

    auto cellRect = [&](int week, int weekday) {
        // 星期一在最上面：weekday 0 對應 box.top()
        return QRectF(box.left() + week * cw + gapX / 2, box.top() + weekday * ch + gapY / 2,
                      cw - gapX, ch - gapY);
    };

    // 先全部鋪成「沒有資料」的顏色，再把有資料的蓋上去。
    // 只畫有資料的那幾天的話，缺漏的日子會變成背景色 —— 看起來像那幾天不存在，
    // 而不是「那幾天是 0 或沒量到」。
    const QDate origin = weekStart(first, weekStartsMonday);
    p.setPen(border.isVisible() ? border.pen() : QPen(Qt::NoPen));
    for (int w = 0; w < weeks; ++w)
        for (int d = 0; d < 7; ++d) {
            const QDate day = origin.addDays(static_cast<qint64>(w) * 7 + d);
            if (day < first || day > last) continue;
            p.setBrush(QBrush(missingColor));
            const QRectF r = cellRect(w, d);
            if (radius > 0.5) p.drawRoundedRect(r, radius, radius);
            else p.drawRect(r);
        }

    const CoordinateRange range = effectiveColorRange();
    for (const CalendarValue& v : values) {
        const QPoint cell = cellFor(v.date);
        if (cell.x() < 0) continue;
        if (!std::isfinite(v.value)) continue;
        p.setBrush(QBrush(colormap.lookup(v.value, range.min, range.max)));
        const QRectF r = cellRect(cell.x(), cell.y());
        if (radius > 0.5) p.drawRoundedRect(r, radius, radius);
        else p.drawRect(r);
    }

    // ── 月份分隔線與標籤 ──
    if (showMonthSeparators || showMonthLabels) {
        QPen sep = monthSeparator.pen();
        sep.setWidthF(sep.widthF() * rp.scaleFactor);
        QDate cursor(first.year(), first.month(), 1);
        while (cursor <= last) {
            const QDate monthStart = (cursor < first) ? first : cursor;
            const QPoint cell = cellFor(monthStart);
            if (cell.x() >= 0) {
                if (showMonthSeparators && monthStart != first) {
                    p.setPen(sep);
                    const double x = box.left() + cell.x() * cw;
                    p.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
                }
                if (showMonthLabels) {
                    Label lb;
                    // 月份名交給 QLocale，翻譯檔不必列十二個月
                    lb.text = displayLocale().monthName(monthStart.month(), QLocale::ShortFormat);
                    lb.font = labelFont;
                    lb.font.size *= rp.scaleFactor;
                    drawText(p, QPointF(box.left() + cell.x() * cw + 2 * rp.scaleFactor,
                                        box.top() - 3 * rp.scaleFactor),
                             lb, Qt::AlignLeft | Qt::AlignBottom);
                }
            }
            cursor = cursor.addMonths(1);
        }
    }

    if (showWeekdayLabels) {
        // 星期名交給 QLocale（中文是「一」…「日」，英文是 M…S），跟著 QtPlot 的語言設定
        const QLocale loc = displayLocale();
        auto day = [&loc](int dow) { return loc.dayName(dow, QLocale::NarrowFormat); };
        const QStringList mon{day(1), day(2), day(3), day(4), day(5), day(6), day(7)};
        const QStringList sun{day(7), day(1), day(2), day(3), day(4), day(5), day(6)};
        const QStringList& names = weekStartsMonday ? mon : sun;
        for (int d = 0; d < 7; ++d) {
            // 只標隔一天，七個全標會擠在一起
            if (d % 2 == 1) continue;
            Label lb;
            lb.text = names[d];
            lb.font = labelFont;
            lb.font.size *= rp.scaleFactor;
            drawText(p, QPointF(box.left() - 4 * rp.scaleFactor,
                                box.top() + (d + 0.5) * ch),
                     lb, Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    p.restore();
}

}  // namespace qtplot

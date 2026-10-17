#include "Flow.h"

#include <QMap>

#include <QPainterPath>

#include <random>

namespace qtplot {

namespace {

/// 把一組點等比例縮放平移到指定的矩形裡，保持長寬比。
///
/// 不保持長寬比的話，節點之間的距離在兩個方向上會被不同程度地拉伸，
/// 力導向算出來的「等距」就白算了。
QVector<Coordinates> normalizeToExtent(const QVector<Coordinates>& pts,
                                       const CoordinateRect& box) {
    QVector<Coordinates> out = pts;
    if (pts.isEmpty()) return out;

    CoordinateRange xr, yr;
    for (const Coordinates& c : pts) {
        xr.expand(c.x);
        yr.expand(c.y);
    }
    const double sx = (xr.span() > 1e-12) ? box.width() / xr.span() : 1.0;
    const double sy = (yr.span() > 1e-12) ? box.height() / yr.span() : 1.0;
    const double s = std::min(sx, sy);

    const double cx = xr.center(), cy = yr.center();
    const double bx = (box.left + box.right) / 2, by = (box.bottom + box.top) / 2;
    for (Coordinates& c : out) {
        c.x = bx + (c.x - cx) * s;
        c.y = by + (c.y - cy) * s;
    }
    return out;
}

}  // namespace

// ───────────────────────── Sankey ─────────────────────────

int Sankey::layerCount() const {
    int maxLayer = -1;
    for (const SankeyNode& n : nodes) maxLayer = std::max(maxLayer, n.layer);
    return maxLayer + 1;
}

QVector<double> Sankey::nodeValues() const {
    QVector<double> in(nodes.size(), 0.0), out(nodes.size(), 0.0);
    for (const SankeyLink& l : links) {
        if (!std::isfinite(l.value) || l.value <= 0) continue;
        if (l.from >= 0 && l.from < nodes.size()) out[l.from] += l.value;
        if (l.to >= 0 && l.to < nodes.size()) in[l.to] += l.value;
    }
    QVector<double> out2(nodes.size());
    // 取進出的較大值。中間的節點常常不平衡（有損耗、有新增），
    // 取小的那邊會讓連線擠不下、互相重疊。
    for (int i = 0; i < nodes.size(); ++i) out2[i] = std::max(in[i], out[i]);
    return out2;
}

AxisLimits Sankey::axisLimits() const {
    if (nodes.isEmpty()) return AxisLimits();
    return AxisLimits(extent.xRange(), extent.yRange());
}

Sankey::Layout Sankey::computeLayout() const {
    Layout out;
    const int n = static_cast<int>(nodes.size());
    const int layers = layerCount();
    out.boxes.resize(n);
    out.sourceTop.resize(links.size());
    out.targetTop.resize(links.size());
    out.drawn.fill(false, links.size());
    if (n == 0 || layers == 0) return out;

    const QVector<double> values = nodeValues();

    QVector<QVector<int>> byLayer(layers);
    for (int i = 0; i < n; ++i)
        if (nodes[i].layer >= 0 && nodes[i].layer < layers) byLayer[nodes[i].layer].append(i);

    const double totalHeight = extent.top - extent.bottom;
    const double padding = totalHeight * std::clamp(nodePaddingFraction, 0.0, 0.2);

    // 最擠的那一層決定「一個單位的流量畫多高」，其他層就用同一個比例 ——
    // 各層各自撐滿的話，粗細就沒辦法跨層比較了。
    double scale = 0;
    for (int k = 0; k < layers; ++k) {
        double sum = 0;
        for (int i : byLayer[k]) sum += values[i];
        const int gaps = std::max(0, static_cast<int>(byLayer[k].size()) - 1);
        const double avail = totalHeight - padding * gaps;
        if (sum > 0 && avail > 0) scale = (scale == 0) ? avail / sum : std::min(scale, avail / sum);
    }
    if (!(scale > 0)) return out;
    out.scale = scale;

    const double totalWidth = extent.right - extent.left;
    const double nodeWidth = totalWidth * std::clamp(nodeWidthFraction, 0.005, 0.3);
    // 層與層之間的間隔：最左的節點貼左邊、最右的貼右邊
    const double layerGap = (layers > 1) ? (totalWidth - nodeWidth) / (layers - 1) : 0;

    for (int k = 0; k < layers; ++k) {
        double sum = 0;
        for (int i : byLayer[k]) sum += values[i];
        const int gaps = std::max(0, static_cast<int>(byLayer[k].size()) - 1);
        const double used = sum * scale + padding * gaps;
        double cursor = extent.top - (totalHeight - used) / 2;  // 整層垂直置中
        const double left = extent.left + layerGap * k;
        for (int i : byLayer[k]) {
            Layout::Box b;
            b.left = left;
            b.right = left + nodeWidth;
            b.top = cursor;
            b.bottom = cursor - values[i] * scale;
            b.valid = true;
            out.boxes[i] = b;
            cursor = b.bottom - padding;
        }
    }

    // 配置順序決定交叉多不多：同一個節點流出去的幾條線，要依「對端節點的高度」
    // 排序再依序配置，線才不會互相穿越。照 links 原本的順序配置的話，
    // 圖上會多出一堆純粹是順序造成的交叉。
    auto centerOf = [&](int node) {
        return out.boxes[node].valid ? (out.boxes[node].top + out.boxes[node].bottom) / 2 : 0.0;
    };

    QVector<int> valid;
    for (int li = 0; li < links.size(); ++li) {
        const SankeyLink& l = links[li];
        if (l.from < 0 || l.from >= n || l.to < 0 || l.to >= n) continue;
        if (!std::isfinite(l.value) || l.value <= 0) continue;
        if (!out.boxes[l.from].valid || !out.boxes[l.to].valid) continue;
        valid.append(li);
        out.drawn[li] = true;
    }

    QVector<double> cursor(n);
    for (int i = 0; i < n; ++i) cursor[i] = out.boxes[i].top;

    QVector<int> bySource = valid;
    std::stable_sort(bySource.begin(), bySource.end(), [&](int a, int b) {
        if (links[a].from != links[b].from) return links[a].from < links[b].from;
        return centerOf(links[a].to) > centerOf(links[b].to);
    });
    for (int li : bySource) {
        out.sourceTop[li] = cursor[links[li].from];
        cursor[links[li].from] -= links[li].value * scale;
    }

    for (int i = 0; i < n; ++i) cursor[i] = out.boxes[i].top;
    QVector<int> byTarget = valid;
    std::stable_sort(byTarget.begin(), byTarget.end(), [&](int a, int b) {
        if (links[a].to != links[b].to) return links[a].to < links[b].to;
        return centerOf(links[a].from) > centerOf(links[b].from);
    });
    for (int li : byTarget) {
        out.targetTop[li] = cursor[links[li].to];
        cursor[links[li].to] -= links[li].value * scale;
    }

    return out;
}

void Sankey::render(RenderPack& rp) {
    const int n = static_cast<int>(nodes.size());
    if (n == 0) return;
    const Layout lay = computeLayout();
    if (!(lay.scale > 0)) return;
    const QVector<Layout::Box>& boxes = lay.boxes;
    const int layers = layerCount();

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 連線先畫，節點方塊才蓋在上面把接縫藏起來
    for (int li = 0; li < links.size(); ++li) {
        if (!lay.drawn[li]) continue;
        const SankeyLink& l = links[li];
        const double h = l.value * lay.scale;
        const QPointF a0 = t.p(boxes[l.from].right, lay.sourceTop[li]);
        const QPointF a1 = t.p(boxes[l.from].right, lay.sourceTop[li] - h);
        const QPointF b0 = t.p(boxes[l.to].left, lay.targetTop[li]);
        const QPointF b1 = t.p(boxes[l.to].left, lay.targetTop[li] - h);

        // 用三次貝茲，控制點放在兩端的水平中點上：緞帶離開節點時是水平的，
        // 看起來才像從方塊「長出來」，而不是斜插進去。
        const double cx = (a0.x() + b0.x()) / 2;
        QPainterPath path;
        path.moveTo(a0);
        path.cubicTo(QPointF(cx, a0.y()), QPointF(cx, b0.y()), b0);
        path.lineTo(b1);
        path.cubicTo(QPointF(cx, b1.y()), QPointF(cx, a1.y()), a1);
        path.closeSubpath();

        QColor c = l.color.isValid() ? l.color : nodes[l.from].color;
        if (!c.isValid()) c = QColor(120, 120, 120);
        c.setAlphaF(std::clamp(linkOpacity, 0.0, 1.0));
        p.setBrush(QBrush(c));
        p.setPen(Qt::NoPen);
        p.drawPath(path);
    }

    for (int i = 0; i < n; ++i) {
        if (!boxes[i].valid) continue;
        const QColor c = nodes[i].color.isValid() ? nodes[i].color : QColor(90, 90, 90);
        p.setBrush(QBrush(c));
        p.setPen(nodeBorder.isVisible() ? nodeBorder.pen() : QPen(Qt::NoPen));
        p.drawRect(QRectF(t.p(boxes[i].left, boxes[i].top), t.p(boxes[i].right, boxes[i].bottom)));

        if (!nodes[i].label.isEmpty() && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = nodes[i].label;
            lb.font = labelFont;
            // 標籤放在方塊外側。最後一層放左邊，其餘放右邊 ——
            // 最後一層的右邊就是圖的邊緣，文字會被切掉。
            const bool last = nodes[i].layer == layers - 1;
            const double y = (boxes[i].top + boxes[i].bottom) / 2;
            const QPointF anchor = last ? t.p(boxes[i].left, y) : t.p(boxes[i].right, y);
            drawText(p, anchor + QPointF(last ? -labelGapPx : labelGapPx, 0), lb,
                     (last ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> Sankey::legendItems() const {
    QVector<LegendItem> items;
    for (const SankeyNode& nd : nodes) {
        if (nd.label.isEmpty() || !nd.color.isValid()) continue;
        LegendItem it;
        it.text = nd.label;
        it.fill = FillStyle(nd.color);
        items.append(it);
    }
    return items;
}

// ───────────────────────── Chord ─────────────────────────

bool Chord::isValid() const {
    const int n = static_cast<int>(matrix.size());
    if (n == 0) return false;
    for (const QVector<double>& row : matrix)
        if (row.size() != n) return false;
    return true;
}

QVector<double> Chord::nodeTotals() const {
    const int n = static_cast<int>(matrix.size());
    QVector<double> totals(n, 0.0);
    if (!isValid()) return totals;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const double v = matrix[i][j];
            if (!std::isfinite(v) || v <= 0) continue;
            totals[i] += v;              // i 流出去的
            if (i != j) totals[j] += v;  // j 收進來的；對角線只算一次
        }
    }
    return totals;
}

AxisLimits Chord::axisLimits() const {
    if (!isValid()) return AxisLimits();
    const double r = radius * 1.18;  // 留給外圈的標籤
    return AxisLimits(CoordinateRange(center.x - r, center.x + r),
                      CoordinateRange(center.y - r, center.y + r));
}

void Chord::render(RenderPack& rp) {
    if (!isValid()) return;
    const int n = static_cast<int>(matrix.size());
    const QVector<double> totals = nodeTotals();
    double grand = 0;
    for (double v : totals) grand += v;
    if (!(grand > 0)) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const double dir = clockwise ? -1.0 : 1.0;
    const double gap = std::clamp(gapDegrees, 0.0, 30.0);
    const double usable = 360.0 - gap * n;
    const double rOuter = radius;
    const double rInner = radius * (1.0 - std::clamp(arcThicknessFraction, 0.01, 0.5));

    auto colorOf = [&](int i) {
        if (i < colors.size() && colors[i].isValid()) return colors[i];
        return Palette(Palette::Category10).color(i);
    };

    // 每個節點佔的角度區間
    QVector<double> arcStart(n), arcSweep(n);
    double cursor = startAngle;
    for (int i = 0; i < n; ++i) {
        arcStart[i] = cursor + dir * gap / 2;
        arcSweep[i] = dir * usable * (totals[i] / grand);
        cursor += dir * (gap + std::abs(arcSweep[i]));
    }

    // 緞帶先畫，外圈的弧帶才蓋在上面把接縫藏起來。
    // 每個節點內部要依序切出每一條緞帶的落腳處，所以記住用到哪裡了。
    QVector<double> used(n, 0.0);
    auto takeSlice = [&](int node, double value) {
        const double frac = value / totals[node];
        const double a0 = arcStart[node] + arcSweep[node] * used[node];
        used[node] += frac;
        const double a1 = arcStart[node] + arcSweep[node] * used[node];
        return QPair<double, double>(a0, a1);
    };

    auto pointAt = [&](double r, double deg) {
        const double a = deg * kPi / 180.0;
        return t.p(center.x + r * std::cos(a), center.y + r * std::sin(a));
    };

    /// 沿著內圈畫一小段弧（緞帶的端面）。
    auto arcTo = [&](QPainterPath& path, double from, double to) {
        const int steps = std::max(2, static_cast<int>(std::abs(to - from)));
        for (int k = 1; k <= steps; ++k)
            path.lineTo(pointAt(rInner, from + (to - from) * k / steps));
    };

    for (int i = 0; i < n; ++i) {
        for (int j = i; j < n; ++j) {
            const double vij = (std::isfinite(matrix[i][j]) && matrix[i][j] > 0) ? matrix[i][j] : 0;
            const double vji = (i == j) ? 0
                                        : ((std::isfinite(matrix[j][i]) && matrix[j][i] > 0)
                                               ? matrix[j][i]
                                               : 0);
            if (vij <= 0 && vji <= 0) continue;

            // i 這一端吃掉 vij（它流出去的）加 vji（它收進來的）
            const QPair<double, double> si = takeSlice(i, vij + vji);
            const QPair<double, double> sj = (i == j) ? si : takeSlice(j, vij + vji);

            // 穿過圓心：控制點放在圓心，緞帶才會往中間收束
            const QPointF mid = t.p(center);
            QPainterPath path;
            path.moveTo(pointAt(rInner, si.first));
            arcTo(path, si.first, si.second);
            if (i == j) {
                // 自迴圈：兩端都在同一段弧上，畫成一片花瓣。
                // 不能照一般的走法再描一次同一段弧 —— QPainterPath 預設是
                // OddEven 填充，同一塊區域描兩次會互相抵銷，只剩下髮絲般的邊線。
                path.cubicTo(mid, mid, pointAt(rInner, si.first));
            } else {
                path.cubicTo(mid, mid, pointAt(rInner, sj.first));
                arcTo(path, sj.first, sj.second);
                path.cubicTo(mid, mid, pointAt(rInner, si.first));
            }
            path.closeSubpath();

            // 用量大的那一端的顏色，讀者才會把緞帶歸給主要的來源
            QColor c = colorOf(vij >= vji ? i : j);
            c.setAlphaF(std::clamp(ribbonOpacity, 0.0, 1.0));
            p.setBrush(QBrush(c));
            p.setPen(ribbonBorder.isVisible() ? ribbonBorder.pen() : QPen(Qt::NoPen));
            p.drawPath(path);
        }
    }

    for (int i = 0; i < n; ++i) {
        if (std::abs(arcSweep[i]) < 1e-9) continue;
        QPainterPath arc = sectorPath(t, center, rOuter, rInner, arcStart[i], arcSweep[i]);
        p.setBrush(QBrush(colorOf(i)));
        p.setPen(arcBorder.isVisible() ? arcBorder.pen() : QPen(Qt::NoPen));
        p.drawPath(arc);

        if (i < labels.size() && !labels[i].isEmpty() && labelFont.color.alpha() > 0) {
            const double midDeg = arcStart[i] + arcSweep[i] / 2;
            const double a = midDeg * kPi / 180.0;
            QPointF anchor = pointAt(rOuter, midDeg);
            anchor += QPointF(std::cos(a), -std::sin(a)) * labelGapPx;
            Label lb;
            lb.text = labels[i];
            lb.font = labelFont;
            // 左半圈的文字靠右對齊，右半圈靠左，整圈看起來才齊
            const double norm = std::fmod(std::fmod(midDeg, 360.0) + 360.0, 360.0);
            const Qt::Alignment al =
                ((norm > 90 && norm < 270) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
            drawText(p, anchor, lb, al);
        }
    }
    p.restore();
}

QVector<LegendItem> Chord::legendItems() const {
    QVector<LegendItem> items;
    for (int i = 0; i < labels.size(); ++i) {
        if (labels[i].isEmpty()) continue;
        LegendItem it;
        it.text = labels[i];
        it.fill = FillStyle((i < colors.size() && colors[i].isValid())
                                ? colors[i]
                                : Palette(Palette::Category10).color(i));
        items.append(it);
    }
    return items;
}


// ───────────────────────── NetworkGraph ─────────────────────────

double NetworkGraph::diameterFor(double size) const {
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (const GraphNode& n : nodes) {
        if (!std::isfinite(n.size)) continue;
        lo = std::min(lo, n.size);
        hi = std::max(hi, n.size);
    }
    if (!(hi > lo)) return (nodeMinDiameterPx + nodeMaxDiameterPx) / 2;
    const double f = std::clamp((size - lo) / (hi - lo), 0.0, 1.0);
    // 面積正比，和氣泡圖同一套：直徑正比會把大小差距讀成平方
    const double aMin = nodeMinDiameterPx * nodeMinDiameterPx;
    const double aMax = nodeMaxDiameterPx * nodeMaxDiameterPx;
    return std::sqrt(aMin + (aMax - aMin) * f);
}

QVector<Coordinates> NetworkGraph::computeLayout() const {
    const int n = static_cast<int>(nodes.size());
    QVector<Coordinates> pos(n);
    if (n == 0) return pos;

    if (layout == Layout::Manual) {
        for (int i = 0; i < n; ++i) pos[i] = nodes[i].position;
        return pos;
    }
    if (layout == Layout::Linear) {
        for (int i = 0; i < n; ++i)
            pos[i] = {(n == 1) ? 0.0 : static_cast<double>(i) / (n - 1), 0.0};
        // 這裡不走 normalizeToExtent：它會保持長寬比，而一條線的高度是 0，
        // 等比例縮放會把整條線壓成一點。直接鋪滿 extent 的寬度就好。
        for (int i = 0; i < n; ++i) {
            pos[i].x = extent.left + pos[i].x * extent.width();
            pos[i].y = (extent.bottom + extent.top) / 2;
        }
        return pos;
    }
    if (layout == Layout::Circular || n == 1) {
        for (int i = 0; i < n; ++i) {
            const double deg = 360.0 * i / n;
            pos[i] = fromPolar(1.0, deg);
        }
        return normalizeToExtent(pos, extent);
    }

    // Fruchterman–Reingold。用自己的引擎而不是 DataGen 的全域引擎：
    // 版面必須只取決於資料與 seed，不能被「前面跑過什麼」影響。
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);
    for (int i = 0; i < n; ++i) {
        if (nodes[i].pinned) pos[i] = nodes[i].position;
        else                 pos[i] = {uni(rng), uni(rng)};
    }

    // k 是「理想邊長」：面積開根號除以節點數開根號，讓節點大致均勻鋪滿
    const double area = 4.0;  // 座標框是 [-1,1]^2
    const double k = std::max(1e-6, edgeLengthFactor) * std::sqrt(area / n);
    // 初始溫度取框寬的一成。太大的話前幾步就把節點甩到天邊，
    // 之後再怎麼冷卻也回不來。
    double temperature = 0.2;
    const double cooling = temperature / std::max(1, iterations);

    QVector<Coordinates> disp(n);
    for (int step = 0; step < std::max(1, iterations); ++step) {
        for (int i = 0; i < n; ++i) disp[i] = {0, 0};

        // 斥力：所有節點互推
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                double dx = pos[i].x - pos[j].x;
                double dy = pos[i].y - pos[j].y;
                double d = std::hypot(dx, dy);
                if (d < 1e-6) {
                    // 完全重疊時沒有方向可推。用索引決定一個固定的偏移，
                    // 不要用亂數 —— 那會讓結果不可重現。
                    dx = 1e-3 * ((i % 2) ? 1 : -1);
                    dy = 1e-3 * ((j % 2) ? 1 : -1);
                    d = std::hypot(dx, dy);
                }
                const double force = k * k / d;
                disp[i].x += dx / d * force;
                disp[i].y += dy / d * force;
                disp[j].x -= dx / d * force;
                disp[j].y -= dy / d * force;
            }
        }

        // 引力：有邊相連的互拉
        for (const GraphEdge& e : edges) {
            if (e.from < 0 || e.from >= n || e.to < 0 || e.to >= n) continue;
            if (e.from == e.to) continue;
            double dx = pos[e.from].x - pos[e.to].x;
            double dy = pos[e.from].y - pos[e.to].y;
            const double d = std::max(1e-6, std::hypot(dx, dy));
            const double w = std::max(0.0, e.weight);
            const double force = d * d / k * (w > 0 ? w : 1.0);
            disp[e.from].x -= dx / d * force;
            disp[e.from].y -= dy / d * force;
            disp[e.to].x += dx / d * force;
            disp[e.to].y += dy / d * force;
        }

        // 向心力：和離中心的距離成正比。沒有它的話，只有一條邊的葉節點
        // 會被全體斥力一路推到框邊，擠成一圈 —— 它們彼此之間沒有邊可以拉住。
        if (gravity > 0) {
            for (int i = 0; i < n; ++i) {
                disp[i].x -= pos[i].x * k * gravity * n * 0.5;
                disp[i].y -= pos[i].y * k * gravity * n * 0.5;
            }
        }

        for (int i = 0; i < n; ++i) {
            if (nodes[i].pinned) continue;
            const double d = std::hypot(disp[i].x, disp[i].y);
            if (d < 1e-12) continue;
            // 每一步最多走 temperature，溫度隨迭代降低 —— 先大步攤開再微調
            const double limit = std::min(d, temperature);
            pos[i].x += disp[i].x / d * limit;
            pos[i].y += disp[i].y / d * limit;

            // 夾回框內。這一步是原始演算法的一部分，漏掉的話只要有一個節點
            // 被甩出去，最後的等比例縮放就會把其餘所有節點壓成中間一小團 ——
            // 看起來像「版面沒收斂」，其實是一個離群點造成的。
            pos[i].x = std::clamp(pos[i].x, -1.0, 1.0);
            pos[i].y = std::clamp(pos[i].y, -1.0, 1.0);
        }
        temperature = std::max(0.0, temperature - cooling);
    }

    return normalizeToExtent(pos, extent);
}

AxisLimits NetworkGraph::axisLimits() const {
    if (nodes.isEmpty()) return AxisLimits();
    // 節點是像素大小，資料範圍換不出來，所以留一成餘裕給最大的那顆
    AxisLimits lim(extent.xRange(), extent.yRange());
    const double padX = extent.width() * 0.08;
    const double padY = extent.height() * 0.08;
    lim.expand({extent.left - padX, extent.bottom - padY});
    lim.expand({extent.right + padX, extent.top + padY});
    return lim;
}

void NetworkGraph::render(RenderPack& rp) {
    const int n = static_cast<int>(nodes.size());
    if (n == 0) return;
    const QVector<Coordinates> pos = computeLayout();

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    double maxWeight = 0;
    for (const GraphEdge& e : edges) maxWeight = std::max(maxWeight, e.weight);

    // 邊先畫，節點才蓋在上面把端點藏起來
    for (const GraphEdge& e : edges) {
        if (e.from < 0 || e.from >= n || e.to < 0 || e.to >= n) continue;
        LineStyle ls = edgeLine;
        if (e.color.isValid()) ls.color = e.color;
        if (maxWeight > 0)
            ls.width = std::max(0.5, edgeMaxWidthPx * std::clamp(e.weight / maxWeight, 0.0, 1.0));
        if (!ls.isVisible()) continue;
        p.setPen(ls.pen());
        p.setBrush(Qt::NoBrush);

        if (edgeShape == EdgeShape::Arc && e.from != e.to) {
            // 以兩端的中點為圓心畫半橢圓。高度用「兩端距離」的比例，
            // 跨得遠的弧才會高一點、不會全部疊在一起。
            const Coordinates a = pos[e.from], b = pos[e.to];
            const double cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2;
            const double rx = std::abs(b.x - a.x) / 2;
            const double ry = rx * arcHeightFraction;
            if (rx > 0) {
                // 從靠右那端（0°）掃到靠左那端（180°），就是上半弧
                const bool rightFirst = b.x > a.x;
                p.drawPath(arcPath(t, {cx, cy}, rx, ry, rightFirst ? 0 : 180,
                                   rightFirst ? 180 : -180));
                continue;
            }
        }
        p.drawLine(t.p(pos[e.from]), t.p(pos[e.to]));
    }

    // 大的先畫，小的才不會被蓋住
    QVector<int> order;
    order.reserve(n);
    for (int i = 0; i < n; ++i) order.append(i);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return nodes[a].size > nodes[b].size; });

    for (int i : order) {
        const double d = diameterFor(nodes[i].size);
        const QPointF c = t.p(pos[i]);
        p.setBrush(QBrush(nodes[i].color.isValid() ? nodes[i].color : QColor(90, 150, 210)));
        p.setPen(nodeBorder.isVisible() ? nodeBorder.pen() : QPen(Qt::NoPen));
        p.drawEllipse(c, d / 2, d / 2);

        if (showLabels && !nodes[i].label.isEmpty() && labelFont.color.alpha() > 0 &&
            d >= minLabelDiameterPx) {
            Label lb;
            lb.text = nodes[i].label;
            lb.font = labelFont;
            drawText(p, c, lb, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> NetworkGraph::legendItems() const {
    QVector<LegendItem> items;
    if (legendText.isEmpty()) return items;
    LegendItem it;
    it.text = legendText;
    it.fill = FillStyle(nodes.isEmpty() || !nodes.first().color.isValid()
                            ? QColor(90, 150, 210)
                            : nodes.first().color);
    items.append(it);
    return items;
}


// ───────────────────────── 沖積圖 ─────────────────────────

AlluvialResult alluvialFromRecords(const QVector<QStringList>& records,
                                   const QStringList& stageNames) {
    AlluvialResult out;
    const int stages = stageNames.size();
    if (stages < 2) return out;

    // 每一階段的分類清單，照第一次出現的順序。
    // 用 QVector 查找而不是 QHash 排序：資料量是「分類數」不是「筆數」，
    // 線性查找完全夠，而且順序才會穩定。
    QVector<QStringList> categoriesPerStage(stages);
    for (const QStringList& rec : records) {
        if (rec.size() != stages) continue;
        for (int s = 0; s < stages; ++s)
            if (!categoriesPerStage[s].contains(rec[s])) categoriesPerStage[s].append(rec[s]);
    }

    QVector<QVector<int>> nodeIndex(stages);
    for (int s = 0; s < stages; ++s) {
        nodeIndex[s].resize(categoriesPerStage[s].size());
        for (int k = 0; k < categoriesPerStage[s].size(); ++k) {
            SankeyNode n;
            n.label = categoriesPerStage[s][k];
            n.layer = s;
            nodeIndex[s][k] = static_cast<int>(out.nodes.size());
            out.nodes.append(n);
        }
    }

    // 相鄰兩階段之間的流量。QMap 而不是 QHash —— 迭代順序要固定，
    // 否則連線的先後會變，畫出來的疊放順序跟著變。
    QMap<QPair<int, int>, double> flow;
    for (const QStringList& rec : records) {
        if (rec.size() != stages) continue;
        for (int s = 0; s + 1 < stages; ++s) {
            const int from = nodeIndex[s][categoriesPerStage[s].indexOf(rec[s])];
            const int to = nodeIndex[s + 1][categoriesPerStage[s + 1].indexOf(rec[s + 1])];
            flow[{from, to}] += 1.0;
        }
    }
    for (auto it = flow.cbegin(); it != flow.cend(); ++it) {
        SankeyLink l;
        l.from = it.key().first;
        l.to = it.key().second;
        l.value = it.value();
        out.links.append(l);
    }
    return out;
}

}  // namespace qtplot

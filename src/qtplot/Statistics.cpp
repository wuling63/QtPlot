#include "Statistics.h"

#include <functional>

#include <QFontMetricsF>
#include <QPolygonF>

#include <numeric>
#include <random>

namespace qtplot {

double sampleQuantile(const QVector<double>& sorted, double fraction) {
    const int n = static_cast<int>(sorted.size());
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 1) return sorted.first();
    const double pos = std::clamp(fraction, 0.0, 1.0) * (n - 1);
    const int lo = static_cast<int>(std::floor(pos));
    const int hi = std::min(lo + 1, n - 1);
    const double frac = pos - lo;
    return sorted[lo] + (sorted[hi] - sorted[lo]) * frac;
}

namespace {

QColor withAlpha(QColor c, int alpha) {
    c.setAlpha(alpha);
    return c;
}

/// 一組資料點的水平抖動量，固定種子所以每次重畫位置都一樣
/// （用隨機種子的話每次 repaint 點都會跳，看起來像壞掉）。
///
/// 必須整組共用同一個亂數序列：對每個點各開一個 engine 只取第一個輸出，
/// 相鄰種子的首個輸出高度相關，抖動會整體偏向同一側而不是平均散開。
QVector<double> jitterOffsets(int groupIndex, int count) {
    std::mt19937 rng(static_cast<unsigned>(991 + groupIndex * 7919));
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    QVector<double> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) out.append(dist(rng));
    return out;
}

}  // namespace

// ───────────────────────── 統計計算 ─────────────────────────

BoxStats BoxStats::fromValues(const QVector<double>& values, double whiskerIqrFactor) {
    BoxStats s;
    QVector<double> v;
    v.reserve(values.size());
    for (double x : values)
        if (std::isfinite(x)) v.append(x);
    if (v.isEmpty()) return s;

    std::sort(v.begin(), v.end());
    s.count = static_cast<int>(v.size());
    s.q1 = sampleQuantile(v, 0.25);
    s.median = sampleQuantile(v, 0.50);
    s.q3 = sampleQuantile(v, 0.75);

    s.mean = std::accumulate(v.begin(), v.end(), 0.0) / s.count;
    if (s.count > 1) {
        double ss = 0;
        for (double x : v) ss += (x - s.mean) * (x - s.mean);
        s.stdDev = std::sqrt(ss / (s.count - 1));
        s.stdErr = s.stdDev / std::sqrt(static_cast<double>(s.count));
    }

    if (whiskerIqrFactor > 0) {
        const double limit = whiskerIqrFactor * s.iqr();
        const double lo = s.q1 - limit;
        const double hi = s.q3 + limit;
        // 鬚停在界線「之內」最遠的那個實際資料點，不是停在界線本身
        s.min = v.first();
        s.max = v.last();
        bool haveMin = false;
        for (double x : v) {
            if (x < lo || x > hi) {
                s.outliers.append(x);
                continue;
            }
            if (!haveMin) {
                s.min = x;
                haveMin = true;
            }
            s.max = x;
        }
        if (!haveMin) {  // 全部都是離群值（極端情況）
            s.min = s.q1;
            s.max = s.q3;
        }
    } else {
        s.min = v.first();
        s.max = v.last();
    }
    return s;
}

LinearFit linearFit(const QVector<double>& xs, const QVector<double>& ys) {
    LinearFit fit;
    const qsizetype n = std::min(xs.size(), ys.size());
    double sx = 0, sy = 0;
    int count = 0;
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        sx += xs[i];
        sy += ys[i];
        ++count;
    }
    if (count < 2) return fit;

    const double mx = sx / count, my = sy / count;
    double sxx = 0, sxy = 0, syy = 0;
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        const double dx = xs[i] - mx, dy = ys[i] - my;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }
    if (sxx <= 0) return fit;  // 所有 x 相同，斜率無定義

    fit.count = count;
    fit.slope = sxy / sxx;
    fit.intercept = my - fit.slope * mx;
    fit.rSquared = (syy > 0) ? (sxy * sxy) / (sxx * syy) : 1.0;
    return fit;
}

double silvermanBandwidth(const QVector<double>& values) {
    QVector<double> v;
    v.reserve(values.size());
    for (double x : values)
        if (std::isfinite(x)) v.append(x);
    if (v.size() < 2) return 1.0;

    std::sort(v.begin(), v.end());
    const int n = static_cast<int>(v.size());
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / n;
    double ss = 0;
    for (double x : v) ss += (x - mean) * (x - mean);
    const double sd = std::sqrt(ss / (n - 1));
    const double iqr = sampleQuantile(v, 0.75) - sampleQuantile(v, 0.25);

    // 取 σ 與 IQR/1.34 的較小者，資料有離群值時才不會把頻寬撐得太寬
    double spread = sd;
    if (iqr > 0) spread = std::min(sd, iqr / 1.34);
    if (!(spread > 0)) spread = (v.last() - v.first()) / 6.0;
    if (!(spread > 0)) return 1.0;

    return 1.06 * spread * std::pow(static_cast<double>(n), -0.2);
}

double kernelValue(KernelType kernel, double u) {
    const double a = std::abs(u);
    switch (kernel) {
    case KernelType::Gaussian:     return std::exp(-0.5 * u * u) / std::sqrt(2 * kPi);
    case KernelType::Epanechnikov: return a <= 1 ? 0.75 * (1 - u * u) : 0.0;
    case KernelType::Uniform:      return a <= 1 ? 0.5 : 0.0;
    case KernelType::Triangular:   return a <= 1 ? 1 - a : 0.0;
    case KernelType::Cosine:       return a <= 1 ? kPi / 4 * std::cos(kPi / 2 * u) : 0.0;
    }
    return 0.0;
}

QVector<double> kernelDensity(const QVector<double>& values, const QVector<double>& evalPoints,
                              double bandwidth, KernelType kernel) {
    QVector<double> out(evalPoints.size(), 0.0);
    QVector<double> v;
    v.reserve(values.size());
    for (double x : values)
        if (std::isfinite(x)) v.append(x);
    if (v.isEmpty() || evalPoints.isEmpty()) return out;

    const double h = (bandwidth > 0) ? bandwidth : silvermanBandwidth(v);
    if (!(h > 0)) return out;

    if (kernel == KernelType::Gaussian) {
        const double norm = 1.0 / (v.size() * h * std::sqrt(2 * kPi));
        for (qsizetype i = 0; i < evalPoints.size(); ++i) {
            const double x = evalPoints[i];
            double sum = 0;
            for (double xi : v) {
                const double z = (x - xi) / h;
                sum += std::exp(-0.5 * z * z);
            }
            out[i] = sum * norm;
        }
        return out;
    }

    const double norm = 1.0 / (v.size() * h);
    for (qsizetype i = 0; i < evalPoints.size(); ++i) {
        double sum = 0;
        for (double xi : v) sum += kernelValue(kernel, (evalPoints[i] - xi) / h);
        out[i] = sum * norm;
    }
    return out;
}

// ───────────────────────── BoxPlot ─────────────────────────

QVector<Box> BoxPlot::fromGroups(const QVector<QVector<double>>& groups, const QStringList& labels,
                                 double whiskerIqrFactor) {
    QVector<Box> out;
    out.reserve(groups.size());
    for (qsizetype i = 0; i < groups.size(); ++i) {
        Box b;
        b.position = static_cast<double>(i);
        b.stats = BoxStats::fromValues(groups[i], whiskerIqrFactor);
        b.label = (i < labels.size()) ? labels.at(i) : QString();
        out.append(b);
    }
    return out;
}

AxisLimits BoxPlot::axisLimits() const {
    AxisLimits lim;
    for (const Box& b : boxes) {
        if (!b.stats.isValid()) continue;
        CoordinateRange along(b.position - b.size / 2, b.position + b.size / 2);
        CoordinateRange value;
        value.expand(b.stats.min);
        value.expand(b.stats.max);
        for (double o : b.stats.outliers) value.expand(o);
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

void BoxPlot::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    for (const Box& b : boxes) {
        const BoxStats& s = b.stats;
        if (!s.isValid()) continue;

        const double half = b.size / 2;
        const double capHalf = half * std::clamp(capFraction, 0.0, 1.0);

        // 沿著「類別軸」的像素位置，沿著「數值軸」的像素位置
        auto pt = [&](double along, double value) {
            return horizontal ? t.p(value, along) : t.p(along, value);
        };

        // ── 鬚 ──
        if (whiskerLine.isVisible()) {
            p.setPen(whiskerLine.pen());
            p.drawLine(pt(b.position, s.q3), pt(b.position, s.max));
            p.drawLine(pt(b.position, s.q1), pt(b.position, s.min));
            p.drawLine(pt(b.position - capHalf, s.max), pt(b.position + capHalf, s.max));
            p.drawLine(pt(b.position - capHalf, s.min), pt(b.position + capHalf, s.min));
        }

        // ── 箱 ──
        const QRectF boxRect = QRectF(pt(b.position - half, s.q1), pt(b.position + half, s.q3))
                                   .normalized();
        p.setBrush(QBrush(b.fillColor.isValid() ? b.fillColor : QColor(Qt::lightGray)));
        p.setPen(borderLine.isVisible() ? borderLine.pen() : QPen(Qt::NoPen));
        p.drawRect(boxRect);

        // ── 中位數 ──
        if (medianLine.isVisible()) {
            p.setPen(medianLine.pen());
            p.drawLine(pt(b.position - half, s.median), pt(b.position + half, s.median));
        }

        // ── 平均 ──
        if (meanMarker.isVisible()) drawMarker(p, pt(b.position, s.mean), meanMarker);

        // ── 離群值 ──
        if (outlierMarker.isVisible() && !s.outliers.isEmpty()) {
            QVector<QPointF> pts;
            pts.reserve(s.outliers.size());
            for (double o : s.outliers) pts.append(pt(b.position, o));
            drawMarkers(p, pts, outlierMarker);
        }
    }
    p.restore();
}

QVector<LegendItem> BoxPlot::legendItems() const {
    QVector<LegendItem> out;
    for (const Box& b : boxes) {
        const QString text = b.legendText.isEmpty() ? b.label : b.legendText;
        if (text.isEmpty()) continue;
        LegendItem it;
        it.text = text;
        it.fill = FillStyle(b.fillColor.isValid() ? b.fillColor : QColor(Qt::lightGray));
        it.line = borderLine;
        out.append(it);
    }
    return out;
}

// ───────────────────────── 階層式分群 ─────────────────────────

QVector<ClusterMerge> hierarchicalCluster(const QVector<QVector<double>>& distances,
                                          Linkage linkage) {
    QVector<ClusterMerge> out;
    const int n = static_cast<int>(distances.size());
    if (n < 2) return out;
    for (const QVector<double>& row : distances)
        if (row.size() != n) return out;

    // d[i][j] 是「目前還活著的群」之間的距離。合併之後直接就地更新，
    // 不重算 —— 重算的話浮點累加的順序會隨成員順序改變，結果就不穩定。
    QVector<QVector<double>> d = distances;
    QVector<bool> alive(n, true);
    QVector<int> size(n, 1);
    // 每個活著的群對應的「節點編號」：葉子是 0..n-1，合併結果是 n, n+1, ...
    QVector<int> nodeId(n);
    for (int i = 0; i < n; ++i) nodeId[i] = i;

    for (int step = 0; step < n - 1; ++step) {
        // 找最近的一對。平手時取索引最小的那一對 —— 不定下來的話，
        // 同一份資料在不同平台可能得到不同的樹。
        int bi = -1, bj = -1;
        double best = std::numeric_limits<double>::max();
        for (int i = 0; i < n; ++i) {
            if (!alive[i]) continue;
            for (int j = i + 1; j < n; ++j) {
                if (!alive[j]) continue;
                if (d[i][j] < best) {
                    best = d[i][j];
                    bi = i;
                    bj = j;
                }
            }
        }
        if (bi < 0) break;

        ClusterMerge m;
        // 小的編號放左邊，輸出才有固定的樣子
        m.left = std::min(nodeId[bi], nodeId[bj]);
        m.right = std::max(nodeId[bi], nodeId[bj]);
        m.distance = best;
        m.size = size[bi] + size[bj];
        out.append(m);

        // Lance–Williams：用舊的兩個距離算出新群到其他群的距離
        for (int k = 0; k < n; ++k) {
            if (!alive[k] || k == bi || k == bj) continue;
            double nd = 0;
            switch (linkage) {
            case Linkage::Single:   nd = std::min(d[bi][k], d[bj][k]); break;
            case Linkage::Complete: nd = std::max(d[bi][k], d[bj][k]); break;
            case Linkage::Average:
                nd = (size[bi] * d[bi][k] + size[bj] * d[bj][k]) / (size[bi] + size[bj]);
                break;
            }
            d[bi][k] = d[k][bi] = nd;
        }

        alive[bj] = false;
        size[bi] = m.size;
        nodeId[bi] = n + step;
    }
    return out;
}

QVector<int> dendrogramLeafOrder(const QVector<ClusterMerge>& merges, int leafCount) {
    QVector<int> out;
    if (leafCount <= 0) return out;
    if (merges.isEmpty()) {
        for (int i = 0; i < leafCount; ++i) out.append(i);
        return out;
    }

    // 從最後一次合併往下展開。遞迴深度最多是合併數，實務上不會爆。
    std::function<void(int)> expand = [&](int node) {
        if (node < leafCount) {
            out.append(node);
            return;
        }
        const int idx = node - leafCount;
        if (idx < 0 || idx >= merges.size()) return;
        expand(merges[idx].left);
        expand(merges[idx].right);
    };
    expand(leafCount + static_cast<int>(merges.size()) - 1);

    // 保險：沒被走到的葉子補在後面（資料不完整時才會發生）
    QVector<bool> seen(leafCount, false);
    for (int i : out)
        if (i >= 0 && i < leafCount) seen[i] = true;
    for (int i = 0; i < leafCount; ++i)
        if (!seen[i]) out.append(i);
    return out;
}

// ───────────────────────── Dendrogram ─────────────────────────

double Dendrogram::maxDistance() const {
    double m = 0;
    for (const ClusterMerge& c : merges) m = std::max(m, c.distance);
    return m;
}

QVector<double> Dendrogram::leafPositions() const {
    QVector<double> out(std::max(0, leafCount), 0.0);
    QVector<int> order = leafOrder.isEmpty() ? dendrogramLeafOrder(merges, leafCount) : leafOrder;
    for (int pos = 0; pos < order.size(); ++pos) {
        const int leaf = order[pos];
        if (leaf >= 0 && leaf < out.size()) out[leaf] = pos;
    }
    return out;
}

AxisLimits Dendrogram::axisLimits() const {
    AxisLimits lim;
    if (leafCount <= 0) return lim;
    const double maxD = maxDistance();
    // 類別方向：0 .. leafCount-1，兩端各留半格
    if (horizontal) {
        lim.expand({0, -0.5});
        lim.expand({maxD, leafCount - 0.5});
    } else {
        lim.expand({-0.5, 0});
        lim.expand({leafCount - 0.5, maxD});
    }
    return lim;
}

void Dendrogram::render(RenderPack& rp) {
    if (leafCount <= 0 || merges.isEmpty()) return;
    const QVector<double> leafPos = leafPositions();

    // 每個節點（葉子或合併）在類別軸上的位置與高度
    const int total = leafCount + static_cast<int>(merges.size());
    QVector<double> pos(total, 0.0), height(total, 0.0);
    for (int i = 0; i < leafCount; ++i) pos[i] = leafPos[i];
    for (int k = 0; k < merges.size(); ++k) {
        const ClusterMerge& m = merges[k];
        const int node = leafCount + k;
        if (m.left < 0 || m.left >= node || m.right < 0 || m.right >= node) continue;
        pos[node] = (pos[m.left] + pos[m.right]) / 2;
        height[node] = m.distance;
    }

    // 顏色：在 colorThreshold 以下的子樹各自一個顏色
    QVector<int> colorOf(total, -1);
    if (colorThreshold > 0) {
        int next = 0;
        // 由上往下走，第一個低於門檻的節點就是一群的根
        std::function<void(int, int)> paint = [&](int node, int group) {
            if (group < 0 && node >= leafCount) {
                const ClusterMerge& m = merges[node - leafCount];
                if (m.distance <= colorThreshold) group = next++;
            }
            colorOf[node] = group;
            if (node < leafCount) return;
            const ClusterMerge& m = merges[node - leafCount];
            paint(m.left, group);
            paint(m.right, group);
        };
        paint(total - 1, -1);
    }

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setBrush(Qt::NoBrush);

    auto at = [&](double category, double dist) {
        return horizontal ? t.p(dist, category) : t.p(category, dist);
    };

    for (int k = 0; k < merges.size(); ++k) {
        const ClusterMerge& m = merges[k];
        const int node = leafCount + k;
        if (m.left < 0 || m.left >= node || m.right < 0 || m.right >= node) continue;

        LineStyle ls = line;
        const int g = colorOf[node];
        if (g >= 0) ls.color = palette.color(g);
        if (!ls.isVisible()) continue;
        p.setPen(ls.pen());

        // ㄇ 字形：兩個子節點各往上畫到這次合併的高度，再橫向接起來
        const double h = m.distance;
        p.drawLine(at(pos[m.left], height[m.left]), at(pos[m.left], h));
        p.drawLine(at(pos[m.right], height[m.right]), at(pos[m.right], h));
        p.drawLine(at(pos[m.left], h), at(pos[m.right], h));
    }

    if (labelFont.color.alpha() > 0 && !leafLabels.isEmpty()) {
        for (int i = 0; i < leafCount && i < leafLabels.size(); ++i) {
            if (leafLabels[i].isEmpty()) continue;
            Label lb;
            lb.text = leafLabels[i];
            lb.font = labelFont;
            const int g = colorOf.value(i, -1);
            if (g >= 0) lb.font.color = palette.color(g);
            const QPointF anchor = at(pos[i], 0);
            if (horizontal)
                drawText(p, anchor + QPointF(-labelGapPx, 0), lb,
                         Qt::AlignRight | Qt::AlignVCenter);
            else
                drawText(p, anchor + QPointF(0, labelGapPx), lb,
                         Qt::AlignHCenter | Qt::AlignTop);
        }
    }
    p.restore();
}

// ───────────────────────── Beeswarm ─────────────────────────

QVector<Beeswarm::SwarmPoint> Beeswarm::layout(double valueRadius,
                                               double categoryRadius) const {
    QVector<SwarmPoint> out;
    if (!(valueRadius > 0) || !(categoryRadius > 0)) return out;

    const double maxOffset = std::max(0.0, maxSpreadFraction);

    for (int g = 0; g < groups.size(); ++g) {
        // 由小到大逐點放。順序固定才可重現 —— 照原始順序放的話，
        // 同一份資料只要排序不同就會得到不同的圖。
        QVector<double> sorted;
        for (double v : groups[g].values)
            if (std::isfinite(v)) sorted.append(v);
        std::sort(sorted.begin(), sorted.end());

        QVector<SwarmPoint> placed;
        placed.reserve(sorted.size());

        for (double v : sorted) {
            // 收集會擋路的鄰居：數值方向上距離小於兩個半徑的那些。
            // 更遠的點在數值方向就已經錯開，不可能重疊。
            QVector<QPair<double, double>> blocked;  // 禁止的 offset 區間
            for (const SwarmPoint& p : placed) {
                const double dv = std::abs(p.value - v);
                if (dv >= 2 * valueRadius) continue;
                // 兩個半徑 r 的圓要不重疊：dv² + do² >= (2r)²
                // 換算成類別軸的單位之後，do 的禁止範圍是 ±half
                const double ratio = dv / valueRadius;
                const double half = categoryRadius * std::sqrt(std::max(0.0, 4.0 - ratio * ratio));
                blocked.append({p.offset - half, p.offset + half});
            }

            double chosen = 0;
            if (!blocked.isEmpty()) {
                // 候選位置：0，以及每個禁止區間的兩個邊界。
                // 最佳解一定落在其中之一 —— 再往外就白走了。
                QVector<double> candidates{0.0};
                for (const auto& b : blocked) {
                    candidates.append(b.first);
                    candidates.append(b.second);
                }
                std::sort(candidates.begin(), candidates.end(),
                          [](double a, double b) { return std::abs(a) < std::abs(b); });

                bool found = false;
                for (double c : candidates) {
                    if (std::abs(c) > maxOffset) continue;
                    bool ok = true;
                    for (const auto& b : blocked) {
                        // 邊界本身算可以放（剛好相切）
                        if (c > b.first + 1e-12 && c < b.second - 1e-12) {
                            ok = false;
                            break;
                        }
                    }
                    if (ok) {
                        chosen = c;
                        found = true;
                        break;
                    }
                }
                // 真的排不下就夾在邊界上。讓它無限往外長的話，
                // 相鄰兩組會疊在一起，那比重疊更糟。
                if (!found) chosen = (blocked.first().second > 0) ? maxOffset : -maxOffset;
            }
            placed.append({g, chosen, v});
        }
        out.append(placed);
    }
    return out;
}

AxisLimits Beeswarm::axisLimits() const {
    AxisLimits lim;
    if (groups.isEmpty()) return lim;
    for (int g = 0; g < groups.size(); ++g) {
        for (double v : groups[g].values) {
            if (!std::isfinite(v)) continue;
            if (horizontal) lim.expand({v, static_cast<double>(g)});
            else            lim.expand({static_cast<double>(g), v});
        }
    }
    // 類別軸兩端各留半格
    if (lim.hasData()) {
        if (horizontal) {
            lim.expand({lim.x.min, -0.5});
            lim.expand({lim.x.max, groups.size() - 0.5});
        } else {
            lim.expand({-0.5, lim.y.min});
            lim.expand({groups.size() - 0.5, lim.y.max});
        }
    }
    return lim;
}

void Beeswarm::render(RenderPack& rp) {
    if (groups.isEmpty() || !marker.isVisible()) return;
    const Transform t = transform(rp);

    // 標記的半徑換算到兩個軸的資料單位。版面必須知道標記在畫面上多大，
    // 不然不同縮放下會得到不同的疏密。
    const double rPx = marker.size / 2 + std::max(0.0, spacingPx) / 2;
    const double sx = std::abs(t.scaleX()), sy = std::abs(t.scaleY());
    if (!(sx > 0) || !(sy > 0)) return;
    const double valueRadius = horizontal ? rPx / sx : rPx / sy;
    const double categoryRadius = horizontal ? rPx / sy : rPx / sx;

    const QVector<SwarmPoint> pts = layout(valueRadius, categoryRadius);
    QPainter& p = *rp.painter;
    p.save();

    // 一組一組畫，顏色才跟著組走
    for (int g = 0; g < groups.size(); ++g) {
        QVector<QPointF> centers;
        for (const SwarmPoint& sp : pts) {
            if (sp.group != g) continue;
            centers.append(horizontal ? t.p(sp.value, g + sp.offset)
                                      : t.p(g + sp.offset, sp.value));
        }
        MarkerStyle m = marker;
        if (groups[g].fillColor.isValid()) m.fillColor = groups[g].fillColor;
        drawMarkers(p, centers, m);
    }
    p.restore();
}

QVector<LegendItem> Beeswarm::legendItems() const {
    QVector<LegendItem> items;
    for (const SwarmGroup& g : groups) {
        const QString text = g.legendText.isEmpty() ? g.label : g.legendText;
        if (text.isEmpty()) continue;
        LegendItem it;
        it.text = text;
        MarkerStyle m = marker;
        if (g.fillColor.isValid()) m.fillColor = g.fillColor;
        it.marker = m;
        it.line = LineStyle::none();
        items.append(it);
    }
    return items;
}

// ───────────────────────── Lollipop ─────────────────────────

QVector<LollipopItem> Lollipop::fromValues(const QVector<double>& values, QColor color) {
    QVector<LollipopItem> out;
    out.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i) {
        LollipopItem it;
        it.position = static_cast<double>(i);
        it.value = values[i];
        it.color = color;
        out.append(it);
    }
    return out;
}

AxisLimits Lollipop::axisLimits() const {
    AxisLimits lim;
    for (const LollipopItem& it : items) {
        if (!std::isfinite(it.value)) continue;
        CoordinateRange value;
        value.expand(it.value);
        if (includeBaseline) value.expand(baseline);
        if (horizontal) {
            lim.x.expand(value);
            lim.y.expand(it.position);
        } else {
            lim.x.expand(it.position);
            lim.y.expand(value);
        }
    }
    // 類別軸兩端各留半格，頭尾的圓點才不會被切掉
    if (horizontal) lim.y.pad(0.08);
    else            lim.x.pad(0.08);
    return lim;
}

void Lollipop::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    auto pt = [&](double along, double value) {
        return horizontal ? t.p(value, along) : t.p(along, value);
    };

    // 先畫完所有桿子再畫頭，圓點才不會被後面的桿子壓到
    if (stem.isVisible()) {
        p.setPen(stem.pen());
        for (const LollipopItem& it : items) {
            if (!std::isfinite(it.value)) continue;
            p.drawLine(pt(it.position, baseline), pt(it.position, it.value));
        }
    }

    for (const LollipopItem& it : items) {
        if (!std::isfinite(it.value)) continue;
        MarkerStyle m = marker;
        if (it.color.isValid()) {
            m.fillColor = it.color;
            m.outline.color = it.color;
        }
        drawMarker(p, pt(it.position, it.value), m);

        if (showValueLabels && labelFont.color.alpha() > 0) {
            Label lb;
            lb.text = it.label.isEmpty() ? QString::number(it.value, 'g', 4) : it.label;
            lb.font = labelFont;
            const double gap = marker.size / 2 + 4;
            const bool positive = it.value >= baseline;
            QPointF anchor = pt(it.position, it.value);
            if (horizontal) {
                anchor += QPointF(positive ? gap : -gap, 0);
                drawText(p, anchor, lb, (positive ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
            } else {
                anchor += QPointF(0, positive ? -gap : gap);
                drawText(p, anchor, lb, Qt::AlignHCenter | (positive ? Qt::AlignBottom : Qt::AlignTop));
            }
        }
    }
    p.restore();
}

QVector<LegendItem> Lollipop::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = stem;
    it.marker = marker;
    return {it};
}

// ───────────────────────── Population ─────────────────────────

AxisLimits Population::axisLimits() const {
    AxisLimits lim;
    for (const Group& g : groups) {
        const BoxStats s = BoxStats::fromValues(g.values, 0);  // 範圍要含全部資料
        if (!s.isValid()) continue;
        lim.x.expand(g.position - groupWidth / 2);
        lim.x.expand(g.position + groupWidth / 2);
        // 密度曲線畫得比資料更寬，範圍沒算進去的話小提琴的頭尾會被裁掉
        const double pad = showDistribution ? (s.max - s.min) * distributionPadFraction : 0.0;
        lim.y.expand(s.min - pad);
        lim.y.expand(s.max + pad);
    }
    return lim;
}

void Population::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    for (int gi = 0; gi < groups.size(); ++gi) {
        const Group& g = groups.at(gi);
        const BoxStats s = BoxStats::fromValues(g.values, 1.5);
        if (!s.isValid()) continue;

        const QColor color = g.color.isValid() ? g.color : QColor(Qt::gray);
        const double half = groupWidth / 2;

        // ── 密度曲線（小提琴）──
        if (showDistribution && g.values.size() >= 2) {
            const BoxStats full = BoxStats::fromValues(g.values, 0);
            const double pad = (full.max - full.min) * distributionPadFraction + 1e-12;
            QVector<double> ys;
            ys.reserve(distributionSamples);
            for (int i = 0; i < distributionSamples; ++i)
                ys.append(full.min - pad +
                          (full.max - full.min + 2 * pad) * i / (distributionSamples - 1.0));
            const QVector<double> dens = kernelDensity(g.values, ys);

            double maxDens = 0;
            for (double d : dens) maxDens = std::max(maxDens, d);
            if (maxDens > 0) {
                QPolygonF poly;
                poly.reserve(distributionSamples * 2);
                const double dc = g.position + groupWidth * distributionOffset;
                for (int i = 0; i < ys.size(); ++i) {
                    const double w = half * dens[i] / maxDens;
                    poly.append(t.p(dc + w, ys[i]));
                }
                // mirrored 畫成左右對稱（小提琴）；否則只畫右半邊再沿中線收回來
                for (int i = ys.size() - 1; i >= 0; --i) {
                    const double w = mirrored ? -half * dens[i] / maxDens : 0.0;
                    poly.append(t.p(dc + w, ys[i]));
                }
                p.setBrush(QBrush(withAlpha(color, fillAlpha)));
                p.setPen(distributionLine.isVisible() ? distributionLine.pen() : QPen(Qt::NoPen));
                p.drawPolygon(poly);
            }
        }

        // ── 平均值長條 ──
        if (showMeanBar) {
            const QRectF r = QRectF(t.p(g.position - half * 0.8, s.mean),
                                    t.p(g.position + half * 0.8, 0.0))
                                 .normalized();
            p.setBrush(QBrush(withAlpha(color, fillAlpha)));
            p.setPen(boxBorder.isVisible() ? boxBorder.pen() : QPen(Qt::NoPen));
            p.drawRect(r);
        }

        // ── 箱 ──
        if (showBox) {
            const double bw = half * 0.42;
            const double bc = g.position + groupWidth * boxOffset;
            double lo = s.q1, hi = s.q3, mid = s.median;
            switch (boxMode) {
            case BoxMode::Quartiles:  break;
            case BoxMode::MeanStdDev: lo = s.mean - s.stdDev; hi = s.mean + s.stdDev; mid = s.mean; break;
            case BoxMode::MeanStdErr: lo = s.mean - s.stdErr; hi = s.mean + s.stdErr; mid = s.mean; break;
            case BoxMode::Whiskers:    lo = s.min; hi = s.max; mid = s.mean; break;
            }
            const QRectF r = QRectF(t.p(bc - bw, lo), t.p(bc + bw, hi)).normalized();
            p.setBrush(QBrush(withAlpha(color, showDistribution ? 200 : fillAlpha)));
            p.setPen(boxBorder.isVisible() ? boxBorder.pen() : QPen(Qt::NoPen));
            p.drawRect(r);
            if (boxBorder.isVisible()) {
                LineStyle med = boxBorder;
                med.width = boxBorder.width * 2;
                p.setPen(med.pen());
                p.drawLine(t.p(bc - bw, mid), t.p(bc + bw, mid));
            }
        }

        // ── 誤差線 ──
        if (showErrorBar && errorLine.isVisible()) {
            double lo = 0, hi = 0;
            switch (errorKind) {
            case ErrorKind::StdDev: lo = s.mean - s.stdDev; hi = s.mean + s.stdDev; break;
            case ErrorKind::StdErr: lo = s.mean - s.stdErr; hi = s.mean + s.stdErr; break;
            case ErrorKind::Iqr:    lo = s.q1; hi = s.q3; break;
            }
            p.setPen(errorLine.pen());
            const double cap = half * 0.22;
            const double ec = g.position + groupWidth * boxOffset;
            p.drawLine(t.p(ec, lo), t.p(ec, hi));
            p.drawLine(t.p(ec - cap, lo), t.p(ec + cap, lo));
            p.drawLine(t.p(ec - cap, hi), t.p(ec + cap, hi));
            // 平均值的橫線畫粗一點，和誤差端帽區分開
            LineStyle meanLine = errorLine;
            meanLine.width = errorLine.width * 2;
            p.setPen(meanLine.pen());
            p.drawLine(t.p(ec - cap * 1.4, s.mean), t.p(ec + cap * 1.4, s.mean));
        }

        // ── 個別資料點 ──
        if (showPoints && pointMarker.isVisible()) {
            const QVector<double> offsets =
                (jitter > 0) ? jitterOffsets(gi, static_cast<int>(g.values.size()))
                             : QVector<double>(g.values.size(), 0.0);
            QVector<QPointF> pts;
            pts.reserve(g.values.size());
            for (int i = 0; i < g.values.size(); ++i) {
                const double v = g.values.at(i);
                if (!std::isfinite(v)) continue;
                pts.append(t.p(g.position + groupWidth * pointsOffset + half * jitter * offsets.at(i), v));
            }
            MarkerStyle m = pointMarker;
            if (!m.fillColor.isValid()) m.fillColor = color;
            drawMarkers(p, pts, m);
        }
    }
    p.restore();
}

QVector<LegendItem> Population::legendItems() const {
    QVector<LegendItem> out;
    for (const Group& g : groups) {
        const QString text = g.legendText.isEmpty() ? g.label : g.legendText;
        if (text.isEmpty()) continue;
        LegendItem it;
        it.text = text;
        it.fill = FillStyle(withAlpha(g.color.isValid() ? g.color : QColor(Qt::gray), 200));
        it.line = LineStyle::none();
        out.append(it);
    }
    return out;
}


// ───────────────────────── 山脊圖 ─────────────────────────

QVector<double> Ridgeline::groupPositions() const {
    QVector<double> pos;
    pos.reserve(groups.size());
    for (int i = 0; i < groups.size(); ++i)
        pos.append(topToBottom ? static_cast<double>(groups.size() - 1 - i)
                               : static_cast<double>(i));
    return pos;
}

QStringList Ridgeline::orderedLabels() const {
    QStringList out;
    out.resize(groups.size());
    const QVector<double> pos = groupPositions();
    for (int i = 0; i < groups.size(); ++i) {
        const int slot = static_cast<int>(pos[i]);
        if (slot >= 0 && slot < out.size()) out[slot] = groups[i].label;
    }
    return out;
}

namespace {

/// 一組資料的核密度曲線，以及取樣用的 X。
struct RidgeCurve {
    QVector<double> xs;
    QVector<double> density;
    double peak = 0;
};

RidgeCurve ridgeCurve(const QVector<double>& values, double bandwidth, int samples,
                      double padFraction) {
    RidgeCurve c;
    CoordinateRange r;
    for (double v : values) r.expand(v);
    if (!r.isValid() || samples < 2) return c;
    r.pad(padFraction);
    r.ensureArea();

    c.xs.reserve(samples);
    for (int i = 0; i < samples; ++i)
        c.xs.append(r.min + (r.max - r.min) * i / (samples - 1));
    c.density = kernelDensity(values, c.xs, bandwidth);
    for (double d : c.density) c.peak = std::max(c.peak, d);
    return c;
}

}  // namespace

AxisLimits Ridgeline::axisLimits() const {
    AxisLimits lim;
    if (groups.isEmpty()) return lim;
    for (const RidgeGroup& g : groups) {
        const RidgeCurve c = ridgeCurve(g.values, bandwidth, samples, padFraction);
        for (double x : c.xs) lim.x.expand(x);
    }
    const QVector<double> pos = groupPositions();
    for (double p : pos) lim.y.expand(p);
    // 最上面那條的峰頂會長到基線之上 (1 + overlap) 格，不留出來就會被切掉
    lim.y.max += 1.0 + std::max(0.0, overlap);
    lim.y.min -= 0.35;
    return lim;
}

void Ridgeline::render(RenderPack& rp) {
    if (groups.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    const QVector<double> pos = groupPositions();
    const double height = 1.0 + std::max(0.0, overlap);

    // 由遠而近畫：位置高（畫面上方）的先畫，位置低的後畫蓋上去。
    //
    // 想成一排由近而遠的山 —— 近的那座擋住遠的那座的山腳。
    // 反過來畫的話，遠山的山腳會浮在近山前面，重疊處看起來像破掉的透明層。
    QVector<int> order(groups.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return pos[a] > pos[b]; });

    for (int idx : order) {
        const RidgeGroup& g = groups[idx];
        const RidgeCurve c = ridgeCurve(g.values, bandwidth, samples, padFraction);
        if (c.xs.size() < 2 || !(c.peak > 0)) continue;

        const double base = pos[idx];
        QPolygonF poly;
        poly.reserve(c.xs.size() + 2);
        poly << t.p(c.xs.first(), base);
        for (int i = 0; i < c.xs.size(); ++i)
            poly << t.p(c.xs[i], base + c.density[i] / c.peak * height);
        poly << t.p(c.xs.last(), base);

        QColor fill = g.color.isValid() ? g.color : palette.color(idx);
        fill.setAlpha(std::clamp(fillAlpha, 0, 255));
        p.setBrush(QBrush(fill));
        QPen pen = line.isVisible() ? line.pen() : QPen(Qt::NoPen);
        if (line.isVisible()) pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        p.drawPolygon(poly);

        if (showBaseline && baseline.isVisible()) {
            QPen bp = baseline.pen();
            bp.setWidthF(bp.widthF() * rp.scaleFactor);
            p.setPen(bp);
            p.drawLine(t.p(c.xs.first(), base), t.p(c.xs.last(), base));
        }
    }
    p.restore();
}

QVector<LegendItem> Ridgeline::legendItems() const {
    QVector<LegendItem> out;
    for (int i = 0; i < groups.size(); ++i) {
        if (groups[i].label.isEmpty()) continue;
        LegendItem it;
        it.text = groups[i].label;
        it.fill = FillStyle(groups[i].color.isValid() ? groups[i].color : palette.color(i));
        it.line = LineStyle::none();
        out.append(it);
    }
    return out;
}

}  // namespace qtplot

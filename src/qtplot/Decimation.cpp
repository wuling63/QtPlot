#include "Decimation.h"

#include <QPainter>

namespace qtplot {

void MinMaxIndex::clear() {
    m_min.clear();
    m_max.clear();
    m_size = 0;
}

void MinMaxIndex::build(const double* data, qsizetype n) {
    clear();
    m_size = n;
    if (!data || n < kFanout) return;
    constexpr double inf = std::numeric_limits<double>::infinity();

    // 第 0 層：直接從原始資料算
    {
        const qsizetype cells = n / kFanout;  // 不足一格的尾巴查詢時直接掃
        std::vector<double> mn(static_cast<size_t>(cells)), mx(static_cast<size_t>(cells));
        for (qsizetype c = 0; c < cells; ++c) {
            double lo = inf, hi = -inf;
            const double* p = data + c * kFanout;
            for (int i = 0; i < kFanout; ++i) {
                const double v = p[i];
                if (!std::isfinite(v)) continue;
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
            mn[c] = lo;
            mx[c] = hi;
        }
        m_min.push_back(std::move(mn));
        m_max.push_back(std::move(mx));
    }
    // 往上一層一層合併，直到剩不到一格
    while (m_min.back().size() >= static_cast<size_t>(kFanout)) {
        const std::vector<double>& pmn = m_min.back();
        const std::vector<double>& pmx = m_max.back();
        const size_t cells = pmn.size() / kFanout;
        std::vector<double> mn(cells), mx(cells);
        for (size_t c = 0; c < cells; ++c) {
            double lo = inf, hi = -inf;
            for (int i = 0; i < kFanout; ++i) {
                lo = std::min(lo, pmn[c * kFanout + i]);
                hi = std::max(hi, pmx[c * kFanout + i]);
            }
            mn[c] = lo;
            mx[c] = hi;
        }
        m_min.push_back(std::move(mn));
        m_max.push_back(std::move(mx));
    }
}

bool MinMaxIndex::query(const double* data, qsizetype a, qsizetype b, double& lo, double& hi) const {
    lo = std::numeric_limits<double>::infinity();
    hi = -std::numeric_limits<double>::infinity();
    a = std::max<qsizetype>(0, a);
    b = std::min(b, m_size);
    if (a >= b) return false;

    // level = -1 是原始資料。每往上一層，先把頭尾不對齊的部分在這一層吃掉，
    // 剩下對齊的中段除以 32 交給上一層。查詢量是 O(32 · 層數)。
    int level = -1;
    auto take = [&](qsizetype i) {
        if (level < 0) {
            const double v = data[i];
            if (!std::isfinite(v)) return;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        } else {
            lo = std::min(lo, m_min[level][static_cast<size_t>(i)]);
            hi = std::max(hi, m_max[level][static_cast<size_t>(i)]);
        }
    };
    while (true) {
        const qsizetype nextCells =
            (level + 1 < static_cast<int>(m_min.size())) ? static_cast<qsizetype>(m_min[level + 1].size()) : 0;
        const qsizetype alignedA = (a + kFanout - 1) / kFanout;  // 上一層第一個完整的格子
        const qsizetype alignedB = std::min(b / kFanout, nextCells);
        if (alignedB - alignedA < 1) {  // 上一層一格都放不下：這一層掃完
            for (qsizetype i = a; i < b; ++i) take(i);
            break;
        }
        for (qsizetype i = a; i < alignedA * kFanout; ++i) take(i);
        for (qsizetype i = alignedB * kFanout; i < b; ++i) take(i);
        a = alignedA;
        b = alignedB;
        ++level;
    }
    return lo <= hi;
}

void drawEnvelope(QPainter& painter, const QVector<EnvelopeColumn>& cols, const LineStyle& line, bool vertical) {
    if (cols.isEmpty() || !line.isVisible()) return;
    const double w = line.hairline ? 1.0 / std::max(1e-6, std::abs(painter.deviceTransform().m11())) : line.width;

    // 每一段連續的行填成一個多邊形：上緣從左走到右，下緣從右走回來。
    // 相鄰兩行之間是斜邊而不是直角，點不算太密（每像素兩三點）時看起來就是一條平滑的線，
    // 不會變成階梯；很密的時候就是一條帶。一整段只填一次，比描一條上下來回的折線快得多。
    auto pt = [vertical](double along, double across) {
        return vertical ? QPointF(across, along) : QPointF(along, across);
    };
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(line.color);
    QPolygonF poly;
    auto flush = [&](int first, int last) {  // [first, last]
        poly.clear();
        poly.reserve((last - first + 1) * 2 + 2);
        const double cap = (first == last) ? w / 2 : 0;  // 單獨一行：至少畫出線寬那麼寬
        for (int i = first; i <= last; ++i)
            poly.append(pt(cols[i].pos - (i == first ? cap : 0), cols[i].lo - w / 2));
        for (int i = last; i >= first; --i)
            poly.append(pt(cols[i].pos + (i == first ? cap : 0), cols[i].hi + w / 2));
        painter.drawPolygon(poly);
    };
    int start = 0;
    for (int i = 1; i <= cols.size(); ++i) {
        // 中間有一行沒資料（NaN）就斷開，不要把缺口補起來
        if (i == cols.size() || cols[i].pos - cols[i - 1].pos > 1.5) {
            flush(start, i - 1);
            start = i;
        }
    }
    painter.restore();
}

}  // namespace qtplot

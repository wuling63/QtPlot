// Decimation.h — 大量資料的抽樣：最小／最大值索引與「包絡線」畫法。
//
// 點數遠多於像素時，每個像素行只需要知道「這一行裡的最小值與最大值」，
// 畫一條從最小到最大的直線，看起來就和畫完全部點一模一樣。
//
//   MinMaxIndex   事先把資料分層算好每一塊的最小／最大值。查任意一段只要 O(log n)，
//                 五千萬點縮到看全圖時不必每畫一張就把全部點掃一遍。
//   buildEnvelope 依像素行把資料切段，每段取最小／最大值，得到每一行的直線。
//   drawEnvelope  把相鄰的行連成一個多邊形一次填完。比把上下來回的鋸齒折線交給
//                 QPainter 描邊快好幾倍（描邊要算每一段的轉角與反鋸齒），
//                 行與行之間是斜邊，點不算太密時看起來和原本的折線一樣平滑。
#pragma once

#include "Core.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace qtplot {

/// 分層的最小／最大值索引。第 k 層的每一格是原始資料連續 32^(k+1) 點的最小與最大值。
///
/// 只看有限值：NaN／無限大會被跳過，整塊都不是有限值時該格是 (+inf, -inf)。
/// 額外記憶體約是原始資料的 1/16。
class MinMaxIndex {
public:
    static constexpr int kFanout = 32;

    void build(const double* data, qsizetype n);
    void clear();
    bool isEmpty() const { return m_min.empty(); }
    qsizetype size() const { return m_size; }

    /// [a, b) 的最小與最大值。沒有任何有限值時回傳 false。
    /// data 必須是 build() 時的同一份資料。
    bool query(const double* data, qsizetype a, qsizetype b, double& lo, double& hi) const;

private:
    std::vector<std::vector<double>> m_min;
    std::vector<std::vector<double>> m_max;
    qsizetype m_size = 0;
};

/// 沒有索引時直接掃：[a, b) 的有限值範圍。
inline bool scanMinMax(const double* data, qsizetype a, qsizetype b, double& lo, double& hi) {
    lo = std::numeric_limits<double>::infinity();
    hi = -std::numeric_limits<double>::infinity();
    for (qsizetype i = a; i < b; ++i) {
        const double v = data[i];
        if (!std::isfinite(v)) continue;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    return lo <= hi;
}

/// 包絡線的一行：沿軸的像素位置，以及垂直方向的兩端（像素，lo <= hi）。
struct EnvelopeColumn {
    double pos;
    double lo;
    double hi;
};

/// 依像素行把 [i0, i1]（兩端都含）切段，每段取數值範圍。
///
/// - pxFrom / pxTo：i0 與 i1 沿軸的像素位置。
/// - boundaryAt(e)：沿軸像素 e 這條邊對應的索引邊界（「從這個索引起落在 e 之後」）。
///   可以隨像素遞增或遞減（反向軸、period < 0 都會遞減），這裡會自己判斷。
/// - valueRange(a, b, lo, hi)：[a, b) 的數值範圍（資料座標），沒有有限值回傳 false。
/// - toPx(v)：數值 → 垂直方向的像素。
///
/// 每段會多算前一個點：相鄰兩行的直線才會接在一起，否則陡升的地方會斷開。
template <class Boundary, class Range, class ToPx>
QVector<EnvelopeColumn> buildEnvelope(double pxFrom, double pxTo, qsizetype i0, qsizetype i1,
                                      Boundary boundaryAt, Range valueRange, ToPx toPx) {
    QVector<EnvelopeColumn> out;
    if (i1 < i0 || !std::isfinite(pxFrom) || !std::isfinite(pxTo)) return out;
    const double pLo = std::min(pxFrom, pxTo), pHi = std::max(pxFrom, pxTo);
    const qsizetype c0 = static_cast<qsizetype>(std::floor(pLo));
    const qsizetype c1 = static_cast<qsizetype>(std::floor(pHi)) + 1;
    if (c1 - c0 > 1'000'000) return out;  // 像素寬得離譜（座標壞掉）：不畫，總比卡死好

    std::vector<qsizetype> edge(static_cast<size_t>(c1 - c0 + 1));
    for (qsizetype k = 0; k < static_cast<qsizetype>(edge.size()); ++k)
        edge[k] = std::clamp<qsizetype>(boundaryAt(static_cast<double>(c0 + k)), i0, i1 + 1);
    const bool reversed = edge.front() > edge.back();

    out.reserve(static_cast<int>(c1 - c0));
    for (qsizetype k = 0; k + 1 < static_cast<qsizetype>(edge.size()); ++k) {
        const qsizetype a = reversed ? edge[k + 1] : edge[k];
        const qsizetype b = reversed ? edge[k] : edge[k + 1];
        if (a >= b) continue;
        double lo, hi;
        if (!valueRange(std::max(i0, a - 1), b, lo, hi)) continue;
        const double p0 = toPx(lo), p1 = toPx(hi);
        out.append({static_cast<double>(c0 + k) + 0.5, std::min(p0, p1), std::max(p0, p1)});
    }
    return out;
}

/// 畫包絡線。vertical = true 時「沿軸」是 Y（垂直方向的 Signal）。
void drawEnvelope(QPainter& painter, const QVector<EnvelopeColumn>& cols, const LineStyle& line, bool vertical);

}  // namespace qtplot

#include "Inspection.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace qtplot {

QVector<ClassMetrics> classMetrics(const QVector<QVector<double>>& counts) {
    const int n = static_cast<int>(counts.size());
    QVector<ClassMetrics> out(n);
    for (int k = 0; k < n; ++k) {
        double rowSum = 0, colSum = 0;
        for (int j = 0; j < n; ++j) {
            if (j < counts[k].size()) rowSum += counts[k][j];
            if (k < counts[j].size()) colSum += counts[j][k];
        }
        const double tp = k < counts[k].size() ? counts[k][k] : 0;
        ClassMetrics& m = out[k];
        m.support = static_cast<int>(std::lround(rowSum));
        m.recall = rowSum > 0 ? tp / rowSum : 0;
        m.precision = colSum > 0 ? tp / colSum : 0;
        m.f1 = (m.precision + m.recall) > 0 ? 2 * m.precision * m.recall / (m.precision + m.recall) : 0;
    }
    return out;
}

double accuracy(const QVector<QVector<double>>& counts) {
    double diag = 0, total = 0;
    for (int i = 0; i < counts.size(); ++i)
        for (int j = 0; j < counts[i].size(); ++j) {
            total += counts[i][j];
            if (i == j) diag += counts[i][j];
        }
    return total > 0 ? diag / total : 0;
}

QVector<QVector<double>> confusionCounts(const QVector<int>& actual, const QVector<int>& predicted, int classCount) {
    QVector<QVector<double>> m(std::max(0, classCount), QVector<double>(std::max(0, classCount), 0.0));
    const qsizetype n = std::min(actual.size(), predicted.size());
    for (qsizetype i = 0; i < n; ++i) {
        const int a = actual[i], p = predicted[i];
        if (a < 0 || p < 0 || a >= classCount || p >= classCount) continue;
        m[a][p] += 1;
    }
    return m;
}

ThresholdStats thresholdStats(const QVector<double>& okScores, const QVector<double>& ngScores, double threshold) {
    ThresholdStats s;
    for (double v : okScores) {
        if (!std::isfinite(v)) continue;
        (v >= threshold ? s.fp : s.tn)++;
    }
    for (double v : ngScores) {
        if (!std::isfinite(v)) continue;
        (v >= threshold ? s.tp : s.fn)++;
    }
    const int ng = s.tp + s.fn, ok = s.tn + s.fp, total = ng + ok;
    s.missRate = ng > 0 ? double(s.fn) / ng : 0;
    s.falseAlarmRate = ok > 0 ? double(s.fp) / ok : 0;
    s.precision = (s.tp + s.fp) > 0 ? double(s.tp) / (s.tp + s.fp) : 0;
    s.accuracy = total > 0 ? double(s.tp + s.tn) / total : 0;
    return s;
}

// ───────────────────────── 混淆矩陣 ─────────────────────────

ConfusionMatrix::ConfusionMatrix() {
    cellFont.size = 10;
}

double ConfusionMatrix::normalized(int row, int col) const {
    const int n = size();
    if (row < 0 || col < 0 || row >= n || col >= counts[row].size()) return 0;
    const double v = counts[row][col];
    double denom = 1;
    switch (normalize) {
    case Normalize::None:
        return v;
    case Normalize::Row:
        denom = 0;
        for (double c : counts[row]) denom += c;
        break;
    case Normalize::Column:
        denom = 0;
        for (int r = 0; r < n; ++r)
            if (col < counts[r].size()) denom += counts[r][col];
        break;
    case Normalize::All:
        denom = 0;
        for (const auto& r : counts)
            for (double c : r) denom += c;
        break;
    }
    return denom > 0 ? v / denom : 0;
}

AxisLimits ConfusionMatrix::axisLimits() const {
    const int n = size();
    if (n == 0) return {};
    return AxisLimits(-0.5, n - 0.5, -0.5, n - 0.5);
}

void ConfusionMatrix::render(RenderPack& rp) {
    const int n = size();
    if (n == 0) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 色階範圍：正規化之後是 0–1；原始筆數用最大格
    double vmax = 0;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < counts[r].size(); ++c) vmax = std::max(vmax, normalized(r, c));
    if (normalize != Normalize::None) vmax = std::max(vmax, 1e-12);
    double total = 0;
    for (const auto& r : counts)
        for (double c : r) total += c;

    QFont font = cellFont.font();
    font.setPointSizeF(cellFont.size * rp.scaleFactor);
    const double cellPx = std::min(std::abs(t.px(1) - t.px(0)), std::abs(t.py(1) - t.py(0)));
    // 格子小到放不下兩行字時只留一行
    const bool twoLines = showCounts && showPercent && cellPx > QFontMetricsF(font).height() * 2.6;

    for (int r = 0; r < n; ++r) {
        const double y = n - 1 - r;  // 第 0 列在最上面
        for (int c = 0; c < n; ++c) {
            const double v = c < counts[r].size() ? counts[r][c] : 0;
            const double nv = normalized(r, c);
            const QRectF cell = t.pixelRect(CoordinateRect(c - 0.5, c + 0.5, y - 0.5, y + 0.5));
            const QColor fill = colormap.lookup(vmax > 0 ? nv / vmax : 0);
            p.setPen(gridLine.isVisible() ? gridLine.pen() : QPen(Qt::NoPen));
            p.setBrush(fill);
            p.drawRect(cell);

            QStringList lines;
            const double share = normalize == Normalize::None ? (total > 0 ? v / total : 0) : nv;
            if (showCounts) lines << QString::number(v, 'g', 6);
            if (showPercent) lines << QStringLiteral("%1%").arg(share * 100, 0, 'f', 1);
            if (!twoLines && lines.size() > 1) lines = {normalize == Normalize::None ? lines.first() : lines.last()};
            if (lines.isEmpty()) continue;
            // 深色格子用白字，淺色用黑字
            const QColor ink = fill.lightnessF() < 0.55 ? QColor(255, 255, 255) : QColor(25, 25, 25);
            p.setPen(ink);
            p.setFont(font);
            p.drawText(cell, Qt::AlignCenter, lines.join(QLatin1Char('\n')));
        }
    }

    // 對角線（答對的格子）加框，一眼看出模型主要錯在哪裡
    if (highlightDiagonal && diagonalBorder.isVisible()) {
        QPen pen = diagonalBorder.pen();
        pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        for (int k = 0; k < n; ++k) {
            const double y = n - 1 - k;
            const QRectF cell = t.pixelRect(CoordinateRect(k - 0.5, k + 0.5, y - 0.5, y + 0.5));
            p.drawRect(cell.adjusted(pen.widthF() / 2, pen.widthF() / 2, -pen.widthF() / 2, -pen.widthF() / 2));
        }
    }
    p.restore();
}

}  // namespace qtplot

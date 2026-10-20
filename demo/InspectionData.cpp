#include "InspectionData.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <random>

namespace qtplot {
namespace demo {

namespace {

constexpr int kW = 320, kH = 240;

/// 點 (px, py) 到線段 (ax, ay)–(bx, by) 的距離。
double segmentDistance(double px, double py, double ax, double ay, double bx, double by) {
    const double vx = bx - ax, vy = by - ay;
    const double len2 = vx * vx + vy * vy;
    const double t = len2 > 0 ? std::clamp(((px - ax) * vx + (py - ay) * vy) / len2, 0.0, 1.0) : 0;
    return std::hypot(px - (ax + t * vx), py - (ay + t * vy));
}

/// 瑕疵的形狀：回傳影像上某點「離瑕疵多近」（0 = 遠，1 = 正中）。
struct Defect {
    enum Kind { Scratch, Spot, Pits } kind;
    double strength(double x, double y) const {
        switch (kind) {
        case Scratch:
            return std::exp(-std::pow(segmentDistance(x, y, 70, 60, 230, 170) / 3.0, 2));
        case Spot:
            return std::exp(-(std::pow(x - 210, 2) + std::pow(y - 90, 2)) / (2 * 9.0 * 9.0));
        case Pits:
            return std::max(std::exp(-(std::pow(x - 100, 2) + std::pow(y - 160, 2)) / (2 * 5.0 * 5.0)),
                            std::exp(-(std::pow(x - 245, 2) + std::pow(y - 70, 2)) / (2 * 6.0 * 6.0)));
        }
        return 0;
    }
    /// 影像上要變暗（刮痕、黑點）還是變亮（凹坑反光）。
    double shade() const { return kind == Pits ? +80 : -90; }
};

}  // namespace

int inspectionSampleCount() { return 5; }

InspectionSample makeInspectionSample(int index) {
    static const char* names[] = {"良品 A", "良品 B", "刮痕", "黑點", "凹坑"};
    InspectionSample s;
    index = std::clamp(index, 0, inspectionSampleCount() - 1);
    s.name = QString::fromUtf8(names[index]);
    s.defective = index >= 2;
    std::mt19937 rng(1000 + index);
    std::normal_distribution<double> noise(0, 1);

    const bool hasDefect = s.defective;
    const Defect defect{index == 2 ? Defect::Scratch : index == 3 ? Defect::Spot : Defect::Pits};

    // ── 表面：髮絲紋（沿 X 的細紋）、由中間往外變暗、加一點雜訊 ──
    s.image = QImage(kW, kH, QImage::Format_RGB32);
    std::vector<double> rowGrain(kH);
    for (int y = 0; y < kH; ++y) rowGrain[y] = noise(rng) * 6;
    for (int y = 0; y < kH; ++y) {
        auto* line = reinterpret_cast<QRgb*>(s.image.scanLine(y));
        for (int x = 0; x < kW; ++x) {
            const double dx = (x - kW / 2.0) / kW, dy = (y - kH / 2.0) / kH;
            double v = 165 + rowGrain[y] + 6 * std::sin(x * 0.05 + y * 0.9) - 70 * (dx * dx + dy * dy) + noise(rng) * 4;
            if (hasDefect) v += defect.shade() * defect.strength(x, y);
            const int g = std::clamp(static_cast<int>(std::lround(v)), 0, 255);
            line[x] = qRgb(g, g, static_cast<int>(std::clamp(g * 1.03, 0.0, 255.0)));
        }
    }

    // ── 異常熱圖：模型輸出的解析度通常比影像低，這裡是 64 × 48 ──
    // 良品也有一點雜訊和偶發的小隆起（打光不均），才看得出門檻太低時會誤判
    s.anomaly.resize(s.rows * s.cols);
    std::vector<double> raw(s.rows * s.cols);
    for (double& v : raw) v = 0.12 + std::abs(noise(rng)) * 0.06;
    for (int r = 0; r < s.rows; ++r)
        for (int c = 0; c < s.cols; ++c) {
            // 3×3 平均，讓雜訊不要一格一格跳
            double sum = 0;
            int n = 0;
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc) {
                    const int rr = r + dr, cc = c + dc;
                    if (rr < 0 || cc < 0 || rr >= s.rows || cc >= s.cols) continue;
                    sum += raw[rr * s.cols + cc];
                    ++n;
                }
            double v = sum / n;
            const double x = (c + 0.5) * kW / s.cols, y = (r + 0.5) * kH / s.rows;
            if (index == 1) v += 0.28 * std::exp(-(std::pow(x - 80, 2) + std::pow(y - 60, 2)) / (2 * 25.0 * 25.0));
            if (hasDefect) {
                // 模型看到的瑕疵會比實際範圍大一圈
                const double spread = std::max({defect.strength(x, y), defect.strength(x + 4, y), defect.strength(x - 4, y),
                                                defect.strength(x, y + 4), defect.strength(x, y - 4)});
                v += 0.85 * spread;
            }
            s.anomaly[r * s.cols + c] = std::clamp(v, 0.0, 1.0);
        }
    return s;
}

InspectionScores makeInspectionScores() {
    InspectionScores s;
    std::mt19937 rng(20261020);
    std::normal_distribution<double> ok(0.32, 0.075), ng(0.63, 0.12);
    for (int i = 0; i < 400; ++i) s.ok.append(std::clamp(ok(rng), 0.0, 1.0));
    for (int i = 0; i < 120; ++i) s.ng.append(std::clamp(ng(rng), 0.0, 1.0));
    return s;
}

}  // namespace demo
}  // namespace qtplot

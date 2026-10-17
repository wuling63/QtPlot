#include "DataGen.h"

#include <QtGlobal>

#include <chrono>
#include <cmath>
#include <random>

namespace qtplot {
namespace gen {

namespace {

/// 固定種子，讓範例圖可重現。setSeed() 可以換掉。
std::mt19937& rng() {
    static std::mt19937 engine(0);
    return engine;
}

}  // namespace

void setSeed(unsigned int seed) {
    if (seed == 0) {
        rng().seed(static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    } else {
        rng().seed(seed);
    }
}

void resetSeed() {
    // 和 rng() 裡建構引擎時用的種子一致，等於把它倒回程式剛啟動的狀態
    rng().seed(0u);
}

QVector<double> consecutive(int count, double step, double offset) {
    QVector<double> out;
    if (count <= 0) return out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) out.append(offset + i * step);
    return out;
}

QVector<double> linspace(double start, double stop, int count) {
    QVector<double> out;
    if (count <= 0) return out;
    out.reserve(count);
    if (count == 1) {
        out.append(start);
        return out;
    }
    const double step = (stop - start) / (count - 1);
    for (int i = 0; i < count; ++i) out.append(start + i * step);
    return out;
}

QVector<double> range(double start, double stop, double step) {
    QVector<double> out;
    if (step == 0) return out;
    // 用計數而不是累加，避免浮點誤差累積讓最後一點少掉或多出來
    const int count = static_cast<int>(std::floor((stop - start) / step));
    out.reserve(std::max(0, count));
    for (int i = 0; i < count; ++i) out.append(start + i * step);
    return out;
}

QVector<double> sin(int count, double amplitude, double oscillations, double phase, double offset) {
    QVector<double> out;
    if (count <= 0) return out;
    out.reserve(count);
    const double sweep = 2 * M_PI * oscillations / count;
    for (int i = 0; i < count; ++i)
        out.append(offset + amplitude * std::sin(i * sweep + phase * 2 * M_PI));
    return out;
}

QVector<double> cos(int count, double amplitude, double oscillations, double phase, double offset) {
    QVector<double> out;
    if (count <= 0) return out;
    out.reserve(count);
    const double sweep = 2 * M_PI * oscillations / count;
    for (int i = 0; i < count; ++i)
        out.append(offset + amplitude * std::cos(i * sweep + phase * 2 * M_PI));
    return out;
}

QVector<double> randomUniform(int count, double min, double max) {
    QVector<double> out;
    if (count <= 0) return out;
    out.reserve(count);
    std::uniform_real_distribution<double> dist(min, max);
    for (int i = 0; i < count; ++i) out.append(dist(rng()));
    return out;
}

QVector<double> randomNormal(int count, double mean, double stdDev) {
    QVector<double> out;
    if (count <= 0) return out;
    out.reserve(count);
    std::normal_distribution<double> dist(mean, stdDev);
    for (int i = 0; i < count; ++i) out.append(dist(rng()));
    return out;
}

QVector<double> randomWalk(int count, double mult, double start) {
    QVector<double> out;
    if (count <= 0) return out;
    out.reserve(count);
    std::normal_distribution<double> dist(0.0, 1.0);
    double v = start;
    for (int i = 0; i < count; ++i) {
        v += dist(rng()) * mult;
        out.append(v);
    }
    return out;
}

QVector<double> addNoise(const QVector<double>& values, double stdDev) {
    QVector<double> out = values;
    std::normal_distribution<double> dist(0.0, stdDev);
    for (double& v : out) v += dist(rng());
    return out;
}

QVector<double> randomSample(int count, double min, double max) {
    QVector<double> out = randomUniform(count, min, max);
    std::sort(out.begin(), out.end());
    return out;
}

QVector<double> gaussian2D(int rows, int cols, double sigma) {
    QVector<double> out;
    if (rows <= 0 || cols <= 0) return out;
    out.resize(static_cast<qsizetype>(rows) * cols);
    const double cy = (rows - 1) / 2.0;
    const double cx = (cols - 1) / 2.0;
    const double sy = std::max(1.0, rows * sigma);
    const double sx = std::max(1.0, cols * sigma);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const double dy = (r - cy) / sy;
            const double dx = (c - cx) / sx;
            out[static_cast<qsizetype>(r) * cols + c] = std::exp(-(dx * dx + dy * dy) / 2);
        }
    }
    return out;
}

QVector<double> sin2D(int rows, int cols, double oscillations) {
    QVector<double> out;
    if (rows <= 0 || cols <= 0) return out;
    out.resize(static_cast<qsizetype>(rows) * cols);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const double x = 2 * M_PI * oscillations * c / cols;
            const double y = 2 * M_PI * oscillations * r / rows;
            out[static_cast<qsizetype>(r) * cols + c] = std::sin(x) * std::cos(y);
        }
    }
    return out;
}

}  // namespace gen
}  // namespace qtplot

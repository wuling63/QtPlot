#include "Spectral.h"

#include <QStringList>

#include <numeric>

namespace qtplot {

// ───────────────────────────── 窗函數 ─────────────────────────────

QVector<double> windowValues(WindowFunction w, int n) {
    QVector<double> out;
    if (n <= 0) return out;
    out.resize(n);
    if (n == 1) {
        out[0] = 1;
        return out;
    }
    const double d = n - 1;
    for (int i = 0; i < n; ++i) {
        const double x = 2 * kPi * i / d;
        switch (w) {
        case WindowFunction::Rectangular:
            out[i] = 1;
            break;
        case WindowFunction::Hann:
            out[i] = 0.5 - 0.5 * std::cos(x);
            break;
        case WindowFunction::Hamming:
            out[i] = 0.54 - 0.46 * std::cos(x);
            break;
        case WindowFunction::Blackman:
            out[i] = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2 * x);
            break;
        case WindowFunction::BlackmanHarris:
            out[i] = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x) -
                     0.01168 * std::cos(3 * x);
            break;
        case WindowFunction::FlatTop:
            out[i] = 0.21557895 - 0.41663158 * std::cos(x) + 0.277263158 * std::cos(2 * x) -
                     0.083578947 * std::cos(3 * x) + 0.006947368 * std::cos(4 * x);
            break;
        }
    }
    return out;
}

double windowCoherentGain(WindowFunction w, int n) {
    const QVector<double> v = windowValues(w, n);
    if (v.isEmpty()) return 1.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

QStringList windowFunctionNames() {
    return {QStringLiteral("Rectangular"), QStringLiteral("Hann"),
            QStringLiteral("Hamming"),     QStringLiteral("Blackman"),
            QStringLiteral("BlackmanHarris"), QStringLiteral("FlatTop")};
}

// ───────────────────────────── FFT ─────────────────────────────

namespace {

bool isPowerOfTwo(int n) { return n > 0 && (n & (n - 1)) == 0; }

int nextPowerOfTwo(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

/// 就地 radix-2 Cooley–Tukey。n 必須是 2 的次方。
void fftRadix2(QVector<double>& re, QVector<double>& im, bool inverse) {
    const int n = static_cast<int>(re.size());
    if (n <= 1) return;

    // 位元反轉重排
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    const double sign = inverse ? 1.0 : -1.0;
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = sign * 2 * kPi / len;
        const double wRe = std::cos(ang);
        const double wIm = std::sin(ang);
        for (int i = 0; i < n; i += len) {
            // 每一段重新起算旋轉因子，不要跨段累乘 —— 累乘的相位誤差會隨 n 線性長大
            double curRe = 1, curIm = 0;
            for (int k = 0; k < len / 2; ++k) {
                const int a = i + k;
                const int b = a + len / 2;
                const double tr = re[b] * curRe - im[b] * curIm;
                const double ti = re[b] * curIm + im[b] * curRe;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
                const double nextRe = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = nextRe;
            }
        }
    }
}

/// Bluestein（chirp-z）：任意長度的 DFT 寫成一次捲積。
///
/// 關鍵等式 nk = (n² + k² − (k−n)²) / 2，把 exp(−2πi·nk/N) 拆成
/// 只與 n 有關、只與 k 有關、以及只與 (k−n) 有關的三項，中間那項就是捲積核。
void fftBluestein(QVector<double>& re, QVector<double>& im, bool inverse) {
    const int n = static_cast<int>(re.size());
    const int m = nextPowerOfTwo(2 * n - 1);
    const double sign = inverse ? 1.0 : -1.0;

    QVector<double> aRe(m, 0.0), aIm(m, 0.0), bRe(m, 0.0), bIm(m, 0.0);
    for (int i = 0; i < n; ++i) {
        // fmod 先把 i² 收回到 [0, 2n)，再乘 π/n。直接算 π·i²/n 的話，
        // i 大時相位是個很大的數，三角函數的有效位數會被吃掉。
        const double ang = sign * kPi * std::fmod(static_cast<double>(i) * i, 2.0 * n) / n;
        const double c = std::cos(ang), s = std::sin(ang);
        aRe[i] = re[i] * c - im[i] * s;
        aIm[i] = re[i] * s + im[i] * c;
        bRe[i] = c;
        bIm[i] = -s;
        if (i > 0) {  // 核是對稱的，補到尾端讓捲積變成循環捲積
            bRe[m - i] = c;
            bIm[m - i] = -s;
        }
    }

    fftRadix2(aRe, aIm, false);
    fftRadix2(bRe, bIm, false);
    for (int i = 0; i < m; ++i) {
        const double r = aRe[i] * bRe[i] - aIm[i] * bIm[i];
        const double q = aRe[i] * bIm[i] + aIm[i] * bRe[i];
        aRe[i] = r;
        aIm[i] = q;
    }
    fftRadix2(aRe, aIm, true);
    for (int i = 0; i < m; ++i) {
        aRe[i] /= m;
        aIm[i] /= m;
    }

    for (int k = 0; k < n; ++k) {
        const double ang = sign * kPi * std::fmod(static_cast<double>(k) * k, 2.0 * n) / n;
        const double c = std::cos(ang), s = std::sin(ang);
        re[k] = aRe[k] * c - aIm[k] * s;
        im[k] = aRe[k] * s + aIm[k] * c;
    }
}

}  // namespace

void fft(QVector<double>& re, QVector<double>& im, bool inverse) {
    const int n = static_cast<int>(re.size());
    if (im.size() != n || n <= 1) return;

    if (isPowerOfTwo(n))
        fftRadix2(re, im, inverse);
    else
        fftBluestein(re, im, inverse);

    if (inverse) {
        for (int i = 0; i < n; ++i) {
            re[i] /= n;
            im[i] /= n;
        }
    }
}

void naiveDft(const QVector<double>& inRe, const QVector<double>& inIm, QVector<double>& outRe,
              QVector<double>& outIm, bool inverse) {
    const int n = static_cast<int>(inRe.size());
    outRe.fill(0.0, n);
    outIm.fill(0.0, n);
    if (inIm.size() != n) return;
    const double sign = inverse ? 1.0 : -1.0;
    for (int k = 0; k < n; ++k) {
        double sr = 0, si = 0;
        for (int t = 0; t < n; ++t) {
            const double ang = sign * 2 * kPi * ((static_cast<long long>(t) * k) % n) / n;
            const double c = std::cos(ang), s = std::sin(ang);
            sr += inRe[t] * c - inIm[t] * s;
            si += inRe[t] * s + inIm[t] * c;
        }
        outRe[k] = inverse ? sr / n : sr;
        outIm[k] = inverse ? si / n : si;
    }
}

// ───────────────────────── 單邊功率譜 ─────────────────────────

double Periodogram::peakFrequency() const {
    if (amplitude.isEmpty()) return std::numeric_limits<double>::quiet_NaN();
    int best = 0;
    for (int i = 1; i < amplitude.size(); ++i)
        if (amplitude[i] > amplitude[best]) best = i;
    return frequencies.value(best, std::numeric_limits<double>::quiet_NaN());
}

Periodogram periodogram(const QVector<double>& samples, double sampleRate, WindowFunction w,
                        double reference) {
    Periodogram out;
    const int n = static_cast<int>(samples.size());
    if (n < 2 || !(sampleRate > 0)) return out;

    const QVector<double> win = windowValues(w, n);
    const double gain = windowCoherentGain(w, n);

    QVector<double> re(n), im(n, 0.0);
    for (int i = 0; i < n; ++i) re[i] = std::isfinite(samples[i]) ? samples[i] * win[i] : 0.0;
    fft(re, im, false);

    const int bins = n / 2 + 1;
    out.frequencies.reserve(bins);
    out.amplitude.reserve(bins);
    out.power.reserve(bins);
    out.decibels.reserve(bins);
    const double ref = (reference > 0) ? reference : 1.0;

    for (int k = 0; k < bins; ++k) {
        // 直流與 Nyquist 沒有對應的負頻率鏡像，所以不乘 2
        const bool mirrored = (k > 0 && !(n % 2 == 0 && k == n / 2));
        const double mag = std::hypot(re[k], im[k]) / n / gain * (mirrored ? 2.0 : 1.0);
        out.frequencies.append(sampleRate * k / n);
        out.amplitude.append(mag);
        out.power.append(mag * mag);
        out.decibels.append(20 * std::log10(std::max(mag, 1e-300) / ref));
    }
    return out;
}

// ───────────────────────── 頻譜圖 ─────────────────────────

CoordinateRange Spectrogram::valueRange() const {
    CoordinateRange r;
    for (double v : values)
        if (std::isfinite(v)) r.expand(v);
    return r;
}

Spectrogram spectrogram(const QVector<double>& samples, double sampleRate, int windowSize,
                        int hopSize, WindowFunction w, bool decibels, double dynamicRangeDb) {
    Spectrogram out;
    if (windowSize < 2 || !(sampleRate > 0) || samples.size() < windowSize) return out;
    if (hopSize <= 0) hopSize = std::max(1, windowSize / 4);

    out.rows = windowSize / 2 + 1;
    out.cols = static_cast<int>((samples.size() - windowSize) / hopSize) + 1;
    out.sampleRate = sampleRate;
    out.windowSize = windowSize;
    out.hopSize = hopSize;
    out.isDecibels = decibels;
    out.values.resize(static_cast<qsizetype>(out.rows) * out.cols);

    const QVector<double> win = windowValues(w, windowSize);
    const double gain = windowCoherentGain(w, windowSize);

    QVector<double> re(windowSize), im(windowSize);
    for (int col = 0; col < out.cols; ++col) {
        const int start = col * hopSize;
        for (int i = 0; i < windowSize; ++i) {
            const double s = samples[start + i];
            re[i] = std::isfinite(s) ? s * win[i] : 0.0;
            im[i] = 0;
        }
        fft(re, im, false);
        for (int k = 0; k < out.rows; ++k) {
            const bool mirrored = (k > 0 && k != windowSize / 2);
            const double mag =
                std::hypot(re[k], im[k]) / windowSize / gain * (mirrored ? 2.0 : 1.0);
            out.values[static_cast<qsizetype>(k) * out.cols + col] =
                decibels ? 20 * std::log10(std::max(mag, 1e-300)) : mag;
        }
    }

    if (decibels && dynamicRangeDb > 0) {
        // 不夾下限的話，靜音的格子是 −6000 dB，整張圖的色階就全被它拉掉了
        double peak = -std::numeric_limits<double>::infinity();
        for (double v : out.values)
            if (std::isfinite(v)) peak = std::max(peak, v);
        if (std::isfinite(peak)) {
            const double floorDb = peak - dynamicRangeDb;
            for (double& v : out.values) v = std::max(v, floorDb);
        }
    }
    return out;
}

}  // namespace qtplot

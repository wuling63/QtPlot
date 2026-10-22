// Spectral.h — FFT、窗函數、功率譜與頻譜圖（STFT）。
//
// 這裡沒有圖元：頻譜圖就是一張 Heatmap，功率譜就是一條 Scatter。
// 要算的是把一段波形變成「哪個時間有哪個頻率」的那個步驟。
#pragma once

#include "Core.h"

#include <QStringList>

namespace qtplot {

// ───────────────────────────── 窗函數 ─────────────────────────────

/// 取一段有限長度的訊號做 FFT，等於把它乘上一個矩形窗，
/// 而矩形窗的頻譜有很強的旁波，於是一個單頻訊號會在頻譜上拖出一片裙邊
/// （頻譜洩漏）。乘上兩端漸收的窗可以把旁波壓下去，代價是主波變寬。
enum class WindowFunction {
    Rectangular,     ///< 不開窗。頻率解析度最好，旁波最差（−13 dB）
    Hann,            ///< 通用首選。旁波 −31 dB，主波寬 2 格
    Hamming,         ///< 旁波 −43 dB，但遠端衰減比 Hann 慢
    Blackman,        ///< 旁波 −58 dB
    BlackmanHarris,  ///< 旁波 −92 dB，主波寬 4 格，適合量很小的雜訊底
    FlatTop          ///< 主波最寬但振幅最準，量單一正弦波的幅值時用
};

/// 窗函數的 n 個係數。用「對稱」的定義（分母 n−1），和 scipy.signal.get_window
/// 的 sym=True 一致。做 FFT 其實該用週期型（分母 n），但兩者在 n 大時差異
/// 小於 1/n，而對稱型拿去畫窗本身的形狀才不會缺一格。
QVector<double> windowValues(WindowFunction w, int n);

/// 窗函數的相干增益（係數平均值）。還原振幅時要除掉它。
double windowCoherentGain(WindowFunction w, int n);

/// 窗函數的名稱。
QStringList windowFunctionNames();

// ───────────────────────────── FFT ─────────────────────────────

/// 就地做複數 FFT。re 與 im 必須同長度；長度任意（不限 2 的次方）。
///
/// 長度是 2 的次方時走 radix-2，否則走 Bluestein（把 DFT 寫成一次捲積，
/// 再用補零到 2 的次方的 FFT 算那個捲積）。只支援 2 的次方的 FFT 很常見，
/// 但那會逼呼叫端自己補零，而補零改變的是頻率格的間距 ——
/// 「我要 1000 點的頻譜」和「補到 1024 點的頻譜」刻度並不一樣，
/// 呼叫端自己補很容易把這件事弄錯。
///
/// inverse = true 時算反變換，並除以 n（所以 fft 後接 ifft 會回到原值）。
void fft(QVector<double>& re, QVector<double>& im, bool inverse = false);

/// 樸素 DFT，O(n²)。只用來驗證 fft() —— 兩者結果必須一致。
void naiveDft(const QVector<double>& inRe, const QVector<double>& inIm, QVector<double>& outRe,
              QVector<double>& outIm, bool inverse = false);

// ───────────────────────── 單邊功率譜 ─────────────────────────

/// 一段訊號的功率譜。
struct Periodogram {
    QVector<double> frequencies;  ///< Hz，從 0 到 sampleRate/2
    QVector<double> amplitude;    ///< 還原過窗增益的振幅（和輸入同單位）
    QVector<double> power;        ///< 振幅的平方
    QVector<double> decibels;     ///< 20·log10(amplitude / reference)
    /// 平均了幾段。periodogram() 是 1；welch() 是實際切出來的段數。
    int segments = 1;

    /// 振幅最大的那一格的頻率。空譜回傳 NaN。
    double peakFrequency() const;
};

/// 單邊功率譜。
///
/// 單邊的意思是只留 0 到 Nyquist；實數訊號的負頻率是正頻率的共軛，
/// 畫出來只是鏡像。除了直流與 Nyquist 之外的格子振幅要乘 2，
/// 把被丟掉的那一半的能量補回來 —— 漏掉這一步，算出來的振幅會剛好差一半。
///
/// reference 是 dB 的 0 點。
Periodogram periodogram(const QVector<double>& samples, double sampleRate,
                        WindowFunction w = WindowFunction::Hann, double reference = 1.0);

/// Welch 平均功率譜。
///
/// 把訊號切成 segmentSize 長、彼此重疊 overlap（0～0.95）的段，每段開窗做一次功率譜，
/// 再把「功率」平均起來。單一段的功率譜每一格都是一個隨機變數 —— 雜訊底看起來像草叢，
/// 平均 K 段之後起伏大約縮成 1/√K，很小的峰才不會被草叢藏住。
/// 代價是頻率格距變成 sampleRate / segmentSize（比整段做 FFT 粗）。
///
/// 振幅的換算和 periodogram() 一樣（正弦波的峰值振幅），兩者可以畫在同一張圖上比較。
/// 訊號比 segmentSize 短時退回整段做一次 periodogram()。
Periodogram welch(const QVector<double>& samples, double sampleRate, int segmentSize,
                  double overlap = 0.5, WindowFunction w = WindowFunction::Hann, double reference = 1.0);

// ───────────────────────── 找頻譜峰值 ─────────────────────────

/// 頻譜上的一個峰。
struct SpectralPeak {
    double frequency = 0;   ///< Hz，用拋物線內插過（比格距準）
    double decibels = 0;    ///< 內插後的峰值 dB
    double amplitude = 0;   ///< 由 decibels 換回來的振幅（同 Periodogram::amplitude 的單位）
    double prominence = 0;  ///< 比附近的雜訊底（鄰近格子的中位數）高出幾 dB
    int bin = 0;            ///< 峰所在的頻率格
};

/// 找出頻譜上最明顯的幾個峰，依高度由大到小排。
///
/// - minProminenceDb：峰要比附近的雜訊底高出這麼多才算。雜訊底取峰兩側各約 2% 頻寬內
///   格子的中位數（扣掉峰本身的主波）—— 中位數不怕旁邊有別的峰。
///   不用地形學那種「比兩側谷底高多少」：雜訊譜的谷底常常深達 −30 dB，
///   照那個算法，純雜訊裡最高的那一格永遠會被當成峰。
///   純雜訊譜裡最高的格子大約比中位數高 8～10 dB，所以門檻低於 10 dB 時要有心理準備。
/// - minSpacingHz：兩個峰至少隔這麼遠，太近的只留比較高的那個。
/// - minFrequency：低於這個頻率的不算（直流與極低頻的漂移常常是最高的，卻不是想看的）。
///
/// 頻率用相鄰三格的 dB 做拋物線內插：開了窗的峰形接近高斯，在對數軸上近似拋物線，
/// 內插後的誤差遠小於一格。不內插的話，100.7 Hz 在 2 Hz 格距下只會報 100 或 102。
QVector<SpectralPeak> findSpectralPeaks(const Periodogram& spectrum, int maxPeaks = 5,
                                        double minProminenceDb = 10, double minSpacingHz = 0,
                                        double minFrequency = 0);

// ───────────────────────── 頻譜圖（STFT） ─────────────────────────

/// 短時傅立葉變換的結果，可以直接餵給 Heatmap。
struct Spectrogram {
    /// row-major，共 rows × cols 個值。
    ///
    /// **第 0 列是最低頻**。Heatmap 預設把第 0 列畫在「上面」，所以拿去畫的時候
    /// 要把 flipVertically 設成 true，低頻才會在下面（Plot::addSpectrogram 會設）。
    /// 這裡選「低頻在第 0 列」是因為用索引取值的人想的是 values[freq][time]，
    /// 反過來存會讓每個讀資料的人都得先想一次。
    QVector<double> values;
    int rows = 0;  ///< 頻率格數 = windowSize/2 + 1
    int cols = 0;  ///< 時間格數

    double sampleRate = 0;
    int windowSize = 0;
    int hopSize = 0;
    bool isDecibels = false;

    /// 頻率軸的上限（Nyquist）。
    double maxFrequency() const { return sampleRate / 2; }
    /// 時間軸的長度（秒）。
    double duration() const { return (cols > 0) ? cols * hopSize / sampleRate : 0; }
    /// 給 Heatmap::extent 用的範圍：X 是秒、Y 是 Hz。
    CoordinateRect extent() const { return {0, duration(), 0, maxFrequency()}; }

    double at(int freqIndex, int timeIndex) const {
        return (freqIndex >= 0 && freqIndex < rows && timeIndex >= 0 && timeIndex < cols)
                   ? values.at(static_cast<qsizetype>(freqIndex) * cols + timeIndex)
                   : std::numeric_limits<double>::quiet_NaN();
    }
    CoordinateRange valueRange() const;
};

/// 頻譜圖。
///
/// windowSize 同時決定兩個軸的解析度，而且是反向的：窗長則頻率分得細、
/// 時間分得粗（短促的聲音被抹開），窗短則反過來。這是測不準關係，
/// 沒有「都要好」的設定，只能照要看什麼挑。
///
/// hopSize 是相鄰兩格的間隔（樣本數），<= 0 時用 windowSize/4（75% 重疊）。
/// 不重疊（hopSize = windowSize）的話，剛好落在窗兩端的事件會被窗壓掉，
/// 畫出來會有週期性的明暗條紋。
///
/// decibels = true 時取 20·log10，並把下限夾在峰值以下 dynamicRangeDb。
/// 不轉 dB 的話幾乎看不到東西：聲音的能量跨好幾個數量級，
/// 線性色階會讓除了最強的那一塊之外全部變成同一個顏色。
Spectrogram spectrogram(const QVector<double>& samples, double sampleRate, int windowSize,
                        int hopSize = 0, WindowFunction w = WindowFunction::Hann,
                        bool decibels = true, double dynamicRangeDb = 80);

}  // namespace qtplot

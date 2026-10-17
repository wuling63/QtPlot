// Finance.h — 財金圖：蠟燭圖、OHLC 棒圖，以及它們需要的「跳過非交易日」軸。
//
// 金融資料有個直角座標圖沒有的問題：時間軸上有洞。週末與假日沒有交易，
// 如果把 X 直接設成時間戳，K 線之間就會出現一條一條的空白，週線圖尤其明顯。
// 業界的做法是 X 用「第幾根」當座標（連續的 0, 1, 2…），刻度再換回真正的日期 ——
// 這就是 IndexedDateTickGenerator 的用途。
#pragma once

#include "Plottable.h"

#include <QDateTime>

namespace qtplot {

/// 一根 K 線。
struct Ohlc {
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
    /// Unix 秒。用序號當 X 時這個欄位只拿來產生刻度標籤。
    double timestamp = 0;

    bool isRising() const { return close >= open; }
    bool isValid() const {
        return std::isfinite(open) && std::isfinite(high) && std::isfinite(low) &&
               std::isfinite(close);
    }
};

/// 把「序號軸」標成日期。
///
/// timestamps[i] 是第 i 根 K 線的時間，軸上的座標 i 會被標成那個日期。
/// 刻度只會落在實際有資料的序號上，所以不會冒出週末那天的標籤。
class IndexedDateTickGenerator : public TickGenerator {
public:
    QVector<double> timestamps;  ///< 每個序號對應的 Unix 秒
    QString format;              ///< 空字串代表依可視跨度自動挑格式
    /// 只在「日期跳一級」的地方放刻度（換月、換年），而不是等距放。
    /// 日線圖開著比較好讀；分鐘線建議關掉。
    bool breakOnBoundary = true;

    QVector<Tick> generate(const CoordinateRange& range, double lengthPx,
                           const FontStyle& font, bool vertical) override;
};

/// 蠟燭圖 / OHLC 棒圖。
class Candlestick : public IPlottable {
public:
    QVector<Ohlc> bars;

    enum class Style {
        Candle,  ///< 蠟燭：實體 + 上下影線
        Ohlc     ///< 美式棒線：一條直線加左右兩個橫撇
    };
    Style style = Style::Candle;

    /// X 用序號（0, 1, 2…）而不是 timestamp。預設開著 —— 關掉就會出現週末的空白。
    bool useIndexX = true;

    /// 一根佔一格的多少比例。
    double barWidthFraction = 0.75;
    /// 一根 K 線代表多少秒（只在 useIndexX = false 時有用）。
    /// 0 代表用頭尾時間除以根數估 —— 資料有缺口（停牌、週末）時估出來會偏寬，要自己指定。
    double barDuration = 0;

    /// 漲跌配色。預設用歐美慣例（綠漲紅跌）；台股習慣相反，
    /// 交換這兩個顏色即可。
    QColor risingColor{38, 166, 154};
    QColor fallingColor{239, 83, 80};
    /// 上漲的蠟燭畫成空心（只有外框）。日本的傳統畫法。
    bool hollowRising = false;
    /// 影線與邊框的線寬。顏色跟著漲跌色走。
    double lineWidth = 1.0;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 取出收盤價序列（丟給移動平均之類的計算用）。
    QVector<double> closes() const;
    /// 每根 K 線的 X 座標（依 useIndexX 決定是序號還是時間戳）。
    QVector<double> xs() const;
    /// 每根 K 線的時間戳，給 IndexedDateTickGenerator 用。
    QVector<double> timestamps() const;
};

/// 成交量長條。顏色跟著當根 K 線的漲跌。
/// 通常綁在另一根 Y 軸上，或是另外開一張圖。
class Volume : public IPlottable {
public:
    QVector<Ohlc> bars;
    bool useIndexX = true;
    double barWidthFraction = 0.75;
    /// 一根代表多少秒（useIndexX = false 時）。0 代表自動估。
    double barDuration = 0;
    QColor risingColor{38, 166, 154, 140};
    QColor fallingColor{239, 83, 80, 140};

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};

/// 把 K 線合併成較長的週期（例如日線 → 週線、1 分線 → 5 分線）。
///
/// 以 timestamp 除以 seconds 分組：開盤取第一根、收盤取最後一根、
/// 高低取極值、量相加。輸入必須依時間排序。
QVector<Ohlc> resampleOhlc(const QVector<Ohlc>& bars, double seconds);
/// 每 count 根合成一根（不看時間，適合序號軸）。
QVector<Ohlc> resampleOhlcByCount(const QVector<Ohlc>& bars, int count);

// ───────────────────────── 技術指標 ─────────────────────────

/// 簡單移動平均。回傳長度與輸入相同，前 period-1 個是 NaN
/// （長度對齊才能直接和原序列共用同一組 X；NaN 會讓線從第 period 根才開始畫）。
QVector<double> simpleMovingAverage(const QVector<double>& values, int period);

/// 指數移動平均。前 period-1 個同樣是 NaN。
QVector<double> exponentialMovingAverage(const QVector<double>& values, int period);

/// 布林通道。
struct BollingerBands {
    QVector<double> middle;  ///< 移動平均
    QVector<double> upper;   ///< middle + k × 移動標準差
    QVector<double> lower;
};
BollingerBands bollingerBands(const QVector<double>& values, int period = 20, double k = 2.0);

// ───────────────────────── 測試資料 ─────────────────────────

/// 產生一串看起來像真的的日線資料，並自動跳過週六日。
/// 固定種子，所以同一組參數每次產生的結果都一樣。
QVector<Ohlc> randomOhlcSeries(int count, double startPrice = 100.0,
                               const QDate& startDate = QDate(2026, 1, 2),
                               double volatility = 0.02);

}  // namespace qtplot

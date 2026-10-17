// Finance.cpp — 財金圖的範例：蠟燭圖、OHLC 棒線、技術指標、成交量。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

QTPLOT_RECIPE("財金", "K 線", "蠟燭圖",
              "addCandlestick 會做兩件事：X 用序號而不是時間戳，"
              "並把底軸換成 IndexedDateTickGenerator 把序號標回日期。"
              "刻度只落在換月的位置，不是等距亂切。") {
    plot.addCandlestick(randomOhlcSeries(120, 100));
    plot.title.text = QStringLiteral("日線");
    plot.leftAxis()->label.text = QStringLiteral("價格");
}

QTPLOT_RECIPE("財金", "K 線", "為什麼 X 要用序號",
              "上面那張把 useIndexX 關掉、改用真正的時間戳當 X。"
              "每個週末都會留下一段空白，K 線被切成一叢一叢的 —— "
              "這就是金融圖表一律用序號當 X 的原因。") {
    const QVector<Ohlc> bars = randomOhlcSeries(40, 100);
    auto* c = plot.emplace<Candlestick>();
    c->bars = bars;
    c->useIndexX = false;  // 改用時間戳
    plot.bottomAxis()->tickGenerator = std::make_shared<DateTimeTickGenerator>();
    plot.title.text = QStringLiteral("用時間戳當 X：週末會留白");
}

QTPLOT_RECIPE("財金", "K 線", "OHLC 美式棒線",
              "一條直線表示高低，左撇是開盤、右撇是收盤。"
              "比蠟燭省空間，根數多的時候比較不糊。") {
    plot.addOhlc(randomOhlcSeries(90, 100));
    plot.title.text = QStringLiteral("OHLC 棒線");
}

QTPLOT_RECIPE("財金", "K 線", "漲跌配色",
              "預設是歐美慣例（綠漲紅跌）。台股、陸股習慣相反，"
              "把兩個顏色交換即可。hollowRising 可以畫成日本傳統的空心陽線。") {
    const QVector<Ohlc> bars = randomOhlcSeries(60, 100);

    auto* west = plot.addCandlestick(bars);
    west->legendText = QStringLiteral("歐美：綠漲紅跌");

    // 同一份資料往右移，用台股配色再畫一次
    QVector<Ohlc> shifted = bars;
    auto* tw = plot.emplace<Candlestick>();
    tw->bars = shifted;
    tw->risingColor = QColor(214, 39, 40);    // 紅漲
    tw->fallingColor = QColor(44, 160, 44);   // 綠跌
    tw->hollowRising = true;
    tw->legendText = QStringLiteral("台股：紅漲綠跌（空心陽線）");
    // 畫在下方，兩種配色才比得出來
    for (Ohlc& b : tw->bars) {
        b.open -= 18;
        b.high -= 18;
        b.low -= 18;
        b.close -= 18;
    }
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("財金", "技術指標", "多週期移動平均",
              "同時疊幾條不同週期的 SMA 是最常見的用法：短週期貼著價格走、"
              "長週期平滑但反應慢，兩條交叉的位置就是常被拿來看的訊號。"
              "週期越長，開頭的 NaN 越多，所以三條線起點不同。") {
    const QVector<Ohlc> bars = randomOhlcSeries(160, 100);
    auto* candles = plot.addCandlestick(bars);
    const QVector<double> xs = candles->xs();
    const QVector<double> closes = candles->closes();

    const QVector<int> periods{3, 8, 20};
    const Palette palette(Palette::Category10);
    for (int i = 0; i < periods.size(); ++i) {
        auto* s = plot.addScatter(xs, simpleMovingAverage(closes, periods[i]),
                                  palette.color(i));
        s->marker = MarkerStyle::none();
        s->line.width = 1.8;
        s->legendText = QStringLiteral("SMA(%1)").arg(periods[i]);
    }

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("財金", "技術指標", "移動平均線",
              "simpleMovingAverage 與 exponentialMovingAverage 回傳的長度和輸入相同，"
              "前 period-1 個是 NaN —— 長度對齊才能直接共用同一組 X，"
              "而 NaN 會讓線從第 period 根才開始畫，不會從 0 拉一條假的線過來。") {
    const QVector<Ohlc> bars = randomOhlcSeries(160, 100);
    auto* candles = plot.addCandlestick(bars);
    const QVector<double> xs = candles->xs();
    const QVector<double> closes = candles->closes();

    auto* sma = plot.addScatter(xs, simpleMovingAverage(closes, 20), QColor(31, 119, 180));
    sma->marker = MarkerStyle::none();
    sma->line.width = 1.8;
    sma->legendText = QStringLiteral("SMA(20)");

    auto* ema = plot.addScatter(xs, exponentialMovingAverage(closes, 20), QColor(255, 127, 14));
    ema->marker = MarkerStyle::none();
    ema->line.width = 1.8;
    ema->legendText = QStringLiteral("EMA(20)");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("財金", "技術指標", "布林通道",
              "中線是 20 期移動平均，上下軌是 ±2 倍移動標準差。"
              "標準差用母體公式（分母 n）—— 這是布林通道的慣例，"
              "用樣本公式（n-1）會和看盤軟體的數字對不起來。") {
    const QVector<Ohlc> bars = randomOhlcSeries(160, 100);
    auto* candles = plot.addCandlestick(bars);
    const QVector<double> xs = candles->xs();
    const BollingerBands bb = bollingerBands(candles->closes(), 20, 2.0);

    auto* band = plot.addFillY(xs, bb.lower, bb.upper, QColor(100, 120, 200));
    band->legendText = QStringLiteral("±2σ");
    auto* mid = plot.addScatter(xs, bb.middle, QColor(60, 80, 180));
    mid->marker = MarkerStyle::none();
    mid->legendText = QStringLiteral("SMA(20)");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("財金", "技術指標", "價格與成交量共圖",
              "成交量綁在右軸，並把右軸的範圍放大到價格區的三倍，"
              "成交量就會被壓在下面三分之一，不會蓋住 K 線。"
              "這是在沒有多子圖的情況下最簡單的做法。") {
    const QVector<Ohlc> bars = randomOhlcSeries(120, 100);
    plot.addCandlestick(bars);

    auto* volAxis = plot.addAxis(Edge::Right);
    volAxis->label.text = QStringLiteral("成交量");
    volAxis->showTickLabels = true;
    volAxis->majorTickLength = 5;

    auto* vol = plot.addVolume(bars);
    vol->yAxis = volAxis;

    // 成交量的最大值 × 3 當右軸上限 → 長條只佔下面三分之一
    double maxVol = 0;
    for (const Ohlc& b : bars) maxVol = std::max(maxVol, b.volume);
    volAxis->setRange(0, maxVol * 3);

    plot.leftAxis()->label.text = QStringLiteral("價格");
    plot.title.text = QStringLiteral("價格與成交量");
}

QTPLOT_RECIPE("財金", "技術指標", "標示買賣點",
              "K 線上疊標記與參考線：用 Markers 標進出場，AxisSpan 標持有區間。") {
    const QVector<Ohlc> bars = randomOhlcSeries(100, 100);
    auto* candles = plot.addCandlestick(bars);
    const QVector<double> closes = candles->closes();

    const int buyIndex = 22;
    const int sellIndex = 68;

    plot.addVerticalSpan(buyIndex, sellIndex, QColor(80, 140, 220))->legendText =
        QStringLiteral("持有期間");

    auto* buy = plot.addMarkers({double(buyIndex)}, {bars[buyIndex].low * 0.985},
                                QColor(44, 160, 44));
    buy->marker.shape = MarkerShape::FilledTriangleUp;
    buy->marker.size = 14;
    buy->legendText = QStringLiteral("買進");

    auto* sell = plot.addMarkers({double(sellIndex)}, {bars[sellIndex].high * 1.015},
                                 QColor(214, 39, 40));
    sell->marker.shape = MarkerShape::FilledTriangleDown;
    sell->marker.size = 14;
    sell->legendText = QStringLiteral("賣出");

    const double ret = (bars[sellIndex].close / bars[buyIndex].close - 1) * 100;
    plot.addAnnotation(QStringLiteral("報酬率 %1%").arg(ret, 0, 'f', 2),
                       Qt::AlignRight | Qt::AlignTop);
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

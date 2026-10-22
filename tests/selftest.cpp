// selftest.cpp — 不開視窗的自我檢查。
//
// 圖畫出來對不對要靠眼睛看（`QtPlotDemo --export`），但刻度位置、統計量、
// 技術指標這些是純粹的數值，應該用數字驗。看圖只能發現「很離譜」的錯，
// 像是小數位數少一位、標準差用錯分母這種，看圖是看不出來的。
//
// 用法: qtplot_selftest        全部通過回傳 0，否則回傳失敗數
#include "qtplot/Annotations.h"
#include "qtplot/Business.h"
#include "qtplot/Chart3D.h"
#include "qtplot/DataGen.h"
#include "qtplot/Decimation.h"
#include "qtplot/Field.h"
#include "qtplot/Flow.h"
#include "qtplot/Specialty.h"
#include "qtplot/Finance.h"
#include "qtplot/Plot.h"
#include "qtplot/Polar.h"
#include "qtplot/Multiplot.h"
#include "qtplot/Multivariate.h"
#include "qtplot/Sunburst.h"
#include "qtplot/Analysis.h"
#include "qtplot/Spectral.h"
#include "qtplot/Statistics.h"

#include <QGuiApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QTimeZone>
#include <QTextStream>

#include <cmath>
#include <random>
#include <atomic>
#include <thread>
#include <functional>

using namespace qtplot;

namespace {

int g_failures = 0;
QTextStream& out() {
    static QTextStream s(stdout);
    return s;
}

void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) {
        out() << "  ok   " << what << "\n";
    } else {
        ++g_failures;
        out() << "  FAIL " << what;
        if (!detail.isEmpty()) out() << "   (" << detail << ")";
        out() << "\n";
    }
}

void checkClose(double actual, double expected, double tol, const QString& what) {
    const bool ok = std::abs(actual - expected) <= tol;
    check(ok, what,
          ok ? QString() : QStringLiteral("得到 %1，應為 %2").arg(actual, 0, 'g', 10)
                               .arg(expected, 0, 'g', 10));
}

FontStyle tickFont() {
    FontStyle f;
    f.size = 9.5;
    return f;
}

// ───────────────────── 自動刻度 ─────────────────────

void testAutomaticTicks() {
    out() << "自動刻度\n";
    AutomaticTickGenerator gen;

    auto majors = [&](CoordinateRange r, double px) {
        QVector<double> v;
        for (const Tick& t : gen.generate(r, px, tickFont(), false))
            if (t.isMajor) v.append(t.position);
        return v;
    };

    const QVector<double> a = majors({0, 10}, 700);
    check(!a.isEmpty() && a.size() <= 12, QStringLiteral("0..10 的主刻度數量合理"),
          QStringLiteral("得到 %1 個").arg(a.size()));
    if (a.size() >= 2) {
        const double step = a[1] - a[0];
        const double mant = step / std::pow(10.0, std::floor(std::log10(step)));
        check(std::abs(mant - 1) < 1e-9 || std::abs(mant - 2) < 1e-9 || std::abs(mant - 5) < 1e-9,
              QStringLiteral("間距是 1/2/5 × 10^n"), QStringLiteral("間距 %1").arg(step));
    }

    // 所有刻度都必須落在範圍內
    bool inRange = true;
    for (double v : majors({-3.3, 7.7}, 700)) inRange = inRange && v >= -3.3 && v <= 7.7;
    check(inRange, QStringLiteral("刻度不會超出軸的範圍"));

    // 間距 0.25 要印兩位小數，不能四捨五入成 0.3
    check(formatTickLabel(0.25, 0.25) == QStringLiteral("0.25"),
          QStringLiteral("formatTickLabel(0.25, 間距 0.25)"),
          formatTickLabel(0.25, 0.25));
    check(formatTickLabel(0.2, 0.2) == QStringLiteral("0.20") ||
              formatTickLabel(0.2, 0.2) == QStringLiteral("0.2"),
          QStringLiteral("formatTickLabel(0.2, 間距 0.2)"), formatTickLabel(0.2, 0.2));
    // 同一根軸的格式要一致：大值用科學記號時小值也要
    check(formatTickLabel(900000, 100000, 1000000).contains(QLatin1Char('e')),
          QStringLiteral("軸的量級到 1e6 時，900000 也用科學記號"),
          formatTickLabel(900000, 100000, 1000000));
}

// ───────────────────── 日期時間刻度 ─────────────────────

void testDateTimeTicks() {
    out() << "日期時間刻度\n";
    DateTimeTickGenerator gen;

    // 約兩個月的跨度，700px 寬 —— 應該給出數根刻度，而且標籤不能重複
    const double t0 = QDateTime(QDate(2026, 1, 2), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
    const double t1 = QDateTime(QDate(2026, 2, 26), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
    const QVector<Tick> ticks = gen.generate({t0, t1}, 700, tickFont(), false);

    check(ticks.size() >= 4, QStringLiteral("兩個月跨度至少給 4 根刻度"),
          QStringLiteral("只有 %1 根").arg(ticks.size()));

    QStringList labels;
    for (const Tick& t : ticks) labels.append(t.label);
    check(labels.size() == QSet<QString>(labels.begin(), labels.end()).size(),
          QStringLiteral("刻度標籤不重複"), labels.join(QStringLiteral(", ")));
    out() << "       標籤: " << labels.join(QStringLiteral(" | ")) << "\n";
}

// ───────────────────── 序號日期刻度 ─────────────────────

void testIndexedDateTicks() {
    out() << "序號日期刻度\n";
    const QVector<Ohlc> bars = randomOhlcSeries(120, 100);

    IndexedDateTickGenerator gen;
    for (const Ohlc& b : bars) gen.timestamps.append(b.timestamp);

    const QVector<Tick> ticks = gen.generate({0, 119.0}, 700, tickFont(), false);
    check(!ticks.isEmpty(), QStringLiteral("有產生刻度"));

    bool onData = true;
    for (const Tick& t : ticks)
        onData = onData && t.position >= 0 && t.position <= 119 &&
                 std::abs(t.position - std::round(t.position)) < 1e-9;
    check(onData, QStringLiteral("刻度都落在實際有資料的序號上"));

    // 每根 K 線都必須是平日
    bool weekdaysOnly = true;
    for (const Ohlc& b : bars) {
        const int dow = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(b.timestamp),
                                                      QTimeZone::UTC).date().dayOfWeek();
        weekdaysOnly = weekdaysOnly && dow <= 5;
    }
    check(weekdaysOnly, QStringLiteral("測試資料不含週六日"));

    QStringList labels;
    for (const Tick& t : ticks) labels.append(t.label);
    check(labels.size() == QSet<QString>(labels.begin(), labels.end()).size(),
          QStringLiteral("刻度標籤不重複"), labels.join(QStringLiteral(", ")));
    out() << "       標籤: " << labels.join(QStringLiteral(" | ")) << "\n";
}

// ───────────────────── 統計 ─────────────────────

void testStatistics() {
    out() << "統計\n";
    // 1..10 的四分位數（線性內插法 / numpy 預設）：q1 = 3.25, median = 5.5, q3 = 7.75
    QVector<double> v;
    for (int i = 1; i <= 10; ++i) v.append(i);
    const BoxStats s = BoxStats::fromValues(v, 1.5);
    checkClose(s.q1, 3.25, 1e-9, QStringLiteral("q1(1..10) = 3.25"));
    checkClose(s.median, 5.5, 1e-9, QStringLiteral("median(1..10) = 5.5"));
    checkClose(s.q3, 7.75, 1e-9, QStringLiteral("q3(1..10) = 7.75"));
    checkClose(s.mean, 5.5, 1e-9, QStringLiteral("mean(1..10) = 5.5"));
    // 樣本標準差（分母 n-1）
    checkClose(s.stdDev, 3.02765035409749, 1e-9, QStringLiteral("stdDev 用 n-1"));
    check(s.outliers.isEmpty(), QStringLiteral("1..10 沒有離群值"));

    // 加一個極端值，必須被判為離群
    QVector<double> w = v;
    w.append(100.0);
    const BoxStats s2 = BoxStats::fromValues(w, 1.5);
    check(s2.outliers.size() == 1 && std::abs(s2.outliers.first() - 100.0) < 1e-9,
          QStringLiteral("100 被判為離群值"),
          QStringLiteral("離群值 %1 個").arg(s2.outliers.size()));
    check(s2.max <= 10.0, QStringLiteral("上鬚停在離群值之前的資料點"),
          QStringLiteral("max = %1").arg(s2.max));

    // 完美線性資料的迴歸
    QVector<double> xs, ys;
    for (int i = 0; i < 20; ++i) {
        xs.append(i);
        ys.append(3.0 * i + 7.0);
    }
    const LinearFit fit = linearFit(xs, ys);
    checkClose(fit.slope, 3.0, 1e-9, QStringLiteral("斜率 = 3"));
    checkClose(fit.intercept, 7.0, 1e-9, QStringLiteral("截距 = 7"));
    checkClose(fit.rSquared, 1.0, 1e-9, QStringLiteral("R² = 1"));

    // 核密度：對 x 的積分應該是 1
    const QVector<double> data = {1, 2, 2, 3, 3, 3, 4, 4, 5};
    QVector<double> grid;
    const double lo = -6, hi = 12, n = 2000;
    for (int i = 0; i <= n; ++i) grid.append(lo + (hi - lo) * i / n);
    const QVector<double> dens = kernelDensity(data, grid);
    double area = 0;
    for (int i = 1; i < dens.size(); ++i)
        area += (dens[i] + dens[i - 1]) / 2 * (grid[i] - grid[i - 1]);
    checkClose(area, 1.0, 1e-3, QStringLiteral("核密度對 x 的積分 = 1"));
}

// ───────────────────── 技術指標 ─────────────────────

void testIndicators() {
    out() << "技術指標\n";
    QVector<double> v;
    for (int i = 1; i <= 10; ++i) v.append(i);

    const QVector<double> sma = simpleMovingAverage(v, 3);
    check(sma.size() == v.size(), QStringLiteral("SMA 長度與輸入相同"));
    check(std::isnan(sma[0]) && std::isnan(sma[1]), QStringLiteral("SMA 前 period-1 個是 NaN"));
    checkClose(sma[2], 2.0, 1e-9, QStringLiteral("SMA(3) 第 3 個 = 2"));
    checkClose(sma[9], 9.0, 1e-9, QStringLiteral("SMA(3) 最後一個 = 9"));

    const QVector<double> ema = exponentialMovingAverage(v, 3);
    check(std::isnan(ema[1]) && !std::isnan(ema[2]), QStringLiteral("EMA 從第 period 個開始"));
    checkClose(ema[2], 2.0, 1e-9, QStringLiteral("EMA 的種子是前 period 個的簡單平均"));

    // 定值序列的布林通道寬度應該是 0
    const QVector<double> flat(30, 5.0);
    const BollingerBands bb = bollingerBands(flat, 20, 2.0);
    checkClose(bb.upper[25] - bb.lower[25], 0.0, 1e-9, QStringLiteral("定值序列的通道寬度 = 0"));

    // 已知序列的母體標準差（分母 n，布林通道的慣例）
    QVector<double> seq;
    for (int i = 0; i < 5; ++i) seq.append(i);  // 0..4，母體 σ = sqrt(2)
    const BollingerBands bb2 = bollingerBands(seq, 5, 1.0);
    checkClose(bb2.upper[4] - bb2.middle[4], std::sqrt(2.0), 1e-9,
               QStringLiteral("布林通道用母體標準差（分母 n）"));
}

// ───────────────────── 座標換算 ─────────────────────

void testTransform() {
    out() << "座標換算\n";
    Transform t;
    t.rect = PixelRect(100, 500, 50, 450);  // left, right, top, bottom
    t.xr = {0, 10};
    t.yr = {-5, 5};

    checkClose(t.px(0), 100, 1e-9, QStringLiteral("x 最小值對到左緣"));
    checkClose(t.px(10), 500, 1e-9, QStringLiteral("x 最大值對到右緣"));
    // 資料 y 向上、像素 y 向下，所以最大值對到「上緣」= 較小的像素 y
    checkClose(t.py(5), 50, 1e-9, QStringLiteral("y 最大值對到上緣（像素 y 較小）"));
    checkClose(t.py(-5), 450, 1e-9, QStringLiteral("y 最小值對到下緣"));
    checkClose(t.cx(t.px(3.7)), 3.7, 1e-9, QStringLiteral("x 換算可逆"));
    checkClose(t.cy(t.py(-1.3)), -1.3, 1e-9, QStringLiteral("y 換算可逆"));
}

// ───────────────────── 自動範圍 ─────────────────────

/// 重設：在同一個 Plot 上連續畫不同的東西，前一張的狀態不可以殘留。
///
/// 這一項是補的 —— 原本的視覺回歸抓不到：--export 每張圖都開一個新的
/// PlotWidget，永遠是乾淨的。只有 GUI 會在同一個 Plot 上反覆換圖，
/// 於是 addAxis() 加出來的軸一張一張疊上去，刻度愈來愈多。
/// 氣泡標註的尾巴幾何。
///
/// 這一項存在的理由：尾巴算錯的時候，圖上只是「看起來怪怪的」，
/// 肉眼判斷不出根部寬了幾個像素 —— 寫這段時我就先看圖誤判過一次，
/// 把「尾巴下緣和方框底邊幾乎共線」看成跑錯邊的線。這種事要用數字驗。
/// 扇形／圓弧的資料範圍，以及極座標軸的角度換算。
/// 格線的交替填色與刻度標籤旋轉。
/// 立體長條圖的資料範圍。
///
/// 擠出是以像素計的，axisLimits() 算不出精確的餘裕，只能用比例估。
/// 這幾項釘住的是那個估計值怎麼算，以及別把長條的寬度算丟了。
/// 商務圖表：瀑布的累計、漏斗的範圍、靶心的範圍。
/// 旭日圖的數值彙總與層數，以及圓餅標籤的推開演算法。
/// 反向軸：刻度與資料必須走同一套換算。
///
/// 這是甘特圖那支範例踩出來的 bug：Axis::getPixel（畫刻度）有處理 inverted，
/// Transform（畫資料）沒有，於是長條和軸標籤上下顛倒。
/// 看圖很難判斷是「資料錯了」還是「刻度錯了」，所以用數字把兩邊釘在一起。
void testPerformancePaths() {
    out() << "效能路徑（索引、包絡線、蓋章、執行緒）\n";

    // ── 最小／最大值索引和直接掃的結果一樣 ──
    {
        std::mt19937 rng(7);
        std::uniform_real_distribution<double> d(-100, 100);
        QVector<double> v(200'003);
        for (double& x : v) x = d(rng);
        for (int i = 0; i < 300; ++i) v[(i * 7919) % v.size()] = qQNaN();  // 混一些 NaN
        MinMaxIndex idx;
        idx.build(v.constData(), v.size());
        std::uniform_int_distribution<qsizetype> pick(0, v.size());
        int bad = 0;
        for (int k = 0; k < 2000; ++k) {
            qsizetype a = pick(rng), b = pick(rng);
            if (a > b) std::swap(a, b);
            double l1, h1, l2, h2;
            const bool o1 = idx.query(v.constData(), a, b, l1, h1);
            const bool o2 = scanMinMax(v.constData(), a, b, l2, h2);
            if (o1 != o2 || (o1 && (l1 != l2 || h1 != h2))) ++bad;
        }
        check(bad == 0, "索引查詢和直接掃一致（2000 段隨機區間，含 NaN）", QString::number(bad));
        double lo, hi;
        check(!idx.query(v.constData(), 5, 5, lo, hi), "空區間回傳 false");
        QVector<double> allNan(5000, qQNaN());
        MinMaxIndex idx2;
        idx2.build(allNan.constData(), allNan.size());
        check(!idx2.query(allNan.constData(), 0, allNan.size(), lo, hi), "全是 NaN 回傳 false");
    }

    // ── 使用者改了 ys，索引要跟著重建 ──
    {
        Plot plot;
        QVector<double> ys(100'000, 0.0);
        Signal* s = plot.addSignal(ys);
        plot.autoScale(0, true);
        checkClose(plot.leftAxis()->range.max, 0.5, 1e-9, "全 0 的訊號（ensureArea 撐開成 ±0.5）");
        s->ys[54321] = 1000;  // 就地改一個點
        plot.autoScale(0, true);
        checkClose(plot.leftAxis()->range.max, 1000, 1e-9, "就地改 ys[i] 之後，索引重建，範圍跟著變");
        s->ys = QVector<double>(200'000, -5.0);  // 整個換掉
        plot.autoScale(0, true);
        checkClose(plot.leftAxis()->range.min, -5.5, 1e-9, "整個換掉 ys 之後也對");
    }

    // ── 包絡線：一百萬點裡的單一尖峰不能被抽樣吃掉 ──
    {
        Plot plot;
        plot.antiAlias = false;
        QVector<double> ys(1'000'000, 0.0);
        ys[777'777] = 100;
        Signal* s = plot.addSignal(ys);
        s->line.color = QColor(255, 0, 0);
        plot.setAxisLimits(0, 1'000'000, -10, 110);
        plot.grid.isVisible = false;
        const QImage img = plot.renderToImage(800, 400);
        const PixelRect d = plot.lastDataRect();
        const int col = static_cast<int>(plot.bottomAxis()->getPixel(777'777, d));
        const int top = static_cast<int>(plot.leftAxis()->getPixel(95, d));
        bool found = false;
        for (int x = col - 2; x <= col + 2; ++x)
            if (qRed(img.pixel(x, top)) > 200 && qGreen(img.pixel(x, top)) < 80) found = true;
        check(found, "百萬點裡的單一尖峰有畫出來");
        // 尖峰以外的地方不該有紅色（包絡線沒有亂伸）
        bool stray = false;
        const int farCol = static_cast<int>(plot.bottomAxis()->getPixel(300'000, d));
        if (qRed(img.pixel(farCol, top)) > 200 && qGreen(img.pixel(farCol, top)) < 80) stray = true;
        check(!stray, "尖峰以外的像素行沒有被拉高");
    }

    // ── 蓋章畫出來的標記和逐一畫路徑的一樣 ──
    {
        QVector<QPointF> pts;
        std::mt19937 rng(11);
        std::uniform_real_distribution<double> ux(10, 390), uy(10, 290);
        for (int i = 0; i < 800; ++i) pts.append(QPointF(std::round(ux(rng)), std::round(uy(rng))));
        const MarkerStyle m(MarkerShape::FilledCircle, 7, QColor(20, 90, 200));
        QImage a(400, 300, QImage::Format_ARGB32_Premultiplied), b = a;
        a.fill(Qt::white);
        b.fill(Qt::white);
        {
            QPainter p(&a);
            p.setRenderHint(QPainter::Antialiasing);
            drawMarkers(p, pts, m);  // 800 點：走蓋章
        }
        {
            QPainter p(&b);
            p.setRenderHint(QPainter::Antialiasing);
            for (int i = 0; i < pts.size(); i += 200) drawMarkers(p, pts.mid(i, 200), m);  // 每批 200：走路徑
        }
        double diff = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x) diff += std::abs(qGray(a.pixel(x, y)) - qGray(b.pixel(x, y)));
        diff /= a.width() * a.height();
        check(diff < 1.5, "蓋章與路徑畫的標記幾乎一樣", QStringLiteral("平均差 %1 灰階").arg(diff, 0, 'f', 3));
    }

    // ── 背景執行緒餵資料，GUI 執行緒同時畫 ──
    {
        Plot plot;
        DataStreamer* s = plot.addDataStreamer(5000);
        DataLogger* lg = plot.addDataLogger();
        std::atomic<bool> stop{false};
        std::atomic<qint64> fed{0};
        QThread* producer = QThread::create([&] {
            double phase = 0;
            while (!stop) {
                QVector<double> chunk(64);
                for (double& v : chunk) v = std::sin(phase += 0.01);
                s->addRange(chunk);
                lg->addRange(chunk);
                fed += chunk.size();
            }
        });
        const quint64 v0 = s->dataVersion();
        producer->start();
        QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
        QElapsedTimer t;
        t.start();
        int frames = 0;
        while (t.elapsed() < 300) {
            QPainter p(&img);
            plot.render(p, QRectF(0, 0, 400, 300));
            ++frames;
        }
        stop = true;
        producer->wait();
        delete producer;
        check(frames > 5 && fed > 0, "邊餵邊畫不會當掉", QStringLiteral("%1 張、%2 點").arg(frames).arg(fed.load()));
        check(s->totalAdded() == fed && lg->count() == fed, "兩邊的筆數都對（沒有掉資料）",
              QStringLiteral("%1 / %2 / %3").arg(s->totalAdded()).arg(lg->count()).arg(fed.load()));
        check(s->dataVersion() > v0, "dataVersion 有前進");
    }
}

void testLegendSymbolStyle() {
    out() << "圖例符號：塗滿與外框\n";
    // 量一個長條圖圖例色塊上的兩點：正中央、靠左緣往內 1 像素
    struct Probe { QColor center, edge; };
    auto probe = [](const std::function<void(Legend&)>& setup) {
        Plot plot;
        plot.legend.isVisible = true;
        plot.legend.background = FillStyle(QColor(255, 255, 255));
        auto* bars = plot.addBars({1, 2}, QColor(200, 0, 0));
        bars->legendText = QStringLiteral("A");
        setup(plot.legend);
        const QImage img = plot.renderToImage(400, 300, 1.0);
        const QRectF row = plot.legendHitAreas().value(0).rect;
        const double symLeft = row.left() + 2;  // 命中範圍比色塊左緣多留 2 像素
        const double symRight = symLeft + plot.legend.symbolWidth - 6;
        const int midY = int(row.center().y());
        return Probe{img.pixelColor(int((symLeft + symRight) / 2), midY), img.pixelColor(int(symLeft + 1), midY)};
    };
    auto near = [](QColor a, QColor b) {
        return std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) + std::abs(a.blue() - b.blue()) < 60;
    };
    const QColor red(200, 0, 0), white(255, 255, 255), blue(0, 0, 255);

    const Probe filled = probe([](Legend&) {});
    check(near(filled.center, red), "預設色塊塗滿", filled.center.name());

    const Probe hollow = probe([](Legend& lg) { lg.fillSymbols = false; });
    check(near(hollow.center, white), "fillSymbols = false：色塊中間透出底色", hollow.center.name());
    // 1.5 像素的框經過反鋸齒，量到的是紅白混色：看紅色明顯壓過綠藍就好
    check(hollow.edge.red() - hollow.edge.green() > 60 && hollow.edge.red() - hollow.edge.blue() > 60,
          "只畫框時框用項目的顏色", hollow.edge.name());

    const Probe outlined = probe([&](Legend& lg) { lg.symbolOutline = LineStyle(blue, 2); });
    check(near(outlined.center, red) && near(outlined.edge, blue), "symbolOutline：塗滿再加指定顏色的框",
          outlined.center.name() + " / " + outlined.edge.name());

    const Probe hollowBlue = probe([&](Legend& lg) {
        lg.fillSymbols = false;
        lg.symbolOutline = LineStyle(blue, 2);
    });
    check(near(hollowBlue.center, white) && near(hollowBlue.edge, blue), "只畫框又指定外框時用指定的顏色",
          hollowBlue.center.name() + " / " + hollowBlue.edge.name());
}

void testInspection() {
    out() << "視覺檢測（混淆矩陣、門檻統計、熱圖遮罩）\n";
    // 2 類：實際 A 有 8 筆（6 對 2 錯）、實際 B 有 4 筆（1 錯 3 對）
    const QVector<QVector<double>> m = {{6, 2}, {1, 3}};
    const QVector<ClassMetrics> cm = classMetrics(m);
    checkClose(cm[0].recall, 6.0 / 8, 1e-12, "A 的召回率 = 6/8");
    checkClose(cm[0].precision, 6.0 / 7, 1e-12, "A 的精確率 = 6/7（預測 A 的那一欄）");
    checkClose(cm[1].f1, 2 * (3.0 / 5) * (3.0 / 4) / (3.0 / 5 + 3.0 / 4), 1e-12, "B 的 F1");
    check(cm[0].support == 8 && cm[1].support == 4, "support 是每列的總和");
    checkClose(accuracy(m), 9.0 / 12, 1e-12, "正確率 = 對角線 / 全部");
    check(classMetrics({{0, 0}, {0, 5}})[0].precision == 0, "沒有樣本時分母為 0 不會除錯，給 0");

    const auto counts = confusionCounts({0, 0, 1, 1, 2, 5}, {0, 1, 1, 1, 0, 0}, 3);
    check(counts[0][0] == 1 && counts[0][1] == 1 && counts[1][1] == 2 && counts[2][0] == 1,
          "confusionCounts 數得對，超出類別數的那筆被略過");

    const ThresholdStats s = thresholdStats({0.1, 0.2, 0.6, 0.3}, {0.7, 0.4, 0.9}, 0.5);
    check(s.tp == 2 && s.fn == 1 && s.tn == 3 && s.fp == 1, "門檻 0.5：抓到 2、漏 1、過殺 1");
    checkClose(s.missRate, 1.0 / 3, 1e-12, "漏檢率 = 漏的 / NG 總數");
    checkClose(s.falseAlarmRate, 1.0 / 4, 1e-12, "誤判率 = 過殺 / OK 總數");
    check(s.counts() == QVector<QVector<double>>{{3, 1}, {1, 2}}, "counts() 的排法：列實際 {OK, NG}、欄預測 {OK, NG}");
    check(thresholdStats({0.5}, {}, 0.5).fp == 1, "剛好等於門檻算 NG");

    ConfusionMatrix c;
    c.counts = m;
    c.normalize = ConfusionMatrix::Normalize::Row;
    checkClose(c.normalized(0, 1), 2.0 / 8, 1e-12, "依列正規化");
    c.normalize = ConfusionMatrix::Normalize::Column;
    checkClose(c.normalized(1, 0), 1.0 / 7, 1e-12, "依欄正規化");
    c.normalize = ConfusionMatrix::Normalize::All;
    checkClose(c.normalized(1, 1), 3.0 / 12, 1e-12, "全體正規化");

    {
        Plot plot;
        plot.addConfusionMatrix({QStringLiteral("A"), QStringLiteral("B")}, m);
        plot.renderToImage(400, 400);
        const QVector<Tick>& ticks = plot.leftAxis()->ticks();
        bool aOnTop = false;
        for (const Tick& t : ticks)
            if (t.label == QStringLiteral("A") && t.position == 1) aOnTop = true;
        check(aOnTop, "第 0 列（A）在左軸最上面");
        check(!plot.grid.isVisible && plot.equalAxisScale, "關掉格線、設成等比例");
    }

    // hideBelow：低於門檻的格子是透明的
    {
        Plot plot;
        plot.style.figureBackground = QColor(255, 255, 255);
        plot.style.dataBackground = QColor(255, 255, 255);
        auto* h = plot.addHeatmap({0.1, 0.9}, 1, 2);
        h->colormap = Colormap(Colormap::Viridis);  // Hot 的最高值是純白，會和底色分不出來
        h->hideBelow = 0.5;
        plot.setAxisLimits(0, 2, 0, 1);
        plot.grid.isVisible = false;
        const QImage img = plot.renderToImage(400, 200);
        const PixelRect d = plot.lastDataRect();
        const QColor left = img.pixelColor(int(d.left + d.width() * 0.25), int((d.top + d.bottom) / 2));
        const QColor right = img.pixelColor(int(d.left + d.width() * 0.75), int((d.top + d.bottom) / 2));
        check(left == QColor(255, 255, 255), "低於 hideBelow 的格子透出底色");
        check(right != QColor(255, 255, 255), "高於門檻的格子照常上色");
    }
}

void testContour3DAndDepthShade() {
    out() << "3D 等值線與依深度調淡\n";
    auto c = std::make_unique<Contour3D>();
    c->xs = gen::linspace(-1, 1, 21);
    c->ys = gen::linspace(-1, 1, 21);
    for (double y : c->ys)
        for (double x : c->xs) c->zs.append(x * x + y * y);  // 0..2 的碗
    c->levelCount = 4;
    const QVector<double> lv = c->effectiveLevels();
    check(lv.size() == 4, "levelCount = 4 給四條");
    checkClose(lv.first(), 0.4, 1e-9, "等值在 Z 範圍內等距、不含兩端（第一條 = 2/5）");
    checkClose(lv.last(), 1.6, 1e-9, "最後一條 = 8/5");
    c->levels = {0.5};
    check(c->effectiveLevels() == QVector<double>{0.5}, "指定 levels 時用指定的");
    CoordinateRange x, y, z;
    c->limits(x, y, z);
    checkClose(z.max, 2, 1e-9, "Z 範圍來自格點值");
    c->colorByLevel = false;
    check(!c->colorRange().isValid(), "不依高度上色時不參與色階");
    c->colorByLevel = true;

    {
        Plot plot;
        auto* chart = plot.addChart3D();
        chart->showBox = false;  // 只看等值線本身
        chart->add(std::move(c));
        const QImage img = plot.renderToImage(400, 300);
        int dark = 0;
        for (int yy = 0; yy < img.height(); ++yy)
            for (int xx = 0; xx < img.width(); ++xx)
                if (qGray(img.pixel(xx, yy)) < 200) ++dark;
        check(dark > 200, "等值線有畫出來", QString::number(dark));
    }

    // 依深度調淡：同一張圖畫兩次，開了之後整體要變淡（遠的點變透明）
    auto inkOf = [](bool shade) {
        Plot plot;
        auto s = std::make_unique<Scatter3D>();
        for (int i = 0; i < 40; ++i) s->points.append({std::cos(i * 0.3), std::sin(i * 0.3), i * 0.1});
        s->color = QColor(0, 0, 0);
        s->edgeWidth = 0;
        s->depthShade = shade;
        auto* chart = plot.addChart3D();
        chart->showBox = false;
        chart->add(std::move(s));
        const QImage img = plot.renderToImage(400, 300);
        long ink = 0;
        for (int yy = 0; yy < img.height(); ++yy)
            for (int xx = 0; xx < img.width(); ++xx) ink += 255 - qGray(img.pixel(xx, yy));
        return ink;
    };
    const long plain = inkOf(false), shaded = inkOf(true);
    check(shaded < plain * 0.9 && shaded > plain * 0.3, "depthShade 讓遠的點變淡（但不是全部消失）",
          QStringLiteral("%1 → %2").arg(plain).arg(shaded));
}

void testTriSurface() {
    out() << "三角網格曲面（TriSurface3D）\n";
    // 莫比烏斯帶：8 × 30 的參數格網
    const int nw = 8, nt = 30;
    auto s = std::make_unique<TriSurface3D>();
    QVector<Coordinates> uv;
    for (int i = 0; i < nt; ++i) {
        const double t = 2 * M_PI * i / (nt - 1);
        for (int j = 0; j < nw; ++j) {
            const double w = -0.25 + 0.5 * j / (nw - 1);
            const double r = 1 + w * std::cos(t / 2);
            s->vertices.append({r * std::cos(t), r * std::sin(t), w * std::sin(t / 2)});
            uv.append({w, t});
        }
    }
    s->triangulateParameters(uv);
    // 矩形格網的 Delaunay 剛好把每一格切成兩個三角形
    check(s->triangles.size() == 2 * (nw - 1) * (nt - 1), "參數格網三角剖分：每格兩個三角形",
          QString::number(s->triangles.size()));
    bool indicesOk = true;
    for (const Triangle& t : s->triangles)
        if (t.a < 0 || t.b < 0 || t.c < 0 || t.a >= s->vertices.size() || t.b >= s->vertices.size() ||
            t.c >= s->vertices.size())
            indicesOk = false;
    check(indicesOk, "三角形的索引都落在頂點範圍內");

    CoordinateRange x, y, z;
    s->limits(x, y, z);
    checkClose(z.max, 0.25, 1e-3, "Z 上界是帶寬的一半");
    checkClose(z.min, -0.25, 1e-3, "Z 下界");
    checkClose(x.max, 1.25, 1e-3, "X 上界 = 1 + 半寬");
    const CoordinateRange c = s->colorRange();
    check(c.isValid() && std::abs(c.max - z.max) < 1e-12, "依高度上色的色階範圍就是 Z 範圍");
    s->fillColor = QColor(200, 0, 0);
    check(!s->colorRange().isValid(), "指定單一顏色時不參與色階");
    s->fillColor = QColor();

    // 壞掉的索引要跳過，不能當掉
    s->triangles.append({0, 1, 99999});
    s->triangles.append({-1, 0, 1});
    Plot plot;
    auto* chart = plot.addChart3D();
    chart->add(std::move(s));
    const QImage img = plot.renderToImage(400, 300);
    int colored = 0;
    for (int yy = 0; yy < img.height(); yy += 4)
        for (int xx = 0; xx < img.width(); xx += 4) {
            const QColor px = img.pixelColor(xx, yy);
            if (px.saturation() > 80) ++colored;  // viridis 的顏色飽和度高，背景與座標盒是灰的
        }
    check(colored > 100, "曲面有畫出來（壞索引被跳過）", QString::number(colored));
}

void testTickStylesAndLogScale() {
    out() << "刻度樣式與對數軸\n";

    {
        // 方向：Inside 時版面不替刻度留位置，Cross 留一半
        Plot plot;
        plot.addScatter({0, 1}, {0, 1});
        plot.renderToImage(400, 300);
        Axis* left = plot.leftAxis();
        left->majorTickLength = 10;
        const double outside = left->measureThickness();
        left->tickDirection = TickDirection::Inside;
        const double inside = left->measureThickness();
        left->tickDirection = TickDirection::Cross;
        const double cross = left->measureThickness();
        checkClose(outside - inside, 10, 1e-9, "Inside 省下整根刻度的厚度");
        checkClose(outside - cross, 5, 1e-9, "Cross 省下一半");
        left->majorTickMarker = MarkerStyle(MarkerShape::FilledCircle, 16, QColor(0, 0, 0));
        check(left->outwardTickLength() == 8, "Cross 的標記往外佔一半大小", QString::number(left->outwardTickLength()));
    }

    // 找繪圖區某一列上，靠近 x 的像素有沒有指定顏色
    auto hasColorNear = [](const QImage& img, int x0, int x1, int y, QColor want) {
        for (int x = std::max(0, x0); x <= std::min(img.width() - 1, x1); ++x)
            for (int dy = -1; dy <= 1; ++dy) {
                const QColor c = img.pixelColor(x, std::clamp(y + dy, 0, img.height() - 1));
                if (std::abs(c.red() - want.red()) + std::abs(c.green() - want.green()) + std::abs(c.blue() - want.blue()) < 90)
                    return true;
            }
        return false;
    };

    {
        // 鏡像刻度往裡畫、tickStyler 把 y = 0 那根改成紅色
        Plot plot;
        plot.grid.isVisible = false;
        plot.setAxisLimits(-1, 1, -1, 1);
        Axis* left = plot.leftAxis();
        left->tickDirection = TickDirection::Inside;
        left->mirrorTicks = true;
        left->majorTickLength = 10;
        left->majorTickLine.width = 2;
        left->tickStyler = [](Tick& t) {
            if (t.isMajor && std::abs(t.position) < 1e-9) {
                t.color = QColor(220, 0, 0);
                t.length = 14;
            }
        };
        const QImage img = plot.renderToImage(400, 300);
        const PixelRect d = plot.lastDataRect();
        const int y0 = int(std::lround(left->getPixel(0, d)));
        check(hasColorNear(img, int(d.left) + 2, int(d.left) + 12, y0, QColor(220, 0, 0)), "tickStyler：y = 0 的刻度往內畫成紅色");
        check(hasColorNear(img, int(d.right) - 12, int(d.right) - 2, y0, QColor(220, 0, 0)), "mirrorTicks：對面那一邊也有");
        bool zeroLabelRed = false;
        for (const Tick& t : left->ticks())
            if (std::abs(t.position) < 1e-9 && t.color == QColor(220, 0, 0) && t.length == 14) zeroLabelRed = true;
        check(zeroLabelRed, "tickStyler 改的是存下來的刻度");
    }

    {
        Axis a(Edge::Left);
        a.setColor(QColor(10, 120, 200));
        check(a.frameLine.color == QColor(10, 120, 200) && a.majorTickLine.color == a.frameLine.color &&
                  a.minorTickLine.color == a.frameLine.color && a.tickFont.color == a.frameLine.color &&
                  a.label.font.color == a.frameLine.color,
              "setColor 一次換掉框線、刻度、文字與軸標題");
    }

    {
        // 對數刻度的文字格式
        LogTickGenerator g;
        check(g.formatLabel(1, 2) == QStringLiteral("100"), "Auto：小的次方印一般數字", g.formatLabel(1, 2));
        check(g.formatLabel(1, 5) == QString::fromUtf8("10⁵"), "Auto：大的次方印上標", g.formatLabel(1, 5));
        check(g.formatLabel(1, -4) == QString::fromUtf8("10⁻⁴"), "負的次方有上標減號", g.formatLabel(1, -4));
        g.labelFormat = LogTickGenerator::LabelFormat::Plain;
        check(g.formatLabel(1, 5) == QStringLiteral("100000"), "Plain", g.formatLabel(1, 5));
        g.labelFormat = LogTickGenerator::LabelFormat::ENotation;
        check(g.formatLabel(1, 5) == QStringLiteral("1e5") && g.formatLabel(2, -3) == QStringLiteral("2e-3"), "ENotation",
              g.formatLabel(2, -3));
        g.labelFormat = LogTickGenerator::LabelFormat::Superscript;
        check(g.formatLabel(2, 3) == QString::fromUtf8("2×10³"), "次刻度的上標寫法", g.formatLabel(2, 3));
        g.base = 2;
        check(g.formatLabel(1, 5) == QString::fromUtf8("2⁵"), "底數 2", g.formatLabel(1, 5));

        LogTickGenerator h;
        int labeledMinor = 0;
        for (const Tick& t : h.generate({0, 4}, 400, FontStyle(), true))
            if (!t.isMajor && !t.label.isEmpty()) ++labeledMinor;
        check(labeledMinor == 0, "範圍夠寬時次刻度不印字");
        for (const Tick& t : h.generate({0.2, 0.9}, 400, FontStyle(), true))
            if (!t.isMajor && !t.label.isEmpty()) ++labeledMinor;
        check(labeledMinor > 3, "範圍不到一個十的次方時次刻度自動印字", QString::number(labeledMinor));
    }

    {
        // 真正的對數軸
        Plot plot;
        QVector<double> xs, ys;
        for (int i = 0; i <= 40; ++i) {
            xs.append(i);
            ys.append(std::pow(10.0, i / 10.0));  // 1 .. 10000
        }
        auto* sc = plot.addScatter(xs, ys);
        Axis* left = plot.leftAxis();
        left->logScale = true;
        plot.autoScale();
        check(left->range.min > 0 && left->range.min < 1 && left->range.max > 10000, "自動範圍在對數空間留邊界",
              QStringLiteral("%1 ～ %2").arg(left->range.min).arg(left->range.max));
        plot.renderToImage(400, 300);
        const PixelRect d = plot.lastDataRect();
        const double p1 = left->getPixel(1, d), p10 = left->getPixel(10, d), p100 = left->getPixel(100, d);
        checkClose(p1 - p10, p10 - p100, 1e-6, "每個十的次方一樣長");
        checkClose(left->getCoordinate(p10, d), 10, 1e-9, "getCoordinate 是 getPixel 的反函數");
        const Transform t = plot.transformFor(nullptr, nullptr);
        checkClose(t.py(100), p100, 1e-9, "資料（Transform）和刻度（Axis）用同一套換算");
        checkClose(t.cy(p100), 100, 1e-9, "Transform 的反換算");
        RenderPack rp;
        rp.dataRect = d;
        checkClose(sc->transform(rp).py(100), p100, 1e-9, "圖元自己的 transform() 也走對數換算");
        bool decades = true;
        int majors = 0;
        for (const Tick& tk : left->ticks())
            if (tk.isMajor) {
                ++majors;
                const double e = std::log10(tk.position);
                if (std::abs(e - std::round(e)) > 1e-9) decades = false;
            }
        check(decades && majors >= 5, "主刻度落在 1、10、100…（原始數值）", QString::number(majors));
        check(std::isfinite(t.py(0)) && t.py(0) > d.bottom, "0 與負值畫到繪圖區外面，不是 NaN");

        // 平移：對數空間平移，兩端的比值不變
        const double ratio = left->range.max / left->range.min;
        left->pan(p10, p100, d);
        checkClose(left->range.max / left->range.min, ratio, 1e-6, "平移之後兩端的比值不變");
        // 縮放：定點的像素位置不動
        plot.renderToImage(400, 300);
        const double before = left->getPixel(50, d);
        left->zoom(0.5, 50);
        checkClose(left->getPixel(50, d), before, 1e-6, "縮放時定點不動");
        checkClose(std::log10(left->range.max / left->range.min), std::log10(ratio) / 2, 1e-6, "zoom(0.5) 剩一半的十的次方");

        // 資料含 0（長條圖從 0 開始）：自動範圍只看正的部分
        Plot bars;
        bars.addBars({0, 5, 50, 500});
        bars.leftAxis()->logScale = true;
        bars.autoScale();
        check(bars.leftAxis()->range.min > 0, "資料含 0 時自動範圍仍然是正的", QString::number(bars.leftAxis()->range.min));
        bars.renderToImage(400, 300);  // 不能當掉
        check(Axis::positiveRange({-5, -1}) == CoordinateRange(1, 10), "全是負的時給 1～10");
    }
}

void testAxisBreaks() {
    out() << "截斷軸\n";

    Plot plot;
    plot.setAxisLimits(0, 10, 0, 1010);
    Axis* y = plot.leftAxis();
    y->addBreak(10, 1000);
    plot.renderToImage(600, 400, 1.0);
    const PixelRect d = plot.lastDataRect();
    const double gap = y->breakGapPx();

    // 截掉 990 之後只剩 20 個單位，扣掉空隙平均分配：兩段一樣高
    const double lowTop = y->getPixel(10, d), highBottom = y->getPixel(1000, d);
    checkClose(y->getPixel(0, d), d.bottom, 1e-9, "底端對到繪圖區底部");
    checkClose(y->getPixel(1010, d), d.top, 1e-9, "頂端對到繪圖區頂部");
    checkClose(d.bottom - lowTop, highBottom - d.top, 1e-6, "0–10 與 1000–1010 兩段等高");
    checkClose(lowTop - highBottom, gap, 1e-6, "兩段之間留 breakGapPx 的空隙");
    checkClose(y->getPixel(500, d), (lowTop + highBottom) / 2, 1e-6, "截斷區裡的值落在空隙正中間");

    // 資料（Transform）與刻度（Axis）必須用同一套換算
    const Transform t = plot.transformFor(plot.bottomAxis(), y);
    check(t.yBreaks != nullptr, "transformFor 帶上截斷");
    for (double v : {0.0, 5.0, 10.0, 500.0, 1000.0, 1005.0, 1010.0})
        checkClose(t.py(v), y->getPixel(v, d), 1e-9, QStringLiteral("資料與刻度換算一致（v=%1）").arg(v));
    for (double v : {2.5, 1007.0})
        checkClose(t.cy(t.py(v)), v, 1e-9, QStringLiteral("來回換算（v=%1）").arg(v));
    // 範圍外要能線性外推，否則折線畫出繪圖區時會折回來
    check(t.py(-5) > d.bottom && t.py(1020) < d.top, "範圍外線性外推");

    // 刻度分段產生：兩段裡都要有刻度，而且沒有任何刻度落在截斷區裡
    int low = 0, high = 0, inside = 0;
    for (const Tick& tk : y->ticks()) {
        if (!tk.isMajor) continue;
        if (tk.position <= 10) ++low;
        else if (tk.position >= 1000) ++high;
        else ++inside;
    }
    check(low >= 3 && high >= 3, "兩段各自有刻度", QStringLiteral("下段 %1、上段 %2").arg(low).arg(high));
    check(inside == 0, "截斷區裡沒有刻度");

    // 合併與對調
    Axis a(Edge::Left);
    a.addBreak(5, 3);
    a.addBreak(4, 8);
    a.addBreak(20, 30);
    check(a.breaks().breaks.size() == 2 && a.breaks().breaks[0] == AxisBreak{3, 8},
          "重疊的截斷合併、from > to 自動對調");
    a.addBreak(1, 1);
    check(a.breaks().breaks.size() == 2, "長度為零的截斷被忽略");

    // 壓在可視範圍邊上的截斷只吃掉長度，不留空隙
    a.range = {6, 40};
    const auto gaps = a.breaks().gapFractions(a.range, 300);
    check(gaps.size() == 1, "只有完全落在範圍內的截斷才留空隙");

    // 反向軸也要對
    y->inverted = true;
    plot.renderToImage(600, 400, 1.0);
    const Transform ti = plot.transformFor(plot.bottomAxis(), y);
    checkClose(ti.py(0), d.top, 1e-9, "反向截斷軸：0 在頂端");
    checkClose(ti.cy(ti.py(1003)), 1003, 1e-9, "反向截斷軸：來回換算");
    y->inverted = false;

    // 自動範圍的邊界照「畫得出來的長度」留，不是照整個範圍
    {
        Plot p2;
        p2.addScatter({0, 60, 500, 560}, {0, 1, 0, 1});
        p2.bottomAxis()->addBreak(61, 499);
        p2.autoScale(0.05, true);
        // 沒被截掉的是 0–61 與 499–560，共 122 個單位，兩端各留 6.1
        checkClose(p2.bottomAxis()->range.min, -6.1, 1e-9, "截斷軸自動範圍：左邊界只留可見長度的 5%");
        checkClose(p2.bottomAxis()->range.max, 566.1, 1e-9, "截斷軸自動範圍：右邊界只留可見長度的 5%");
    }

    // 清掉之後回到線性
    y->clearBreaks();
    plot.renderToImage(600, 400, 1.0);
    checkClose(y->getPixel(505, plot.lastDataRect()), (d.top + d.bottom) / 2, 1e-6, "clearBreaks 後恢復線性");
}

void testInvertedAxis() {
    out() << "反向軸\n";

    Plot plot;
    plot.setAxisLimits(0, 10, 0, 100);
    plot.renderToImage(600, 400, 1.0);
    const PixelRect d = plot.lastDataRect();

    for (int pass = 0; pass < 2; ++pass) {
        const bool inverted = (pass == 1);
        plot.leftAxis()->inverted = inverted;
        plot.bottomAxis()->inverted = inverted;
        const Transform t = plot.transformFor(plot.bottomAxis(), plot.leftAxis());
        const QString tag = inverted ? QStringLiteral("反向") : QStringLiteral("正向");

        for (double v : {0.0, 25.0, 50.0, 100.0}) {
            checkClose(t.py(v), plot.leftAxis()->getPixel(v, d), 1e-9,
                       QStringLiteral("%1 Y 軸：資料與刻度的換算一致（v=%2）").arg(tag).arg(v));
        }
        for (double v : {0.0, 2.5, 10.0}) {
            checkClose(t.px(v), plot.bottomAxis()->getPixel(v, d), 1e-9,
                       QStringLiteral("%1 X 軸：資料與刻度的換算一致（v=%2）").arg(tag).arg(v));
        }

        // 來回換算要回得去，否則滑鼠的座標讀數會跟著錯
        checkClose(t.cy(t.py(37.0)), 37.0, 1e-9,
                   QStringLiteral("%1 Y 軸來回換算").arg(tag));
        checkClose(t.cx(t.px(3.0)), 3.0, 1e-9,
                   QStringLiteral("%1 X 軸來回換算").arg(tag));
    }

    {
        // 方向真的反過來了：反向軸上，大的值畫在下面
        plot.leftAxis()->inverted = true;
        const Transform t = plot.transformFor(plot.bottomAxis(), plot.leftAxis());
        check(t.py(100) > t.py(0), QStringLiteral("反向 Y 軸：大的值畫在下面"));
        plot.leftAxis()->inverted = false;
        const Transform t2 = plot.transformFor(plot.bottomAxis(), plot.leftAxis());
        check(t2.py(100) < t2.py(0), QStringLiteral("正向 Y 軸：大的值畫在上面"));
    }
}

/// 氣泡圖、甘特圖、文氏圖。
/// 桑基圖的版面與弦圖的彙總。
/// 玫瑰圖的堆疊與挖洞，以及極座標軸的半徑標籤。
/// 史密斯圖的反射係數映射。
///
/// Γ = (z-1)/(z+1)。這個公式寫錯了圖還是會畫出一堆圓，只是整個偏掉 ——
/// 看圖分不出來，只能靠幾個已知的特例去釘。
/// 逐點資料標籤。
///
/// 重點在 maxLabels 那條規則：超過就「安靜地不畫」。這種行為最容易寫錯
/// 又最難從圖上看出來 —— 標籤沒出現時，分不清是資料沒設、字型透明、
/// 還是被這條規則擋掉。
/// 馬賽克圖與網路圖。
/// 等角投影。
///
/// 這一族最容易出錯的是「繪製順序」：順序反了只會讓前後關係看起來怪，
/// 不會有任何錯誤訊息，而且在柱子一樣高的時候完全看不出來。
/// 含負值的堆疊。
/// 樹狀圖與平行座標圖。
/// 六角分箱與啞鈴圖。
/// 河流圖的堆疊與蜂群圖的排點。
/// 階層式分群與地平線圖。
/// 弧形文字與環狀圖的標籤方向。
void testArcText() {
    out() << "弧形文字\n";

    Transform t;
    t.rect = PixelRect(0, 400, 0, 400);   // left, right, top, bottom
    t.xr = CoordinateRange(-2, 2);
    t.yr = CoordinateRange(-2, 2);

    Label lb;
    lb.font.size = 12;
    lb.font.color = QColor(0, 0, 0);

    {
        lb.text = QStringLiteral("AB");
        const double two = arcTextSpanDegrees(t, {0, 0}, 1.0, lb);
        lb.text = QStringLiteral("ABAB");
        const double four = arcTextSpanDegrees(t, {0, 0}, 1.0, lb);
        check(two > 0, QStringLiteral("跨度為正"));
        checkClose(four, two * 2, 1e-6, QStringLiteral("字數加倍，跨度也加倍"));
    }
    {
        // 半徑愈大，同一段文字佔的角度愈小 —— 弧長變長了
        lb.text = QStringLiteral("ABCD");
        const double small = arcTextSpanDegrees(t, {0, 0}, 0.5, lb);
        const double big = arcTextSpanDegrees(t, {0, 0}, 1.0, lb);
        checkClose(small, big * 2, 1e-6, QStringLiteral("半徑加倍，跨度減半"));
    }
    {
        lb.text = QString();
        checkClose(arcTextSpanDegrees(t, {0, 0}, 1.0, lb), 0, 1e-12,
                   QStringLiteral("空字串的跨度是 0"));
        lb.text = QStringLiteral("A");
        checkClose(arcTextSpanDegrees(t, {0, 0}, 0.0, lb), 0, 1e-12,
                   QStringLiteral("半徑 0 時回傳 0，不除以零"));
    }

    {
        // 標籤方向決定「放不放得下」是比弧長還是比環的厚度。
        // 用錯方向的話，外圈的長標籤會被誤判成放得下、整段溢出去。
        auto redPixels = [](const QImage& img) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (c.red() > 180 && c.green() < 80 && c.blue() < 80) ++n;
                }
            return n;
        };

        // 一個很窄（角度小）但很厚（半徑範圍大）的環：
        // 沿弧排放不下，沿半徑排放得下。
        Plot plot;
        QVector<SunburstNode> roots;
        for (int i = 0; i < 24; ++i)
            // 扇形一律灰色。用自動配色的話，調色盤裡本來就有接近純紅的顏色，
            // 偵測紅色像素會抓到扇形本身，整個測試就白做了。
            roots.append({i == 0 ? QStringLiteral("LONGLABEL") : QString(), 1,
                          QColor(225, 225, 225), {}});
        auto* sb = plot.addSunburst(roots);
        sb->border = LineStyle(QColor(180, 180, 180), 1.0);
        sb->innerFraction = 0.05;
        sb->labelFont.color = QColor(255, 0, 0);
        sb->labelFont.size = 11;
        sb->minLabelArcPx = 0;
        plot.setAxisLimits(-1.1, 1.1, -1.1, 1.1);

        sb->labelOrientation = Sunburst::LabelOrientation::Radial;
        const int radial = redPixels(plot.renderToImage(500, 500, 1.0));
        check(radial > 20, QStringLiteral("窄而厚的環：沿半徑排放得下"),
              QStringLiteral("紅色像素 %1").arg(radial));

        sb->labelOrientation = Sunburst::LabelOrientation::Tangential;
        const int tangential = redPixels(plot.renderToImage(500, 500, 1.0));
        check(tangential == 0, QStringLiteral("同一個環：沿弧排放不下，所以不畫"),
              QStringLiteral("紅色像素 %1").arg(tangential));

        sb->labelOrientation = Sunburst::LabelOrientation::Curved;
        const int curved = redPixels(plot.renderToImage(500, 500, 1.0));
        check(curved == 0, QStringLiteral("逐字彎曲也放不下，同樣不畫"),
              QStringLiteral("紅色像素 %1").arg(curved));
    }
    {
        // Sector 的弧形標籤：有文字才畫
        Plot plot;
        auto* sec = plot.addSector(0, 0, 1.0, 0, 180);
        sec->innerRadiusX = sec->innerRadiusY = 0.8;
        sec->fill = FillStyle::none();
        sec->line = LineStyle::none();
        sec->arcLabel.font.color = QColor(0, 200, 0);
        sec->arcLabel.font.size = 14;
        plot.setAxisLimits(-1.2, 1.2, -1.2, 1.2);

        auto greenPixels = [](const QImage& img) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (c.green() > 150 && c.red() < 110 && c.blue() < 110) ++n;
                }
            return n;
        };
        check(greenPixels(plot.renderToImage(400, 400, 1.0)) == 0,
              QStringLiteral("沒有文字就不畫弧形標籤"));
        sec->arcLabel.text = QStringLiteral("HELLO");
        check(greenPixels(plot.renderToImage(400, 400, 1.0)) > 20,
              QStringLiteral("有文字就畫出來"));
    }
}

void testClusterAndHorizon() {
    out() << "階層分群與地平線圖\n";

    // 四個點排成兩對：{0,1} 很近、{2,3} 很近，兩對之間很遠
    const QVector<QVector<double>> d{{0, 1, 10, 11},
                                     {1, 0, 11, 10},
                                     {10, 11, 0, 1},
                                     {11, 10, 1, 0}};
    {
        const QVector<ClusterMerge> m = hierarchicalCluster(d, Linkage::Average);
        check(m.size() == 3, QStringLiteral("n 個點會有 n−1 次合併"));
        // 前兩次應該是把兩對各自併起來，距離都是 1
        checkClose(m[0].distance, 1, 1e-12, QStringLiteral("第一次合併的距離是最近的那對"));
        checkClose(m[1].distance, 1, 1e-12, QStringLiteral("第二次合併也是距離 1"));
        check(m[2].distance > 9, QStringLiteral("最後一次合併的距離大得多"),
              QStringLiteral("%1").arg(m[2].distance));

        // 距離必須遞增 —— 遞減的話樹會長出倒掛的枝幹
        bool increasing = true;
        for (int i = 1; i < m.size(); ++i)
            if (m[i].distance < m[i - 1].distance - 1e-12) increasing = false;
        check(increasing, QStringLiteral("合併的距離遞增"));

        // 編號規則和 SciPy 一致：>= n 代表之前的合併
        check(m[2].left >= 4 && m[2].right >= 4,
              QStringLiteral("最後一次合併的兩邊都是先前的合併結果"));
        check(m[2].size == 4, QStringLiteral("最後一群包含全部成員"));
    }
    {
        // 三種 linkage 在這組資料上第一次合併都一樣（最近的一對），
        // 但最後一次的距離不同：Single 取最近、Complete 取最遠。
        const double single = hierarchicalCluster(d, Linkage::Single).last().distance;
        const double complete = hierarchicalCluster(d, Linkage::Complete).last().distance;
        const double average = hierarchicalCluster(d, Linkage::Average).last().distance;
        check(single <= average && average <= complete,
              QStringLiteral("Single ≤ Average ≤ Complete"),
              QStringLiteral("%1 / %2 / %3").arg(single).arg(average).arg(complete));
    }
    {
        // 葉子順序：同一群的要相鄰，否則樹狀圖會出現交叉
        const QVector<ClusterMerge> m = hierarchicalCluster(d, Linkage::Average);
        const QVector<int> order = dendrogramLeafOrder(m, 4);
        check(order.size() == 4, QStringLiteral("每個葉子各出現一次"));
        QSet<int> seen;
        for (int i : order) seen.insert(i);
        check(seen.size() == 4, QStringLiteral("沒有重複的葉子"));

        // {0,1} 必須相鄰，{2,3} 也是
        const int p0 = order.indexOf(0), p1 = order.indexOf(1);
        const int p2 = order.indexOf(2), p3 = order.indexOf(3);
        check(std::abs(p0 - p1) == 1, QStringLiteral("同一群的葉子相鄰（0 與 1）"));
        check(std::abs(p2 - p3) == 1, QStringLiteral("同一群的葉子相鄰（2 與 3）"));
    }
    {
        check(hierarchicalCluster({}, Linkage::Average).isEmpty(),
              QStringLiteral("空矩陣沒有合併"));
        check(hierarchicalCluster({{0}}, Linkage::Average).isEmpty(),
              QStringLiteral("只有一個點時沒有合併"));
        // 非方陣
        check(hierarchicalCluster({{0, 1}, {1, 0}, {2, 2}}, Linkage::Average).isEmpty(),
              QStringLiteral("非方陣回傳空"));
    }
    {
        Dendrogram dg;
        dg.merges = hierarchicalCluster(d, Linkage::Average);
        dg.leafCount = 4;
        checkClose(dg.maxDistance(), dg.merges.last().distance, 1e-12,
                   QStringLiteral("最大距離是最後一次合併"));
        const QVector<double> pos = dg.leafPositions();
        check(pos.size() == 4, QStringLiteral("每個葉子各一個位置"));
        // 不能叫 slots —— 那是 Qt 的巨集，會被展開成空的
        QSet<int> usedSlots;
        for (double v : pos) usedSlots.insert(static_cast<int>(v));
        check(usedSlots.size() == 4, QStringLiteral("四個葉子佔四個不同的位置"));
    }

    {
        // 地平線圖：一層的高度
        Horizon h;
        h.xs = {0, 1, 2};
        h.ys = {0, 6, -3};
        h.bands = 3;
        checkClose(h.resolvedBandValue(), 2.0, 1e-12,
                   QStringLiteral("自動的一層高度 = 最大絕對值 / 層數"));

        h.bandValue = 5;
        checkClose(h.resolvedBandValue(), 5.0, 1e-12, QStringLiteral("有指定就用指定的"));

        // 全部是 0 時不能回傳 0（會除以零）
        Horizon flat;
        flat.xs = {0, 1};
        flat.ys = {0, 0};
        check(flat.resolvedBandValue() > 0, QStringLiteral("值全為 0 時仍回傳正的層高"));

        Horizon empty;
        check(!empty.axisLimits().hasData(), QStringLiteral("沒有資料不回報範圍"));
    }
}

void testStreamAndSwarm() {
    out() << "河流圖與蜂群圖\n";

    {
        // Zero：和一般的堆疊一樣，基準線在 0
        const QVector<StackedBand> b = stackStream({{1, 2}, {3, 4}}, StackOffset::Zero);
        checkClose(b[0].lower[0], 0, 1e-12, QStringLiteral("Zero：第一層從 0 起"));
        checkClose(b[1].upper[0], 4, 1e-12, QStringLiteral("Zero：頂端是總和"));
    }
    {
        // Centered：每一欄對稱於中線，所以上下界的和是 0
        const QVector<StackedBand> b = stackStream({{1, 5}, {3, 1}}, StackOffset::Centered);
        for (int i = 0; i < 2; ++i) {
            const double mid = (b.first().lower[i] + b.last().upper[i]) / 2;
            checkClose(mid, 0, 1e-12,
                       QStringLiteral("Centered：第 %1 欄對稱於 0").arg(i));
        }
    }
    {
        // Expand：每一欄的總厚度都是 1
        const QVector<StackedBand> b = stackStream({{1, 30}, {3, 10}}, StackOffset::Expand);
        for (int i = 0; i < 2; ++i)
            checkClose(b.last().upper[i] - b.first().lower[i], 1.0, 1e-12,
                       QStringLiteral("Expand：第 %1 欄正規化成 1").arg(i));
        checkClose(b[0].upper[0], 0.25, 1e-12, QStringLiteral("Expand：比例正確"));
    }
    {
        // 不論哪種基準線，每一層的「厚度」都必須等於它的值 ——
        // 基準線只能平移，不能改變厚度。
        const QVector<QVector<double>> src{{2, 7, 3}, {5, 1, 6}, {4, 4, 2}};
        for (StackOffset off : {StackOffset::Zero, StackOffset::Centered, StackOffset::Wiggle}) {
            const QVector<StackedBand> b = stackStream(src, off);
            for (int j = 0; j < src.size(); ++j)
                for (int i = 0; i < 3; ++i)
                    checkClose(b[j].upper[i] - b[j].lower[i], src[j][i], 1e-12,
                               QStringLiteral("厚度等於原值（層 %1、欄 %2）").arg(j).arg(i));
        }
    }
    {
        // 負值在河流圖裡沒有定義，當成 0
        const QVector<StackedBand> b = stackStream({{-5, 2}}, StackOffset::Zero);
        checkClose(b[0].upper[0] - b[0].lower[0], 0, 1e-12,
                   QStringLiteral("負值當成 0（厚度為 0）"));
        check(stackStream({}).isEmpty(), QStringLiteral("空輸入回傳空"));
    }

    {
        // 蜂群：點不可以重疊
        Beeswarm sw;
        SwarmGroup g;
        for (int i = 0; i < 40; ++i) g.values.append(std::sin(i * 1.1) * 10 + 50);
        sw.groups = {g};
        sw.maxSpreadFraction = 10;   // 給足空間，先驗「不重疊」這件事

        const double vr = 0.6, cr = 0.02;
        const QVector<Beeswarm::SwarmPoint> pts = sw.layout(vr, cr);
        check(pts.size() == 40, QStringLiteral("每個點都有位置"));

        int overlaps = 0;
        for (int i = 0; i < pts.size(); ++i)
            for (int j = i + 1; j < pts.size(); ++j) {
                const double dv = (pts[i].value - pts[j].value) / vr;
                const double dc = (pts[i].offset - pts[j].offset) / cr;
                if (dv * dv + dc * dc < 4.0 - 1e-6) ++overlaps;
            }
        check(overlaps == 0, QStringLiteral("沒有任何兩點重疊"),
              QStringLiteral("%1 對重疊").arg(overlaps));
    }
    {
        // 結果要可重現，而且不受輸入順序影響 —— 內部先排序過
        Beeswarm a, b;
        SwarmGroup ga, gb;
        for (int i = 0; i < 25; ++i) ga.values.append((i * 7) % 25);
        gb.values = ga.values;
        std::reverse(gb.values.begin(), gb.values.end());
        a.groups = {ga};
        b.groups = {gb};

        const auto pa = a.layout(0.5, 0.05);
        const auto pb = b.layout(0.5, 0.05);
        bool same = pa.size() == pb.size();
        for (int i = 0; same && i < pa.size(); ++i)
            same = (pa[i].value == pb[i].value && pa[i].offset == pb[i].offset);
        check(same, QStringLiteral("輸入順序不影響結果"));
    }
    {
        // 排不下時夾在 maxSpreadFraction 內，不會無限往外長
        Beeswarm sw;
        SwarmGroup g;
        for (int i = 0; i < 60; ++i) g.values.append(50);  // 全部一樣
        sw.groups = {g};
        sw.maxSpreadFraction = 0.4;
        double worst = 0;
        for (const auto& p : sw.layout(0.5, 0.05)) worst = std::max(worst, std::abs(p.offset));
        check(worst <= 0.4 + 1e-9, QStringLiteral("位移夾在 maxSpreadFraction 內"),
              QStringLiteral("最大位移 %1").arg(worst, 0, 'f', 3));
    }

    {
        // 弧線圖的線性版面：節點等距排在一條水平線上
        NetworkGraph g;
        for (int i = 0; i < 5; ++i)
            g.nodes.append({QString(), 1, QColor(), {0, 0}, false});
        g.layout = NetworkGraph::Layout::Linear;
        g.extent = CoordinateRect(0, 8, 0, 4);
        const QVector<Coordinates> pos = g.computeLayout();
        checkClose(pos.first().x, 0, 1e-12, QStringLiteral("線性版面：第一個在最左"));
        checkClose(pos.last().x, 8, 1e-12, QStringLiteral("線性版面：最後一個在最右"));
        checkClose(pos[1].x - pos[0].x, pos[2].x - pos[1].x, 1e-12,
                   QStringLiteral("線性版面：等距"));
        bool sameY = true;
        for (const Coordinates& c : pos) sameY = sameY && std::abs(c.y - 2.0) < 1e-12;
        check(sameY, QStringLiteral("線性版面：全部在同一條水平線上"));
    }
}

void testHexbinAndDumbbell() {
    out() << "六角分箱與啞鈴圖\n";

    {
        // 每一個點都要落進某一格，一個不多一個不少
        Hexbin h;
        gen::resetSeed();
        const int n = 2000;
        for (int i = 0; i < n; ++i) {
            h.xs.append(gen::randomNormal(1, 0, 1).first());
            h.ys.append(gen::randomNormal(1, 0, 1).first());
        }
        h.columns = 20;

        const QVector<HexCell> cells = h.bins();
        int total = 0;
        for (const HexCell& c : cells) total += c.count;
        check(total == n, QStringLiteral("每個點都分到一格，總數不變"),
              QStringLiteral("分到 %1 / 共 %2").arg(total).arg(n));
        check(!cells.isEmpty(), QStringLiteral("有分出格子"));
        check(h.maxCount() > 0, QStringLiteral("最大計數為正"));

        // 格子不重複
        QSet<QPair<int, int>> seen;
        bool dup = false;
        for (const HexCell& c : cells) {
            // 用四捨五入到小數三位當作鍵，避免浮點誤差
            const QPair<int, int> key{static_cast<int>(std::llround(c.x * 1000)),
                                      static_cast<int>(std::llround(c.y * 1000))};
            if (seen.contains(key)) dup = true;
            seen.insert(key);
        }
        check(!dup, QStringLiteral("沒有重複的格子"));
    }
    {
        // 結果必須可重現：同一份資料算兩次要完全一樣。
        // 內部若用 QHash 蒐集格子，走訪順序每個行程都不同，PNG 就不可重現了。
        Hexbin h;
        for (int i = 0; i < 200; ++i) {
            h.xs.append((i % 17) * 0.3);
            h.ys.append((i % 11) * 0.4);
        }
        const QVector<HexCell> a = h.bins();
        const QVector<HexCell> b = h.bins();
        bool same = a.size() == b.size();
        for (int i = 0; same && i < a.size(); ++i)
            same = (a[i].x == b[i].x && a[i].y == b[i].y && a[i].count == b[i].count);
        check(same, QStringLiteral("兩次分箱的結果完全一致（含順序）"));
    }
    {
        // NaN 不計入
        Hexbin h;
        h.xs = {0, 1, std::numeric_limits<double>::quiet_NaN(), 2};
        h.ys = {0, 1, 1, 2};
        int total = 0;
        for (const HexCell& c : h.bins()) total += c.count;
        check(total == 3, QStringLiteral("NaN 的點不計入"),
              QStringLiteral("算到 %1").arg(total));

        Hexbin empty;
        check(empty.bins().isEmpty(), QStringLiteral("沒有資料就沒有格子"));
        check(empty.maxCount() == 0, QStringLiteral("沒有資料時最大計數是 0"));
    }

    {
        Dumbbell d;
        d.rows = {{QStringLiteral("a"), 10, 30, QColor(), QColor()},
                  {QStringLiteral("b"), 50, 20, QColor(), QColor()}};
        const AxisLimits lim = d.axisLimits();
        checkClose(lim.x.min, 10, 1e-9, QStringLiteral("數值軸下界取兩端的最小值"));
        checkClose(lim.x.max, 50, 1e-9, QStringLiteral("數值軸上界取兩端的最大值"));
        // 類別軸兩端各留半格，端點的標記才不會被切掉
        checkClose(lim.y.min, -0.5, 1e-9, QStringLiteral("類別軸下端留半格"));
        checkClose(lim.y.max, 1.5, 1e-9, QStringLiteral("類別軸上端留半格"));

        d.horizontal = false;
        const AxisLimits v = d.axisLimits();
        checkClose(v.y.max, 50, 1e-9, QStringLiteral("直向時數值換到 Y 軸"));
        checkClose(v.x.min, -0.5, 1e-9, QStringLiteral("直向時類別換到 X 軸"));

        Dumbbell empty;
        check(!empty.axisLimits().hasData(), QStringLiteral("空的啞鈴圖不回報範圍"));
    }
}

void testTreemapAndParallel() {
    out() << "樹狀圖與平行座標\n";

    {
        // 父節點自己的值要被忽略，改用子節點的總和
        TreemapNode root;
        root.value = 999;
        root.children = {{QStringLiteral("a"), 3, QColor(), {}},
                         {QStringLiteral("b"), 7, QColor(), {}}};
        checkClose(root.total(), 10, 1e-9, QStringLiteral("父節點用子節點的總和"));

        TreemapNode leaf;
        leaf.value = 4;
        checkClose(leaf.total(), 4, 1e-9, QStringLiteral("葉節點用自己的 value"));
        leaf.value = -2;
        checkClose(leaf.total(), 0, 1e-9, QStringLiteral("負值當成 0"));
    }
    {
        // 面積要正比於數值。squarified 只決定「排法」，不該改變面積 ——
        // 這一項錯的話，圖上還是一堆方塊，眼睛完全分不出來。
        Treemap tm;
        tm.roots = {{QStringLiteral("a"), 50, QColor(), {}},
                    {QStringLiteral("b"), 30, QColor(), {}},
                    {QStringLiteral("c"), 20, QColor(), {}}};
        tm.extent = CoordinateRect(0, 10, 0, 10);
        tm.paddingFraction = 0;     // 先關掉留白，面積才對得起來
        tm.groupPaddingFraction = 0;

        const QVector<Treemap::Cell> cells = tm.layout();
        check(cells.size() == 3, QStringLiteral("三個葉節點三塊"));
        checkClose(tm.grandTotal(), 100, 1e-9, QStringLiteral("總量"));

        double totalArea = 0;
        for (const Treemap::Cell& c : cells) totalArea += c.rect.width() * c.rect.height();
        checkClose(totalArea, 100, 1e-6, QStringLiteral("全部矩形鋪滿 extent"));

        for (const Treemap::Cell& c : cells) {
            const double area = c.rect.width() * c.rect.height();
            checkClose(area, c.value, 1e-6,
                       QStringLiteral("「%1」的面積正比於數值").arg(c.label));
        }
    }
    {
        // 長寬比：squarified 的重點就是不要出現細長條
        Treemap tm;
        for (int i = 0; i < 12; ++i)
            tm.roots.append({QStringLiteral("n%1").arg(i), 12.0 - i, QColor(), {}});
        tm.extent = CoordinateRect(0, 10, 0, 10);
        tm.paddingFraction = 0;
        tm.groupPaddingFraction = 0;

        double worst = 1;
        for (const Treemap::Cell& c : tm.layout()) {
            const double w = c.rect.width(), h = c.rect.height();
            if (w > 0 && h > 0) worst = std::max(worst, std::max(w / h, h / w));
        }
        check(worst < 5.0, QStringLiteral("沒有特別細長的矩形"),
              QStringLiteral("最差長寬比 %1").arg(worst, 0, 'f', 2));
    }
    {
        // 階層：群組本身也是一塊，而且排在它的子塊前面（畫的順序）
        Treemap tm;
        tm.roots = {{QStringLiteral("g"), 0, QColor(), {{QStringLiteral("x"), 1, QColor(), {}},
                                                        {QStringLiteral("y"), 1, QColor(), {}}}}};
        tm.extent = CoordinateRect(0, 4, 0, 4);
        const QVector<Treemap::Cell> cells = tm.layout();
        check(cells.size() == 3, QStringLiteral("一個群組加兩個葉節點"));
        check(!cells[0].isLeaf && cells[0].depth == 0, QStringLiteral("群組排在最前面"));
        check(cells[1].isLeaf && cells[1].depth == 1, QStringLiteral("子塊的深度是 1"));

        Treemap empty;
        check(empty.layout().isEmpty(), QStringLiteral("空的樹狀圖沒有版面"));
        check(!empty.axisLimits().hasData(), QStringLiteral("空的樹狀圖不回報範圍"));
    }

    {
        ParallelCoordinates pc;
        pc.axes = {{QStringLiteral("a"), CoordinateRange(), false, 5},
                   {QStringLiteral("b"), CoordinateRange(), false, 5}};
        pc.rows = {{0, 100}, {10, 300}, {5, 200}};
        pc.extent = CoordinateRect(0, 10, 0, 10);

        const QVector<CoordinateRange> r = pc.resolvedRanges();
        checkClose(r[0].min, 0, 1e-12, QStringLiteral("第一軸的範圍下界來自資料"));
        checkClose(r[0].max, 10, 1e-12, QStringLiteral("第一軸的範圍上界"));
        checkClose(r[1].min, 100, 1e-12, QStringLiteral("第二軸各自取自己的範圍"));

        // 兩根軸分別落在 extent 的左右兩端
        checkClose(pc.axisX(0), 0, 1e-12, QStringLiteral("第一根軸在最左"));
        checkClose(pc.axisX(1), 10, 1e-12, QStringLiteral("最後一根軸在最右"));

        // 最小值對到底、最大值對到頂
        checkClose(pc.valueToY(0, 0), 0, 1e-12, QStringLiteral("最小值對到 extent 底部"));
        checkClose(pc.valueToY(0, 10), 10, 1e-12, QStringLiteral("最大值對到 extent 頂部"));
        checkClose(pc.valueToY(0, 5), 5, 1e-12, QStringLiteral("中間值線性內插"));

        // inverted 把方向翻過來
        pc.axes[0].inverted = true;
        checkClose(pc.valueToY(0, 0), 10, 1e-12, QStringLiteral("反向軸：最小值在上"));
        checkClose(pc.valueToY(0, 10), 0, 1e-12, QStringLiteral("反向軸：最大值在下"));
    }
    {
        // 指定範圍優先於資料
        ParallelCoordinates pc;
        ParallelAxisSpec spec;
        spec.range = CoordinateRange(-100, 100);
        pc.axes = {spec};
        pc.rows = {{0}, {10}};
        checkClose(pc.resolvedRanges().at(0).min, -100, 1e-12,
                   QStringLiteral("有指定範圍就不看資料"));
    }
    {
        // 全部一樣大時不能除以零
        ParallelCoordinates pc;
        pc.axes = {ParallelAxisSpec()};
        pc.rows = {{7}, {7}};
        pc.extent = CoordinateRect(0, 1, 0, 10);
        const double y = pc.valueToY(0, 7);
        check(std::isfinite(y), QStringLiteral("值全部相同時仍算得出位置"),
              QStringLiteral("y = %1").arg(y));
    }
}

void testStackSigned() {
    out() << "有正有負的堆疊\n";

    {
        // 全正：就是一般的累積和
        const QVector<StackedBand> b = stackSigned({{1, 2}, {3, 4}, {5, 6}});
        check(b.size() == 3, QStringLiteral("每個數列一條帶"));
        checkClose(b[0].lower[0], 0, 1e-12, QStringLiteral("第一層從 0 開始"));
        checkClose(b[0].upper[0], 1, 1e-12, QStringLiteral("第一層的上界"));
        checkClose(b[1].lower[0], 1, 1e-12, QStringLiteral("第二層接著第一層"));
        checkClose(b[1].upper[0], 4, 1e-12, QStringLiteral("第二層的上界"));
        checkClose(b[2].upper[0], 9, 1e-12, QStringLiteral("第三層的上界是總和"));
    }
    {
        // 混合：正的往上、負的往下，兩邊各自累積。
        // 用單純的累積和會得到 10 / 10-4=6 / 6+3=9 —— 第二層整個把第三層拉下去。
        const QVector<StackedBand> b = stackSigned({{10}, {-4}, {3}});
        checkClose(b[0].lower[0], 0, 1e-12, QStringLiteral("正值從 0 往上"));
        checkClose(b[0].upper[0], 10, 1e-12, QStringLiteral("正值的上界"));
        checkClose(b[1].upper[0], 0, 1e-12, QStringLiteral("負值從 0 往下"));
        checkClose(b[1].lower[0], -4, 1e-12, QStringLiteral("負值的下界"));
        // 第三層是正的，要接在「正累積」上，不受中間那個負值影響
        checkClose(b[2].lower[0], 10, 1e-12,
                   QStringLiteral("後面的正值接在正累積上，不被負值拉走"));
        checkClose(b[2].upper[0], 13, 1e-12, QStringLiteral("第三層的上界"));
    }
    {
        // 兩個負值要往下疊，不是互相抵銷
        const QVector<StackedBand> b = stackSigned({{-2}, {-3}});
        checkClose(b[0].upper[0], 0, 1e-12, QStringLiteral("第一個負值的上界是 0"));
        checkClose(b[0].lower[0], -2, 1e-12, QStringLiteral("第一個負值的下界"));
        checkClose(b[1].upper[0], -2, 1e-12, QStringLiteral("第二個負值接著往下"));
        checkClose(b[1].lower[0], -5, 1e-12, QStringLiteral("第二個負值的下界"));
    }
    {
        // 0 當成正的，不改變累積
        const QVector<StackedBand> b = stackSigned({{5}, {0}, {2}});
        checkClose(b[1].lower[0], 5, 1e-12, QStringLiteral("0 的上下界相同"));
        checkClose(b[1].upper[0], 5, 1e-12, QStringLiteral("0 不改變累積"));
        checkClose(b[2].upper[0], 7, 1e-12, QStringLiteral("0 之後接著累積"));
    }
    {
        // NaN 讓那一段斷開，但不能影響累積 —— 當成 0 的話後面的層會位移
        const QVector<StackedBand> b =
            stackSigned({{5, 5}, {std::numeric_limits<double>::quiet_NaN(), 2}, {1, 1}});
        check(std::isnan(b[1].lower[0]), QStringLiteral("NaN 那一點的下界也是 NaN"));
        checkClose(b[2].lower[0], 5, 1e-12,
                   QStringLiteral("NaN 不計入累積，後面的層不位移"));
        checkClose(b[2].lower[1], 7, 1e-12, QStringLiteral("非 NaN 的那一點正常累積"));
    }
    {
        // 長度不一致以最短的為準
        const QVector<StackedBand> b = stackSigned({{1, 2, 3}, {4, 5}});
        check(b[0].upper.size() == 2, QStringLiteral("長度取最短的"));

        check(stackSigned({}).isEmpty(), QStringLiteral("空輸入回傳空"));
    }
}

void testIsometric() {
    out() << "等角投影\n";

    {
        IsoProjection pr;  // 預設 30 度
        const double c = std::cos(30 * kPi / 180.0);
        const double s2 = std::sin(30 * kPi / 180.0);

        // 原點投到原點
        const Coordinates o = pr.project(0, 0, 0);
        checkClose(o.x, 0, 1e-12, QStringLiteral("原點投到原點 (x)"));
        checkClose(o.y, 0, 1e-12, QStringLiteral("原點投到原點 (y)"));

        // +x 往右下、+y 往左下
        const Coordinates px = pr.project(1, 0, 0);
        checkClose(px.x, c, 1e-12, QStringLiteral("+x 往右"));
        checkClose(px.y, -s2, 1e-12, QStringLiteral("+x 往下"));
        const Coordinates py = pr.project(0, 1, 0);
        checkClose(py.x, -c, 1e-12, QStringLiteral("+y 往左"));
        checkClose(py.y, -s2, 1e-12, QStringLiteral("+y 往下"));

        // +z 純粹往上，不影響水平位置
        const Coordinates pz = pr.project(0, 0, 1);
        checkClose(pz.x, 0, 1e-12, QStringLiteral("+z 不影響水平位置"));
        checkClose(pz.y, 1, 1e-12, QStringLiteral("+z 往上"));

        // heightScale 只縮放高度
        pr.heightScale = 0.5;
        checkClose(pr.project(0, 0, 2).y, 1.0, 1e-12, QStringLiteral("heightScale 縮放高度"));
        checkClose(pr.project(1, 0, 0).x, c, 1e-12,
                   QStringLiteral("heightScale 不影響水平位置"));
    }
    {
        // 深度：x + y 愈大愈靠近觀眾
        check(IsoProjection::depth(2, 2) > IsoProjection::depth(0, 0),
              QStringLiteral("x + y 大的比較靠近觀眾"));
        checkClose(IsoProjection::depth(1, 3), IsoProjection::depth(3, 1), 1e-12,
                   QStringLiteral("同一條等深線上的兩點深度相同"));
    }
    {
        // 範圍要含到八個角投影之後的極值。
        // 只取柱頂或柱底都會漏掉一側 —— 最左和最右來自不同的角。
        IsoBars b;
        b.bars = {{0, 0, 3, 1.0, 1.0, QColor(), QString()}};
        const AxisLimits lim = b.axisLimits();
        const double c = std::cos(30 * kPi / 180.0);
        checkClose(lim.x.min, -c, 1e-9, QStringLiteral("X 下界來自 +y 那個角"));
        checkClose(lim.x.max, c, 1e-9, QStringLiteral("X 上界來自 +x 那個角"));
        // 投影是 y' = -(x + y) * sin30 + z。最高點不是「柱頂中心」，
        // 而是柱頂「最遠」的那個角（x + y 最小）：-(-1) * 0.5 + 3 = 3.5。
        checkClose(lim.y.max, 3.5, 1e-9, QStringLiteral("Y 上界是柱頂最遠的那個角"));
        // 最低點是底面「最近」的那個角（x + y 最大）：-(1) * 0.5 + 0 = -0.5
        checkClose(lim.y.min, -0.5, 1e-9, QStringLiteral("Y 下界是底面最近的那個角"));
    }
    {
        IsoBars empty;
        check(!empty.axisLimits().hasData(), QStringLiteral("沒有柱子就不回報範圍"));
    }

    {
        IsoStack st;
        st.layers = {{2, 1, 0.8, QColor(), QString()},
                     {1, 0.8, 0, QColor(), QString()},
                     {-1, 1, 1, QColor(), QString()}};  // 負高度要被忽略
        checkClose(st.totalHeight(), 3, 1e-9, QStringLiteral("總高不含負的那一層"));
        check(st.axisLimits().hasData(), QStringLiteral("有層時回報範圍"));

        IsoStack empty;
        check(!empty.axisLimits().hasData(), QStringLiteral("沒有層就不回報範圍"));
    }
}

void testMosaicAndNetwork() {
    out() << "馬賽克圖與網路圖\n";

    {
        Mosaic m;
        m.columns = {{QStringLiteral("a"), {10, 20, 30}, {}},
                     {QStringLiteral("b"), {5, 5}, {}},
                     {QStringLiteral("c"), {}, {}}};
        const QVector<double> totals = m.columnTotals();
        checkClose(totals[0], 60, 1e-9, QStringLiteral("欄總量是各段的和"));
        checkClose(totals[1], 10, 1e-9, QStringLiteral("第二欄的總量"));
        checkClose(totals[2], 0, 1e-9, QStringLiteral("空欄的總量是 0"));
        checkClose(m.grandTotal(), 70, 1e-9, QStringLiteral("全體總量"));

        // 負值與 NaN 不計入
        m.columns[1].values = {5, -3, std::numeric_limits<double>::quiet_NaN(), 5};
        checkClose(m.columnTotals().at(1), 10, 1e-9,
                   QStringLiteral("負值與 NaN 不計入欄總量"));
    }

    {
        // 力導向版面必須是決定性的。只要有一點不確定性，輸出的 PNG 就不可重現，
        // 視覺回歸會整個失效 —— 這個專案為此已經踩過一次（QHash 的走訪順序）。
        NetworkGraph g;
        for (int i = 0; i < 12; ++i)
            g.nodes.append({QStringLiteral("n%1").arg(i), 1.0, QColor(), {0, 0}, false});
        for (int i = 1; i < 12; ++i) g.edges.append({0, i, 1.0, QColor()});
        g.extent = CoordinateRect(0, 10, 0, 10);
        g.seed = 5;
        g.iterations = 120;

        const QVector<Coordinates> a = g.computeLayout();
        const QVector<Coordinates> b = g.computeLayout();
        bool same = a.size() == b.size();
        for (int i = 0; same && i < a.size(); ++i)
            same = (a[i].x == b[i].x && a[i].y == b[i].y);
        check(same, QStringLiteral("同一個種子算兩次得到完全一樣的版面"));

        g.seed = 6;
        const QVector<Coordinates> c = g.computeLayout();
        bool differs = false;
        for (int i = 0; i < a.size() && i < c.size(); ++i)
            if (std::abs(a[i].x - c[i].x) > 1e-9) differs = true;
        check(differs, QStringLiteral("換個種子就會得到不同的版面"));

        // 全部節點都要落在 extent 裡（等比例縮放之後）
        int outside = 0;
        for (const Coordinates& p : a)
            if (p.x < -1e-6 || p.x > 10 + 1e-6 || p.y < -1e-6 || p.y > 10 + 1e-6) ++outside;
        check(outside == 0, QStringLiteral("版面結果全部落在 extent 內"),
              QStringLiteral("%1 個跑出去").arg(outside));

        // 節點不該疊在同一點上（那代表斥力沒作用）
        double minDist = std::numeric_limits<double>::max();
        for (int i = 0; i < a.size(); ++i)
            for (int j = i + 1; j < a.size(); ++j)
                minDist = std::min(minDist, std::hypot(a[i].x - a[j].x, a[i].y - a[j].y));
        check(minDist > 0.2, QStringLiteral("節點沒有疊在一起"),
              QStringLiteral("最近兩點相距 %1").arg(minDist, 0, 'f', 3));
    }
    {
        // 環形版面：全部等距排在一個圓上
        NetworkGraph g;
        for (int i = 0; i < 8; ++i)
            g.nodes.append({QString(), 1.0, QColor(), {0, 0}, false});
        g.layout = NetworkGraph::Layout::Circular;
        g.extent = CoordinateRect(-1, 1, -1, 1);
        const QVector<Coordinates> pos = g.computeLayout();
        check(pos.size() == 8, QStringLiteral("環形版面的節點數"));
        double r0 = std::hypot(pos[0].x, pos[0].y);
        bool allSame = true;
        for (const Coordinates& p : pos)
            if (std::abs(std::hypot(p.x, p.y) - r0) > 1e-9) allSame = false;
        check(allSame, QStringLiteral("環形版面上每個節點離中心等距"));
    }
    {
        // 釘住的節點不能被推走
        NetworkGraph g;
        g.nodes = {{QString(), 1, QColor(), {0.5, -0.5}, true},
                   {QString(), 1, QColor(), {0, 0}, false},
                   {QString(), 1, QColor(), {0, 0}, false}};
        g.edges = {{0, 1, 1, QColor()}, {1, 2, 1, QColor()}};
        g.layout = NetworkGraph::Layout::Manual;
        const QVector<Coordinates> pos = g.computeLayout();
        checkClose(pos[0].x, 0.5, 1e-12, QStringLiteral("Manual 版面直接用 position"));
    }
    {
        NetworkGraph empty;
        check(!empty.axisLimits().hasData(), QStringLiteral("空的網路圖不回報範圍"));
        check(empty.computeLayout().isEmpty(), QStringLiteral("空的網路圖沒有版面"));
    }
}

void testPointLabels() {
    out() << "逐點資料標籤\n";

    auto countRed = [](const QImage& img) {
        int n = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 180 && c.green() < 80 && c.blue() < 80) ++n;
            }
        return n;
    };

    {
        Plot plot;
        QVector<double> xs, ys;
        QStringList texts;
        for (int i = 0; i < 5; ++i) {
            xs.append(i);
            ys.append(i);
            texts.append(QStringLiteral("P%1").arg(i));
        }
        auto* m = plot.addMarkers(xs, ys);
        m->marker = MarkerStyle(MarkerShape::FilledCircle, 6, QColor(0, 0, 255));
        m->pointLabels.texts = texts;
        m->pointLabels.font.color = QColor(255, 0, 0);
        m->pointLabels.font.size = 14;
        plot.setAxisLimits(-1, 5, -1, 5);

        check(countRed(plot.renderToImage(500, 500, 1.0)) > 20,
              QStringLiteral("標籤有畫出來"));

        // 超過 maxLabels 就整批不畫
        m->pointLabels.maxLabels = 3;
        check(countRed(plot.renderToImage(500, 500, 1.0)) == 0,
              QStringLiteral("點數超過 maxLabels 就整批不畫"));

        m->pointLabels.maxLabels = 200;
        m->pointLabels.font.color = QColor(255, 0, 0, 0);
        check(countRed(plot.renderToImage(500, 500, 1.0)) == 0,
              QStringLiteral("字型透明就不畫"));
    }
    {
        // texts 比資料點少：後面的點就沒有標籤，不會越界
        Plot plot;
        auto* m = plot.addMarkers({0, 1, 2, 3}, {0, 1, 2, 3});
        m->pointLabels.texts = {QStringLiteral("A"), QStringLiteral("B")};
        m->pointLabels.font.color = QColor(255, 0, 0);
        m->pointLabels.font.size = 14;
        plot.setAxisLimits(-1, 4, -1, 4);
        const int n = countRed(plot.renderToImage(500, 500, 1.0));
        check(n > 10, QStringLiteral("texts 比資料短時仍畫得出前幾個"),
              QStringLiteral("紅色像素 %1").arg(n));
    }
    {
        // Scatter 也吃同一組設定
        Plot plot;
        auto* sc = plot.addScatter({0, 1, 2}, {0, 1, 2});
        sc->pointLabels.texts = {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
        sc->pointLabels.font.color = QColor(255, 0, 0);
        sc->pointLabels.font.size = 14;
        plot.setAxisLimits(-1, 3, -1, 3);
        check(countRed(plot.renderToImage(500, 500, 1.0)) > 20,
              QStringLiteral("Scatter 也支援逐點標籤"));
    }
    {
        // 預設是關的：沒設 texts 就什麼都不畫
        PointLabels def;
        check(!def.isVisible(), QStringLiteral("預設不畫（texts 是空的）"));
        def.texts = {QStringLiteral("a")};
        check(def.isVisible(), QStringLiteral("給了 texts 就會畫"));
    }
}

void testSmithChart() {
    out() << "史密斯圖\n";

    // 四個標準特例（圓心／短路／開路／被動阻抗在圓內）已經由 testSpecialty()
    // 的「史密斯圖」那一段驗過，這裡只補它沒涵蓋的部分。
    {
        // 純電抗（r = 0）一定落在單位圓上：|Γ| = 1，全反射
        for (double x : {0.2, 0.5, 1.0, 2.0, 5.0, -0.7, -1.6}) {
            const Coordinates g = SmithChartAxis::toCartesian(0, x);
            checkClose(std::hypot(g.x, g.y), 1.0, 1e-12,
                       QStringLiteral("純電抗 jx 落在單位圓上（x=%1）").arg(x));
        }
    }
    {
        // 共軛阻抗對稱於實軸
        const Coordinates a = SmithChartAxis::toCartesian(0.4, 0.9);
        const Coordinates b = SmithChartAxis::toCartesian(0.4, -0.9);
        checkClose(a.x, b.x, 1e-12, QStringLiteral("共軛阻抗的實部相同"));
        checkClose(a.y, -b.y, 1e-12, QStringLiteral("共軛阻抗的虛部相反"));
    }
    {
        // 等電阻圓的幾何：圓心 (r/(1+r), 0)、半徑 1/(1+r)，
        // 而且一定和單位圓內切於開路點 Γ=+1
        for (double r : {0.2, 1.0, 5.0}) {
            const double cx = r / (1 + r);
            const double rad = 1.0 / (1 + r);
            checkClose(cx + rad, 1.0, 1e-12,
                       QStringLiteral("等電阻圓內切於開路點（r=%1）").arg(r));
            // 該圓與實軸的兩個交點就是 z=r 與 z=∞
            const Coordinates onAxis = SmithChartAxis::toCartesian(r, 0);
            checkClose(onAxis.x, cx - rad, 1e-12,
                       QStringLiteral("等電阻圓的左交點就是 z=r（r=%1）").arg(r));
        }
    }
    {
        // center / radius 要有作用
        SmithChartAxis ax;
        ax.center = {5, -2};
        ax.radius = 3;
        const Coordinates c = ax.coordinates(1, 0);
        checkClose(c.x, 5, 1e-12, QStringLiteral("coordinates 有套用 center (x)"));
        checkClose(c.y, -2, 1e-12, QStringLiteral("coordinates 有套用 center (y)"));
        const Coordinates edge = ax.coordinates(0, 0);
        checkClose(edge.x, 2, 1e-12, QStringLiteral("coordinates 有套用 radius"));

        const AxisLimits lim = ax.axisLimits();
        check(lim.x.span() > 2 * ax.radius,
              QStringLiteral("範圍比圓本身大，留給電抗標籤"));
    }
}

void testRoseCharts() {
    out() << "玫瑰圖與半徑標籤\n";

    {
        // 沒有 stacks 時用 slices 的 value
        Coxcomb c;
        c.slices = {{10, QStringLiteral("a"), QColor(), 0, QString()},
                    {20, QStringLiteral("b"), QColor(), 0, QString()}};
        const QVector<double> totals = c.sliceTotals();
        checkClose(totals[0], 10, 1e-9, QStringLiteral("沒堆疊時用 value"));
        checkClose(totals[1], 20, 1e-9, QStringLiteral("沒堆疊時用 value（第二格）"));
    }
    {
        // 有 stacks 時 value 被忽略，改用各段的和
        Coxcomb c;
        c.slices = {{999, QStringLiteral("a"), QColor(), 0, QString()},
                    {999, QStringLiteral("b"), QColor(), 0, QString()}};
        c.stacks = {{1, 2, 3}, {4, 5}};
        const QVector<double> totals = c.sliceTotals();
        checkClose(totals[0], 6, 1e-9, QStringLiteral("堆疊時用各段的和，忽略 value"));
        checkClose(totals[1], 9, 1e-9, QStringLiteral("每一格的段數可以不一樣"));
    }
    {
        // 負值與 NaN 的段不計入
        Coxcomb c;
        c.slices = {{1, QString(), QColor(), 0, QString()}};
        c.stacks = {{5, -2, std::numeric_limits<double>::quiet_NaN(), 3}};
        checkClose(c.sliceTotals().at(0), 8, 1e-9, QStringLiteral("負值與 NaN 的段不計入"));
    }
    {
        // 堆疊時圖例列的是「段」而不是「格」
        Coxcomb c;
        c.slices = {{1, QStringLiteral("格一"), QColor(), 0, QString()},
                    {1, QStringLiteral("格二"), QColor(), 0, QString()}};
        c.stacks = {{1, 1}, {1, 1}};
        c.stackLabels = {QStringLiteral("段 A"), QStringLiteral("段 B")};
        const QVector<LegendItem> items = c.legendItems();
        check(items.size() == 2, QStringLiteral("圖例有兩項"));
        check(items[0].text == QStringLiteral("段 A"),
              QStringLiteral("堆疊時圖例列的是段，不是格"), items[0].text);
    }

    {
        // 每塊自己的半徑：比整體半徑大的那幾塊要把範圍撐開，不然會被切掉
        Pie pie;
        pie.radius = 1.0;
        pie.slices = {{1, QString(), QColor(), 0, QString()},
                      {1, QString(), QColor(), 0, QString()}};
        const double plain = pie.axisLimits().x.max;

        pie.slices[1].radiusFraction = 1.5;
        const double bigger = pie.axisLimits().x.max;
        checkClose(bigger, plain * 1.5, 1e-9,
                   QStringLiteral("有一塊半徑 1.5 倍時範圍跟著放大 1.5 倍"));

        // 0 代表「沿用整體半徑」，不是「半徑為 0」
        pie.slices[1].radiusFraction = 0;
        checkClose(pie.axisLimits().x.max, plain, 1e-9,
                   QStringLiteral("radiusFraction = 0 代表沿用整體半徑"));
    }

    {
        // 玫瑰圖中央的文字真的會畫出來。
        // 和 PolarAxis 的半徑標籤同一類風險：欄位加了卻忘了接上繪製。
        Plot plot;
        auto* rose = plot.addCoxcomb({1.0, 1.0, 1.0});
        rose->donutFraction = 0.4;
        rose->showLabels = false;
        rose->sliceBorder = LineStyle::none();
        for (PieSlice& sl : rose->slices) sl.fillColor = QColor(255, 255, 255);
        rose->centerLabel.text = QStringLiteral("XYZ");
        rose->centerLabel.font.color = QColor(0, 200, 0);
        rose->centerLabel.font.size = 20;
        plot.setAxisLimits(-1.3, 1.3, -1.3, 1.3);

        auto greenPixels = [](const QImage& img) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (c.green() > 150 && c.red() < 110 && c.blue() < 110) ++n;
                }
            return n;
        };

        check(greenPixels(plot.renderToImage(500, 500, 1.0)) > 20,
              QStringLiteral("玫瑰圖的中心文字有畫出來"));

        rose->centerLabel.text.clear();
        check(greenPixels(plot.renderToImage(500, 500, 1.0)) == 0,
              QStringLiteral("中心文字清空就不畫"));
    }

    {
        // 半徑標籤真的會畫出來。
        //
        // 這一項是補的：ringLabelAngle / showRingLabels 曾經只剩宣告、
        // 沒有任何程式用到，而既有的範例都把 showRingLabels 關著，
        // 所以漏掉之後完全沒人發現。
        Plot plot;
        auto* ax = plot.addPolarAxis(1.0, 4);
        ax->ringRadii = {0.5, 1.0};
        ax->ringLabels = {QStringLiteral("AAA"), QStringLiteral("BBB")};
        ax->showRingLabels = true;
        // 放射線的標籤關掉，確認半徑標籤不受它影響 ——
        // 兩者共用同一個字型的話，這裡就會一起消失
        ax->labelFont.color = QColor(0, 0, 0, 0);
        ax->ringLabelFont.color = QColor(255, 0, 0);
        ax->spokeLine = LineStyle::none();
        ax->ringLine = LineStyle::none();
        plot.setAxisLimits(-1.3, 1.3, -1.3, 1.3);

        const QImage img = plot.renderToImage(500, 500, 1.0);
        int redPixels = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 180 && c.green() < 90 && c.blue() < 90) ++redPixels;
            }
        check(redPixels > 20, QStringLiteral("半徑標籤有畫出來（放射線標籤關著也一樣）"),
              QStringLiteral("紅色像素 %1").arg(redPixels));

        ax->showRingLabels = false;
        const QImage img2 = plot.renderToImage(500, 500, 1.0);
        int redPixels2 = 0;
        for (int y = 0; y < img2.height(); ++y)
            for (int x = 0; x < img2.width(); ++x) {
                const QColor c = img2.pixelColor(x, y);
                if (c.red() > 180 && c.green() < 90 && c.blue() < 90) ++redPixels2;
            }
        check(redPixels2 == 0, QStringLiteral("showRingLabels 關掉就不畫"),
              QStringLiteral("紅色像素 %1").arg(redPixels2));
    }
}

void testFlowCharts() {
    out() << "桑基圖與弦圖\n";

    {
        // 節點高度取進出的較大值。中間的節點常常不平衡（有損耗、有新增），
        // 取小的那邊會讓連線擠不下、互相重疊。
        Sankey sk;
        sk.nodes = {{QStringLiteral("a"), 0, QColor()},
                    {QStringLiteral("mid"), 1, QColor()},
                    {QStringLiteral("b"), 2, QColor()}};
        sk.links = {{0, 1, 100, QColor()}, {1, 2, 60, QColor()}};
        const QVector<double> v = sk.nodeValues();
        checkClose(v[0], 100, 1e-9, QStringLiteral("來源節點用流出量"));
        checkClose(v[1], 100, 1e-9, QStringLiteral("中間節點取進出的較大值（進 100 出 60）"));
        checkClose(v[2], 60, 1e-9, QStringLiteral("終點節點用流入量"));
        check(sk.layerCount() == 3, QStringLiteral("層數取最大的 layer + 1"));
    }

    {
        // 連線的配置順序：同一個來源流出去的幾條，要依「對端節點的高度」排序。
        // 這裡刻意把 links 的順序寫反，確認版面會自己排好。
        //
        // 三個目標節點在第 1 層，依序由上往下是 t0、t1、t2。
        // 來源只有一個，它流出去的三條線在來源端的上緣，應該是
        // 「對應 t0 的最高、對應 t2 的最低」。
        Sankey sk;
        sk.nodes = {{QStringLiteral("src"), 0, QColor()},
                    {QStringLiteral("t0"), 1, QColor()},
                    {QStringLiteral("t1"), 1, QColor()},
                    {QStringLiteral("t2"), 1, QColor()}};
        // 故意倒著寫：先 t2、再 t1、最後 t0
        sk.links = {{0, 3, 10, QColor()}, {0, 2, 10, QColor()}, {0, 1, 10, QColor()}};
        sk.extent = CoordinateRect(0, 10, 0, 10);

        const Sankey::Layout lay = sk.computeLayout();
        check(lay.scale > 0, QStringLiteral("版面算得出來"));
        // 目標節點由上往下：t0 在最上面
        check(lay.boxes[1].top > lay.boxes[2].top, QStringLiteral("t0 排在 t1 上面"));
        check(lay.boxes[2].top > lay.boxes[3].top, QStringLiteral("t1 排在 t2 上面"));

        // links[2] 連到 t0、links[1] 連到 t1、links[0] 連到 t2
        check(lay.sourceTop[2] > lay.sourceTop[1],
              QStringLiteral("來源端：接 t0 的那條排在接 t1 的上面"));
        check(lay.sourceTop[1] > lay.sourceTop[0],
              QStringLiteral("來源端：接 t1 的那條排在接 t2 的上面"));
        // 沒有排序的話會照 links 的順序配置，第 0 條（接 t2）會跑到最上面
        check(lay.sourceTop[0] < lay.sourceTop[2],
              QStringLiteral("沒有被 links 原始順序牽著走"));
    }
    {
        // 無效的連線（索引越界、值非正）要被跳過，不能把後面的配置擠掉
        Sankey sk;
        sk.nodes = {{QStringLiteral("a"), 0, QColor()}, {QStringLiteral("b"), 1, QColor()}};
        sk.links = {{0, 5, 10, QColor()},    // 目標越界
                    {0, 1, -3, QColor()},    // 負值
                    {0, 1, 10, QColor()}};
        const Sankey::Layout lay = sk.computeLayout();
        check(!lay.drawn[0] && !lay.drawn[1] && lay.drawn[2],
              QStringLiteral("只有合法的那一條會畫"));
    }

    {
        // 弦圖：每個節點的總量是該列加該行，對角線只算一次
        Chord c;
        c.matrix = {{5, 10}, {3, 0}};
        check(c.isValid(), QStringLiteral("2×2 是合法的方陣"));
        const QVector<double> totals = c.nodeTotals();
        // 節點 0：流出 5（自己）+ 10（給 1），流入 3（來自 1）→ 18
        checkClose(totals[0], 18, 1e-9, QStringLiteral("節點 0 的總量（對角線只算一次）"));
        // 節點 1：流出 3，流入 10 → 13
        checkClose(totals[1], 13, 1e-9, QStringLiteral("節點 1 的總量"));
    }
    {
        Chord bad;
        bad.matrix = {{1, 2, 3}, {4, 5, 6}};  // 2×3，不是方陣
        check(!bad.isValid(), QStringLiteral("非方陣不合法"));
        check(!bad.axisLimits().hasData(), QStringLiteral("不合法時不回報範圍"));

        Chord empty;
        check(!empty.isValid(), QStringLiteral("空矩陣不合法"));
    }
}

void testBubbleGanttVenn() {
    out() << "氣泡、甘特與文氏\n";

    {
        BubbleChart b;
        b.bubbles = BubbleChart::fromArrays({0, 1, 2}, {0, 0, 0}, {10, 20, 30},
                                            Palette(Palette::Category10));
        b.minDiameterPx = 20;
        b.maxDiameterPx = 60;

        const CoordinateRange r = b.sizeRange();
        checkClose(r.min, 10, 1e-9, QStringLiteral("sizeRange 的下界"));
        checkClose(r.max, 30, 1e-9, QStringLiteral("sizeRange 的上界"));

        // 兩端一定落在設定的直徑上
        b.areaProportional = true;
        checkClose(b.diameterFor(10), 20, 1e-9, QStringLiteral("最小的 size 畫成 minDiameterPx"));
        checkClose(b.diameterFor(30), 60, 1e-9, QStringLiteral("最大的 size 畫成 maxDiameterPx"));

        // 面積正比：中間那個 size 的「面積」要落在兩端面積的中點，
        // 而不是直徑的中點。直徑的中點會是 40，面積的中點是 sqrt((400+3600)/2) ≈ 44.7。
        const double mid = b.diameterFor(20);
        const double areaMid = std::sqrt((20.0 * 20.0 + 60.0 * 60.0) / 2);
        checkClose(mid, areaMid, 1e-9, QStringLiteral("面積正比時中間值落在面積的中點"));
        check(std::abs(mid - 40.0) > 1.0,
              QStringLiteral("面積正比不等於直徑的中點"),
              QStringLiteral("直徑 %1 vs 線性 40").arg(mid, 0, 'f', 2));

        // 關掉就是直徑正比，中間值剛好是 40
        b.areaProportional = false;
        checkClose(b.diameterFor(20), 40, 1e-9, QStringLiteral("直徑正比時中間值就是直徑的中點"));

        check(b.axisLimits().hasData(), QStringLiteral("氣泡圖回報範圍"));
        checkClose(b.pointAt(1).x, 1.0, 1e-9, QStringLiteral("pointAt 取得第二顆的位置"));
    }
    {
        // 全部一樣大時不能除以零，要退回中間的直徑
        BubbleChart b;
        b.bubbles = BubbleChart::fromArrays({0, 1}, {0, 0}, {7, 7},
                                            Palette(Palette::Category10));
        b.minDiameterPx = 20;
        b.maxDiameterPx = 60;
        checkClose(b.diameterFor(7), 40, 1e-9, QStringLiteral("size 全部相同時取中間的直徑"));
    }
    {
        // 命中測試要在像素空間算：兩軸數量級差很多時，資料距離會被大的那軸主導
        BubbleChart b;
        b.bubbles = BubbleChart::fromArrays({0, 1000}, {0, 1}, {1, 1},
                                            Palette(Palette::Category10));
        Transform t;
        t.rect = PixelRect(0, 100, 0, 100);
        t.xr = CoordinateRange(0, 1000);
        t.yr = CoordinateRange(0, 1);
        const HitResult h = b.hitTest({950, 0.95}, t);
        check(h.isHit() && h.index == 1, QStringLiteral("命中靠近游標的那一顆"));
    }

    {
        Gantt g;
        g.tasks = {{QStringLiteral("a"), 0, 5, 0, 0.5, QColor(), {}},
                   {QStringLiteral("b"), 5, 12, 1, 0.0, QColor(), {0}},
                   {QStringLiteral("c"), 3, 9, 1, 0.0, QColor(), {}}};
        check(g.rowCount() == 2, QStringLiteral("列數取最大的 row + 1"),
              QStringLiteral("實際 %1").arg(g.rowCount()));
        const AxisLimits lim = g.axisLimits();
        checkClose(lim.x.min, 0, 1e-9, QStringLiteral("甘特圖的 X 下界是最早的開始"));
        checkClose(lim.x.max, 12, 1e-9, QStringLiteral("甘特圖的 X 上界是最晚的結束"));
        checkClose(lim.y.min, -g.barHeight / 2, 1e-9, QStringLiteral("第一列的下緣"));
    }

    {
        Venn v;
        v.sets = {{QStringLiteral("A"), QColor()}, {QStringLiteral("B"), QColor()}};
        v.radius = 1.0;
        const QVector<Coordinates> cs = v.circleCenters();
        check(cs.size() == 2, QStringLiteral("兩個集合兩個圓"));
        checkClose(cs[0].x, -0.5, 1e-9, QStringLiteral("左圓的圓心"));
        checkClose(cs[1].x, 0.5, 1e-9, QStringLiteral("右圓的圓心"));
        // 交集的錨點在正中間（兩個圓都參與 = 全部參與）
        const Coordinates both = v.regionAnchor(0b11);
        checkClose(both.x, 0, 1e-9, QStringLiteral("A∩B 的文字放在正中間"));
        // 只屬於 A 的區塊要被推離 B
        check(v.regionAnchor(0b01).x < cs[0].x,
              QStringLiteral("只有 A 的區塊被推離另一個圓"));
    }
    {
        Venn v;
        v.sets = {{QStringLiteral("A"), QColor()}, {QStringLiteral("B"), QColor()},
                  {QStringLiteral("C"), QColor()}};
        const QVector<Coordinates> cs = v.circleCenters();
        check(cs.size() == 3, QStringLiteral("三個集合三個圓"));
        // 正三角形：兩兩的圓心距都一樣
        const double d01 = std::hypot(cs[0].x - cs[1].x, cs[0].y - cs[1].y);
        const double d12 = std::hypot(cs[1].x - cs[2].x, cs[1].y - cs[2].y);
        const double d02 = std::hypot(cs[0].x - cs[2].x, cs[0].y - cs[2].y);
        checkClose(d01, d12, 1e-9, QStringLiteral("三個圓排成正三角形（01 vs 12）"));
        checkClose(d01, d02, 1e-9, QStringLiteral("三個圓排成正三角形（01 vs 02）"));
        const Coordinates all = v.regionAnchor(0b111);
        checkClose(all.x, 0, 1e-9, QStringLiteral("三者交集放在正中間 (x)"));
        checkClose(all.y, 0, 1e-9, QStringLiteral("三者交集放在正中間 (y)"));
    }
}

void testSunburstAndPie() {
    out() << "旭日圖與圓餅標籤\n";

    {
        // 父節點自己填的值要被忽略，改用子節點的總和 ——
        // 不然同一圈的角度加起來不等於整圈
        SunburstNode root;
        root.label = QStringLiteral("root");
        root.value = 999;  // 故意填一個對不上的數字
        root.children = {{QStringLiteral("a"), 3, QColor(), {}},
                         {QStringLiteral("b"), 7, QColor(), {}}};
        checkClose(root.total(), 10, 1e-9, QStringLiteral("父節點用子節點的總和，不用自己的值"));
        check(root.depth() == 2, QStringLiteral("兩層"));
    }
    {
        // 葉節點才看自己的 value
        SunburstNode leaf;
        leaf.value = 5;
        checkClose(leaf.total(), 5, 1e-9, QStringLiteral("葉節點用自己的 value"));
        check(leaf.depth() == 1, QStringLiteral("葉節點算一層"));
        leaf.value = -3;
        checkClose(leaf.total(), 0, 1e-9, QStringLiteral("負值當成 0"));
    }
    {
        // 各支深淺不同時，整體層數取最深的那一支
        Sunburst sb;
        sb.roots = {
            {QStringLiteral("shallow"), 4, QColor(), {}},
            {QStringLiteral("deep"), 0, QColor(),
             {{QStringLiteral("x"), 0, QColor(), {{QStringLiteral("y"), 2, QColor(), {}}}}}},
        };
        check(sb.depth() == 3, QStringLiteral("層數取最深的那一支"),
              QStringLiteral("實際 %1").arg(sb.depth()));
        check(sb.axisLimits().hasData(), QStringLiteral("有節點時回報範圍"));

        Sunburst empty;
        check(!empty.axisLimits().hasData(), QStringLiteral("空的旭日圖不回報範圍"));
    }

    {
        // 圓餅的標籤文字：LabelAndPercent 要能正確算出百分比
        Pie pie;
        pie.slices = {{25, QStringLiteral("a"), QColor(), 0, QString()},
                      {75, QStringLiteral("b"), QColor(), 0, QString()}};
        pie.labelMode = Pie::LabelMode::Percent;
        const QStringList texts = pie.sliceTexts();
        check(texts.size() == 2, QStringLiteral("每塊各一段文字"));
        check(texts[0] == QStringLiteral("25.0%"), QStringLiteral("百分比算得對"),
              texts[0]);
        check(texts[1] == QStringLiteral("75.0%"), QStringLiteral("第二塊的百分比"), texts[1]);

        pie.labelMode = Pie::LabelMode::None;
        check(pie.sliceTexts().at(0).isEmpty(), QStringLiteral("None 時不產生文字"));
    }
    {
        // 開了引線標籤之後，範圍要留得比圓本身大很多（引線 + 文字都在圓外）
        Pie pie;
        pie.slices = {{1, QStringLiteral("a"), QColor(), 0, QString()}};
        pie.radius = 1.0;
        const double plain = pie.axisLimits().x.max;
        pie.useLeaderLines = true;
        pie.leaderLengthFraction = 0.4;
        check(pie.axisLimits().x.max > plain * 1.3,
              QStringLiteral("引線標籤會把範圍撐大"),
              QStringLiteral("%1 → %2").arg(plain, 0, 'f', 2)
                  .arg(pie.axisLimits().x.max, 0, 'f', 2));
    }
    {
        // 壓扁之後垂直範圍要跟著縮，否則圖的上下會空一大片
        Pie pie;
        pie.slices = {{1, QStringLiteral("a"), QColor(), 0, QString()}};
        pie.tiltFraction = 0.5;
        const AxisLimits lim = pie.axisLimits();
        check(lim.y.span() < lim.x.span(), QStringLiteral("壓扁的圓餅垂直範圍比較小"),
              QStringLiteral("x=%1 y=%2").arg(lim.x.span(), 0, 'f', 2)
                  .arg(lim.y.span(), 0, 'f', 2));
    }
}

void testBusinessCharts() {
    out() << "商務圖表\n";

    {
        // 瀑布圖的累計。isTotal 只改「從哪裡畫」，不改算術 ——
        // 兩者綁在一起的話，期初餘額（既是總計、又要讓累計從它開始）就寫不出來。
        // 這一版就是先寫錯才改的：期初那根被畫成了 0。
        Waterfall w;
        w.steps = {{120, QStringLiteral("期初"), true, QColor()},
                   {45, QStringLiteral("新客戶"), false, QColor()},
                   {18, QStringLiteral("加購"), false, QColor()},
                   {-22, QStringLiteral("流失"), false, QColor()},
                   {-13, QStringLiteral("降級"), false, QColor()},
                   {0, QStringLiteral("期末"), true, QColor()}};
        const QVector<double> cum = w.cumulative();
        check(cum.size() == 6, QStringLiteral("累計的長度等於步驟數"));
        checkClose(cum[0], 120, 1e-9, QStringLiteral("期初就讓累計從 120 開始"));
        checkClose(cum[1], 165, 1e-9, QStringLiteral("累計 +45"));
        checkClose(cum[2], 183, 1e-9, QStringLiteral("累計 +18"));
        checkClose(cum[3], 161, 1e-9, QStringLiteral("累計 -22"));
        checkClose(cum[4], 148, 1e-9, QStringLiteral("累計 -13"));
        checkClose(cum[5], 148, 1e-9, QStringLiteral("期末（delta=0）不改變累計"));

        // 範圍一定要含到 0，否則讀者看不出長條是從哪裡起算的
        const AxisLimits lim = w.axisLimits();
        checkClose(lim.y.min, 0, 1e-9, QStringLiteral("瀑布圖的 Y 下界含 0"));
        checkClose(lim.y.max, 183, 1e-9, QStringLiteral("瀑布圖的 Y 上界是最高的累計"));
    }
    {
        // 中途小計：delta = 0 就只是把累計畫出來，不加東西
        Waterfall w;
        w.steps = {{10, QString(), false, QColor()},
                   {5, QString(), false, QColor()},
                   {0, QStringLiteral("小計"), true, QColor()},
                   {-3, QString(), false, QColor()}};
        const QVector<double> cum = w.cumulative();
        checkClose(cum[2], 15, 1e-9, QStringLiteral("中途小計等於當下的累計"));
        checkClose(cum[3], 12, 1e-9, QStringLiteral("小計之後繼續累加"));
    }

    {
        // 漏斗佔的就是 extent 那塊，不多也不少
        Funnel f;
        f.stages = Funnel::fromValues({100, 60, 30}, {QStringLiteral("a"), QStringLiteral("b"),
                                                      QStringLiteral("c")},
                                      Palette(Palette::Category10));
        f.extent = CoordinateRect(2, 8, 1, 9);
        const AxisLimits lim = f.axisLimits();
        checkClose(lim.x.min, 2, 1e-9, QStringLiteral("漏斗的 X 範圍就是 extent"));
        checkClose(lim.x.max, 8, 1e-9, QStringLiteral("漏斗的 X 上界"));
        checkClose(lim.y.min, 1, 1e-9, QStringLiteral("漏斗的 Y 下界"));
        checkClose(lim.y.max, 9, 1e-9, QStringLiteral("漏斗的 Y 上界"));
        check(f.legendItems().size() == 3, QStringLiteral("每段各出一個圖例項"));
    }
    {
        // 沒有段落時不該回報任何範圍（空圖元不應該撐開座標軸）
        Funnel f;
        check(!f.axisLimits().hasData(), QStringLiteral("空漏斗不回報範圍"));
    }

    {
        // 靶心圖的 X 要含到分級的最大值，否則最外圈的背景帶會被切掉
        Bullet b;
        b.rows = {{QStringLiteral("a"), 70, 80, {50, 75, 100}, QColor()},
                  {QStringLiteral("b"), 30, 40, {20, 35, 60}, QColor()}};
        b.rowHeight = 0.7;
        const AxisLimits lim = b.axisLimits();
        checkClose(lim.x.min, 0, 1e-9, QStringLiteral("靶心圖從 0 起算"));
        checkClose(lim.x.max, 100, 1e-9, QStringLiteral("靶心圖的 X 含到最大分級"));
        checkClose(lim.y.min, -0.35, 1e-9, QStringLiteral("第一列的下緣"));
        checkClose(lim.y.max, 1.35, 1e-9, QStringLiteral("最後一列的上緣"));
    }
}

void testBars3D() {
    out() << "立體長條圖\n";

    {
        Bars3D b;
        b.bars = Bars3D::fromValues({10, 20, 30}, Palette(Palette::Category10));
        check(b.bars.size() == 3, QStringLiteral("fromValues 長度正確"));
        checkClose(b.bars[0].position, 0, 1e-9, QStringLiteral("第一根在位置 0"));
        checkClose(b.bars[2].position, 2, 1e-9, QStringLiteral("第三根在位置 2"));
        check(b.bars[0].fillColor != b.bars[1].fillColor,
              QStringLiteral("相鄰兩根取到不同的調色盤顏色"));
    }
    {
        // 長條的「寬度」要算進 X 範圍，不然最左邊那根會被切掉一半
        Bars3D b;
        b.bars = Bars3D::fromValues({10}, Palette(Palette::Category10));
        b.bars[0].size = 0.8;
        b.depthPadFraction = 0;
        const AxisLimits lim = b.axisLimits();
        checkClose(lim.x.min, -0.4, 1e-9, QStringLiteral("X 下界含長條的半個寬度"));
        checkClose(lim.x.max, 0.4, 1e-9, QStringLiteral("X 上界含長條的半個寬度"));
    }
    {
        // includeZero：長條圖不從 0 起算會誤讀
        Bars3D b;
        b.bars = Bars3D::fromValues({50, 60}, Palette(Palette::Category10));
        b.depthPadFraction = 0;
        b.includeZero = true;
        checkClose(b.axisLimits().y.min, 0, 1e-9, QStringLiteral("includeZero 把 0 含進來"));
        b.includeZero = false;
        checkClose(b.axisLimits().y.min, 0, 1e-9,
                   QStringLiteral("valueBase 預設是 0，所以關掉也還是 0"));
    }
    {
        // 餘裕：往右上各加資料範圍的一個比例
        Bars3D b;
        b.bars = Bars3D::fromValues({0, 100}, Palette(Palette::Category10));
        b.bars[0].size = 0;
        b.bars[1].size = 0;
        b.depthPadFraction = 0.2;
        const AxisLimits lim = b.axisLimits();
        // X 原本是 0..1，加兩成 → 上界 1.2；Y 原本 0..100 → 上界 120
        checkClose(lim.x.max, 1.2, 1e-9, QStringLiteral("X 上界留了兩成餘裕"));
        checkClose(lim.y.max, 120, 1e-9, QStringLiteral("Y 上界留了兩成餘裕"));
        checkClose(lim.x.min, 0, 1e-9, QStringLiteral("餘裕只加在擠出的那一側"));
        checkClose(lim.y.min, 0, 1e-9, QStringLiteral("下界不動"));
    }
    {
        // 負值的長條往下長，範圍要跟著含進去
        Bars3D b;
        b.bars = Bars3D::fromValues({-30, 40}, Palette(Palette::Category10));
        b.depthPadFraction = 0;
        const AxisLimits lim = b.axisLimits();
        checkClose(lim.y.min, -30, 1e-9, QStringLiteral("負值長條的 Y 下界"));
        checkClose(lim.y.max, 40, 1e-9, QStringLiteral("負值長條的 Y 上界"));
    }
}

void testGridFillAndRotation() {
    out() << "格線填色與刻度旋轉\n";

    /// 在指定的資料 x 上取一個像素的顏色。
    auto sampleAt = [](double xMin, double xMax, double dataX) {
        Plot p;
        p.style.dataBackground = QColor(255, 255, 255);
        p.grid.majorLine = LineStyle::none();
        p.grid.minorLine = LineStyle::none();
        p.bottomAxis()->gridFillColor1 = QColor(255, 0, 0);
        p.bottomAxis()->gridFillColor2 = QColor(0, 0, 255);
        p.setAxisLimits(xMin, xMax, 0, 1);
        const QImage img = p.renderToImage(600, 400, 1.0);
        const PixelRect d = p.lastDataRect();
        const Transform t = p.transformFor(p.bottomAxis(), p.leftAxis());
        const int px = static_cast<int>(std::round(t.px(dataX)));
        const int py = static_cast<int>(std::round((d.top + d.bottom) / 2));
        return img.pixelColor(px, py);
    };

    {
        // 同一個資料位置，在兩個不同的視野下必須是同一個顏色。
        // 條帶的顏色若是看「視野內第幾條」決定的，平移時整排會忽然換色。
        const QColor a = sampleAt(0, 10, 1.0);
        const QColor b = sampleAt(-2, 12, 1.0);
        const QColor c = sampleAt(0.5, 10.5, 1.0);
        check(a == b && b == c, QStringLiteral("條帶顏色只看資料座標，不隨視野改變"),
              QStringLiteral("%1 / %2 / %3").arg(a.name(), b.name(), c.name()));
    }
    {
        // 相鄰兩格要真的交替
        const QColor first = sampleAt(0, 10, 1.0);
        const QColor second = sampleAt(0, 10, 3.0);
        check(first != second, QStringLiteral("相鄰兩條帶顏色不同"),
              QStringLiteral("%1 vs %2").arg(first.name(), second.name()));
        check(sampleAt(0, 10, 5.0) == first, QStringLiteral("隔一條帶顏色回到原本的"));
    }

    {
        // 旋轉之後，長標籤往下伸的距離遠超過一行字高，軸必須變厚
        Plot p;
        auto ticks = std::make_shared<ManualTickGenerator>();
        ticks->ticks = {{0, QStringLiteral("First Long Title")},
                        {1, QStringLiteral("Second Long Title")},
                        {2, QStringLiteral("Third Long Title")}};
        p.bottomAxis()->tickGenerator = ticks;
        p.setAxisLimits(-0.5, 2.5, 0, 1);
        p.renderToImage(600, 400, 1.0);

        const double flat = p.bottomAxis()->measureThickness();
        p.bottomAxis()->tickLabelRotation = -45;
        const double rotated = p.bottomAxis()->measureThickness();
        check(rotated > flat * 2, QStringLiteral("旋轉 45 度後水平軸明顯變厚"),
              QStringLiteral("%1 → %2 px").arg(flat, 0, 'f', 1).arg(rotated, 0, 'f', 1));

        // 轉 90 度時外接框的高就等於最長標籤的寬
        p.bottomAxis()->tickLabelRotation = -90;
        check(p.bottomAxis()->measureThickness() > rotated,
              QStringLiteral("轉 90 度比 45 度更厚"));
    }
    {
        // 垂直軸相反：轉起來之後反而變窄（最寬的標籤被立起來了）
        Plot p;
        auto ticks = std::make_shared<ManualTickGenerator>();
        ticks->ticks = {{0, QStringLiteral("A Very Long Tick Label")},
                        {1, QStringLiteral("Another Long One")}};
        p.leftAxis()->tickGenerator = ticks;
        p.setAxisLimits(0, 1, -0.5, 1.5);
        p.renderToImage(600, 400, 1.0);

        const double flat = p.leftAxis()->measureThickness();
        p.leftAxis()->tickLabelRotation = -90;
        check(p.leftAxis()->measureThickness() < flat,
              QStringLiteral("垂直軸的標籤轉 90 度之後變窄"));
    }

    {
        // 逐軸覆蓋：optional 沒設就沿用 Plot 的，設了（即使是透明）就以自己的為準
        Plot p;
        check(!p.leftAxis()->gridMajorLine.has_value(),
              QStringLiteral("預設沒有逐軸的格線覆蓋"));
        p.leftAxis()->gridMajorLine = LineStyle::none();
        check(p.leftAxis()->gridMajorLine.has_value(),
              QStringLiteral("設成透明色算是「有覆蓋」，不是「沿用」"));
    }
}

void testSectorsAndPolar() {
    out() << "扇形、圓弧與極座標\n";

    {
        // 第一象限的四分之一圓：端點就是極值
        Sector sec;
        sec.setRadius(1.0);
        sec.startAngle = 0;
        sec.sweepAngle = 90;
        const AxisLimits lim = sec.axisLimits();
        checkClose(lim.x.min, 0, 1e-9, QStringLiteral("四分之一扇形 x 下界"));
        checkClose(lim.x.max, 1, 1e-9, QStringLiteral("四分之一扇形 x 上界"));
        checkClose(lim.y.min, 0, 1e-9, QStringLiteral("四分之一扇形 y 下界"));
        checkClose(lim.y.max, 1, 1e-9, QStringLiteral("四分之一扇形 y 上界"));
    }
    {
        // 跨過 0° 的弧：最右邊是 0° 那一點，不是任何一個端點。
        // 只取端點的話 x 上界會算成 cos(45°) ≈ 0.707，弧的右緣就被切掉。
        Arc arc;
        arc.setRadius(1.0);
        arc.startAngle = -45;
        arc.sweepAngle = 90;
        const AxisLimits lim = arc.axisLimits();
        checkClose(lim.x.max, 1.0, 1e-9, QStringLiteral("跨過 0° 的弧，x 上界在 0° 而非端點"));
        checkClose(lim.y.max, std::sin(45 * kPi / 180.0), 1e-9,
                   QStringLiteral("跨過 0° 的弧，y 上界在端點"));
    }
    {
        // 橢圓弧：兩個方向的半徑分開算
        Arc arc;
        arc.radiusX = 3;
        arc.radiusY = 2;
        arc.startAngle = 0;
        arc.sweepAngle = 360;
        const AxisLimits lim = arc.axisLimits();
        checkClose(lim.x.max, 3, 1e-9, QStringLiteral("整圈橢圓弧 x 上界 = radiusX"));
        checkClose(lim.y.max, 2, 1e-9, QStringLiteral("整圈橢圓弧 y 上界 = radiusY"));
        checkClose(lim.x.min, -3, 1e-9, QStringLiteral("整圈橢圓弧 x 下界"));
    }
    {
        // 環狀扇形不該把圓心算進範圍 —— 中間是挖空的
        Sector ring;
        ring.setRadius(2.0);
        ring.setInnerRadius(1.0);
        ring.startAngle = 0;
        ring.sweepAngle = 90;
        const AxisLimits lim = ring.axisLimits();
        checkClose(lim.x.min, 0, 1e-9, QStringLiteral("環狀扇形 x 下界（外弧端點）"));
        checkClose(lim.x.max, 2, 1e-9, QStringLiteral("環狀扇形 x 上界"));
    }

    {
        // 預設：0° 在正上方、順時針
        PolarAxis ax;
        ax.radius = 1.0;
        ax.startAngle = 90;
        ax.clockwise = true;
        const Coordinates a = ax.coordinates(1.0, 0);
        checkClose(a.x, 0, 1e-9, QStringLiteral("預設極座標 0° 在正上方 (x)"));
        checkClose(a.y, 1, 1e-9, QStringLiteral("預設極座標 0° 在正上方 (y)"));
        const Coordinates b = ax.coordinates(1.0, 90);
        checkClose(b.x, 1, 1e-9, QStringLiteral("順時針時 90° 轉到右邊"));
        checkClose(b.y, 0, 1e-9, QStringLiteral("順時針時 90° 的 y"));
    }
    {
        // 數學慣例：0° 在右、逆時針
        PolarAxis ax;
        ax.startAngle = 0;
        ax.clockwise = false;
        const Coordinates b = ax.coordinates(1.0, 90);
        checkClose(b.x, 0, 1e-9, QStringLiteral("逆時針時 90° 轉到上面 (x)"));
        checkClose(b.y, 1, 1e-9, QStringLiteral("逆時針時 90° 轉到上面 (y)"));
    }
    {
        // 圓心平移要跟著算進去
        PolarAxis ax;
        ax.center = {5, -3};
        ax.startAngle = 0;
        ax.clockwise = false;
        const Coordinates c = ax.coordinates(2.0, 0);
        checkClose(c.x, 7, 1e-9, QStringLiteral("coordinates 有算進 center (x)"));
        checkClose(c.y, -3, 1e-9, QStringLiteral("coordinates 有算進 center (y)"));
    }
    {
        // 自動產生的放射線：角度等分，而且就是能餵回 coordinates() 的那個值
        PolarAxis ax;
        ax.spokeCount = 4;
        const QVector<PolarSpoke> sp = ax.resolvedSpokes();
        check(sp.size() == 4, QStringLiteral("spokeCount 決定放射線數量"));
        checkClose(sp[1].angle, 90, 1e-9, QStringLiteral("四條放射線的第二條在 90°"));

        ax.spokeLabels = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
        check(ax.resolvedSpokes().size() == 3,
              QStringLiteral("spokeLabels 的數量會蓋過 spokeCount"));
    }
}

void testTooltipTail() {
    out() << "氣泡標註的尾巴\n";

    // 刻意用很扁的框：pointAtPercent 的誤差就是在這種比例下爆開的
    const QRectF box(100, 100, 200, 40);
    const double shortSide = 40;

    /// 兩個根部點沿著矩形邊界相隔多遠（像素）。
    auto baseWidthOf = [&](const QPolygonF& tail) {
        const QPointF a = tail[0], b = tail[2];
        // 兩點一定在同一條邊或相鄰的邊上，沿邊界量就是「走過去」的距離
        return std::abs(a.x() - b.x()) + std::abs(a.y() - b.y());
    };

    auto onBorder = [&](QPointF q) {
        const double eps = 1e-6;
        const bool xIn = q.x() >= box.left() - eps && q.x() <= box.right() + eps;
        const bool yIn = q.y() >= box.top() - eps && q.y() <= box.bottom() + eps;
        const bool onX = std::abs(q.x() - box.left()) < eps || std::abs(q.x() - box.right()) < eps;
        const bool onY = std::abs(q.y() - box.top()) < eps || std::abs(q.y() - box.bottom()) < eps;
        return xIn && yIn && (onX || onY);
    };

    for (double w : {0.3, 0.5, 0.7, 1.0}) {
        // 正左方：根部整段落在左邊那條短邊上，所以寬度應該剛好是 w * 短邊
        const QPolygonF tail = Tooltip::tailPolygon(box, QPointF(-200, 120), w);
        check(tail.size() == 3, QStringLiteral("tailWidth=%1 有三個頂點").arg(w));
        checkClose(baseWidthOf(tail), w * shortSide, 1e-6,
                   QStringLiteral("tailWidth=%1 的根部寬度").arg(w));
        check(tail[1] == QPointF(-200, 120), QStringLiteral("tailWidth=%1 頂點就是 tip").arg(w));
        check(onBorder(tail[0]) && onBorder(tail[2]),
              QStringLiteral("tailWidth=%1 兩個根部點都在框線上").arg(w));
    }

    {
        // 繞一圈：每個方向都要長得出尾巴，根部永遠在框線上、頂點永遠是 tip
        int bad = 0;
        for (int deg = 0; deg < 360; deg += 5) {
            const double rad = deg * kPi / 180.0;
            const QPointF tip(box.center().x() + 400 * std::cos(rad),
                              box.center().y() + 400 * std::sin(rad));
            const QPolygonF tail = Tooltip::tailPolygon(box, tip, 0.5);
            if (tail.size() != 3 || !onBorder(tail[0]) || !onBorder(tail[2]) || tail[1] != tip)
                ++bad;
        }
        check(bad == 0, QStringLiteral("繞一圈 72 個方向都正常"),
              QStringLiteral("%1 個方向有問題").arg(bad));
    }

    {
        // tip 在框裡面就不該長尾巴
        const QPolygonF tail = Tooltip::tailPolygon(box, box.center(), 0.5);
        check(tail.isEmpty(), QStringLiteral("tip 在框內時不畫尾巴"));
    }
    {
        const QPolygonF tail = Tooltip::tailPolygon(box, QPointF(-200, 120), 0.0);
        check(tail.isEmpty(), QStringLiteral("tailWidth=0 不畫尾巴"));
    }
}

void testReset() {
    out() << "重設\n";
    {
        Plot p;
        const int defaults = p.axes().size();
        check(defaults == 4, QStringLiteral("預設四根軸"),
              QStringLiteral("實際 %1").arg(defaults));

        // 模擬「同一側兩根軸」那類範例
        p.addAxis(Edge::Right);
        p.addAxis(Edge::Right);
        p.addAxis(Edge::Top);
        check(p.axes().size() == defaults + 3, QStringLiteral("addAxis 之後軸會變多"),
              QStringLiteral("實際 %1").arg(p.axes().size()));

        p.reset();
        check(p.axes().size() == defaults, QStringLiteral("reset 之後多的軸被收回"),
              QStringLiteral("實際 %1").arg(p.axes().size()));
    }
    {
        // clear() 刻意不收軸 —— 它只負責清圖元。兩者的分工要維持住，
        // 不然「清掉資料但保留多軸版面」這種用法會壞掉。
        Plot p;
        p.addAxis(Edge::Right);
        p.addScatter({0, 1}, {0, 1});
        p.clear();
        check(p.plottables().isEmpty(), QStringLiteral("clear 清掉圖元"));
        check(p.axes().size() == 5, QStringLiteral("clear 不收軸（和 reset 分工）"),
              QStringLiteral("實際 %1").arg(p.axes().size()));
    }
    {
        Plot p;
        p.title.text = QStringLiteral("舊標題");
        p.legend.isVisible = true;
        p.equalAxisScale = true;
        p.leftAxis()->label.text = QStringLiteral("舊軸標題");
        p.leftAxis()->inverted = true;
        p.leftAxis()->locked = true;
        p.leftAxis()->setRange(5, 6);
        p.addScatter({0, 1}, {0, 1});

        p.reset();
        check(p.plottables().isEmpty(), QStringLiteral("reset 清掉圖元"));
        check(p.title.text.isEmpty(), QStringLiteral("reset 清掉標題"));
        check(!p.legend.isVisible, QStringLiteral("reset 關掉圖例"));
        check(!p.equalAxisScale, QStringLiteral("reset 關掉等比例軸"));
        check(p.leftAxis()->label.text.isEmpty(), QStringLiteral("reset 清掉軸標題"));
        check(!p.leftAxis()->inverted, QStringLiteral("reset 取消反向軸"));
        check(!p.leftAxis()->locked, QStringLiteral("reset 解除鎖定"));
        check(!p.leftAxis()->rangeWasSet, QStringLiteral("reset 之後 autoScale 又管得到"));

        // 真的能重新自動貼合
        p.addScatter({0, 1, 2}, {0, 10, 20});
        p.autoScale(0.0);
        checkClose(p.axisLimits().y.max, 20, 1e-9, QStringLiteral("reset 之後 autoScale 正常"));
    }
}

void testAutoScale() {
    out() << "自動範圍\n";
    {
        Plot p;
        p.addScatter({0, 1, 2}, {0, 10, 20});
        p.autoScale(0.0);
        checkClose(p.axisLimits().x.min, 0, 1e-9, QStringLiteral("autoScale 貼合資料"));
        checkClose(p.axisLimits().y.max, 20, 1e-9, QStringLiteral("autoScale 貼合資料（y）"));
    }
    {
        Plot p;
        p.addScatter({0, 1, 2}, {0, 10, 20});
        p.leftAxis()->setRange(-100, 100);
        p.autoScale(0.0);
        checkClose(p.leftAxis()->range.min, -100, 1e-9,
                   QStringLiteral("setRange 過的軸不被 autoScale 蓋掉"));
        checkClose(p.bottomAxis()->range.max, 2, 1e-9,
                   QStringLiteral("沒設過的軸照常貼合資料"));
    }
    {
        Plot p;
        p.addScatter({0, 1, 2}, {0, 10, 20});
        p.leftAxis()->setRange(-100, 100);
        p.autoScale(0.0, /*force=*/true);
        checkClose(p.leftAxis()->range.max, 20, 1e-9,
                   QStringLiteral("force = true 連設過的軸也重算"));
    }
    {
        Plot p;
        p.addScatter({0, 1, 2}, {0, 10, 20});
        p.leftAxis()->locked = true;
        const double before = p.leftAxis()->range.max;
        p.autoScale(0.0, true);
        checkClose(p.leftAxis()->range.max, before, 1e-9,
                   QStringLiteral("locked 的軸連 force 都不動"));
    }
    {
        // 多軸：autoScale 必須逐軸收集。把全部圖元混在一起算的話，
        // 右軸那組大數值會把左軸也一起撐大（成交量把價格壓成一條線就是這個 bug）。
        Plot p;
        p.addScatter({0, 1, 2}, {0, 10, 20});
        auto* right = p.addAxis(Edge::Right);
        auto* big = p.addScatter({0, 1, 2}, {0, 1e6, 2e6});
        big->yAxis = right;
        p.autoScale(0.0);
        checkClose(p.leftAxis()->range.max, 20, 1e-9,
                   QStringLiteral("左軸只看綁在它上面的圖元"));
        checkClose(right->range.max, 2e6, 1e-9, QStringLiteral("右軸貼合自己的資料"));
        checkClose(p.bottomAxis()->range.max, 2, 1e-9, QStringLiteral("X 軸不受影響"));
    }
}

// ───────────────────── 等值線 ─────────────────────

/// 常態分布與 Q-Q 圖。
void testNormalAndQQ() {
    out() << "常態分布與 Q-Q\n";

    // 教科書上的值。normalQuantile 若係數抄錯，這幾項會馬上爆掉。
    checkClose(normalQuantile(0.5), 0.0, 1e-12, QStringLiteral("Φ⁻¹(0.5) = 0"));
    checkClose(normalQuantile(0.975), 1.959963985, 1e-8, QStringLiteral("Φ⁻¹(0.975) = 1.95996"));
    checkClose(normalQuantile(0.025), -1.959963985, 1e-8, QStringLiteral("Φ⁻¹(0.025) 對稱"));
    checkClose(normalQuantile(0.99), 2.326347874, 1e-8, QStringLiteral("Φ⁻¹(0.99) = 2.32635"));
    checkClose(normalQuantile(0.001), -3.090232306, 1e-8, QStringLiteral("Φ⁻¹(0.001) = −3.09023"));
    // 尾端：低精度的逼近（1e-9）在這裡會明顯偏掉，AS241 不會
    checkClose(normalQuantile(1e-15), -7.941345326, 1e-6, QStringLiteral("極左尾仍然準"));

    checkClose(normalCdf(0), 0.5, 1e-12, QStringLiteral("Φ(0) = 0.5"));
    checkClose(normalCdf(1.959963985), 0.975, 1e-9, QStringLiteral("Φ(1.95996) = 0.975"));
    check(normalCdf(-8) > 0, QStringLiteral("左尾的 Φ 不會下溢成 0"),
          QStringLiteral("得到 %1").arg(normalCdf(-8), 0, 'g', 4));
    {
        // 來回轉一圈
        double worst = 0;
        for (double pp = 0.001; pp < 0.9995; pp += 0.001)
            worst = std::max(worst, std::abs(normalCdf(normalQuantile(pp)) - pp));
        check(worst < 1e-12, QStringLiteral("Φ 與 Φ⁻¹ 互為反函數"),
              QStringLiteral("最大誤差 %1").arg(worst, 0, 'g', 4));
    }

    {
        // 標準常態的分位數本身拿來做 Q-Q，點應該落在 y = x 上
        QVector<double> v;
        for (int i = 1; i <= 99; ++i) v.append(normalQuantile(i / 100.0));
        const QQResult qq = normalQQ(v);
        check(qq.sample.size() == 99, QStringLiteral("Q-Q 的點數等於樣本數"));
        checkClose(qq.referenceLine.slope, 1.0, 0.02,
                   QStringLiteral("常態樣本的參考線斜率接近 1"));
        checkClose(qq.referenceLine.intercept, 0.0, 0.02,
                   QStringLiteral("常態樣本的參考線截距接近 0"));
    }
    {
        // 參考線穿過四分位數，不是最小平方迴歸：加一個極端離群值不該把線拉歪
        QVector<double> v;
        for (int i = 1; i <= 99; ++i) v.append(normalQuantile(i / 100.0));
        const double before = normalQQ(v).referenceLine.slope;
        v.append(500.0);
        const double after = normalQQ(v).referenceLine.slope;
        check(std::abs(after - before) < 0.05,
              QStringLiteral("參考線不被離群值拉走"),
              QStringLiteral("%1 → %2").arg(before, 0, 'f', 4).arg(after, 0, 'f', 4));
    }
    {
        QVector<double> v{3, 1, 2};
        const QQResult qq = normalQQ(v);
        check(qq.sample == QVector<double>({1, 2, 3}), QStringLiteral("Q-Q 的樣本軸已排序"));
        check(qq.theoretical[0] < 0 && qq.theoretical[2] > 0,
              QStringLiteral("理論分位數左負右正"));
    }
    check(normalQQ({}).sample.isEmpty(), QStringLiteral("空樣本的 Q-Q 是空的"));
}

/// ROC 與 PR 曲線。
void testClassifierCurves() {
    out() << "ROC 與 PR 曲線\n";

    {
        // 完美分類器：所有陽性的分數都比陰性高
        const QVector<double> scores{0.9, 0.8, 0.7, 0.3, 0.2, 0.1};
        const QVector<bool> labels{true, true, true, false, false, false};
        const ClassifierCurve roc = rocCurve(scores, labels);
        checkClose(roc.area, 1.0, 1e-12, QStringLiteral("完美分類器的 AUC = 1"));
        check(roc.positives == 3 && roc.negatives == 3,
              QStringLiteral("陽性／陰性數算對"));
        const ClassifierCurve pr = prCurve(scores, labels);
        checkClose(pr.area, 1.0, 1e-12, QStringLiteral("完美分類器的 AP = 1"));
    }
    {
        // 反向分類器：AUC 應該是 0
        const QVector<double> scores{0.1, 0.2, 0.3, 0.7, 0.8, 0.9};
        const QVector<bool> labels{true, true, true, false, false, false};
        checkClose(rocCurve(scores, labels).area, 0.0, 1e-12,
                   QStringLiteral("完全反向的 AUC = 0"));
    }
    {
        // 全部同分：什麼都分不出來，曲線是對角線，AUC = 0.5
        const QVector<double> scores{0.5, 0.5, 0.5, 0.5};
        const QVector<bool> labels{true, false, true, false};
        const ClassifierCurve roc = rocCurve(scores, labels);
        checkClose(roc.area, 0.5, 1e-12, QStringLiteral("全部同分的 AUC = 0.5"));
        check(roc.xs.size() == 2, QStringLiteral("同分的一整批只落一個點"),
              QStringLiteral("點數 %1").arg(roc.xs.size()));
    }
    {
        // 同分處理是不是真的和輸入順序無關 —— 一次推進一個樣本的寫法會在這裡壞掉
        const QVector<double> scores{0.9, 0.5, 0.5, 0.5, 0.1};
        const QVector<bool> a{true, true, false, false, false};
        const QVector<bool> b{true, false, false, true, false};
        const double areaA = rocCurve(scores, a).area;
        const double areaB = rocCurve(scores, b).area;
        checkClose(areaA, areaB, 1e-12,
                   QStringLiteral("同分樣本的先後順序不影響 AUC"));

        // 手算驗證：3 個陰性、2 個陽性。門檻 0.9 → TPR 0.5、FPR 0；
        // 門檻 0.5（三個同分一起算）→ TPR 1、FPR 2/3；門檻 0.1 → 1, 1。
        // 梯形面積 = 0.5·(2/3)·(0.5+1) + (1/3)·1 = 0.5 + 1/3 = 5/6
        checkClose(areaA, 5.0 / 6.0, 1e-12, QStringLiteral("手算的 AUC 對得上"));
    }
    {
        // 曲線的端點
        const QVector<double> scores{0.9, 0.6, 0.4, 0.1};
        const QVector<bool> labels{true, false, true, false};
        const ClassifierCurve roc = rocCurve(scores, labels);
        checkClose(roc.xs.first(), 0.0, 1e-12, QStringLiteral("ROC 起點在 (0, 0)"));
        checkClose(roc.ys.first(), 0.0, 1e-12, QStringLiteral("ROC 起點的 TPR 為 0"));
        checkClose(roc.xs.last(), 1.0, 1e-12, QStringLiteral("ROC 終點 FPR 為 1"));
        checkClose(roc.ys.last(), 1.0, 1e-12, QStringLiteral("ROC 終點 TPR 為 1"));
        check(std::isinf(roc.thresholds.first()),
              QStringLiteral("第一個門檻是 infinity（全判為陰性）"));
    }
    {
        // PR 的 AP 不是梯形面積。這組資料兩者差得出來。
        const QVector<double> scores{0.9, 0.8, 0.7, 0.6};
        const QVector<bool> labels{true, false, true, false};
        const ClassifierCurve pr = prCurve(scores, labels);
        // 召回 0.5 時精確率 1，召回 1 時精確率 2/3
        // AP = 0.5·1 + 0.5·(2/3) = 5/6
        checkClose(pr.area, 5.0 / 6.0, 1e-12, QStringLiteral("AP 用的是階梯定義"));
        const double trap = trapezoidArea(pr.xs, pr.ys);
        check(std::abs(trap - pr.area) > 1e-6,
              QStringLiteral("梯形法和 AP 確實不同（所以不能混用）"),
              QStringLiteral("梯形 %1 / AP %2").arg(trap, 0, 'f', 5).arg(pr.area, 0, 'f', 5));
    }
    {
        // 退化輸入不該當掉
        check(rocCurve({1, 2}, {true, true}).xs.isEmpty(),
              QStringLiteral("沒有陰性樣本時回傳空 ROC"));
        check(rocCurve({1, 2, 3}, {true, false}).xs.isEmpty(),
              QStringLiteral("長度不符時回傳空 ROC"));
        check(prCurve({1, 2}, {false, false}).xs.isEmpty(),
              QStringLiteral("沒有陽性樣本時回傳空 PR"));
    }
}

/// Kaplan–Meier 存活曲線。
void testKaplanMeier() {
    out() << "Kaplan–Meier\n";

    {
        // 沒有設限：S(t) 就是「還沒發生事件的比例」
        const QVector<double> times{1, 2, 3, 4};
        const QVector<bool> events{true, true, true, true};
        const SurvivalCurve km = kaplanMeier(times, events);
        check(km.points.size() == 5, QStringLiteral("四個事件加 t=0 共五個轉折點"),
              QStringLiteral("得到 %1").arg(km.points.size()));
        checkClose(km.points[0].survival, 1.0, 1e-12, QStringLiteral("t=0 時 S=1"));
        checkClose(km.points[1].survival, 0.75, 1e-12, QStringLiteral("第一個事件後 S=0.75"));
        checkClose(km.points[4].survival, 0.0, 1e-12, QStringLiteral("全部發生後 S=0"));
        checkClose(km.medianSurvival, 2.0, 1e-12, QStringLiteral("中位存活時間 = 2"));
    }
    {
        // 教科書例題：n=6，時間 1(事件) 2(設限) 3(事件) 4(設限) 5(事件) 6(設限)
        // S(1) = 5/6；S(3) = 5/6 · 3/4 = 0.625；S(5) = 0.625 · 1/2 = 0.3125
        //
        // points 每個「有事情發生」的時間都收一個點，只有設限的時間也算 ——
        // 那種點的 S 不變但 atRisk 會掉，而且設限記號要畫在那裡。
        // 所以索引不等於「第幾個事件」：t=0,1,2,3,4,5,6 共七個點。
        const QVector<double> times{1, 2, 3, 4, 5, 6};
        const QVector<bool> events{true, false, true, false, true, false};
        const SurvivalCurve km = kaplanMeier(times, events);
        check(km.points.size() == 7, QStringLiteral("七個轉折點（含只有設限的時間）"),
              QStringLiteral("得到 %1").arg(km.points.size()));
        auto atTime = [&](double t) {
            for (const SurvivalPoint& p : km.points)
                if (std::abs(p.time - t) < 1e-12) return p;
            return SurvivalPoint();
        };
        checkClose(atTime(1).survival, 5.0 / 6.0, 1e-12, QStringLiteral("S(1) = 5/6"));
        checkClose(atTime(2).survival, 5.0 / 6.0, 1e-12,
                   QStringLiteral("只有設限的時間 S 不變"));
        checkClose(atTime(3).survival, 0.625, 1e-12, QStringLiteral("S(3) = 0.625"));
        checkClose(atTime(5).survival, 0.3125, 1e-12, QStringLiteral("S(5) = 0.3125"));
        check(km.censorTimes.size() == 3, QStringLiteral("三個設限記號"),
              QStringLiteral("得到 %1").arg(km.censorTimes.size()));
        checkClose(km.medianSurvival, 5.0, 1e-12,
                   QStringLiteral("S 第一次掉到 0.5 以下是 t=5"));
        // 設限者在被設限之前仍算在風險集合裡
        check(atTime(1).atRisk == 6, QStringLiteral("t=1 時風險人數 6"));
        check(atTime(2).atRisk == 5, QStringLiteral("t=2 時風險人數 5"),
              QStringLiteral("得到 %1").arg(atTime(2).atRisk));
        check(atTime(3).atRisk == 4, QStringLiteral("t=3 時風險人數 4（設限者已移除）"),
              QStringLiteral("得到 %1").arg(atTime(3).atRisk));
    }
    {
        // 同時間的事件與設限：先算事件
        const QVector<double> times{5, 5, 9};
        const QVector<bool> events{true, false, true};
        const SurvivalCurve km = kaplanMeier(times, events);
        checkClose(km.points[1].survival, 2.0 / 3.0, 1e-12,
                   QStringLiteral("同時間時分母含設限者"));
    }
    {
        // 信賴區間必須留在 [0, 1] 之內 —— 用 S ± z·SE 的寫法在這裡會超出去
        QVector<double> times;
        QVector<bool> events;
        for (int i = 1; i <= 40; ++i) {
            times.append(i);
            events.append(i % 4 == 0);
        }
        const SurvivalCurve km = kaplanMeier(times, events);
        bool inRange = true;
        bool ordered = true;
        for (const SurvivalPoint& p : km.points) {
            if (!(p.lower >= -1e-12 && p.upper <= 1 + 1e-12)) inRange = false;
            if (!(p.lower <= p.survival + 1e-12 && p.survival <= p.upper + 1e-12))
                ordered = false;
        }
        check(inRange, QStringLiteral("信賴區間不會跑出 [0, 1]"));
        check(ordered, QStringLiteral("下界 ≤ S ≤ 上界"));
    }
    {
        check(kaplanMeier({}, {}).points.isEmpty(), QStringLiteral("空輸入回傳空曲線"));
        check(kaplanMeier({1, 2}, {true}).points.isEmpty(),
              QStringLiteral("長度不符時回傳空曲線"));
    }
}

/// 管制圖的界限與 Nelson 規則。
void testControlChart() {
    out() << "管制圖\n";

    {
        // 兩種 σ 估法的差別：這組資料是一條直線往上漂
        QVector<double> drift;
        for (int i = 0; i < 30; ++i) drift.append(i);
        const ControlLimits mr = controlLimits(drift, SigmaEstimate::MovingRange);
        const ControlLimits sd = controlLimits(drift, SigmaEstimate::StandardDeviation);
        checkClose(mr.center, 14.5, 1e-12, QStringLiteral("中心線是平均值"));
        // 每步都差 1，平均移動全距 = 1，σ = 1/1.128
        checkClose(mr.sigma, 1.0 / 1.128, 1e-12,
                   QStringLiteral("移動全距法的 σ = R̄/1.128"));
        check(sd.sigma > mr.sigma * 5,
              QStringLiteral("漂移讓標準差法的 σ 大很多（界限被撐寬）"),
              QStringLiteral("MR %1 / SD %2").arg(mr.sigma, 0, 'f', 3).arg(sd.sigma, 0, 'f', 3));
        // 這就是後果：用標準差法，沒有一點會被規則 1 抓到
        check(nelsonRules(drift, sd).isEmpty(),
              QStringLiteral("標準差法下規則 1 抓不到這段漂移"));
        check(!nelsonRules(drift, mr).isEmpty(),
              QStringLiteral("移動全距法下規則 1 抓得到"));
    }
    {
        ControlLimits cl;
        cl.center = 0;
        cl.sigma = 1;
        checkClose(cl.upper(), 3.0, 1e-12, QStringLiteral("UCL = 中心 + 3σ"));
        checkClose(cl.lower(), -3.0, 1e-12, QStringLiteral("LCL = 中心 − 3σ"));

        // 規則 1：只有超出 3σ 的那一點
        QVector<double> v(20, 0.0);
        v[7] = 3.5;
        v[12] = -4.0;
        const QVector<RuleViolation> r1 = nelsonRules(v, cl, {ControlRule::Beyond3Sigma});
        check(r1.size() == 2, QStringLiteral("規則 1 抓到兩點"),
              QStringLiteral("得到 %1").arg(r1.size()));
        check(r1.size() == 2 && r1[0].index == 7 && r1[1].index == 12,
              QStringLiteral("規則 1 抓到的是第 7 與第 12 點"));
        // 剛好等於 3σ 不算（規則說的是「超出」）
        QVector<double> edge(20, 0.0);
        edge[3] = 3.0;
        check(nelsonRules(edge, cl, {ControlRule::Beyond3Sigma}).isEmpty(),
              QStringLiteral("剛好落在 3σ 上不算違規"));
    }
    {
        ControlLimits cl;
        cl.center = 0;
        cl.sigma = 1;
        // 規則 2：連續 9 點同側。這些點全都在界限內，規則 1 完全抓不到。
        QVector<double> v(20, -0.3);
        for (int i = 5; i < 14; ++i) v[i] = 0.3;
        check(nelsonRules(v, cl, {ControlRule::Beyond3Sigma}).isEmpty(),
              QStringLiteral("規則 1 看不到「連續同側」"));
        const QVector<RuleViolation> r2 = nelsonRules(v, cl, {ControlRule::NineSameSide});
        check(r2.size() == 1 && r2[0].index == 13,
              QStringLiteral("規則 2 報在第 9 點（索引 13）"),
              QStringLiteral("得到 %1 筆").arg(r2.size()));
        // 8 點不該觸發
        QVector<double> v8(20, -0.3);
        for (int i = 5; i < 13; ++i) v8[i] = 0.3;
        check(nelsonRules(v8, cl, {ControlRule::NineSameSide}).isEmpty(),
              QStringLiteral("只有 8 點同側時不觸發"));
    }
    {
        ControlLimits cl;
        cl.center = 0;
        cl.sigma = 1;
        // 規則 3 說的是「連續 6 個點」，也就是 5 個上升的差，不是 6 個差。
        // 數成 6 個差的話要 7 個點才報，整整晚一點。
        QVector<double> up{0, 0.1, 0.2, 0.3, 0.4, 0.5};
        const QVector<RuleViolation> r3 = nelsonRules(up, cl, {ControlRule::SixTrending});
        check(r3.size() == 1 && r3[0].index == 5, QStringLiteral("規則 3 在第 6 點就報"),
              QStringLiteral("得到 %1 筆").arg(r3.size()));
        QVector<double> up5{0, 0.1, 0.2, 0.3, 0.4};
        check(nelsonRules(up5, cl, {ControlRule::SixTrending}).isEmpty(),
              QStringLiteral("只有五個點上升時不觸發"));
        QVector<double> down{0, -0.1, -0.2, -0.3, -0.4, -0.5};
        check(nelsonRules(down, cl, {ControlRule::SixTrending}).size() == 1,
              QStringLiteral("下降的趨勢也抓得到"));

        // 規則 4：14 點上下交替 = 13 個正負相反的差。
        // 把點數從 1 開始數的寫法要 15 點才報，這一組剛好測得出來。
        QVector<double> zig14, zig13;
        for (int i = 0; i < 14; ++i) zig14.append((i % 2) ? 0.5 : -0.5);
        for (int i = 0; i < 13; ++i) zig13.append((i % 2) ? 0.5 : -0.5);
        const QVector<RuleViolation> r4 =
            nelsonRules(zig14, cl, {ControlRule::FourteenAlternating});
        check(r4.size() == 1 && r4[0].index == 13,
              QStringLiteral("規則 4 在第 14 點報"),
              QStringLiteral("得到 %1 筆").arg(r4.size()));
        check(nelsonRules(zig13, cl, {ControlRule::FourteenAlternating}).isEmpty(),
              QStringLiteral("13 點交替時不觸發"));

        // 規則 5：3 點中 2 點同側超出 2σ
        QVector<double> v(10, 0.0);
        v[4] = 2.5;
        v[6] = 2.6;
        const QVector<RuleViolation> r5 =
            nelsonRules(v, cl, {ControlRule::TwoOfThreeBeyond2Sigma});
        check(r5.size() == 1 && r5[0].index == 6, QStringLiteral("規則 5 抓到 3 中 2"),
              QStringLiteral("得到 %1 筆").arg(r5.size()));
        // 一正一負不算同側
        QVector<double> mixed(10, 0.0);
        mixed[4] = 2.5;
        mixed[6] = -2.6;
        check(nelsonRules(mixed, cl, {ControlRule::TwoOfThreeBeyond2Sigma}).isEmpty(),
              QStringLiteral("一正一負不算「同一側」"));

        // 規則 7：連續 15 點都在 1σ 之內
        QVector<double> tight(15, 0.2);
        const QVector<RuleViolation> r7 =
            nelsonRules(tight, cl, {ControlRule::FifteenWithin1Sigma});
        check(r7.size() == 1 && r7[0].index == 14,
              QStringLiteral("規則 7 抓到變異過小"));

        // 規則 8：連續 8 點都在 1σ 之外（兩側交替也算）
        QVector<double> bimodal;
        for (int i = 0; i < 8; ++i) bimodal.append((i % 2) ? 1.5 : -1.5);
        const QVector<RuleViolation> r8 =
            nelsonRules(bimodal, cl, {ControlRule::EightOutside1Sigma});
        check(r8.size() == 1, QStringLiteral("規則 8 抓到雙峰"),
              QStringLiteral("得到 %1 筆").arg(r8.size()));
    }
    {
        ControlLimits cl;
        cl.center = 0;
        cl.sigma = 1;
        // 同一點被兩條規則抓到就回兩筆，而且排好序
        QVector<double> v(10, 0.0);
        v[5] = 2.5;
        v[7] = 4.0;
        v[8] = 2.8;
        const QVector<RuleViolation> all = nelsonRules(
            v, cl, {ControlRule::Beyond3Sigma, ControlRule::TwoOfThreeBeyond2Sigma});
        bool sorted = true;
        for (int i = 1; i < all.size(); ++i)
            if (all[i].index < all[i - 1].index) sorted = false;
        check(sorted, QStringLiteral("違規清單依索引排序"));
        check(all.size() >= 2, QStringLiteral("兩條規則的結果會合併"),
              QStringLiteral("得到 %1 筆").arg(all.size()));

        check(nelsonRules(v, ControlLimits()).isEmpty(),
              QStringLiteral("界限無效時不回報任何違規"));
        check(controlLimits({1.0}).isValid() == false,
              QStringLiteral("只有一筆資料算不出 σ"));
    }
    {
        // 圖元：limits 沒設時自己算，設了就用設的
        ControlChart c;
        c.values = {10, 12, 11, 13, 9, 11, 12, 10};
        check(c.effectiveLimits().isValid(), QStringLiteral("沒給界限時自動算"));
        ControlLimits manual;
        manual.center = 100;
        manual.sigma = 5;
        c.limits = manual;
        checkClose(c.effectiveLimits().center, 100, 1e-12,
                   QStringLiteral("給了界限就用給的"));
        // 界限在資料範圍之外時，axisLimits 要含進來，否則界限線被裁掉
        const AxisLimits lim = c.axisLimits();
        check(lim.y.max >= 115 && lim.y.min <= 85,
              QStringLiteral("座標範圍含進 ±3σ 界限"),
              QStringLiteral("[%1, %2]").arg(lim.y.min, 0, 'f', 1).arg(lim.y.max, 0, 'f', 1));
    }
}

/// 森林圖的版面。
void testForestPlot() {
    out() << "森林圖\n";

    ForestPlot f;
    f.rows = {{QStringLiteral("研究 A"), 1.2, 0.8, 1.8, 10, false, QColor(), QString()},
              {QStringLiteral("研究 B"), 0.9, 0.5, 1.6, 20, false, QColor(), QString()},
              {QStringLiteral("合併"), 1.05, 0.85, 1.30, 0, true, QColor(), QString()}};

    f.topToBottom = true;
    const QVector<double> pos = f.rowPositions();
    check(pos.size() == 3 && pos[0] == 2 && pos[2] == 0,
          QStringLiteral("topToBottom 時第一列在最上面"));
    const QStringList labels = f.rowLabels();
    // rowLabels 要按「位置」排，不是按 rows 的順序 —— 弄反的話整張圖的列名會上下顛倒
    check(labels.size() == 3 && labels[0] == QStringLiteral("合併") &&
              labels[2] == QStringLiteral("研究 A"),
          QStringLiteral("列名依位置 0,1,2 排好"),
          labels.join(QLatin1Char('/')));

    f.topToBottom = false;
    const QVector<double> pos2 = f.rowPositions();
    check(pos2[0] == 0 && pos2[2] == 2, QStringLiteral("關掉 topToBottom 就反過來"));
    check(f.rowLabels()[0] == QStringLiteral("研究 A"),
          QStringLiteral("列名跟著反過來"));

    const AxisLimits lim = f.axisLimits();
    check(lim.x.min <= 0.5 && lim.x.max >= 1.8,
          QStringLiteral("X 範圍含所有區間的兩端"));
    f.nullValue = 1.0;
    check(f.axisLimits().x.min <= 1.0, QStringLiteral("無效應線也含進範圍"));
    check(ForestPlot().axisLimits().hasData() == false,
          QStringLiteral("沒有資料時不回報範圍"));
}

/// FFT、窗函數、功率譜與頻譜圖。
/// 雜訊用的固定亂數（每次跑結果一樣）。
QVector<double> fixedNoise(int n, double sigma, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd(0, sigma);
    QVector<double> v(n);
    for (double& x : v) x = nd(rng);
    return v;
}

double stddevOf(const QVector<double>& v, int from, int to) {
    double mean = 0;
    for (int i = from; i < to; ++i) mean += v[i] / (to - from);
    double ss = 0;
    for (int i = from; i < to; ++i) ss += (v[i] - mean) * (v[i] - mean);
    return std::sqrt(ss / (to - from));
}

void testSpectralAnalysis() {
    out() << "頻譜分析（Welch、找峰值、即時頻譜、瀑布）\n";
    const double fs = 1000;

    {
        // Welch：一個剛好落在格上的正弦波加上強雜訊
        const int n = 16384, seg = 512;
        const double f0 = 51 * fs / seg;  // 99.609375 Hz，落在 512 點的第 51 格
        QVector<double> sig = fixedNoise(n, 1.0, 7);
        for (int i = 0; i < n; ++i) sig[i] += std::sin(2 * kPi * f0 * i / fs);
        const Periodogram w = welch(sig, fs, seg, 0.5);
        check(w.segments == (n - seg) / (seg / 2) + 1, "Welch 的段數 = (n − 段長) / hop + 1",
              QString::number(w.segments));
        check(w.frequencies.size() == seg / 2 + 1, "格數由段長決定");
        checkClose(w.amplitude[51], 1.0, 0.12, "平均後正弦波的振幅還原成 1");
        // 雜訊底的起伏：單段功率譜像草叢，平均 63 段之後要平很多
        const Periodogram one = periodogram(sig.mid(0, seg), fs);
        const double raw = stddevOf(one.decibels, 100, 220), avg = stddevOf(w.decibels, 100, 220);
        check(avg < raw * 0.4, "平均之後雜訊底的起伏小很多",
              QStringLiteral("%1 dB → %2 dB").arg(raw, 0, 'f', 2).arg(avg, 0, 'f', 2));
        const Periodogram shortSig = welch(sig.mid(0, 300), fs, seg);
        check(shortSig.segments == 1 && shortSig.frequencies.size() == 151, "訊號比段長短時退回整段做一次");
    }

    {
        // 找峰值：100.7 Hz（不在格上）、振幅 1；230 Hz、−20 dB
        const int n = 500;  // 格距 2 Hz
        QVector<double> sig(n);
        for (int i = 0; i < n; ++i)
            sig[i] = std::sin(2 * kPi * 100.7 * i / fs) + 0.1 * std::sin(2 * kPi * 230 * i / fs);
        const Periodogram pg = periodogram(sig, fs, WindowFunction::Hann);
        const QVector<SpectralPeak> peaks = findSpectralPeaks(pg, 5, 10);
        check(peaks.size() == 2, "找到兩個峰（裙邊不算）", QString::number(peaks.size()));
        if (peaks.size() >= 2) {
            checkClose(peaks[0].frequency, 100.7, 0.2, "內插後的頻率比格距（2 Hz）準得多");
            checkClose(peaks[0].decibels, 0.0, 0.6, "內插後的峰值接近 0 dB");
            checkClose(peaks[0].amplitude, 1.0, 0.07, "峰值換回振幅");
            checkClose(peaks[1].frequency, 230.0, 0.2, "第二個峰在 230 Hz");
            checkClose(peaks[1].decibels, -20.0, 0.6, "第二個峰約 −20 dB");
            check(peaks[0].decibels > peaks[1].decibels && peaks[1].prominence > 40, "依高度由大到小排，顯著度以雜訊底為準");
        }
        check(findSpectralPeaks(pg, 1, 10).size() == 1, "maxPeaks 限制個數");
        const QVector<SpectralPeak> high = findSpectralPeaks(pg, 5, 10, 0, 150);
        check(!high.isEmpty() && std::abs(high[0].frequency - 230) < 1, "minFrequency 以下的不算");
        check(findSpectralPeaks(pg, 5, 10, 200).size() == 1, "隔太近的只留比較高的");
        const Periodogram noise = periodogram(fixedNoise(1024, 1.0, 3), fs);
        check(findSpectralPeaks(noise, 5, 15).isEmpty(), "純雜訊在顯著度 15 dB 下找不到峰",
              QString::number(findSpectralPeaks(noise, 1, 0).value(0).prominence));
        check(findSpectralPeaks(Periodogram(), 5).isEmpty(), "空譜不會當掉");
    }

    {
        // 即時頻譜：收滿 fftSize 才開始算，之後每 fftSize/2 個樣本算一次
        LiveSpectrum ls(1024, fs);
        const double f0 = 100 * fs / 1024;  // 落在第 100 格
        auto tone = [&](int from, int count, double f, double amp) {
            QVector<double> v(count);
            for (int i = 0; i < count; ++i) v[i] = amp * std::sin(2 * kPi * f * (from + i) / fs);
            return v;
        };
        ls.addRange(tone(0, 1000, f0, 1));
        check(ls.frameCount() == 0 && ls.spectrum().frequencies.isEmpty(), "沒收滿之前不算頻譜");
        ls.addRange(tone(1000, 24, f0, 1));
        check(ls.frameCount() == 1, "收滿 1024 個樣本算第一次");
        ls.addRange(tone(1024, 512, f0, 1));
        check(ls.frameCount() == 2, "之後每 512 個樣本算一次");
        const Periodogram sp = ls.spectrum();
        checkClose(sp.peakFrequency(), f0, 1e-9, "峰值頻率正確");
        checkClose(sp.amplitude[100], 1.0, 0.01, "振幅還原成 1");

        // 峰值保持：換到另一個頻率之後，舊的峰在保持曲線上還在
        ls.addRange(tone(1536, 4096, 3 * f0, 1));
        const Periodogram after = ls.spectrum();
        const QVector<double> hold = ls.peakHold();
        check(hold.size() == after.decibels.size() && hold[100] > -1 && after.decibels[100] < -40,
              "峰值保持留住已經消失的峰", QStringLiteral("%1 / %2 dB").arg(hold.value(100)).arg(after.decibels.value(100)));
        ls.resetPeakHold();
        check(ls.peakHold().isEmpty(), "resetPeakHold 清掉保持");
        ls.setFftSize(256);
        check(ls.frameCount() == 0 && ls.fftSize() == 256, "改 FFT 長度會重來");

        // 平均：同一串雜訊，開平均的雜訊底比較平
        auto floorJitter = [&](double a) {
            LiveSpectrum s(512, fs);
            s.averaging = a;
            s.addRange(fixedNoise(512 * 40, 1.0, 11));
            return stddevOf(s.spectrum().decibels, 20, 230);
        };
        const double raw = floorJitter(0), avg = floorJitter(0.9);
        check(avg < raw * 0.6, "averaging 讓雜訊底變平", QStringLiteral("%1 → %2 dB").arg(raw, 0, 'f', 2).arg(avg, 0, 'f', 2));

        // 背景執行緒一直餵、主執行緒一直讀，不能當掉也不能讀到長度不一致的資料
        LiveSpectrum shared(512, fs);
        std::atomic<bool> done{false};
        std::thread feeder([&] {
            const QVector<double> chunk = fixedNoise(4096, 1.0, 5);
            for (int r = 0; r < 60; ++r) shared.addRange(chunk);
            done = true;
        });
        bool consistent = true;
        while (!done) {
            const Periodogram p = shared.spectrum();
            if (p.frequencies.size() != p.decibels.size()) consistent = false;
        }
        feeder.join();
        check(consistent && shared.frameCount() > 400, "背景執行緒餵資料時讀到的頻譜一致",
              QString::number(shared.frameCount()));

        Plot plot;
        auto* live = plot.addLiveSpectrum(1024, fs);
        live->peakLabels = 2;
        live->showPeakHold = true;
        live->addRange(tone(0, 4096, f0, 0.5));
        plot.renderToImage(500, 300);
        checkClose(plot.bottomAxis()->range.max, fs / 2, 1e-9, "autoAxes：X 軸是 0～Nyquist");
        checkClose(plot.leftAxis()->range.min, live->minDb, 1e-9, "autoAxes：Y 軸是 minDb～maxDb");
    }

    {
        // 瀑布：頻譜圖的每個時間格是一條曲線
        const int n = 8000;
        QVector<double> chirp(n);
        for (int i = 0; i < n; ++i) {
            const double t = i / fs;
            chirp[i] = 0.1 * std::sin(2 * kPi * (50 * t + 25 * t * t));  // 峰值約 −20 dB
        }
        const Spectrogram sp = spectrogram(chirp, fs, 256, 64);
        Waterfall3D w;
        w.setSpectrogram(sp, 20);
        check(w.zs.size() == 20 && w.ys.size() == 20, "最多取 maxCurves 條");
        check(w.xs.size() == sp.rows && w.zs[0].size() == sp.rows, "每條曲線一個頻率格一點");
        checkClose(w.ys.last(), double(sp.cols - 1) * sp.hopSize / fs, 1e-9, "最後一條是最新的時間格");
        checkClose(w.floorZ, sp.valueRange().min, 1e-9, "地板設成頻譜圖的最小值");
        CoordinateRange x, y, z;
        w.limits(x, y, z);
        check(z.max < -10, "Z 範圍不會被硬撐到 0（dB 全是負的）", QString::number(z.max));
        Waterfall3D few;
        few.setSpectrogram(sp, 1000);
        check(few.zs.size() == sp.cols, "時間格比 maxCurves 少時全部都畫");
        Waterfall3D band;
        band.setSpectrogram(sp, 10, Colormap(Colormap::Viridis), 200);
        check(!band.xs.isEmpty() && band.xs.last() <= 200 && band.xs.last() > 195 && band.zs[0].size() == band.xs.size(),
              "maxFrequency 只取到指定的頻率", QString::number(band.xs.value(band.xs.size() - 1)));
    }
}

void testSpectral() {
    out() << "頻譜\n";

    {
        // 任意長度都要和樸素 DFT 一致。2 的次方走 radix-2，其餘走 Bluestein ——
        // 只測 2 的次方的話，Bluestein 整條路徑都沒被跑過。
        for (int n : {2, 3, 4, 5, 7, 8, 10, 12, 16, 31, 64}) {
            QVector<double> re(n), im(n);
            for (int i = 0; i < n; ++i) {
                re[i] = std::sin(0.7 * i) + 0.3 * i;
                im[i] = std::cos(0.31 * i);
            }
            QVector<double> refRe, refIm;
            naiveDft(re, im, refRe, refIm, false);
            QVector<double> fr = re, fi = im;
            fft(fr, fi, false);
            double worst = 0;
            for (int k = 0; k < n; ++k)
                worst = std::max(worst, std::hypot(fr[k] - refRe[k], fi[k] - refIm[k]));
            check(worst < 1e-9, QStringLiteral("n = %1 的 FFT 和 DFT 一致").arg(n),
                  QStringLiteral("最大誤差 %1").arg(worst, 0, 'g', 4));
        }
    }
    {
        // 來回一圈要回到原值
        for (int n : {6, 13, 32}) {
            QVector<double> re(n), im(n);
            for (int i = 0; i < n; ++i) {
                re[i] = i * 1.7 - 3;
                im[i] = std::sin(i);
            }
            QVector<double> re0 = re, im0 = im;
            fft(re, im, false);
            fft(re, im, true);
            double worst = 0;
            for (int i = 0; i < n; ++i)
                worst = std::max(worst, std::hypot(re[i] - re0[i], im[i] - im0[i]));
            check(worst < 1e-10, QStringLiteral("n = %1 的 fft → ifft 回到原值").arg(n),
                  QStringLiteral("最大誤差 %1").arg(worst, 0, 'g', 4));
        }
    }
    {
        const QVector<double> hann = windowValues(WindowFunction::Hann, 9);
        check(hann.size() == 9, QStringLiteral("窗的長度就是要求的點數"));
        checkClose(hann.first(), 0.0, 1e-12, QStringLiteral("Hann 窗兩端為 0"));
        checkClose(hann.last(), 0.0, 1e-12, QStringLiteral("Hann 窗另一端也是 0"));
        checkClose(hann[4], 1.0, 1e-12, QStringLiteral("Hann 窗中點為 1"));
        checkClose(windowCoherentGain(WindowFunction::Rectangular, 64), 1.0, 1e-12,
                   QStringLiteral("矩形窗的相干增益是 1"));
        checkClose(windowCoherentGain(WindowFunction::Hann, 1024), 0.5, 1e-3,
                   QStringLiteral("Hann 窗的相干增益約 0.5"));
        check(windowValues(WindowFunction::Hann, 0).isEmpty(),
              QStringLiteral("n = 0 時回傳空窗"));
    }
    {
        // 50 Hz、振幅 2 的正弦。取樣率 1000、長度 1000 點 → 剛好整數個週期。
        const double fs = 1000;
        const int n = 1000;
        QVector<double> sig(n);
        for (int i = 0; i < n; ++i) sig[i] = 2.0 * std::sin(2 * kPi * 50 * i / fs);
        const Periodogram pg = periodogram(sig, fs, WindowFunction::Hann);
        check(pg.frequencies.size() == n / 2 + 1, QStringLiteral("單邊譜的格數 = n/2+1"),
              QStringLiteral("得到 %1").arg(pg.frequencies.size()));
        checkClose(pg.frequencies.last(), fs / 2, 1e-9,
                   QStringLiteral("最後一格是 Nyquist"));
        checkClose(pg.peakFrequency(), 50.0, 1e-9, QStringLiteral("峰值在 50 Hz"));
        // 振幅要還原回 2：忘了乘 2 補鏡像就會得到 1，忘了除窗增益會得到 1
        int peak = 0;
        for (int i = 1; i < pg.amplitude.size(); ++i)
            if (pg.amplitude[i] > pg.amplitude[peak]) peak = i;
        checkClose(pg.amplitude[peak], 2.0, 0.02, QStringLiteral("還原出來的振幅是 2"));
        checkClose(pg.decibels[peak], 20 * std::log10(2.0), 0.1,
                   QStringLiteral("dB 和振幅對得上"));
        check(periodogram({1.0}, fs).frequencies.isEmpty(),
              QStringLiteral("樣本太少時回傳空譜"));
        check(periodogram(sig, 0).frequencies.isEmpty(),
              QStringLiteral("取樣率 0 時回傳空譜"));
    }
    {
        // 直流不該被乘 2
        const int n = 256;
        QVector<double> flat(n, 3.0);
        const Periodogram pg = periodogram(flat, 1000, WindowFunction::Rectangular);
        checkClose(pg.amplitude.first(), 3.0, 1e-9,
                   QStringLiteral("直流的振幅不乘 2"));
    }
    {
        // 掃頻訊號：頻率隨時間上升，頻譜圖的峰值位置也該隨時間上升
        const double fs = 2000;
        const int n = 4000;
        QVector<double> chirp(n);
        for (int i = 0; i < n; ++i) {
            const double t = i / fs;
            chirp[i] = std::sin(2 * kPi * (100 * t + 150 * t * t));
        }
        const Spectrogram sp = spectrogram(chirp, fs, 256, 64);
        check(sp.rows == 129, QStringLiteral("頻率格數 = 窗長/2+1"),
              QStringLiteral("得到 %1").arg(sp.rows));
        check(sp.cols > 50, QStringLiteral("時間格數隨 hop 算出來"),
              QStringLiteral("得到 %1").arg(sp.cols));
        checkClose(sp.maxFrequency(), 1000.0, 1e-9, QStringLiteral("頻率上限是 Nyquist"));

        auto peakRow = [&](int col) {
            int best = 0;
            for (int r = 1; r < sp.rows; ++r)
                if (sp.at(r, col) > sp.at(best, col)) best = r;
            return best;
        };
        const int early = peakRow(5);
        const int late = peakRow(sp.cols - 6);
        check(late > early + 10, QStringLiteral("掃頻的峰值隨時間往高頻移動"),
              QStringLiteral("第 5 格在 %1、倒數第 6 格在 %2").arg(early).arg(late));

        // 第 0 列是最低頻 —— 寫錯方向的話上面那個檢查會反向
        check(sp.at(0, 0) < sp.at(early, 0),
              QStringLiteral("第 0 列是最低頻（不是最高頻）"));

        // dB 的下限被夾住，不會出現 −6000
        const CoordinateRange vr = sp.valueRange();
        checkClose(vr.span(), 80.0, 1e-6, QStringLiteral("動態範圍夾在 80 dB"));

        const CoordinateRect ext = sp.extent();
        checkClose(ext.top, 1000.0, 1e-9, QStringLiteral("extent 的上緣是 Nyquist"));
        checkClose(ext.left, 0.0, 1e-12, QStringLiteral("extent 從 0 秒開始"));

        check(spectrogram(chirp, fs, 4000000).values.isEmpty(),
              QStringLiteral("窗比資料長時回傳空頻譜"));
    }
}

/// 字型樣式的每一個欄位，以及「一次改掉整張圖」。
void testFonts() {
    out() << "字型\n";

    {
        FontStyle f;
        checkClose(f.effectiveWeight(), FontStyle::Normal, 0,
                   QStringLiteral("預設字重是 Normal"));
        f.bold = true;
        checkClose(f.effectiveWeight(), FontStyle::Bold, 0,
                   QStringLiteral("bold 等同 Bold(700)"));
        // weight 一旦設了就蓋過 bold —— 兩個欄位同時存在，順序必須講死，
        // 否則「我設了 Light 怎麼還是粗的」會查很久
        f.weight = FontStyle::Light;
        checkClose(f.effectiveWeight(), FontStyle::Light, 0,
                   QStringLiteral("設了 weight 就蓋過 bold"));
        f.weight = FontStyle::Auto;
        checkClose(f.effectiveWeight(), FontStyle::Bold, 0,
                   QStringLiteral("Auto 退回看 bold"));
    }
    {
        // 每個欄位都要真的傳進 QFont
        FontStyle f;
        f.size = 17;
        f.weight = FontStyle::DemiBold;
        f.italic = true;
        f.underline = true;
        f.strikeOut = true;
        f.stretch = 150;
        const QFont q = f.font();
        checkClose(q.pointSizeF(), 17, 1e-9, QStringLiteral("字級傳進 QFont"));
        checkClose(q.weight(), FontStyle::DemiBold, 0, QStringLiteral("字重傳進 QFont"));
        check(q.italic(), QStringLiteral("斜體傳進 QFont"));
        check(q.underline(), QStringLiteral("底線傳進 QFont"));
        check(q.strikeOut(), QStringLiteral("刪除線傳進 QFont"));
        check(q.stretch() == 150, QStringLiteral("橫向拉伸傳進 QFont"));
        check(FontStyle().font().stretch() != 150,
              QStringLiteral("stretch 為 0 時不動 QFont 的預設"));
    }
    {
        Label lb;
        lb.text = QStringLiteral("MMMMM");
        const double w0 = measureText(lb).width();

        lb.font.size = 22;
        check(measureText(lb).width() > w0 * 1.5, QStringLiteral("字級變大，量到的寬度跟著變大"));

        Label spaced;
        spaced.text = lb.text;
        spaced.font.letterSpacing = 6;
        check(measureText(spaced).width() > w0 + 20,
              QStringLiteral("字距放寬，量到的寬度跟著變大"),
              QStringLiteral("%1 → %2").arg(w0, 0, 'f', 1)
                  .arg(measureText(spaced).width(), 0, 'f', 1));

        Label wide;
        wide.text = lb.text;
        wide.font.stretch = 200;
        check(measureText(wide).width() > w0 * 1.5,
              QStringLiteral("橫向拉伸，量到的寬度跟著變大"));
    }
    {
        // 轉 90 度之後外接框要長寬互換，版面才留得出正確的空間
        Label lb;
        lb.text = QStringLiteral("very wide label");
        const QSizeF flat = measureText(lb);
        lb.font.rotation = 90;
        const QSizeF turned = measureText(lb);
        checkClose(turned.width(), flat.height(), 1e-6,
                   QStringLiteral("轉 90 度後寬度換成原本的高度"));
        checkClose(turned.height(), flat.width(), 1e-6,
                   QStringLiteral("轉 90 度後高度換成原本的寬度"));
        lb.font.rotation = 180;
        checkClose(measureText(lb).width(), flat.width(), 1e-6,
                   QStringLiteral("轉 180 度的外接框和原本一樣"));
    }

    {
        // visitFonts 要走到每一處。這裡放進一批各式各樣的圖元，
        // 改完之後再走一次確認「全部都變了」—— 漏掉的那一處會在第二趟現形。
        Plot plot;
        plot.title.text = QStringLiteral("標題");
        plot.legend.isVisible = true;
        plot.addScatter({1, 2, 3}, {1, 4, 9})->legendText = QStringLiteral("資料");
        plot.addBars({3, 1, 2});
        plot.addPie({3, 2, 1});
        plot.addText(QStringLiteral("註"), 1, 1);
        plot.addSunburst({{QStringLiteral("A"), 1, QColor(), {}}});
        GanttTask task;
        task.label = QStringLiteral("工項");
        task.start = 0;
        task.end = 1;
        plot.addGantt({task});
        plot.addControlChart({1, 2, 3, 2, 1});
        plot.addColorbar(Colormap(Colormap::Viridis), CoordinateRange(0, 1));

        int visited = 0;
        plot.visitFonts([&](FontStyle&) { ++visited; });
        check(visited >= 20, QStringLiteral("visitFonts 走到的字型數量合理"),
              QStringLiteral("走到 %1 處").arg(visited));

        plot.setFontFamily(QStringLiteral("Courier New"));
        int wrongFamily = 0;
        plot.visitFonts([&](FontStyle& f) {
            if (f.family != QStringLiteral("Courier New")) ++wrongFamily;
        });
        check(wrongFamily == 0, QStringLiteral("setFontFamily 一處都沒漏"),
              QStringLiteral("%1 處沒改到").arg(wrongFamily));

        plot.setFontWeight(FontStyle::Black);
        int wrongWeight = 0;
        plot.visitFonts([&](FontStyle& f) {
            if (f.effectiveWeight() != FontStyle::Black) ++wrongWeight;
        });
        check(wrongWeight == 0, QStringLiteral("setFontWeight 一處都沒漏"));

        // 等比縮放要保住層次：標題本來就比刻度大，縮放後仍然要大
        const double titleBefore = plot.title.font.size;
        const double tickBefore = plot.bottomAxis()->tickFont.size;
        plot.scaleFontSizes(2.0);
        checkClose(plot.title.font.size, titleBefore * 2, 1e-9,
                   QStringLiteral("scaleFontSizes 是乘上去的"));
        check(plot.title.font.size > plot.bottomAxis()->tickFont.size,
              QStringLiteral("縮放後標題仍然比刻度大"));
        checkClose(plot.bottomAxis()->tickFont.size, tickBefore * 2, 1e-9,
                   QStringLiteral("刻度字級也被縮放"));

        plot.scaleFontSizes(-1);
        checkClose(plot.title.font.size, titleBefore * 2, 1e-9,
                   QStringLiteral("負倍率被忽略，不會把字變不見"));
    }
    {
        // setFontColor 要保住原本的透明度 —— 次要說明是靠調淡來分層的
        Plot plot;
        plot.title.font.color = QColor(0, 0, 0, 120);
        plot.setFontColor(QColor(200, 30, 30));
        check(plot.title.font.color.alpha() == 120,
              QStringLiteral("setFontColor 保留原本的透明度"),
              QStringLiteral("得到 %1").arg(plot.title.font.color.alpha()));
        check(plot.title.font.color.red() == 200, QStringLiteral("顏色本身有換掉"));
        plot.setFontColor(QColor(10, 10, 10, 200));
        checkClose(plot.title.font.color.alpha(), 200, 0,
                   QStringLiteral("明確給了透明度就照給的"));
    }
    {
        // 轉角度之後軸要變厚：版面沒跟著算的話，轉過的刻度標籤會被切掉
        Plot plot;
        plot.addScatter({0, 1}, {0, 1});
        auto ticks = std::make_shared<ManualTickGenerator>();
        ticks->ticks = {{0, QStringLiteral("很長的類別名稱甲"), true},
                        {1, QStringLiteral("很長的類別名稱乙"), true}};
        plot.bottomAxis()->tickGenerator = ticks;
        plot.setAxisLimits(0, 1, 0, 1);
        plot.renderToImage(500, 400, 1.0);
        const double flatHeight = plot.lastDataRect().height();

        plot.bottomAxis()->tickLabelRotation = -45;
        plot.renderToImage(500, 400, 1.0);
        const double rotatedHeight = plot.lastDataRect().height();
        check(rotatedHeight < flatHeight - 5,
              QStringLiteral("刻度轉 45 度後底軸變厚，繪圖區變矮"),
              QStringLiteral("%1 → %2").arg(flatHeight, 0, 'f', 1)
                  .arg(rotatedHeight, 0, 'f', 1));

        // tickFont.rotation 也要算數，否則 setFontRotation 會漏掉刻度
        plot.bottomAxis()->tickLabelRotation = 0;
        plot.bottomAxis()->tickFont.rotation = -45;
        plot.renderToImage(500, 400, 1.0);
        checkClose(plot.lastDataRect().height(), rotatedHeight, 1e-6,
                   QStringLiteral("tickFont.rotation 和 tickLabelRotation 等效"));

        // 兩個都設時以 tickLabelRotation 為準
        plot.bottomAxis()->tickLabelRotation = -90;
        plot.renderToImage(500, 400, 1.0);
        check(plot.lastDataRect().height() < rotatedHeight,
              QStringLiteral("兩個都設時 tickLabelRotation 優先"));
    }
    {
        // 字變大，軸也要跟著變厚
        Plot plot;
        plot.addScatter({0, 1}, {0, 1});
        plot.leftAxis()->label.text = QStringLiteral("數值");
        plot.renderToImage(500, 400, 1.0);
        const double before = plot.lastDataRect().width();
        plot.scaleFontSizes(2.2);
        plot.renderToImage(500, 400, 1.0);
        check(plot.lastDataRect().width() < before - 10,
              QStringLiteral("字級加倍後左軸變厚，繪圖區變窄"),
              QStringLiteral("%1 → %2").arg(before, 0, 'f', 1)
                  .arg(plot.lastDataRect().width(), 0, 'f', 1));
    }
    {
        // PlotStyle 的字型欄位預設不動任何東西
        Plot plot;
        plot.title.font.size = 33;
        plot.title.font.family = QStringLiteral("Georgia");
        plot.setStyle(PlotStyle::dark());
        checkClose(plot.title.font.size, 33, 1e-9,
                   QStringLiteral("換主題不會動到字級"));
        check(plot.title.font.family == QStringLiteral("Georgia"),
              QStringLiteral("換主題不會動到字型家族"));

        PlotStyle st = PlotStyle::light();
        st.fontFamily = QStringLiteral("Arial");
        st.fontSize = 22;  // 基準 11 → 兩倍
        plot.setStyle(st);
        check(plot.title.font.family == QStringLiteral("Arial"),
              QStringLiteral("主題指定字型家族時會套用"));
        checkClose(plot.title.font.size, 66, 1e-6,
                   QStringLiteral("主題的字級是等比縮放，不是一律設成同一個值"));
    }
}

/// 魚骨圖。
void testFishbone() {
    out() << "魚骨圖\n";

    {
        const QVector<FishboneCategory> six = Fishbone::sixM();
        check(six.size() == 6, QStringLiteral("sixM 給六根骨架"));
        check(six.first().name == QStringLiteral("人"), QStringLiteral("第一根是「人」"));
        check(six.first().causes.isEmpty(), QStringLiteral("骨架是空的，要自己填"));
    }
    {
        Fishbone fb;
        fb.extent = {2, 12, -1, 4};
        const AxisLimits lim = fb.axisLimits();
        checkClose(lim.x.min, 2, 1e-12, QStringLiteral("範圍就是 extent，和資料無關"));
        checkClose(lim.x.max, 12, 1e-12, QStringLiteral("extent 的右界"));
        checkClose(lim.y.min, -1, 1e-12, QStringLiteral("extent 的下界"));
        checkClose(lim.y.max, 4, 1e-12, QStringLiteral("extent 的上界"));
    }
    {
        Fishbone fb;
        fb.categories = {{QStringLiteral("甲"), {}, QColor()},
                         {QString(), {}, QColor()},
                         {QStringLiteral("丙"), {}, QColor()}};
        const QVector<LegendItem> items = fb.legendItems();
        check(items.size() == 2, QStringLiteral("沒有名字的類別不進圖例"),
              QStringLiteral("得到 %1 筆").arg(items.size()));
        check(items.first().text == QStringLiteral("甲"), QStringLiteral("圖例照類別順序"));
    }

    // 像素檢查：脊椎是整張圖最長的一條水平黑線，找它在哪一列
    auto spineRow = [](const QImage& img) {
        int best = -1, bestRun = 0;
        for (int y = 0; y < img.height(); ++y) {
            int run = 0;
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 110 && c.green() < 110 && c.blue() < 110)
                    ++run;
                else
                    run = 0;
                if (run > bestRun) {
                    bestRun = run;
                    best = y;
                }
            }
        }
        return best;
    };

    {
        // 兩側都有骨頭時脊椎在正中間；全部排在上面時要沉到下面，
        // 否則下半張圖整個空著、骨頭卻只分到一半的高度。
        Plot plot;
        auto* fb = plot.addFishbone(QStringLiteral("問題"),
                                    {{QStringLiteral("甲"), {}, QColor()},
                                     {QStringLiteral("乙"), {}, QColor()},
                                     {QStringLiteral("丙"), {}, QColor()},
                                     {QStringLiteral("丁"), {}, QColor()}});
        fb->extent = {0, 10, 0, 6};
        plot.setAxisLimits(0, 10, 0, 6);
        plot.title.text = QString();

        const int both = spineRow(plot.renderToImage(600, 400, 1.0));
        check(std::abs(both - 200) < 40, QStringLiteral("兩側都有骨頭時脊椎在正中間"),
              QStringLiteral("第 %1 列").arg(both));

        fb->alternateSides = false;
        const int oneSide = spineRow(plot.renderToImage(600, 400, 1.0));
        check(oneSide > both + 80, QStringLiteral("全部排上面時脊椎沉到下方"),
              QStringLiteral("%1 → %2").arg(both).arg(oneSide));
    }
    {
        // 最左邊那一根骨頭的原因文字不能被切掉。
        // 版面要把「最長的原因文字」算進去，只算骨頭和橫線的話長名字會消失在圖外。
        auto leftmostInk = [](const QImage& img) {
            for (int x = 0; x < img.width(); ++x)
                for (int y = 0; y < img.height(); ++y) {
                    const QColor c = img.pixelColor(x, y);
                    if (c.red() < 200 || c.green() < 200 || c.blue() < 200) return x;
                }
            return img.width();
        };

        Plot plot;
        auto* fb = plot.addFishbone(
            QStringLiteral("問題"),
            {{QStringLiteral("甲"),
              {{QStringLiteral("一個相當長的原因名稱放在這裡"), {}}},
              QColor()},
             {QStringLiteral("乙"), {{QStringLiteral("短"), {}}}, QColor()}});
        fb->extent = {0, 10, 0, 6};
        plot.setAxisLimits(0, 10, 0, 6);
        plot.title.text = QString();
        const QImage img = plot.renderToImage(800, 500, 1.0);
        check(leftmostInk(img) > 2, QStringLiteral("長原因名沒有被左邊界切掉"),
              QStringLiteral("最左的筆墨在第 %1 行").arg(leftmostInk(img)));
    }
    {
        // 加進去就把座標軸關掉 —— 這是結構圖，留著刻度只會誤導
        Plot plot;
        plot.addFishbone(QStringLiteral("問題"), Fishbone::sixM());
        check(!plot.bottomAxis()->isVisible && !plot.leftAxis()->isVisible,
              QStringLiteral("addFishbone 會把座標軸關掉"));
        check(!plot.grid.isVisible, QStringLiteral("addFishbone 會把格線關掉"));
    }
}

/// 向量輸出（PDF）。
void testVectorExport() {
    out() << "向量輸出\n";

    const QString dir = QDir::tempPath();
    auto isPdf = [](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return false;
        const QByteArray head = f.read(5);
        return head.startsWith("%PDF-");
    };

    {
        const QString path = QDir(dir).filePath(QStringLiteral("qtplot_selftest_plot.pdf"));
        QFile::remove(path);
        Plot plot;
        plot.addScatter({0, 1, 2}, {0, 1, 4});
        plot.title.text = QStringLiteral("PDF");
        check(plot.savePdf(path, 400, 300), QStringLiteral("savePdf 回報成功"));
        check(QFileInfo(path).size() > 500, QStringLiteral("PDF 不是空檔"),
              QStringLiteral("%1 位元組").arg(QFileInfo(path).size()));
        // 檔頭要真的是 PDF。只看「檔案存在」的話，寫出一個空殼也會通過。
        check(isPdf(path), QStringLiteral("檔頭是 %PDF-"));
        QFile::remove(path);
    }
    {
        const QString path = QDir(dir).filePath(QStringLiteral("qtplot_selftest_multi.pdf"));
        QFile::remove(path);
        Multiplot mp(1, 2);
        mp.at(0, 0)->addScatter({0, 1}, {0, 1});
        mp.at(0, 1)->addBars({1, 2, 3});
        check(mp.savePdf(path, 400, 300), QStringLiteral("子圖也存得出 PDF"));
        check(isPdf(path), QStringLiteral("子圖 PDF 的檔頭正確"));
        QFile::remove(path);
    }
    {
        // 寫不進去的路徑要回報失敗，不能默默當成成功
        Plot plot;
        plot.addScatter({0, 1}, {0, 1});
        const QString bad =
            QDir(dir).filePath(QStringLiteral("no_such_dir_qtplot/x.pdf"));
        check(!plot.savePdf(bad, 200, 150), QStringLiteral("路徑不存在時回報失敗"));
    }
}

/// 子圖與嵌入小圖。
void testMultiplot() {
    out() << "子圖\n";

    {
        Multiplot mp(2, 3);
        check(mp.rowCount() == 2 && mp.columnCount() == 3, QStringLiteral("格數就是建構時給的"));
        check(mp.existing(0, 0) == nullptr, QStringLiteral("沒取過的格子是空的"));
        Plot* a = mp.at(0, 0);
        check(a != nullptr, QStringLiteral("at() 會建立那一格"));
        check(mp.at(0, 0) == a, QStringLiteral("再取一次是同一張圖"));
        check(mp.at(5, 5) == nullptr, QStringLiteral("超出範圍回傳 nullptr"));
        check(mp.plots().size() == 1, QStringLiteral("只有建立過的才算數"));
    }
    {
        // 跨格會把被蓋到的格子清掉。留著的話兩張圖會畫在同一塊畫布上互相覆蓋。
        Multiplot mp(2, 2);
        mp.at(0, 0);
        mp.at(0, 1);
        mp.at(1, 0);
        check(mp.plots().size() == 3, QStringLiteral("先建三格"));
        Plot* wide = mp.span(0, 0, 1, 2);
        check(mp.plots().size() == 2, QStringLiteral("跨格把被蓋到的格子移除"),
              QStringLiteral("剩 %1 格").arg(mp.plots().size()));
        check(mp.existing(0, 1) == wide, QStringLiteral("被跨到的格子回傳跨格的那張圖"));
        check(mp.existing(1, 0) != nullptr && mp.existing(1, 0) != wide,
              QStringLiteral("沒被蓋到的格子不受影響"));
    }
    {
        // 格子的像素矩形：等分、間距、權重
        Multiplot mp(1, 2);
        mp.spacingX = 10;
        mp.paddingLeft = mp.paddingRight = mp.paddingTop = mp.paddingBottom = 0;
        const QRectF target(0, 0, 210, 100);
        const QRectF left = mp.cellRect(target, 0, 0);
        const QRectF right = mp.cellRect(target, 0, 1);
        checkClose(left.width(), 100, 1e-9, QStringLiteral("兩欄等分再扣掉間距"));
        checkClose(right.left(), 110, 1e-9, QStringLiteral("第二欄從間距之後開始"));

        mp.columnWeights = {3.0, 1.0};
        checkClose(mp.cellRect(target, 0, 0).width(), 150, 1e-9,
                   QStringLiteral("權重 3:1 時第一欄佔四分之三"));
        checkClose(mp.cellRect(target, 0, 1).width(), 50, 1e-9,
                   QStringLiteral("第二欄佔四分之一"));

        // 跨格的寬度要含中間那條間距，否則兩格併一格會少一截
        Multiplot mp2(1, 2);
        mp2.spacingX = 10;
        mp2.paddingLeft = mp2.paddingRight = mp2.paddingTop = mp2.paddingBottom = 0;
        checkClose(mp2.cellRect(target, 0, 0, 1, 2).width(), 210, 1e-9,
                   QStringLiteral("跨兩欄的寬度含中間的間距"));
    }

    {
        // 對齊：兩張子圖的 Y 刻度位數差很多，繪圖區左緣仍然要對齊。
        // 這是並排比較最顯眼的瑕疵 —— 錯位比資料本身的差異還搶眼。
        // 刻度標籤的寬度要用 ManualTickGenerator 釘死。
        // 用「0–2 對上百萬」是不行的：自動刻度會把 10⁶ 抽成倍率標示，
        // 兩邊的標籤同樣是「1.0 / 2.0」，寬度一樣，根本測不到對齊。
        Multiplot mp(2, 1);
        mp.at(0, 0)->addScatter({0, 1}, {0, 1});
        mp.at(1, 0)->addScatter({0, 1}, {0, 1});
        auto shortTicks = std::make_shared<ManualTickGenerator>();
        shortTicks->ticks = {{0, QStringLiteral("0"), true}, {1, QStringLiteral("1"), true}};
        auto longTicks = std::make_shared<ManualTickGenerator>();
        longTicks->ticks = {{0, QStringLiteral("非常非常長的刻度標籤"), true},
                            {1, QStringLiteral("另一個很長的標籤"), true}};
        mp.at(0, 0)->leftAxis()->tickGenerator = shortTicks;
        mp.at(1, 0)->leftAxis()->tickGenerator = longTicks;

        mp.alignAxes = false;
        mp.renderToImage(600, 400, 1.0);
        const double offLeftA = mp.at(0, 0)->lastDataRect().left;
        const double offLeftB = mp.at(1, 0)->lastDataRect().left;
        check(std::abs(offLeftA - offLeftB) > 5,
              QStringLiteral("關掉對齊時左緣確實會差一截"),
              QStringLiteral("%1 vs %2").arg(offLeftA, 0, 'f', 1).arg(offLeftB, 0, 'f', 1));

        mp.alignAxes = true;
        mp.renderToImage(600, 400, 1.0);
        const double onLeftA = mp.at(0, 0)->lastDataRect().left;
        const double onLeftB = mp.at(1, 0)->lastDataRect().left;
        checkClose(onLeftA, onLeftB, 0.5, QStringLiteral("打開對齊之後左緣一致"));
        const double onRightA = mp.at(0, 0)->lastDataRect().right;
        const double onRightB = mp.at(1, 0)->lastDataRect().right;
        checkClose(onRightA, onRightB, 0.5, QStringLiteral("右緣也一致"));

        // 對齊是靠暫時加大 padding 做的，畫完必須還原。
        // 不還原的話每畫一次就再縮一次，圖會一幀比一幀小。
        const double before = mp.at(0, 0)->lastDataRect().width();
        mp.renderToImage(600, 400, 1.0);
        mp.renderToImage(600, 400, 1.0);
        checkClose(mp.at(0, 0)->lastDataRect().width(), before, 1e-6,
                   QStringLiteral("重複 render 不會愈畫愈小"));
    }
    {
        // 共用軸：範圍要變成聯集，而不是各自貼合自己的資料
        Multiplot mp(1, 2);
        Plot* a = mp.at(0, 0);
        Plot* b = mp.at(0, 1);
        a->addBars({10, 12, 11});
        b->addBars({90, 95, 88});
        mp.renderToImage(600, 400, 1.0);
        check(a->leftAxis()->range.max < 50,
              QStringLiteral("沒共用時各自貼合自己的資料"));

        mp.shareY({a, b});
        mp.renderToImage(600, 400, 1.0);
        checkClose(a->leftAxis()->range.max, b->leftAxis()->range.max, 1e-6,
                   QStringLiteral("共用 Y 之後兩邊的上界一樣"));
        checkClose(a->leftAxis()->range.min, b->leftAxis()->range.min, 1e-6,
                   QStringLiteral("下界也一樣"));
        check(a->leftAxis()->range.max > 90,
              QStringLiteral("共用的範圍是聯集，含得下較大的那組"));

        mp.clearSharing();
        check(mp.plots().size() == 2, QStringLiteral("清掉共用設定不影響子圖"));
    }
    {
        // visitFonts 要走到每一張子圖
        Multiplot mp(1, 2);
        mp.title.text = QStringLiteral("總標題");
        mp.at(0, 0)->addScatter({0, 1}, {0, 1});
        mp.at(0, 1)->addBars({1, 2});
        mp.visitFonts([](FontStyle& f) { f.family = QStringLiteral("Courier New"); });
        int wrong = 0;
        mp.visitFonts([&](FontStyle& f) {
            if (f.family != QStringLiteral("Courier New")) ++wrong;
        });
        check(wrong == 0, QStringLiteral("visitFonts 走到每一張子圖"),
              QStringLiteral("%1 處沒改到").arg(wrong));
    }
    {
        // 嵌入小圖不該影響主圖的自動範圍 ——
        // 算進去的話，主圖會為了容納一個貼在角落的裝飾而改變視野。
        Plot plot;
        plot.addScatter({0, 1, 2}, {0, 1, 2});
        auto* inset = addInset(plot);
        inset->inner().addScatter({500, 600}, {800, 900});
        plot.autoScale(0.0, true);
        check(plot.bottomAxis()->range.max < 10,
              QStringLiteral("小圖的內容不會把主圖的範圍撐開"),
              QStringLiteral("上界 %1").arg(plot.bottomAxis()->range.max, 0, 'f', 1));

        // 改用資料座標定位時才算進來 —— 那時它確實佔了主圖的一塊地方
        inset->useDataCoordinates = true;
        inset->dataRect = {5, 9, 5, 9};
        plot.autoScale(0.0, true);
        check(plot.bottomAxis()->range.max >= 9,
              QStringLiteral("資料座標定位的小圖會算進範圍"));
    }
    {
        // 小圖真的畫出來了：內層填成純紅，數一數畫面上有多少紅
        Plot plot;
        plot.addScatter({0, 1}, {0, 1});
        auto* inset = addInset(plot);
        inset->rectFraction = QRectF(0.1, 0.1, 0.5, 0.5);
        inset->showSourceRegion = false;
        // 兩個底色都要設。只設 figureBackground 的話，資料區會再鋪一層
        // dataBackground（預設白色）蓋掉它，剩下的紅只有外圈那一圈留白。
        inset->inner().style.figureBackground = QColor(255, 0, 0);
        inset->inner().style.dataBackground = QColor(255, 0, 0);
        inset->inner().bottomAxis()->isVisible = false;
        inset->inner().leftAxis()->isVisible = false;
        inset->inner().grid.isVisible = false;
        plot.setAxisLimits(0, 1, 0, 1);

        const QImage img = plot.renderToImage(400, 400, 1.0);
        int red = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 200 && c.green() < 60 && c.blue() < 60) ++red;
            }
        check(red > 5000, QStringLiteral("小圖確實畫在主圖上"),
              QStringLiteral("紅色像素 %1").arg(red));

        inset->isVisible = false;
        const QImage off = plot.renderToImage(400, 400, 1.0);
        int red2 = 0;
        for (int y = 0; y < off.height(); ++y)
            for (int x = 0; x < off.width(); ++x) {
                const QColor c = off.pixelColor(x, y);
                if (c.red() > 200 && c.green() < 60 && c.blue() < 60) ++red2;
            }
        check(red2 == 0, QStringLiteral("關掉就不畫"));
    }
}

/// 柏拉圖、鬆餅圖、斜率／排名圖。
void testBusinessCharts2() {
    out() << "柏拉圖／鬆餅／排名\n";

    {
        Pareto pr;
        pr.items = {{QStringLiteral("乙"), 20, QColor()},
                    {QStringLiteral("甲"), 50, QColor()},
                    {QStringLiteral("丁"), 5, QColor()},
                    {QStringLiteral("丙"), 25, QColor()}};
        checkClose(pr.total(), 100, 1e-12, QStringLiteral("總和"));
        const QVector<ParetoItem> sorted = pr.sortedItems();
        check(sorted.size() == 4 && sorted[0].label == QStringLiteral("甲") &&
                  sorted[3].label == QStringLiteral("丁"),
              QStringLiteral("由大到小排序"));
        const QVector<double> cum = pr.cumulativePercents();
        checkClose(cum[0], 50, 1e-9, QStringLiteral("第一項的累積是 50%"));
        checkClose(cum[1], 75, 1e-9, QStringLiteral("前兩項 75%"));
        checkClose(cum.last(), 100, 1e-9, QStringLiteral("最後一項是 100%"));
        check(pr.itemsToReach(80) == 3, QStringLiteral("要三項才到 80%"),
              QStringLiteral("得到 %1").arg(pr.itemsToReach(80)));
        check(pr.itemsToReach(50) == 1, QStringLiteral("一項就到 50%"));

        pr.sortDescending = false;
        check(pr.sortedItems()[0].label == QStringLiteral("乙"),
              QStringLiteral("關掉排序就照原順序"));
        // 負值與非有限值不該進來
        pr.items.append({QStringLiteral("壞"), -5, QColor()});
        pr.items.append({QStringLiteral("NaN"), std::numeric_limits<double>::quiet_NaN(),
                         QColor()});
        check(pr.sortedItems().size() == 4, QStringLiteral("負值與 NaN 被跳過"));
        check(Pareto().cumulativePercents().isEmpty(), QStringLiteral("空資料回傳空"));
    }

    {
        // 鬆餅圖的格子配額：總數必須剛好等於格子數。
        // 直接四捨五入的話，十個 3.33% 可以湊出 30 格也可以湊出 40 格。
        Waffle w;
        w.rows = 10;
        w.columns = 10;
        w.categories = {{QStringLiteral("甲"), 1, QColor()},
                        {QStringLiteral("乙"), 1, QColor()},
                        {QStringLiteral("丙"), 1, QColor()}};
        const QVector<int> cells = w.cellAssignment();
        check(cells.size() == 100, QStringLiteral("格子數 = rows × columns"));
        int empty = 0;
        QVector<int> counts(3, 0);
        for (int c : cells) {
            if (c < 0) ++empty;
            else ++counts[c];
        }
        check(empty == 0, QStringLiteral("三等分 100 格時沒有剩餘的空格"),
              QStringLiteral("空格 %1").arg(empty));
        check(counts[0] + counts[1] + counts[2] == 100, QStringLiteral("配額總和等於格子數"));
        check(std::abs(counts[0] - counts[1]) <= 1 && std::abs(counts[1] - counts[2]) <= 1,
              QStringLiteral("最大餘額法讓三者只差一格"),
              QStringLiteral("%1 / %2 / %3").arg(counts[0]).arg(counts[1]).arg(counts[2]));

        // 不滿 100% 的話剩下的要留白
        w.categories = {{QStringLiteral("甲"), 30, QColor()}};
        int filled = 0;
        for (int c : w.cellAssignment())
            if (c >= 0) ++filled;
        check(filled == 100, QStringLiteral("只有一類時它佔滿全部格子"));

        w.categories = {{QStringLiteral("甲"), 30, QColor()},
                        {QStringLiteral("空"), 70, QColor()}};
        int first = 0;
        for (int c : w.cellAssignment())
            if (c == 0) ++first;
        check(first == 30, QStringLiteral("30/70 分出 30 格"),
              QStringLiteral("得到 %1").arg(first));

        Waffle empty2;
        check(empty2.cellAssignment().size() == 100,
              QStringLiteral("沒有資料時仍然回傳正確數量的空格"));
    }

    {
        Slope sl;
        sl.periodLabels = {QStringLiteral("前"), QStringLiteral("後")};
        sl.series = {{QStringLiteral("甲"), {10, 30}, QColor(), false},
                     {QStringLiteral("乙"), {20, 20}, QColor(), false},
                     {QStringLiteral("丙"), {30, 10}, QColor(), false}};
        const QVector<QVector<double>> r = sl.ranks();
        check(r[0][0] == 3 && r[0][1] == 1, QStringLiteral("甲從第 3 名升到第 1 名"),
              QStringLiteral("%1 → %2").arg(r[0][0]).arg(r[0][1]));
        check(r[2][0] == 1 && r[2][1] == 3, QStringLiteral("丙從第 1 名掉到第 3 名"));
        check(r[1][0] == 2 && r[1][1] == 2, QStringLiteral("乙維持第 2 名"));

        // 值相同時照索引排，名次才不會隨輸入順序變
        Slope tie;
        tie.periodLabels = {QStringLiteral("一"), QStringLiteral("二")};
        tie.series = {{QStringLiteral("甲"), {5, 5}, QColor(), false},
                      {QStringLiteral("乙"), {5, 5}, QColor(), false}};
        check(tie.ranks()[0][0] == 1 && tie.ranks()[1][0] == 2,
              QStringLiteral("同分時照索引決定名次"));

        sl.useRanks = false;
        check(sl.plotValues()[0][1] == 30, QStringLiteral("不用名次時畫的是原始值"));
        sl.useRanks = true;
        check(sl.plotValues()[0][1] == 1, QStringLiteral("用名次時畫的是名次"));
    }
}

/// 山脊圖、日曆熱圖、沖積圖。
void testMoreTypes() {
    out() << "山脊／日曆／沖積\n";

    {
        Ridgeline r;
        r.groups = {{QStringLiteral("甲"), {1, 2, 3}, QColor()},
                    {QStringLiteral("乙"), {2, 3, 4}, QColor()},
                    {QStringLiteral("丙"), {3, 4, 5}, QColor()}};
        r.topToBottom = true;
        const QVector<double> pos = r.groupPositions();
        check(pos[0] == 2 && pos[2] == 0, QStringLiteral("第一組排在最上面"));
        const QStringList labels = r.orderedLabels();
        check(labels[0] == QStringLiteral("丙") && labels[2] == QStringLiteral("甲"),
              QStringLiteral("組名依位置 0,1,2 排好"), labels.join(QLatin1Char('/')));

        const AxisLimits lim = r.axisLimits();
        // 最上面那條的峰頂會長到基線之上 (1 + overlap) 格，不留出來就會被切掉
        check(lim.y.max >= 2 + 1 + r.overlap - 1e-9,
              QStringLiteral("Y 範圍留得下最上面那條的峰"),
              QStringLiteral("上界 %1").arg(lim.y.max, 0, 'f', 2));
        check(lim.x.isValid(), QStringLiteral("X 範圍涵蓋密度曲線的取樣範圍"));
        check(Ridgeline().axisLimits().hasData() == false, QStringLiteral("空資料沒有範圍"));
    }

    {
        CalendarHeatmap cal;
        cal.weekStartsMonday = true;
        cal.firstDate = QDate(2025, 1, 1);   // 星期三
        cal.lastDate = QDate(2025, 1, 31);
        check(cal.weekCount() == 5, QStringLiteral("2025 年 1 月跨 5 個週一起算的週"),
              QStringLiteral("得到 %1").arg(cal.weekCount()));

        const QPoint c1 = cal.cellFor(QDate(2025, 1, 1));
        check(c1.x() == 0 && c1.y() == 2, QStringLiteral("1/1 是第 0 週的星期三"),
              QStringLiteral("(%1, %2)").arg(c1.x()).arg(c1.y()));
        const QPoint c2 = cal.cellFor(QDate(2025, 1, 6));
        check(c2.x() == 1 && c2.y() == 0, QStringLiteral("1/6 是第 1 週的星期一"),
              QStringLiteral("(%1, %2)").arg(c2.x()).arg(c2.y()));
        check(cal.cellFor(QDate(2024, 12, 31)).x() == -1,
              QStringLiteral("範圍外的日期回傳 (-1, -1)"));

        // 換成週日起算，同一天的位置要跟著變
        cal.weekStartsMonday = false;
        const QPoint c3 = cal.cellFor(QDate(2025, 1, 1));
        check(c3.y() == 3, QStringLiteral("週日起算時 1/1 變成第 3 列"),
              QStringLiteral("列 %1").arg(c3.y()));

        // 沒給範圍時用資料的頭尾
        CalendarHeatmap auto1;
        auto1.values = {{QDate(2025, 3, 5), 1}, {QDate(2025, 3, 1), 2}, {QDate(2025, 3, 9), 3}};
        check(auto1.effectiveFirst() == QDate(2025, 3, 1), QStringLiteral("自動取最早的日期"));
        check(auto1.effectiveLast() == QDate(2025, 3, 9), QStringLiteral("自動取最晚的日期"));
        checkClose(auto1.effectiveColorRange().min, 1, 1e-12, QStringLiteral("色階下界"));
        checkClose(auto1.effectiveColorRange().max, 3, 1e-12, QStringLiteral("色階上界"));
        check(CalendarHeatmap().weekCount() == 0, QStringLiteral("沒有資料時是 0 週"));
    }

    {
        const QVector<QStringList> records{
            {QStringLiteral("北"), QStringLiteral("A"), QStringLiteral("續")},
            {QStringLiteral("北"), QStringLiteral("A"), QStringLiteral("續")},
            {QStringLiteral("北"), QStringLiteral("B"), QStringLiteral("走")},
            {QStringLiteral("南"), QStringLiteral("A"), QStringLiteral("走")},
        };
        const QStringList stages{QStringLiteral("區"), QStringLiteral("案"),
                                 QStringLiteral("果")};
        const AlluvialResult al = alluvialFromRecords(records, stages);

        // 節點：區(北,南) + 案(A,B) + 果(續,走) = 6
        check(al.nodes.size() == 6, QStringLiteral("三階段共六個節點"),
              QStringLiteral("得到 %1").arg(al.nodes.size()));
        check(al.nodes[0].label == QStringLiteral("北") && al.nodes[0].layer == 0,
              QStringLiteral("第一個節點是第一階段的第一個分類"));
        // 分類照第一次出現的順序，不是字典序
        check(al.nodes[1].label == QStringLiteral("南"), QStringLiteral("照出現順序排"));

        double total = 0;
        for (const SankeyLink& l : al.links) total += l.value;
        // 四筆紀錄 × 兩段轉換 = 8
        checkClose(total, 8, 1e-12, QStringLiteral("流量總和 = 筆數 × 轉換段數"));

        // 北→A 應該有 2
        double northToA = 0;
        for (const SankeyLink& l : al.links)
            if (al.nodes[l.from].label == QStringLiteral("北") &&
                al.nodes[l.to].label == QStringLiteral("A"))
                northToA = l.value;
        checkClose(northToA, 2, 1e-12, QStringLiteral("北→A 有兩筆"));

        check(alluvialFromRecords(records, {QStringLiteral("只有一階")}).nodes.isEmpty(),
              QStringLiteral("少於兩階段時回傳空"));
        // 長度不符的紀錄要被跳過，不能當掉
        QVector<QStringList> bad = records;
        bad.append({QStringLiteral("東")});
        check(alluvialFromRecords(bad, stages).nodes.size() == 6,
              QStringLiteral("長度不符的紀錄被跳過"));
    }
}

/// 一致性、偏倚、不均等、機率紙、立體曲面。
void testDiagnostics() {
    out() << "診斷與推論（第二批）\n";

    {
        // 兩法差了固定的 5：相關係數是 1，但它們完全不能互換 ——
        // 這正是不能用相關判斷一致性的理由。
        QVector<double> a, b;
        for (int i = 0; i < 40; ++i) {
            a.append(i * 1.0);
            b.append(i * 1.0 - 5.0);
        }
        const BlandAltman ba = blandAltman(a, b);
        checkClose(ba.bias, 5.0, 1e-12, QStringLiteral("偏差抓得出固定的 5"));
        checkClose(ba.sd, 0.0, 1e-12, QStringLiteral("差完全一致時標準差為 0"));
        checkClose(linearFit(a, b).rSquared, 1.0, 1e-9,
                   QStringLiteral("同一組資料的相關卻是完美的"));
        check(ba.count == 40, QStringLiteral("筆數"));
        checkClose(ba.means.first(), -2.5, 1e-12, QStringLiteral("第一點的平均"));

        // 界限是 bias ± 1.96·sd
        QVector<double> c{1, 2, 3, 4, 5}, d{2, 2, 3, 4, 6};
        const BlandAltman ba2 = blandAltman(c, d);
        checkClose(ba2.upperLimit - ba2.bias, 1.959963985 * ba2.sd, 1e-9,
                   QStringLiteral("上界是 bias + 1.96·sd"));
        check(blandAltman({}, {}).count == 0, QStringLiteral("空輸入回傳 0 筆"));
        check(blandAltman({1, 2, 3}, {1, 2}).count == 2,
              QStringLiteral("長度不同時取較短的"));
    }

    {
        // 對稱的研究：Egger 截距應該接近 0
        QVector<MetaStudy> sym;
        for (int i = 1; i <= 10; ++i) {
            const double se = 0.05 * i;
            sym.append({0.5 + se, se});
            sym.append({0.5 - se, se});
        }
        const FunnelContour fc = funnelContour(sym);
        checkClose(fc.pooledEffect, 0.5, 1e-9, QStringLiteral("對稱時合併值在中心"));
        check(std::abs(fc.eggerIntercept) < 1e-6,
              QStringLiteral("對稱時 Egger 截距接近 0"),
              QStringLiteral("截距 %1").arg(fc.eggerIntercept, 0, 'g', 4));

        // 不對稱：小研究只留效果大的那一側
        QVector<MetaStudy> skew;
        for (int i = 1; i <= 10; ++i) {
            const double se = 0.05 * i;
            skew.append({0.5 + se * 1.5, se});
        }
        check(std::abs(funnelContour(skew).eggerIntercept) > 0.5,
              QStringLiteral("單側缺漏時 Egger 截距明顯偏離 0"),
              QStringLiteral("截距 %1")
                  .arg(funnelContour(skew).eggerIntercept, 0, 'f', 3));

        check(fc.seLevels.size() == fc.leftBound95.size(),
              QStringLiteral("漏斗兩邊和取樣點等長"));
        check(fc.leftBound95.first() < fc.rightBound95.first() + 1e-12,
              QStringLiteral("左界不大於右界"));
        checkClose(fc.leftBound95.first(), fc.pooledEffect, 1e-12,
                   QStringLiteral("SE = 0 時漏斗收斂到合併值"));
        check(funnelContour({}).seLevels.isEmpty(), QStringLiteral("空輸入回傳空"));
    }

    {
        // 完全平均：勞倫茲曲線就是對角線，吉尼 0
        QVector<double> flat(100, 7.0);
        const LorenzCurve lc = lorenzCurve(flat);
        checkClose(lc.gini, 0.0, 1e-9, QStringLiteral("完全平均時吉尼為 0"));
        check(lc.populationFraction.size() == 101,
              QStringLiteral("曲線含起點 (0, 0)"));
        checkClose(lc.valueFraction.last(), 1.0, 1e-12, QStringLiteral("終點是 (1, 1)"));

        // 全部集中在一個人身上：吉尼接近 1（n 有限所以到不了 1）
        QVector<double> one(100, 0.0);
        one[99] = 1000;
        const LorenzCurve lc2 = lorenzCurve(one);
        check(lc2.gini > 0.98, QStringLiteral("完全集中時吉尼接近 1"),
              QStringLiteral("得到 %1").arg(lc2.gini, 0, 'f', 4));

        // 不均的那組吉尼要比平均的那組大
        QVector<double> uneven;
        for (int i = 1; i <= 100; ++i) uneven.append(i);
        const double g = lorenzCurve(uneven).gini;
        check(g > 0.2 && g < 0.5, QStringLiteral("線性遞增的吉尼落在合理區間"),
              QStringLiteral("得到 %1").arg(g, 0, 'f', 4));

        check(lorenzCurve({}).populationFraction.isEmpty(), QStringLiteral("空輸入回傳空"));
        check(lorenzCurve({-1, -2}).populationFraction.isEmpty(),
              QStringLiteral("全是負值時回傳空"));
    }

    {
        // 機率紙：用已知參數生出 Weibull 樣本，看估回來的 β 準不準
        const double beta = 2.5, eta = 800;
        QVector<double> x;
        for (int i = 1; i <= 400; ++i) {
            const double u = static_cast<double>(i) / 401.0;
            x.append(eta * std::pow(-std::log(1 - u), 1.0 / beta));
        }
        const ProbabilityPlot pp = probabilityPlot(x, ProbabilityPaper::Weibull);
        check(pp.xs.size() == 400, QStringLiteral("點數等於樣本數"));
        checkClose(pp.weibullShape, beta, 0.15, QStringLiteral("估回來的 β 接近真值"));
        checkClose(pp.weibullScale, eta, 60, QStringLiteral("估回來的 η 接近真值"));

        // 常態紙：常態樣本的參考線斜率是 1/σ
        QVector<double> normal;
        for (int i = 1; i <= 399; ++i) normal.append(normalQuantile(i / 400.0, 10, 2));
        const ProbabilityPlot np = probabilityPlot(normal, ProbabilityPaper::Normal);
        checkClose(np.referenceLine.slope, 0.5, 0.02,
                   QStringLiteral("常態紙的斜率是 1/σ"));
        checkClose(np.referenceLine.predict(10.0), 0.0, 0.05,
                   QStringLiteral("平均值對應到 Φ⁻¹(0.5) = 0"));

        // Weibull 紙要取 log，非正值進不來
        check(probabilityPlot({-1, 0, 5, 7}, ProbabilityPaper::Weibull).xs.size() == 2,
              QStringLiteral("Weibull 紙跳過非正值"));
        check(probabilityPlot({1}, ProbabilityPaper::Normal).xs.isEmpty(),
              QStringLiteral("樣本太少時回傳空"));
    }

    {
        // 立體曲面：投影與範圍
        Surface3D s;
        s.rows = s.cols = 3;
        s.values = QVector<double>(9, 0.0);
        s.values[4] = 5.0;  // 中間凸起
        s.xRange = {0, 2};
        s.yRange = {0, 2};
        checkClose(s.value(1, 1), 5.0, 1e-12, QStringLiteral("value() 取得到中間那格"));
        check(std::isnan(s.value(9, 9)), QStringLiteral("超出範圍回傳 NaN"));
        checkClose(s.zRange().max, 5.0, 1e-12, QStringLiteral("高度範圍的上界"));

        const AxisLimits lim = s.axisLimits();
        check(lim.hasData(), QStringLiteral("有資料時回報範圍"));
        // 等角投影：+x 往右下、+y 往左下，所以最高點在上面
        check(lim.y.max > 0, QStringLiteral("凸起讓投影後的上界為正"));

        Surface3D tooSmall;
        tooSmall.rows = tooSmall.cols = 1;
        tooSmall.values = {1.0};
        check(!tooSmall.axisLimits().hasData(),
              QStringLiteral("格網小於 2×2 時沒有範圍"));
    }
}

/// 圖例點擊切換與視野歷程。
void testInteractionState() {
    out() << "互動狀態\n";

    {
        // 被隱藏的圖元仍然要出現在圖例裡（畫成灰的），
        // 否則點一下關掉之後那一列就消失，再也點不回來。
        Plot plot;
        plot.legend.isVisible = true;
        auto* a = plot.addScatter({0, 1}, {0, 1});
        a->legendText = QStringLiteral("甲");
        auto* b = plot.addScatter({0, 1}, {1, 0});
        b->legendText = QStringLiteral("乙");

        plot.renderToImage(400, 300, 1.0);
        check(plot.legendHitAreas().size() == 2, QStringLiteral("圖例記下兩列的位置"),
              QStringLiteral("得到 %1").arg(plot.legendHitAreas().size()));
        check(plot.legendHitAreas()[0].plottable == a,
              QStringLiteral("第一列對應第一個圖元"));

        b->isVisible = false;
        plot.renderToImage(400, 300, 1.0);
        check(plot.legendHitAreas().size() == 2,
              QStringLiteral("隱藏之後那一列還在"),
              QStringLiteral("得到 %1").arg(plot.legendHitAreas().size()));

        plot.legend.showHiddenSeries = false;
        plot.renderToImage(400, 300, 1.0);
        check(plot.legendHitAreas().size() == 1,
              QStringLiteral("關掉 showHiddenSeries 之後才會消失"));

        // 命中查詢
        plot.legend.showHiddenSeries = true;
        plot.renderToImage(400, 300, 1.0);
        const QRectF row0 = plot.legendHitAreas()[0].rect;
        check(plot.legendPlottableAt(row0.center()) == a,
              QStringLiteral("點在第一列的範圍內就命中第一個圖元"));
        check(plot.legendPlottableAt(QPointF(-100, -100)) == nullptr,
              QStringLiteral("點在圖例外面不命中"));
    }
}

void testMarchingSquares() {
    out() << "等值線（marching squares）\n";
    const CoordinateRect area(0, 1, 0, 1);

    // f(x, y) = x：level 0.5 的等值線應該是 x = 0.5 的一條直線
    {
        const int n = 21;
        const QVector<double> v = sampleField([](double x, double) { return x; }, area, n, n);
        const auto lines = marchingSquares(v, n, n, 0.5, area);
        check(lines.size() == 1, QStringLiteral("線性場只產生一條等值線"),
              QStringLiteral("得到 %1 條").arg(lines.size()));
        bool onLine = true;
        double yMin = 1e30, yMax = -1e30;
        for (const auto& poly : lines)
            for (const Coordinates& c : poly) {
                onLine = onLine && std::abs(c.x - 0.5) < 1e-9;
                yMin = std::min(yMin, c.y);
                yMax = std::max(yMax, c.y);
            }
        check(onLine, QStringLiteral("等值線上每一點的 x 都等於 0.5"));
        check(std::abs(yMin - 0.0) < 1e-9 && std::abs(yMax - 1.0) < 1e-9,
              QStringLiteral("等值線貫穿整個範圍"),
              QStringLiteral("y 從 %1 到 %2").arg(yMin).arg(yMax));
    }

    // f = x² + y² 在 (0,0) 附近：level r² 應該是半徑 r 的圓
    {
        const int n = 81;
        const CoordinateRect box(-1, 1, -1, 1);
        const QVector<double> v =
            sampleField([](double x, double y) { return x * x + y * y; }, box, n, n);
        const double r = 0.6;
        const auto lines = marchingSquares(v, n, n, r * r, box);
        check(lines.size() == 1, QStringLiteral("圓形等值線接成一條折線"),
              QStringLiteral("得到 %1 條").arg(lines.size()));
        if (!lines.isEmpty()) {
            const auto& poly = lines.first();
            double worst = 0;
            for (const Coordinates& c : poly) worst = std::max(worst, std::abs(std::hypot(c.x, c.y) - r));
            // 線段逼近的誤差約為格距的平方項，格距 0.025 時誤差應該遠小於 0.01
            check(worst < 0.01, QStringLiteral("圓上每一點到原點的距離都接近 r"),
                  QStringLiteral("最大偏差 %1").arg(worst, 0, 'g', 4));
            const double closeGap = std::hypot(poly.first().x - poly.last().x,
                                               poly.first().y - poly.last().y);
            check(closeGap < 1e-6, QStringLiteral("封閉曲線的頭尾相接"),
                  QStringLiteral("頭尾距離 %1").arg(closeGap, 0, 'g', 4));
        }
    }

    // 超出資料範圍的 level 不該產生任何線
    {
        const int n = 11;
        const QVector<double> v = sampleField([](double x, double) { return x; }, area, n, n);
        check(marchingSquares(v, n, n, 5.0, area).isEmpty(),
              QStringLiteral("level 超出資料範圍時沒有線"));
    }

    // NaN 的格子要被跳過，不能憑空生出輪廓
    {
        const int n = 11;
        QVector<double> v = sampleField([](double x, double) { return x; }, area, n, n);
        for (int r = 0; r < n; ++r) v[r * n + 5] = std::numeric_limits<double>::quiet_NaN();
        const auto lines = marchingSquares(v, n, n, 0.5, area);
        bool noNanNear = true;
        for (const auto& poly : lines)
            for (const Coordinates& c : poly) noNanNear = noNanNear && std::isfinite(c.x) &&
                                                          std::isfinite(c.y);
        check(noNanNear, QStringLiteral("NaN 區域不會產生非有限的點"));
        check(lines.isEmpty(), QStringLiteral("等值正好落在 NaN 那一行時完全跳過"),
              QStringLiteral("得到 %1 條").arg(lines.size()));
    }
}

// ───────────────────── 特殊座標與三角化 ─────────────────────

void testSpecialty() {
    out() << "特殊座標與三角化\n";

    // ── Delaunay ──
    {
        const QVector<Coordinates> square{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        check(delaunay(square).size() == 2, QStringLiteral("正方形切成兩個三角形"),
              QStringLiteral("得到 %1 個").arg(delaunay(square).size()));
    }
    {
        // 全部落在圓周上 → 都是凸包點 → 三角形數 = n - 2
        QVector<Coordinates> ring;
        const int n = 12;
        for (int i = 0; i < n; ++i) ring.append(fromPolar(1.0, 360.0 * i / n));
        const auto tris = delaunay(ring);
        check(tris.size() == n - 2, QStringLiteral("圓周上 n 點切成 n-2 個三角形"),
              QStringLiteral("n=%1 得到 %2 個").arg(n).arg(tris.size()));
    }
    {
        // Delaunay 的定義：任何一點都不能落在其他三角形的外接圓內
        gen::setSeed(17);
        QVector<Coordinates> pts;
        const QVector<double> xs = gen::randomUniform(40, 0, 10);
        const QVector<double> ys = gen::randomUniform(40, 0, 10);
        for (int i = 0; i < xs.size(); ++i) pts.append({xs[i], ys[i]});

        const auto tris = delaunay(pts);
        check(!tris.isEmpty(), QStringLiteral("散點有三角化結果"));

        int violations = 0;
        for (const Triangle& t : tris) {
            // 外接圓圓心與半徑
            const Coordinates& A = pts[t.a];
            const Coordinates& B = pts[t.b];
            const Coordinates& C = pts[t.c];
            const double d = 2 * (A.x * (B.y - C.y) + B.x * (C.y - A.y) + C.x * (A.y - B.y));
            if (std::abs(d) < 1e-12) continue;
            const double ux = ((A.x * A.x + A.y * A.y) * (B.y - C.y) +
                               (B.x * B.x + B.y * B.y) * (C.y - A.y) +
                               (C.x * C.x + C.y * C.y) * (A.y - B.y)) / d;
            const double uy = ((A.x * A.x + A.y * A.y) * (C.x - B.x) +
                               (B.x * B.x + B.y * B.y) * (A.x - C.x) +
                               (C.x * C.x + C.y * C.y) * (B.x - A.x)) / d;
            const double rad = std::hypot(A.x - ux, A.y - uy);
            for (int i = 0; i < pts.size(); ++i) {
                if (i == t.a || i == t.b || i == t.c) continue;
                // 容差放寬一點，避免浮點邊界誤判
                if (std::hypot(pts[i].x - ux, pts[i].y - uy) < rad - 1e-9) ++violations;
            }
        }
        check(violations == 0, QStringLiteral("滿足 Delaunay 條件（沒有點落在外接圓內）"),
              QStringLiteral("%1 次違反").arg(violations));

        // 不能有退化（重複索引）的三角形
        bool degenerate = false;
        for (const Triangle& t : tris)
            degenerate = degenerate || t.a == t.b || t.b == t.c || t.a == t.c;
        check(!degenerate, QStringLiteral("沒有退化的三角形"));

        // 輸出必須是確定的順序。內部用 QHash 蒐集洞的邊界，而 Qt 的 QHash
        // 每個行程的走訪順序不同 —— 不正規化的話同一支程式跑兩次會畫出
        // 色階差 1 的 PNG，讓「匯出兩次比對雜湊」的回歸檢查每次誤報。
        bool canonical = true;
        for (const Triangle& t : tris) canonical = canonical && t.a < t.b && t.b < t.c;
        check(canonical, QStringLiteral("每個三角形的索引遞增（輸出可重現）"));
        bool sorted = true;
        for (int i = 1; i < tris.size(); ++i) {
            const Triangle& x = tris[i - 1];
            const Triangle& y = tris[i];
            const bool le = (x.a != y.a) ? (x.a < y.a) : (x.b != y.b) ? (x.b < y.b) : (x.c <= y.c);
            sorted = sorted && le;
        }
        check(sorted, QStringLiteral("三角形清單已排序（輸出可重現）"));
    }
    {
        // 共線的點沒有唯一三角化，應該回傳空的而不是硬算
        QVector<Coordinates> line;
        for (int i = 0; i < 6; ++i) line.append({double(i), double(i) * 2});
        check(delaunay(line).isEmpty(), QStringLiteral("共線的點不產生三角形"));
        check(delaunay({{0, 0}, {1, 1}}).isEmpty(), QStringLiteral("少於 3 點不產生三角形"));
    }

    // ── 三角圖座標 ──
    {
        auto c0 = TriangularAxis::toCartesian(1, 0, 0);
        auto c1 = TriangularAxis::toCartesian(0, 1, 0);
        auto c2 = TriangularAxis::toCartesian(0, 0, 1);
        checkClose(c0.x, 0, 1e-12, QStringLiteral("三角圖：(1,0,0) → 左下角"));
        checkClose(c0.y, 0, 1e-12, QStringLiteral("三角圖：(1,0,0) 的 y"));
        checkClose(c1.x, 1, 1e-12, QStringLiteral("三角圖：(0,1,0) → 右下角"));
        checkClose(c2.x, 0.5, 1e-12, QStringLiteral("三角圖：(0,0,1) → 頂點 x"));
        checkClose(c2.y, std::sqrt(3.0) / 2, 1e-12, QStringLiteral("三角圖：(0,0,1) → 頂點 y"));
        // 不必正規化就能用
        auto mid = TriangularAxis::toCartesian(1, 1, 1);
        checkClose(mid.x, 0.5, 1e-12, QStringLiteral("三角圖：重心 x（輸入不必和為 1）"));
        checkClose(mid.y, std::sqrt(3.0) / 6, 1e-12, QStringLiteral("三角圖：重心 y"));
    }

    // ── 史密斯圖座標 ──
    {
        auto match = SmithChartAxis::toCartesian(1, 0);
        checkClose(match.x, 0, 1e-12, QStringLiteral("史密斯圖：z=1 在圓心"));
        checkClose(match.y, 0, 1e-12, QStringLiteral("史密斯圖：z=1 的虛部"));
        auto shortCircuit = SmithChartAxis::toCartesian(0, 0);
        checkClose(shortCircuit.x, -1, 1e-12, QStringLiteral("史密斯圖：短路 z=0 → Γ=-1"));
        auto openCircuit = SmithChartAxis::toCartesian(1e9, 0);
        checkClose(openCircuit.x, 1, 1e-6, QStringLiteral("史密斯圖：開路 z→∞ → Γ=+1"));
        auto inductive = SmithChartAxis::toCartesian(0, 1);
        checkClose(inductive.x, 0, 1e-12, QStringLiteral("史密斯圖：z=j1 → Γ=j（實部）"));
        checkClose(inductive.y, 1, 1e-12, QStringLiteral("史密斯圖：z=j1 → Γ=j（虛部）"));

        // 被動元件（r >= 0）的反射係數必定落在單位圓內
        bool inside = true;
        for (double r : {0.0, 0.3, 1.0, 5.0, 50.0})
            for (double x : {-20.0, -1.0, 0.0, 1.0, 20.0}) {
                const auto g = SmithChartAxis::toCartesian(r, x);
                inside = inside && (std::hypot(g.x, g.y) <= 1.0 + 1e-9);
            }
        check(inside, QStringLiteral("史密斯圖：r>=0 的 Γ 都在單位圓內"));
    }
}

// ───────────────────── 即時資料 ─────────────────────

void testLive() {
    out() << "即時資料\n";

    // DataLogger 的三種視野模式。走真的 render 路徑，beforeLayout 才會被呼叫。
    {
        Plot p;
        auto* log = p.addDataLogger();
        log->view = LiveView::Full;
        for (int i = 0; i < 100; ++i) log->add(i * 0.1);
        p.renderToImage(800, 600);
        checkClose(p.bottomAxis()->range.min, 0, 1e-9, QStringLiteral("Full：左邊界是第一筆"));
        checkClose(p.bottomAxis()->range.max, 99, 1e-9, QStringLiteral("Full：右邊界是最新一筆"));
    }
    {
        Plot p;
        auto* log = p.addDataLogger();
        log->view = LiveView::Slide;
        log->viewWidth = 50;
        for (int i = 0; i < 100; ++i) log->add(i * 0.1);
        p.renderToImage(800, 600);
        checkClose(p.bottomAxis()->range.max, 99, 1e-9, QStringLiteral("Slide：永遠貼著最新一筆"));
        checkClose(p.bottomAxis()->range.span(), 50, 1e-9, QStringLiteral("Slide：寬度固定"));
    }
    {
        Plot p;
        auto* log = p.addDataLogger();
        log->view = LiveView::Jump;
        log->viewWidth = 50;
        log->jumpFraction = 1.0;
        for (int i = 0; i < 60; ++i) log->add(0.0);
        p.renderToImage(800, 600);
        const double right = p.bottomAxis()->range.max;
        checkClose(p.bottomAxis()->range.span(), 50, 1e-9, QStringLiteral("Jump：寬度固定"));
        // 右邊界是整數倍的跳格，不是貼著最新一筆
        check(right >= 59 && std::abs(right - 100.0) < 1e-9,
              QStringLiteral("Jump：右邊界跳到下一格而不是貼齊資料"),
              QStringLiteral("右邊界 %1").arg(right));
    }
    {
        // Y 只看可視範圍內的資料：早期的尖峰捲出去之後，軸要縮回來
        Plot p;
        auto* log = p.addDataLogger();
        log->view = LiveView::Slide;
        log->viewWidth = 50;
        for (int i = 0; i < 200; ++i) log->add(i == 10 ? 1000.0 : 1.0);
        p.renderToImage(800, 600);
        check(p.leftAxis()->range.max < 100,
              QStringLiteral("Slide：捲出去的尖峰不再撐著 Y 軸"),
              QStringLiteral("Y 上界 %1").arg(p.leftAxis()->range.max));
    }

    // DataStreamer 的循環緩衝
    {
        DataStreamer s(10);
        for (int i = 0; i < 25; ++i) s.add(i);
        check(s.capacity() == 10, QStringLiteral("容量不變"));
        check(s.size() == 10, QStringLiteral("緩衝滿了之後 size 停在容量"));
        check(s.totalAdded() == 25, QStringLiteral("totalAdded 持續累加"));
        checkClose(s.latest(), 24, 1e-9, QStringLiteral("latest 是最後加入的那一筆"));
    }
    {
        // Wipe：X 永遠是 0..(capacity-1)，畫面不動
        Plot p;
        auto* s = p.addDataStreamer(100);
        s->mode = DataStreamer::Mode::Wipe;
        for (int i = 0; i < 250; ++i) s->add(std::sin(i * 0.1));
        p.renderToImage(800, 600);
        checkClose(p.bottomAxis()->range.min, 0, 1e-9, QStringLiteral("Wipe：X 從 0 開始"));
        checkClose(p.bottomAxis()->range.max, 99, 1e-9, QStringLiteral("Wipe：X 到 capacity-1 為止"));
    }
    {
        // Scroll：X 跟著總筆數往右走
        Plot p;
        auto* s = p.addDataStreamer(100);
        s->mode = DataStreamer::Mode::Scroll;
        for (int i = 0; i < 250; ++i) s->add(std::sin(i * 0.1));
        p.renderToImage(800, 600);
        checkClose(p.bottomAxis()->range.max, 249, 1e-9, QStringLiteral("Scroll：右邊界是最新一筆"));
        checkClose(p.bottomAxis()->range.min, 150, 1e-9,
                   QStringLiteral("Scroll：左邊界是緩衝裡最舊的那一筆"));
    }
    {
        // 緩衝還沒滿的時候不該畫出未初始化的值
        Plot p;
        auto* s = p.addDataStreamer(100);
        s->mode = DataStreamer::Mode::Scroll;
        for (int i = 0; i < 7; ++i) s->add(5.0);
        p.renderToImage(800, 600);
        checkClose(p.bottomAxis()->range.max, 6, 1e-9, QStringLiteral("未滿：右邊界是第 7 筆"));
        checkClose(p.bottomAxis()->range.min, 0, 1e-9, QStringLiteral("未滿：左邊界是第 1 筆"));
        checkClose(s->latest(), 5.0, 1e-9, QStringLiteral("未滿：latest 正確"));
    }
    {
        // DataStreamerXY：X 自己帶進來，所以不等間距也畫得對
        DataStreamerXY s(5);
        const double times[] = {0.0, 0.3, 1.9, 2.0, 5.5, 9.1, 9.2};
        for (double t : times) s.add(t, t * 2);
        check(s.size() == 5, QStringLiteral("XY：緩衝滿了之後 size 停在容量"));
        check(s.totalAdded() == 7, QStringLiteral("XY：totalAdded 持續累加"));
        checkClose(s.latest().x, 9.2, 1e-9, QStringLiteral("XY：latest 的 X"));
        checkClose(s.latest().y, 18.4, 1e-9, QStringLiteral("XY：latest 的 Y"));

        Plot p;
        auto* xy = p.addDataStreamerXY(5);
        for (double t : times) xy->add(t, t * 2);
        p.renderToImage(800, 600);
        // 最舊的兩筆（0.0、0.3）已經被蓋掉，X 軸應該從 1.9 開始
        checkClose(p.bottomAxis()->range.min, 1.9, 1e-9,
                   QStringLiteral("XY：X 左界是緩衝裡最舊的那一筆"));
        checkClose(p.bottomAxis()->range.max, 9.2, 1e-9, QStringLiteral("XY：X 右界是最新一筆"));
    }
}

// ───────────────────── 實際渲染路徑 ─────────────────────

/// 刻度產生器單獨測是一回事，接上版面計算之後拿到的 lengthPx 又是一回事。
/// 這裡照著範例的做法建圖、真的 render 一次，再看軸上最後留下什麼刻度。
void testRenderedTicks() {
    out() << "實際渲染後的刻度\n";
    {
        Plot p;
        const QVector<Ohlc> bars = randomOhlcSeries(40, 100);
        auto* c = p.emplace<Candlestick>();
        c->bars = bars;
        c->useIndexX = false;
        p.bottomAxis()->tickGenerator = std::make_shared<DateTimeTickGenerator>();
        p.autoScale();
        p.renderToImage(800, 600);

        QStringList labels;
        for (const Tick& t : p.bottomAxis()->ticks())
            if (t.isMajor && !t.label.isEmpty()) labels.append(t.label);
        out() << "       時間戳 X: " << labels.join(QStringLiteral(" | ")) << "\n";
        // 這一項曾經失敗過：日期刻度用「固定秒數」的格線時，月間距 2629800 秒
        // 會讓刻度落在 1 月 1 日與 1 月 31 日，兩根都標成「2026-01」。
        // 改成對齊日曆邊界之後才正常。
        check(labels.size() >= 3, QStringLiteral("時間戳 X 軸至少 3 根刻度"),
              QStringLiteral("只有 %1 根").arg(labels.size()));
        check(labels.size() == QSet<QString>(labels.begin(), labels.end()).size(),
              QStringLiteral("時間戳 X 軸的標籤不重複"), labels.join(QStringLiteral(", ")));
    }
    {
        Plot p;
        p.addCandlestick(randomOhlcSeries(120, 100));
        p.autoScale();
        p.renderToImage(800, 600);

        QStringList labels;
        for (const Tick& t : p.bottomAxis()->ticks())
            if (t.isMajor && !t.label.isEmpty()) labels.append(t.label);
        out() << "       序號 X:  " << labels.join(QStringLiteral(" | ")) << "\n";
        check(labels.size() >= 3, QStringLiteral("序號 X 軸至少 3 根刻度"),
              QStringLiteral("只有 %1 根").arg(labels.size()));
        check(labels.size() == QSet<QString>(labels.begin(), labels.end()).size(),
              QStringLiteral("序號 X 軸的標籤不重複"), labels.join(QStringLiteral(", ")));
    }
}

}  // namespace


// ───────────────────── 第二批補充：3D、圖例、核函數、熱圖、日期刻度 ─────────────────────

void testSupplement2() {
    out() << "第二批補充\n";

    // 每一種核函數對 x 積分都應該是 1
    {
        const QVector<double> data = {1, 2, 2, 3, 3, 3, 4, 4, 5};
        QVector<double> grid;
        for (int i = 0; i <= 4000; ++i) grid.append(-6 + 18.0 * i / 4000);
        const KernelType kinds[] = {KernelType::Gaussian, KernelType::Epanechnikov, KernelType::Uniform,
                                    KernelType::Triangular, KernelType::Cosine};
        const char* names[] = {"高斯", "Epanechnikov", "均勻", "三角", "餘弦"};
        for (int k = 0; k < 5; ++k) {
            const QVector<double> dens = kernelDensity(data, grid, 0.8, kinds[k]);
            double area = 0;
            for (int i = 1; i < dens.size(); ++i) area += (dens[i] + dens[i - 1]) / 2 * (grid[i] - grid[i - 1]);
            checkClose(area, 1.0, 2e-3, QStringLiteral("%1核的密度積分 = 1").arg(QString::fromUtf8(names[k])));
        }
        check(kernelValue(KernelType::Epanechnikov, 1.5) == 0, QStringLiteral("有界的核在 |u| > 1 時為 0"));
    }

    // 3D 刻度與範圍
    {
        const QVector<double> t = Chart3D::niceTicks(CoordinateRange(0, 10), 5);
        check(t.size() >= 4 && t.first() == 0 && t.last() == 10, QStringLiteral("3D 刻度涵蓋 0..10"));
        Chart3D chart;
        auto c = std::make_unique<Columns3D>();
        c->columns = {{1, 1, 5}, {2, 1, 8}, {1, 2, -3}};
        chart.add(std::move(c));
        CoordinateRange x, y, z;
        chart.effectiveLimits(x, y, z);
        check(z.min <= -3 && z.max >= 8, QStringLiteral("立體柱的高度範圍含正負值"));
        check(x.min < 1 && x.max > 2, QStringLiteral("立體柱的 X 範圍含柱寬"));
    }

    // 3D 圖畫得出來，而且旋轉後影像會變
    {
        Plot plot;
        auto* chart = plot.addChart3D();
        auto s = std::make_unique<SurfaceSeries3D>();
        s->xs = gen::linspace(-2, 2, 12);
        s->ys = gen::linspace(-2, 2, 12);
        for (double yy : s->ys)
            for (double xx : s->xs) s->zs.append(std::exp(-xx * xx - yy * yy));
        chart->add(std::move(s));
        const QImage a = plot.renderToImage(400, 300, 1.0);
        chart->azimuth += 60;
        const QImage b = plot.renderToImage(400, 300, 1.0);
        check(a != b, QStringLiteral("改方位角之後 3D 圖的影像改變"));
    }

    // 圖例放在外面要讓出寬度；分欄後點擊仍找得到每一列
    {
        Plot inside, outside;
        for (Plot* p : {&inside, &outside}) {
            for (int i = 0; i < 6; ++i)
                p->addSignal(gen::sin(50), 1.0)->legendText = QStringLiteral("序列 %1").arg(i);
            p->legend.isVisible = true;
        }
        outside.legend.placement = Legend::Placement::OutsideRight;
        outside.legend.columns = 2;
        inside.renderToImage(600, 400, 1.0);
        outside.renderToImage(600, 400, 1.0);
        check(outside.lastDataRect().width() < inside.lastDataRect().width() - 20,
              QStringLiteral("外側圖例讓出繪圖區寬度"));
        check(outside.legendHitAreas().size() == 6, QStringLiteral("分欄圖例的六列都有點擊範圍"));
        bool allRight = true;
        for (const auto& h : outside.legendHitAreas())
            allRight = allRight && h.rect.left() > outside.lastDataRect().right;
        check(allRight, QStringLiteral("外側圖例畫在繪圖區右邊"));
    }

    // 熱圖格子中心對齊：extent 是頭尾兩格的中心
    {
        Heatmap h;
        h.rows = 3;
        h.cols = 5;
        h.values = QVector<double>(15, 1.0);
        h.extent = CoordinateRect(0, 4, 0, 2);
        h.cellCentered = true;
        const CoordinateRect e = h.effectiveExtent();
        checkClose(e.left, -0.5, 1e-12, QStringLiteral("格子中心對齊：左緣往外半格"));
        checkClose(e.top, 2.5, 1e-12, QStringLiteral("格子中心對齊：上緣往外半格"));
    }

    // 固定週刻度一律落在週一
    {
        DateTimeTickGenerator g;
        g.interval = DateTimeTickGenerator::Interval::Weeks;
        const double start = QDateTime(QDate(2026, 3, 4), QTime(0, 0)).toSecsSinceEpoch();
        FontStyle f;
        const QVector<Tick> ticks = g.generate(CoordinateRange(start, start + 90 * 86400.0), 800, f, false);
        bool mondays = !ticks.isEmpty();
        for (const Tick& t : ticks)
            mondays = mondays && QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t.position)).date().dayOfWeek() == 1;
        check(mondays, QStringLiteral("週刻度都在週一"), QStringLiteral("%1 根").arg(ticks.size()));
    }
}


// ───────────────────── 環狀長條圖與環狀熱圖 ─────────────────────

void testRadialCharts() {
    out() << "環狀圖\n";

    {
        RadialBarChart c;
        c.values = {100, 50, 25};
        c.sweepAngle = 270;
        const QVector<double> a = c.sweeps();
        checkClose(a[0], 270, 1e-9, QStringLiteral("最大值畫滿整個角度"));
        checkClose(a[1], 135, 1e-9, QStringLiteral("一半的值畫一半的角度"));

        // 距離正確：每一環的弧長（角度 × 半徑）和數值成正比
        c.equalArcLength = true;
        const QVector<double> b = c.sweeps();
        double m0, t0, m1, t1, m2, t2;
        c.ring(0, m0, t0);
        c.ring(1, m1, t1);
        c.ring(2, m2, t2);
        checkClose(b[0] * m0 / (b[1] * m1), 2.0, 1e-9, QStringLiteral("距離正確：弧長比 = 數值比"));
        checkClose(b[1] * m1 / (b[2] * m2), 2.0, 1e-9, QStringLiteral("距離正確：內環也成比例"));
        bool within = true;
        for (double x : b) within = within && x <= c.sweepAngle + 1e-9;
        check(within, QStringLiteral("距離正確：每一條都不超過總角度"));
        check(m0 > m1 && m1 > m2, QStringLiteral("第一個數值在最外環"));
    }

    {
        CircularHeatmap h;
        h.rings = 2;
        h.sectors = 4;
        h.cells = QVector<QColor>(8);
        h.cells[1 * 4 + 2] = QColor(255, 0, 0);
        check(h.cell(1, 2) == QColor(255, 0, 0), QStringLiteral("環狀熱圖的格子是 [環][扇區]"));
        checkClose(h.sectorAngle(1), 0, 1e-9, QStringLiteral("順時針：第 1 個扇區從 3 點鐘開始"));
    }
}

void testSupplement3() {
    out() << "第三批補充（刻度、色階、直方圖、拖曳、財金）\n";

    {
        AutomaticTickGenerator g;
        g.targetTickCount = 4;
        int majors = 0;
        for (const Tick& t : g.generate({0, 100}, 800, tickFont(), false)) majors += t.isMajor;
        check(majors >= 3 && majors <= 6, QStringLiteral("targetTickCount = 4 大約給 4 根主刻度"),
              QStringLiteral("得到 %1").arg(majors));
        AutomaticTickGenerator dense, normal;
        dense.tickDensity = 3;
        int nd = 0, nn = 0;
        for (const Tick& t : dense.generate({0, 100}, 800, tickFont(), false)) nd += t.isMajor;
        for (const Tick& t : normal.generate({0, 100}, 800, tickFont(), false)) nn += t.isMajor;
        check(nd > nn, QStringLiteral("tickDensity 加大刻度變密"));
    }
    {
        LogTickGenerator g;
        g.minorMultipliers = {2, 5};
        int minors = 0;
        for (const Tick& t : g.generate({0, 3}, 600, tickFont(), true)) minors += !t.isMajor;
        check(minors == 6, QStringLiteral("對數軸只在 2、5 畫次刻度（三個十進位共 6 根）"),
              QStringLiteral("得到 %1").arg(minors));
    }
    {
        const Histogram h = Histogram::fromBinWidth({1, 9, 11, 19, 25, 99}, 0, 30, 10);
        check(h.counts.size() == 3, QStringLiteral("固定箱寬：0–30 每 10 一箱是 3 箱"));
        checkClose(h.counts.value(0) + h.counts.value(1) + h.counts.value(2), 5, 1e-9,
                   QStringLiteral("範圍外的值預設丟掉"));
        double sum = 0;
        for (double p : h.probabilities()) sum += p;
        checkClose(sum, 1.0, 1e-12, QStringLiteral("probabilities 總和為 1"));
        checkClose(h.cumulativeCounts().last(), 5, 1e-9, QStringLiteral("累積次數最後一箱等於總數"));
    }
    {
        const Colormap m = Colormap::fromColors({QColor(255, 0, 0, 0), QColor(0, 0, 255, 255)});
        check(m.lookup(0.5).alpha() > 100 && m.lookup(0.5).alpha() < 155, QStringLiteral("色階查表會內插透明度"));
        check(m.reversed().lookup(0.0) == m.lookup(1.0), QStringLiteral("reversed 頭尾對調"));
        const Colormap s = Colormap(Colormap::Viridis).stepped(4);
        check(s.lookup(0.05) == s.lookup(0.2), QStringLiteral("離散色階：同一段同一個顏色"));
        check(s.lookup(0.2) != s.lookup(0.3), QStringLiteral("離散色階：跨段換顏色"));
        const Palette p = Palette::fromColors({QColor(10, 20, 30), QColor(200, 100, 0)});
        check(p.count() == 2 && p.reversed().color(0) == QColor(200, 100, 0), QStringLiteral("自訂調色盤與反序"));
        check(p.inverted().color(0) == QColor(245, 235, 225), QStringLiteral("調色盤取反色"));
    }
    {
        QVector<Ohlc> bars;
        for (int i = 0; i < 10; ++i) {
            Ohlc o;
            o.open = i;
            o.close = i + 0.5;
            o.high = i + 2;
            o.low = i - 1;
            o.volume = 10;
            o.timestamp = i * 86400.0;
            bars.append(o);
        }
        const QVector<Ohlc> w = resampleOhlc(bars, 5 * 86400.0);
        check(w.size() == 2, QStringLiteral("10 根日線合成 2 根 5 日線"));
        check(w.size() == 2 && w[0].open == 0 && w[0].close == 4.5 && w[0].high == 6 && w[0].low == -1 &&
                  w[0].volume == 50,
              QStringLiteral("合併後的開高低收量都正確"));
        check(resampleOhlcByCount(bars, 3).size() == 4, QStringLiteral("每 3 根一組，10 根變 4 根"));
    }
    {
        const QVector<double> g = gridScattered({{0, 0}, {1, 1}}, {5, 9}, CoordinateRect(0, 1, 0, 1), 2, 2);
        checkClose(g.value(0), 5, 1e-9, QStringLiteral("IDW：量測點上就是量測值"));
        checkClose(g.value(1), 7, 1e-9, QStringLiteral("IDW：兩點等距處是平均"));
    }

    Transform t;
    t.rect = PixelRect(0, 100, 0, 100);
    t.xr = {0, 100};
    t.yr = {0, 100};
    {
        Scatter s;
        s.xs = {1, 2};
        s.ys = {1, 2};
        s.xScale = 10;
        s.yOffset = 5;
        check(s.displayXs()[1] == 20 && s.displayYs()[0] == 6, QStringLiteral("散點的縮放與位移"));
        check(s.axisLimits().x.max == 20, QStringLiteral("自動範圍用的是縮放後的座標"));
        s.draggablePoints = true;
        s.beginDrag({20, 7}, t, 4);
        s.dragBy({0, 3});
        check(s.draggedIndex() == 1 && s.ys[1] == 5, QStringLiteral("拖曳資料點改的是原始資料"));
    }
    {
        AxisSpan sp;
        sp.vertical = true;
        sp.start = 20;
        sp.end = 60;
        sp.resizable = true;
        check(sp.isGrabbed({21, 50}, t, 4), QStringLiteral("可調寬度的區間：邊緣抓得到"));
        sp.beginDrag({21, 50}, t, 4);
        sp.dragBy({-5, 0});
        check(sp.start == 15 && sp.end == 60, QStringLiteral("抓左邊只動 start"));
        check(!sp.isGrabbed({40, 50}, t, 4), QStringLiteral("沒開 draggable 時中間抓不到"));
    }
    {
        Arrow a;
        a.tip = {80, 80};
        a.base = {20, 20};
        a.draggableEndpoints = true;
        a.beginDrag({80, 80}, t, 4);
        a.dragBy({5, 0});
        check(a.tip.x == 85 && a.base.x == 20, QStringLiteral("線段：拖尖端只動尖端"));
    }
    {
        Shape r;
        r.rect = CoordinateRect(10, 50, 10, 50);
        r.resizable = true;
        r.beginDrag({50, 50}, t, 4);
        r.dragBy({10, 10});
        check(r.rect.right == 60 && r.rect.top == 60 && r.rect.left == 10, QStringLiteral("矩形：拖右上角改兩個邊"));
    }
    {
        TrendLine tl;
        tl.point1 = {0, 0};
        tl.point2 = {10, 20};
        checkClose(tl.slope(), 2, 1e-12, QStringLiteral("趨勢線斜率"));
        tl.beginDrag({10, 20}, t, 4);
        tl.dragBy({0, 10});
        checkClose(tl.slope(), 3, 1e-12, QStringLiteral("拖第二點改變斜率"));
    }
    {
        Plot plot;
        auto* a = plot.addSignal({1, 2});
        auto* b = plot.addSignal({1, 2});
        plot.moveToFront(a);
        check(plot.plottables().last() == a, QStringLiteral("moveToFront 移到最後畫"));
        plot.moveToBack(a);
        check(plot.plottables().first() == a && plot.plottables().last() == b, QStringLiteral("moveToBack 移到最先畫"));
    }
    {
        DataLogger log;
        log.add(1);
        log.add(2);
        log.setY(0, 9);
        log.setY(5, 9);  // 超出範圍不做事
        check(log.ys().value(0) == 9 && log.count() == 2, QStringLiteral("DataLogger::setY 改已記錄的值"));
    }
    {
        Label a, b;
        a.text = b.text = QStringLiteral("一\n二\n三");
        b.font.lineHeight = 2.0;
        check(measureText(b).height() > measureText(a).height() * 1.5, QStringLiteral("行距倍率加高多行文字"));
    }
}

int main(int argc, char* argv[]) {
    // 要量字型就需要 QGuiApplication（離屏也可以）
    QGuiApplication app(argc, argv);

    testTransform();
    testAutomaticTicks();
    testDateTimeTicks();
    testIndexedDateTicks();
    testStatistics();
    testIndicators();
    testAutoScale();
    testReset();
    testTooltipTail();
    testSectorsAndPolar();
    testGridFillAndRotation();
    testBars3D();
    testBusinessCharts();
    testSunburstAndPie();
    testInvertedAxis();
    testAxisBreaks();
    testTickStylesAndLogScale();
    testTriSurface();
    testContour3DAndDepthShade();
    testInspection();
    testLegendSymbolStyle();
    testPerformancePaths();
    testBubbleGanttVenn();
    testFlowCharts();
    testRoseCharts();
    testSmithChart();
    testPointLabels();
    testMosaicAndNetwork();
    testIsometric();
    testStackSigned();
    testTreemapAndParallel();
    testHexbinAndDumbbell();
    testStreamAndSwarm();
    testClusterAndHorizon();
    testArcText();
    testNormalAndQQ();
    testClassifierCurves();
    testKaplanMeier();
    testControlChart();
    testForestPlot();
    testSpectral();
    testSpectralAnalysis();
    testFonts();
    testFishbone();
    testVectorExport();
    testMultiplot();
    testBusinessCharts2();
    testMoreTypes();
    testDiagnostics();
    testInteractionState();
    testMarchingSquares();
    testSpecialty();
    testLive();
    testRenderedTicks();
    testSupplement2();
    testRadialCharts();
    testSupplement3();

    out() << "\n";
    if (g_failures == 0) {
        out() << "全部通過\n";
    } else {
        out() << g_failures << " 項失敗\n";
    }
    out().flush();
    return g_failures;
}

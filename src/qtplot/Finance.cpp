#include "Finance.h"

#include <QFontMetricsF>
#include <QTimeZone>

#include <numeric>
#include <random>

namespace qtplot {

namespace {

/// 兩個時間戳在哪一級上「跳了一格」。數字越大代表跳得越粗。
enum class Boundary { None, Day, Week, Month, Year };

Boundary boundaryBetween(double prevSecs, double secs) {
    const QDate a = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(prevSecs)).date();
    const QDate b = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secs)).date();
    if (a.year() != b.year()) return Boundary::Year;
    if (a.month() != b.month()) return Boundary::Month;
    // 週一視為換週（weekNumber 跨年時會怪怪的，用星期幾判斷比較穩）
    if (b.dayOfWeek() < a.dayOfWeek()) return Boundary::Week;
    if (a != b) return Boundary::Day;
    return Boundary::None;
}

}  // namespace

// ───────────────── IndexedDateTickGenerator ─────────────────

QVector<Tick> IndexedDateTickGenerator::generate(const CoordinateRange& range, double lengthPx,
                                                 const FontStyle& font, bool vertical) {
    QVector<Tick> out;
    if (timestamps.isEmpty() || !range.hasArea() || lengthPx <= 1) return out;

    QFontMetricsF fm(font.font());
    const double labelExtent =
        vertical ? fm.height() : fm.horizontalAdvance(QStringLiteral("0000-00-00"));
    const int maxTicks = std::max(2, static_cast<int>(lengthPx / (labelExtent + 12)));

    const int i0 = std::max(0, static_cast<int>(std::floor(range.min)));
    const int i1 = std::min(static_cast<int>(timestamps.size()) - 1,
                            static_cast<int>(std::ceil(range.max)));
    if (i1 <= i0) return out;

    const double spanSecs = timestamps[i1] - timestamps[i0];

    QString fmt = format;
    if (fmt.isEmpty()) {
        if (spanSecs < 2 * 86400)         fmt = QStringLiteral("HH:mm");
        else if (spanSecs < 120 * 86400)  fmt = QStringLiteral("MM-dd");
        else if (spanSecs < 3 * 31557600) fmt = QStringLiteral("yyyy-MM");
        else                              fmt = QStringLiteral("yyyy");
    }

    if (breakOnBoundary) {
        // 只在換月／換年這種「日期跳一級」的位置放刻度。
        // 等距取樣會落在隨機的某一天，讀起來沒有意義。
        const Boundary want = (spanSecs < 120.0 * 86400)   ? Boundary::Week
                              : (spanSecs < 3 * 31557600.0) ? Boundary::Month
                                                            : Boundary::Year;
        QVector<int> candidates;
        for (int i = i0 + 1; i <= i1; ++i) {
            const Boundary b = boundaryBetween(timestamps[i - 1], timestamps[i]);
            if (static_cast<int>(b) >= static_cast<int>(want)) candidates.append(i);
        }
        // 候選太多就等間隔抽，標籤才不會重疊
        const int step = std::max(1, static_cast<int>(std::ceil(
                                         candidates.size() / static_cast<double>(maxTicks))));
        for (int k = 0; k < candidates.size(); k += step) {
            const int i = candidates.at(k);
            Tick t;
            t.position = i;
            t.isMajor = true;
            t.label = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(timestamps[i]))
                          .toString(fmt);
            out.append(t);
        }
        if (!out.isEmpty()) return out;
        // 一個邊界都沒有（資料太短）就往下走等距版本
    }

    const int step = std::max(1, (i1 - i0) / maxTicks);
    for (int i = i0; i <= i1; i += step) {
        Tick t;
        t.position = i;
        t.isMajor = true;
        t.label = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(timestamps[i])).toString(fmt);
        out.append(t);
    }
    return out;
}

// ───────────────────────── Candlestick ─────────────────────────

QVector<double> Candlestick::closes() const {
    QVector<double> out;
    out.reserve(bars.size());
    for (const Ohlc& b : bars) out.append(b.close);
    return out;
}

QVector<double> Candlestick::xs() const {
    QVector<double> out;
    out.reserve(bars.size());
    for (qsizetype i = 0; i < bars.size(); ++i)
        out.append(useIndexX ? static_cast<double>(i) : bars[i].timestamp);
    return out;
}

QVector<double> Candlestick::timestamps() const {
    QVector<double> out;
    out.reserve(bars.size());
    for (const Ohlc& b : bars) out.append(b.timestamp);
    return out;
}

AxisLimits Candlestick::axisLimits() const {
    AxisLimits lim;
    for (qsizetype i = 0; i < bars.size(); ++i) {
        const Ohlc& b = bars[i];
        if (!b.isValid()) continue;
        const double x = useIndexX ? static_cast<double>(i) : b.timestamp;
        lim.x.expand(x);
        lim.y.expand(b.high);
        lim.y.expand(b.low);
    }
    // 頭尾各留半根的寬度，第一根與最後一根才不會被切掉
    if (lim.x.isValid() && bars.size() > 1) {
        const double spacing = useIndexX ? 1.0
                               : barDuration > 0 ? barDuration
                                                 : (bars.last().timestamp - bars.first().timestamp) /
                                                       (bars.size() - 1);
        lim.x.min -= spacing * barWidthFraction / 2;
        lim.x.max += spacing * barWidthFraction / 2;
    }
    return lim;
}

void Candlestick::render(RenderPack& rp) {
    if (bars.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 一格有多寬（資料單位）
    double spacing = 1.0;
    if (!useIndexX && barDuration > 0)
        spacing = barDuration;
    else if (!useIndexX && bars.size() > 1)
        spacing = (bars.last().timestamp - bars.first().timestamp) / (bars.size() - 1);
    const double halfWidth = spacing * barWidthFraction / 2;

    // 一根佔不到一個像素時實體畫不出來，只畫影線比較誠實
    const double barPx = std::abs(t.px(halfWidth * 2) - t.px(0));
    const bool tooNarrow = barPx < 1.5;

    for (qsizetype i = 0; i < bars.size(); ++i) {
        const Ohlc& b = bars[i];
        if (!b.isValid()) continue;
        const double x = useIndexX ? static_cast<double>(i) : b.timestamp;
        // 看不到的範圍不用畫
        if (x + halfWidth < t.xr.min || x - halfWidth > t.xr.max) continue;

        const QColor color = b.isRising() ? risingColor : fallingColor;
        const double px = t.px(x);
        const double pHigh = t.py(b.high);
        const double pLow = t.py(b.low);
        const double pOpen = t.py(b.open);
        const double pClose = t.py(b.close);

        QPen pen(color);
        pen.setWidthF(lineWidth);
        pen.setCapStyle(Qt::FlatCap);
        p.setPen(pen);

        if (style == Style::Ohlc) {
            // 美式棒線：開盤在左、收盤在右
            const double w = std::abs(t.px(x + halfWidth) - px);
            p.drawLine(QPointF(px, pHigh), QPointF(px, pLow));
            p.drawLine(QPointF(px - w, pOpen), QPointF(px, pOpen));
            p.drawLine(QPointF(px, pClose), QPointF(px + w, pClose));
            continue;
        }

        // 蠟燭：先畫影線，再畫實體蓋上去
        p.drawLine(QPointF(px, pHigh), QPointF(px, pLow));
        if (tooNarrow) continue;

        const double left = t.px(x - halfWidth);
        const double right = t.px(x + halfWidth);
        QRectF body(QPointF(left, pOpen), QPointF(right, pClose));
        body = body.normalized();
        // 開收同價時實體高度為 0，Qt 會畫不出來，補成一條線的厚度
        if (body.height() < lineWidth) body.setHeight(lineWidth);

        const bool hollow = hollowRising && b.isRising();
        p.setBrush(hollow ? QBrush(Qt::NoBrush) : QBrush(color));
        p.drawRect(body);
    }
    p.restore();
}

QVector<LegendItem> Candlestick::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.fill = FillStyle(risingColor);
    it.line = LineStyle(fallingColor, lineWidth);
    return {it};
}

// ───────────────────────── Volume ─────────────────────────

AxisLimits Volume::axisLimits() const {
    AxisLimits lim;
    for (qsizetype i = 0; i < bars.size(); ++i) {
        const Ohlc& b = bars[i];
        if (!std::isfinite(b.volume)) continue;
        lim.x.expand(useIndexX ? static_cast<double>(i) : b.timestamp);
        lim.y.expand(b.volume);
    }
    lim.y.expand(0.0);  // 成交量從 0 起算
    return lim;
}

void Volume::render(RenderPack& rp) {
    if (bars.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    p.setPen(Qt::NoPen);

    double spacing = 1.0;
    if (!useIndexX && barDuration > 0)
        spacing = barDuration;
    else if (!useIndexX && bars.size() > 1)
        spacing = (bars.last().timestamp - bars.first().timestamp) / (bars.size() - 1);
    const double halfWidth = spacing * barWidthFraction / 2;

    for (qsizetype i = 0; i < bars.size(); ++i) {
        const Ohlc& b = bars[i];
        if (!std::isfinite(b.volume)) continue;
        const double x = useIndexX ? static_cast<double>(i) : b.timestamp;
        if (x + halfWidth < t.xr.min || x - halfWidth > t.xr.max) continue;

        const QRectF r =
            QRectF(t.p(x - halfWidth, b.volume), t.p(x + halfWidth, 0.0)).normalized();
        p.setBrush(QBrush(b.isRising() ? risingColor : fallingColor));
        p.drawRect(r);
    }
    p.restore();
}

// ───────────────────────── 技術指標 ─────────────────────────

QVector<double> simpleMovingAverage(const QVector<double>& values, int period) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QVector<double> out(values.size(), nan);
    if (period < 1 || values.size() < period) return out;

    double sum = 0;
    for (qsizetype i = 0; i < values.size(); ++i) {
        sum += values[i];
        if (i >= period) sum -= values[i - period];
        if (i >= period - 1) out[i] = sum / period;
    }
    return out;
}

QVector<double> exponentialMovingAverage(const QVector<double>& values, int period) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QVector<double> out(values.size(), nan);
    if (period < 1 || values.size() < period) return out;

    // 前 period 個用簡單平均當種子，之後才遞迴 —— 直接從第一個值起算的話
    // 開頭那段會被第一筆資料主導，畫出來是一條假的趨勢。
    const double alpha = 2.0 / (period + 1);
    double ema = std::accumulate(values.begin(), values.begin() + period, 0.0) / period;
    out[period - 1] = ema;
    for (qsizetype i = period; i < values.size(); ++i) {
        ema = values[i] * alpha + ema * (1 - alpha);
        out[i] = ema;
    }
    return out;
}

BollingerBands bollingerBands(const QVector<double>& values, int period, double k) {
    BollingerBands b;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    b.middle = simpleMovingAverage(values, period);
    b.upper = QVector<double>(values.size(), nan);
    b.lower = QVector<double>(values.size(), nan);
    if (period < 2 || values.size() < period) return b;

    for (qsizetype i = period - 1; i < values.size(); ++i) {
        const double mean = b.middle[i];
        double ss = 0;
        for (qsizetype j = i - period + 1; j <= i; ++j) ss += (values[j] - mean) * (values[j] - mean);
        // 母體標準差（分母 n）—— 這是布林通道的慣例，用 n-1 會和看盤軟體對不起來
        const double sd = std::sqrt(ss / period);
        b.upper[i] = mean + k * sd;
        b.lower[i] = mean - k * sd;
    }
    return b;
}

// ───────────────────────── 測試資料 ─────────────────────────

namespace {

Ohlc mergeBars(const QVector<Ohlc>& bars, qsizetype from, qsizetype to) {
    Ohlc o = bars[from];
    o.close = bars[to - 1].close;
    o.volume = 0;
    for (qsizetype i = from; i < to; ++i) {
        o.high = std::max(o.high, bars[i].high);
        o.low = std::min(o.low, bars[i].low);
        o.volume += bars[i].volume;
    }
    return o;
}

}  // namespace

QVector<Ohlc> resampleOhlc(const QVector<Ohlc>& bars, double seconds) {
    QVector<Ohlc> out;
    if (bars.isEmpty() || !(seconds > 0)) return bars;
    qsizetype start = 0;
    double bucket = std::floor(bars[0].timestamp / seconds);
    for (qsizetype i = 1; i <= bars.size(); ++i) {
        const double b = (i < bars.size()) ? std::floor(bars[i].timestamp / seconds) : bucket + 1;
        if (b != bucket) {
            Ohlc o = mergeBars(bars, start, i);
            o.timestamp = bucket * seconds;
            out.append(o);
            start = i;
            bucket = b;
        }
    }
    return out;
}

QVector<Ohlc> resampleOhlcByCount(const QVector<Ohlc>& bars, int count) {
    if (count <= 1) return bars;
    QVector<Ohlc> out;
    for (qsizetype i = 0; i < bars.size(); i += count)
        out.append(mergeBars(bars, i, std::min<qsizetype>(i + count, bars.size())));
    return out;
}

QVector<Ohlc> randomOhlcSeries(int count, double startPrice, const QDate& startDate,
                               double volatility) {
    QVector<Ohlc> out;
    if (count <= 0) return out;
    out.reserve(count);

    std::mt19937 rng(20260113);
    std::normal_distribution<double> step(0.0003, volatility);
    std::uniform_real_distribution<double> wick(0.0, 1.0);
    std::uniform_real_distribution<double> vol(0.6, 1.6);

    double price = startPrice;
    QDate date = startDate;
    for (int i = 0; i < count; ++i) {
        // 跳過週末 —— 這正是為什麼 X 要用序號而不是時間戳
        while (date.dayOfWeek() > 5) date = date.addDays(1);

        Ohlc b;
        b.timestamp = QDateTime(date, QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
        b.open = price;
        price *= (1.0 + step(rng));
        b.close = price;
        const double hi = std::max(b.open, b.close);
        const double lo = std::min(b.open, b.close);
        const double range = std::max(hi - lo, hi * volatility * 0.3);
        b.high = hi + range * wick(rng) * 0.8;
        b.low = lo - range * wick(rng) * 0.8;
        b.volume = 1.0e6 * vol(rng) * (1 + std::abs(b.close - b.open) / std::max(b.open, 1e-9) * 20);
        out.append(b);

        date = date.addDays(1);
    }
    return out;
}

}  // namespace qtplot

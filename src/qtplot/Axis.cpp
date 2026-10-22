#include "Axis.h"

#include <algorithm>
#include <QFontMetricsF>

namespace qtplot {

namespace {

/// 把整數轉成上標數字（10⁶ 的那個 ⁶）。
QString superscript(int value) {
    static const QChar kDigits[] = {QChar(0x2070), QChar(0x00B9), QChar(0x00B2), QChar(0x00B3),
                                    QChar(0x2074), QChar(0x2075), QChar(0x2076), QChar(0x2077),
                                    QChar(0x2078), QChar(0x2079)};
    QString out;
    if (value < 0) {
        out.append(QChar(0x207B));  // 上標減號
        value = -value;
    }
    const QString digits = QString::number(value);
    for (QChar d : digits) out.append(kDigits[d.digitValue()]);
    return out;
}

/// 把 raw 往上取整到最近的 1 / 2 / 5 × 10^n。
double niceStep(double raw) {
    if (!(raw > 0) || !std::isfinite(raw)) return 1.0;
    double exp10 = std::floor(std::log10(raw));
    double pow10 = std::pow(10.0, exp10);
    double frac = raw / pow10;  // [1, 10)
    double nice;
    if (frac <= 1.0)      nice = 1.0;
    else if (frac <= 2.0) nice = 2.0;
    else if (frac <= 5.0) nice = 5.0;
    else                  nice = 10.0;
    return nice * pow10;
}

}  // namespace

// ───────────────────── AutomaticTickGenerator ─────────────────────

QVector<Tick> AutomaticTickGenerator::generate(const CoordinateRange& range, double lengthPx,
                                               const FontStyle& font, bool vertical) {
    QVector<Tick> out;
    if (!range.hasArea() || lengthPx <= 1) return out;

    QFontMetricsF fm(font.font());
    // 標籤要多少空間才不重疊。水平軸看寬度（抓一個保守的字寬），垂直軸看字高。
    double labelExtent = vertical ? fm.height()
                                  : fm.horizontalAdvance(QStringLiteral("-000.00"));
    double minSpacingPx = (labelExtent + labelPaddingPx) / std::clamp(tickDensity, 0.1, 10.0);
    int targetCount = targetTickCount > 0 ? targetTickCount
                                          : std::max(2, static_cast<int>(lengthPx / minSpacingPx));

    double spacing = niceStep(range.span() / targetCount);
    if (spacing <= 0 || !std::isfinite(spacing)) return out;

    // 整根軸共用一種標籤格式，刻度才不會一下 900000 一下 1e+06
    const double axisMaxAbs = std::max(std::abs(range.min), std::abs(range.max));

    // 共同倍率：把 10 的次方抽到軸的端點，刻度就只剩好讀的小數字
    multiplierLabel.clear();
    double multiplier = 1.0;
    if (allowMultiplier && !labelFormatter && axisMaxAbs > 0) {
        const int exponent = static_cast<int>(std::floor(std::log10(axisMaxAbs)));
        if (std::abs(exponent) >= std::max(1, multiplierThreshold)) {
            multiplier = std::pow(10.0, exponent);
            multiplierLabel = QStringLiteral("×10") + superscript(exponent);
        }
    }
    const double invMul = 1.0 / multiplier;

    // 標籤實際量過之後可能還是太寬（例如 -1234567）。量到不重疊為止。
    for (int guard = 0; guard < 8 && !vertical && targetTickCount <= 0; ++guard) {
        double widest = 0;
        double first = std::ceil(range.min / spacing) * spacing;
        for (double v = first; v <= range.max; v += spacing) {
            QString s = labelFormatter
                            ? labelFormatter(v, spacing)
                            : formatTickLabel(v * invMul, spacing * invMul, axisMaxAbs * invMul);
            widest = std::max(widest, fm.horizontalAdvance(s));
        }
        double needed = widest + labelPaddingPx;
        double actual = spacing / range.span() * lengthPx;
        if (actual >= needed) break;
        spacing = niceStep(spacing * 1.01 * needed / std::max(actual, 1e-12));
    }

    const double firstMajor = std::ceil(range.min / spacing) * spacing;
    const int count = static_cast<int>(std::floor((range.max - firstMajor) / spacing)) + 1;
    if (count <= 0 || count > maxTicks) {
        if (count > maxTicks) {
            // 範圍太大／間距太小時寧可少畫，也不要塞爆
            spacing = niceStep(range.span() / 5.0);
        } else {
            return out;
        }
    }

    const double start = std::ceil(range.min / spacing) * spacing;
    for (double v = start; v <= range.max + spacing * 1e-9; v += spacing) {
        Tick t;
        t.position = v;
        t.isMajor = true;
        t.label = labelFormatter
                      ? labelFormatter(v, spacing)
                      : formatTickLabel(v * invMul, spacing * invMul, axisMaxAbs * invMul);
        out.append(t);
        if (out.size() > maxTicks) break;
    }

    // 次刻度：主刻度之間等分，兩端也補到範圍邊界
    if (minorTicksPerMajor > 1 && !out.isEmpty()) {
        const double minorSpacing = spacing / minorTicksPerMajor;
        double v = out.first().position - spacing;
        const double end = out.last().position + spacing;
        while (v <= end) {
            // 跳過落在主刻度上的
            double rel = std::abs(std::remainder(v, spacing));
            if (rel > minorSpacing * 1e-6 && v >= range.min && v <= range.max) {
                Tick t;
                t.position = v;
                t.isMajor = false;
                out.append(t);
            }
            v += minorSpacing;
        }
    }
    return out;
}

// ───────────────────── LogTickGenerator ─────────────────────

QVector<Tick> LogTickGenerator::generate(const CoordinateRange& range, double lengthPx,
                                         const FontStyle& font, bool vertical) {
    Q_UNUSED(lengthPx);
    Q_UNUSED(font);
    Q_UNUSED(vertical);
    QVector<Tick> out;
    if (!range.hasArea() || base <= 1) return out;

    // 資料座標已經是 log 後的值（使用者畫圖前自己取 log），所以整數位就是十的次方
    int lo = static_cast<int>(std::floor(range.min));
    int hi = static_cast<int>(std::ceil(range.max));
    if (hi - lo > 40) return out;  // 範圍太大就不畫，避免刻度爆量

    // 範圍裡的主刻度少於兩根時，次刻度也要印字，否則軸上幾乎沒有數字
    int majorsInRange = 0;
    for (int e = lo; e <= hi; ++e)
        if (e >= range.min && e <= range.max) ++majorsInRange;
    const bool minorLabels = labelMinorTicks || majorsInRange < 2;

    for (int e = lo; e <= hi; ++e) {
        if (e >= range.min && e <= range.max) {
            Tick t;
            t.position = e;
            t.isMajor = true;
            t.label = formatLabel(1, e);
            out.append(t);
        }
        for (double m : minorMultipliers) {
            if (!(m > 1) || !(m < base)) continue;
            double pos = e + std::log10(m) / std::log10(base);
            if (pos >= range.min && pos <= range.max) {
                Tick t;
                t.position = pos;
                t.isMajor = false;
                if (minorLabels) t.label = formatLabel(m, e);
                out.append(t);
            }
        }
    }
    return out;
}

QString LogTickGenerator::formatLabel(double mantissa, int exponent) const {
    const double value = mantissa * std::pow(base, exponent);
    const bool isTen = std::abs(base - 10.0) < 1e-12;
    // 底數寫法：10、2、e
    const QString baseText = std::abs(base - std::exp(1.0)) < 1e-9 ? QStringLiteral("e") : QString::number(base, 'g', 6);
    const QString mant = (mantissa == 1) ? QString() : QString::number(mantissa, 'g', 6) + QChar(0x00D7);

    LabelFormat f = labelFormat;
    if (f == LabelFormat::Auto) f = (std::abs(exponent) <= 3) ? LabelFormat::Plain : LabelFormat::Superscript;
    // e 記號只對 10 為底有意義；其他底數退回上標
    if (f == LabelFormat::ENotation && !isTen) f = LabelFormat::Superscript;

    switch (f) {
    case LabelFormat::Plain:
        return QString::number(value, 'g', 12);
    case LabelFormat::ENotation:
        return QString::number(mantissa, 'g', 6) + QStringLiteral("e") + QString::number(exponent);
    default:
        return mant + baseText + superscript(exponent);
    }
}

// ───────────────────── DateTimeTickGenerator ─────────────────────

QVector<Tick> DateTimeTickGenerator::generate(const CoordinateRange& range, double lengthPx,
                                              const FontStyle& font, bool vertical) {
    QVector<Tick> out;
    if (!range.hasArea() || lengthPx <= 1) return out;

    QFontMetricsF fm(font.font());
    double labelExtent = vertical ? fm.height() : fm.horizontalAdvance(QStringLiteral("0000-00-00"));
    int targetCount = std::max(2, static_cast<int>(lengthPx / (labelExtent + 10)));

    const double span = range.span();  // 秒

    // 刻度必須對齊日曆邊界（整點、午夜、一號、元旦），不能用「固定秒數」的格線。
    // 一個月不是固定秒數：用 2629800（平均月長）當間距的話，刻度會落在
    // 1 月 1 日與 1 月 31 日，兩根都被標成「2026-01」—— 標籤重複而且位置沒有意義。
    enum class Unit { Second, Minute, Hour, Day, Month, Year };
    struct Step {
        Unit unit;
        int count;
        double approxSeconds;
    };
    static const Step kSteps[] = {
        {Unit::Second, 1, 1},        {Unit::Second, 2, 2},      {Unit::Second, 5, 5},
        {Unit::Second, 10, 10},      {Unit::Second, 15, 15},    {Unit::Second, 30, 30},
        {Unit::Minute, 1, 60},       {Unit::Minute, 2, 120},    {Unit::Minute, 5, 300},
        {Unit::Minute, 10, 600},     {Unit::Minute, 15, 900},   {Unit::Minute, 30, 1800},
        {Unit::Hour, 1, 3600},       {Unit::Hour, 2, 7200},     {Unit::Hour, 3, 10800},
        {Unit::Hour, 6, 21600},      {Unit::Hour, 12, 43200},
        {Unit::Day, 1, 86400},       {Unit::Day, 2, 172800},    {Unit::Day, 7, 604800},
        {Unit::Day, 14, 1209600},
        {Unit::Month, 1, 2629800},   {Unit::Month, 2, 5259600}, {Unit::Month, 3, 7889400},
        {Unit::Month, 6, 15778800},
        {Unit::Year, 1, 31557600},   {Unit::Year, 2, 63115200}, {Unit::Year, 5, 157788000},
        {Unit::Year, 10, 315576000}, {Unit::Year, 20, 631152000},
        {Unit::Year, 50, 1577880000}, {Unit::Year, 100, 3155760000.0},
    };
    Step step = kSteps[0];
    for (const Step& s : kSteps) {
        step = s;
        if (span / s.approxSeconds <= targetCount) break;
    }

    const int n = std::max(1, intervalCount);
    switch (interval) {
    case Interval::Auto:    break;
    case Interval::Seconds: step = {Unit::Second, n, 1.0 * n}; break;
    case Interval::Minutes: step = {Unit::Minute, n, 60.0 * n}; break;
    case Interval::Hours:   step = {Unit::Hour, n, 3600.0 * n}; break;
    case Interval::Days:    step = {Unit::Day, n, 86400.0 * n}; break;
    case Interval::Weeks:   step = {Unit::Day, 7 * n, 604800.0 * n}; break;
    case Interval::Months:  step = {Unit::Month, n, 2629800.0 * n}; break;
    case Interval::Years:   step = {Unit::Year, n, 31557600.0 * n}; break;
    }
    // 固定間隔可能讓刻度多到上千根，太多就不畫（標籤會疊成一團，而且很慢）
    if (interval != Interval::Auto && span / step.approxSeconds > 400) return out;

    QString fmt = format;
    if (fmt.isEmpty()) {
        switch (step.unit) {
        case Unit::Second: fmt = QStringLiteral("HH:mm:ss"); break;
        case Unit::Minute:
        case Unit::Hour:   fmt = QStringLiteral("MM-dd HH:mm"); break;
        case Unit::Day:    fmt = QStringLiteral("yyyy-MM-dd"); break;
        case Unit::Month:  fmt = QStringLiteral("yyyy-MM"); break;
        case Unit::Year:   fmt = QStringLiteral("yyyy"); break;
        }
    }

    const QDateTime from = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(std::floor(range.min)));

    // 先退到不晚於 range.min 的那個邊界，再一格一格往前走
    QDateTime cursor;
    switch (step.unit) {
    case Unit::Second:
    case Unit::Minute:
    case Unit::Hour: {
        // 從當天午夜起算，整點／整分才會對齊
        cursor = QDateTime(from.date(), QTime(0, 0), from.timeZone());
        const qint64 unitSecs = (step.unit == Unit::Second) ? 1 : (step.unit == Unit::Minute ? 60 : 3600);
        const qint64 stepSecs = unitSecs * step.count;
        const qint64 elapsed = cursor.secsTo(from);
        cursor = cursor.addSecs((elapsed / stepSecs) * stepSecs);
        break;
    }
    case Unit::Day: {
        QDate d = from.date();
        // 週刻度從週一起算，每週的刻度才會落在同一天
        if (interval == Interval::Weeks) d = d.addDays(1 - d.dayOfWeek());
        cursor = QDateTime(d, QTime(0, 0), from.timeZone());
        break;
    }
    case Unit::Month: {
        const QDate d = from.date();
        // 對齊到步長的整數倍月份（1 月、3 月、…）
        const int monthIndex = (d.month() - 1) / step.count * step.count + 1;
        cursor = QDateTime(QDate(d.year(), monthIndex, 1), QTime(0, 0), from.timeZone());
        break;
    }
    case Unit::Year: {
        const int year = from.date().year() / step.count * step.count;
        cursor = QDateTime(QDate(year, 1, 1), QTime(0, 0), from.timeZone());
        break;
    }
    }

    auto advance = [&](const QDateTime& dt) {
        switch (step.unit) {
        case Unit::Second: return dt.addSecs(step.count);
        case Unit::Minute: return dt.addSecs(static_cast<qint64>(step.count) * 60);
        case Unit::Hour:   return dt.addSecs(static_cast<qint64>(step.count) * 3600);
        case Unit::Day:    return dt.addDays(step.count);
        case Unit::Month:  return dt.addMonths(step.count);
        case Unit::Year:   return dt.addYears(step.count);
        }
        return dt;
    };

    // cursor 可能還在範圍左邊，往前推到範圍內
    int guard = 0;
    while (cursor.toSecsSinceEpoch() < range.min && ++guard < 1000) cursor = advance(cursor);

    guard = 0;
    while (cursor.toSecsSinceEpoch() <= range.max && ++guard < 400) {
        Tick t;
        t.position = static_cast<double>(cursor.toSecsSinceEpoch());
        t.isMajor = true;
        t.label = labelFormatter ? labelFormatter(cursor, step.approxSeconds) : cursor.toString(fmt);
        out.append(t);
        cursor = advance(cursor);
    }
    return out;
}

// ───────────────────────────── Axis ─────────────────────────────

Axis::Axis(Edge e) : m_edge(e) {
    tickGenerator = std::make_shared<AutomaticTickGenerator>();
    tickFont.size = 9.5;
    label.font.size = 11;
    subLabel.font.size = 9;
    subLabel.font.color = QColor(0, 0, 0, 150);
    minorTickLine.color = QColor(0, 0, 0, 140);
    if (isVertical()) label.font.rotation = subLabel.font.rotation = -90;
}

LogTickGenerator& Axis::defaultLogTicks() const {
    if (!m_logTicks) m_logTicks = std::make_shared<LogTickGenerator>();
    return *m_logTicks;
}

void Axis::regenerateTicks(double lengthPx) {
    regenerateTicksRaw(lengthPx);
    if (tickStyler)
        for (Tick& t : m_ticks) tickStyler(t);
}

void Axis::regenerateTicksRaw(double lengthPx) {
    if (logScale) {
        // 在 log_base 空間產生刻度，再把位置換回原始數值
        LogTickGenerator* lg = dynamic_cast<LogTickGenerator*>(tickGenerator.get());
        if (!lg) lg = &defaultLogTicks();
        double lo, hi;
        logBounds(range, lo, hi);
        const double toBase = 1.0 / std::log10(lg->base > 1 ? lg->base : 10.0);
        m_ticks = lg->generate({lo * toBase, hi * toBase}, lengthPx, tickFont, isVertical());
        for (Tick& t : m_ticks) t.position = std::pow(lg->base > 1 ? lg->base : 10.0, t.position);
        return;
    }
    if (!tickGenerator) {
        m_ticks.clear();
        return;
    }
    if (m_breaks.isEmpty()) {
        m_ticks = tickGenerator->generate(range, lengthPx, tickFont, isVertical());
        return;
    }
    // 截斷軸：每一段各自產生刻度。整段一起產生的話，間距是照「整個範圍」挑的，
    // 0–10 加 1000–1010 會挑出 200 的間距，兩段裡都只剩端點。
    m_ticks.clear();
    for (const CoordinateRange& seg : m_breaks.visibleSegments(range)) {
        const double segPx = (m_breaks.toFraction(seg.max, range, lengthPx) -
                              m_breaks.toFraction(seg.min, range, lengthPx)) * lengthPx;
        if (segPx < 1) continue;
        for (const Tick& t : tickGenerator->generate(seg, segPx, tickFont, isVertical())) {
            if (t.position < seg.min || t.position > seg.max) continue;  // 不讓刻度跑進截斷區
            m_ticks.append(t);
        }
    }
}

void Axis::addBreak(double from, double to) {
    if (!std::isfinite(from) || !std::isfinite(to) || from == to) return;
    if (from > to) std::swap(from, to);
    QVector<AxisBreak>& v = m_breaks.breaks;
    v.append({from, to});
    std::sort(v.begin(), v.end(), [](const AxisBreak& a, const AxisBreak& b) { return a.from < b.from; });
    // 合併重疊的截斷，換算時才能假設它們互不相交
    QVector<AxisBreak> merged;
    for (const AxisBreak& b : v) {
        if (!merged.isEmpty() && b.from <= merged.last().to) merged.last().to = std::max(merged.last().to, b.to);
        else merged.append(b);
    }
    v = merged;
}

void Axis::renderBreaks(QPainter& painter, const PixelRect& d, QColor background) const {
    if (m_breaks.isEmpty() || !isVisible) return;
    const double length = isVertical() ? d.height() : d.width();
    const auto gaps = m_breaks.gapFractions(range, length);
    if (gaps.isEmpty()) return;

    painter.save();
    painter.setClipping(false);
    const double s = breakMarkSize;
    for (const auto& g : gaps) {
        // 比例 → 像素：要套反向，和 getPixel 同一套
        auto at = [&](double f) {
            if (inverted) f = 1.0 - f;
            return isVertical() ? d.bottom - f * d.height() : d.left + f * d.width();
        };
        const double a = at(g.first), b = at(g.second);
        const double lo = std::min(a, b), hi = std::max(a, b);

        if (fillBreakGap && background.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(background);
            // 多蓋 1.5 像素，把繪圖區兩側框線在空隙裡的那一截也蓋掉
            if (isVertical()) painter.drawRect(QRectF(d.left - 1.5, lo, d.width() + 3, hi - lo));
            else              painter.drawRect(QRectF(lo, d.top - 1.5, hi - lo, d.height() + 3));
        }

        if (!breakMarkLine.isVisible() || s <= 0) continue;
        painter.setPen(breakMarkLine.pen());
        // 兩側框線上各畫一對平行斜線（//），標出斷開的兩端
        const double edges[2] = {isVertical() ? d.left : d.top, isVertical() ? d.right : d.bottom};
        for (double e : edges) {
            for (double p : {lo, hi}) {
                if (isVertical()) painter.drawLine(QPointF(e - s, p + s / 2), QPointF(e + s, p - s / 2));
                else              painter.drawLine(QPointF(p - s / 2, e + s), QPointF(p + s / 2, e - s));
            }
        }
    }
    painter.restore();
}

QString Axis::multiplierLabel() const {
    // 只有自動產生器會算倍率
    if (auto* automatic = dynamic_cast<AutomaticTickGenerator*>(tickGenerator.get()))
        return automatic->multiplierLabel;
    return QString();
}

double Axis::getPixel(double coord, const PixelRect& dataRect) const {
    const double frac = logScale ? logFraction(coord, range)
                        : m_breaks.isEmpty()
                            ? (coord - range.min) / range.span()
                            : m_breaks.toFraction(coord, range, isVertical() ? dataRect.height() : dataRect.width());
    const double f = inverted ? (1.0 - frac) : frac;
    if (isVertical()) return dataRect.bottom - f * dataRect.height();
    return dataRect.left + f * dataRect.width();
}

double Axis::getCoordinate(double pixel, const PixelRect& dataRect) const {
    double frac;
    if (isVertical()) frac = (dataRect.bottom - pixel) / dataRect.height();
    else              frac = (pixel - dataRect.left) / dataRect.width();
    if (inverted) frac = 1.0 - frac;
    if (logScale) return logFromFraction(frac, range);
    if (!m_breaks.isEmpty())
        return m_breaks.fromFraction(frac, range, isVertical() ? dataRect.height() : dataRect.width());
    return range.min + frac * range.span();
}

void Axis::pan(double fromPixel, double toPixel, const PixelRect& dataRect) {
    const double a = getCoordinate(fromPixel, dataRect), b = getCoordinate(toPixel, dataRect);
    if (logScale) {
        if (!(a > 0) || !(b > 0)) return;
        // 對數空間平移 = 兩端乘上同一個倍數
        const double ratio = a / b;
        const CoordinateRange r = positiveRange(range);
        range = {r.min * ratio, r.max * ratio};
        return;
    }
    range.translate(a - b);
}

void Axis::zoom(double factor, double centerCoord) {
    if (!(factor > 0)) return;
    if (logScale) {
        double lo, hi;
        logBounds(range, lo, hi);
        const double c = (centerCoord > 0) ? std::log10(centerCoord) : (lo + hi) / 2;
        range = {std::pow(10.0, c + (lo - c) * factor), std::pow(10.0, c + (hi - c) * factor)};
        return;
    }
    range.zoom(factor, std::isnan(centerCoord) ? range.center() : centerCoord);
}

CoordinateRange Axis::positiveRange(CoordinateRange r) {
    if (!r.isValid() || !(r.max > 0)) return {1, 10};
    if (!(r.min > 0)) r.min = r.max / 1000;
    if (!(r.max > r.min)) {
        // 只有一個值：上下各留半個十的次方
        r = {r.min / std::sqrt(10.0), r.max * std::sqrt(10.0)};
    }
    return r;
}

void Axis::setColor(const QColor& color) {
    frameLine.color = color;
    majorTickLine.color = color;
    minorTickLine.color = color;
    majorTickMarker.fillColor = color;
    majorTickMarker.outline.color = color;
    tickFont.color = color;
    label.font.color = color;
}

double Axis::outwardTickLength() const {
    double out = 0;
    switch (tickDirection) {
    case TickDirection::Outside: out = majorTickLength; break;
    case TickDirection::Inside:  out = 0; break;
    case TickDirection::Cross:   out = majorTickLength / 2; break;
    }
    if (majorTickMarker.isVisible()) {
        const double s = majorTickMarker.size;
        out = std::max(out, tickDirection == TickDirection::Outside ? s : tickDirection == TickDirection::Cross ? s / 2 : 0.0);
    }
    return out;
}

double Axis::multiplierHeadroom() const {
    if (!isVisible || !showTickLabels || !isVertical()) return 0;
    if (multiplierLabel().isEmpty()) return 0;
    return QFontMetricsF(tickFont.font()).height() + 4;
}

double Axis::measureThickness() const {
    if (!isVisible) return 0;

    double thickness = outwardTickLength() + 2;
    if (showTickLabels) {
        QFontMetricsF fm(tickFont.font());
        const double rad = effectiveTickRotation() * kPi / 180.0;
        const double cs = std::abs(std::cos(rad)), sn = std::abs(std::sin(rad));
        const double lineHeight = fm.height();

        double widest = 0;
        for (const Tick& t : m_ticks) {
            if (!t.label.isEmpty())  // 對數軸的次刻度也可能有字
                widest = std::max(widest, static_cast<double>(fm.horizontalAdvance(t.label)));
        }

        if (isVertical()) {
            // 垂直軸的厚度是最寬的刻度標籤（轉過之後要看外接框的寬）
            thickness += widest * cs + lineHeight * sn + 4;
        } else {
            // 水平軸平常只要一行高；一旦轉起來，長標籤會往下伸很多，
            // 厚度得照外接框的高算，否則文字會被圖的下緣切掉
            thickness += widest * sn + lineHeight * cs + 2;
        }
    }
    if (!labelImage.isNull()) {
        thickness += (isVertical() ? labelImage.width() : labelImage.height()) + 4;
    } else if (label.isVisible()) {
        QSizeF s = measureText(label);
        thickness += (isVertical() ? s.width() : s.height()) + 4;
    }
    if (subLabel.isVisible()) {
        QSizeF s = measureText(subLabel);
        thickness += (isVertical() ? s.width() : s.height()) + 4;
    }
    return thickness;
}

void Axis::render(QPainter& painter, const PixelRect& dataRect0) const {
    if (!isVisible) return;

    // 同一邊有多根軸時，第 2 根以後要往外讓開。位移只影響「畫在哪」；
    // 座標換算仍然用原本的繪圖區，所以資料的位置不會跟著跑掉。
    PixelRect dataRect = dataRect0;
    switch (m_edge) {
    case Edge::Left:   dataRect.left -= renderOffsetPx; break;
    case Edge::Right:  dataRect.right += renderOffsetPx; break;
    case Edge::Bottom: dataRect.bottom += renderOffsetPx; break;
    case Edge::Top:    dataRect.top -= renderOffsetPx; break;
    }

    painter.save();

    // 框線：貼著繪圖區的那一邊
    if (showFrame && frameLine.isVisible()) {
        painter.setPen(frameLine.pen());
        switch (m_edge) {
        case Edge::Left:   painter.drawLine(QPointF(dataRect.left, dataRect.top),
                                            QPointF(dataRect.left, dataRect.bottom)); break;
        case Edge::Right:  painter.drawLine(QPointF(dataRect.right, dataRect.top),
                                            QPointF(dataRect.right, dataRect.bottom)); break;
        case Edge::Bottom: painter.drawLine(QPointF(dataRect.left, dataRect.bottom),
                                            QPointF(dataRect.right, dataRect.bottom)); break;
        case Edge::Top:    painter.drawLine(QPointF(dataRect.left, dataRect.top),
                                            QPointF(dataRect.right, dataRect.top)); break;
        }
    }

    QFontMetricsF fm(tickFont.font());
    double maxLabelExtent = 0;

    // 一根刻度：base 是它在框線上的點，normal 是指向繪圖區外的單位向量。
    // 鏡像刻度畫在對面那一邊，normal 反過來（一樣是「外」），所以 Inside 仍然是往繪圖區裡面。
    auto drawTick = [&](QPointF base, QPointF normal, double len, const LineStyle& ls, bool major, const QColor& override) {
        double out = len, in = 0;
        if (tickDirection == TickDirection::Inside) { out = 0; in = len; }
        if (tickDirection == TickDirection::Cross)  { out = in = len / 2; }
        if (ls.isVisible() && len > 0) {
            LineStyle s = ls;
            if (override.isValid()) s.color = override;
            painter.setPen(s.pen());
            painter.drawLine(base + normal * out, base - normal * in);
        }
        if (major && majorTickMarker.isVisible()) {
            MarkerStyle m = majorTickMarker;
            if (override.isValid()) { m.fillColor = override; m.outline.color = override; }
            const double half = m.size / 2;
            const double shift = (tickDirection == TickDirection::Outside) ? half
                                 : (tickDirection == TickDirection::Inside) ? -half : 0.0;
            drawMarker(painter, base + normal * shift, m);
        }
    };
    QPointF normal, mirrorNormal;
    switch (m_edge) {
    case Edge::Left:   normal = QPointF(-1, 0); break;
    case Edge::Right:  normal = QPointF(1, 0); break;
    case Edge::Bottom: normal = QPointF(0, 1); break;
    case Edge::Top:    normal = QPointF(0, -1); break;
    }
    mirrorNormal = -normal;

    for (const Tick& t : m_ticks) {
        if (t.position < range.min || t.position > range.max) continue;
        const double p = getPixel(t.position, dataRect0);  // 換算一律用原本的繪圖區
        const double len = (t.length >= 0) ? t.length : (t.isMajor ? majorTickLength : minorTickLength);
        const LineStyle& ls = t.isMajor ? majorTickLine : minorTickLine;

        QPointF base, mirrorBase;
        switch (m_edge) {
        case Edge::Left:   base = QPointF(dataRect.left, p);   mirrorBase = QPointF(dataRect0.right, p); break;
        case Edge::Right:  base = QPointF(dataRect.right, p);  mirrorBase = QPointF(dataRect0.left, p); break;
        case Edge::Bottom: base = QPointF(p, dataRect.bottom); mirrorBase = QPointF(p, dataRect0.top); break;
        case Edge::Top:    base = QPointF(p, dataRect.top);    mirrorBase = QPointF(p, dataRect0.bottom); break;
        }
        drawTick(base, normal, len, ls, t.isMajor, t.color);
        if (mirrorTicks) drawTick(mirrorBase, mirrorNormal, len, ls, t.isMajor, t.color);

        if (showTickLabels && !t.label.isEmpty()) {
            Label lb;
            lb.text = t.label;
            lb.font = tickFont;
            if (t.color.isValid()) lb.font.color = t.color;
            const double tickRotation = effectiveTickRotation();
            lb.font.rotation = tickRotation;
            // 文字排在刻度外側的盡頭；刻度往內畫的話就貼著框線
            double outward = outwardTickLength();
            if (t.length >= 0 && tickDirection != TickDirection::Inside)
                outward = std::max(outward, tickDirection == TickDirection::Cross ? t.length / 2 : t.length);
            const double gap = outward + 3;
            const bool rotated = tickRotation != 0;
            const double labelWidth = fm.horizontalAdvance(t.label);

            // 轉過的標籤要把「靠近刻度的那一端」對到刻度上。
            // drawText 是繞 anchor 轉的，對齊是在轉過的座標系裡算的：
            // 往上甩（負角）時文字往左下延伸，所以靠右對齊；往下甩則相反。
            const Qt::Alignment rotatedAlign =
                (tickRotation < 0 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;

            switch (m_edge) {
            case Edge::Left:
                drawText(painter, QPointF(dataRect.left - gap, p), lb,
                         rotated ? rotatedAlign : (Qt::AlignRight | Qt::AlignVCenter));
                maxLabelExtent = std::max(maxLabelExtent, labelWidth);
                break;
            case Edge::Right:
                drawText(painter, QPointF(dataRect.right + gap, p), lb,
                         rotated ? rotatedAlign : (Qt::AlignLeft | Qt::AlignVCenter));
                maxLabelExtent = std::max(maxLabelExtent, labelWidth);
                break;
            case Edge::Bottom:
                drawText(painter, QPointF(p, dataRect.bottom + gap), lb,
                         rotated ? rotatedAlign : (Qt::AlignHCenter | Qt::AlignTop));
                maxLabelExtent = std::max(maxLabelExtent, static_cast<double>(fm.height()));
                break;
            case Edge::Top:
                drawText(painter, QPointF(p, dataRect.top - gap), lb,
                         rotated ? rotatedAlign : (Qt::AlignHCenter | Qt::AlignBottom));
                maxLabelExtent = std::max(maxLabelExtent, static_cast<double>(fm.height()));
                break;
            }
        }
    }

    // 倍率標示：放在軸的末端（水平軸的右端、垂直軸的上端）
    if (showTickLabels) {
        const QString mul = multiplierLabel();
        if (!mul.isEmpty()) {
            Label lb;
            lb.text = mul;
            lb.font = tickFont;
            // 一律貼著繪圖區的內緣放。放到外緣去的話，圖的邊距只有十幾個像素，
            // 標示會被圖的邊界切掉。水平軸再往下讓一行，才不會壓到最後一個刻度數字。
            const double gap = outwardTickLength() + 3;
            const double lineHeight = fm.height();
            switch (m_edge) {
            case Edge::Bottom:
                drawText(painter, QPointF(dataRect.right, dataRect.bottom + gap + lineHeight), lb,
                         Qt::AlignRight | Qt::AlignTop);
                break;
            case Edge::Top:
                drawText(painter, QPointF(dataRect.right, dataRect.top - gap - lineHeight), lb,
                         Qt::AlignRight | Qt::AlignBottom);
                break;
            case Edge::Left:
                drawText(painter, QPointF(dataRect.left, dataRect.top - 3), lb,
                         Qt::AlignLeft | Qt::AlignBottom);
                break;
            case Edge::Right:
                drawText(painter, QPointF(dataRect.right, dataRect.top - 3), lb,
                         Qt::AlignRight | Qt::AlignBottom);
                break;
            }
        }
    }

    // 軸標題：排在刻度標籤外側。
    // 標題是以中心對齊畫的，所以還要讓開它自己一半的厚度，
    // 否則垂直軸的標題會壓在刻度數字上。
    // outer：下一個東西（副標題）要從離繪圖區多遠的地方開始排
    double outer = outwardTickLength() + 6 + maxLabelExtent;
    const double midX = (dataRect.left + dataRect.right) / 2;
    const double midY = (dataRect.top + dataRect.bottom) / 2;
    if (!labelImage.isNull()) {
        const double w = labelImage.width(), h = labelImage.height();
        QRectF r;
        switch (m_edge) {
        case Edge::Left:   r = QRectF(dataRect.left - outer - w, midY - h / 2, w, h); break;
        case Edge::Right:  r = QRectF(dataRect.right + outer, midY - h / 2, w, h); break;
        case Edge::Bottom: r = QRectF(midX - w / 2, dataRect.bottom + outer, w, h); break;
        case Edge::Top:    r = QRectF(midX - w / 2, dataRect.top - outer - h, w, h); break;
        }
        painter.drawImage(r, labelImage);
        outer += (isVertical() ? w : h) + 4;
    } else if (label.isVisible()) {
        const QSizeF labelSize = measureText(label);
        const double half = (isVertical() ? labelSize.width() : labelSize.height()) / 2;
        const double gap = outwardTickLength() + 6 + maxLabelExtent + half;
        outer = gap + (isVertical() ? half : half * 2 + 0) + 2;
        switch (m_edge) {
        case Edge::Left:
            drawText(painter, QPointF(dataRect.left - gap, (dataRect.top + dataRect.bottom) / 2),
                     label, Qt::AlignHCenter | Qt::AlignVCenter);
            break;
        case Edge::Right:
            drawText(painter, QPointF(dataRect.right + gap, (dataRect.top + dataRect.bottom) / 2),
                     label, Qt::AlignHCenter | Qt::AlignVCenter);
            break;
        case Edge::Bottom:
            drawText(painter, QPointF((dataRect.left + dataRect.right) / 2, dataRect.bottom + gap),
                     label, Qt::AlignHCenter | Qt::AlignTop);
            break;
        case Edge::Top:
            drawText(painter, QPointF((dataRect.left + dataRect.right) / 2, dataRect.top - gap),
                     label, Qt::AlignHCenter | Qt::AlignBottom);
            break;
        }
    }

    if (subLabel.isVisible()) {
        const QSizeF s = measureText(subLabel);
        const double half = (isVertical() ? s.width() : s.height()) / 2;
        const double c = outer + half;
        switch (m_edge) {
        case Edge::Left:   drawText(painter, QPointF(dataRect.left - c, midY), subLabel, Qt::AlignHCenter | Qt::AlignVCenter); break;
        case Edge::Right:  drawText(painter, QPointF(dataRect.right + c, midY), subLabel, Qt::AlignHCenter | Qt::AlignVCenter); break;
        case Edge::Bottom: drawText(painter, QPointF(midX, dataRect.bottom + c), subLabel, Qt::AlignHCenter | Qt::AlignVCenter); break;
        case Edge::Top:    drawText(painter, QPointF(midX, dataRect.top - c), subLabel, Qt::AlignHCenter | Qt::AlignVCenter); break;
        }
    }

    painter.restore();
}

void Axis::renderGridFill(QPainter& painter, const PixelRect& dataRect) const {
    if (!isVisible) return;
    if (!gridFillColor1.isValid() && !gridFillColor2.isValid()) return;

    // 條帶的邊界是主格線。先收集落在視野內的主刻度。
    QVector<double> edges;
    for (const Tick& t : m_ticks) {
        if (!t.isMajor) continue;
        if (t.position < range.min || t.position > range.max) continue;
        edges.append(t.position);
    }
    if (edges.size() < 2) return;
    std::sort(edges.begin(), edges.end());
    // 對數軸的主刻度是等比的，判斷「第幾格」要在對數空間算
    auto lin = [&](double v) { return logScale ? std::log10(std::max(v, 1e-300)) : v; };
    const double spacing = lin(edges[1]) - lin(edges[0]);
    if (!(spacing > 0)) return;

    // 視野邊緣那兩段也要填，否則左右兩側會各缺一塊
    if (edges.first() > range.min) edges.prepend(range.min);
    if (edges.last() < range.max) edges.append(range.max);

    painter.save();
    painter.setPen(Qt::NoPen);
    for (qsizetype i = 0; i + 1 < edges.size(); ++i) {
        // 用中點落在第幾格來決定顏色，而不是用陣列索引 ——
        // 索引會隨著平移時「視野內有幾根刻度」而變，條帶會忽然整排換色
        const double mid = (lin(edges[i]) + lin(edges[i + 1])) / 2;
        const long long k = static_cast<long long>(std::floor(mid / spacing));
        const QColor& c = ((k % 2 + 2) % 2 == 0) ? gridFillColor1 : gridFillColor2;
        if (!c.isValid() || c.alpha() == 0) continue;

        const double a = getPixel(edges[i], dataRect);
        const double b = getPixel(edges[i + 1], dataRect);
        const QRectF band = isVertical()
                                ? QRectF(QPointF(dataRect.left, std::min(a, b)),
                                         QPointF(dataRect.right, std::max(a, b)))
                                : QRectF(QPointF(std::min(a, b), dataRect.top),
                                         QPointF(std::max(a, b), dataRect.bottom));
        painter.fillRect(band, c);
    }
    painter.restore();
}

void Axis::renderGridlines(QPainter& painter, const PixelRect& dataRect,
                           const LineStyle& defaultMajor, const LineStyle& defaultMinor) const {
    if (!isVisible) return;
    const LineStyle& majorStyle = gridMajorLine ? *gridMajorLine : defaultMajor;
    const LineStyle& minorStyle = gridMinorLine ? *gridMinorLine : defaultMinor;

    painter.save();
    for (int pass = 0; pass < 2; ++pass) {
        const bool major = (pass == 1);  // 主格線畫在次格線之上
        const LineStyle& ls = major ? majorStyle : minorStyle;
        if (!ls.isVisible()) continue;
        painter.setPen(ls.pen());
        for (const Tick& t : m_ticks) {
            if (t.isMajor != major) continue;
            if (t.position < range.min || t.position > range.max) continue;
            const double p = getPixel(t.position, dataRect);
            if (isVertical()) painter.drawLine(QPointF(dataRect.left, p), QPointF(dataRect.right, p));
            else              painter.drawLine(QPointF(p, dataRect.top), QPointF(p, dataRect.bottom));
        }
    }
    painter.restore();
}

}  // namespace qtplot

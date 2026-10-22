#include "Live.h"
#include "Decimation.h"

#include <QPolygonF>

namespace qtplot {

namespace {

/// 畫一段 X 遞增的 (x, y) 折線：點比像素多時改畫包絡線（每個像素行一條最小到最大的直線）。
/// xsSorted 為 false 時（DataStreamerXY 的事件可能亂序）不能二分搜尋，退回直接畫折線。
void drawDecimated(QPainter& p, const Transform& t, const QVector<double>& xs, const QVector<double>& ys,
                   qsizetype from, qsizetype to, const LineStyle& line, const MarkerStyle& marker,
                   bool xsSorted = true) {
    if (to - from < 2 || !line.isVisible()) return;
    const double pxFrom = t.px(xs[from]), pxTo = t.px(xs[to - 1]);
    const double pointsPerPixel = (to - from) / std::max(1.0, std::abs(pxTo - pxFrom));

    if (xsSorted && pointsPerPixel > 2.0 && std::isfinite(pxFrom) && std::isfinite(pxTo)) {
        const double* x = xs.constData();
        const double* y = ys.constData();
        const auto cols = buildEnvelope(
            pxFrom, pxTo, from, to - 1,
            [&](double e) { return static_cast<qsizetype>(std::lower_bound(x + from, x + to, t.cx(e)) - x); },
            [&](qsizetype a, qsizetype b, double& lo, double& hi) { return scanMinMax(y, a, b, lo, hi); },
            [&](double v) { return t.py(v); });
        drawEnvelope(p, cols, line, false);
        return;
    }

    QPolygonF pts;
    pts.reserve(to - from);
    for (qsizetype i = from; i < to; ++i) {
        if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) continue;
        pts.append(t.p(xs[i], ys[i]));
    }
    if (pts.size() < 2) return;
    p.setPen(line.pen());
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(pts);
    if (marker.isVisible()) drawMarkers(p, QVector<QPointF>(pts.begin(), pts.end()), marker);
}

/// 等間距的一段：第 j 點（j = 0..n-1）在 x = x0 + j·dx，值是 valueAt(j)。
/// DataStreamer 的資料在循環緩衝裡，不是連續陣列，所以用存取函式。
template <class ValueAt>
void drawUniform(QPainter& p, const Transform& t, qsizetype n, double x0, double dx, ValueAt valueAt,
                 const LineStyle& line) {
    if (n < 2 || dx == 0 || !line.isVisible()) return;
    const double pxFrom = t.px(x0), pxTo = t.px(x0 + (n - 1) * dx);
    const double pointsPerPixel = n / std::max(1.0, std::abs(pxTo - pxFrom));

    if (pointsPerPixel > 2.0) {
        const auto cols = buildEnvelope(
            pxFrom, pxTo, 0, n - 1,
            [&](double e) {
                const double fj = (t.cx(e) - x0) / dx;
                return static_cast<qsizetype>(std::clamp(std::ceil(fj), 0.0, double(n)));
            },
            [&](qsizetype a, qsizetype b, double& lo, double& hi) {
                lo = std::numeric_limits<double>::infinity();
                hi = -lo;
                for (qsizetype j = a; j < b; ++j) {
                    const double v = valueAt(j);
                    if (!std::isfinite(v)) continue;
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
                return lo <= hi;
            },
            [&](double v) { return t.py(v); });
        drawEnvelope(p, cols, line, false);
        return;
    }

    QPolygonF pts;
    pts.reserve(n);
    for (qsizetype j = 0; j < n; ++j) {
        const double v = valueAt(j);
        if (!std::isfinite(v)) continue;
        pts.append(t.p(x0 + j * dx, v));
    }
    if (pts.size() < 2) return;
    p.setPen(line.pen());
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(pts);
}

/// 把一段資料的 Y 範圍套到軸上。
void applyYRange(Axis* axis, CoordinateRange r, double padding, bool growOnly) {
    if (!axis || !r.isValid()) return;
    if (growOnly && axis->range.isValid()) r.expand(axis->range);
    r.ensureArea(1.0);
    r.pad(padding);
    axis->range = r;
}

}  // namespace

// ───────────────────────────── DataLogger ─────────────────────────────

void DataLogger::addLocked(double x, double y) {
    m_xs.append(x);
    m_ys.append(y);
}

void DataLogger::add(double y) {
    {
        QMutexLocker lock(&m_mutex);
        addLocked(xOffset + m_ys.size() * period, y);
    }
    ++m_version;
}

void DataLogger::add(double x, double y) {
    {
        QMutexLocker lock(&m_mutex);
        addLocked(x, y);
    }
    ++m_version;
}

void DataLogger::addRange(const QVector<double>& ys) {
    {
        QMutexLocker lock(&m_mutex);  // 一批只上一次鎖：背景執行緒一次塞幾千點時差很多
        m_xs.reserve(m_xs.size() + ys.size());
        m_ys.reserve(m_ys.size() + ys.size());
        for (double y : ys) addLocked(xOffset + m_ys.size() * period, y);
    }
    ++m_version;
}

void DataLogger::addRange(const QVector<double>& xs, const QVector<double>& ys) {
    {
        QMutexLocker lock(&m_mutex);
        const qsizetype n = std::min(xs.size(), ys.size());
        for (qsizetype i = 0; i < n; ++i) addLocked(xs[i], ys[i]);
    }
    ++m_version;
}

void DataLogger::setY(int index, double y) {
    {
        QMutexLocker lock(&m_mutex);
        if (index < 0 || index >= m_ys.size()) return;
        m_ys[index] = y;
    }
    ++m_version;
}

void DataLogger::clear() {
    {
        QMutexLocker lock(&m_mutex);
        m_xs.clear();
        m_ys.clear();
        m_viewRight = 0;
    }
    ++m_version;
}

int DataLogger::count() const {
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_ys.size());
}

void DataLogger::snapshot(QVector<double>& xs, QVector<double>& ys) const {
    QMutexLocker lock(&m_mutex);
    xs = m_xs;
    ys = m_ys;
}

double DataLogger::latest() const {
    QMutexLocker lock(&m_mutex);
    return m_ys.isEmpty() ? std::numeric_limits<double>::quiet_NaN() : m_ys.last();
}

void DataLogger::beforeLayout() {
    QMutexLocker lock(&m_mutex);
    if (m_ys.isEmpty() || view == LiveView::Manual) return;

    const double newest = m_xs.last();
    const double oldest = m_xs.first();

    CoordinateRange xr;
    switch (view) {
    case LiveView::Full:
        xr = {oldest, newest};
        break;
    case LiveView::Slide:
        // 永遠貼著最新的資料
        xr = {newest - viewWidth, newest};
        break;
    case LiveView::Jump:
        // 超出右邊界才跳，跳完往前留一段空間。這樣畫面是一格一格換的，
        // 不像 Slide 那樣整段一直滑動，讀數值時比較不暈。
        if (m_viewRight <= 0) m_viewRight = oldest + viewWidth;
        while (newest > m_viewRight) m_viewRight += viewWidth * std::clamp(jumpFraction, 0.1, 1.0);
        xr = {m_viewRight - viewWidth, m_viewRight};
        break;
    case LiveView::Manual:
        return;
    }
    xr.ensureArea(1.0);
    if (xAxis && !xAxis->locked) xAxis->range = xr;

    if (autoScaleY && yAxis && !yAxis->locked) {
        // 只看「可視範圍內」的資料 —— 否則早就捲出去的尖峰會一直把 Y 軸撐著
        // X 遞增：二分搜尋找可視範圍，不必掃過全部（記錄一整天的資料時差很多）
        CoordinateRange yr;
        const auto first = std::lower_bound(m_xs.cbegin(), m_xs.cend(), xr.min) - m_xs.cbegin();
        const auto last = std::upper_bound(m_xs.cbegin(), m_xs.cend(), xr.max) - m_xs.cbegin();
        double lo, hi;
        if (scanMinMax(m_ys.constData(), first, last, lo, hi)) {
            yr.expand(lo);
            yr.expand(hi);
        }
        applyYRange(yAxis, yr, yPadding, false);
    }
}

AxisLimits DataLogger::axisLimits() const {
    QMutexLocker lock(&m_mutex);
    AxisLimits lim;
    for (qsizetype i = 0; i < m_ys.size(); ++i) {
        if (!std::isfinite(m_xs[i]) || !std::isfinite(m_ys[i])) continue;
        lim.x.expand(m_xs[i]);
        lim.y.expand(m_ys[i]);
    }
    return lim;
}

void DataLogger::render(RenderPack& rp) {
    QMutexLocker lock(&m_mutex);
    if (m_ys.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // X 遞增，用二分搜尋找可視範圍
    auto lo = std::lower_bound(m_xs.cbegin(), m_xs.cend(), t.xr.min);
    auto hi = std::upper_bound(m_xs.cbegin(), m_xs.cend(), t.xr.max);
    qsizetype from = std::max<qsizetype>(0, (lo - m_xs.cbegin()) - 1);
    qsizetype to = std::min<qsizetype>(m_xs.size(), (hi - m_xs.cbegin()) + 1);

    drawDecimated(p, t, m_xs, m_ys, from, to, line, marker);

    if (latestMarker.isVisible() && std::isfinite(m_ys.last()))
        drawMarker(p, t.p(m_xs.last(), m_ys.last()), latestMarker);

    p.restore();
}

QVector<LegendItem> DataLogger::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    it.marker = marker;
    return {it};
}

// ───────────────────────────── DataStreamer ─────────────────────────────

DataStreamer::DataStreamer(int capacity) {
    setCapacity(capacity);
}

void DataStreamer::resetLocked(int capacity) {
    const int n = std::max(2, capacity);
    m_data.assign(n, std::numeric_limits<double>::quiet_NaN());
    m_count = 0;
    m_seenY = CoordinateRange();
}

void DataStreamer::setCapacity(int capacity) {
    {
        QMutexLocker lock(&m_mutex);
        resetLocked(capacity);
    }
    ++m_version;
}

void DataStreamer::addLocked(double y) {
    if (m_data.isEmpty()) return;
    m_data[static_cast<qsizetype>(m_count % m_data.size())] = y;
    ++m_count;
    if (std::isfinite(y)) m_seenY.expand(y);
}

void DataStreamer::add(double y) {
    {
        QMutexLocker lock(&m_mutex);
        addLocked(y);
    }
    ++m_version;
}

void DataStreamer::addRange(const QVector<double>& ys) {
    {
        QMutexLocker lock(&m_mutex);  // 一批只上一次鎖
        for (double y : ys) addLocked(y);
    }
    ++m_version;
}

void DataStreamer::clear() {
    {
        QMutexLocker lock(&m_mutex);
        resetLocked(static_cast<int>(m_data.size()));
    }
    ++m_version;
}

int DataStreamer::capacity() const {
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_data.size());
}

int DataStreamer::size() const {
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(std::min<qint64>(m_count, m_data.size()));
}

qint64 DataStreamer::totalAdded() const {
    QMutexLocker lock(&m_mutex);
    return m_count;
}

double DataStreamer::latest() const {
    QMutexLocker lock(&m_mutex);
    if (m_count == 0 || m_data.isEmpty()) return std::numeric_limits<double>::quiet_NaN();
    return m_data.at(static_cast<qsizetype>((m_count - 1) % m_data.size()));
}

void DataStreamer::beforeLayout() {
    QMutexLocker lock(&m_mutex);
    if (m_count == 0 || m_data.isEmpty()) return;

    const qsizetype cap = m_data.size();
    if (xAxis && !xAxis->locked) {
        if (mode == Mode::Wipe) {
            // 掃描模式的 X 永遠是 0..capacity，畫面不動，只有波形被覆蓋
            xAxis->range = {xOffset, xOffset + (cap - 1) * period};
        } else {
            const qint64 newest = m_count - 1;
            const qint64 oldest = std::max<qint64>(0, m_count - cap);
            xAxis->range = {xOffset + oldest * period, xOffset + newest * period};
        }
        xAxis->range.ensureArea(1.0);
    }

    if (autoScaleY && yAxis && !yAxis->locked) {
        CoordinateRange yr;
        if (growOnly) {
            yr = m_seenY;  // 看過的全部，軸只放大不縮小
        } else {
            for (double v : m_data)
                if (std::isfinite(v)) yr.expand(v);
        }
        applyYRange(yAxis, yr, yPadding, false);
    }
}

AxisLimits DataStreamer::axisLimits() const {
    QMutexLocker lock(&m_mutex);
    AxisLimits lim;
    if (m_count == 0 || m_data.isEmpty()) return lim;
    const qsizetype cap = m_data.size();
    if (mode == Mode::Wipe) {
        lim.x.expand(xOffset);
        lim.x.expand(xOffset + (cap - 1) * period);
    } else {
        const qint64 newest = m_count - 1;
        const qint64 oldest = std::max<qint64>(0, m_count - cap);
        lim.x.expand(xOffset + oldest * period);
        lim.x.expand(xOffset + newest * period);
    }
    for (double v : m_data)
        if (std::isfinite(v)) lim.y.expand(v);
    return lim;
}

void DataStreamer::render(RenderPack& rp) {
    QMutexLocker lock(&m_mutex);
    if (m_count == 0 || m_data.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    const double* data = m_data.constData();

    const qsizetype cap = m_data.size();
    const qsizetype head = static_cast<qsizetype>(m_count % cap);  // 下一個要寫的位置

    if (mode == Mode::Wipe) {
        // 緩衝就是畫面：位置 i 直接對到 x。寫入點前後各畫一段，
        // 中間留 wipeGap 個空位，才看得出新舊的交界在哪。
        auto drawRun = [&](qsizetype a, qsizetype b) {
            drawUniform(p, t, b - a, xOffset + a * period, period, [&](qsizetype j) { return data[a + j]; }, line);
        };
        const qsizetype gapEnd = std::min(cap, head + std::max(0, wipeGap));
        drawRun(0, head);
        drawRun(gapEnd, cap);

        if (wipeMarker.isVisible() && m_count >= cap) {
            p.setPen(wipeMarker.pen());
            const double x = t.px(xOffset + head * period);
            p.drawLine(QPointF(x, rp.dataRect.top), QPointF(x, rp.dataRect.bottom));
        }
    } else {
        // 捲動模式：由舊到新重新排列。緩衝未滿時從 0 開始，滿了之後從 head 開始。
        const qsizetype n = std::min<qint64>(m_count, cap);
        const qsizetype start = (m_count > cap) ? head : 0;
        const qint64 firstIndex = std::max<qint64>(0, m_count - cap);

        drawUniform(p, t, n, xOffset + firstIndex * period, period,
                    [&](qsizetype j) { return data[(start + j) % cap]; }, line);
    }
    p.restore();
}

QVector<LegendItem> DataStreamer::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    return {it};
}


// ───────────────────────── DataStreamerXY ─────────────────────────

DataStreamerXY::DataStreamerXY(int capacity) { resetLocked(capacity); }

void DataStreamerXY::resetLocked(int capacity) {
    const int n = std::max(2, capacity);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    m_xs.assign(n, nan);
    m_ys.assign(n, nan);
    m_count = 0;
}

void DataStreamerXY::setCapacity(int capacity) {
    {
        QMutexLocker lock(&m_mutex);
        resetLocked(capacity);
    }
    ++m_version;
}

void DataStreamerXY::add(double x, double y) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_ys.isEmpty()) return;
        const qsizetype i = static_cast<qsizetype>(m_count % m_ys.size());
        m_xs[i] = x;
        m_ys[i] = y;
        ++m_count;
    }
    ++m_version;
}

void DataStreamerXY::addRange(const QVector<double>& xs, const QVector<double>& ys) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_ys.isEmpty()) return;
        const qsizetype n = std::min(xs.size(), ys.size());
        for (qsizetype k = 0; k < n; ++k) {
            const qsizetype i = static_cast<qsizetype>(m_count % m_ys.size());
            m_xs[i] = xs[k];
            m_ys[i] = ys[k];
            ++m_count;
        }
    }
    ++m_version;
}

void DataStreamerXY::clear() {
    {
        QMutexLocker lock(&m_mutex);
        resetLocked(static_cast<int>(m_ys.size()));
    }
    ++m_version;
}

int DataStreamerXY::capacity() const {
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_ys.size());
}

int DataStreamerXY::size() const {
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(std::min<qint64>(m_count, m_ys.size()));
}

qint64 DataStreamerXY::totalAdded() const {
    QMutexLocker lock(&m_mutex);
    return m_count;
}

Coordinates DataStreamerXY::latest() const {
    QMutexLocker lock(&m_mutex);
    if (m_count == 0 || m_ys.isEmpty())
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()};
    const qsizetype i = static_cast<qsizetype>((m_count - 1) % m_ys.size());
    return {m_xs[i], m_ys[i]};
}

void DataStreamerXY::collect(QVector<double>& xs, QVector<double>& ys) const {
    QMutexLocker lock(&m_mutex);
    const qsizetype cap = m_ys.size();
    const qsizetype n = std::min<qint64>(m_count, cap);
    // 緩衝未滿時從 0 開始；滿了之後最舊的在寫入點
    const qsizetype start = (m_count > cap) ? static_cast<qsizetype>(m_count % cap) : 0;
    xs.reserve(n);
    ys.reserve(n);
    for (qsizetype j = 0; j < n; ++j) {
        const qsizetype i = (start + j) % cap;
        if (!std::isfinite(m_xs[i]) || !std::isfinite(m_ys[i])) continue;
        xs.append(m_xs[i]);
        ys.append(m_ys[i]);
    }
}

void DataStreamerXY::beforeLayout() {
    if (m_count == 0) return;
    QVector<double> xs, ys;
    collect(xs, ys);
    if (xs.isEmpty()) return;

    if (xAxis && !xAxis->locked) {
        // X 不保證遞增（事件可能亂序到達），所以取 min/max 而不是頭尾
        CoordinateRange xr;
        for (double x : xs) xr.expand(x);
        if (viewWidth > 0) xr.min = xr.max - viewWidth;
        xr.ensureArea(1.0);
        xAxis->range = xr;
    }
    if (autoScaleY && yAxis && !yAxis->locked) {
        CoordinateRange yr;
        for (qsizetype i = 0; i < ys.size(); ++i) {
            if (viewWidth > 0 && xAxis && xs[i] < xAxis->range.min) continue;
            yr.expand(ys[i]);
        }
        if (yr.isValid()) {
            yr.ensureArea(1.0);
            yr.pad(yPadding);
            yAxis->range = yr;
        }
    }
}

AxisLimits DataStreamerXY::axisLimits() const {
    AxisLimits lim;
    QVector<double> xs, ys;
    collect(xs, ys);
    for (qsizetype i = 0; i < xs.size(); ++i) {
        lim.x.expand(xs[i]);
        lim.y.expand(ys[i]);
    }
    return lim;
}

void DataStreamerXY::render(RenderPack& rp) {
    QVector<double> xs, ys;
    collect(xs, ys);
    if (xs.size() < 2) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();
    // 事件可能亂序到達：X 有排好才能用包絡線（要二分搜尋）
    drawDecimated(p, t, xs, ys, 0, xs.size(), line, marker, std::is_sorted(xs.cbegin(), xs.cend()));
    p.restore();
}

QVector<LegendItem> DataStreamerXY::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem it;
    it.text = legendText;
    it.line = line;
    it.marker = marker;
    return {it};
}

// ───────────────────────── LiveSpectrum ─────────────────────────

LiveSpectrum::LiveSpectrum(int fftSize, double sampleRate)
    : m_fftSize(std::max(8, fftSize)), m_sampleRate(sampleRate > 0 ? sampleRate : 1.0) {
    resetLocked();
}

void LiveSpectrum::resetLocked() {
    m_buffer.fill(0.0, m_fftSize);
    m_count = 0;
    m_sinceUpdate = 0;
    m_frames = 0;
    m_frequencies.clear();
    m_avgPower.clear();
    m_peakDb.clear();
    m_version.fetch_add(1, std::memory_order_relaxed);
}

void LiveSpectrum::setFftSize(int n) {
    QMutexLocker lock(&m_mutex);
    m_fftSize = std::max(8, n);
    resetLocked();
}

int LiveSpectrum::fftSize() const {
    QMutexLocker lock(&m_mutex);
    return m_fftSize;
}

void LiveSpectrum::setSampleRate(double fs) {
    QMutexLocker lock(&m_mutex);
    if (fs > 0) m_sampleRate = fs;
    resetLocked();
}

double LiveSpectrum::sampleRate() const {
    QMutexLocker lock(&m_mutex);
    return m_sampleRate;
}

void LiveSpectrum::clear() {
    QMutexLocker lock(&m_mutex);
    resetLocked();
}

void LiveSpectrum::resetPeakHold() {
    QMutexLocker lock(&m_mutex);
    m_peakDb.clear();
    m_version.fetch_add(1, std::memory_order_relaxed);
}

void LiveSpectrum::addLocked(double sample) {
    m_buffer[static_cast<qsizetype>(m_count % m_fftSize)] = std::isfinite(sample) ? sample : 0.0;
    ++m_count;
    ++m_sinceUpdate;
    const int interval = (updateInterval > 0) ? updateInterval : std::max(1, m_fftSize / 2);
    // 緩衝收滿之前不算：前面補零的頻譜會有一個假的低頻隆起
    if (m_count >= m_fftSize && m_sinceUpdate >= interval) computeLocked();
}

void LiveSpectrum::add(double sample) {
    QMutexLocker lock(&m_mutex);
    addLocked(sample);
}

void LiveSpectrum::addRange(const QVector<double>& samples) {
    QMutexLocker lock(&m_mutex);
    for (double v : samples) addLocked(v);
}

void LiveSpectrum::computeLocked() {
    m_sinceUpdate = 0;
    // 由舊到新排好再做 FFT（循環緩衝的起點是下一個要寫的位置）
    QVector<double> ordered(m_fftSize);
    const qsizetype head = static_cast<qsizetype>(m_count % m_fftSize);
    for (int i = 0; i < m_fftSize; ++i) ordered[i] = m_buffer[(head + i) % m_fftSize];

    const Periodogram pg = periodogram(ordered, m_sampleRate, window, reference);
    if (m_avgPower.size() != pg.power.size()) {
        m_avgPower = pg.power;
        m_frequencies = pg.frequencies;
    } else {
        // 平均功率，不是平均 dB：dB 的平均會讓雜訊底系統性地偏低
        const double a = std::clamp(averaging, 0.0, 0.99);
        for (int k = 0; k < m_avgPower.size(); ++k) m_avgPower[k] = a * m_avgPower[k] + (1 - a) * pg.power[k];
    }

    const double ref = (reference > 0) ? reference : 1.0;
    if (m_peakDb.size() != m_avgPower.size())
        m_peakDb.fill(-std::numeric_limits<double>::infinity(), m_avgPower.size());
    for (int k = 0; k < m_avgPower.size(); ++k) {
        const double db = 20 * std::log10(std::max(std::sqrt(m_avgPower[k]), 1e-300) / ref);
        m_peakDb[k] = std::max(m_peakDb[k] - std::max(0.0, peakHoldDecayDb), db);
    }
    ++m_frames;
    m_version.fetch_add(1, std::memory_order_relaxed);
}

Periodogram LiveSpectrum::spectrum() const {
    QMutexLocker lock(&m_mutex);
    Periodogram out;
    out.frequencies = m_frequencies;
    out.power = m_avgPower;
    out.segments = static_cast<int>(std::min<qint64>(m_frames, std::numeric_limits<int>::max()));
    const double ref = (reference > 0) ? reference : 1.0;
    out.amplitude.reserve(m_avgPower.size());
    out.decibels.reserve(m_avgPower.size());
    for (double p : m_avgPower) {
        const double mag = std::sqrt(p);
        out.amplitude.append(mag);
        out.decibels.append(20 * std::log10(std::max(mag, 1e-300) / ref));
    }
    return out;
}

QVector<double> LiveSpectrum::peakHold() const {
    QMutexLocker lock(&m_mutex);
    return m_peakDb;
}

qint64 LiveSpectrum::frameCount() const {
    QMutexLocker lock(&m_mutex);
    return m_frames;
}

void LiveSpectrum::beforeLayout() {
    if (!autoAxes) return;
    const double nyquist = sampleRate() / 2;
    if (xAxis && !xAxis->locked) xAxis->range = {0, nyquist};
    if (yAxis && !yAxis->locked && maxDb > minDb) yAxis->range = {minDb, maxDb};
}

AxisLimits LiveSpectrum::axisLimits() const {
    AxisLimits lim;
    lim.x = {0, sampleRate() / 2};
    lim.y = {std::min(minDb, maxDb), std::max(minDb, maxDb)};
    return lim;
}

void LiveSpectrum::render(RenderPack& rp) {
    const Periodogram sp = spectrum();
    const QVector<double> hold = showPeakHold ? peakHold() : QVector<double>();
    const int n = static_cast<int>(sp.frequencies.size());
    if (n < 2) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 填色：曲線往下填到繪圖區底邊（dB 沒有自然的 0）
    if (fill.isVisible()) {
        QPolygonF poly;
        poly.reserve(n + 2);
        poly << QPointF(t.px(sp.frequencies.first()), rp.dataRect.bottom);
        for (int k = 0; k < n; ++k) poly << t.p(sp.frequencies[k], sp.decibels[k]);
        poly << QPointF(t.px(sp.frequencies.last()), rp.dataRect.bottom);
        p.setPen(Qt::NoPen);
        p.setBrush(fill.brush(poly.boundingRect()));
        p.drawPolygon(poly);
    }

    if (showPeakHold && hold.size() == n) drawDecimated(p, t, sp.frequencies, hold, 0, n, peakHoldLine, MarkerStyle());
    drawDecimated(p, t, sp.frequencies, sp.decibels, 0, n, line, MarkerStyle());

    if (peakLabels > 0) {
        const double spacing = (peakMinSpacingHz > 0) ? peakMinSpacingHz : sp.frequencies.last() * 0.02;
        const QVector<SpectralPeak> peaks =
            findSpectralPeaks(sp, peakLabels, peakMinProminenceDb, spacing, peakMinFrequency);
        for (const SpectralPeak& pk : peaks) {
            const QPointF at = t.p(pk.frequency, pk.decibels);
            const double lift = peakMarker.isVisible() ? peakMarker.size * 0.6 + 2 : 2;
            if (peakMarker.isVisible()) drawMarker(p, at - QPointF(0, lift), peakMarker);
            Label lb;
            lb.font = peakFont;
            lb.text = QStringLiteral("%1 Hz\n%2 dB").arg(pk.frequency, 0, 'f', 1).arg(pk.decibels, 0, 'f', 1);
            drawText(p, at - QPointF(0, lift * 2 + 2), lb, Qt::AlignHCenter | Qt::AlignBottom);
        }
    }
    p.restore();
}

QVector<LegendItem> LiveSpectrum::legendItems() const {
    QVector<LegendItem> items;
    if (!legendText.isEmpty()) {
        LegendItem it;
        it.text = legendText;
        it.line = line;
        it.fill = fill;
        items.append(it);
    }
    if (showPeakHold && !peakHoldLegendText.isEmpty()) {
        LegendItem it;
        it.text = peakHoldLegendText;
        it.line = peakHoldLine;
        items.append(it);
    }
    return items;
}

}  // namespace qtplot

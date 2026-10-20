#include "Plot.h"
#include "Translation.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QImage>
#include <QPageSize>
#include <QPdfWriter>
#include <QSvgGenerator>

namespace qtplot {

namespace {

/// 把背景影像畫進一個矩形。
void drawBackgroundImage(QPainter& p, const BackgroundImage& bg, const QRectF& target) {
    if (!bg.isValid() || target.isEmpty()) return;
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, bg.smooth);
    p.setOpacity(std::clamp(bg.opacity, 0.0, 1.0));

    QRectF dst = target;
    if (bg.keepAspectRatio) {
        const double imgAspect = static_cast<double>(bg.image.width()) / bg.image.height();
        const double boxAspect = target.width() / target.height();
        if (imgAspect > boxAspect) {
            const double h = target.width() / imgAspect;
            dst = QRectF(target.left(), target.center().y() - h / 2, target.width(), h);
        } else {
            const double w = target.height() * imgAspect;
            dst = QRectF(target.center().x() - w / 2, target.top(), w, target.height());
        }
    }
    p.drawImage(dst, bg.image);
    p.restore();
}

}  // namespace

// ───────────────────────────── PlotStyle ─────────────────────────────

PlotStyle PlotStyle::light() { return PlotStyle(); }

PlotStyle PlotStyle::dark() {
    PlotStyle s;
    s.figureBackground = QColor(27, 30, 35);
    s.dataBackground = QColor(20, 22, 26);
    s.axisColor = QColor(200, 205, 212);
    s.gridColor = QColor(255, 255, 255, 36);
    s.textColor = QColor(222, 226, 232);
    s.palette = Palette(Palette::Nord);
    return s;
}

PlotStyle PlotStyle::hardcopy() {
    PlotStyle s;
    s.dataBackground = QColor(255, 255, 255);
    s.gridColor = QColor(0, 0, 0, 25);
    s.palette = Palette(Palette::Tsitsulin);
    return s;
}

// ───────────────────────────── Plot ─────────────────────────────

Plot::Plot() {
    initDefaults();
    m_languageGeneration = languageGeneration();
}

// ───────────────────────────── 翻譯 ─────────────────────────────

void Plot::setAutoText(const void* owner, std::function<QString*()> target, const char* source,
                       const char* comment) {
    QString* t = target();
    if (!t) return;
    *t = QCoreApplication::translate("qtplot", source, comment);
    m_autoTexts.append({owner, std::move(target), source, comment, *t});
}

void Plot::forgetAutoTexts(const void* owner) {
    m_autoTexts.erase(std::remove_if(m_autoTexts.begin(), m_autoTexts.end(),
                                     [&](const AutoText& a) { return a.owner == owner; }),
                      m_autoTexts.end());
}

void Plot::retranslate() {
    m_languageGeneration = languageGeneration();
    for (auto it = m_autoTexts.begin(); it != m_autoTexts.end();) {
        QString* t = it->target();
        if (!t) {  // 魚骨圖的類別被使用者刪掉了
            it = m_autoTexts.erase(it);
            continue;
        }
        if (*t == it->applied) {
            *t = QCoreApplication::translate("qtplot", it->source, it->comment);
            it->applied = *t;
        }
        ++it;
    }
}

void Plot::initDefaults() {
    m_bottom = addAxis(Edge::Bottom);
    m_left = addAxis(Edge::Left);
    m_top = addAxis(Edge::Top);
    m_right = addAxis(Edge::Right);

    // 上／右軸預設只當作框線，不顯示刻度標籤
    m_top->showTickLabels = false;
    m_top->majorTickLength = 0;
    m_top->minorTickLength = 0;
    m_right->showTickLabels = false;
    m_right->majorTickLength = 0;
    m_right->minorTickLength = 0;

    title = Label();
    title.font.size = 14;
    title.font.bold = true;
    legend = Legend();
    legend.font.size = 10;
    legend.title.font.size = 10;
    legend.title.font.bold = true;
    grid = Grid();
    style = PlotStyle::light();
    equalAxisScale = false;
    antiAlias = true;
    paddingLeft = 8;
    paddingRight = 12;
    paddingTop = 8;
    paddingBottom = 8;
    figureBackgroundImage = BackgroundImage();
    dataBackgroundImage = BackgroundImage();

    m_colorIndex = 0;
    m_autoScalePending = false;
    m_explicitLimits = false;
    m_lastDataRect = PixelRect();
    m_lastRenderMs = 0;
}

void Plot::reset() {
    // 順序有關係：圖元持有 xAxis / yAxis 這兩個裸指標，指向 m_axisOwner 裡的東西。
    // 先丟圖元再丟軸，中間就不會有一瞬間存在「指著已釋放的軸」的圖元。
    m_plottables.clear();
    m_axes.clear();
    m_axisOwner.clear();
    m_bottom = m_left = m_top = m_right = nullptr;
    m_autoTexts.clear();

    initDefaults();
}

Plot::~Plot() = default;

Axis* Plot::addAxis(Edge edge) {
    auto axis = std::make_unique<Axis>(edge);
    Axis* raw = axis.get();
    m_axisOwner.push_back(std::move(axis));
    m_axes.append(raw);
    return raw;
}

void Plot::attach(IPlottable* p) {
    if (!p->xAxis) p->xAxis = m_bottom;
    if (!p->yAxis) p->yAxis = m_left;
}

void Plot::remove(IPlottable* plottable) {
    for (auto it = m_plottables.begin(); it != m_plottables.end(); ++it) {
        if (it->get() == plottable) {
            // 先忘掉它的自動文字：位址之後可能被新的圖元重用，留著就會改到別人身上
            forgetAutoTexts(plottable);
            m_plottables.erase(it);
            return;
        }
    }
}

void Plot::moveToFront(IPlottable* plottable) {
    auto it = std::find_if(m_plottables.begin(), m_plottables.end(),
                           [&](const auto& p) { return p.get() == plottable; });
    if (it == m_plottables.end()) return;
    std::rotate(it, it + 1, m_plottables.end());
}

void Plot::moveToBack(IPlottable* plottable) {
    auto it = std::find_if(m_plottables.begin(), m_plottables.end(),
                           [&](const auto& p) { return p.get() == plottable; });
    if (it == m_plottables.end()) return;
    std::rotate(m_plottables.begin(), it, it + 1);
}

void Plot::clear() {
    for (const auto& p : m_plottables) forgetAutoTexts(p.get());
    m_plottables.clear();
    m_colorIndex = 0;
    m_explicitLimits = false;
    for (Axis* a : m_axes) a->clearRangeOverride();
}

QVector<IPlottable*> Plot::plottables() const {
    QVector<IPlottable*> out;
    out.reserve(static_cast<int>(m_plottables.size()));
    for (const auto& p : m_plottables) out.append(p.get());
    return out;
}

QColor Plot::nextColor() { return style.palette.color(m_colorIndex++); }

void Plot::setStyle(const PlotStyle& s) {
    style = s;
    for (Axis* a : m_axes) {
        a->frameLine.color = s.axisColor;
        a->majorTickLine.color = s.axisColor;
        a->minorTickLine.color = QColor(s.axisColor.red(), s.axisColor.green(),
                                       s.axisColor.blue(), 150);
        a->tickFont.color = s.textColor;
        a->label.font.color = s.textColor;
    }
    grid.majorLine.color = s.gridColor;
    title.font.color = s.textColor;
    legend.font.color = s.textColor;
    legend.title.font.color = s.textColor;
    // 深色主題的圖例底色要跟著換，不然白底會在深色圖上刺眼
    const bool darkBg = s.figureBackground.lightness() < 128;
    legend.background = FillStyle(darkBg ? QColor(40, 44, 52, 235) : QColor(255, 255, 255, 235));
    legend.border.color = darkBg ? QColor(255, 255, 255, 60) : QColor(0, 0, 0, 70);

    // 字型放在最後：前面只動顏色，這裡才動家族與字級，
    // 而且兩個都預設「不動」，所以既有的主題不受影響。
    if (!s.fontFamily.isEmpty()) setFontFamily(s.fontFamily);
    if (s.fontSize > 0) scaleFontSizes(s.fontSize / 11.0);
}

// ───────────────────────────── 文字 ─────────────────────────────

void Plot::visitFonts(const FontVisitor& visit) {
    visit(title.font);
    legend.visitFonts(visit);
    for (Axis* a : m_axes) a->visitFonts(visit);
    for (const auto& p : m_plottables) p->visitFonts(visit);
}

void Plot::setFontFamily(const QString& family) {
    visitFonts([&](FontStyle& f) { f.family = family; });
}

void Plot::scaleFontSizes(double factor) {
    if (!(factor > 0)) return;
    visitFonts([&](FontStyle& f) { f.size *= factor; });
}

void Plot::setFontSize(double points) {
    if (!(points > 0)) return;
    visitFonts([&](FontStyle& f) { f.size = points; });
}

void Plot::setFontWeight(int weight) {
    visitFonts([&](FontStyle& f) { f.weight = weight; });
}

void Plot::setFontColor(QColor color) {
    visitFonts([&](FontStyle& f) {
        // 保留原本的透明度：次要說明文字是靠調淡來分層的，
        // 一律設成不透明會讓那些字和主要文字一樣搶眼。
        const int alpha = f.color.alpha();
        QColor c = color;
        if (color.alpha() == 255) c.setAlpha(alpha);
        f.color = c;
    });
}

void Plot::setFontRotation(double degrees) {
    visitFonts([&](FontStyle& f) { f.rotation = degrees; });
}

// ───────────────────────────── 範圍 ─────────────────────────────

void Plot::setAxisLimits(double xMin, double xMax, double yMin, double yMax) {
    m_bottom->range = {xMin, xMax};
    m_top->range = {xMin, xMax};
    m_left->range = {yMin, yMax};
    m_right->range = {yMin, yMax};
    for (Axis* a : {m_bottom, m_top, m_left, m_right}) a->rangeWasSet = true;
    m_autoScalePending = false;
    m_explicitLimits = true;
}

void Plot::setAxisLimits(const AxisLimits& lim) {
    if (lim.x.isValid() && lim.y.isValid())
        setAxisLimits(lim.x.min, lim.x.max, lim.y.min, lim.y.max);
}

AxisLimits Plot::axisLimits() const { return AxisLimits(m_bottom->range, m_left->range); }

bool Plot::hasExplicitAxisLimits() const {
    if (m_explicitLimits) return true;
    // 直接呼叫 axis->setRange() 的也算，不然 autoScale 還是會蓋掉
    for (const Axis* a : m_axes)
        if (a->rangeWasSet) return true;
    return false;
}

AxisLimits Plot::dataLimits() const {
    AxisLimits lim;
    for (const auto& p : m_plottables) {
        if (!p->isVisible) continue;
        const AxisLimits l = p->axisLimits();
        // 單軸圖元（參考線）只回報一軸，所以兩軸分別累積，不用 hasData() 過濾
        lim.x.expand(l.x);
        lim.y.expand(l.y);
    }
    return lim;
}

CoordinateRange Plot::dataLimits(const Axis* axis) const {
    CoordinateRange r;
    if (!axis) return r;
    for (const auto& p : m_plottables) {
        if (!p->isVisible) continue;
        const AxisLimits l = p->axisLimits();
        // 同一個圖元可能同時綁在這根軸的 X 與 Y 上（理論上不會，但不要假設）
        if (p->xAxis == axis) r.expand(l.x);
        if (p->yAxis == axis) r.expand(l.y);
    }
    return r;
}

void Plot::autoScale(double fraction, bool force) {
    // 必須「逐軸」收集：把所有圖元的範圍混在一起算，多軸圖就毀了 ——
    // 成交量綁在右軸，它的 0..2e6 卻會把左邊的價格軸也撐到 2e6，
    // K 線被壓成一條線。只看綁在這根軸上的圖元才正確。
    auto applyTo = [&](Axis* axis, CoordinateRange r) {
        // locked：使用者不想讓它動。rangeWasSet：使用者已經指定過視野。
        if (axis->locked || (!force && axis->rangeWasSet)) return;
        if (!r.isValid()) return;
        r.ensureArea(1.0);
        if (axis->hasBreaks()) {
            // 截斷軸：邊界要照「畫得出來的長度」留。照整個範圍算的話，
            // 0–60 加 500–560 會在兩端各留 28 個單位，比左邊那一整段還寬一半。
            double kept = r.span();
            for (const AxisBreak& b : axis->breaks().breaks)
                kept -= std::max(0.0, std::min(b.to, r.max) - std::max(b.from, r.min));
            const double m = std::max(0.0, kept) * fraction;
            r = {r.min - m, r.max + m};
        } else {
            r.pad(fraction);
        }
        axis->range = r;
    };

    for (Axis* a : m_axes) {
        CoordinateRange r = dataLimits(a);
        // 上／右軸預設只是框線，沒有圖元綁在上面，跟著對面那根走
        if (!r.isValid()) {
            if (a == m_top) r = dataLimits(m_bottom);
            else if (a == m_right) r = dataLimits(m_left);
            else continue;  // 其他沒有資料的軸不動
        }
        applyTo(a, r);
    }
    m_autoScalePending = false;
}

// ───────────────────────────── 互動 ─────────────────────────────

Transform Plot::transformFor(const Axis* xAxis, const Axis* yAxis) const {
    Transform t;
    t.rect = m_lastDataRect;
    const Axis* ax = xAxis ? xAxis : m_bottom;
    const Axis* ay = yAxis ? yAxis : m_left;
    t.xr = ax->range;
    t.yr = ay->range;
    t.xInverted = ax->inverted;
    t.yInverted = ay->inverted;
    t.xBreaks = ax->hasBreaks() ? &ax->breaks() : nullptr;
    t.yBreaks = ay->hasBreaks() ? &ay->breaks() : nullptr;
    return t;
}

HitResult Plot::hitTest(Coordinates target, double maxDistancePx, HitMode mode) const {
    HitResult best;
    if (m_lastDataRect.isEmpty()) return best;
    for (const auto& p : m_plottables) {
        if (!p->isVisible) continue;
        const Transform t = transformFor(p->xAxis, p->yAxis);
        HitResult r = p->hitTest(target, t, mode);
        if (!r.isHit()) continue;
        if (!best.isHit() || r.distancePx < best.distancePx) best = r;
    }
    if (maxDistancePx > 0 && best.isHit() && best.distancePx > maxDistancePx) return HitResult();
    return best;
}

IPlottable* Plot::grabbedPlottable(Coordinates at, double tolerancePx) const {
    if (m_lastDataRect.isEmpty()) return nullptr;
    // 反向找：後加入的畫在上面，視覺上在最上層的應該優先抓到
    for (auto it = m_plottables.rbegin(); it != m_plottables.rend(); ++it) {
        IPlottable* p = it->get();
        if (!p->isVisible || !p->isDraggable()) continue;
        if (p->isGrabbed(at, transformFor(p->xAxis, p->yAxis), tolerancePx)) return p;
    }
    return nullptr;
}

// ───────────────────────── 圖元捷徑 ─────────────────────────

namespace {
QColor orNext(QColor c, Plot& plot) { return c.isValid() ? c : plot.nextColor(); }

/// 取同一個顏色但換透明度。
QColor withTextAlpha(QColor c, int alpha) {
    c.setAlpha(alpha);
    return c;
}

/// 把一根軸的刻度換成類別名稱（0, 1, 2, ... 對應 labels）。
void setCategoryTicks(Axis* axis, const QStringList& labels) {
    auto manual = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < labels.size(); ++i)
        manual->ticks.append({static_cast<double>(i), labels.at(i), true});
    axis->tickGenerator = manual;
}
}  // namespace

Scatter* Plot::addScatter(QVector<double> xs, QVector<double> ys, QColor color) {
    const QColor c = orNext(color, *this);
    auto* s = emplace<Scatter>();
    s->xs = std::move(xs);
    s->ys = std::move(ys);
    s->line = LineStyle(c, 1.5);
    s->marker = MarkerStyle(MarkerShape::FilledCircle, 6, c);
    return s;
}

Scatter* Plot::addLine(double x1, double y1, double x2, double y2, QColor color) {
    Scatter* s = addScatter({x1, x2}, {y1, y2}, color);
    s->marker = MarkerStyle::none();
    return s;
}

Markers* Plot::addMarkers(QVector<double> xs, QVector<double> ys, QColor color) {
    const QColor c = orNext(color, *this);
    auto* m = emplace<Markers>();
    m->xs = std::move(xs);
    m->ys = std::move(ys);
    m->marker = MarkerStyle(MarkerShape::FilledCircle, 7, c);
    return m;
}

Signal* Plot::addSignal(QVector<double> ys, double period, QColor color) {
    const QColor c = orNext(color, *this);
    auto* s = emplace<Signal>();
    s->ys = std::move(ys);
    s->period = period;
    s->line = LineStyle(c, 1.5);
    return s;
}

SignalXY* Plot::addSignalXY(QVector<double> xs, QVector<double> ys, QColor color) {
    const QColor c = orNext(color, *this);
    auto* s = emplace<SignalXY>();
    s->xs = std::move(xs);
    s->ys = std::move(ys);
    s->line = LineStyle(c, 1.5);
    return s;
}

FunctionPlot* Plot::addFunction(std::function<double(double)> f, QColor color) {
    const QColor c = orNext(color, *this);
    auto* p = emplace<FunctionPlot>();
    p->function = std::move(f);
    p->line = LineStyle(c, 1.5);
    return p;
}

QVector<FillY*> Plot::addStackedArea(const QVector<double>& xs,
                                     const QVector<QVector<double>>& series,
                                     const QVector<QColor>& colors) {
    QVector<FillY*> out;
    const QVector<StackedBand> bands = stackSigned(series);
    for (int k = 0; k < bands.size(); ++k) {
        const QColor c = (k < colors.size() && colors[k].isValid()) ? colors[k]
                                                                   : style.palette.color(k);
        auto* f = emplace<FillY>();
        // x 可能比資料長，截到一樣長 —— 不截的話尾端會接到未定義的值
        f->xs = xs.mid(0, bands[k].lower.size());
        f->ys1 = bands[k].lower;
        f->ys2 = bands[k].upper;
        f->fill = FillStyle(c);
        f->line = LineStyle(c.darker(115), 1.0);
        out.append(f);
    }
    return out;
}

Horizon* Plot::addHorizon(QVector<double> xs, QVector<double> ys, int bands) {
    auto* h = emplace<Horizon>();
    h->xs = std::move(xs);
    h->ys = std::move(ys);
    h->bands = bands;
    return h;
}

Dendrogram* Plot::addDendrogram(QVector<ClusterMerge> merges, int leafCount,
                                QStringList leafLabels) {
    auto* d = emplace<Dendrogram>();
    d->merges = std::move(merges);
    d->leafCount = leafCount;
    d->leafLabels = std::move(leafLabels);
    d->palette = style.palette;
    d->line.color = style.axisColor;
    d->labelFont.color = style.textColor;
    return d;
}

QVector<FillY*> Plot::addStream(const QVector<double>& xs,
                                const QVector<QVector<double>>& series, StackOffset offset,
                                const QVector<QColor>& colors) {
    QVector<FillY*> out;
    const QVector<StackedBand> bands = stackStream(series, offset);
    for (int k = 0; k < bands.size(); ++k) {
        const QColor c = (k < colors.size() && colors[k].isValid()) ? colors[k]
                                                                   : style.palette.color(k);
        auto* f = emplace<FillY>();
        f->xs = xs.mid(0, bands[k].lower.size());
        f->ys1 = bands[k].lower;
        f->ys2 = bands[k].upper;
        f->fill = FillStyle(c);
        // 河流圖通常不畫邊線：層一多的話邊線會把顏色切碎
        f->line = LineStyle::none();
        out.append(f);
    }
    return out;
}

FillY* Plot::addFillY(QVector<double> xs, QVector<double> ys1, QVector<double> ys2, QColor color) {
    const QColor c = orNext(color, *this);
    auto* f = emplace<FillY>();
    f->xs = std::move(xs);
    f->ys1 = std::move(ys1);
    f->ys2 = std::move(ys2);
    f->fill = FillStyle(QColor(c.red(), c.green(), c.blue(), 80));
    f->line = LineStyle(QColor(c.red(), c.green(), c.blue(), 170), 1.0);
    return f;
}

BarPlot* Plot::addBars(QVector<double> values, QColor color) {
    const QColor c = orNext(color, *this);
    auto* b = emplace<BarPlot>();
    b->bars = BarPlot::fromValues(values, c);
    return b;
}

BarPlot* Plot::addBars(QVector<Bar> bars) {
    auto* b = emplace<BarPlot>();
    b->bars = std::move(bars);
    return b;
}

BarPlot* Plot::addHistogram(const QVector<double>& values, int binCount, QColor color) {
    const QColor c = orNext(color, *this);
    const Histogram h = Histogram::fromValues(values, binCount);
    auto* b = emplace<BarPlot>();
    b->bars = h.toBars(c);
    b->borderLine = LineStyle(QColor(255, 255, 255, 200), 1.0);
    return b;
}

ErrorBar* Plot::addErrorBar(QVector<double> xs, QVector<double> ys, QVector<double> yErr) {
    auto* e = emplace<ErrorBar>();
    e->xs = std::move(xs);
    e->ys = std::move(ys);
    e->yErrorsNegative = yErr;
    e->yErrorsPositive = std::move(yErr);
    return e;
}

Heatmap* Plot::addHeatmap(QVector<double> values, int rows, int cols) {
    auto* h = emplace<Heatmap>();
    h->values = std::move(values);
    h->rows = rows;
    h->cols = cols;
    return h;
}

AxisLine* Plot::addHorizontalLine(double y, QColor color) {
    auto* l = emplace<AxisLine>();
    l->vertical = false;
    l->position = y;
    l->line = LineStyle(orNext(color, *this), 1.5);
    return l;
}

AxisLine* Plot::addVerticalLine(double x, QColor color) {
    auto* l = emplace<AxisLine>();
    l->vertical = true;
    l->position = x;
    l->line = LineStyle(orNext(color, *this), 1.5);
    return l;
}

AxisSpan* Plot::addHorizontalSpan(double y1, double y2, QColor color) {
    const QColor c = orNext(color, *this);
    auto* s = emplace<AxisSpan>();
    s->vertical = false;
    s->start = y1;
    s->end = y2;
    s->fill = FillStyle(QColor(c.red(), c.green(), c.blue(), 60));
    return s;
}

AxisSpan* Plot::addVerticalSpan(double x1, double x2, QColor color) {
    const QColor c = orNext(color, *this);
    auto* s = emplace<AxisSpan>();
    s->vertical = true;
    s->start = x1;
    s->end = x2;
    s->fill = FillStyle(QColor(c.red(), c.green(), c.blue(), 60));
    return s;
}

Crosshair* Plot::addCrosshair(double x, double y) {
    auto* c = emplace<Crosshair>();
    c->position = {x, y};
    return c;
}

Text* Plot::addText(const QString& text, double x, double y) {
    auto* t = emplace<Text>();
    t->label.text = text;
    t->label.font.color = style.textColor;
    t->position = {x, y};
    return t;
}

Annotation* Plot::addAnnotation(const QString& text, Qt::Alignment corner) {
    auto* a = emplace<Annotation>();
    a->label.text = text;
    a->label.font.color = style.textColor;
    a->corner = corner;
    return a;
}

Arrow* Plot::addArrow(double tipX, double tipY, double baseX, double baseY, QColor color) {
    auto* a = emplace<Arrow>();
    a->tip = {tipX, tipY};
    a->base = {baseX, baseY};
    a->line = LineStyle(orNext(color, *this), 2.0);
    return a;
}

Shape* Plot::addRectangle(double left, double right, double bottom, double top) {
    const QColor c = nextColor();
    auto* s = emplace<Shape>();
    s->kind = Shape::Kind::Rectangle;
    s->rect = CoordinateRect(left, right, bottom, top);
    s->fill = FillStyle(QColor(c.red(), c.green(), c.blue(), 80));
    s->line = LineStyle(c, 1.5);
    return s;
}

Shape* Plot::addEllipse(double centerX, double centerY, double radiusX, double radiusY) {
    Shape* s = addRectangle(centerX - radiusX, centerX + radiusX,
                            centerY - radiusY, centerY + radiusY);
    s->kind = Shape::Kind::Ellipse;
    return s;
}

Bars3D* Plot::addBars3D(const QVector<double>& values) {
    auto* b = emplace<Bars3D>();
    b->bars = Bars3D::fromValues(values, style.palette);
    b->labelFont.color = style.textColor;
    return b;
}

Funnel* Plot::addFunnel(const QVector<double>& values, const QStringList& labels) {
    auto* f = emplace<Funnel>();
    f->stages = Funnel::fromValues(values, labels, style.palette);
    f->labelFont.color = Qt::white;
    f->labelFont.bold = true;
    f->border.color = style.figureBackground;
    return f;
}

Waterfall* Plot::addWaterfall(QVector<WaterfallStep> steps) {
    auto* w = emplace<Waterfall>();
    w->steps = std::move(steps);
    w->valueFont.color = style.textColor;
    return w;
}

Bullet* Plot::addBullet(QVector<BulletRow> rows) {
    auto* b = emplace<Bullet>();
    b->rows = std::move(rows);
    const bool dark = style.figureBackground.lightness() < 128;
    if (dark) {
        b->rangeColor = QColor(90, 90, 90);
        b->targetLine.color = QColor(235, 235, 235);
        for (BulletRow& r : b->rows) r.valueColor = QColor(230, 230, 230);
    }
    return b;
}

Fishbone* Plot::addFishbone(const QString& effect, QVector<FishboneCategory> categories) {
    auto* f = emplace<Fishbone>();
    f->effect = effect;
    f->categories = std::move(categories);
    // 類別名剛好是（目前語言的）6M 的話，記下來給語言切換時重翻。
    // 鍵和 Fishbone::sixM() 用的是同一組，翻譯檔裡只有一份。
    static const struct { const char* source; const char* comment; } k6M[] = {
        QT_TRANSLATE_NOOP3("qtplot", "人", "fishbone 6M: Man"),
        QT_TRANSLATE_NOOP3("qtplot", "機", "fishbone 6M: Machine"),
        QT_TRANSLATE_NOOP3("qtplot", "料", "fishbone 6M: Material"),
        QT_TRANSLATE_NOOP3("qtplot", "法", "fishbone 6M: Method"),
        QT_TRANSLATE_NOOP3("qtplot", "環", "fishbone 6M: Environment"),
        QT_TRANSLATE_NOOP3("qtplot", "測", "fishbone 6M: Measurement"),
    };
    for (int i = 0; i < f->categories.size(); ++i) {
        for (const auto& k : k6M) {
            if (f->categories[i].name != QCoreApplication::translate("qtplot", k.source, k.comment)) continue;
            auto target = [f, i]() -> QString* { return i < f->categories.size() ? &f->categories[i].name : nullptr; };
            setAutoText(f, target, k.source, k.comment);
            break;
        }
    }
    f->palette = style.palette;
    f->spine.color = style.textColor;
    f->boneLine.color = style.textColor;
    f->headBorder.color = style.textColor;
    f->effectFont.color = style.textColor;
    f->effectFont.size = 12;
    f->effectFont.bold = true;
    f->categoryFont.color = style.textColor;
    f->categoryFont.bold = true;
    f->causeFont.color = withTextAlpha(style.textColor, 215);
    f->causeFont.size = 9;
    f->subCauseFont.color = withTextAlpha(style.textColor, 165);
    f->subCauseFont.size = 8;

    // 結構圖，沒有刻度可讀
    for (Axis* a : {bottomAxis(), leftAxis(), topAxis(), rightAxis()}) a->isVisible = false;
    grid.isVisible = false;
    return f;
}

ConfusionMatrix* Plot::addConfusionMatrix(const QStringList& labels, QVector<QVector<double>> counts) {
    auto* cm = emplace<ConfusionMatrix>();
    cm->labels = labels;
    cm->counts = std::move(counts);
    const int n = cm->size();

    // 類別名放在刻度上：底軸是預測、左軸是實際（第 0 列在最上面，所以左軸倒著排）
    auto bottom = std::make_shared<ManualTickGenerator>();
    auto left = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < n; ++i) {
        const QString name = i < labels.size() ? labels[i] : QString::number(i);
        bottom->ticks.append({static_cast<double>(i), name});
        left->ticks.append({static_cast<double>(n - 1 - i), name});
    }
    Axis* bx = bottomAxis();
    Axis* ly = leftAxis();
    bx->tickGenerator = bottom;
    ly->tickGenerator = left;
    setAutoText(bx, [bx] { return &bx->label.text; }, QT_TRANSLATE_NOOP("qtplot", "預測"));
    setAutoText(ly, [ly] { return &ly->label.text; }, QT_TRANSLATE_NOOP("qtplot", "實際"));
    grid.isVisible = false;
    equalAxisScale = true;
    if (n > 0) setAxisLimits(-0.5, n - 0.5, -0.5, n - 0.5);
    return cm;
}

Pareto* Plot::addPareto(QVector<ParetoItem> items) {
    auto* pr = emplace<Pareto>();
    pr->items = std::move(items);
    pr->palette = style.palette;
    pr->valueFont.color = style.textColor;
    pr->valueFont.size = 9;

    // 累積折線要一根自己的百分比軸。和左軸共用一根的話，
    // 「80%」會被當成數量 80 畫上去 —— 兩條線的尺根本不同。
    Axis* right = addAxis(Edge::Right);
    right->setRange(0, 105);
    setAutoText(right, [right] { return &right->label.text; }, QT_TRANSLATE_NOOP("qtplot", "累積百分比"));
    right->label.font.color = pr->cumulativeLine.color;
    right->tickFont.color = pr->cumulativeLine.color;
    pr->cumulativeAxis = right;

    QStringList labels;
    for (const ParetoItem& it : pr->sortedItems()) labels << it.label;
    if (!labels.isEmpty()) setCategoryTicks(bottomAxis(), labels);
    return pr;
}

Waffle* Plot::addWaffle(QVector<WaffleCategory> categories, int rows, int columns) {
    auto* w = emplace<Waffle>();
    w->categories = std::move(categories);
    w->rows = rows;
    w->columns = columns;
    w->palette = style.palette;
    w->extent = CoordinateRect(0, columns, 0, rows);

    for (Axis* a : {bottomAxis(), leftAxis(), topAxis(), rightAxis()}) a->isVisible = false;
    grid.isVisible = false;
    equalAxisScale = true;
    return w;
}

Slope* Plot::addSlope(QStringList periodLabels, QVector<SlopeSeries> series) {
    auto* s = emplace<Slope>();
    s->periodLabels = periodLabels;
    s->series = std::move(series);
    s->palette = style.palette;
    s->labelFont.color = style.textColor;
    s->labelFont.size = 9;
    s->line.color = withTextAlpha(style.axisColor, 160);
    if (!periodLabels.isEmpty()) setCategoryTicks(bottomAxis(), periodLabels);
    grid.isVisible = false;
    return s;
}

Surface3D* Plot::addSurface3D(QVector<double> values, int rows, int cols) {
    auto* s = emplace<Surface3D>();
    s->values = std::move(values);
    s->rows = rows;
    s->cols = cols;
    // 投影出來的座標沒有「刻度」的意義 —— 留著只會讓人以為讀得出數值
    for (Axis* a : {bottomAxis(), leftAxis(), topAxis(), rightAxis()}) a->isVisible = false;
    grid.isVisible = false;
    equalAxisScale = true;
    return s;
}

RadialBarChart* Plot::addRadialBarChart(QVector<double> values, QStringList labels) {
    auto* c = emplace<RadialBarChart>();
    c->values = std::move(values);
    c->labels = std::move(labels);
    c->palette = style.palette;
    c->labelFont.color = style.textColor;
    c->labelFont.size = 9.5;
    c->centerLabel.font.color = style.textColor;
    c->centerLabel.font.bold = true;
    for (Axis* a : {bottomAxis(), leftAxis(), topAxis(), rightAxis()}) a->isVisible = false;
    grid.isVisible = false;
    equalAxisScale = true;
    return c;
}

CircularHeatmap* Plot::addCircularHeatmap(int rings, int sectors, QVector<QColor> cells, QStringList sectorLabels) {
    auto* h = emplace<CircularHeatmap>();
    h->rings = rings;
    h->sectors = sectors;
    h->cells = std::move(cells);
    h->sectorLabels = std::move(sectorLabels);
    h->sectorLabelFont.color = withTextAlpha(style.textColor, 140);
    h->sectorLabelFont.size = 7.5;
    h->groupLabelFont.color = style.textColor;
    h->groupLabelFont.size = 9;
    h->groupLabelFont.bold = true;
    h->centerLabel.font.color = style.textColor;
    h->centerLabel.font.size = 11;
    h->cellBorder.color = style.figureBackground;
    for (Axis* a : {bottomAxis(), leftAxis(), topAxis(), rightAxis()}) a->isVisible = false;
    grid.isVisible = false;
    equalAxisScale = true;
    return h;
}

LabeledLine* Plot::addLabeledLine(Coordinates start, Coordinates end, const QString& text, QColor color) {
    auto* l = emplace<LabeledLine>();
    l->start = start;
    l->end = end;
    l->text = text;
    l->line.color = orNext(color, *this);
    l->font.size = 9;
    return l;
}

ColorGrid* Plot::addColorGrid(int rows, int cols, QVector<QColor> colors, CoordinateRect extent) {
    auto* g = emplace<ColorGrid>();
    g->rows = rows;
    g->cols = cols;
    g->colors = std::move(colors);
    g->extent = extent;
    return g;
}

Chart3D* Plot::addChart3D() {
    auto* c = emplace<Chart3D>();
    c->tickFont.color = withTextAlpha(style.textColor, 190);
    c->axisLabelFont.color = style.textColor;
    for (Axis* a : {bottomAxis(), leftAxis(), topAxis(), rightAxis()}) a->isVisible = false;
    grid.isVisible = false;
    return c;
}

Ridgeline* Plot::addRidgeline(QVector<RidgeGroup> groups) {
    auto* r = emplace<Ridgeline>();
    r->groups = std::move(groups);
    r->palette = style.palette;
    r->baseline.color = withTextAlpha(style.axisColor, 70);
    setCategoryTicks(leftAxis(), r->orderedLabels());
    grid.isVisible = false;
    return r;
}

CalendarHeatmap* Plot::addCalendarHeatmap(QVector<CalendarValue> values) {
    auto* c = emplace<CalendarHeatmap>();
    c->values = std::move(values);
    c->labelFont.color = withTextAlpha(style.textColor, 190);
    c->labelFont.size = 8.5;
    const int weeks = std::max(1, c->weekCount());
    c->extent = CoordinateRect(0, weeks, 0, 7);

    for (Axis* a : {bottomAxis(), leftAxis(), topAxis(), rightAxis()}) a->isVisible = false;
    grid.isVisible = false;
    equalAxisScale = true;
    return c;
}

Sankey* Plot::addSankey(QVector<SankeyNode> nodes, QVector<SankeyLink> links) {
    auto* s = emplace<Sankey>();
    s->nodes = std::move(nodes);
    s->links = std::move(links);
    for (int i = 0; i < s->nodes.size(); ++i)
        if (!s->nodes[i].color.isValid()) s->nodes[i].color = style.palette.color(i);
    s->labelFont.color = style.textColor;
    return s;
}

NetworkGraph* Plot::addNetwork(QVector<GraphNode> nodes, QVector<GraphEdge> edges) {
    auto* g = emplace<NetworkGraph>();
    g->nodes = std::move(nodes);
    g->edges = std::move(edges);
    for (int i = 0; i < g->nodes.size(); ++i)
        if (!g->nodes[i].color.isValid()) g->nodes[i].color = style.palette.color(i);
    g->labelFont.color = style.textColor;
    g->nodeBorder.color = style.figureBackground;
    return g;
}

IsoBars* Plot::addIsoBars(const QVector<double>& values) {
    auto* b = emplace<IsoBars>();
    b->bars = IsoBars::fromValues(values, style.palette);
    b->labelFont.color = style.textColor;
    b->groundLine.color = withTextAlpha(style.axisColor, 45);
    return b;
}

IsoStack* Plot::addIsoStack(QVector<IsoLayer> layers) {
    auto* s = emplace<IsoStack>();
    s->layers = std::move(layers);
    for (int i = 0; i < s->layers.size(); ++i)
        if (!s->layers[i].color.isValid()) s->layers[i].color = style.palette.color(i);
    s->labelFont.color = style.textColor;
    return s;
}

Mosaic* Plot::addMosaic(QVector<MosaicColumn> columns) {
    auto* m = emplace<Mosaic>();
    m->columns = std::move(columns);
    m->labelFont.color = Qt::white;
    m->columnLabelFont.color = style.textColor;
    m->border.color = style.figureBackground;
    return m;
}

Chord* Plot::addChord(const QStringList& labels, QVector<QVector<double>> matrix) {
    usePolarLayout();
    auto* c = emplace<Chord>();
    c->labels = labels;
    c->matrix = std::move(matrix);
    for (int i = 0; i < labels.size(); ++i) c->colors.append(style.palette.color(i));
    c->labelFont.color = style.textColor;
    c->arcBorder.color = style.figureBackground;
    return c;
}

BubbleChart* Plot::addBubbles(const QVector<double>& xs, const QVector<double>& ys,
                              const QVector<double>& sizes) {
    auto* b = emplace<BubbleChart>();
    b->bubbles = BubbleChart::fromArrays(xs, ys, sizes, style.palette);
    b->labelFont.color = style.textColor;
    b->border.color = withTextAlpha(style.axisColor, 120);
    return b;
}

Gantt* Plot::addGantt(QVector<GanttTask> tasks) {
    auto* g = emplace<Gantt>();
    g->tasks = std::move(tasks);
    for (int i = 0; i < g->tasks.size(); ++i)
        if (!g->tasks[i].fillColor.isValid()) g->tasks[i].fillColor = style.palette.color(i);
    g->labelFont.color = Qt::white;
    g->borderLine.color = withTextAlpha(style.axisColor, 90);
    g->dependencyLine.color = withTextAlpha(style.axisColor, 200);
    return g;
}

Venn* Plot::addVenn(const QStringList& labels) {
    auto* v = emplace<Venn>();
    for (int i = 0; i < labels.size(); ++i) v->sets.append({labels[i], style.palette.color(i)});
    v->setLabelFont.color = style.textColor;
    v->setLabelFont.bold = true;
    v->countLabelFont.color = style.textColor;
    return v;
}

ParallelCoordinates* Plot::addParallelCoordinates(const QStringList& axisLabels,
                                                  QVector<QVector<double>> rows) {
    auto* pc = emplace<ParallelCoordinates>();
    for (const QString& l : axisLabels) {
        ParallelAxisSpec spec;
        spec.label = l;
        pc->axes.append(spec);
    }
    pc->rows = std::move(rows);
    pc->axisLabelFont.color = style.textColor;
    pc->axisLabelFont.bold = true;
    pc->tickFont.color = withTextAlpha(style.textColor, 170);
    pc->tickFont.size = 8;
    pc->axisLine.color = withTextAlpha(style.axisColor, 200);
    return pc;
}

Hexbin* Plot::addHexbin(QVector<double> xs, QVector<double> ys, int columns) {
    auto* h = emplace<Hexbin>();
    h->xs = std::move(xs);
    h->ys = std::move(ys);
    h->columns = columns;
    return h;
}

Beeswarm* Plot::addBeeswarm(const QVector<QVector<double>>& groups, QStringList labels) {
    auto* b = emplace<Beeswarm>();
    for (int i = 0; i < groups.size(); ++i) {
        SwarmGroup g;
        g.values = groups[i];
        if (i < labels.size()) g.label = labels[i];
        g.fillColor = style.palette.color(i);
        b->groups.append(g);
    }
    if (!labels.isEmpty()) setCategoryTicks(bottomAxis(), labels);
    return b;
}

Dumbbell* Plot::addDumbbell(QVector<DumbbellRow> rows) {
    auto* d = emplace<Dumbbell>();
    d->rows = std::move(rows);
    d->valueFont.color = style.textColor;
    d->deltaFont.color = withTextAlpha(style.textColor, 190);
    d->connector.color = withTextAlpha(style.axisColor, 110);
    return d;
}

Treemap* Plot::addTreemap(QVector<TreemapNode> roots) {
    auto* t = emplace<Treemap>();
    t->roots = std::move(roots);
    t->palette = style.palette;
    t->border.color = style.figureBackground;
    t->labelFont.color = Qt::white;
    t->groupLabelFont.color = style.textColor;
    t->groupLabelFont.bold = true;
    return t;
}

Sunburst* Plot::addSunburst(QVector<SunburstNode> roots) {
    usePolarLayout();
    auto* s = emplace<Sunburst>();
    s->roots = std::move(roots);
    s->palette = style.palette;
    s->border.color = style.figureBackground;
    s->labelFont.color = Qt::white;
    return s;
}

Sector* Plot::addSector(double centerX, double centerY, double radiusX, double startAngle,
                        double sweepAngle, double radiusY) {
    const QColor c = nextColor();
    auto* s = emplace<Sector>();
    s->center = {centerX, centerY};
    s->radiusX = radiusX;
    s->radiusY = (radiusY > 0) ? radiusY : radiusX;
    s->startAngle = startAngle;
    s->sweepAngle = sweepAngle;
    s->fill = FillStyle(QColor(c.red(), c.green(), c.blue(), 80));
    s->line.color = c;
    return s;
}

Arc* Plot::addArc(double centerX, double centerY, double radiusX, double startAngle,
                  double sweepAngle, double radiusY) {
    auto* a = emplace<Arc>();
    a->center = {centerX, centerY};
    a->radiusX = radiusX;
    a->radiusY = (radiusY > 0) ? radiusY : radiusX;
    a->startAngle = startAngle;
    a->sweepAngle = sweepAngle;
    a->line.color = nextColor();
    return a;
}

Shape* Plot::addPolygon(QVector<Coordinates> points) {
    const QColor c = nextColor();
    auto* s = emplace<Shape>();
    s->kind = Shape::Kind::Polygon;
    s->points = std::move(points);
    s->fill = FillStyle(QColor(c.red(), c.green(), c.blue(), 80));
    s->line = LineStyle(c, 1.5);
    return s;
}

// ───────────────────────────── 標註 ─────────────────────────────

Tooltip* Plot::addTooltip(const QString& text, double tipX, double tipY, double labelX,
                          double labelY) {
    auto* t = emplace<Tooltip>();
    t->label.text = text;
    t->label.font.color = style.textColor;
    t->tip = {tipX, tipY};
    t->labelPosition = {labelX, labelY};
    const bool dark = style.figureBackground.lightness() < 128;
    t->background = FillStyle(dark ? QColor(40, 44, 52, 235) : QColor(255, 255, 255, 235));
    t->border.color = withTextAlpha(style.axisColor, 160);
    return t;
}

Callout* Plot::addCallout(const QString& text, double targetX, double targetY, double textX,
                          double textY) {
    auto* c = emplace<Callout>();
    c->label.text = text;
    c->label.font.color = style.textColor;
    c->target = {targetX, targetY};
    c->textPosition = {textX, textY};
    const bool dark = style.figureBackground.lightness() < 128;
    c->background = FillStyle(dark ? QColor(40, 44, 52, 235) : QColor(255, 255, 255, 235));
    c->border.color = withTextAlpha(style.axisColor, 120);
    c->leader.color = withTextAlpha(style.axisColor, 160);
    return c;
}

Bracket* Plot::addBracket(const QString& text, double x1, double y1, double x2, double y2) {
    auto* b = emplace<Bracket>();
    b->label.text = text;
    b->label.font.color = style.textColor;
    b->start = {x1, y1};
    b->end = {x2, y2};
    b->line.color = style.axisColor;
    return b;
}

ScaleBar* Plot::addScaleBar(double width, double height) {
    auto* s = emplace<ScaleBar>();
    s->width = width;
    s->height = height;
    s->line.color = style.axisColor;
    s->font.color = style.textColor;
    return s;
}

ImageRect* Plot::addImage(const QImage& image, double left, double right, double bottom,
                          double top) {
    auto* i = emplace<ImageRect>();
    i->image = image;
    i->rect = CoordinateRect(left, right, bottom, top);
    return i;
}

// ───────────────────────── 特殊座標 ─────────────────────────

TIN* Plot::addTIN(QVector<Coordinates> points, QVector<double> zs) {
    auto* t = emplace<TIN>();
    t->points = std::move(points);
    t->zs = std::move(zs);
    return t;
}

TriangularAxis* Plot::addTriangularAxis(QStringList cornerLabels) {
    usePolarLayout();  // 等比例、不畫直角軸與格線 —— 三角圖也需要這些
    auto* a = emplace<TriangularAxis>();
    if (!cornerLabels.isEmpty()) a->cornerLabels = std::move(cornerLabels);
    a->labelFont.color = style.textColor;
    a->edgeLine.color = style.axisColor;
    a->gridLine.color = withTextAlpha(style.axisColor, 45);
    return a;
}

SmithChartAxis* Plot::addSmithChart() {
    usePolarLayout();
    auto* a = emplace<SmithChartAxis>();
    a->labelFont.color = style.textColor;
    a->outerLine.color = style.axisColor;
    a->gridLine.color = withTextAlpha(style.axisColor, 70);
    a->minorGridLine.color = withTextAlpha(style.axisColor, 30);
    return a;
}

PhasorPlot* Plot::addPhasors(QVector<Phasor> phasors) {
    usePolarLayout();
    auto* p = emplace<PhasorPlot>();
    p->phasors = std::move(phasors);
    for (int i = 0; i < p->phasors.size(); ++i)
        if (!p->phasors[i].color.isValid()) p->phasors[i].color = style.palette.color(i);
    p->labelFont.color = style.textColor;
    return p;
}

// ───────────────────────────── 裝飾 ─────────────────────────────

namespace {
/// 色階圖例的共同初始化。
void initColorbar(Colorbar* bar, Edge edge, const PlotStyle& style) {
    bar->edge = edge;
    bar->tickFont.color = style.textColor;
    bar->label.font.color = style.textColor;
    bar->border.color = withTextAlpha(style.axisColor, 120);
}
}  // namespace

Colorbar* Plot::addColorbar(const Heatmap& source, Edge edge) {
    auto* bar = emplace<Colorbar>();
    bar->follow(source);
    initColorbar(bar, edge, style);
    return bar;
}

Colorbar* Plot::addColorbar(const ContourLines& source, Edge edge) {
    auto* bar = emplace<Colorbar>();
    bar->follow(source);
    initColorbar(bar, edge, style);
    return bar;
}

Colorbar* Plot::addColorbar(const TIN& source, Edge edge) {
    auto* bar = emplace<Colorbar>();
    bar->follow(source);
    initColorbar(bar, edge, style);
    return bar;
}

Colorbar* Plot::addColorbar(const Hexbin& source, Edge edge) {
    auto* bar = emplace<Colorbar>();
    bar->follow(source);
    initColorbar(bar, edge, style);
    return bar;
}

Colorbar* Plot::addColorbar(const Chart3D& source, Edge edge) {
    auto* bar = emplace<Colorbar>();
    bar->follow(source);
    initColorbar(bar, edge, style);
    return bar;
}

Colorbar* Plot::addColorbar(Colormap map, CoordinateRange range, Edge edge) {
    auto* bar = emplace<Colorbar>();
    bar->colormap = map;
    bar->range = range;
    initColorbar(bar, edge, style);
    return bar;
}

ImageMarker* Plot::addImageMarkers(QVector<double> xs, QVector<double> ys, const QImage& image) {
    auto* m = emplace<ImageMarker>();
    m->xs = std::move(xs);
    m->ys = std::move(ys);
    m->image = image;
    return m;
}

FloatingAxis* Plot::addFloatingAxis(bool horizontal, double position) {
    auto* a = emplace<FloatingAxis>();
    a->horizontal = horizontal;
    a->position = position;
    a->line.color = style.axisColor;
    a->tickLine.color = style.axisColor;
    a->tickFont.color = style.textColor;
    a->label.font.color = style.textColor;
    return a;
}

LegendPanel* Plot::addLegendPanel(QVector<IPlottable*> sources, Qt::Alignment corner) {
    auto* lp = emplace<LegendPanel>();
    lp->style = legend;  // 沿用主圖例的字型與外框，比較一致
    lp->style.manualItems.clear();
    lp->style.placement = Legend::Placement::Inside;
    lp->style.alignment = corner;
    lp->style.title = Label();
    lp->style.title.font = legend.title.font;
    lp->sources = std::move(sources);
    return lp;
}

void LegendPanel::render(RenderPack& rp) {
    if (!rp.plot || !rp.painter) return;
    QVector<LegendItem> list = items;
    if (list.isEmpty())
        for (IPlottable* p : sources)
            if (p && p->isVisible) list += p->legendItems();
    rp.plot->drawLegendBox(*rp.painter, style, list, rp.dataRect, 0, nullptr);
}

// ───────────────────────── 即時資料 ─────────────────────────

DataLogger* Plot::addDataLogger(QColor color) {
    const QColor c = orNext(color, *this);
    auto* d = emplace<DataLogger>();
    d->line = LineStyle(c, 1.5);
    d->latestMarker.fillColor = c;
    d->latestMarker.outline.color = c;
    return d;
}

DataStreamer* Plot::addDataStreamer(int capacity, QColor color) {
    const QColor c = orNext(color, *this);
    auto* d = emplace<DataStreamer>(capacity);
    d->line = LineStyle(c, 1.5);
    return d;
}

DataStreamerXY* Plot::addDataStreamerXY(int capacity, QColor color) {
    const QColor c = orNext(color, *this);
    auto* d = emplace<DataStreamerXY>(capacity);
    d->line = LineStyle(c, 1.5);
    return d;
}

// ───────────────────────────── 場 ─────────────────────────────

ContourLines* Plot::addContour(QVector<double> values, int rows, int cols, int levelCount) {
    auto* c = emplace<ContourLines>();
    c->values = std::move(values);
    c->rows = rows;
    c->cols = cols;
    if (levelCount > 0) c->levelCount = levelCount;
    c->labelFont.color = style.textColor;
    c->labelBackground = style.dataBackground;
    c->line.color = QColor(style.axisColor.red(), style.axisColor.green(),
                           style.axisColor.blue(), 170);
    return c;
}

ContourLines* Plot::addContour(const std::function<double(double, double)>& f,
                               const CoordinateRect& area, int rows, int cols, int levelCount) {
    ContourLines* c = addContour(sampleField(f, area, rows, cols), rows, cols, levelCount);
    c->extent = area;
    return c;
}

TrendLine* Plot::addTrendLine(Coordinates p1, Coordinates p2, QColor color) {
    auto* tl = emplace<TrendLine>();
    tl->point1 = p1;
    tl->point2 = p2;
    const QColor c = orNext(color, *this);
    tl->line.color = c;
    tl->handleMarker = MarkerStyle(MarkerShape::FilledCircle, 9, c);
    return tl;
}

ContourLines* Plot::addContourScattered(const QVector<Coordinates>& points,
                                       const QVector<double>& values, int levelCount, int gridSize) {
    CoordinateRect area;
    CoordinateRange xr, yr;
    for (const Coordinates& c : points) {
        xr.expand(c.x);
        yr.expand(c.y);
    }
    if (!xr.isValid() || !yr.isValid()) return addContour(QVector<double>(), 0, 0, levelCount);
    area = CoordinateRect(xr.min, xr.max, yr.min, yr.max);
    gridSize = std::max(4, gridSize);
    ContourLines* c = addContour(gridScattered(points, values, area, gridSize, gridSize),
                                 gridSize, gridSize, levelCount);
    c->extent = area;
    return c;
}

VectorField* Plot::addVectorField(QVector<Coordinates> positions, QVector<Vector2> vectors) {
    auto* v = emplace<VectorField>();
    v->positions = std::move(positions);
    v->vectors = std::move(vectors);
    v->line.color = nextColor();
    return v;
}

VectorField* Plot::addVectorField(const std::function<Vector2(double, double)>& f,
                                  const CoordinateRect& area, int nx, int ny) {
    VectorField built = VectorField::fromFunction(f, area, nx, ny);
    return addVectorField(std::move(built.positions), std::move(built.vectors));
}

// ───────────────────────── 財金圖 ─────────────────────────

namespace {

/// 把底軸換成「序號 → 日期」的刻度產生器。
void useIndexedDateAxis(Axis* axis, const QVector<double>& timestamps) {
    auto gen = std::make_shared<IndexedDateTickGenerator>();
    gen->timestamps = timestamps;
    axis->tickGenerator = gen;
}

}  // namespace

Candlestick* Plot::addCandlestick(QVector<Ohlc> bars) {
    auto* c = emplace<Candlestick>();
    c->bars = std::move(bars);
    useIndexedDateAxis(bottomAxis(), c->timestamps());
    return c;
}

Candlestick* Plot::addOhlc(QVector<Ohlc> bars) {
    Candlestick* c = addCandlestick(std::move(bars));
    c->style = Candlestick::Style::Ohlc;
    return c;
}

Volume* Plot::addVolume(QVector<Ohlc> bars) {
    auto* v = emplace<Volume>();
    v->bars = std::move(bars);
    return v;
}

// ───────────────────────── 統計圖 ─────────────────────────

BoxPlot* Plot::addBoxPlot(const QVector<QVector<double>>& groups, QStringList labels) {
    auto* b = emplace<BoxPlot>();
    b->boxes = BoxPlot::fromGroups(groups, labels);
    for (int i = 0; i < b->boxes.size(); ++i) b->boxes[i].fillColor = style.palette.color(i);
    if (!labels.isEmpty()) setCategoryTicks(bottomAxis(), labels);
    return b;
}

BoxPlot* Plot::addBoxPlot(QVector<Box> boxes) {
    auto* b = emplace<BoxPlot>();
    b->boxes = std::move(boxes);
    // 沒指定顏色的箱子補上調色盤的顏色
    for (int i = 0; i < b->boxes.size(); ++i)
        if (!b->boxes[i].fillColor.isValid()) b->boxes[i].fillColor = style.palette.color(i);
    return b;
}

Lollipop* Plot::addLollipop(QVector<double> values, QColor color) {
    const QColor c = orNext(color, *this);
    auto* l = emplace<Lollipop>();
    l->items = Lollipop::fromValues(values, c);
    l->marker.fillColor = c;
    l->marker.outline.color = c;
    l->labelFont.color = style.textColor;
    return l;
}

Population* Plot::addPopulation(const QVector<QVector<double>>& groups, QStringList labels) {
    auto* pop = emplace<Population>();
    for (int i = 0; i < groups.size(); ++i) {
        Population::Group g;
        g.values = groups[i];
        g.position = i;
        g.color = style.palette.color(i);
        g.label = (i < labels.size()) ? labels.at(i) : QString();
        pop->groups.append(g);
    }
    if (!labels.isEmpty()) setCategoryTicks(bottomAxis(), labels);
    return pop;
}

Population* Plot::addViolin(const QVector<QVector<double>>& groups, QStringList labels) {
    Population* pop = addPopulation(groups, std::move(labels));
    pop->showDistribution = true;
    pop->mirrored = true;
    pop->showPoints = false;
    pop->showErrorBar = false;
    pop->showBox = true;
    return pop;
}

Scatter* Plot::addRegressionLine(const QVector<double>& xs, const QVector<double>& ys,
                                 LinearFit* fitOut, QColor color) {
    const LinearFit fit = linearFit(xs, ys);
    if (fitOut) *fitOut = fit;

    // 迴歸線要畫滿可視範圍，不是只連到資料的頭尾，所以用 FunctionPlot 的概念：
    // 這裡直接取資料範圍再外推一成，縮放後也還夠長。
    CoordinateRange xr;
    for (double x : xs)
        if (std::isfinite(x)) xr.expand(x);
    if (!fit.isValid() || !xr.isValid()) return nullptr;
    xr.pad(0.1);

    Scatter* line = addScatter({xr.min, xr.max},
                               {fit.predict(xr.min), fit.predict(xr.max)},
                               orNext(color, *this));
    line->marker = MarkerStyle::none();
    return line;
}

// ───────────────────── 診斷與推論圖 ─────────────────────

ControlChart* Plot::addControlChart(QVector<double> values, QVector<ControlRule> rules) {
    auto* c = emplace<ControlChart>();
    c->values = std::move(values);
    c->rules = std::move(rules);
    c->line.color = nextColor();
    c->marker.fillColor = c->line.color;
    c->marker.outline.color = c->line.color;
    c->centerLine.color = withTextAlpha(style.textColor, 180);
    c->limitFont.color = withTextAlpha(style.textColor, 160);
    c->limitFont.size = 9;
    c->violationFont.color = c->violationMarker.fillColor;
    c->violationFont.size = 8;
    c->violationFont.bold = true;
    return c;
}

ForestPlot* Plot::addForestPlot(QVector<ForestRow> rows) {
    auto* f = emplace<ForestPlot>();
    f->rows = std::move(rows);
    f->intervalLine.color = style.textColor;
    f->summaryColor = style.textColor;
    f->nullLine.color = withTextAlpha(style.axisColor, 130);
    f->annotationFont.color = style.textColor;
    f->annotationFont.size = 9;
    // 左軸的刻度換成列名。版面會自己量最長的那一個再留出空間。
    setCategoryTicks(leftAxis(), f->rowLabels());
    return f;
}

Markers* Plot::addNormalQQ(const QVector<double>& values, Scatter** qqLineOut, QColor color) {
    const QQResult qq = normalQQ(values);
    if (qq.sample.isEmpty()) {
        if (qqLineOut) *qqLineOut = nullptr;
        return nullptr;
    }
    const QColor c = orNext(color, *this);
    auto* pts = addMarkers(qq.theoretical, qq.sample, c);

    Scatter* ref = nullptr;
    if (qq.referenceLine.isValid()) {
        // 參考線要畫滿理論分位數的範圍再外推一些，不然兩端的點會落在線外側
        CoordinateRange xr(qq.theoretical.first(), qq.theoretical.last());
        xr.pad(0.08);
        ref = addScatter({xr.min, xr.max},
                         {qq.referenceLine.predict(xr.min), qq.referenceLine.predict(xr.max)},
                         withTextAlpha(style.textColor, 150));
        ref->marker = MarkerStyle::none();
        ref->line.pattern = LinePattern::Dashed;
    }
    if (qqLineOut) *qqLineOut = ref;

    Axis* bx = bottomAxis();
    Axis* ly = leftAxis();
    setAutoText(bx, [bx] { return &bx->label.text; }, QT_TRANSLATE_NOOP("qtplot", "理論分位數"));
    setAutoText(ly, [ly] { return &ly->label.text; }, QT_TRANSLATE_NOOP("qtplot", "樣本分位數"));
    return pts;
}

Scatter* Plot::addClassifierCurve(const ClassifierCurve& curve, QColor color, bool fillBelow) {
    if (curve.xs.isEmpty()) return nullptr;
    const QColor c = orNext(color, *this);
    auto* s = addScatter(curve.xs, curve.ys, c);
    s->marker = MarkerStyle::none();
    s->line.width = 1.8;
    if (fillBelow) {
        s->fillToY = 0;
        QColor f = c;
        f.setAlpha(55);
        s->fill = FillStyle(f);
    }
    return s;
}

SurvivalPlottables Plot::addSurvivalCurve(const SurvivalCurve& km, QColor color, bool showBand,
                                          bool showCensorMarks) {
    SurvivalPlottables out;
    if (km.points.isEmpty()) return out;
    const QColor c = orNext(color, *this);

    QVector<double> ts, ss, lo, hi;
    ts.reserve(km.points.size());
    for (const SurvivalPoint& p : km.points) {
        ts.append(p.time);
        ss.append(p.survival);
        lo.append(p.lower);
        hi.append(p.upper);
    }

    // 帶要和階梯線一樣是階梯形。FillY 連的是直線，所以得自己把每一段的
    // 起點複製一份 —— 直接餵轉折點的話，帶會從上一個點斜著拉到下一個點，
    // 和階梯線中間露出一塊白。
    if (showBand) {
        QVector<double> bx, b1, b2;
        for (int i = 0; i < ts.size(); ++i) {
            if (i > 0) {
                bx.append(ts[i]);
                b1.append(lo[i - 1]);
                b2.append(hi[i - 1]);
            }
            bx.append(ts[i]);
            b1.append(lo[i]);
            b2.append(hi[i]);
        }
        QColor f = c;
        f.setAlpha(45);
        out.band = addFillY(bx, b1, b2, f);
        out.band->line = LineStyle::none();
    }

    out.curve = addScatter(ts, ss, c);
    out.curve->step = Scatter::StepStyle::Horizontal;
    out.curve->marker = MarkerStyle::none();
    out.curve->line.width = 1.8;

    if (showCensorMarks && !km.censorTimes.isEmpty()) {
        out.censorMarks = addMarkers(km.censorTimes, km.censorSurvival, c);
        out.censorMarks->marker.shape = MarkerShape::VerticalBar;
        out.censorMarks->marker.size = 9;
    }

    Axis* ly = leftAxis();
    setAutoText(ly, [ly] { return &ly->label.text; }, QT_TRANSLATE_NOOP("qtplot", "存活機率"));
    return out;
}

Heatmap* Plot::addSpectrogram(const Spectrogram& sp, Colormap map) {
    if (sp.values.isEmpty()) return nullptr;
    auto* h = emplace<Heatmap>();
    h->values = sp.values;
    h->rows = sp.rows;
    h->cols = sp.cols;
    h->colormap = map;
    h->extent = sp.extent();
    // Spectrogram 的第 0 列是最低頻，而 Heatmap 預設把第 0 列畫在最上面。
    // 不翻的話整張圖的頻率軸會上下顛倒 —— 刻度是對的，資料是反的，最難查。
    h->flipVertically = true;
    h->colorRange = sp.valueRange();
    Axis* bx = bottomAxis();
    Axis* ly = leftAxis();
    setAutoText(bx, [bx] { return &bx->label.text; }, QT_TRANSLATE_NOOP("qtplot", "時間（秒）"));
    setAutoText(ly, [ly] { return &ly->label.text; }, QT_TRANSLATE_NOOP("qtplot", "頻率（Hz）"));
    return h;
}

// ───────────────────────── 極座標族 ─────────────────────────

void Plot::usePolarLayout() {
    // 圓要是圓的，兩軸就得等比例
    equalAxisScale = true;
    grid.isVisible = false;
    // 直角座標的刻度對這類圖沒有意義，整組關掉（但框線留著當邊界）
    for (Axis* a : m_axes) {
        a->showTickLabels = false;
        a->majorTickLength = 0;
        a->minorTickLength = 0;
        a->showFrame = false;
    }
    paddingLeft = paddingRight = paddingTop = paddingBottom = 10;
}

namespace {

/// 把 values + labels 配成扇形，顏色由調色盤輪流給。
QVector<PieSlice> makeSlices(const QVector<double>& values, const QStringList& labels,
                             Plot& plot) {
    QVector<PieSlice> out;
    out.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i) {
        PieSlice s;
        s.value = values[i];
        s.label = (i < labels.size()) ? labels.at(i) : QString();
        s.fillColor = plot.style.palette.color(static_cast<int>(i));
        out.append(s);
    }
    return out;
}

}  // namespace

PolarAxis* Plot::addPolarAxis(double radius, int spokeCount) {
    usePolarLayout();
    auto* a = emplace<PolarAxis>();
    a->radius = radius;
    a->spokeCount = spokeCount;
    a->labelFont.color = style.textColor;
    a->ringLabelFont.color = withTextAlpha(style.textColor, 170);
    a->spokeLine.color = withTextAlpha(style.axisColor, 90);
    a->ringLine.color = withTextAlpha(style.axisColor, 60);
    return a;
}

Pie* Plot::addPie(QVector<double> values, QStringList labels) {
    usePolarLayout();
    auto* pie = emplace<Pie>();
    pie->slices = makeSlices(values, labels, *this);
    pie->labelFont.color = style.textColor;
    pie->sliceBorder.color = style.figureBackground;
    return pie;
}

Pie* Plot::addDonut(QVector<double> values, QStringList labels, double holeFraction) {
    Pie* pie = addPie(std::move(values), std::move(labels));
    pie->donutFraction = holeFraction;
    pie->centerLabel.font.color = style.textColor;
    return pie;
}

Coxcomb* Plot::addCoxcomb(QVector<double> values, QStringList labels) {
    usePolarLayout();
    auto* c = emplace<Coxcomb>();
    c->slices = makeSlices(values, labels, *this);
    c->labelFont.color = style.textColor;
    c->sliceBorder.color = style.figureBackground;
    return c;
}

Coxcomb* Plot::addCoxcomb(QVector<PieSlice> slices) {
    usePolarLayout();
    auto* c = emplace<Coxcomb>();
    c->slices = std::move(slices);
    // 沒指定顏色的格子補上調色盤的顏色
    for (int i = 0; i < c->slices.size(); ++i)
        if (!c->slices[i].fillColor.isValid()) c->slices[i].fillColor = style.palette.color(i);
    c->labelFont.color = style.textColor;
    c->sliceBorder.color = style.figureBackground;
    return c;
}

Radar* Plot::addRadar(QVector<QVector<double>> seriesValues, QStringList axisLabels) {
    usePolarLayout();
    auto* r = emplace<Radar>();
    r->axisLabels = std::move(axisLabels);
    r->labelFont.color = style.textColor;
    for (qsizetype i = 0; i < seriesValues.size(); ++i) {
        RadarSeries s;
        s.values = seriesValues[i];
        s.lineColor = style.palette.color(static_cast<int>(i));
        r->series.append(s);
    }
    return r;
}

RadialGauge* Plot::addRadialGauge(QVector<double> values, QStringList labels, double maximum) {
    usePolarLayout();
    auto* g = emplace<RadialGauge>();
    g->values = std::move(values);
    g->labels = std::move(labels);
    g->maximum = maximum;
    g->labelFont.color = style.textColor;
    for (qsizetype i = 0; i < g->values.size(); ++i)
        g->colors.append(style.palette.color(static_cast<int>(i)));
    return g;
}

// ───────────────────────────── 版面 ─────────────────────────────

PixelRect Plot::computeDataRect(const PixelRect& figure) {
    // 兩段式：先用估計長度產生刻度量出軸的厚度，算出繪圖區後再用真實長度重算刻度。
    // 一次不夠是因為刻度標籤的寬度會回頭影響繪圖區寬度。
    double left = paddingLeft, right = paddingRight;
    double top = paddingTop, bottom = paddingBottom;

    if (title.isVisible()) top += measureText(title).height() + 6;

    for (int pass = 0; pass < 2; ++pass) {
        const double estWidth = std::max(10.0, figure.width() - left - right);
        const double estHeight = std::max(10.0, figure.height() - top - bottom);

        for (Axis* a : m_axes) {
            if (!a->isVisible) continue;
            a->regenerateTicks(a->isVertical() ? estHeight : estWidth);
        }

        double l = paddingLeft, r = paddingRight;
        double t = paddingTop, b = paddingBottom;
        if (title.isVisible()) t += measureText(title).height() + 6;
        // 垂直軸的倍率標示（×10⁶）畫在繪圖區上方，要留高度給它
        double headroom = 0;
        for (Axis* a : m_axes) headroom = std::max(headroom, a->multiplierHeadroom());
        t += headroom;

        // 同一邊可以有多根軸。第一根貼著繪圖區，第二根以後往外讓開前面幾根的厚度，
        // 否則它們會疊在同一個位置、刻度數字糊成一團。
        double offLeft = 0, offRight = 0, offBottom = 0, offTop = 0;
        for (Axis* a : m_axes) {
            if (!a->isVisible) continue;
            const double thickness = a->measureThickness();
            switch (a->edge()) {
            case Edge::Left:   a->renderOffsetPx = offLeft;   offLeft += thickness;   l += thickness; break;
            case Edge::Right:  a->renderOffsetPx = offRight;  offRight += thickness;  r += thickness; break;
            case Edge::Bottom: a->renderOffsetPx = offBottom; offBottom += thickness; b += thickness; break;
            case Edge::Top:    a->renderOffsetPx = offTop;    offTop += thickness;    t += thickness; break;
            }
        }

        // 畫在繪圖區外側的圖元（色階圖例）也要佔位置。
        // 和座標軸同樣的道理：同一邊有兩個色階時，第二個必須讓開第一個，
        // 而且還要先讓開那一邊所有座標軸的厚度，不然會蓋在刻度數字上。
        for (const auto& p : m_plottables) {
            if (!p->isVisible) continue;
            const double ml = p->outsideMargin(Edge::Left);
            const double mr = p->outsideMargin(Edge::Right);
            const double mb = p->outsideMargin(Edge::Bottom);
            const double mt = p->outsideMargin(Edge::Top);
            if (ml > 0) { p->setOutsideOffset(Edge::Left,   offLeft);   offLeft += ml;   l += ml; }
            if (mr > 0) { p->setOutsideOffset(Edge::Right,  offRight);  offRight += mr;  r += mr; }
            if (mb > 0) { p->setOutsideOffset(Edge::Bottom, offBottom); offBottom += mb; b += mb; }
            if (mt > 0) { p->setOutsideOffset(Edge::Top,    offTop);    offTop += mt;    t += mt; }
        }
        // 放在外面的圖例最後才排，貼在座標軸與色階圖例的外側
        m_legendOutsideOffset = 0;
        if (legend.isVisible && legend.placement != Legend::Placement::Inside) {
            const QVector<LegendItem> items = collectLegendItems();
            if (!items.isEmpty()) {
                const LegendGrid g = measureLegend(legend, items);
                if (legend.placement == Legend::Placement::OutsideRight) {
                    m_legendOutsideOffset = offRight;
                    r += g.width + legend.marginX;
                } else {
                    m_legendOutsideOffset = offBottom;
                    b += g.height + legend.marginY;
                }
            }
        }
        left = l; right = r; top = t; bottom = b;
    }

    PixelRect dataRect = figure.shrunk(left, right, top, bottom);
    if (dataRect.isEmpty()) {
        // 視窗太小的時候別畫出負寬度的矩形
        dataRect = PixelRect(figure.left, std::max(figure.left + 1, figure.right),
                             figure.top, std::max(figure.top + 1, figure.bottom));
    }
    return dataRect;
}

// ───────────────────────────── 圖例 ─────────────────────────────

QVector<LegendItem> Plot::collectLegendItems() const {
    if (!legend.manualItems.isEmpty()) return legend.manualItems;
    QVector<LegendItem> items;
    for (const auto& p : m_plottables) {
        if (!p->isVisible && !legend.showHiddenSeries) continue;
        QVector<LegendItem> mine = p->legendItems();
        for (LegendItem& it : mine) {
            it.source = p.get();
            it.dimmed = !p->isVisible;
        }
        items += mine;
    }
    return items;
}

IPlottable* Plot::legendPlottableAt(QPointF pixel) const {
    for (const LegendHit& h : m_legendHits)
        if (h.rect.contains(pixel)) return h.plottable;
    return nullptr;
}

Plot::LegendGrid Plot::measureLegend(const Legend& lg, const QVector<LegendItem>& items) const {
    LegendGrid g;
    QFontMetricsF fm(lg.font.font());
    g.rowHeight = fm.height();
    const int n = static_cast<int>(items.size());
    if (n == 0) return g;

    if (lg.horizontal)
        // 橫向排列時 0 或 1 都代表全部擠在同一列（每列一項等於直向排列，不需要用這個設定）
        g.columns = lg.columns > 1 ? std::min(lg.columns, n) : n;
    else
        g.columns = std::clamp(lg.columns, 1, n);
    g.rows = (n + g.columns - 1) / g.columns;

    // 橫向排列一列一列填；直向排列一欄一欄填（讀的方向和排列方向一致）
    g.columnWidths.fill(0, g.columns);
    for (int i = 0; i < n; ++i) {
        const int col = lg.horizontal ? i % g.columns : i / g.rows;
        const double w = lg.symbolWidth + fm.horizontalAdvance(items[i].text) + 4.0;
        g.columnWidths[col] = std::max(g.columnWidths[col], w);
    }

    double contentW = 0;
    for (double w : g.columnWidths) contentW += w;
    contentW += lg.itemSpacing * 2 * (g.columns - 1);
    double contentH = g.rowHeight * g.rows + lg.itemSpacing * (g.rows - 1);

    if (lg.title.isVisible()) {
        const QSizeF t = measureText(lg.title);
        g.titleHeight = t.height() + lg.itemSpacing;
        contentW = std::max(contentW, t.width());
        contentH += g.titleHeight;
    }

    g.width = contentW + lg.padding * 2;
    g.height = contentH + lg.padding * 2;
    return g;
}

void Plot::renderLegend(QPainter& painter, const PixelRect& dataRect) {
    m_legendHits.clear();
    drawLegendBox(painter, legend, collectLegendItems(), dataRect, m_legendOutsideOffset, &m_legendHits);
}

void Plot::drawLegendBox(QPainter& painter, const Legend& lg, const QVector<LegendItem>& items,
                         const PixelRect& dataRect, double outsideOffset,
                         QVector<LegendHit>* hits) const {
    if (items.isEmpty()) return;

    QFontMetricsF fm(lg.font.font());
    const LegendGrid g = measureLegend(lg, items);
    const double rowHeight = g.rowHeight;
    const double boxW = g.width;
    const double boxH = g.height;

    double x = dataRect.left + lg.marginX;
    double y = dataRect.top + lg.marginY;
    if (lg.placement == Legend::Placement::OutsideRight) {
        // 繪圖區右邊、座標軸與色階圖例的外側；垂直位置照 alignment
        x = dataRect.right + outsideOffset + lg.marginX;
        y = dataRect.top;
        if (lg.alignment & Qt::AlignBottom)  y = dataRect.bottom - boxH;
        if (lg.alignment & Qt::AlignVCenter) y = (dataRect.top + dataRect.bottom) / 2 - boxH / 2;
    } else if (lg.placement == Legend::Placement::OutsideBottom) {
        // 底軸之下；水平位置照 alignment（預設置中）
        y = dataRect.bottom + outsideOffset + lg.marginY;
        x = (dataRect.left + dataRect.right) / 2 - boxW / 2;
        if ((lg.alignment & Qt::AlignLeft) && !(lg.alignment & Qt::AlignHCenter)) x = dataRect.left;
        if ((lg.alignment & Qt::AlignRight) && !(lg.alignment & Qt::AlignHCenter)) x = dataRect.right - boxW;
    } else {
        if (lg.alignment & Qt::AlignRight)   x = dataRect.right - lg.marginX - boxW;
        if (lg.alignment & Qt::AlignHCenter) x = (dataRect.left + dataRect.right) / 2 - boxW / 2;
        if (lg.alignment & Qt::AlignBottom)  y = dataRect.bottom - lg.marginY - boxH;
        if (lg.alignment & Qt::AlignVCenter) y = (dataRect.top + dataRect.bottom) / 2 - boxH / 2;
    }

    painter.save();
    const QRectF box(x, y, boxW, boxH);
    if (lg.background.isVisible() || lg.border.isVisible()) {
        painter.setBrush(lg.background.brush());
        painter.setPen(lg.border.isVisible() ? lg.border.pen() : QPen(Qt::NoPen));
        painter.drawRect(box);
    }

    if (lg.title.isVisible())
        drawText(painter, QPointF(x + boxW / 2, y + lg.padding), lg.title, Qt::AlignHCenter | Qt::AlignTop);

    const double originY = y + lg.padding + g.titleHeight;
    for (int i = 0; i < items.size(); ++i) {
        const LegendItem& it = items[i];
        const int col = lg.horizontal ? i % g.columns : i / g.rows;
        const int row = lg.horizontal ? i / g.columns : i % g.rows;
        double cx = x + lg.padding;
        for (int c = 0; c < col; ++c) cx += g.columnWidths[c] + lg.itemSpacing * 2;
        const double cy = originY + row * (rowHeight + lg.itemSpacing);

        const double midY = cy + rowHeight / 2;
        const double symLeft = cx;
        const double symRight = cx + lg.symbolWidth - 6;
        const double rowWidth =
            lg.symbolWidth + fm.horizontalAdvance(it.text) + 4.0;

        // 這一列佔的範圍，給點擊切換用
        if (hits) hits->append({QRectF(cx - 2, cy, rowWidth + 4, rowHeight), it.source});

        // 被隱藏的圖元那一列整列調淡 —— 看得到但看得出來是關著的
        auto dim = [&](QColor c) {
            if (it.dimmed) c.setAlpha(c.alpha() * std::clamp(lg.dimmedAlpha, 0, 255) / 255);
            return c;
        };

        // 符號：填色塊 → 線 → 標記，依樣式有無決定畫什麼
        if (it.fill.isVisible() && !lg.overrideMarkerShape) {
            FillStyle f = it.fill;
            f.color = dim(f.color);
            const QRectF sym(symLeft, midY - rowHeight * 0.3, symRight - symLeft, rowHeight * 0.6);
            painter.setPen(Qt::NoPen);
            // 花紋的筆刷沒有底色：有底色就先鋪
            if (f.hatch != HatchPattern::None && f.color.alpha() > 0 && !f.hasGradient())
                painter.fillRect(sym, f.color);
            painter.setBrush(f.brush());
            painter.drawRect(sym);
        }
        if (it.line.isVisible() && !lg.overrideMarkerShape) {
            LineStyle ls = it.line;
            ls.color = dim(ls.color);
            painter.setPen(ls.pen());
            painter.drawLine(QPointF(symLeft, midY), QPointF(symRight, midY));
        }
        if (it.marker.isVisible() || lg.overrideMarkerShape) {
            MarkerStyle m = it.marker;
            if (lg.overrideMarkerShape) {
                // 原本沒有標記的項目（只有線或色塊）就借它的顏色
                if (!it.marker.isVisible()) {
                    const QColor c = it.line.isVisible() ? it.line.color : it.fill.color;
                    m = MarkerStyle(lg.markerShapeOverride, 8, c);
                }
                m.shape = lg.markerShapeOverride;
            }
            m.fillColor = dim(m.fillColor);
            m.outline.color = dim(m.outline.color);
            drawMarker(painter, QPointF((symLeft + symRight) / 2, midY), m);
        }

        Label lb;
        lb.text = it.text;
        lb.font = lg.font;
        lb.font.color = dim(lb.font.color);
        if (it.dimmed) lb.font.strikeOut = true;
        drawText(painter, QPointF(cx + lg.symbolWidth, midY), lb,
                 Qt::AlignLeft | Qt::AlignVCenter);
    }
    painter.restore();
}

// ───────────────────────────── 渲染 ─────────────────────────────

void Plot::render(QPainter& painter, const QRectF& target) {
    QElapsedTimer timer;
    timer.start();

    const PixelRect figure = PixelRect::fromQRectF(target);
    if (figure.isEmpty()) return;

    if (m_autoScalePending) autoScale();
    if (m_languageGeneration != languageGeneration()) retranslate();

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, antiAlias);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. 整張圖的底色與背景影像
    if (style.figureBackground.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(style.figureBackground));
        painter.drawRect(target);
    }
    drawBackgroundImage(painter, figureBackgroundImage, target);

    // 2. 讓需要依資料調整軸的圖元先動（即時資料）。
    //    一定要在 computeDataRect 之前 —— 版面與刻度都是用軸的範圍算的。
    for (const auto& p : m_plottables)
        if (p->isVisible) p->beforeLayout();

    // 3. 版面
    PixelRect dataRect = computeDataRect(figure);
    if (!fixedDataRect.isEmpty()) {
        // 固定的繪圖區：刻度要依新的長度重算
        dataRect = PixelRect::fromQRectF(fixedDataRect.translated(target.topLeft()));
        for (Axis* a : m_axes)
            if (a->isVisible) a->regenerateTicks(a->isVertical() ? dataRect.height() : dataRect.width());
    }

    // 3. 等比例軸：把較「鬆」的那一軸放大到和另一軸同比例
    if (equalAxisScale && dataRect.width() > 0 && dataRect.height() > 0) {
        const double sx = dataRect.width() / m_bottom->range.span();
        const double sy = dataRect.height() / m_left->range.span();
        const double s = std::min(sx, sy);
        const double cxv = m_bottom->range.center();
        const double cyv = m_left->range.center();
        const double halfX = dataRect.width() / s / 2;
        const double halfY = dataRect.height() / s / 2;
        m_bottom->range = {cxv - halfX, cxv + halfX};
        m_top->range = m_bottom->range;
        m_left->range = {cyv - halfY, cyv + halfY};
        m_right->range = m_left->range;
        for (Axis* a : m_axes)
            if (a->isVisible) a->regenerateTicks(a->isVertical() ? dataRect.height()
                                                                : dataRect.width());
    }
    m_lastDataRect = dataRect;

    // 4. 繪圖區底色與背景影像
    if (style.dataBackground.alpha() > 0 && style.dataBackground != style.figureBackground) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(style.dataBackground));
        painter.drawRect(dataRect.toQRectF());
    }
    drawBackgroundImage(painter, dataBackgroundImage, dataRect.toQRectF());

    // 髮絲線：暫時把軸與格線的線條換成 cosmetic pen，畫完再換回來
    struct AxisLines { LineStyle frame, major, minor; };
    QVector<AxisLines> savedAxisLines;
    const Grid savedGrid = grid;
    if (hairline) {
        auto thin = [](LineStyle& s) { s.hairline = true; s.width = 1.0; };
        for (Axis* a : m_axes) {
            savedAxisLines.append({a->frameLine, a->majorTickLine, a->minorTickLine});
            thin(a->frameLine);
            thin(a->majorTickLine);
            thin(a->minorTickLine);
        }
        thin(grid.majorLine);
        thin(grid.minorLine);
    }

    // 5. 格線（在圖元之下）
    auto drawGrid = [&]() {
        if (!grid.isVisible) return;
        // 填色全部先畫完，再畫線。混著畫的話，後一根軸的條帶會蓋住前一根的格線。
        for (Axis* a : m_axes) {
            if (!a->isVisible || !a->showTickLabels) continue;
            a->renderGridFill(painter, dataRect);
        }
        for (Axis* a : m_axes) {
            if (!a->isVisible || !a->showTickLabels) continue;  // 上／右軸不重複畫格線
            a->renderGridlines(painter, dataRect, grid.majorLine, grid.minorLine);
        }
    };
    if (!grid.onTop) drawGrid();

    // 6. 圖元（裁切到繪圖區，圖才不會畫到軸上）
    painter.save();
    painter.setClipRect(dataRect.toQRectF());
    RenderPack rp;
    rp.painter = &painter;
    rp.figureRect = figure;
    rp.dataRect = dataRect;
    rp.plot = this;
    for (const auto& p : m_plottables) {
        if (!p->isVisible || !p->clipToDataArea()) continue;
        painter.save();
        p->render(rp);
        painter.restore();
    }
    painter.restore();

    // 不裁切的那些畫在外面（色階圖例之類），所以要在解除裁切之後才畫
    for (const auto& p : m_plottables) {
        if (!p->isVisible || p->clipToDataArea()) continue;
        painter.save();
        p->render(rp);
        painter.restore();
    }

    if (grid.onTop) drawGrid();

    // 7. 軸
    for (Axis* a : m_axes)
        if (a->isVisible) a->render(painter, dataRect);

    // 截斷的空隙要蓋在圖元與框線之上，所以放在軸之後
    {
        const QColor gapColor = style.dataBackground.alpha() > 0 ? style.dataBackground : style.figureBackground;
        for (Axis* a : m_axes)
            if (a->hasBreaks()) a->renderBreaks(painter, dataRect, gapColor);
    }

    if (hairline) {
        for (int i = 0; i < m_axes.size() && i < savedAxisLines.size(); ++i) {
            m_axes[i]->frameLine = savedAxisLines[i].frame;
            m_axes[i]->majorTickLine = savedAxisLines[i].major;
            m_axes[i]->minorTickLine = savedAxisLines[i].minor;
        }
        grid = savedGrid;
    }

    // 8. 標題
    if (title.isVisible()) {
        const double cx = titleCenteredOnFigure ? (figure.left + figure.right) / 2
                                                : (dataRect.left + dataRect.right) / 2;
        drawText(painter, QPointF(cx, figure.top + paddingTop), title, Qt::AlignHCenter | Qt::AlignTop);
    }

    // 9. 圖例（最上層）
    if (legend.isVisible) renderLegend(painter, dataRect);

    // 10. 整張圖的外框
    if (figureBorder.isVisible()) {
        const double h = figureBorder.width / 2;
        painter.setPen(figureBorder.pen());
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(target.adjusted(h, h, -h, -h));
    }

    painter.restore();
    m_lastRenderMs = timer.nsecsElapsed() / 1.0e6;
}

QImage Plot::renderToImage(int width, int height, double scale) {
    const int w = std::max(1, static_cast<int>(width * scale));
    const int h = std::max(1, static_cast<int>(height * scale));
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(style.figureBackground);
    QPainter painter(&img);
    if (scale != 1.0) painter.scale(scale, scale);
    render(painter, QRectF(0, 0, width, height));
    painter.end();
    return img;
}

bool Plot::savePng(const QString& path, int width, int height, double scale) {
    return renderToImage(width, height, scale).save(path, "PNG");
}

PixelRect Plot::measureDataRect(const QRectF& target) {
    const PixelRect figure = PixelRect::fromQRectF(target);
    if (figure.isEmpty()) return figure;
    if (m_autoScalePending) autoScale();
    for (const auto& p : m_plottables)
        if (p->isVisible) p->beforeLayout();
    return computeDataRect(figure);
}

bool Plot::savePdf(const QString& path, int width, int height) {
    // 解析度設成 72，一個裝置像素就剛好是一點，呼叫端給的尺寸才會和
    // savePng 的那組數字意義一致。
    QPdfWriter writer(path);
    writer.setResolution(72);
    writer.setPageSize(QPageSize(QSizeF(width, height), QPageSize::Point, QString(),
                                 QPageSize::ExactMatch));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    if (!painter.isActive()) return false;
    render(painter, QRectF(0, 0, width, height));
    painter.end();
    return true;
}

bool Plot::saveSvg(const QString& path, int width, int height) {
    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(QSize(width, height));
    gen.setViewBox(QRect(0, 0, width, height));
    gen.setTitle(title.text.isEmpty() ? QStringLiteral("QtPlot") : title.text);
    QPainter painter;
    if (!painter.begin(&gen)) return false;
    render(painter, QRectF(0, 0, width, height));
    painter.end();
    return true;
}

}  // namespace qtplot

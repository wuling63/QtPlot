#include "Multiplot.h"

#include <QEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>
#include <QSvgGenerator>

namespace qtplot {

// ───────────────────────────── Multiplot ─────────────────────────────

Multiplot::Multiplot(int rows, int cols)
    : m_rows(std::max(1, rows)), m_cols(std::max(1, cols)) {}

Multiplot::~Multiplot() = default;

void Multiplot::resize(int rows, int cols) {
    m_rows = std::max(1, rows);
    m_cols = std::max(1, cols);
    for (int i = static_cast<int>(m_cells.size()) - 1; i >= 0; --i) {
        const Cell& c = m_cells[i];
        if (c.row >= m_rows || c.col >= m_cols) m_cells.erase(m_cells.begin() + i);
    }
}

Multiplot::Cell* Multiplot::findCell(int row, int col) const {
    for (const Cell& c : m_cells)
        if (row >= c.row && row < c.row + c.rowSpan && col >= c.col && col < c.col + c.colSpan)
            return const_cast<Cell*>(&c);
    return nullptr;
}

Plot* Multiplot::existing(int row, int col) const {
    Cell* c = findCell(row, col);
    return c ? c->plot.get() : nullptr;
}

Plot* Multiplot::at(int row, int col) {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return nullptr;
    if (Cell* c = findCell(row, col)) return c->plot.get();
    Cell cell;
    cell.row = row;
    cell.col = col;
    cell.plot = std::make_unique<Plot>();
    Plot* raw = cell.plot.get();
    m_cells.push_back(std::move(cell));
    return raw;
}

Plot* Multiplot::span(int row, int col, int rowSpan, int colSpan) {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return nullptr;
    rowSpan = std::clamp(rowSpan, 1, m_rows - row);
    colSpan = std::clamp(colSpan, 1, m_cols - col);

    // 先把會被蓋到的格子清掉。留著的話兩張圖會畫在同一塊畫布上互相覆蓋，
    // 而且是後畫的贏 —— 與其讓它靜靜地蓋掉，不如直接拿掉。
    for (int i = static_cast<int>(m_cells.size()) - 1; i >= 0; --i) {
        const Cell& c = m_cells[i];
        const bool overlapRow = c.row < row + rowSpan && row < c.row + c.rowSpan;
        const bool overlapCol = c.col < col + colSpan && col < c.col + c.colSpan;
        if (overlapRow && overlapCol) m_cells.erase(m_cells.begin() + i);
    }

    Cell cell;
    cell.row = row;
    cell.col = col;
    cell.rowSpan = rowSpan;
    cell.colSpan = colSpan;
    cell.plot = std::make_unique<Plot>();
    Plot* raw = cell.plot.get();
    m_cells.push_back(std::move(cell));
    return raw;
}

QVector<Plot*> Multiplot::plots() const {
    QVector<Plot*> out;
    out.reserve(static_cast<qsizetype>(m_cells.size()));
    for (const Cell& c : m_cells) out.append(c.plot.get());
    return out;
}

void Multiplot::shareX(const QVector<Plot*>& group) {
    if (group.size() >= 2) m_shareX.append(group);
}
void Multiplot::shareY(const QVector<Plot*>& group) {
    if (group.size() >= 2) m_shareY.append(group);
}
void Multiplot::shareXAll() { shareX(plots()); }
void Multiplot::shareYAll() { shareY(plots()); }
void Multiplot::clearSharing() {
    m_shareX.clear();
    m_shareY.clear();
}

void Multiplot::visitFonts(const FontVisitor& visit) {
    visit(title.font);
    for (const Cell& c : m_cells) c.plot->visitFonts(visit);
}

double Multiplot::titleHeight() const {
    return title.isVisible() ? measureText(title).height() + 8 : 0.0;
}

QRectF Multiplot::cellRect(const QRectF& target, int row, int col, int rowSpan,
                           int colSpan) const {
    const double left = target.left() + paddingLeft;
    const double top = target.top() + paddingTop + titleHeight();
    const double width = std::max(1.0, target.width() - paddingLeft - paddingRight);
    const double height = std::max(1.0, target.height() - paddingTop - paddingBottom
                                            - titleHeight());

    auto weight = [](const QVector<double>& w, int i) {
        const double v = (i < w.size()) ? w[i] : 1.0;
        return (v > 0) ? v : 1.0;
    };
    double totalW = 0, totalH = 0;
    for (int c = 0; c < m_cols; ++c) totalW += weight(columnWeights, c);
    for (int r = 0; r < m_rows; ++r) totalH += weight(rowWeights, r);

    const double usableW = width - spacingX * (m_cols - 1);
    const double usableH = height - spacingY * (m_rows - 1);

    double x = left;
    for (int c = 0; c < col; ++c) x += usableW * weight(columnWeights, c) / totalW + spacingX;
    double y = top;
    for (int r = 0; r < row; ++r) y += usableH * weight(rowWeights, r) / totalH + spacingY;

    double w = 0;
    for (int c = col; c < col + colSpan && c < m_cols; ++c)
        w += usableW * weight(columnWeights, c) / totalW + (c > col ? spacingX : 0);
    double h = 0;
    for (int r = row; r < row + rowSpan && r < m_rows; ++r)
        h += usableH * weight(rowWeights, r) / totalH + (r > row ? spacingY : 0);

    return QRectF(x, y, std::max(1.0, w), std::max(1.0, h));
}

void Multiplot::applySharing() {
    auto unify = [](const QVector<Plot*>& group, bool horizontal) {
        CoordinateRange r;
        for (Plot* p : group) {
            if (!p) continue;
            Axis* a = horizontal ? p->bottomAxis() : p->leftAxis();
            const CoordinateRange d = p->dataLimits(a);
            if (d.isValid()) r.expand(d);
        }
        if (!r.hasArea()) return;
        r.pad(0.05);
        for (Plot* p : group) {
            if (!p) continue;
            if (horizontal) {
                p->bottomAxis()->setRange(r.min, r.max);
                p->topAxis()->setRange(r.min, r.max);
            } else {
                p->leftAxis()->setRange(r.min, r.max);
                p->rightAxis()->setRange(r.min, r.max);
            }
        }
    };
    for (const QVector<Plot*>& g : m_shareX) unify(g, true);
    for (const QVector<Plot*>& g : m_shareY) unify(g, false);
}

void Multiplot::render(QPainter& painter, const QRectF& target) {
    if (target.width() <= 0 || target.height() <= 0) return;

    painter.save();
    if (background.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(background));
        painter.drawRect(target);
    }
    if (title.isVisible()) {
        drawText(painter, QPointF(target.center().x(), target.top() + paddingTop + 2), title,
                 Qt::AlignHCenter | Qt::AlignTop);
    }

    applySharing();

    // ── 對齊：先量一遍每張子圖各邊要讓出多少，再取同一欄（列）的最大值補回去 ──
    //
    // 補的方式是暫時加大 padding，畫完就還原 —— 直接寫進 padding 會在下一次
    // render 時再加一次，圖會一幀比一幀往內縮。
    struct Saved { Plot* plot; double l, r, t, b; };
    QVector<Saved> saved;
    if (alignAxes && m_cells.size() > 1) {
        QHash<int, double> maxLeft, maxRight, maxTop, maxBottom;
        QVector<QRectF> targets;
        QVector<PixelRect> areas;
        targets.reserve(static_cast<qsizetype>(m_cells.size()));
        areas.reserve(static_cast<qsizetype>(m_cells.size()));
        for (const Cell& c : m_cells) {
            const QRectF rect = cellRect(target, c.row, c.col, c.rowSpan, c.colSpan);
            const PixelRect area = c.plot->measureDataRect(rect);
            targets.append(rect);
            areas.append(area);
            const int firstCol = c.col, lastCol = c.col + c.colSpan - 1;
            const int firstRow = c.row, lastRow = c.row + c.rowSpan - 1;
            maxLeft[firstCol] = std::max(maxLeft.value(firstCol, 0.0), area.left - rect.left());
            maxRight[lastCol] = std::max(maxRight.value(lastCol, 0.0), rect.right() - area.right);
            maxTop[firstRow] = std::max(maxTop.value(firstRow, 0.0), area.top - rect.top());
            maxBottom[lastRow] =
                std::max(maxBottom.value(lastRow, 0.0), rect.bottom() - area.bottom);
        }
        for (int i = 0; i < static_cast<int>(m_cells.size()); ++i) {
            const Cell& c = m_cells[i];
            Plot* p = c.plot.get();
            saved.append({p, p->paddingLeft, p->paddingRight, p->paddingTop, p->paddingBottom});
            const QRectF& rect = targets[i];
            const PixelRect& area = areas[i];
            p->paddingLeft += maxLeft.value(c.col, 0.0) - (area.left - rect.left());
            p->paddingRight +=
                maxRight.value(c.col + c.colSpan - 1, 0.0) - (rect.right() - area.right);
            p->paddingTop += maxTop.value(c.row, 0.0) - (area.top - rect.top());
            p->paddingBottom +=
                maxBottom.value(c.row + c.rowSpan - 1, 0.0) - (rect.bottom() - area.bottom);
        }
    }

    for (const Cell& c : m_cells)
        c.plot->render(painter, cellRect(target, c.row, c.col, c.rowSpan, c.colSpan));

    for (const Saved& s : saved) {
        s.plot->paddingLeft = s.l;
        s.plot->paddingRight = s.r;
        s.plot->paddingTop = s.t;
        s.plot->paddingBottom = s.b;
    }
    painter.restore();
}

QImage Multiplot::renderToImage(int width, int height, double scale) {
    const int w = std::max(1, static_cast<int>(width * scale));
    const int h = std::max(1, static_cast<int>(height * scale));
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(background);
    QPainter painter(&img);
    if (scale != 1.0) painter.scale(scale, scale);
    render(painter, QRectF(0, 0, width, height));
    painter.end();
    return img;
}

bool Multiplot::savePng(const QString& path, int width, int height, double scale) {
    return renderToImage(width, height, scale).save(path, "PNG");
}

bool Multiplot::saveSvg(const QString& path, int width, int height) {
    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(QSize(width, height));
    gen.setViewBox(QRect(0, 0, width, height));
    QPainter painter(&gen);
    render(painter, QRectF(0, 0, width, height));
    painter.end();
    return true;
}

namespace {

/// 把 QPainter 接到一個 width × height「點」的 PDF 頁面上。
///
/// QPdfWriter 的單位是裝置像素，解析度由 setResolution 決定。把解析度設成 72，
/// 一個裝置像素就剛好是一點，呼叫端給的尺寸才會和 PNG 的那組數字意義一致。
bool renderToPdf(const QString& path, int width, int height,
                 const std::function<void(QPainter&)>& draw) {
    QPdfWriter writer(path);
    writer.setResolution(72);
    QPageSize size(QSizeF(width, height), QPageSize::Point, QString(),
                   QPageSize::ExactMatch);
    writer.setPageSize(size);
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    if (!painter.isActive()) return false;
    draw(painter);
    painter.end();
    return true;
}

}  // namespace

bool Multiplot::savePdf(const QString& path, int width, int height) {
    return renderToPdf(path, width, height,
                       [&](QPainter& p) { render(p, QRectF(0, 0, width, height)); });
}

// ───────────────────────────── MultiplotWidget ─────────────────────────────

MultiplotWidget::MultiplotWidget(QWidget* parent)
    : QWidget(parent), m_multiplot(std::make_unique<Multiplot>()) {
    setAutoFillBackground(false);
}

MultiplotWidget::~MultiplotWidget() = default;

void MultiplotWidget::resetMultiplot(int rows, int cols) {
    m_multiplot = std::make_unique<Multiplot>(rows, cols);
    update();
}

void MultiplotWidget::changeEvent(QEvent* event) {
    // 換語言之後重畫：各子圖在 render 時會把自動填的軸名重翻
    if (event->type() == QEvent::LanguageChange) update();
    QWidget::changeEvent(event);
}

void MultiplotWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    m_multiplot->render(painter, QRectF(rect()));
}

// ───────────────────────────── InsetPlot ─────────────────────────────

InsetPlot::InsetPlot() : m_inner(std::make_unique<Plot>()) {
    // 小圖的預設版面要比主圖緊：擠在角落裡，留白和主圖一樣寬的話就沒地方畫資料了
    m_inner->paddingLeft = m_inner->paddingRight = 2;
    m_inner->paddingTop = m_inner->paddingBottom = 2;
    m_inner->bottomAxis()->tickFont.size = 7.5;
    m_inner->leftAxis()->tickFont.size = 7.5;
    // 不透明。半透明的話主圖的線會從小圖底下透出來，看起來像畫錯了。
    m_inner->style.figureBackground = QColor(255, 255, 255);
}

InsetPlot::~InsetPlot() = default;

AxisLimits InsetPlot::axisLimits() const {
    // 小圖是畫在主圖「上面」的，不參與主圖的自動範圍 ——
    // 算進去的話，主圖會為了容納一個貼在角落的裝飾而改變視野。
    if (!useDataCoordinates) return {};
    AxisLimits lim;
    lim.x.expand(dataRect.left);
    lim.x.expand(dataRect.right);
    lim.y.expand(dataRect.bottom);
    lim.y.expand(dataRect.top);
    return lim;
}

void InsetPlot::visitFonts(const FontVisitor& visit) { m_inner->visitFonts(visit); }

void InsetPlot::render(RenderPack& rp) {
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;

    QRectF box;
    if (useDataCoordinates) {
        box = t.pixelRect(dataRect);
    } else {
        const QRectF area = t.rect.toQRectF();
        box = QRectF(area.left() + rectFraction.x() * area.width(),
                     area.top() + rectFraction.y() * area.height(),
                     rectFraction.width() * area.width(),
                     rectFraction.height() * area.height());
    }
    box = box.adjusted(marginPx * rp.scaleFactor, marginPx * rp.scaleFactor,
                       -marginPx * rp.scaleFactor, -marginPx * rp.scaleFactor);
    if (box.width() < 8 || box.height() < 8) return;

    // ── 標出放大的是哪一段 ──
    CoordinateRect src = sourceRegion;
    if (!(src.width() > 0 && src.height() > 0)) {
        src = CoordinateRect(m_inner->bottomAxis()->range.min, m_inner->bottomAxis()->range.max,
                             m_inner->leftAxis()->range.min, m_inner->leftAxis()->range.max);
    }
    if (showSourceRegion && src.width() > 0 && src.height() > 0) {
        const QRectF srcBox = t.pixelRect(src);
        p.save();
        p.setBrush(sourceFill.brush(srcBox));
        p.setPen(sourceLine.isVisible() ? sourceLine.pen() : QPen(Qt::NoPen));
        p.drawRect(srcBox);

        if (connectorLine.isVisible()) {
            QPen cp = connectorLine.pen();
            cp.setWidthF(cp.widthF() * rp.scaleFactor);
            p.setPen(cp);
            // 連兩條線：取小圖與來源框「相隔最遠」的那一組角，線才不會穿過兩個框
            const QPointF srcCorners[4] = {srcBox.topLeft(), srcBox.topRight(),
                                           srcBox.bottomRight(), srcBox.bottomLeft()};
            const QPointF boxCorners[4] = {box.topLeft(), box.topRight(), box.bottomRight(),
                                           box.bottomLeft()};
            QVector<QPair<double, int>> order;
            for (int i = 0; i < 4; ++i) {
                const QPointF d = boxCorners[i] - srcCorners[i];
                order.append({std::hypot(d.x(), d.y()), i});
            }
            std::sort(order.begin(), order.end(),
                      [](const QPair<double, int>& a, const QPair<double, int>& b) {
                          return a.first > b.first;
                      });
            for (int k = 0; k < 2; ++k) {
                const int i = order[k].second;
                p.drawLine(srcCorners[i], boxCorners[i]);
            }
        }
        p.restore();
    }

    // ── 小圖本身 ──
    p.save();
    p.setClipRect(box);
    m_inner->render(p, box);
    p.restore();

    if (border.isVisible()) {
        QPen bp = border.pen();
        bp.setWidthF(bp.widthF() * rp.scaleFactor);
        p.setPen(bp);
        p.setBrush(Qt::NoBrush);
        p.drawRect(box);
    }
}

InsetPlot* addInset(Plot& host) {
    auto* inset = host.emplace<InsetPlot>();
    inset->inner().setStyle(host.style);
    inset->inner().paddingLeft = inset->inner().paddingRight = 2;
    inset->inner().paddingTop = inset->inner().paddingBottom = 2;
    inset->inner().bottomAxis()->tickFont.size = 7.5;
    inset->inner().leftAxis()->tickFont.size = 7.5;
    return inset;
}

}  // namespace qtplot

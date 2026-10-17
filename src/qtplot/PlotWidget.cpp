#include "PlotWidget.h"
#include "Translation.h"

#include <QApplication>
#include <QClipboard>
#include <QEasingCurve>
#include <QFileDialog>
#include <QActionGroup>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QTimer>
#include <QTouchEvent>
#include <QVariantAnimation>
#include <QWheelEvent>

namespace qtplot {

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent), m_plot(std::make_unique<Plot>()) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    // 自己畫底色，不要讓 widget 的樣式背景透出來
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    // 觸控螢幕：單指平移、兩指捏合（見 touchEvent）
    setAttribute(Qt::WA_AcceptTouchEvents, true);

    m_viewAnim = new QVariantAnimation(this);
    m_viewAnim->setStartValue(0.0);
    m_viewAnim->setEndValue(1.0);
    m_viewAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_viewAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        if (m_animFrom.size() != m_animTo.size()) return;
        const double t = v.toDouble();
        ViewSnapshot now = m_animFrom;
        for (int i = 0; i < now.size(); ++i) {
            now[i].min = m_animFrom[i].min + (m_animTo[i].min - m_animFrom[i].min) * t;
            now[i].max = m_animFrom[i].max + (m_animTo[i].max - m_animFrom[i].max) * t;
        }
        applyView(now);
        pushAxesToLinked();
        requestFull();
    });
    connect(m_viewAnim, &QVariantAnimation::finished, this, [this] {
        // 最後一格不一定剛好落在 1.0（計時器的粒度），收尾時直接套終點
        applyView(m_animTo);
        pushAxesToLinked();
        pushViewHistory();
        emit axisLimitsChanged();
        requestFull();
    });
}

PlotWidget::~PlotWidget() = default;

bool PlotWidget::inDataArea(QPointF pixel) const {
    const PixelRect r = m_plot->lastDataRect();
    return !r.isEmpty() && r.contains(pixel.x(), pixel.y());
}

// ───────────────────────────── 繪製 ─────────────────────────────

void PlotWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);

    if (!layerCacheEnabled) {
        m_cache = QImage();
        m_plot->render(painter, QRectF(0, 0, width(), height()));
    } else {
        // 只有疊加層在動、而且圖的狀態沒變，才沿用快取
        const qreal dpr = devicePixelRatioF();
        const bool sizeOk = !m_cache.isNull() && m_cache.devicePixelRatio() == dpr &&
                            m_cache.size() == (QSizeF(size()) * dpr).toSize();
        const bool reuse = m_overlayOnly && !m_fullPending && sizeOk && sceneFingerprint() == m_cacheFingerprint;
        if (!reuse) renderCache();
        painter.drawImage(QPointF(0, 0), m_cache);
    }
    m_overlayOnly = false;
    m_fullPending = false;
    drawOverlay(painter);
}

void PlotWidget::renderCache() {
    const qreal dpr = devicePixelRatioF();
    const QSize px = (QSizeF(size()) * dpr).toSize().expandedTo(QSize(1, 1));
    if (m_cache.size() != px || m_cache.devicePixelRatio() != dpr) {
        m_cache = QImage(px, QImage::Format_ARGB32_Premultiplied);
        m_cache.setDevicePixelRatio(dpr);
    }
    // 底色先鋪不透明的：widget 本身是不透明的（WA_OpaquePaintEvent），圖的背景若是半透明，
    // 直接畫會透出上一張的殘影
    QColor base = palette().color(QPalette::Window);
    m_cache.fill(base);
    QPainter cp(&m_cache);
    m_plot->render(cp, QRectF(0, 0, width(), height()));
    cp.end();
    m_cacheFingerprint = sceneFingerprint();
}

QVector<double> PlotWidget::sceneFingerprint() const {
    QVector<double> f;
    for (const Axis* a : m_plot->axes()) {
        f.append(a->range.min);
        f.append(a->range.max);
    }
    const QVector<IPlottable*> items = m_plot->plottables();
    f.append(items.size());
    for (const IPlottable* p : items) {
        f.append(p->isVisible ? 1 : 0);
        f.append(static_cast<double>(p->dataVersion()));
    }
    f.append(m_plot->legend.isVisible ? 1 : 0);
    f.append(m_plot->grid.isVisible ? 1 : 0);
    return f;
}

void PlotWidget::requestFull() {
    m_fullPending = true;
    m_overlayOnly = false;
    update();
}

void PlotWidget::requestOverlay() {
    if (!m_fullPending) m_overlayOnly = true;
    update();
}

void PlotWidget::refresh() { requestFull(); }

void PlotWidget::setAutoRefresh(int intervalMs) {
    if (intervalMs <= 0) {
        delete m_autoRefreshTimer;
        m_autoRefreshTimer = nullptr;
        return;
    }
    if (!m_autoRefreshTimer) {
        m_autoRefreshTimer = new QTimer(this);
        connect(m_autoRefreshTimer, &QTimer::timeout, this, [this] {
            quint64 sum = 0;
            for (const IPlottable* p : m_plot->plottables()) sum += p->dataVersion();
            if (sum == m_lastDataVersion) return;  // 沒有新資料：不重畫
            m_lastDataVersion = sum;
            requestFull();
        });
    }
    m_autoRefreshTimer->start(intervalMs);
}

int PlotWidget::autoRefreshInterval() const { return m_autoRefreshTimer ? m_autoRefreshTimer->interval() : 0; }

void PlotWidget::drawOverlay(QPainter& painter) {
    // 框選中的半透明矩形畫在最上層，不進 Plot 的渲染流程
    if (m_drag == DragMode::Rectangle) {
        const QRectF r = QRectF(m_rectStart, m_rectCurrent).normalized();
        painter.setPen(QPen(QColor(0, 120, 215), 1, Qt::DashLine));
        painter.setBrush(QBrush(QColor(0, 120, 215, 40)));
        painter.drawRect(r);
    }

    drawSelection(painter);
    if (showTooltipOnHover && m_hover.isHit()) drawTooltip(painter);

    if (showBenchmark) {
        const QString s = tr("%1 個圖元, %2 ms (%3 FPS)")
                              .arg(m_plot->plottableCount())
                              .arg(m_plot->lastRenderMs(), 0, 'f', 2)
                              .arg(m_plot->lastRenderMs() > 0 ? 1000.0 / m_plot->lastRenderMs() : 0.0,
                                   0, 'f', 0);
        const PixelRect d = m_plot->lastDataRect();
        Label lb;
        lb.text = s;
        lb.font.size = 9;
        lb.font.color = QColor(200, 0, 0);
        QSizeF sz = measureText(lb);
        QRectF box(d.left + 6, d.top + 6, sz.width() + 8, sz.height() + 4);
        painter.setPen(QPen(QColor(200, 0, 0, 120)));
        painter.setBrush(QBrush(QColor(255, 255, 255, 230)));
        painter.drawRect(box);
        drawText(painter, box.center(), lb, Qt::AlignHCenter | Qt::AlignVCenter);
    }
}

// ───────────────────────────── 滑鼠 ─────────────────────────────

void PlotWidget::mousePressEvent(QMouseEvent* event) {
    // 動畫中途又抓住圖：停在目前的位置，不要等它跑完再跳
    stopAnimation();
    const QPointF pos = event->position();
    m_dragStart = m_dragLast = pos;
    m_dragMoved = false;
    m_dragStartLimits = m_plot->axisLimits();
    // 動之前先把「現在」記成一步，倒回去才回得到按下去之前的視野
    pushViewHistory();

    const bool alt = event->modifiers() & Qt::AltModifier;

    // 圖例的點擊要最先處理：它畫在繪圖區「裡面」，不先攔下來的話
    // 這一下會被當成平移的起點，圖跟著游標跑。
    if (event->button() == Qt::LeftButton && legendToggleEnabled) {
        if (IPlottable* item = m_plot->legendPlottableAt(pos)) {
            item->isVisible = !item->isVisible;
            m_drag = DragMode::None;
            emit legendItemToggled(item, item->isVisible);
            requestFull();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        // 先問有沒有抓到可拖的圖元。抓到就拖圖元，不要變成平移 ——
        // 不然使用者想移動一條參考線，結果整張圖跟著跑。
        IPlottable* grabbed = nullptr;
        if (dragPlottablesEnabled && !alt && inDataArea(pos)) {
            const PixelRect d = m_plot->lastDataRect();
            grabbed = m_plot->grabbedPlottable(
                {m_plot->bottomAxis()->getCoordinate(pos.x(), d),
                 m_plot->leftAxis()->getCoordinate(pos.y(), d)});
        }
        if (selectionMode) {
            m_drag = DragMode::Select;
            m_rectStart = m_rectCurrent = pos;
        } else if (grabbed) {
            m_drag = DragMode::DragPlottable;
            m_dragging = grabbed;
            const PixelRect d = m_plot->lastDataRect();
            m_dragLastCoords = {m_plot->bottomAxis()->getCoordinate(pos.x(), d),
                                m_plot->leftAxis()->getCoordinate(pos.y(), d)};
            grabbed->beginDrag(m_dragLastCoords,
                               m_plot->transformFor(grabbed->xAxis, grabbed->yAxis), 6);
            setCursor(Qt::SizeAllCursor);
        } else if (alt && rectangleZoomEnabled) {
            m_drag = DragMode::Rectangle;
            m_rectStart = m_rectCurrent = pos;
        } else if (panEnabled) {
            m_drag = DragMode::Pan;
            setCursor(Qt::ClosedHandCursor);
        }
        if (inDataArea(pos)) {
            const PixelRect d = m_plot->lastDataRect();
            const Coordinates at{m_plot->bottomAxis()->getCoordinate(pos.x(), d),
                                 m_plot->leftAxis()->getCoordinate(pos.y(), d)};
            emit plotClicked(at);
            const HitResult hit = m_plot->hitTest(at, hitTolerancePx);
            if (hit.isHit()) emit pointClicked(hit);
        }
    } else if (event->button() == Qt::RightButton && zoomEnabled) {
        m_drag = DragMode::Zoom;
    } else if (event->button() == Qt::MiddleButton && rectangleZoomEnabled) {
        m_drag = DragMode::Rectangle;
        m_rectStart = m_rectCurrent = pos;
    }
    QWidget::mousePressEvent(event);
}

void PlotWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();

    // 游標的資料座標
    const PixelRect d = m_plot->lastDataRect();
    if (!d.isEmpty()) {
        m_cursorCoords = {m_plot->bottomAxis()->getCoordinate(pos.x(), d),
                          m_plot->leftAxis()->getCoordinate(pos.y(), d)};
        emit mouseMoved(m_cursorCoords);
    }

    // 通知想跟著游標的圖元（會吸附的十字線）
    if (!d.isEmpty()) {
        bool anyWants = false;
        for (IPlottable* p : m_plot->plottables()) {
            if (!p->isVisible || !p->wantsCursorUpdates()) continue;
            p->onCursorMoved(m_cursorCoords, *m_plot);
            anyWants = true;
        }
        if (anyWants) requestFull();
    }

    // 命中測試：只在沒有拖曳時做，拖曳中沒必要也會卡
    if (m_drag == DragMode::None && !d.isEmpty() && inDataArea(pos)) {
        const HitResult hit = (showTooltipOnHover || receivers(SIGNAL(pointHovered(qtplot::HitResult))) > 0)
                                  ? m_plot->hitTest(m_cursorCoords, hitTolerancePx)
                                  : HitResult();
        if (hit.index != m_hover.index || hit.plottable != m_hover.plottable) {
            m_hover = hit;
            emit pointHovered(m_hover);
            if (showTooltipOnHover) requestOverlay();
        } else {
            m_hover = hit;
        }
    }

    if (m_drag != DragMode::None) {
        if ((pos - m_dragStart).manhattanLength() > 2) m_dragMoved = true;
        switch (m_drag) {
        case DragMode::DragPlottable: {
            if (m_dragging && !d.isEmpty()) {
                const Coordinates now{m_plot->bottomAxis()->getCoordinate(pos.x(), d),
                                      m_plot->leftAxis()->getCoordinate(pos.y(), d)};
                m_dragging->dragBy({now.x - m_dragLastCoords.x, now.y - m_dragLastCoords.y});
                m_dragLastCoords = now;
                requestFull();
            }
            break;
        }
        case DragMode::Pan:
            applyPan(m_dragLast, pos, event->modifiers());
            m_dragLast = pos;
            pushAxesToLinked();
            requestFull();
            break;
        case DragMode::Zoom:
            applyZoomDrag(m_dragLast, pos, event->modifiers());
            m_dragLast = pos;
            requestFull();
            break;
        case DragMode::Rectangle:
        case DragMode::Select:
            m_rectCurrent = pos;
            requestOverlay();
            break;
        case DragMode::None:
            break;
        }
    }
    QWidget::mouseMoveEvent(event);
}

void PlotWidget::mouseReleaseEvent(QMouseEvent* event) {
    const QPointF pos = event->position();

    if (m_drag == DragMode::Select) {
        m_drag = DragMode::None;
        const PixelRect d = m_plot->lastDataRect();
        if (!d.isEmpty() && (pos - m_rectStart).manhattanLength() > 4) {
            Axis* bx = m_plot->bottomAxis();
            Axis* by = m_plot->leftAxis();
            const CoordinateRect rect(bx->getCoordinate(m_rectStart.x(), d),
                                      bx->getCoordinate(pos.x(), d),
                                      by->getCoordinate(pos.y(), d),
                                      by->getCoordinate(m_rectStart.y(), d));
            m_selected = m_plot->pointsInRect(rect);
            emit pointsSelected(m_selected);
        }
        requestOverlay();
        QWidget::mouseReleaseEvent(event);
        return;  // 框選不改變視野
    }

    if (m_drag == DragMode::Rectangle) {
        // 太小的框當成誤觸，不要把視野縮到一個點
        m_drag = DragMode::None;
        if ((pos - m_rectStart).manhattanLength() > 6) {
            // 先算出終點再倒回來，讓動畫從目前的視野走過去
            const ViewSnapshot before = captureView();
            applyRectangleZoom(m_rectStart, pos);
            const ViewSnapshot target = captureView();
            applyView(before);
            animateToView(target);
            m_dragMoved = false;  // 歷程與通知由動畫結束時處理
        }
        requestOverlay();
    } else if (m_drag == DragMode::Zoom) {
        m_drag = DragMode::None;
        // 右鍵沒有拖動 → 當成單擊，開內容選單
        if (!m_dragMoved && contextMenuEnabled) showContextMenu(event->pos());
    } else if (m_drag == DragMode::Pan) {
        m_drag = DragMode::None;
        unsetCursor();
    } else if (m_drag == DragMode::DragPlottable) {
        m_drag = DragMode::None;
        unsetCursor();
        if (m_dragging) m_dragging->endDrag();
        if (m_dragging && m_dragMoved) emit plottableDragged(m_dragging);
        m_dragging = nullptr;
        QWidget::mouseReleaseEvent(event);
        return;  // 拖圖元不算改變視野
    }

    if (m_dragMoved) {
        pushAxesToLinked();
        pushViewHistory();
        emit axisLimitsChanged();
    }
    QWidget::mouseReleaseEvent(event);
}

void PlotWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && doubleClickAutoScale) autoScaleAnimated();
    QWidget::mouseDoubleClickEvent(event);
}

void PlotWidget::wheelEvent(QWheelEvent* event) {
    if (!zoomEnabled) {
        QWidget::wheelEvent(event);
        return;
    }
    const double delta = event->angleDelta().y();
    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }
    stopAnimation();
    // 往前滾放大（視野變小），所以 factor < 1
    const double steps = delta / 120.0;
    const double factor = std::pow(1.0 / wheelZoomFactor, steps);
    // Windows 的精確式觸控板把兩指捏合送成「Ctrl + 捲動」。Ctrl 平常的意思是「只縮 Y」，
    // 但捏合時使用者要的是兩軸一起縮 —— 用輸入裝置的種類分辨。
    const bool touchpadPinch = gesturesEnabled && (event->modifiers() & Qt::ControlModifier) &&
                               event->device() &&
                               event->device()->type() == QInputDevice::DeviceType::TouchPad;
    // 滾輪是連續的，每一格都記一步會讓歷程被滾動洗掉。
    // 只在「開始滾」的時候記一次：連續滾動之間的間隔很短，用時間判斷。
    if (m_wheelTimer.isValid() && m_wheelTimer.elapsed() > 400) m_wheelTimer.invalidate();
    if (!m_wheelTimer.isValid()) pushViewHistory();
    m_wheelTimer.restart();
    zoomAt(event->position(), factor, touchpadPinch ? Qt::NoModifier : event->modifiers());
    pushAxesToLinked();
    emit axisLimitsChanged();
    requestFull();
    event->accept();
}

void PlotWidget::leaveEvent(QEvent* event) {
    m_cursorCoords = {std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::quiet_NaN()};
    if (m_hover.isHit()) {
        m_hover = HitResult();
        emit pointHovered(m_hover);
        if (showTooltipOnHover) requestOverlay();
    }
    QWidget::leaveEvent(event);
}

void PlotWidget::resetPlot() {
    stopAnimation();
    m_plot->reset();
    m_hover = HitResult();
    m_dragging = nullptr;
    m_drag = DragMode::None;
    m_dragMoved = false;
    m_selected.clear();
    m_cursorCoords = {std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::quiet_NaN()};
    // 換了圖，舊的視野快照對不上新的軸，留著只會套到錯的軸上
    clearViewHistory();
}

void PlotWidget::clearSelection() {
    if (m_selected.isEmpty()) return;
    m_selected.clear();
    requestOverlay();
}

void PlotWidget::drawSelection(QPainter& painter) {
    const PixelRect d = m_plot->lastDataRect();
    if (d.isEmpty()) return;

    painter.save();
    // 框選中的半透明矩形
    if (m_drag == DragMode::Select) {
        painter.setPen(selectionBorder.isVisible() ? selectionBorder.pen() : QPen(Qt::NoPen));
        painter.setBrush(selectionFill.brush());
        painter.drawRect(QRectF(m_rectStart, m_rectCurrent).normalized());
    }

    // 已選到的點。裁切到繪圖區，縮放之後選集跑出畫面才不會畫到軸上。
    if (!m_selected.isEmpty() && selectedMarker.isVisible()) {
        painter.setClipRect(d.toQRectF());
        QVector<QPointF> pts;
        pts.reserve(m_selected.size());
        for (const HitResult& h : m_selected) {
            const Transform t = m_plot->transformFor(h.plottable ? h.plottable->xAxis : nullptr,
                                                     h.plottable ? h.plottable->yAxis : nullptr);
            pts.append(t.p(h.point));
        }
        drawMarkers(painter, pts, selectedMarker);
    }
    painter.restore();
}

// ───────────────────────── 共用座標軸 ─────────────────────────

void PlotWidget::linkAxes(PlotWidget* other, bool shareX, bool shareY) {
    if (!other || other == this) return;
    auto addLink = [](PlotWidget* from, PlotWidget* to, bool sx, bool sy) {
        for (AxisLink& l : from->m_links) {
            if (l.widget == to) {
                l.shareX = sx;
                l.shareY = sy;
                return;
            }
        }
        from->m_links.append({to, sx, sy});
    };
    // 兩邊互綁，呼叫一次就夠 —— 只綁單向的話從另一邊操作不會同步，很難查
    addLink(this, other, shareX, shareY);
    addLink(other, this, shareX, shareY);
    pushAxesToLinked();
}

void PlotWidget::unlinkAxes(PlotWidget* other) {
    auto remove = [](PlotWidget* from, PlotWidget* to) {
        for (int i = from->m_links.size() - 1; i >= 0; --i)
            if (from->m_links[i].widget == to) from->m_links.removeAt(i);
    };
    remove(this, other);
    if (other) remove(other, this);
}

void PlotWidget::pushAxesToLinked() {
    if (m_links.isEmpty() || m_pushingAxes) return;
    // 互綁會造成 A 推給 B、B 再推回 A 的無限遞迴，用旗標擋掉
    m_pushingAxes = true;
    for (const AxisLink& l : m_links) {
        if (!l.widget) continue;
        Plot& other = l.widget->plot();
        if (l.shareX) {
            other.bottomAxis()->range = m_plot->bottomAxis()->range;
            other.topAxis()->range = m_plot->topAxis()->range;
        }
        if (l.shareY) {
            other.leftAxis()->range = m_plot->leftAxis()->range;
            other.rightAxis()->range = m_plot->rightAxis()->range;
        }
        l.widget->requestFull();
    }
    m_pushingAxes = false;
}

void PlotWidget::drawTooltip(QPainter& painter) {
    const PixelRect d = m_plot->lastDataRect();
    if (d.isEmpty()) return;

    const Transform t = m_plot->transformFor(
        m_hover.plottable ? m_hover.plottable->xAxis : nullptr,
        m_hover.plottable ? m_hover.plottable->yAxis : nullptr);
    const QPointF at = t.p(m_hover.point);

    Label lb;
    lb.text = tooltipFormatter ? tooltipFormatter(m_hover)
                               : QStringLiteral("x = %1\ny = %2")
                                     .arg(m_hover.point.x, 0, 'g', 6)
                                     .arg(m_hover.point.y, 0, 'g', 6);
    lb.font.size = 9;
    lb.font.color = m_plot->style.textColor;
    if (lb.text.isEmpty()) return;

    // 命中的點本身先標出來
    painter.save();
    painter.setPen(QPen(QColor(220, 60, 60), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(at, 6, 6);

    const QSizeF sz = measureText(lb);
    const double pad = 5;
    QRectF box(at.x() + 12, at.y() - sz.height() - 12, sz.width() + pad * 2, sz.height() + pad * 2);
    // 靠近邊界時翻到另一側，提示框才不會被繪圖區切掉
    if (box.right() > d.right) box.moveLeft(at.x() - 12 - box.width());
    if (box.top() < d.top) box.moveTop(at.y() + 12);
    if (box.bottom() > d.bottom) box.moveTop(d.bottom - box.height());
    if (box.left() < d.left) box.moveLeft(d.left);

    const bool dark = m_plot->style.figureBackground.lightness() < 128;
    painter.setBrush(QBrush(dark ? QColor(40, 44, 52, 240) : QColor(255, 255, 255, 240)));
    painter.setPen(QPen(dark ? QColor(255, 255, 255, 70) : QColor(0, 0, 0, 90)));
    painter.drawRoundedRect(box, 3, 3);
    drawText(painter, box.center(), lb, Qt::AlignHCenter | Qt::AlignVCenter);
    painter.restore();
}

// ───────────────────────── 互動實作 ─────────────────────────

void PlotWidget::applyPan(QPointF from, QPointF to, Qt::KeyboardModifiers mods) {
    const PixelRect d = m_plot->lastDataRect();
    if (d.isEmpty()) return;

    const bool onlyX = mods & Qt::ShiftModifier;
    const bool onlyY = mods & Qt::ControlModifier;

    Axis* bx = m_plot->bottomAxis();
    Axis* by = m_plot->leftAxis();

    if (!onlyY && !bx->locked) {
        const double dx = bx->getCoordinate(from.x(), d) - bx->getCoordinate(to.x(), d);
        bx->range.translate(dx);
        m_plot->topAxis()->range = bx->range;
    }
    if (!onlyX && !by->locked) {
        const double dy = by->getCoordinate(from.y(), d) - by->getCoordinate(to.y(), d);
        by->range.translate(dy);
        m_plot->rightAxis()->range = by->range;
    }
}

void PlotWidget::applyZoomDrag(QPointF from, QPointF to, Qt::KeyboardModifiers mods) {
    const PixelRect d = m_plot->lastDataRect();
    if (d.isEmpty()) return;

    const bool onlyX = mods & Qt::ShiftModifier;
    const bool onlyY = mods & Qt::ControlModifier;

    // 往右／往上拖 = 放大。每 100 像素約 2 倍。
    const double fx = std::pow(0.5, (to.x() - from.x()) / 100.0);
    const double fy = std::pow(0.5, (from.y() - to.y()) / 100.0);

    Axis* bx = m_plot->bottomAxis();
    Axis* by = m_plot->leftAxis();

    if (!onlyY && !bx->locked) {
        bx->range.zoom(fx, bx->range.center());
        m_plot->topAxis()->range = bx->range;
    }
    if (!onlyX && !by->locked) {
        by->range.zoom(fy, by->range.center());
        m_plot->rightAxis()->range = by->range;
    }
}

void PlotWidget::applyRectangleZoom(QPointF a, QPointF b) {
    const PixelRect d = m_plot->lastDataRect();
    if (d.isEmpty()) return;

    Axis* bx = m_plot->bottomAxis();
    Axis* by = m_plot->leftAxis();

    double x1 = bx->getCoordinate(std::min(a.x(), b.x()), d);
    double x2 = bx->getCoordinate(std::max(a.x(), b.x()), d);
    double y1 = by->getCoordinate(std::max(a.y(), b.y()), d);  // 像素 y 大 = 資料 y 小
    double y2 = by->getCoordinate(std::min(a.y(), b.y()), d);

    if (x2 <= x1 || y2 <= y1) return;
    if (!bx->locked) {
        bx->range = {x1, x2};
        m_plot->topAxis()->range = bx->range;
    }
    if (!by->locked) {
        by->range = {y1, y2};
        m_plot->rightAxis()->range = by->range;
    }
}

void PlotWidget::zoomAt(QPointF pixel, double factor, Qt::KeyboardModifiers mods) {
    const PixelRect d = m_plot->lastDataRect();
    if (d.isEmpty()) return;

    // 手勢縮放傳 NoModifier：捏合時系統自己加了 Ctrl 不該變成只縮一軸
    const bool onlyX = mods & Qt::ShiftModifier;
    const bool onlyY = mods & Qt::ControlModifier;

    Axis* bx = m_plot->bottomAxis();
    Axis* by = m_plot->leftAxis();

    // 以游標下的資料點為定點，縮放後該點仍在同一個像素上
    if (!onlyY && !bx->locked) {
        bx->range.zoom(factor, bx->getCoordinate(pixel.x(), d));
        m_plot->topAxis()->range = bx->range;
    }
    if (!onlyX && !by->locked) {
        by->range.zoom(factor, by->getCoordinate(pixel.y(), d));
        m_plot->rightAxis()->range = by->range;
    }
}

// ───────────────────────── 內容選單 ─────────────────────────

// ───────────────────────────── 視野歷程 ─────────────────────────────

PlotWidget::ViewSnapshot PlotWidget::captureView() const {
    ViewSnapshot v;
    for (const Axis* a : m_plot->axes()) v.append(a->range);
    return v;
}

void PlotWidget::applyView(const ViewSnapshot& view) {
    const QVector<Axis*>& axes = m_plot->axes();
    // 軸的數量對不上就不套用。換過圖之後舊的快照毫無意義，
    // 硬套只會把範圍塞到錯的軸上。
    if (view.size() != axes.size()) return;
    for (int i = 0; i < axes.size(); ++i) axes[i]->range = view[i];
}

void PlotWidget::pushViewHistory() {
    if (!viewHistoryEnabled) return;
    const ViewSnapshot now = captureView();
    if (m_viewIndex >= 0 && m_viewIndex < m_viewHistory.size() &&
        m_viewHistory[m_viewIndex] == now)
        return;  // 沒變就不記，否則連點幾下會塞進一堆一樣的

    // 倒回去之後又做了新的操作：把前面的分支丟掉（和編輯器的 undo 一樣）
    while (m_viewHistory.size() > m_viewIndex + 1) m_viewHistory.removeLast();
    m_viewHistory.append(now);
    while (m_viewHistory.size() > std::max(2, viewHistoryLimit)) m_viewHistory.removeFirst();
    m_viewIndex = static_cast<int>(m_viewHistory.size()) - 1;
}

void PlotWidget::clearViewHistory() {
    m_viewHistory.clear();
    m_viewIndex = -1;
}

void PlotWidget::undoView() {
    if (!canUndoView()) return;
    // 第一次倒回之前要先把「現在」記起來，否則重做就回不來了
    if (m_viewIndex == m_viewHistory.size() - 1) {
        const ViewSnapshot now = captureView();
        if (now != m_viewHistory[m_viewIndex]) {
            m_viewHistory.append(now);
            m_viewIndex = static_cast<int>(m_viewHistory.size()) - 1;
        }
    }
    --m_viewIndex;
    animateToView(m_viewHistory[m_viewIndex]);
}

void PlotWidget::redoView() {
    if (!canRedoView()) return;
    ++m_viewIndex;
    animateToView(m_viewHistory[m_viewIndex]);
}

void PlotWidget::keyPressEvent(QKeyEvent* event) {
    if (viewHistoryEnabled && (event->modifiers() & Qt::ControlModifier)) {
        if (event->key() == Qt::Key_Z) {
            undoView();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Y) {
            redoView();
            event->accept();
            return;
        }
    }
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_P) {
        printPreviewDialog();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PlotWidget::showContextMenu(QPoint pos) {
    QMenu menu(this);
    menu.addAction(tr("自動範圍(&A)"), this, [this] { autoScaleAnimated(); });
    menu.addSeparator();
    menu.addAction(tr("複製圖片(&C)"), this, [this] { copyToClipboard(); });
    menu.addAction(tr("另存圖片(&S)…"), this, [this] { saveImageDialog(); });
    menu.addAction(tr("列印(&P)…"), this, [this] { printPreviewDialog(); });
    menu.addSeparator();

    QAction* grid = menu.addAction(tr("顯示格線(&G)"));
    grid->setCheckable(true);
    grid->setChecked(m_plot->grid.isVisible);
    connect(grid, &QAction::toggled, this, [this](bool on) {
        m_plot->grid.isVisible = on;
        requestFull();
    });

    QAction* leg = menu.addAction(tr("顯示圖例(&L)"));
    leg->setCheckable(true);
    leg->setChecked(m_plot->legend.isVisible);
    connect(leg, &QAction::toggled, this, [this](bool on) {
        m_plot->legend.isVisible = on;
        requestFull();
    });

    if (languageMenuEnabled) {
        QMenu* langMenu = menu.addMenu(tr("語言(&N)"));
        auto* group = new QActionGroup(langMenu);
        auto addLang = [&](const QString& label, const QString& code, bool checked) {
            QAction* a = langMenu->addAction(label);
            a->setCheckable(true);
            a->setChecked(checked);
            group->addAction(a);
            connect(a, &QAction::triggered, this, [this, code] {
                setLanguage(code);
                // 其他 widget 會收到 LanguageChange 自己重畫；這張保險起見直接重畫
                requestFull();
            });
        };
        const bool followSystem = language().isEmpty();
        addLang(tr("跟隨系統"), QString(), followSystem);
        langMenu->addSeparator();
        for (const QString& code : availableLanguages())
            addLang(languageDisplayName(code), code, !followSystem && effectiveLanguage() == code);
    }

    QAction* bench = menu.addAction(tr("顯示效能(&B)"));
    bench->setCheckable(true);
    bench->setChecked(showBenchmark);
    connect(bench, &QAction::toggled, this, [this](bool on) {
        showBenchmark = on;
        requestFull();
    });

    menu.exec(mapToGlobal(pos));
}

void PlotWidget::copyToClipboard() {
    QImage img = m_plot->renderToImage(width(), height(), 2.0);
    QApplication::clipboard()->setImage(img);
}

void PlotWidget::saveImageDialog() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("另存圖片"), QStringLiteral("plot.png"),
        tr("PNG 圖片 (*.png);;SVG 向量圖 (*.svg);;PDF 文件 (*.pdf);;JPEG 圖片 (*.jpg)"));
    if (path.isEmpty()) return;

    bool ok;
    if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        ok = m_plot->saveSvg(path, width(), height());
    } else if (path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        ok = m_plot->savePdf(path, width(), height());
    } else {
        // 螢幕解析度存出來會太糊，固定放大兩倍
        ok = m_plot->renderToImage(width(), height(), 2.0).save(path);
    }
    if (!ok) QMessageBox::warning(this, tr("另存圖片"), tr("寫入失敗：%1").arg(path));
}

// ───────────────────────────── 動畫 ─────────────────────────────

bool PlotWidget::isAnimating() const {
    return m_viewAnim && m_viewAnim->state() == QAbstractAnimation::Running;
}

void PlotWidget::stopAnimation() {
    if (isAnimating()) m_viewAnim->stop();  // stop() 不會發 finished，視野就停在目前這一格
}

void PlotWidget::animateToView(const ViewSnapshot& target) {
    stopAnimation();
    const ViewSnapshot from = captureView();
    // 沒顯示出來（離屏、測試）或關掉動畫：直接跳過去，結果和有動畫時跑完一樣
    if (animationDurationMs <= 0 || !isVisible() || from.size() != target.size() || from == target) {
        applyView(target);
        pushAxesToLinked();
        pushViewHistory();
        emit axisLimitsChanged();
        requestFull();
        return;
    }
    m_animFrom = from;
    m_animTo = target;
    m_viewAnim->setDuration(animationDurationMs);
    m_viewAnim->start();
}

void PlotWidget::animateTo(const AxisLimits& limits) {
    pushViewHistory();
    const ViewSnapshot before = captureView();
    m_plot->setAxisLimits(limits);
    // 上／右軸跟著主要兩軸，和平移縮放的行為一致
    m_plot->topAxis()->range = m_plot->bottomAxis()->range;
    m_plot->rightAxis()->range = m_plot->leftAxis()->range;
    const ViewSnapshot target = captureView();
    applyView(before);
    animateToView(target);
}

void PlotWidget::autoScaleAnimated() {
    pushViewHistory();
    const ViewSnapshot before = captureView();
    m_plot->autoScale(0.05, true);
    const ViewSnapshot target = captureView();
    applyView(before);
    animateToView(target);
}

// ───────────────────────────── 語言 ─────────────────────────────

void PlotWidget::changeEvent(QEvent* event) {
    // 換語言之後重畫：Plot 在 render 時會把自動填的軸名重翻
    if (event->type() == QEvent::LanguageChange) requestFull();
    QWidget::changeEvent(event);
}

// ───────────────────────────── 手勢 ─────────────────────────────

bool PlotWidget::event(QEvent* event) {
    if (gesturesEnabled) {
        switch (event->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            if (touchEvent(static_cast<QTouchEvent*>(event))) return true;
            break;
        default:
            break;
        }
        if (event->type() == QEvent::NativeGesture &&
            nativeGestureEvent(static_cast<QNativeGestureEvent*>(event)))
            return true;
    }
    return QWidget::event(event);
}

void PlotWidget::beginGesture() {
    if (m_gestureActive) return;
    m_gestureActive = true;
    stopAnimation();
    // 滑鼠拖到一半又用手勢：放掉滑鼠那一邊，不然兩邊同時作用，圖亂跳
    if (m_drag != DragMode::None) {
        m_drag = DragMode::None;
        unsetCursor();
    }
    pushViewHistory();
}

void PlotWidget::endGesture() {
    if (!m_gestureActive) return;
    m_gestureActive = false;
    pushAxesToLinked();
    pushViewHistory();
    emit axisLimitsChanged();
}

bool PlotWidget::touchEvent(QTouchEvent* event) {
    // 觸控自己處理，不交給 Qt 合成滑鼠事件：接下 TouchBegin 之後 Qt 就不再合成，
    // 不接的話又收不到第二根手指。所以單指平移、輕點、兩指捏合都在這裡做。
    QVector<QPointF> pts;
    for (const QEventPoint& p : event->points())
        if (p.state() != QEventPoint::Released) pts.append(p.position());

    const PixelRect d = m_plot->lastDataRect();
    switch (event->type()) {
    case QEvent::TouchBegin:
        beginGesture();
        m_touchStart = pts.isEmpty() ? QPointF() : pts.first();
        m_touchMoved = false;
        m_touchCount = 0;
        [[fallthrough]];
    case QEvent::TouchUpdate: {
        if (pts.isEmpty() || d.isEmpty()) break;
        const bool two = pts.size() >= 2;
        const QPointF center = two ? (pts[0] + pts[1]) / 2 : pts[0];
        const double spread = two ? QLineF(pts[0], pts[1]).length() : 0;
        const int count = two ? 2 : 1;
        if (count == m_touchCount) {
            if (panEnabled) applyPan(m_touchCenter, center, Qt::NoModifier);
            if (two && zoomEnabled && spread > 1 && m_touchSpread > 1)
                zoomAt(center, m_touchSpread / spread, Qt::NoModifier);  // 張開 = 放大 = 視野變小
            if ((center - m_touchStart).manhattanLength() > 8 || two) m_touchMoved = true;
            pushAxesToLinked();
            requestFull();
        }
        m_touchCount = count;
        m_touchCenter = center;
        m_touchSpread = spread;
        break;
    }
    case QEvent::TouchEnd:
    case QEvent::TouchCancel: {
        endGesture();
        m_touchCount = 0;
        if (event->type() == QEvent::TouchEnd && !m_touchMoved) {
            // 輕點：和滑鼠單擊一樣可以切換圖例；連點兩下是自動範圍
            const QPointF at = m_touchStart;
            if (legendToggleEnabled) {
                if (IPlottable* item = m_plot->legendPlottableAt(at)) {
                    item->isVisible = !item->isVisible;
                    emit legendItemToggled(item, item->isVisible);
                }
            }
            const bool doubleTap = m_lastTap.isValid() &&
                                   m_lastTap.elapsed() < QApplication::doubleClickInterval() &&
                                   (at - m_lastTapPos).manhattanLength() < 30;
            if (doubleTap && doubleClickAutoScale) {
                autoScaleAnimated();
                m_lastTap.invalidate();
            } else {
                m_lastTap.restart();
                m_lastTapPos = at;
            }
        }
        requestFull();
        break;
    }
    default:
        return false;
    }
    event->accept();
    return true;
}

bool PlotWidget::nativeGestureEvent(QNativeGestureEvent* event) {
    switch (event->gestureType()) {
    case Qt::BeginNativeGesture:
        beginGesture();
        return true;
    case Qt::EndNativeGesture:
        endGesture();
        return true;
    case Qt::ZoomNativeGesture: {
        if (!zoomEnabled) return false;
        // value 是這一步的相對變化量：+0.1 代表放大 10%
        const double scale = 1.0 + event->value();
        if (scale <= 0) return true;
        const bool standalone = !m_gestureActive;  // 有些平台不送 Begin／End
        if (standalone) beginGesture();
        zoomAt(event->position(), 1.0 / scale, Qt::NoModifier);
        if (standalone) endGesture();
        pushAxesToLinked();
        requestFull();
        return true;
    }
    case Qt::SmartZoomNativeGesture:
        // macOS 的兩指輕點兩下：和滑鼠雙擊一樣當成自動範圍
        if (doubleClickAutoScale) autoScaleAnimated();
        return true;
    default:
        return false;
    }
}

// ───────────────────────────── 列印 ─────────────────────────────

void PlotWidget::print(QPrinter& printer) {
    QPainter painter(&printer);
    if (!painter.isActive()) return;

    // 版面照螢幕上的大小排，再等比放大到紙上。直接用紙張的像素數排版的話，
    // 600 dpi 的印表機上字只有 11 個裝置像素高 —— 小到看不見。
    const QSizeF logical(std::max(1, width()), std::max(1, height()));
    const QRectF page = printer.pageLayout().paintRectPixels(printer.resolution());
    const double scale = std::min(page.width() / logical.width(), page.height() / logical.height());
    painter.translate((page.width() - logical.width() * scale) / 2,
                      (page.height() - logical.height() * scale) / 2);
    painter.scale(scale, scale);
    m_plot->render(painter, QRectF(QPointF(0, 0), logical));
    painter.end();
    // render() 會更新 lastDataRect；用的是螢幕上的尺寸，所以互動的座標換算不受影響
}

namespace {
// 寬的圖印橫向、高的圖印直向，紙張才用得滿
void matchOrientation(QPrinter& printer, const QWidget* w) {
    printer.setPageOrientation(w->width() >= w->height() ? QPageLayout::Landscape : QPageLayout::Portrait);
}
}  // namespace

void PlotWidget::printPreviewDialog() {
    QPrinter printer(QPrinter::HighResolution);
    matchOrientation(printer, this);
    QPrintPreviewDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("預覽列印"));
    connect(&dlg, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter* p) { print(*p); });
    dlg.exec();
}

void PlotWidget::printDialog() {
    QPrinter printer(QPrinter::HighResolution);
    matchOrientation(printer, this);
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("列印"));
    if (dlg.exec() == QDialog::Accepted) print(printer);
}

}  // namespace qtplot

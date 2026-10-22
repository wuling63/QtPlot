// PlotWidget.h — 把 Plot 掛到 QWidget 上，加上滑鼠互動。
//
// 操作沿用 ScottPlot 的習慣，換過去的人不用重新學：
//   左鍵拖曳      平移
//   右鍵拖曳      縮放（水平拖動縮 X、垂直拖動縮 Y）
//   中鍵拖曳      框選縮放（Alt + 左鍵同效）
//   滾輪          以游標為中心縮放
//   左鍵雙擊      自動範圍
//   右鍵單擊      內容選單
//   Shift / Ctrl  拖曳時只動 X / 只動 Y
//   Ctrl + P      預覽列印
//
// 觸控螢幕：單指拖曳平移、兩指捏合縮放（同時拖動就順便平移）、輕點切換圖例、點兩下自動範圍。
// 觸控板：兩指捏合縮放（macOS／Linux 的原生手勢，Windows 的 Ctrl + 捲動）。
#pragma once

#include "Plot.h"

#include <QElapsedTimer>
#include <QWidget>

class QNativeGestureEvent;
class QTouchEvent;
class QPrinter;
class QVariantAnimation;

namespace qtplot {

class PlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit PlotWidget(QWidget* parent = nullptr);
    ~PlotWidget() override;

    Plot& plot() { return *m_plot; }
    const Plot& plot() const { return *m_plot; }

    /// 重畫（改完資料或樣式後呼叫）。
    ///
    /// 請用這個，不要直接呼叫 QWidget::update()：開著分層快取時，widget 自己只會在
    /// 「只有提示框、框選框在動」的時候沿用快取，其他一律整張重畫 —— 但 update() 剛好和
    /// 滑鼠移動擠在同一幀時，分不出這次是誰要的，可能會晚一幀才看到新資料。
    void refresh();

    /// 分層快取：圖本身畫成一張點陣圖存起來，滑鼠移過去只變提示框、拖框選框時，
    /// 直接貼快取再疊上這些東西，不必把整張圖（可能是上千萬點）重畫一次。
    bool layerCacheEnabled = true;

    /// 定時檢查即時資料（DataStreamer／DataLogger）有沒有新進來，有才重畫。0 代表關掉。
    ///
    /// 資料從背景執行緒（擷取卡、相機）餵進來時用：背景執行緒不能碰 widget，
    /// 改由 GUI 執行緒每隔 intervalMs 看一次 IPlottable::dataVersion()。
    /// 沒有新資料時不會重畫，不會白白吃 CPU。
    void setAutoRefresh(int intervalMs);
    int autoRefreshInterval() const;

    // ── 互動開關 ──
    bool panEnabled = true;
    bool zoomEnabled = true;
    bool rectangleZoomEnabled = true;
    bool contextMenuEnabled = true;
    bool doubleClickAutoScale = true;
    /// 滾輪每一格的縮放倍率（> 1）。
    double wheelZoomFactor = 1.15;
    /// 觸控螢幕的兩指捏合縮放、觸控板的捏合手勢。
    bool gesturesEnabled = true;

    /// 視野切換的動畫長度（毫秒）。0 代表直接跳過去。
    ///
    /// 只有「一下子跳很遠」的操作會動畫：自動範圍、框選縮放、復原／重做。
    /// 平移與滾輪縮放是手直接在操作，加動畫只會覺得跟不上手，所以不加。
    /// widget 沒有顯示在畫面上時（離屏測試、匯出）一律直接跳過去。
    int animationDurationMs = 250;
    /// 動畫是否進行中。
    bool isAnimating() const;
    /// 平滑地切換到指定的視野（主要兩軸）。
    void animateTo(const AxisLimits& limits);
    /// 自動範圍，有動畫。
    void autoScaleAnimated();

    /// 顯示左上角的渲染耗時（偵錯用）。
    bool showBenchmark = false;

    /// 點圖例切換該筆資料的顯示。
    ///
    /// 被關掉的那一列會畫成灰色加刪除線而不是消失 —— 消失的話就再也點不回來了
    /// （靠 Legend::showHiddenSeries 做到，預設是開的）。
    bool legendToggleEnabled = true;

    // ── 視野歷程 ──
    /// 記住視野的變化，可以倒回去。
    ///
    /// 縮過頭之後只能按「自動範圍」是很惱人的：那會跳回整體，
    /// 而使用者想回的是上一個視野。Ctrl + Z 倒回、Ctrl + Y 重做。
    bool viewHistoryEnabled = true;
    /// 最多記幾步。
    int viewHistoryLimit = 50;

    bool canUndoView() const { return m_viewIndex > 0; }
    bool canRedoView() const { return m_viewIndex + 1 < m_viewHistory.size(); }
    /// 回到上一個視野。沒有上一步時什麼都不做。
    void undoView();
    /// 再往前一步。
    void redoView();
    /// 把目前的視野記成一步。平移／縮放結束時會自動呼叫。
    void pushViewHistory();
    void clearViewHistory();

    // ── 命中與拖曳 ──
    /// 游標靠近資料點時，在旁邊顯示該點的數值。
    bool showTooltipOnHover = false;
    /// 命中的容許距離（像素）。超過這個距離就當作沒指到任何點。
    double hitTolerancePx = 20;
    /// 可以用滑鼠拖動設了 draggable 的圖元。開著時，抓到圖元的拖曳不會變成平移。
    bool dragPlottablesEnabled = true;
    /// 自訂提示文字。留空則用「x = …, y = …」。
    std::function<QString(const qtplot::HitResult&)> tooltipFormatter;

    // ── 框選 ──
    /// 左鍵拖曳改成「框選資料點」而不是平移。
    bool selectionMode = false;
    /// 框選的外觀。
    FillStyle selectionFill{QColor(255, 120, 120, 70)};
    LineStyle selectionBorder{QColor(230, 120, 60), 1.5};
    /// 被選到的點的標記。
    MarkerStyle selectedMarker{MarkerShape::OpenCircle, 11, QColor(230, 60, 60)};
    /// 目前選到的點。
    const QVector<HitResult>& selectedPoints() const { return m_selected; }
    void clearSelection();

    /// 把圖倒回全新狀態，順便丟掉 widget 這一側快取的指標。
    ///
    /// 要在同一個 widget 上換一張圖就用這個，不要直接呼叫 plot().reset()：
    /// 懸停、拖曳、框選記下的是 IPlottable*，reset() 之後那些圖元已經不存在，
    /// 下一次 paintEvent 會去碰已經釋放的記憶體。
    void resetPlot();

    // ── 和另一張圖共用座標軸 ──
    /// 把這張圖的座標軸和另一個 PlotWidget 綁在一起：一邊平移縮放，另一邊跟著動。
    /// 兩邊都要呼叫（或呼叫一次，內部會自動互綁）。
    void linkAxes(PlotWidget* other, bool shareX = true, bool shareY = false);
    void unlinkAxes(PlotWidget* other);

    /// 目前游標命中的資料點（沒命中時 isHit() 為 false）。
    HitResult hoveredPoint() const { return m_hover; }

    /// 游標位置的資料座標。滑鼠不在繪圖區時回傳的 isFinite() 為 false。
    Coordinates cursorCoordinates() const { return m_cursorCoords; }

    /// 把目前的圖存成 PNG（跳出檔案對話框）。
    void saveImageDialog();
    /// 把目前的圖複製到剪貼簿。
    void copyToClipboard();
    /// 跳出圖例樣式對話框（外框、底色、符號塗滿與否、字型），改了馬上看得到。
    /// 圖例原本關著的話會先打開。
    void legendStyleDialog();

    // ── 列印 ──
    /// 印到指定的印表機（或 PDF 印表機）。
    ///
    /// 走向量輸出：字和線在任何解析度下都是銳利的。版面照「螢幕上看到的大小」排，
    /// 再等比放大到紙張的可列印範圍並置中，所以印出來和螢幕上的比例一樣，
    /// 不會因為紙張比較高就把圖拉長。
    void print(QPrinter& printer);
    /// 跳出預覽列印對話框（含「列印」按鈕）。
    void printPreviewDialog();
    /// 跳出系統的列印對話框。
    void printDialog();

    // ── 語言 ──
    /// 右鍵選單裡放「語言」子選單，讓使用者自己切換。
    ///
    /// 換的是整個程式裡 QtPlot 的語言（翻譯器是全域的），所有圖會一起換。
    /// 宿主程式自己管語言的話就關掉，改呼叫 qtplot::setLanguage()。
    bool languageMenuEnabled = true;

signals:
    /// 滑鼠在繪圖區移動（資料座標）。
    void mouseMoved(qtplot::Coordinates coords);
    /// 游標命中的資料點改變了（包含從有到無，此時 isHit() 為 false）。
    void pointHovered(qtplot::HitResult hit);
    /// 使用者點到某個資料點。
    void pointClicked(qtplot::HitResult hit);
    /// 框選完成。
    void pointsSelected(const QVector<qtplot::HitResult>& points);
    /// 拖曳圖元結束。
    void plottableDragged(qtplot::IPlottable* plottable);
    /// 使用者按下繪圖區（資料座標）。
    void plotClicked(qtplot::Coordinates coords);
    /// 視野改變（平移、縮放、自動範圍）。
    void axisLimitsChanged();
    /// 使用者點了圖例切換某筆資料的顯示。
    void legendItemToggled(qtplot::IPlottable* plottable, bool nowVisible);

protected:
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override { return QSize(640, 480); }

private:
    enum class DragMode { None, Pan, Zoom, Rectangle, DragPlottable, Select };
    void drawTooltip(QPainter& painter);
    void drawSelection(QPainter& painter);
    /// 疊在圖上面、不屬於 Plot 的東西：框選框、選到的點、提示框、效能數字。
    void drawOverlay(QPainter& painter);

    /// 圖本身變了：下次整張重畫。
    void requestFull();
    /// 只有疊在上面的東西變了：下次可以沿用快取。
    void requestOverlay();
    /// 快取還能不能用的指紋：每根軸的範圍、圖元數、各圖元的顯示狀態與資料版本。
    /// 只是保險 —— 主要靠 requestFull()／requestOverlay() 分辨。
    QVector<double> sceneFingerprint() const;
    void renderCache();

    QImage m_cache;
    QVector<double> m_cacheFingerprint;
    bool m_overlayOnly = false;
    bool m_fullPending = true;
    class QTimer* m_autoRefreshTimer = nullptr;
    quint64 m_lastDataVersion = 0;
    /// 把目前的軸範圍推給綁在一起的其他圖。
    void pushAxesToLinked();

    // mods 是這個事件當下的修飾鍵（Shift 只動 X、Ctrl 只動 Y）。
    // 要用事件帶的，不能問 QApplication::keyboardModifiers()：那是「現在」鍵盤的狀態，
    // 和事件發生時不一定一樣，合成的事件（觸控板、測試）也不會反映在上面。
    void applyPan(QPointF from, QPointF to, Qt::KeyboardModifiers mods);
    void applyZoomDrag(QPointF from, QPointF to, Qt::KeyboardModifiers mods);
    void applyRectangleZoom(QPointF a, QPointF b);
    void zoomAt(QPointF pixel, double factor, Qt::KeyboardModifiers mods);
    void showContextMenu(QPoint pos);

    bool nativeGestureEvent(QNativeGestureEvent* event);
    bool touchEvent(QTouchEvent* event);
    /// 手勢開始：停掉動畫、取消進行中的滑鼠拖曳、記一步視野。
    void beginGesture();
    void endGesture();
    bool inDataArea(QPointF pixel) const;

    /// 一步視野：每根軸的範圍。
    ///
    /// 只存主要四根是不夠的 —— addAxis() 加出來的軸也會被平移縮放動到，
    /// 漏掉的話倒回去之後那幾根軸還停在原處，資料和刻度就對不上了。
    using ViewSnapshot = QVector<CoordinateRange>;
    ViewSnapshot captureView() const;
    void applyView(const ViewSnapshot& view);
    /// 平滑地切換到 target。結束時記一步視野並發出 axisLimitsChanged。
    void animateToView(const ViewSnapshot& target);
    /// 停在目前的位置（使用者在動畫中途又開始操作時）。
    void stopAnimation();

    QVariantAnimation* m_viewAnim = nullptr;
    ViewSnapshot m_animFrom;
    ViewSnapshot m_animTo;
    bool m_gestureActive = false;
    // 觸控：上一次的手指數、兩指中點與距離。手指數一變就重新取基準，
    // 否則第二根手指放下的那一瞬間，中點會從第一根手指的位置跳到兩指中間 —— 圖跟著跳。
    int m_touchCount = 0;
    QPointF m_touchCenter;
    double m_touchSpread = 0;
    QPointF m_touchStart;
    bool m_touchMoved = false;
    QElapsedTimer m_lastTap;
    QPointF m_lastTapPos;

    std::unique_ptr<Plot> m_plot;
    QVector<ViewSnapshot> m_viewHistory;
    int m_viewIndex = -1;
    /// 用來判斷滾輪是不是「同一次連續滾動」。
    QElapsedTimer m_wheelTimer;
    DragMode m_drag = DragMode::None;
    QPointF m_dragStart;
    QPointF m_dragLast;
    QPointF m_rectStart;
    QPointF m_rectCurrent;
    bool m_dragMoved = false;
    AxisLimits m_dragStartLimits;
    Coordinates m_cursorCoords{std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::quiet_NaN()};
    HitResult m_hover;
    IPlottable* m_dragging = nullptr;
    Coordinates m_dragLastCoords;
    QVector<HitResult> m_selected;
    struct AxisLink {
        PlotWidget* widget = nullptr;
        bool shareX = true;
        bool shareY = false;
    };
    QVector<AxisLink> m_links;
    /// 防止互相推送造成無限遞迴。
    bool m_pushingAxes = false;
};

}  // namespace qtplot

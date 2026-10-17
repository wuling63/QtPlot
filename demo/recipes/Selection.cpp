// Selection.cpp — 視窗層級的互動範例：游標吸附、框選、拖曳。
//
// 這幾支用 QTPLOT_RECIPE_WIDGET，拿到的是 PlotWidget 而不是 Plot ——
// 因為吸附、框選、拖曳都是「視窗」的行為，不是「圖」的設定。
// 匯出的靜態圖只看得到初始狀態，要在 Cookbook 裡用滑鼠試。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

QTPLOT_RECIPE_WIDGET("互動", "命中與提示", "游標吸附到資料點",
                     "十字線跟著游標跑，並吸附到最近的資料點 —— "
                     "游標停在兩點之間讀到的是內插出來的假值，吸附之後讀到的才是真資料。"
                     "左上角顯示 Index / X / Y。snapTolerancePx 超過就不吸附，"
                     "回到跟著游標，不會硬吸到一個離很遠的點。") {
    Plot& plot = widget.plot();
    plot.addSignal(gen::randomWalk(1000), 1.0)->legendText = QStringLiteral("訊號");

    auto* cross = plot.addCrosshair(0, 0);
    cross->followCursor = true;
    cross->snapToData = true;
    cross->snapMode = HitMode::NearestXY;
    cross->snapTolerancePx = 60;
    cross->showReadout = true;
    cross->showLabels = false;

    plot.legend.isVisible = true;
    plot.addAnnotation(QStringLiteral("把游標移到線上"), Qt::AlignRight | Qt::AlignTop);
}

QTPLOT_RECIPE_WIDGET("互動", "命中與提示", "只比 X 的吸附",
                     "NearestX 只比 X，游標的垂直位置不影響結果。"
                     "沿著曲線掃過去時抓到的一定是「這個 X 上的那一點」，"
                     "不會因為手抖跳到鄰近的點 —— 時間序列用這個比較順。"
                     "NearestXY 則是二維距離最近，散點圖用那個比較直覺。") {
    Plot& plot = widget.plot();
    gen::setSeed(4);
    plot.addSignal(gen::randomWalk(400), 1.0)->legendText = QStringLiteral("訊號");

    auto* cross = plot.addCrosshair(0, 0);
    cross->followCursor = true;
    cross->snapToData = true;
    cross->snapMode = HitMode::NearestX;  // ← 和上一支的差別只在這裡
    cross->showReadout = true;
    cross->showLabels = false;
    cross->showHorizontal = false;  // 只留垂直線，更像「掃描游標」

    plot.legend.isVisible = true;
}

QTPLOT_RECIPE_WIDGET("互動", "命中與提示", "多組資料的吸附",
                     "命中測試會掃過所有圖元，所以三組散點都吸附得到。"
                     "readout 顯示的是吸附到的那一點的索引與座標；"
                     "要知道是哪一組，接 PlotWidget::pointHovered 訊號看 hit.plottable。") {
    Plot& plot = widget.plot();
    gen::setSeed(12);
    for (int i = 0; i < 3; ++i) {
        auto* m = plot.addMarkers(gen::randomUniform(40, 0, 1), gen::randomUniform(40, 0, 1));
        m->legendText = QStringLiteral("Scatter %1").arg(i);
    }
    auto* cross = plot.addCrosshair(0, 0);
    cross->followCursor = true;
    cross->snapToData = true;
    cross->showReadout = true;
    cross->showLabels = false;
    cross->lineStyle = LineStyle(QColor(80, 140, 80, 200), 1.0);
    cross->snapMarker = MarkerStyle(MarkerShape::OpenCircle, 15, QColor(40, 120, 40));

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
}

QTPLOT_RECIPE_WIDGET("互動", "框選", "框選資料點",
                     "selectionMode 把左鍵拖曳從「平移」改成「框選」。"
                     "放開之後 selectedPoints() 會列出框內的所有點，"
                     "並發出 pointsSelected 訊號。選到的點會套上標記。"
                     "框選時不會改變視野 —— 選完還在原本的位置，才接得下去做別的事。") {
    Plot& plot = widget.plot();
    gen::setSeed(31);
    plot.addMarkers(gen::randomUniform(150, 0, 1), gen::randomUniform(150, 0, 1));

    widget.selectionMode = true;
    widget.showTooltipOnHover = false;

    plot.addAnnotation(QStringLiteral("按住左鍵拖曳框選"), Qt::AlignLeft | Qt::AlignTop);
    plot.setAxisLimits(-0.05, 1.05, -0.05, 1.05);
}

QTPLOT_RECIPE_WIDGET("互動", "框選", "框選多組資料",
                     "框選會掃過所有圖元，所以一次可以跨多組資料。"
                     "回傳的每一筆都帶著是哪個圖元、第幾筆，"
                     "拿去做後續處理（刪除、標記、匯出）時分得出來源。") {
    Plot& plot = widget.plot();
    gen::setSeed(77);
    for (int i = 0; i < 3; ++i)
        plot.addMarkers(gen::randomUniform(60, 0, 10), gen::randomUniform(60, 0, 10))
            ->legendText = QStringLiteral("第 %1 組").arg(i + 1);

    widget.selectionMode = true;
    widget.selectedMarker = MarkerStyle(MarkerShape::OpenSquare, 13, QColor(30, 30, 30));
    widget.selectionFill = FillStyle(QColor(80, 160, 220, 60));
    widget.selectionBorder = LineStyle(QColor(40, 110, 180), 1.5);

    plot.legend.isVisible = true;
}

QTPLOT_RECIPE_WIDGET("互動", "拖曳", "各種可拖的圖元",
                     "參考線、區間、文字、箭頭、矩形、橢圓、多邊形都可以設 draggable。"
                     "抓到圖元時拖曳不會變成平移；後加入的畫在上層，所以命中是反向找的，"
                     "視覺上在最上面的優先抓到。") {
    Plot& plot = widget.plot();
    plot.addSignal(gen::randomWalk(300, 1.0, 0));

    auto* line = plot.addHorizontalLine(10, QColor(214, 39, 40));
    line->draggable = true;
    line->line.pattern = LinePattern::Dashed;
    line->text.text = QStringLiteral("門檻");
    line->text.font.color = QColor(214, 39, 40);

    auto* span = plot.addVerticalSpan(60, 110, QColor(80, 140, 220));
    span->draggable = true;

    auto* rect = plot.addRectangle(160, 220, -10, 5);
    rect->draggable = true;

    auto* ellipse = plot.addEllipse(250, 8, 25, 6);
    ellipse->draggable = true;

    auto* arrow = plot.addArrow(200, -15, 150, -22, QColor(150, 60, 160));
    arrow->draggable = true;

    auto* text = plot.addText(QStringLiteral("可拖的標註"), 40, -18);
    text->draggable = true;
    text->background = FillStyle(QColor(255, 255, 210));
    text->border = LineStyle(QColor(160, 160, 100), 1.0);

    plot.addAnnotation(QStringLiteral("上面每一個都可以用滑鼠拖"),
                       Qt::AlignRight | Qt::AlignTop);
}

// Interaction.cpp — 互動功能的範例。
//
// 這幾支要在 Cookbook 裡用滑鼠試才看得出效果，匯出的靜態圖只能看到初始狀態。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

QTPLOT_RECIPE("互動", "命中與提示", "游標提示",
              "把游標移到資料點附近，旁邊會跳出該點的數值。"
              "距離是在「像素」空間算的 —— 兩軸的數量級常常差很多，"
              "用資料距離的話數值大的那一軸會完全主導，游標明明貼著某個點卻命中另一個。"
              "Cookbook 預設開著 PlotWidget::showTooltipOnHover。") {
    gen::setSeed(9);
    const QVector<double> xs = gen::randomSample(30, 0, 100);
    QVector<double> ys;
    for (double x : xs) ys.append(x * 0.6 + 10);
    ys = gen::addNoise(ys, 8);

    plot.addScatter(xs, ys)->legendText = QStringLiteral("量測點");
    plot.addAnnotation(QStringLiteral("把游標移到點上試試"), Qt::AlignLeft | Qt::AlignTop);
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
}

QTPLOT_RECIPE("互動", "命中與提示", "大量資料的命中",
              "Signal 的 X 是算出來的，所以命中測試直接反推索引，不必掃過全部點 —— "
              "一百萬點照樣即時。SignalXY 則是用二分搜尋找附近再比較左右兩點。") {
    plot.addSignal(gen::randomWalk(1'000'000), 1.0)->legendText = QStringLiteral("一百萬點");
    plot.legend.isVisible = true;
    plot.addAnnotation(QStringLiteral("縮放進去再把游標移到線上"), Qt::AlignLeft | Qt::AlignTop);
}

QTPLOT_RECIPE("互動", "拖曳", "可拖曳的參考線與區間",
              "設了 draggable 的圖元可以用滑鼠直接拖。抓到圖元時拖曳不會變成平移 —— "
              "不然使用者想移動一條門檻線，結果整張圖跟著跑。"
              "後加入的圖元畫在上層，所以命中時是反向找的，視覺上在最上面的優先抓到。") {
    plot.addSignal(gen::randomWalk(300));

    auto* upper = plot.addHorizontalLine(25, QColor(214, 39, 40));
    upper->draggable = true;
    upper->line.pattern = LinePattern::Dashed;
    upper->text.text = QStringLiteral("上限（可拖）");
    upper->text.font.color = QColor(214, 39, 40);

    auto* lower = plot.addHorizontalLine(-25, QColor(44, 160, 44));
    lower->draggable = true;
    lower->line.pattern = LinePattern::Dashed;
    lower->text.text = QStringLiteral("下限（可拖）");
    lower->text.font.color = QColor(44, 160, 44);

    auto* span = plot.addVerticalSpan(100, 160, QColor(80, 140, 220));
    span->draggable = true;
    span->legendText = QStringLiteral("關注區間（可拖）");

    auto* note = plot.addText(QStringLiteral("這個標註也可以拖"), 200, 30);
    note->draggable = true;
    note->background = FillStyle(QColor(255, 255, 220));
    note->border = LineStyle(QColor(150, 150, 100), 1.0);

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("互動", "刻度", "共同倍率",
              "數值很大時，把共同的 10 的次方抽到軸的端點，刻度就只剩好讀的小數字。"
              "不開的話這根軸會印成 0 / 2e+08 / 4e+08 …，又長又難比大小。"
              "左軸關掉倍率、右軸開著，兩種看一次。") {
    const QVector<double> xs = gen::consecutive(50);
    QVector<double> ys;
    for (int i = 0; i < 50; ++i) ys.append(1.0e8 + 8.0e8 * std::sin(i * 0.2) + i * 1.0e7);

    auto* left = plot.addScatter(xs, ys, QColor(31, 119, 180));
    left->marker = MarkerStyle::none();
    left->legendText = QStringLiteral("左軸：關掉倍率");
    if (auto* g = dynamic_cast<AutomaticTickGenerator*>(plot.leftAxis()->tickGenerator.get()))
        g->allowMultiplier = false;

    auto* rightAxis = plot.addAxis(Edge::Right);
    rightAxis->showTickLabels = true;
    rightAxis->majorTickLength = 5;
    auto* right = plot.addScatter(xs, ys, QColor(214, 39, 40));
    right->yAxis = rightAxis;
    right->marker = MarkerStyle::none();
    right->line.pattern = LinePattern::Dashed;
    right->legendText = QStringLiteral("右軸：開著倍率");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
}

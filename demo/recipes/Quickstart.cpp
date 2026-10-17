// Quickstart.cpp — 最基本的幾支範例。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

QTPLOT_RECIPE("入門", "快速上手", "散點圖",
              "最常用的一種圖：給 X 與 Y 兩個陣列，畫出折線加標記。") {
    plot.addScatter({1, 2, 3, 4, 5}, {1, 4, 9, 16, 25});
    plot.title.text = RT("散點圖");
    plot.bottomAxis()->label.text = RT("X 軸");
    plot.leftAxis()->label.text = RT("Y 軸");
}

QTPLOT_RECIPE("入門", "快速上手", "訊號圖（百萬點）",
              "等間距取樣的資料用 Signal：不存 X 陣列，而且點數超過像素數時會自動做 "
              "min/max 抽樣，所以一百萬點也能即時平移縮放。") {
    plot.addSignal(gen::randomWalk(1'000'000), 1.0);
    plot.title.text = RT("1,000,000 點的隨機漫步");
    plot.bottomAxis()->label.text = RT("取樣點");
}

QTPLOT_RECIPE("入門", "快速上手", "多條曲線與圖例",
              "每次呼叫 add* 會自動取下一個調色盤顏色。設了 legendText 的圖元才會進圖例。") {
    const QVector<double> xs = gen::consecutive(50);
    auto* a = plot.addScatter(xs, gen::sin(50));
    auto* b = plot.addScatter(xs, gen::cos(50));
    a->legendText = QStringLiteral("sin");
    b->legendText = QStringLiteral("cos");
    a->marker = MarkerStyle::none();
    b->marker = MarkerStyle::none();
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("入門", "快速上手", "長條圖",
              "從一串數值直接建一組等距長條。長條圖預設把 0 含進 Y 範圍，"
              "免得視覺上誇大差異。") {
    plot.addBars({12, 19, 7, 23, 15});
    plot.title.text = RT("長條圖");
}

QTPLOT_RECIPE("入門", "快速上手", "填色區間",
              "FillY 把兩條曲線之間填起來，常用來畫信賴區間或最大最小值範圍。") {
    const QVector<double> xs = gen::consecutive(100);
    const QVector<double> center = gen::sin(100, 1.0, 2.0);
    QVector<double> hi, lo;
    for (double v : center) {
        hi.append(v + 0.3);
        lo.append(v - 0.3);
    }
    auto* band = plot.addFillY(xs, lo, hi);
    band->legendText = RT("±0.3 區間");
    auto* line = plot.addScatter(xs, center);
    line->marker = MarkerStyle::none();
    line->legendText = RT("平均");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("入門", "快速上手", "函數圖",
              "FunctionPlot 每個像素行求值一次，所以放大之後解析度會自己跟上，"
              "不像先取樣成陣列那樣放大就露出折線。") {
    plot.addFunction([](double x) { return std::sin(x) / x; });
    plot.setAxisLimits(-20, 20, -0.3, 1.1);
    plot.title.text = QStringLiteral("sin(x) / x");
}

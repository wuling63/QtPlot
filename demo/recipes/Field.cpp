// Field.cpp — 等值線與向量場的範例。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

namespace {

/// 兩個高斯山丘加一個凹陷，用來看等值線畫不畫得出鞍點。
double terrain(double x, double y) {
    auto bump = [](double x, double y, double cx, double cy, double s) {
        const double dx = (x - cx) / s, dy = (y - cy) / s;
        return std::exp(-(dx * dx + dy * dy) / 2);
    };
    return 3.0 * bump(x, y, -1.2, -0.6, 0.9) + 2.4 * bump(x, y, 1.4, 0.8, 1.1) -
           1.6 * bump(x, y, 0.3, -1.3, 0.7);
}

const CoordinateRect kArea(-3, 3, -3, 3);

}  // namespace

QTPLOT_RECIPE("場", "等值線", "等值線",
              "marching squares 從格點純量場抽出輪廓。零散的線段會接成折線，"
              "所以虛線、線寬、標籤都能正常套用。"
              "預設的等值不取資料的頭尾 —— 等於最小值或最大值的等值線沒有資訊。") {
    plot.addContour(terrain, kArea, 120, 120, 10);
    plot.equalAxisScale = true;
    plot.title.text = QStringLiteral("兩個山丘加一個凹陷");
}

QTPLOT_RECIPE("場", "等值線", "等值線疊熱圖",
              "熱圖的值是「格子」、等值線的值是「格點」，兩者差半格。"
              "直接把同一個 extent 給兩者，等值線會整體偏移半格。"
              "latticeExtentFor() 就是做這個換算的。") {
    const int n = 60;
    const QVector<double> v = sampleField(terrain, kArea, n, n);

    auto* heat = plot.addHeatmap(v, n, n);
    heat->extent = kArea;
    heat->colormap = Colormap(Colormap::Viridis);
    heat->smooth = true;
    // Heatmap 的第 0 列預設畫在上方，sampleField 的第 0 列是 bottom，所以要翻
    heat->flipVertically = true;

    auto* c = plot.addContour(v, n, n, 9);
    c->extent = ContourLines::latticeExtentFor(kArea, n, n);
    c->line = LineStyle(QColor(255, 255, 255, 170), 1.0);

    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("場", "等值線", "色階與數值標籤",
              "useColormap 讓每條線依自己的等值上色，showLabels 把數值標在線上，"
              "標籤會跟著線的方向轉，而且底下用背景色蓋掉一小段，文字不會疊在線上。") {
    auto* c = plot.addContour(terrain, kArea, 140, 140, 12);
    c->useColormap = true;
    c->colormap = Colormap(Colormap::Coolwarm);
    c->showLabels = true;
    c->labelFont.size = 8.5;
    c->line.width = 1.6;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("場", "等值線", "只畫特定幾條",
              "levels 自己指定就只畫那幾條。等高線圖常用「每 500 公尺一條細線、"
              "每 2500 公尺一條粗線」這種兩層畫法 —— 疊兩個 ContourLines 即可。") {
    auto* minor = plot.addContour(terrain, kArea, 120, 120);
    minor->levels = {-1.0, -0.5, 0.5, 1.0, 1.5, 2.5, 3.0};
    minor->line = LineStyle(QColor(0, 0, 0, 70), 0.8);

    auto* major = plot.addContour(terrain, kArea, 120, 120);
    major->levels = {0.0, 2.0};
    major->line = LineStyle(QColor(0, 0, 0, 200), 2.0);
    major->showLabels = true;
    major->labelFont.bold = true;

    plot.equalAxisScale = true;
    plot.addAnnotation(QStringLiteral("粗線：0 與 2　細線：其餘"), Qt::AlignLeft | Qt::AlignTop);
}

QTPLOT_RECIPE("場", "向量場", "向量場",
              "每個格點畫一支箭頭。scale 不給就自動縮放：最長的箭頭剛好等於格點間距，"
              "箭頭才不會互相穿過。") {
    plot.addVectorField(
        [](double x, double y) {
            return Vector2{-y, x};  // 繞原點旋轉
        },
        kArea, 15, 15);
    plot.equalAxisScale = true;
    plot.title.text = QStringLiteral("旋轉場 (-y, x)");
}

QTPLOT_RECIPE("場", "向量場", "依大小上色",
              "normalize 讓所有箭頭等長、只表示方向，大小改用顏色表示。"
              "大小差距很大時這樣才看得清方向 —— 否則小的箭頭會短到看不見。") {
    auto* f = plot.addVectorField(
        [](double x, double y) {
            const double r2 = x * x + y * y + 0.3;
            return Vector2{x / r2, y / r2};  // 點源，越靠近中心越強
        },
        kArea, 17, 17);
    f->normalize = true;
    f->useColormap = true;
    f->colormap = Colormap(Colormap::Turbo);
    f->anchor = VectorField::Anchor::Center;
    plot.equalAxisScale = true;
    plot.title.text = QStringLiteral("點源場（顏色 = 強度）");
}

QTPLOT_RECIPE("場", "向量場", "梯度場與等值線",
              "把 f 的等值線和它的梯度 ∇f 畫在一起：箭頭一定垂直於等值線，"
              "而且指向 f 增加的方向。兩者對不上就表示其中一個算錯了 —— "
              "這是檢查場的計算最快的方法。") {
    auto* c = plot.addContour(terrain, kArea, 120, 120, 11);
    c->line = LineStyle(QColor(80, 80, 80, 140), 1.0);

    // 數值梯度（中央差分）
    const double h = 0.02;
    auto* f = plot.addVectorField(
        [h](double x, double y) {
            return Vector2{(terrain(x + h, y) - terrain(x - h, y)) / (2 * h),
                           (terrain(x, y + h) - terrain(x, y - h)) / (2 * h)};
        },
        kArea, 19, 19);
    f->useColormap = true;
    f->colormap = Colormap(Colormap::Hot);
    f->line.width = 1.2;

    plot.equalAxisScale = true;
    plot.title.text = QStringLiteral("等值線與梯度場");
}

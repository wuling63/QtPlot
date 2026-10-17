// Decorations.cpp — 色階圖例、影像標記、浮動軸、不規則取樣的即時資料。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

using namespace qtplot;

namespace {

double terrain(double x, double y) {
    auto bump = [](double x, double y, double cx, double cy, double s) {
        const double dx = (x - cx) / s, dy = (y - cy) / s;
        return std::exp(-(dx * dx + dy * dy) / 2);
    };
    return 3.0 * bump(x, y, -1.2, -0.6, 0.9) + 2.4 * bump(x, y, 1.4, 0.8, 1.1) -
           1.6 * bump(x, y, 0.3, -1.3, 0.7);
}

/// 一個小圖示，用來當影像標記。
QImage pinIcon(QColor color, int size = 26) {
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QBrush(color));
    p.setPen(QPen(Qt::white, 2));
    // 水滴形狀：上面一個圓，下面一個尖
    QPainterPath path;
    path.addEllipse(QPointF(size / 2.0, size * 0.38), size * 0.30, size * 0.30);
    path.moveTo(size * 0.26, size * 0.52);
    path.lineTo(size / 2.0, size * 0.94);
    path.lineTo(size * 0.74, size * 0.52);
    path.closeSubpath();
    p.drawPath(path.simplified());
    p.setBrush(QBrush(Qt::white));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(size / 2.0, size * 0.38), size * 0.12, size * 0.12);
    p.end();
    return img;
}

/// 取出圖上第一個該型別的圖元（會動的範例在 frame > 0 時要拿它來加資料）。
template <typename T>
T* firstOf(Plot& plot) {
    for (IPlottable* p : plot.plottables())
        if (auto* t = dynamic_cast<T*>(p)) return t;
    return nullptr;
}

}  // namespace

// ───────────────────────── 色階圖例 ─────────────────────────

QTPLOT_RECIPE("裝飾", "色階圖例", "熱圖加色階圖例",
              "沒有圖例的話，熱圖的顏色只看得出相對高低，讀不出實際數值。"
              "addColorbar(heatmap) 會直接沿用該熱圖的色階與資料範圍，"
              "不必手動把兩邊的設定對齊 —— 對不齊的圖例比沒有圖例更糟。"
              "圖例畫在繪圖區外側，版面會自動替它留出空間。") {
    const int n = 50;
    auto* heat = plot.addHeatmap(gen::gaussian2D(n, n, 0.28), n, n);
    heat->colormap = Colormap(Colormap::Viridis);
    heat->smooth = true;
    heat->extent = CoordinateRect(0, 10, 0, 10);

    auto* bar = plot.addColorbar(*heat);
    bar->label.text = QStringLiteral("強度");

    plot.equalAxisScale = true;
    plot.title.text = QStringLiteral("2D 高斯");
}

QTPLOT_RECIPE("裝飾", "色階圖例", "圖例的位置與方向",
              "edge 決定貼哪一邊。放在下方就是水平色帶，適合寬扁的圖。"
              "thickness 改厚度、lengthFraction 改長度。刻度和座標軸共用同一套產生器，"
              "所以會落在整齊的數字上，也可以換成 ManualTickGenerator 自己指定。") {
    const int n = 60;
    const CoordinateRect area(-3, 3, -3, 3);
    auto* heat = plot.addHeatmap(sampleField(terrain, area, n, n), n, n);
    heat->colormap = Colormap(Colormap::Coolwarm);
    heat->extent = area;
    heat->flipVertically = true;
    heat->smooth = true;

    auto* bar = plot.addColorbar(*heat, Edge::Bottom);
    bar->label.text = QStringLiteral("高度 (m)");
    bar->thickness = 14;
    bar->lengthFraction = 0.6;

    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("裝飾", "色階圖例", "等值線與 TIN 的圖例",
              "ContourLines 與 TIN 也可以直接餵給 addColorbar。"
              "等值線用的是 levels 的範圍，TIN 用的是 z 的範圍。") {
    gen::setSeed(23);
    const QVector<double> xs = gen::randomUniform(100, 0, 10);
    const QVector<double> ys = gen::randomUniform(100, 0, 10);
    QVector<Coordinates> pts;
    QVector<double> zs;
    for (int i = 0; i < xs.size(); ++i) {
        pts.append({xs[i], ys[i]});
        const double a = std::exp(-(std::pow(xs[i] - 3, 2) + std::pow(ys[i] - 7, 2)) / 8);
        const double b = std::exp(-(std::pow(xs[i] - 7, 2) + std::pow(ys[i] - 3, 2)) / 12);
        zs.append(100 * a + 70 * b);
    }
    auto* tin = plot.addTIN(pts, zs);
    tin->colormap = Colormap(Colormap::Turbo);

    auto* bar = plot.addColorbar(*tin);
    bar->label.text = QStringLiteral("高程 (m)");

    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("裝飾", "色階圖例", "同一邊兩個色階",
              "同一邊放兩個色階時不必手動算位置：版面會讓第二個自動讓開第一個，"
              "也會讓開那一邊座標軸的厚度。") {
    const int n = 40;
    auto* a = plot.addHeatmap(gen::gaussian2D(n, n, 0.3), n, n);
    a->extent = CoordinateRect(0, 10, 0, 10);
    a->colormap = Colormap(Colormap::Viridis);
    a->smooth = true;

    auto* b = plot.addHeatmap(gen::sin2D(n, n, 1.5), n, n);
    b->extent = CoordinateRect(11, 21, 0, 10);
    b->colormap = Colormap(Colormap::Turbo);
    b->smooth = true;

    plot.addColorbar(*a, Edge::Right)->label.text = QStringLiteral("高斯");
    plot.addColorbar(*b, Edge::Right)->label.text = QStringLiteral("正弦");
    plot.title.text = QStringLiteral("兩個色階都貼在右邊");
}

QTPLOT_RECIPE("裝飾", "色階圖例", "兩個色階並排",
              "一張圖上可以有多個圖例。指定不同的 edge 就會分在左右兩邊。") {
    const int n = 40;
    auto* a = plot.addHeatmap(gen::gaussian2D(n, n, 0.3), n, n);
    a->extent = CoordinateRect(0, 10, 0, 10);
    a->colormap = Colormap(Colormap::Viridis);
    a->smooth = true;

    auto* contour = plot.addContour(gen::sin2D(n, n, 1.5), n, n, 8);
    contour->extent = ContourLines::latticeExtentFor(CoordinateRect(0, 10, 0, 10), n, n);
    contour->useColormap = true;
    contour->colormap = Colormap(Colormap::Hot);
    contour->line.width = 1.5;

    plot.addColorbar(*a, Edge::Right)->label.text = QStringLiteral("底圖");
    plot.addColorbar(*contour, Edge::Left)->label.text = QStringLiteral("等值線");
    plot.equalAxisScale = true;
}

// ───────────────────────── 影像標記 ─────────────────────────

QTPLOT_RECIPE("裝飾", "影像標記", "用影像當標記",
              "ImageMarker 在每個資料點畫一張小圖。地圖上的圖釘、流程圖的節點圖示、"
              "或用縮圖代表每個樣本都靠這個。stride 可以每隔幾個點才畫一個，"
              "資料多的時候避免糊成一片。") {
    const QVector<double> xs{1.5, 3.2, 5.8, 7.1, 8.6};
    const QVector<double> ys{4.2, 6.8, 3.1, 7.5, 5.0};

    auto* line = plot.addScatter(xs, ys, QColor(120, 120, 140));
    line->marker = MarkerStyle::none();
    line->line.pattern = LinePattern::Dashed;

    auto* pins = plot.addImageMarkers(xs, ys, pinIcon(QColor(214, 39, 40), 48));
    pins->width = 26;
    pins->height = 26;
    // centered = true：圖示的中心對準資料點。想讓圖釘的「尖端」對準資料點的話，
    // 把圖示畫成尖端在中心的版本，或自己把 ys 往上移半個圖示的高度。
    pins->centered = true;

    plot.setAxisLimits(0, 10, 2, 9);
    plot.title.text = QStringLiteral("路線上的五個停留點");
}

// ───────────────────────── 浮動軸 ─────────────────────────

QTPLOT_RECIPE("裝飾", "浮動軸", "把軸畫在零線上",
              "資料有正有負時，軸畫在 0 上比畫在圖的邊緣好讀 —— "
              "正負號一眼看得出來，不必把視線拉到邊界再對回來。"
              "FloatingAxis 畫在繪圖區「內部」的任意位置。") {
    const QVector<double> xs = gen::linspace(-6, 6, 200);
    QVector<double> ys;
    for (double x : xs) ys.append(std::sin(x) * std::exp(-std::abs(x) / 5));

    auto* s = plot.addScatter(xs, ys);
    s->marker = MarkerStyle::none();
    s->line.width = 2;

    // 原本的四邊軸全部關掉，改用兩根穿過原點的浮動軸
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.grid.isVisible = false;

    auto* xAxis = plot.addFloatingAxis(true, 0.0);
    xAxis->label.text = QStringLiteral("x");
    auto* yAxis = plot.addFloatingAxis(false, 0.0);
    yAxis->labelSide = -1;  // 刻度與標籤畫在軸的另一側
    yAxis->label.text = QStringLiteral("y");

    plot.setAxisLimits(-6.5, 6.5, -0.8, 0.8);
}

QTPLOT_RECIPE("裝飾", "浮動軸", "局部的參考刻度",
              "extent 可以讓浮動軸只畫一段，當作某個區域的局部尺規。") {
    plot.addSignal(gen::randomWalk(300));
    plot.autoScale();

    const CoordinateRange y = plot.leftAxis()->range;
    auto* ruler = plot.addFloatingAxis(false, 150);
    ruler->extent = CoordinateRange(y.center() - y.span() * 0.25, y.center() + y.span() * 0.25);
    ruler->line = LineStyle(QColor(200, 60, 60), 1.5);
    ruler->tickLine = LineStyle(QColor(200, 60, 60), 1.2);
    ruler->tickFont.color = QColor(200, 60, 60);
    ruler->label.text = QStringLiteral("局部尺規");
    ruler->label.font.color = QColor(200, 60, 60);
}

// ───────────────────── 不規則取樣的即時資料 ─────────────────────

QTPLOT_RECIPE_LIVE("即時", "波形", "DataStreamerXY（不規則取樣）",
                   "DataStreamer 的 X 是由索引算出來的，假設每一筆都等間距。"
                   "事件驅動、網路延遲、感測器丟包這些情況下取樣並不等間距，"
                   "就要用 DataStreamerXY 自己帶 X 進來 —— "
                   "否則時間軸上的疏密會是假的。", 25) {
    if (frame == 0) {
        auto* s = plot.addDataStreamerXY(200);
        s->viewWidth = 60;  // 只顯示最近 60 秒
        s->legendText = QStringLiteral("不等間距的事件");
        s->marker = MarkerStyle(MarkerShape::FilledCircle, 4, QColor(31, 119, 180));
        plot.legend.isVisible = true;
        plot.bottomAxis()->label.text = QStringLiteral("時間（秒）");
        return;
    }
    if (auto* s = firstOf<DataStreamerXY>(plot)) {
        // 取樣間隔在 0.1 到 1.2 秒之間跳動
        static double clock = 0;
        if (frame == 1) clock = 0;
        const double jitter = 0.1 + 1.1 * std::abs(std::sin(frame * 2.3));
        clock += jitter;
        s->add(clock, std::sin(clock * 0.4) + 0.3 * std::sin(clock * 1.7));
    }
}

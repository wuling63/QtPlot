// Supplement2.cpp — 對照 ScottPlot 5 Cookbook 第二次補上的範例。
//
// 這一批是函式庫新加的能力：真 3D 圖表（可旋轉的座標盒）、圖例的標題／分欄／放在外面、
// 核密度的核函數、箭頭的形狀、熱圖的透明度與格子對齊、固定間隔的日期刻度。
#include "../Recipe.h"
#include "qtplot/DataGen.h"
#include "qtplot/PlotWidget.h"

#include <QDateTime>
#include <QLocale>
#include <cmath>

using namespace qtplot;

namespace {

/// MATLAB 的 peaks：兩座山一個坑，最常拿來示範曲面。
double peaks(double x, double y) {
    return 3 * std::pow(1 - x, 2) * std::exp(-x * x - std::pow(y + 1, 2))
           - 10 * (x / 5 - std::pow(x, 3) - std::pow(y, 5)) * std::exp(-x * x - y * y)
           - 1.0 / 3 * std::exp(-std::pow(x + 1, 2) - y * y);
}

/// 在規則格網上取樣 f，回傳 row-major 的 z（列對應 y）。
QVector<double> evaluate(const QVector<double>& xs, const QVector<double>& ys,
                         const std::function<double(double, double)>& f) {
    QVector<double> zs;
    zs.reserve(xs.size() * ys.size());
    for (double y : ys)
        for (double x : xs) zs.append(f(x, y));
    return zs;
}

QVector<Point3D> cluster(double x, double y, double z, int n) {
    const QVector<double> dx = gen::randomNormal(n), dy = gen::randomNormal(n), dz = gen::randomNormal(n);
    QVector<Point3D> pts;
    for (int i = 0; i < n; ++i) pts.append({x + dx[i], y + dy[i], z + dz[i]});
    return pts;
}

}  // namespace

// ───────────────────────────── 3D 圖表 ─────────────────────────────

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 散點圖",
              "三群點放進可以任意旋轉的座標盒。平面投影看不出點有多高，"
              "dropLines 從每個點拉一條垂線到地板，位置才讀得出來。") {
    gen::setSeed(3);
    auto* chart = plot.addChart3D();
    const QColor colors[] = {QColor(31, 119, 180), QColor(214, 39, 40), QColor(44, 160, 44)};
    const double centers[][3] = {{2, 2, 2}, {6, 5, 5}, {3, 7, 7}};
    for (int i = 0; i < 3; ++i) {
        auto s = std::make_unique<Scatter3D>();
        s->points = cluster(centers[i][0], centers[i][1], centers[i][2], 40);
        s->color = colors[i];
        s->dropLines = true;
        s->name = QStringLiteral("第 %1 群").arg(i + 1);
        chart->add(std::move(s));
    }
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("圖種", "3D 圖表", "依高度上色的 3D 散點",
              "colorByZ 讓每個點依 Z 值取色，再配一條色階圖例 —— "
              "視角讓高度難以判斷的時候，顏色還讀得出來。") {
    gen::setSeed(8);
    const QVector<double> xs = gen::randomUniform(400, -3, 3), ys = gen::randomUniform(400, -3, 3);
    auto s = std::make_unique<Scatter3D>();
    for (int i = 0; i < xs.size(); ++i) s->points.append({xs[i], ys[i], peaks(xs[i], ys[i])});
    s->colorByZ = true;
    s->markerSize = 5;

    auto* chart = plot.addChart3D();
    chart->add(std::move(s));
    chart->colormap = Colormap(Colormap::Turbo);
    plot.addColorbar(*chart);
}

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 曲線（勞倫茲吸引子）",
              "用小步長積分勞倫茲方程得到的軌跡。依 Z 上色看得出軌跡在兩個翼之間來回跳。") {
    auto l = std::make_unique<Line3D>();
    double x = 1, y = 1, z = 1;
    const double dt = 0.01;
    for (int i = 0; i < 4000; ++i) {
        const double dx = 10 * (y - x), dy = x * (28 - z) - y, dz = x * y - 8.0 / 3 * z;
        x += dx * dt;
        y += dy * dt;
        z += dz * dt;
        l->points.append({x, y, z});
    }
    l->colorByZ = true;
    l->width = 1.2;

    auto* chart = plot.addChart3D();
    chart->add(std::move(l));
    chart->colormap = Colormap(Colormap::Hot);
    chart->aspectZ = 1;
    chart->azimuth = -40;
    chart->elevation = 20;
    plot.title.text = QStringLiteral("勞倫茲吸引子");
}

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 曲面",
              "在格網上取樣 z = f(x, y)，每格依平均高度取色並依朝向打光。"
              "由遠而近畫，所以前面的山會擋住後面的谷。") {
    const QVector<double> xs = gen::linspace(-3, 3, 41), ys = gen::linspace(-3, 3, 41);
    auto s = std::make_unique<SurfaceSeries3D>();
    s->xs = xs;
    s->ys = ys;
    s->zs = evaluate(xs, ys, peaks);

    auto* chart = plot.addChart3D();
    chart->add(std::move(s));
    plot.addColorbar(*chart);
    plot.title.text = QStringLiteral("peaks 函數");
}

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 線框",
              "fill = false 只畫格線。沒有填色擋住，曲面背後的線也看得到。") {
    const QVector<double> xs = gen::linspace(-8, 8, 33), ys = gen::linspace(-8, 8, 33);
    auto s = std::make_unique<SurfaceSeries3D>();
    s->xs = xs;
    s->ys = ys;
    s->zs = evaluate(xs, ys, [](double x, double y) {
        const double r = std::hypot(x, y);
        return r < 1e-9 ? 1.0 : std::sin(r) / r;
    });
    s->fill = false;
    s->wireColor = QColor(31, 119, 180);
    s->wireWidth = 1;

    auto* chart = plot.addChart3D();
    chart->add(std::move(s));
    chart->elevation = 30;
    plot.title.text = QStringLiteral("sin(r) / r");
}

QTPLOT_RECIPE("圖種", "3D 圖表", "透視投影的曲面",
              "perspective = true 時遠的部分看起來比較小，立體感更強，"
              "代價是比例不再精確 —— 要讀數值的圖不要開。") {
    const QVector<double> xs = gen::linspace(0, 2 * kPi, 36), ys = gen::linspace(0, 2 * kPi, 36);
    auto s = std::make_unique<SurfaceSeries3D>();
    s->xs = xs;
    s->ys = ys;
    s->zs = evaluate(xs, ys, [](double x, double y) { return std::sin(x) * std::cos(y); });

    auto* chart = plot.addChart3D();
    chart->add(std::move(s));
    chart->colormap = Colormap(Colormap::Turbo);
    chart->perspective = true;
    chart->azimuth = -35;
    chart->elevation = 35;
    chart->aspectZ = 0.45;
    plot.title.text = QStringLiteral("sin(x) cos(y)，透視投影");
}

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 山脊圖（瀑布）",
              "把山脊圖的每條分布曲線沿 Y 往後排，每條填到地板。"
              "近的曲線擋住遠的，月份用 yTickFormatter 印成文字。") {
    const QStringList months = {QStringLiteral("一月"), QStringLiteral("三月"), QStringLiteral("五月"),
                                QStringLiteral("七月"), QStringLiteral("九月"), QStringLiteral("十一月")};
    const double means[] = {13, 18, 24, 30, 26, 18};
    const double spreads[] = {3.0, 3.4, 2.8, 2.2, 3.0, 3.6};

    auto w = std::make_unique<Waterfall3D>();
    w->xs = gen::linspace(0, 40, 160);
    for (int k = 0; k < months.size(); ++k) {
        w->ys.append(k);
        QVector<double> density;
        for (double x : w->xs)
            density.append(std::exp(-0.5 * std::pow((x - means[k]) / spreads[k], 2)) /
                           (spreads[k] * std::sqrt(2 * kPi)));
        w->zs.append(density);
    }

    auto* chart = plot.addChart3D();
    chart->add(std::move(w));
    chart->xLabel = QStringLiteral("日均溫（°C）");
    chart->yLabel = QStringLiteral("月份");
    chart->zLabel = QStringLiteral("密度");
    chart->yTickFormatter = [months](double y) {
        const int i = static_cast<int>(std::lround(y));
        return (i >= 0 && i < months.size()) ? months[i] : QString();
    };
    chart->aspectX = 1.4;
    chart->aspectZ = 0.5;
    chart->azimuth = -25;
    chart->elevation = 28;
    plot.title.text = QStringLiteral("一年之中氣溫分布的移動");
}

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 柱狀圖",
              "規則格網上的立體柱，高度依色階上色。整根柱一起排序、只畫朝向觀察者的三面，"
              "從任何角度看都不會穿插。") {
    gen::setSeed(2);
    auto c = std::make_unique<Columns3D>();
    const QVector<double> noise = gen::randomUniform(30, -8, 8);
    int i = 0;
    for (int month = 1; month <= 6; ++month)
        for (int store = 1; store <= 5; ++store)
            c->columns.append({double(month), double(store), std::round(20 + 8 * month + 5 * store + noise[i++])});
    c->colorByZ = true;

    auto* chart = plot.addChart3D();
    chart->add(std::move(c));
    chart->xLabel = QStringLiteral("月份");
    chart->yLabel = QStringLiteral("門市");
    chart->zLabel = QStringLiteral("營收");
    chart->zRange = CoordinateRange(0, 100);
}

QTPLOT_RECIPE_WIDGET("圖種", "3D 圖表", "用滑鼠旋轉 3D 圖",
                     "enableMouseRotation() 讓左鍵拖曳改視角：左右拖改方位角、上下拖改仰角，"
                     "滾輪也改仰角。它會順便關掉平移與縮放 —— 兩種拖曳行為同時開著會打架。") {
    Plot& plot = widget.plot();
    const QVector<double> xs = gen::linspace(-3, 3, 31), ys = gen::linspace(-3, 3, 31);
    auto s = std::make_unique<SurfaceSeries3D>();
    s->xs = xs;
    s->ys = ys;
    s->zs = evaluate(xs, ys, peaks);

    auto* chart = plot.addChart3D();
    chart->add(std::move(s));
    chart->colormap = Colormap(Colormap::Turbo);
    plot.addColorbar(*chart);
    plot.title.text = QStringLiteral("按住左鍵拖曳旋轉");
    enableMouseRotation(widget, chart);
}

// ───────────────────────────── 圖例 ─────────────────────────────

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例標題",
              "legend.title 畫在項目上方，說明這組圖例在分什麼（例如「感測器」）。") {
    const char* names[] = {"A 區", "B 區", "C 區"};
    for (int i = 0; i < 3; ++i)
        plot.addSignal(gen::sin(100, 1.0 + i * 0.4, 1.0 + i * 0.5), 1.0)->legendText = QString::fromUtf8(names[i]);
    plot.legend.isVisible = true;
    plot.legend.title.text = QStringLiteral("感測器位置");
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例分欄",
              "項目很多時直向圖例會比繪圖區還高。columns 把它分成幾欄（一欄一欄往下填）。") {
    for (int i = 0; i < 12; ++i) {
        auto* s = plot.addSignal(gen::sin(120, 1.0, 1.0, i / 12.0), 1.0);
        s->legendText = QStringLiteral("序列 %1").arg(i + 1);
    }
    plot.legend.isVisible = true;
    plot.legend.columns = 3;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例放在繪圖區外面",
              "資料滿到四個角落、沒有空位放圖例時，placement = OutsideRight 把它放到右邊外面，"
              "版面會自動讓出寬度，圖例就不會蓋到資料。") {
    gen::setSeed(5);
    for (int i = 0; i < 4; ++i) {
        auto* s = plot.addScatter(gen::consecutive(60), gen::randomWalk(60, 1.0, i * 3.0));
        s->legendText = QStringLiteral("帳戶 %1").arg(QChar('A' + i));
    }
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideRight;
    plot.legend.title.text = QStringLiteral("帳戶");
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例放在下方並換行",
              "placement = OutsideBottom 放在底軸下面；horizontal 配 columns 讓橫向的圖例每列最多放幾項，"
              "多的自動換到下一列。") {
    const char* names[] = {"北部", "中部", "南部", "東部", "離島", "海外"};
    for (int i = 0; i < 6; ++i)
        plot.addSignal(gen::cos(80, 1.0 + i * 0.2, 1.0 + i * 0.3), 1.0)->legendText = QString::fromUtf8(names[i]);
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideBottom;
    plot.legend.horizontal = true;
    plot.legend.columns = 3;
}

// ───────────────────────────── 統計 ─────────────────────────────

QTPLOT_RECIPE("統計", "箱型與分布", "核密度的核函數",
              "同一組資料、同一個頻寬，只換核函數。差別遠小於換頻寬：高斯最平滑；"
              "Epanechnikov 在均方誤差意義下最佳且有界；均勻核會出現階梯狀的轉折。") {
    gen::setSeed(21);
    QVector<double> data = gen::randomNormal(60, -1.5, 0.7);
    data += gen::randomNormal(40, 1.5, 0.9);

    const QVector<double> xs = gen::linspace(-5, 5, 400);
    const double h = 0.6;
    struct K { KernelType type; const char* name; };
    const K kernels[] = {{KernelType::Gaussian, "高斯"}, {KernelType::Epanechnikov, "Epanechnikov"},
                         {KernelType::Uniform, "均勻"}, {KernelType::Triangular, "三角"}, {KernelType::Cosine, "餘弦"}};
    for (const K& k : kernels) {
        auto* s = plot.addScatter(xs, kernelDensity(data, xs, h, k.type));
        s->marker = MarkerStyle::none();
        s->line.width = 2;
        s->legendText = QString::fromUtf8(k.name);
    }

    // 資料點本身畫成底下的短線（rug）
    auto* rug = plot.addMarkers(data, QVector<double>(data.size(), -0.01), QColor(0, 0, 0, 140));
    rug->marker = MarkerStyle(MarkerShape::VerticalBar, 8, QColor(0, 0, 0, 140));

    plot.legend.isVisible = true;
    plot.leftAxis()->label.text = QStringLiteral("密度");
}

// ───────────────────────────── 形狀 ─────────────────────────────

QTPLOT_RECIPE("圖種", "形狀與標註", "箭頭的形狀",
              "headShape 換箭頭的頭：三角、V 形、倒鉤、菱形、圓點、不畫頭。"
              "doubleEnded 兩端都畫頭，標示距離或區間用。") {
    struct H { ArrowHead shape; const char* name; };
    const H heads[] = {{ArrowHead::Triangle, "Triangle"}, {ArrowHead::Open, "Open"}, {ArrowHead::Barbed, "Barbed"},
                       {ArrowHead::Diamond, "Diamond"}, {ArrowHead::Circle, "Circle"}, {ArrowHead::None, "None"}};
    for (int i = 0; i < 6; ++i) {
        const double y = 6 - i;
        auto* a = plot.addArrow(8, y, 2, y, plot.style.palette.color(i));
        a->headShape = heads[i].shape;
        a->headLength = 16;
        a->headWidth = 12;
        auto* t = plot.addText(QString::fromUtf8(heads[i].name), 1.8, y);
        t->alignment = Qt::AlignRight | Qt::AlignVCenter;
    }
    auto* both = plot.addArrow(8, -0.5, 2, -0.5, QColor(80, 80, 80));
    both->doubleEnded = true;
    both->line.width = 1.5;
    plot.addText(QStringLiteral("doubleEnded：6 公尺"), 5, -0.2)->alignment = Qt::AlignHCenter | Qt::AlignBottom;

    plot.bottomAxis()->setRange(-1, 10);
    plot.leftAxis()->setRange(-1.5, 7);
    plot.grid.isVisible = false;
}

// ───────────────────────────── 熱圖 ─────────────────────────────

QTPLOT_RECIPE("場", "熱圖", "依可信度調淡的熱圖",
              "alphaMap 給每一格各自的不透明度：樣本數少的格子畫得淡，讀者自然不會太相信它。"
              "opacity 則是整張圖一起調。") {
    const int n = 24;
    const QVector<double> values = gen::gaussian2D(n, n, 0.3);
    QVector<double> confidence;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            // 中央取樣密、邊緣取樣稀
            const double dx = (c - n / 2.0) / n, dy = (r - n / 2.0) / n;
            confidence.append(std::clamp(1.2 - 2.4 * std::hypot(dx, dy), 0.08, 1.0));
        }
    auto* heat = plot.addHeatmap(values, n, n);
    heat->colormap = Colormap(Colormap::Turbo);
    heat->alphaMap = confidence;
    plot.addColorbar(*heat);
    plot.title.text = QStringLiteral("邊緣樣本少，畫得比較淡");
}

QTPLOT_RECIPE("場", "熱圖", "格子中心對齊刻度",
              "量測值是在格點上取的（第 i 格就是 x = i）時，格子的中心應該對準刻度。"
              "cellCentered = true 讓 extent 指「頭尾兩格的中心」，整張圖往外推半格。") {
    const int n = 6;
    QVector<double> values;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) values.append(r * n + c);
    auto* heat = plot.addHeatmap(values, n, n);
    heat->cellCentered = true;
    heat->extent = CoordinateRect(0, n - 1, 0, n - 1);

    // 在每個格點上放一個點，證明它們落在格子正中央
    QVector<double> xs, ys;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) { xs.append(c); ys.append(r); }
    auto* dots = plot.addMarkers(xs, ys, QColor(255, 255, 255));
    dots->marker = MarkerStyle(MarkerShape::FilledCircle, 6, QColor(255, 255, 255));
    plot.grid.isVisible = false;
}

// ───────────────────────────── 座標軸 ─────────────────────────────

QTPLOT_RECIPE("座標軸", "標籤與刻度", "固定間隔的日期刻度",
              "週報、班表這類資料，刻度要對到「每週一」才有意義。"
              "DateTimeTickGenerator 的 interval = Weeks 讓刻度永遠落在週一，不隨縮放跳到別天。") {
    const double start = QDateTime(QDate(2026, 3, 4), QTime(0, 0)).toSecsSinceEpoch();
    const int n = 90;
    QVector<double> xs, ys = gen::randomWalk(n, 1.0, 100);
    for (int i = 0; i < n; ++i) xs.append(start + i * 86400.0);
    auto* s = plot.addSignalXY(xs, ys);
    s->marker = MarkerStyle::none();

    auto ticks = std::make_shared<DateTimeTickGenerator>();
    ticks->interval = DateTimeTickGenerator::Interval::Weeks;
    ticks->format = QStringLiteral("MM-dd");
    plot.bottomAxis()->tickGenerator = ticks;
    plot.bottomAxis()->label.text = QStringLiteral("每根刻度都是週一");
}

// ───────────────────────────── 長條圖 ─────────────────────────────

namespace {

/// 把一串名稱放到某根軸的指定位置上。
void setTicks(Axis* axis, const QVector<double>& positions, const QStringList& names) {
    auto gen = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < positions.size() && i < names.size(); ++i) gen->ticks.append({positions[i], names[i]});
    axis->tickGenerator = gen;
}

}  // namespace

QTPLOT_RECIPE("圖種", "長條與統計", "群組堆疊長條加百分比",
              "每個月兩根堆疊長條並排。每一段印出它佔自己那根的百分比（labelPlacement = Center），"
              "總數寫在頂端 —— 組成和總量可以同時比較。") {
    const QStringList months = {QStringLiteral("2 月"), QStringLiteral("3 月"), QStringLiteral("4 月"), QStringLiteral("5 月"),
                                QStringLiteral("6 月"), QStringLiteral("7 月"), QStringLiteral("8 月")};
    const double bmw[7][3] = {{25, 6, 10}, {10, 4, 15}, {40, 7, 20}, {50, 20, 16}, {13, 5, 47}, {30, 3, 38}, {30, 6, 20}};
    const double ferrari[7][3] = {{5, 2, 20}, {12, 1, 14}, {15, 2, 32}, {5, 1, 45}, {20, 3, 25}, {30, 2, 40}, {12, 2, 35}};
    struct Brand { const char* name; const double (*data)[3]; double offset; QColor colors[3]; };
    const Brand brands[] = {
        {"BMW", bmw, -0.2, {QColor(165, 200, 160), QColor(130, 80, 90), QColor(250, 190, 150)}},
        {"Ferrari", ferrari, 0.2, {QColor(240, 150, 90), QColor(165, 165, 165), QColor(245, 240, 200)}}};
    const char* parts[] = {"轎車", "跑車", "休旅車"};

    QVector<Bar> bars;
    for (const Brand& b : brands) {
        for (int m = 0; m < months.size(); ++m) {
            const double total = b.data[m][0] + b.data[m][1] + b.data[m][2];
            double bottom = 0;
            for (int s = 0; s < 3; ++s) {
                Bar bar;
                bar.position = m + b.offset;
                bar.valueBase = bottom;
                bar.value = bottom + b.data[m][s];
                bar.size = 0.36;
                bar.fillColor = b.colors[s];
                bar.borderColor = QColor(0, 0, 0, 120);
                const double share = b.data[m][s] / total;
                if (share >= 0.04) bar.label = QStringLiteral("%1%").arg(std::lround(share * 100));
                bar.labelPlacement = Bar::LabelPlacement::Center;
                bars.append(bar);
                bottom += b.data[m][s];
            }
            auto* t = plot.addText(QString::number(total), m + b.offset, total);
            t->alignment = Qt::AlignHCenter | Qt::AlignBottom;
            t->label.font.size = 8;
        }
        for (int s = 0; s < 3; ++s) {
            LegendItem it;
            it.text = QStringLiteral("%1 %2").arg(QString::fromUtf8(b.name), QString::fromUtf8(parts[s]));
            it.fill = FillStyle(b.colors[s]);
            it.line = LineStyle::none();
            plot.legend.manualItems.append(it);
        }
    }
    auto* bp = plot.addBars(bars);
    bp->valueLabelFont.size = 8;

    QVector<double> pos;
    for (int m = 0; m < months.size(); ++m) pos.append(m);
    setTicks(plot.bottomAxis(), pos, months);
    plot.setAxisLimits(-0.6, months.size() - 0.4, 0, 100);
    plot.grid.isVisible = false;
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideRight;
}

QTPLOT_RECIPE("圖種", "長條與統計", "排序的相關係數長條",
              "每一對變數的相關係數排序後畫成橫向長條。發散色階（負紅、正綠）讓正負與強弱一眼看出來。") {
    struct P { const char* pair; double r; };
    const P pairs[] = {
        {"weight - movement", -0.97}, {"height - movement", -0.96}, {"weight - skill moves", -0.94},
        {"height - skill moves", -0.94}, {"height - skill", -0.91}, {"weight - skill", -0.90},
        {"height - attacking", -0.89}, {"weight - attacking", -0.88}, {"height - power", -0.80},
        {"weight - power", -0.78}, {"weight - pace", -0.71}, {"height - pace", -0.67},
        {"age - pace", -0.66}, {"age - movement", -0.61}, {"age - skill moves", -0.54},
        {"age - power", -0.50}, {"age - attacking", -0.49}, {"age - skill", -0.49},
        {"pace - power", 0.32}, {"pace - attacking", 0.40}, {"age - height", 0.42},
        {"pace - skill", 0.43}, {"age - weight", 0.50}, {"skill moves - pace", 0.55},
        {"pace - movement", 0.75}, {"movement - power", 0.84}, {"attacking - movement", 0.90},
        {"skill moves - power", 0.91}, {"skill - movement", 0.92}, {"skill moves - movement", 0.96},
        {"skill - power", 0.96}, {"skill moves - attacking", 0.97}, {"attacking - power", 0.98},
        {"skill moves - skill", 0.98}, {"height - weight", 0.98}, {"attacking - skill", 1.00}};
    const int n = static_cast<int>(std::size(pairs));

    auto mix = [](QColor a, QColor b, double f) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f,
                                a.blueF() + (b.blueF() - a.blueF()) * f);
    };
    QVector<Bar> bars;
    QVector<double> pos;
    QStringList names;
    for (int i = 0; i < n; ++i) {
        Bar b;
        b.position = n - 1 - i;  // 第一對在最上面
        b.value = pairs[i].r;
        b.size = 0.82;
        b.fillColor = pairs[i].r < 0 ? mix(QColor(245, 170, 110), QColor(150, 20, 45), -pairs[i].r)
                                     : mix(QColor(190, 225, 130), QColor(15, 105, 55), pairs[i].r);
        bars.append(b);
        pos.append(n - 1 - i);
        names.append(QString::fromUtf8(pairs[i].pair));
    }
    plot.addBars(bars)->horizontal = true;
    setTicks(plot.leftAxis(), pos, names);
    plot.leftAxis()->tickFont.size = 7.5;
    plot.style.dataBackground = QColor(234, 234, 242);
    plot.grid.majorLine.color = QColor(255, 255, 255);
    plot.setAxisLimits(-1.1, 1.1, -0.7, n - 0.3);
    plot.bottomAxis()->label.text = QStringLiteral("相關係數");
}

QTPLOT_RECIPE("圖種", "長條與統計", "依類別上色的排名長條",
              "由高到低排序的橫向長條，數值寫在條內起點（labelPlacement = InsideStart，白字）。"
              "顏色表示第二個屬性（授權類型），圖例放在上方。") {
    struct M { const char* name; double elo; int license; };
    const M models[] = {
        {"GPT-4", 1225, 0}, {"Claude-v1", 1195, 0}, {"Claude-instant-v1", 1153, 0}, {"GPT-3.5-turbo", 1143, 0},
        {"Vicuna-13B", 1054, 1}, {"PaLM 2", 1042, 0}, {"Vicuna-7B", 1007, 1}, {"Koala-13B", 980, 1},
        {"mpt-7b-chat", 952, 2}, {"FastChat-T5-3B", 941, 2}, {"Alpaca-13B", 937, 1}, {"RWKV-4-Raven-14B", 928, 2},
        {"Oasst-Pythia-12B", 921, 2}, {"ChatGLM-6B", 921, 1}, {"StableLM-Tuned-Alpha-7B", 882, 3},
        {"Dolly-V2-12B", 866, 3}, {"LLaMA-13B", 854, 1}};
    const char* licenses[] = {"商用專有", "開源／研究", "Apache 2.0", "其他"};
    const QColor colors[] = {QColor(20, 190, 160), QColor(232, 85, 70), QColor(165, 170, 210), QColor(25, 95, 120)};
    const int n = static_cast<int>(std::size(models));

    QVector<Bar> bars;
    QVector<double> pos;
    QStringList names;
    for (int i = 0; i < n; ++i) {
        Bar b;
        b.position = n - 1 - i;
        b.value = models[i].elo;
        b.size = 0.8;
        b.fillColor = colors[models[i].license];
        b.label = QLocale().toString(models[i].elo, 'f', 0);
        b.labelPlacement = Bar::LabelPlacement::InsideStart;
        b.labelColor = QColor(255, 255, 255);
        bars.append(b);
        pos.append(n - 1 - i);
        names.append(QString::fromUtf8(models[i].name));
    }
    auto* bp = plot.addBars(bars);
    bp->horizontal = true;
    bp->valueLabelFont.bold = true;
    bp->valueLabelFont.size = 8.5;
    setTicks(plot.leftAxis(), pos, names);

    for (int c = 0; c < 4; ++c) {
        LegendItem it;
        it.text = QString::fromUtf8(licenses[c]);
        it.fill = FillStyle(colors[c]);
        it.line = LineStyle::none();
        plot.legend.manualItems.append(it);
    }
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
    plot.bottomAxis()->isVisible = false;
    plot.grid.isVisible = false;
    plot.setAxisLimits(0, 1300, -0.6, n - 0.4);
    plot.title.text = QStringLiteral("Chatbot Arena Elo 評分（2023）");
}

// ───────────────────────────── 環狀圖 ─────────────────────────────

QTPLOT_RECIPE("極座標", "環狀圖", "強調單一項目的環狀長條圖",
              "每個類別一道環，數值是弧長。只有一條上色、其他灰色，故事（藍莓破 5000）自己跳出來。") {
    const QStringList fruits = {QStringLiteral("葡萄"), QStringLiteral("香蕉"), QStringLiteral("藍莓"), QStringLiteral("柳橙"),
                                QStringLiteral("蘋果"), QStringLiteral("鳳梨"), QStringLiteral("芒果"), QStringLiteral("草莓")};
    auto* chart = plot.addRadialBarChart({6048, 5975, 5315, 4988, 4943, 4888, 4869, 4601}, fruits);
    chart->barColor = QColor(222, 222, 222);
    chart->colors = {QColor(), QColor(), QColor(20, 70, 230)};
    chart->showTracks = false;
    chart->sweepAngle = 300;
    chart->maxValue = 7000;
    chart->roundCaps = false;
    chart->ringGapFraction = 0.35;
    chart->innerRadiusFraction = 0.55;
    chart->labelFont.bold = true;
    plot.title.text = QStringLiteral("藍莓銷量突破 5000 箱");
}

QTPLOT_RECIPE("極座標", "環狀圖", "距離正確的環狀長條",
              "一般的環狀長條圖裡，同樣的角度在外環是比較長的弧，外環的長條看起來比較大。"
              "equalArcLength 讓每一環上的「弧長」都和數值成正比。") {
    const QStringList cats = {QStringLiteral("Phones"), QStringLiteral("Chairs"), QStringLiteral("Storage"), QStringLiteral("Tables"),
                              QStringLiteral("Binders"), QStringLiteral("Machines"), QStringLiteral("Accessories"), QStringLiteral("Copiers"),
                              QStringLiteral("Bookcases"), QStringLiteral("Appliances"), QStringLiteral("Furnishings"), QStringLiteral("Paper"),
                              QStringLiteral("Supplies"), QStringLiteral("Art"), QStringLiteral("Envelopes"), QStringLiteral("Labels"),
                              QStringLiteral("Fasteners")};
    auto* chart = plot.addRadialBarChart({330007, 328449, 223844, 206966, 203413, 189239, 167380, 149528,
                                          114880, 107532, 91705, 78479, 46674, 27119, 16476, 12486, 3024}, cats);
    chart->equalArcLength = true;
    chart->barColor = QColor(70, 115, 170);
    chart->innerRadiusFraction = 0.2;
    chart->ringGapFraction = 0.55;
    chart->trackColor = QColor(238, 238, 238);
    chart->labelFont.size = 7.5;
    plot.title.text = QStringLiteral("距離正確的環狀長條");
}

QTPLOT_RECIPE("極座標", "環狀圖", "附總數的環狀長條圖",
              "每道環各自一色，灰色軌道標出滿刻度，中央寫總數。") {
    const QStringList countries = {QStringLiteral("美國"), QStringLiteral("中國"), QStringLiteral("日本"), QStringLiteral("英國"),
                                   QStringLiteral("ROC"), QStringLiteral("澳洲"), QStringLiteral("義大利"), QStringLiteral("德國")};
    auto* chart = plot.addRadialBarChart({39, 38, 27, 22, 20, 17, 10, 10}, countries);
    chart->colors = {QColor(0, 100, 210), QColor(230, 0, 30), QColor(240, 70, 150), QColor(255, 175, 0),
                     QColor(0, 170, 170), QColor(50, 170, 20), QColor(200, 210, 40), QColor(225, 50, 80)};
    chart->labelFormat = QStringLiteral("%1（%2）");
    chart->innerRadiusFraction = 0.25;
    chart->centerLabel.text = QStringLiteral("總計（183）");
}

QTPLOT_RECIPE("極座標", "環狀圖", "環狀熱圖",
              "五十州繞一圈、七種法律各一道環。全彩是完整保障、淡色是有限保障、細紋是明文禁止、灰色是沒有法律。"
              "黑線把各區分開。") {
    const QStringList states = QStringLiteral(
        "MI KY OH IN IL VT RI PA NY NJ NH DC MA MD ME DE CT WV VA TN SC NC GA FL LA AL MS AR OK TX "
        "NM NV AZ UT CO HI CA WA OR MT AK ID WY NE SD ND KS MO WI MN").split(QLatin1Char(' '));
    const QStringList rights = {QStringLiteral("婚姻"), QStringLiteral("醫院探視"), QStringLiteral("收養"), QStringLiteral("就業"),
                                QStringLiteral("住房"), QStringLiteral("仇恨犯罪"), QStringLiteral("校園")};
    const QColor colors[] = {QColor(205, 40, 40), QColor(240, 185, 70), QColor(15, 95, 170), QColor(125, 185, 60),
                             QColor(200, 30, 130), QColor(130, 60, 150), QColor(100, 175, 225)};
    const int rings = static_cast<int>(rights.size()), sectors = static_cast<int>(states.size());

    // 地區決定各種保障出現的機率
    auto region = [](int s) { return s <= 4 ? 0 : s <= 16 ? 1 : s <= 29 ? 2 : s <= 42 ? 3 : 0; };
    const double strength[] = {0.35, 0.9, 0.2, 0.65};
    gen::setSeed(5);
    const QVector<double> rolls = gen::randomUniform(rings * sectors);

    QVector<QColor> cells(rings * sectors);
    QVector<bool> striped(rings * sectors, false);
    for (int ring = 0; ring < rings; ++ring)
        for (int s = 0; s < sectors; ++s) {
            const int i = ring * sectors + s;
            const double chance = ring == 0 ? 1.0 : strength[region(s)] - ring * 0.04;
            if (rolls[i] < chance) cells[i] = colors[ring];
            else if (rolls[i] < chance + 0.15) cells[i] = colors[ring].lighter(170);
            else if (rolls[i] > 0.96) striped[i] = true;
        }

    auto* h = plot.addCircularHeatmap(rings, sectors, cells, states);
    h->striped = striped;
    h->separators = {5, 17, 30, 43};
    h->groupLabels = {{QStringLiteral("中西部"), 49.5}, {QStringLiteral("東北"), 11}, {QStringLiteral("東南"), 23.5},
                      {QStringLiteral("西南"), 36.5}, {QStringLiteral("西北"), 41.5}};
    h->centerLabel.text = QStringLiteral("各類權利");
    for (int r = 0; r < rings; ++r) {
        LegendItem it;
        it.text = rights[r];
        it.fill = FillStyle(colors[r]);
        it.line = LineStyle::none();
        h->legend.append(it);
    }
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideRight;
}

// ───────────────────────────── 3D 緞帶與面積 ─────────────────────────────

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 折線圖（緞帶）",
              "每個序列是一條往後排的緞帶，像試算表的 3D 折線圖。"
              "Waterfall3D 設了 thickness、關掉 fillToFloor 就只畫每條曲線的頂面。") {
    const QStringList statuses = {QStringLiteral("回報"), QStringLiteral("受理"), QStringLiteral("指派"), QStringLiteral("確認"),
                                  QStringLiteral("取消"), QStringLiteral("處理中"), QStringLiteral("QP"), QStringLiteral("PE 退回"),
                                  QStringLiteral("PE 需要"), QStringLiteral("PE 審查"), QStringLiteral("技術完成"), QStringLiteral("完成")};
    const QStringList months = {QStringLiteral("6 月"), QStringLiteral("7 月"), QStringLiteral("8 月"), QStringLiteral("9 月"),
                                QStringLiteral("10 月"), QStringLiteral("11 月"), QStringLiteral("12 月")};
    const QVector<QVector<double>> counts = {
        {2, 0, 12, 0, 0, 2, 2, 5, 5, 3, 0, 40},
        {5, 6, 8, 5, 2, 15, 5, 4, 3, 2, 10, 210},
        {8, 10, 18, 10, 8, 30, 5, 5, 6, 8, 10, 160},
        {12, 25, 150, 30, 20, 60, 15, 15, 12, 10, 15, 80},
        {15, 40, 130, 30, 40, 40, 30, 20, 85, 15, 20, 45},
        {20, 25, 30, 20, 18, 20, 18, 15, 15, 15, 15, 20},
        {30, 30, 28, 25, 22, 22, 20, 20, 18, 18, 18, 22}};

    auto w = std::make_unique<Waterfall3D>();
    for (int i = 0; i < statuses.size(); ++i) w->xs.append(i);
    for (int k = 0; k < months.size(); ++k) w->ys.append(k);
    w->zs = counts;
    w->thickness = 0.55;
    w->fillToFloor = false;
    w->fillAlpha = 255;
    for (const QString& m : months) w->curveNames.append(QStringLiteral("2015 年 %1").arg(m));

    auto* chart = plot.addChart3D();
    chart->add(std::move(w));
    chart->xTickFormatter = [statuses](double x) { return statuses.value(static_cast<int>(std::lround(x))); };
    chart->yTickFormatter = [months](double y) { return months.value(static_cast<int>(std::lround(y))); };
    chart->xLabel.clear();
    chart->yLabel.clear();
    chart->zLabel = QStringLiteral("件數");
    chart->aspectX = 1.6;
    chart->aspectY = 0.8;
    chart->aspectZ = 0.6;
    chart->azimuth = -20;
    chart->elevation = 22;
    chart->margin = 0.12;
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideRight;
}

QTPLOT_RECIPE("圖種", "3D 圖表", "3D 面積圖",
              "填到地板的實心板：每個序列沿 Y 擠出成一塊。後面的序列比較高，所以不會被前面的擋住。") {
    const QStringList months = {QStringLiteral("1 月"), QStringLiteral("2 月"), QStringLiteral("3 月"), QStringLiteral("4 月"),
                                QStringLiteral("5 月"), QStringLiteral("6 月"), QStringLiteral("7 月"), QStringLiteral("8 月"),
                                QStringLiteral("9 月"), QStringLiteral("10 月"), QStringLiteral("11 月"), QStringLiteral("12 月")};
    const QStringList series = {QStringLiteral("序列 A"), QStringLiteral("序列 B"), QStringLiteral("序列 C"),
                                QStringLiteral("序列 D"), QStringLiteral("序列 E"), QStringLiteral("序列 F")};
    gen::setSeed(6);
    auto w = std::make_unique<Waterfall3D>();
    for (int i = 0; i < months.size(); ++i) w->xs.append(i);
    for (int s = 0; s < series.size(); ++s) {
        w->ys.append(s);
        const QVector<double> noise = gen::randomUniform(months.size(), -4, 4);
        QVector<double> v;
        for (int m = 0; m < months.size(); ++m) v.append(std::max(4.0, 12 + s * 5 + 10 * std::sin(m / 1.8 + s) + noise[m]));
        w->zs.append(v);
    }
    w->thickness = 0.55;
    w->fillAlpha = 235;
    w->edgeColor = QColor(0, 0, 0, 90);
    w->edgeWidth = 0.8;
    w->palette = Palette(Palette::Category10);

    auto* chart = plot.addChart3D();
    chart->add(std::move(w));
    chart->xTickFormatter = [months](double x) { return months.value(static_cast<int>(std::lround(x))); };
    chart->yTickFormatter = [series](double y) { return series.value(static_cast<int>(std::lround(y))); };
    chart->xLabel.clear();
    chart->yLabel.clear();
    chart->zLabel = QStringLiteral("數量");
    chart->zRange = CoordinateRange(0, 60);
    chart->aspectX = 1.2;
    chart->aspectZ = 0.55;
    chart->azimuth = -60;
    chart->elevation = 30;
}

// ───────────────────────────── 漏斗資訊圖 ─────────────────────────────

QTPLOT_RECIPE("圖種", "商務圖表", "圓錐漏斗加說明",
              "cone3D 把每一段畫成圓錐的一截（上下緣是橢圓），"
              "每段的 description 寫在右邊、和那一段對齊。") {
    struct S { const char* label; const char* notes; QColor color; double width; };
    const S stages[] = {
        {"認知", "• 行銷活動與消費者研究\n• 活動、廣告、研討會、社群媒體", QColor(210, 55, 65), 10},
        {"興趣", "• 介紹產品定位\n• 電子報、精準內容、課程", QColor(245, 125, 60), 8.6},
        {"考慮", "• 產品資訊與優惠\n• 案例研究與免費試用", QColor(250, 175, 30), 7.4},
        {"意圖", "• 產品展示與購物車\n• 業務接手培養名單", QColor(80, 195, 165), 6.1},
        {"評估", "• 行銷與業務證明產品最好", QColor(50, 80, 140), 4.6},
        {"購買", "• 完成交易", QColor(110, 90, 125), 2.6}};
    QVector<double> widths;
    QStringList labels;
    for (const S& s : stages) { widths.append(s.width); labels.append(QString::fromUtf8(s.label)); }

    auto* f = plot.addFunnel(widths, labels);
    for (int i = 0; i < f->stages.size(); ++i) {
        f->stages[i].fillColor = stages[i].color;
        f->stages[i].description = QString::fromUtf8(stages[i].notes);
    }
    f->cone3D = true;
    f->showValues = false;
    f->gapFraction = 0.18;
    f->labelFont.color = QColor(255, 255, 255);
    f->labelFont.bold = true;
    f->descriptionFont.size = 9;
    f->descriptionFont.color = QColor(60, 60, 60);
    f->extent = CoordinateRect(0, 10, 0, 10);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
    plot.title.text = QStringLiteral("行銷漏斗");
}

QTPLOT_RECIPE("圖種", "商務圖表", "空心圓錐漏斗加編號",
              "cap = Hollow 讓每一段看起來像一個空杯子。漏斗後面和旁邊的矩形與文字把它變成一張編號資訊圖的版型。") {
    const QColor colors[] = {QColor(75, 180, 215), QColor(185, 200, 110), QColor(240, 195, 95), QColor(230, 105, 125)};

    // 先畫底下的帶子與編號，漏斗才會蓋在它們上面
    for (int i = 0; i < 4; ++i) {
        const double top = 10 - i * 2.5 - 0.35, bottom = top - 1.8;
        auto* band = plot.addRectangle(5, 17, bottom, top);
        band->fill = FillStyle(QColor(248, 240, 200));
        band->line = LineStyle::none();
        auto* tab = plot.addRectangle(17, 19.5, bottom, top);
        tab->fill = FillStyle(colors[i]);
        tab->line = LineStyle::none();
        auto* num = plot.addText(QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0')), 18.25, (top + bottom) / 2);
        num->alignment = Qt::AlignHCenter | Qt::AlignVCenter;
        num->label.font.size = 15;
        num->label.font.bold = true;
        num->label.font.color = QColor(70, 70, 70);
    }

    auto* f = plot.addFunnel({10, 8.2, 6.6, 5});
    for (int i = 0; i < 4; ++i) f->stages[i].fillColor = colors[i];
    f->cone3D = true;
    f->cap = Funnel::CapStyle::Hollow;
    f->ellipseRatio = 0.2;
    f->gapFraction = 0.12;
    f->extent = CoordinateRect(0, 10, 0.5, 10);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
    plot.setAxisLimits(-0.5, 20, 0, 10.6);
}

// ───────────────────────────── 環形資訊圖 ─────────────────────────────

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "七等分的環形資訊圖",
              "七個等分的扇形各自往外推一點、之間留白邊，扇形裡寫標題、外面寫編號、中央寫主題 —— "
              "資訊圖表常見的版型，用甜甜圈就做得出來。") {
    const QColor colors[] = {QColor(140, 195, 40), QColor(225, 50, 50), QColor(245, 150, 20), QColor(40, 65, 75),
                             QColor(95, 140, 150), QColor(30, 160, 220), QColor(40, 185, 190)};
    QStringList labels;
    for (int i = 0; i < 7; ++i) labels.append(QStringLiteral("選項 %1").arg(i + 1));
    auto* pie = plot.addDonut(QVector<double>(7, 1.0), labels, 0.55);
    for (int i = 0; i < 7; ++i) {
        pie->slices[i].fillColor = colors[i];
        pie->slices[i].explode = 0.04;
    }
    pie->labelMode = Pie::LabelMode::Label;
    pie->labelDistance = 0.78;
    pie->labelFont.bold = true;
    pie->labelFont.color = QColor(255, 255, 255);
    pie->sliceBorder = LineStyle(QColor(255, 255, 255), 4);
    pie->centerLabel.text = QStringLiteral("7 個選項\n資訊圖表");
    pie->centerLabel.font.size = 14;
    pie->centerLabel.font.bold = true;
    pie->centerLabel.font.color = QColor(225, 50, 50);

    // 編號放在每塊外側、靠近它的起點
    for (int i = 0; i < 7; ++i) {
        const double a = (90 - 360.0 / 7 * (i + 0.15)) * kPi / 180;
        auto* t = plot.addText(QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0')), 1.18 * std::cos(a), 1.18 * std::sin(a));
        t->alignment = Qt::AlignHCenter | Qt::AlignVCenter;
        t->label.font.size = 13;
        t->label.font.color = colors[i];
    }
    plot.setAxisLimits(-1.4, 1.4, -1.4, 1.4);
}

// ───────────────────────────── 放射狀長條與扇形圖 ─────────────────────────────

namespace {

/// 只有同心圓、沒有放射線與標籤的極座標軸（當外框或背景格線用）。
PolarAxis* addRings(Plot& plot, QVector<double> radii, LineStyle line) {
    auto* axis = plot.addPolarAxis(1.0, 4);
    axis->ringRadii = std::move(radii);
    axis->ringLine = line;
    axis->spokeLine = LineStyle::none();
    axis->showRingLabels = false;
    axis->labelFont.color = QColor(0, 0, 0, 0);
    return axis;
}

}  // namespace

QTPLOT_RECIPE("極座標", "玫瑰圖", "放射狀標籤的極座標長條圖",
              "半徑是線性的，同心圓可以當刻度讀。名稱沿半徑方向轉（radialLabels），"
              "並放在每一格自己的尖端外面（labelsFollowValue）。") {
    const QStringList sources = {QStringLiteral("風力"), QStringLiteral("水力"), QStringLiteral("核能／其他"),
                                 QStringLiteral("硬煤"), QStringLiteral("生質"), QStringLiteral("天然氣"),
                                 QStringLiteral("太陽能"), QStringLiteral("褐煤")};
    const QVector<double> share = {31.0, 4.2, 5.1, 8.3, 8.4, 10.5, 12.1, 17.7};
    const QVector<QColor> colors = {QColor(135, 206, 235), QColor(0, 0, 255), QColor(128, 0, 128), QColor(0, 0, 0),
                                    QColor(0, 128, 0), QColor(255, 165, 0), QColor(255, 255, 0), QColor(165, 42, 42)};

    // 刻度圈：5、10 … 40（最大值 40 對應半徑 1）
    QVector<double> radii;
    QStringList labels;
    for (int v = 5; v <= 40; v += 5) { radii.append(v / 40.0); labels.append(QString::number(v)); }
    addRings(plot, radii, LineStyle(QColor(128, 128, 128, 110), 1.0));

    QVector<PieSlice> slices;
    for (int i = 0; i < share.size(); ++i) slices.append({share[i], sources[i], colors[i], 0, QString()});
    // 補一格看不見的 40，讓半徑 1 對應 40 而不是最大值 31
    auto* rose = plot.addCoxcomb(slices);
    rose->areaProportional = false;
    rose->clockwise = false;
    rose->startAngle = -360.0 / share.size() / 2;  // 第一格（風力）置中在 3 點鐘方向
    rose->radius = 31.0 / 40.0;                   // 最大值 31 畫在刻度 31 的位置
    rose->sliceBorder = LineStyle(QColor(128, 128, 128), 1.0);
    rose->radialLabels = true;
    rose->labelsFollowValue = true;

    // 刻度數字另外放一根只有文字的軸，排在玫瑰圖之後才不會被扇形蓋掉
    auto* tickLabels = addRings(plot, radii, LineStyle::none());
    tickLabels->ringLabels = labels;
    tickLabels->showRingLabels = true;
    // ringLabelAngle 是相對於這根軸自己的 startAngle（90°，順時針）量的：90 - 78 = 12°
    tickLabels->ringLabelAngle = 78;

    addRings(plot, {1.0}, LineStyle(QColor(0, 0, 0), 1.5));
    plot.title.text = QStringLiteral("2023 年德國能源來源（%）");
    plot.equalAxisScale = true;
    plot.setAxisLimits(-1.1, 1.1, -1.1, 1.1);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
}

QTPLOT_RECIPE("極座標", "玫瑰圖", "有放射線與數值刻度的風花圖",
              "每一格填滿整個方位，黑色放射線把方位分開，刻度沿垂直方向標出次數。") {
    const QStringList compass = QStringLiteral("N NNE NE ENE E ESE SE SSE S SSW SW WSW W WNW NW NNW").split(QLatin1Char(' '));
    const double totals[] = {3300, 3100, 3200, 3400, 3500, 3300, 2900, 2700, 3500, 6300, 6500, 5800, 5000, 4100, 3900, 4100};
    const double split[] = {0.08, 0.42, 0.28, 0.12, 0.05, 0.05};
    const QVector<QColor> colors = {QColor(0, 0, 230), QColor(0, 176, 240), QColor(0, 176, 80),
                                    QColor(255, 255, 0), QColor(255, 192, 0), QColor(255, 0, 0)};
    const int dirs = 16;
    const double maxValue = 8000;

    // 刻度圈與數值（沿正上方）
    QVector<double> radii;
    QStringList labels;
    for (int v = 1000; v <= 8000; v += 1000) { radii.append(v / maxValue); labels.append(QString::number(v)); }
    auto* grid = addRings(plot, radii, LineStyle(QColor(128, 128, 128, 70), 1.0));

    QVector<PieSlice> slices;
    QVector<QVector<double>> stacks;
    double largest = 0;
    for (int d = 0; d < dirs; ++d) {
        slices.append({1, compass[d], QColor(), 0, QString()});
        QVector<double> parts;
        double sum = 0;
        for (int k = 0; k < 6; ++k) {
            const double boost = (d >= 9 && d <= 12) ? (k >= 3 ? 1.6 : 0.9) : 1.0;
            parts.append(totals[d] * split[k] * boost);
            sum += parts.last();
        }
        largest = std::max(largest, sum);
        stacks.append(parts);
    }
    auto* rose = plot.addCoxcomb(slices);
    rose->stacks = stacks;
    rose->stackColors = colors;
    rose->areaProportional = false;
    rose->startAngle = 90 + 360.0 / dirs / 2;  // 第一格（N）置中在正上方
    rose->radius = largest / maxValue;         // 最大的那一格畫到它自己的刻度
    rose->sliceBorder = LineStyle::none();
    rose->labelDistance = 1.08 * maxValue / largest;

    // 黑色放射線在格子的交界上，數值刻度畫在資料上面才看得見
    auto* spokes = plot.addPolarAxis(1.0, dirs);
    spokes->startAngle = 90 + 360.0 / dirs / 2;
    spokes->spokeLine = LineStyle(QColor(0, 0, 0), 1.5);
    spokes->labelFont.color = QColor(0, 0, 0, 0);
    spokes->ringLabels = labels;
    spokes->ringRadii = radii;
    spokes->ringLine = LineStyle(QColor(0, 0, 0, 0), 1.0);
    spokes->showRingLabels = true;
    // 相對於 startAngle（101.25°，順時針）：101.25 - 11.25 = 90°，也就是正上方
    spokes->ringLabelAngle = 360.0 / dirs / 2;
    addRings(plot, {1.0}, LineStyle(QColor(0, 0, 0), 1.5));
    Q_UNUSED(grid);

    plot.equalAxisScale = true;
    plot.setAxisLimits(-1.25, 1.25, -1.25, 1.25);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
}

QTPLOT_RECIPE("極座標", "環狀圖", "半圓扇形圖",
              "sweepAngle = 180 的環狀熱圖：每一列是一個扇區、每一道環是一個量，格子裡寫數值（cellLabels）。"
              "左邊的圓點用引線連到各自的扇區。") {
    const QVector<QColor> rows = {QColor(20, 150, 140), QColor(250, 165, 30), QColor(30, 85, 130), QColor(150, 200, 20),
                                  QColor(130, 20, 100), QColor(105, 110, 125), QColor(10, 160, 220)};
    const QStringList outer = {QStringLiteral("4%"), QStringLiteral("40%"), QStringLiteral("4%"), QStringLiteral("40%"),
                               QStringLiteral("4%"), QStringLiteral("40%"), QStringLiteral("4%")};
    const QStringList middle = {QStringLiteral("3%"), QStringLiteral("30%"), QStringLiteral("3%"), QStringLiteral("30%"),
                                QStringLiteral("3%"), QStringLiteral("30%"), QStringLiteral("3%")};
    const QStringList inner = {QStringLiteral("2%"), QStringLiteral("2%"), QStringLiteral("2%"), QStringLiteral("20%"),
                               QStringLiteral("2%"), QStringLiteral("2%"), QStringLiteral("2%")};
    const int n = static_cast<int>(rows.size());

    QVector<QColor> cells(3 * n);
    QStringList text;
    for (int ring = 0; ring < 3; ++ring)
        for (int s = 0; s < n; ++s) {
            cells[ring * n + s] = ring == 0 ? rows[s] : rows[s].darker(ring == 1 ? 135 : 200);
            text.append((ring == 0 ? outer : ring == 1 ? middle : inner)[s]);
        }

    auto* fan = plot.addCircularHeatmap(3, n, cells);
    fan->sweepAngle = 180;
    fan->innerRadiusFraction = 0.25;
    fan->cellLabels = text;
    fan->cellLabelFont.bold = true;
    fan->cellLabelFont.size = 8;
    fan->cellBorder = LineStyle(QColor(10, 25, 35), 1.5);

    for (int s = 0; s < n; ++s) {
        const double y = 0.9 - 1.8 * s / (n - 1);
        const double a = fan->sectorAngle(s + 0.5) * kPi / 180;
        auto* leader = plot.addScatter({-1.6, -0.6, 0.25 * std::cos(a)}, {y, y, 0.25 * std::sin(a)}, rows[s]);
        leader->marker = MarkerStyle::none();
        leader->line.width = 1.2;
        auto* dot = plot.addMarkers({-1.6}, {y}, rows[s]);
        dot->marker = MarkerStyle(MarkerShape::FilledCircle, 12, rows[s]);
    }

    plot.style.figureBackground = QColor(10, 25, 35);
    plot.style.dataBackground = QColor(10, 25, 35);
    plot.setAxisLimits(-1.9, 1.15, -1.15, 1.15);
}

QTPLOT_RECIPE("極座標", "環狀圖", "外圈色帶的極座標長條",
              "十二格長在七圈的編號格線上，外面再一圈同色的弧形色帶與圓點 —— "
              "玫瑰圖加 Sector 與標記就組得出這種資訊圖版型。") {
    const QVector<QColor> palette = {QColor(26, 150, 160), QColor(0, 160, 225), QColor(130, 20, 100),
                                     QColor(245, 160, 30), QColor(150, 200, 20), QColor(30, 85, 130)};
    const QVector<double> values = {4, 3, 6, 3, 4, 5, 4, 6, 3, 4, 5, 6};
    const int n = static_cast<int>(values.size());

    QVector<PieSlice> slices;
    for (int i = 0; i < n; ++i) slices.append({values[i], QString(), palette[i % palette.size()], 0, QString()});
    auto* rose = plot.addCoxcomb(slices);
    rose->areaProportional = false;
    rose->radius = 6.0 / 7.0;  // 最大值 6 畫在第 6 圈
    rose->sliceBorder = LineStyle::none();

    // 七圈格線，編號沿正下方；放射線在格子交界
    QVector<double> radii;
    QStringList labels;
    for (int k = 1; k <= 7; ++k) { radii.append(k / 7.0); labels.append(QString::number(k)); }
    auto* grid = plot.addPolarAxis(1.0, n);
    grid->ringRadii = radii;
    grid->ringLabels = labels;
    grid->showRingLabels = true;
    grid->ringLabelAngle = 180;  // 從 startAngle（90°）順時針轉 180° 就是正下方
    grid->ringLine = LineStyle(QColor(40, 50, 60), 1.0);
    grid->spokeLine = LineStyle(QColor(40, 50, 60), 1.0);
    grid->labelFont.color = QColor(0, 0, 0, 0);

    // 外圈色帶與圓點
    const double step = 360.0 / n;
    for (int i = 0; i < n; ++i) {
        const double start = 90 - step * (i + 1);
        auto* band = plot.addSector(0, 0, 1.16, start + 1, step - 2);
        band->innerRadiusX = band->innerRadiusY = 1.08;
        band->fill = FillStyle(palette[i % palette.size()]);
        band->line = LineStyle::none();
        const double a = (start + step) * kPi / 180;
        auto* dot = plot.addMarkers({1.12 * std::cos(a)}, {1.12 * std::sin(a)});
        dot->marker = MarkerStyle(MarkerShape::FilledCircle, 8, palette[i % palette.size()].darker(140));
    }

    plot.equalAxisScale = true;
    plot.setAxisLimits(-1.25, 1.25, -1.25, 1.25);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
}

// ───────────────────────────── 標註散點圖 ─────────────────────────────

namespace {

/// 最小平方迴歸線在 [x1, x2] 之間的兩端點，以及皮爾森相關係數。
struct FitResult { Coordinates a, b; double r; };
FitResult fitLine(const QVector<double>& xs, const QVector<double>& ys, double x1, double x2) {
    const LinearFit f = linearFit(xs, ys);
    const double r = (f.slope >= 0 ? 1 : -1) * std::sqrt(std::max(0.0, f.rSquared));
    return {{x1, f.slope * x1 + f.intercept}, {x2, f.slope * x2 + f.intercept}, r};
}

}  // namespace

QTPLOT_RECIPE("圖種", "散點與線", "分組標名加趨勢線",
              "依組別上色、每點標上名字，再對全部的點畫一條虛線趨勢線。圖例橫放在標題下，兼當色票。") {
    struct A { const char* name; double months, above; int country; };
    const A analysts[] = {
        {"J. Panner", 46, 45, 1}, {"T. Halmkin", 46, 35, 0}, {"A. Duckerin", 36, 31, 1}, {"P. Feldstein", 43, 25, 0},
        {"C. Spires", 30, 23, 1}, {"L. Sprull", 28, 20, 1}, {"J. Dunks", 34, 20, 0}, {"L. Bobb", 33, 17, 2},
        {"C. Gurrado", 36, 17, 1}, {"F. Nibloe", 47, 19, 1}, {"A. Bampkin", 48, 18, 0}, {"B. Byrom", 18, 11, 0},
        {"W. Phant", 17, 10, 1}, {"M. Butson", 37, 6, 2}, {"R. Skellorne", 13, 4, 1}, {"F. Ferrey", 14, 1, 0},
        {"A. Cabbell", 21, 2, 1}, {"S. Anstey", 24, 1, 0}, {"F. Arend", 31, 2, 0}};
    const QStringList countries = {QStringLiteral("美國"), QStringLiteral("日本"), QStringLiteral("加拿大")};
    const QColor colors[] = {QColor(40, 120, 200), QColor(190, 70, 70), QColor(240, 190, 30)};

    QVector<double> allX, allY;
    for (int c = 0; c < 3; ++c) {
        QVector<double> xs, ys;
        for (const A& a : analysts)
            if (a.country == c) { xs.append(a.months); ys.append(a.above); }
        auto* m = plot.addMarkers(xs, ys, colors[c]);
        m->marker = MarkerStyle(MarkerShape::FilledCircle, 9, colors[c]);
        m->legendText = countries[c];
        allX += xs;
        allY += ys;
    }
    for (const A& a : analysts) {
        auto* t = plot.addText(QString::fromUtf8(a.name), a.months, a.above);
        t->alignment = Qt::AlignHCenter | Qt::AlignBottom;
        t->offsetY = -7;
        t->label.font.size = 8;
        t->label.font.color = QColor(70, 70, 70);
    }
    const FitResult fit = fitLine(allX, allY, 13, 48);
    auto* trend = plot.addLabeledLine(fit.a, fit.b, QString(), QColor(128, 128, 128));
    trend->line = LineStyle(QColor(128, 128, 128), 1.5, LinePattern::Dotted);

    plot.title.text = QStringLiteral("試辦計畫後分析師的表現");
    plot.bottomAxis()->label.text = QStringLiteral("任職月數");
    plot.leftAxis()->label.text = QStringLiteral("表現高於平均的月數");
    plot.setAxisLimits(0, 50, 0, 50);
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("圖種", "散點與線", "依類別換標記形狀",
              "六組同時用顏色和標記形狀區分 —— 色盲讀者或黑白列印時仍然分得出來。") {
    struct C { const char* name; MarkerShape shape; QColor color; double fertility, life; int count; };
    const C continents[] = {
        {"非洲", MarkerShape::FilledSquare, QColor(90, 180, 80), 5.2, 55, 45},
        {"亞洲", MarkerShape::FilledDiamond, QColor(230, 120, 70), 2.6, 70, 45},
        {"大洋洲", MarkerShape::FilledTriangleUp, QColor(120, 135, 190), 3.6, 66, 10},
        {"歐洲", MarkerShape::Asterisk, QColor(230, 120, 200), 1.5, 76, 40},
        {"北美洲", MarkerShape::HashTag, QColor(120, 200, 90), 2.2, 73, 20},
        {"南美洲", MarkerShape::Plus, QColor(230, 200, 40), 2.5, 73, 12}};
    gen::setSeed(2);
    plot.setStyle(PlotStyle::dark());
    for (const C& c : continents) {
        const QVector<double> f0 = gen::randomNormal(c.count, c.fertility, 0.6), noise = gen::randomNormal(c.count, 0, 3);
        QVector<double> fertility, life;
        for (int i = 0; i < c.count; ++i) {
            fertility.append(std::max(1.0, f0[i]));
            life.append(c.life - (fertility.last() - c.fertility) * 3 + noise[i]);
        }
        auto* m = plot.addMarkers(fertility, life, c.color);
        m->marker = MarkerStyle(c.shape, 8, c.color);
        m->legendText = QString::fromUtf8(c.name);
    }
    plot.title.text = QStringLiteral("各洲的生育率與預期壽命");
    plot.bottomAxis()->label.text = QStringLiteral("生育率");
    plot.leftAxis()->label.text = QStringLiteral("預期壽命");
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideRight;
}

QTPLOT_RECIPE("統計", "迴歸", "迴歸線加相關係數標籤",
              "每組一條迴歸線、全部再一條，各自標上皮爾森相關係數。"
              "LabeledLine 依線在螢幕上的實際角度寫字，所以縮放視窗後字仍然貼著線。") {
    gen::setSeed(9);
    struct G { const char* name; QColor color; double slope, intercept, noise; int count; double textAt; };
    const G groups[] = {{"0", QColor(30, 160, 230), -1.1, 215, 15, 140, 0.05},
                        {"1", QColor(220, 70, 50), -0.5, 170, 22, 150, 0.05}};
    QVector<double> allX, allY;
    for (const G& g : groups) {
        const QVector<double> age = gen::randomNormal(g.count, 56, 9), noise = gen::randomNormal(g.count, 0, g.noise);
        QVector<double> hr;
        for (int i = 0; i < g.count; ++i) hr.append(g.intercept + g.slope * age[i] + noise[i]);
        allX += age;
        allY += hr;
        QColor fill = g.color;
        fill.setAlpha(115);
        auto* m = plot.addMarkers(age, hr, g.color);
        m->marker = MarkerStyle(MarkerShape::FilledCircle, 9, fill);
        m->marker.outline = LineStyle(g.color, 1.0);
        m->legendText = QString::fromUtf8(g.name);

        const FitResult fit = fitLine(age, hr, 28, 78);
        auto* l = plot.addLabeledLine(fit.a, fit.b, QStringLiteral("r = %1").arg(fit.r, 0, 'f', 2), g.color.darker(120));
        l->textPosition = g.textAt;
    }
    const FitResult all = fitLine(allX, allY, 28, 78);
    auto* overall = plot.addLabeledLine(all.a, all.b, QStringLiteral("r = %1").arg(all.r, 0, 'f', 2), QColor(60, 60, 60));
    overall->textPosition = 0.92;
    overall->textOffsetPx = -4;

    plot.bottomAxis()->label.text = QStringLiteral("年齡");
    plot.leftAxis()->label.text = QStringLiteral("最大心率");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
}

QTPLOT_RECIPE("統計", "迴歸", "分類區域",
              "背景依「這一點最可能屬於哪一類」上色（比較每一類的核密度），類別難以分辨的地方顏色變淡。"
              "ColorGrid 每格一個顏色；迴歸線上沿線寫相關係數。") {
    gen::setSeed(4);
    struct S { const char* name; QColor color; double length, width, sdL, sdW, slope; };
    const S species[] = {{"Iris-setosa", QColor(40, 170, 240), 1.46, 0.25, 0.17, 0.1, 0.2},
                         {"Iris-versicolor", QColor(240, 80, 70), 4.26, 1.33, 0.47, 0.2, 0.35},
                         {"Iris-virginica", QColor(140, 210, 40), 5.55, 2.03, 0.55, 0.27, 0.15}};
    QVector<QVector<double>> lengths, widths;
    for (const S& s : species) {
        const QVector<double> l = gen::randomNormal(50, s.length, s.sdL), noise = gen::randomNormal(50, 0, s.sdW * 0.8);
        QVector<double> w;
        for (int i = 0; i < 50; ++i) w.append(s.width + s.slope * (l[i] - s.length) + noise[i]);
        lengths.append(l);
        widths.append(w);
    }

    // 每一格算三類的核密度，取最大的那一類上色；越難分辨越淡
    const int cols = 90, rows = 90;
    const double x0 = 0.9, x1 = 7, y0 = 0, y1 = 2.6;
    QVector<QColor> cells;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            const double x = x0 + (c + 0.5) * (x1 - x0) / cols, y = y1 - (r + 0.5) * (y1 - y0) / rows;
            double dens[3], total = 0;
            for (int k = 0; k < 3; ++k) {
                dens[k] = 1e-9;
                for (int i = 0; i < 50; ++i)
                    dens[k] += std::exp(-(std::pow((x - lengths[k][i]) / 0.6, 2) + std::pow((y - widths[k][i]) / 0.3, 2)) / 2);
                total += dens[k];
            }
            const int best = static_cast<int>(std::max_element(dens, dens + 3) - dens);
            QColor col = species[best].color;
            col.setAlphaF(0.1 + 0.6 * (dens[best] / total - 1.0 / 3));
            cells.append(col);
        }
    plot.addColorGrid(rows, cols, cells, CoordinateRect(x0, x1, y0, y1))->smooth = true;

    QVector<double> allL, allW;
    for (int k = 0; k < 3; ++k) {
        QColor fill = species[k].color;
        fill.setAlpha(150);
        auto* m = plot.addMarkers(lengths[k], widths[k], species[k].color);
        m->marker = MarkerStyle(MarkerShape::FilledCircle, 9, fill);
        m->legendText = QString::fromUtf8(species[k].name);
        const FitResult fit = fitLine(lengths[k], widths[k], x0, x1);
        plot.addLabeledLine(fit.a, fit.b, QStringLiteral("r = %1").arg(fit.r, 0, 'f', 2), species[k].color.darker(115))
            ->textPosition = 0.85;
        allL += lengths[k];
        allW += widths[k];
    }
    const FitResult all = fitLine(allL, allW, x0, x1);
    plot.addLabeledLine(all.a, all.b, QStringLiteral("r = %1").arg(all.r, 0, 'f', 2), QColor(50, 50, 50))->textPosition = 0.8;

    plot.setAxisLimits(x0, x1, y0, y1);
    plot.bottomAxis()->label.text = QStringLiteral("花瓣長");
    plot.leftAxis()->label.text = QStringLiteral("花瓣寬");
    plot.grid.isVisible = false;
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
}

QTPLOT_RECIPE("圖種", "商務圖表", "氣泡加分組趨勢與平均線",
              "泡泡大小是第三個變數，平滑曲線是每個部門的趨勢，虛線標出整體平均 —— "
              "每顆泡泡都讀得出高於或低於平均。") {
    gen::setSeed(12);
    struct D { const char* name; QColor color; double a, b, noise; };
    const D departments[] = {{"研發", QColor(30, 90, 220), 25, 0.3, 6},
                             {"會計與財務", QColor(30, 180, 50), 90, -0.75, 8},
                             {"業務與行銷", QColor(250, 190, 40), 50, 1.1, 6}};
    const QStringList names[] = {
        {"Timothy", "Paul", "Joseph", "Richard", "Michael", "Patrick", "Christopher", "Bowles", "Pryor", "David"},
        {"Jonathan", "Pruden", "Frank", "Collins", "Bowman", "Thompson", "Francis", "Colbert", "Walker", "Gerald"},
        {"Jerome", "Petersen", "Boyle", "Winship", "Ronald", "Carlson", "Rendon", "Gomez", "Guerrier", "Pierce"}};

    QVector<double> allAge, allScore;
    for (int d = 0; d < 3; ++d) {
        QVector<double> ages = gen::randomUniform(10, 22, 58);
        for (double& a : ages) a = std::round(a);
        std::sort(ages.begin(), ages.end());
        const QVector<double> noise = gen::randomNormal(10, 0, departments[d].noise), sizes = gen::randomUniform(10, 1, 10);
        QVector<double> scores;
        for (int i = 0; i < 10; ++i)
            scores.append(std::clamp(departments[d].a + departments[d].b * ages[i] + noise[i], 5.0, 98.0));
        allAge += ages;
        allScore += scores;

        auto* b = plot.addBubbles(ages, scores, sizes);
        for (Bubble& bubble : b->bubbles) bubble.fillColor = departments[d].color;
        b->maxDiameterPx = 30;
        b->minDiameterPx = 8;
        b->minLabelDiameterPx = 999;  // 名字寫在泡泡旁邊
        b->border = LineStyle::none();
        b->legendText = QString::fromUtf8(departments[d].name);
        for (int i = 0; i < 10; ++i) {
            auto* t = plot.addText(names[d][i], ages[i], scores[i]);
            t->alignment = Qt::AlignLeft | Qt::AlignTop;
            t->offsetX = 8;
            t->offsetY = 4;
            t->label.font.size = 7.5;
            t->label.font.color = QColor(90, 90, 90);
        }

        // 趨勢：每一點取年齡相差 8 歲以內的鄰居平均
        QVector<double> smooth;
        for (int i = 0; i < 10; ++i) {
            double sum = 0;
            int n = 0;
            for (int j = 0; j < 10; ++j)
                if (std::abs(ages[j] - ages[i]) <= 8) { sum += scores[j]; ++n; }
            smooth.append(sum / n);
        }
        auto* trend = plot.addScatter(ages, smooth, departments[d].color);
        trend->marker = MarkerStyle::none();
        trend->line.width = 2.5;
        trend->interpolation = Scatter::Interpolation::SmoothMonotone;
    }

    double meanAge = 0, meanScore = 0;
    for (double v : allAge) meanAge += v / allAge.size();
    for (double v : allScore) meanScore += v / allScore.size();
    auto* h = plot.addHorizontalLine(meanScore, QColor(128, 128, 128));
    h->text.text = QStringLiteral("平均 = %1").arg(meanScore, 0, 'f', 2);
    auto* v = plot.addVerticalLine(meanAge, QColor(128, 128, 128));
    v->text.text = QStringLiteral("平均 = %1").arg(meanAge, 0, 'f', 2);

    plot.title.text = QStringLiteral("各年齡員工的績效分析");
    plot.bottomAxis()->label.text = QStringLiteral("年齡");
    plot.leftAxis()->label.text = QStringLiteral("績效分數");
    plot.setAxisLimits(18, 62, 0, 100);
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideBottom;
    plot.legend.horizontal = true;
}

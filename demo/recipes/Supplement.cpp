// Supplement.cpp — 對照 ScottPlot 5 Cookbook 補上的範例。
//
// 這些功能函式庫早就有了，只是一直沒有範例：圖元管理、直方圖的幾種讀法、
// 浮動長條、熱圖的儲存格標籤、手動圖例、版面邊距、窗函數……
// 章節沿用既有的分類，所以在瀏覽器裡會和同類的範例排在一起。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

#include <algorithm>
#include <cmath>

using namespace qtplot;

namespace {

/// 固定箱界的分箱。兩組資料要疊在一起比的時候，箱界必須相同 ——
/// 各自用 Histogram::fromValues 分箱的話，兩組的箱子對不齊，
/// 疊出來的長條一高一低其實是箱界錯開造成的。
struct FixedBins {
    QVector<double> centers;
    QVector<double> counts;
    double width = 1;
};

FixedBins binFixed(const QVector<double>& values, double lo, double hi, int count) {
    FixedBins b;
    b.width = (hi - lo) / count;
    b.counts.fill(0, count);
    for (int i = 0; i < count; ++i) b.centers.append(lo + (i + 0.5) * b.width);
    for (double v : values) {
        const int i = static_cast<int>(std::floor((v - lo) / b.width));
        if (i >= 0 && i < count) b.counts[i] += 1;
    }
    return b;
}

QVector<Bar> toBars(const QVector<double>& centers, const QVector<double>& heights,
                    double width, QColor color) {
    QVector<Bar> bars;
    for (int i = 0; i < centers.size(); ++i) {
        Bar b;
        b.position = centers[i];
        b.value = heights[i];
        b.size = width;
        b.fillColor = color;
        bars.append(b);
    }
    return bars;
}

double meanOf(const QVector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return v.isEmpty() ? 0 : s / v.size();
}

/// 樣本標準差（分母 n−1）。
double sdOf(const QVector<double>& v) {
    if (v.size() < 2) return 0;
    const double m = meanOf(v);
    double s = 0;
    for (double x : v) s += (x - m) * (x - m);
    return std::sqrt(s / (v.size() - 1));
}

void setCategoryTicks(Axis* axis, const QVector<double>& positions, const QStringList& names) {
    auto g = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < positions.size() && i < names.size(); ++i)
        g->ticks.append({positions[i], names[i]});
    axis->tickGenerator = g;
}

/// 取得圖上唯一的某種圖元（即時範例 frame > 0 時要拿它來加資料）。
template <typename T>
T* firstOf(Plot& plot) {
    for (IPlottable* p : plot.plottables())
        if (auto* t = dynamic_cast<T*>(p)) return t;
    return nullptr;
}

}  // namespace

// ═══════════════════════════ 圖元管理 ═══════════════════════════

QTPLOT_RECIPE("入門", "圖元管理", "移除某個圖元",
              "Plot::remove() 把一個圖元從圖上拿掉並釋放它。"
              "Plot 擁有圖元，所以 remove 之後手上那個指標就失效了，不要再碰。"
              "這裡加了三條曲線，把中間那條移除。") {
    const QVector<double> xs = gen::consecutive(100);
    auto* a = plot.addScatter(xs, gen::sin(100, 1.0, 1.0));
    auto* b = plot.addScatter(xs, gen::sin(100, 1.0, 2.0));
    auto* c = plot.addScatter(xs, gen::sin(100, 1.0, 3.0));
    a->legendText = RT("1 個週期");
    b->legendText = RT("2 個週期（會被移除）");
    c->legendText = RT("3 個週期");
    for (Scatter* s : {a, b, c}) s->marker = MarkerStyle::none();

    plot.remove(b);  // b 從這裡開始是懸空指標
    plot.legend.isVisible = true;
    plot.title.text = RT("剩下 %1 個圖元").arg(plot.plottableCount());
}

QTPLOT_RECIPE("入門", "圖元管理", "移除某一類的全部圖元",
              "plottables() 回傳的是一份複本，所以可以一邊走訪一邊 remove。"
              "用 dynamic_cast 篩出某一類（這裡是所有 AxisLine），資料曲線則留著。") {
    for (int i = 0; i < 3; ++i) plot.addSignal(gen::randomWalk(200))->line.width = 1.5;
    for (double y : {-10.0, 0.0, 10.0}) plot.addHorizontalLine(y, QColor(200, 0, 0));
    for (double x : {50.0, 150.0}) plot.addVerticalLine(x, QColor(200, 0, 0));
    const int before = plot.plottableCount();

    int removed = 0;
    for (IPlottable* p : plot.plottables()) {
        if (dynamic_cast<AxisLine*>(p)) {
            plot.remove(p);
            ++removed;
        }
    }
    plot.title.text = RT("原本 %1 個圖元，移除 %2 條參考線")
                          .arg(before)
                          .arg(removed);
}

QTPLOT_RECIPE("入門", "圖元管理", "隱藏而不移除",
              "只是暫時不想看的話設 isVisible = false 就好，圖元與樣式都還在，"
              "改回 true 就回來了。隱藏的圖元不參與自動範圍，"
              "圖例預設仍會列出它（畫成灰色），在 Cookbook 裡點那一列就能切回來。") {
    const QVector<double> xs = gen::consecutive(80);
    const QStringList names{RT("北區"), RT("中區"),
                            RT("南區"), RT("東區")};
    for (int i = 0; i < names.size(); ++i) {
        auto* s = plot.addScatter(xs, gen::sin(80, 1.0 + i * 0.3, 1.5, i * 0.6, i * 0.5));
        s->marker = MarkerStyle::none();
        s->line.width = 2;
        s->legendText = names[i];
        if (i == 1 || i == 3) s->isVisible = false;
    }
    plot.legend.isVisible = true;
    plot.title.text = RT("中區與東區被隱藏（圖例裡是灰的）");
}

// ═══════════════════════════ 直方圖 ═══════════════════════════

QTPLOT_RECIPE("統計", "直方圖", "機率密度加常態曲線",
              "Histogram::densities() 把次數換成機率密度（長條總面積為 1），"
              "這樣才能和理論的機率密度函數畫在同一根軸上比較。"
              "曲線用樣本的平均數與標準差代入 normalPdf()。") {
    const QVector<double> data = gen::randomNormal(1000, 50, 10);
    const Histogram h = Histogram::fromValues(data, 30);
    auto* bars = plot.addBars(toBars(h.binCenters, h.densities(), h.binWidth,
                                     QColor(100, 150, 210)));
    bars->borderLine = LineStyle(QColor(255, 255, 255), 1.0);
    bars->legendText = RT("樣本（n = 1000）");

    const double m = meanOf(data);
    const double s = sdOf(data);
    auto* curve = plot.addFunction([m, s](double x) { return normalPdf(x, m, s); },
                                   QColor(214, 39, 40));
    curve->line.width = 2.5;
    curve->legendText = QStringLiteral("N(%1, %2²)").arg(m, 0, 'f', 1).arg(s, 0, 'f', 1);

    plot.legend.isVisible = true;
    plot.bottomAxis()->label.text = RT("數值");
    plot.leftAxis()->label.text = RT("機率密度");
}

QTPLOT_RECIPE("統計", "直方圖", "累積機率直方圖",
              "把每一箱的次數往右累加再除以 n，長條高度就是「小於等於這一箱右界的比例」。"
              "疊上理論的累積分布函數 normalCdf()，兩者越貼近，資料越接近常態。") {
    const QVector<double> data = gen::randomNormal(500, 0, 1);
    const Histogram h = Histogram::fromValues(data, 25);
    QVector<double> cum;
    double running = 0;
    for (double c : h.counts) {
        running += c;
        cum.append(running / data.size());
    }
    auto* bars = plot.addBars(toBars(h.binCenters, cum, h.binWidth, QColor(150, 190, 120)));
    bars->borderLine = LineStyle(QColor(255, 255, 255), 1.0);
    bars->legendText = RT("累積比例");

    auto* cdf = plot.addFunction([](double x) { return normalCdf(x); }, QColor(30, 30, 30));
    cdf->line.width = 2;
    cdf->line.pattern = LinePattern::Dashed;
    cdf->legendText = QStringLiteral("Φ(x)");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
    plot.setAxisLimits(-3.5, 3.5, 0, 1.05);
    plot.leftAxis()->label.text = RT("累積機率");
}

QTPLOT_RECIPE("統計", "直方圖", "兩組直方圖重疊",
              "兩組要疊在一起比，箱界一定要相同。各自分箱的話箱子會錯開，"
              "看到的高低差有一部分只是箱界不同造成的。"
              "這裡兩組都用 [0, 100) 切 25 箱，並用半透明讓重疊處看得見。") {
    const QVector<double> a = gen::randomNormal(800, 42, 9);
    const QVector<double> b = gen::randomNormal(800, 58, 12);
    const FixedBins ha = binFixed(a, 0, 100, 25);
    const FixedBins hb = binFixed(b, 0, 100, 25);

    auto* pa = plot.addBars(toBars(ha.centers, ha.counts, ha.width, QColor(31, 119, 180, 130)));
    pa->legendText = RT("對照組");
    auto* pb = plot.addBars(toBars(hb.centers, hb.counts, hb.width, QColor(255, 127, 14, 130)));
    pb->legendText = RT("實驗組");

    plot.legend.isVisible = true;
    plot.bottomAxis()->label.text = RT("分數");
    plot.leftAxis()->label.text = RT("人數");
}

QTPLOT_RECIPE("統計", "直方圖", "次數與機率密度雙軸",
              "左軸是次數、右軸是機率密度，兩者只差一個常數 n × 箱寬。"
              "兩根軸的範圍照這個比例設，長條和曲線就會同時對上兩邊的刻度 —— "
              "讀者要絕對數量看左邊，要和理論分布比就看右邊。") {
    const QVector<double> data = gen::randomNormal(400, 170, 7);
    const Histogram h = Histogram::fromValues(data, 20);
    auto* bars = plot.addBars(toBars(h.binCenters, h.counts, h.binWidth, QColor(120, 120, 200)));
    bars->borderLine = LineStyle(QColor(255, 255, 255), 1.0);
    bars->legendText = RT("次數（左軸）");

    double maxCount = 0;
    for (double c : h.counts) maxCount = std::max(maxCount, c);
    const double top = maxCount * 1.15;
    const double scale = data.size() * h.binWidth;  // 次數 = 密度 × n × 箱寬

    plot.leftAxis()->setRange(0, top);
    plot.leftAxis()->label.text = RT("次數");

    auto* right = plot.addAxis(Edge::Right);
    right->setRange(0, top / scale);
    right->label.text = RT("機率密度");

    const double m = meanOf(data);
    const double s = sdOf(data);
    auto* curve = plot.addFunction([m, s](double x) { return normalPdf(x, m, s); },
                                   QColor(214, 39, 40));
    curve->yAxis = right;
    curve->line.width = 2.5;
    curve->legendText = RT("常態密度（右軸）");

    plot.legend.isVisible = true;
    plot.bottomAxis()->label.text = RT("身高 (cm)");
}

// ═══════════════════════════ 核密度與常態分布 ═══════════════════════════

QTPLOT_RECIPE("統計", "箱型與分布", "核密度的頻寬",
              "核密度估計的形狀幾乎完全由頻寬決定。silvermanBandwidth() 是常用的預設，"
              "但它假設資料接近常態 —— 遇到雙峰時往往太寬，把兩個峰抹成一個。"
              "下方的短線是原始資料（rug），三條曲線分別用 0.3×、1×、3× 的 Silverman 頻寬。") {
    QVector<double> data = gen::randomNormal(150, 0, 1);
    data += gen::randomNormal(100, 4, 0.8);
    const double bw = silvermanBandwidth(data);
    const QVector<double> xs = gen::linspace(-4, 8, 400);

    const struct {
        double factor;
        QColor color;
    } variants[] = {{0.3, QColor(44, 160, 44)}, {1.0, QColor(31, 119, 180)},
                    {3.0, QColor(214, 39, 40)}};
    for (const auto& v : variants) {
        auto* s = plot.addScatter(xs, kernelDensity(data, xs, bw * v.factor), v.color);
        s->marker = MarkerStyle::none();
        s->line.width = 2;
        s->legendText = RT("%1× 頻寬（h = %2）")
                            .arg(v.factor, 0, 'g', 2)
                            .arg(bw * v.factor, 0, 'f', 2);
    }

    QVector<double> rugY(data.size(), -0.015);
    auto* rug = plot.addMarkers(data, rugY, QColor(0, 0, 0, 120));
    rug->marker = MarkerStyle(MarkerShape::VerticalBar, 10, QColor(0, 0, 0, 120));

    plot.legend.isVisible = true;
    plot.leftAxis()->label.text = RT("密度");
}

QTPLOT_RECIPE("統計", "常態分布", "常態分布的尾端面積",
              "normalQuantile(0.975) 給出 z = 1.96，兩側塗色區域合起來就是 5%。"
              "面積同時用兩種方法算：normalCdf() 的解析解，"
              "以及對塗色那段曲線做 trapezoidArea() 數值積分 —— 兩者應該只差到小數第四位。") {
    const double z = normalQuantile(0.975);
    const QVector<double> xs = gen::linspace(-4, 4, 401);
    QVector<double> ys;
    for (double x : xs) ys.append(normalPdf(x));

    QVector<double> tx = gen::linspace(z, 4, 200);
    QVector<double> ty, zeros(tx.size(), 0.0);
    for (double x : tx) ty.append(normalPdf(x));
    QVector<double> lx;
    for (double x : tx) lx.prepend(-x);
    QVector<double> ly = ty;
    std::reverse(ly.begin(), ly.end());

    const QColor tail(214, 39, 40, 110);
    auto* right = plot.addFillY(tx, zeros, ty, tail);
    right->legendText = RT("拒絕域");
    plot.addFillY(lx, zeros, ly, tail);

    auto* curve = plot.addScatter(xs, ys, QColor(30, 30, 30));
    curve->marker = MarkerStyle::none();
    curve->line.width = 2;
    curve->legendText = RT("標準常態 φ(x)");

    for (double at : {-z, z}) {
        auto* l = plot.addVerticalLine(at, QColor(214, 39, 40));
        l->line.pattern = LinePattern::Dashed;
    }

    const double exact = 2 * (1 - normalCdf(z));
    const double numeric = 2 * trapezoidArea(tx, ty);
    plot.addAnnotation(RT("z = %1\n解析：2·(1 − Φ(z)) = %2\n數值積分（至 |x| = 4）= %3")
                           .arg(z, 0, 'f', 3)
                           .arg(exact, 0, 'f', 5)
                           .arg(numeric, 0, 'f', 5),
                       Qt::AlignLeft | Qt::AlignTop);
    plot.legend.isVisible = true;
    plot.setAxisLimits(-4, 4, 0, 0.45);
}

// ═══════════════════════════ 浮動長條 ═══════════════════════════

QTPLOT_RECIPE("圖種", "長條與統計", "浮動長條（範圍圖）",
              "Bar 的 valueBase 是長條的底，value 是頂，兩個都設就是一根懸空的長條，"
              "很適合畫「最低到最高」這類範圍。記得把 includeZero 關掉，"
              "否則 Y 軸會被硬拉到 0，範圍全擠在上半部。") {
    const double lows[12] = {13, 13, 15, 18, 22, 24, 26, 26, 24, 21, 18, 15};
    const double highs[12] = {19, 20, 22, 26, 29, 32, 34, 33, 31, 28, 24, 21};
    const Colormap cmap(Colormap::Coolwarm);
    QVector<Bar> bars;
    QVector<double> pos;
    QStringList names;
    for (int i = 0; i < 12; ++i) {
        Bar b;
        b.position = i + 1;
        b.valueBase = lows[i];
        b.value = highs[i];
        b.size = 0.6;
        b.fillColor = cmap.lookup((lows[i] + highs[i]) / 2, 14, 30);
        bars.append(b);
        pos.append(i + 1);
        names.append(RT("%1月").arg(i + 1));
    }
    auto* bp = plot.addBars(bars);
    bp->includeZero = false;
    setCategoryTicks(plot.bottomAxis(), pos, names);
    plot.leftAxis()->label.text = RT("氣溫 (°C)");
    plot.title.text = RT("每月最低～最高氣溫");
}

QTPLOT_RECIPE("圖種", "長條與統計", "橫向浮動長條",
              "horizontal = true 時長條朝右，valueBase／value 變成左右兩端。"
              "類別名稱長的時候橫向比較好讀，文字不必轉角度。") {
    const QStringList products{RT("入門款筆電"), RT("輕薄筆電"),
                               RT("電競筆電"), RT("工作站"),
                               RT("平板")};
    const double lo[] = {15, 28, 35, 60, 8};
    const double hi[] = {25, 52, 80, 150, 38};
    QVector<Bar> bars;
    QVector<double> pos;
    for (int i = 0; i < products.size(); ++i) {
        Bar b;
        b.position = i;
        b.valueBase = lo[i];
        b.value = hi[i];
        b.size = 0.55;
        b.fillColor = Palette(Palette::Category10).color(i);
        bars.append(b);
        pos.append(i);
    }
    auto* bp = plot.addBars(bars);
    bp->horizontal = true;
    bp->includeZero = false;
    setCategoryTicks(plot.leftAxis(), pos, products);
    plot.bottomAxis()->label.text = RT("售價區間（千元）");
}

// ═══════════════════════════ 熱圖 ═══════════════════════════

QTPLOT_RECIPE("場", "熱圖", "儲存格數值標籤",
              "格子不多的時候把數值直接印上去，讀者不用回頭對色階。"
              "文字顏色要跟著底色換：深色格用白字、淺色格用黑字，否則一半的數字看不見。"
              "第 0 列畫在上面，所以第 r 列的中心在 y = rows − r − 0.5。") {
    const int rows = 6, cols = 8;
    QVector<double> values;
    const QVector<double> raw = gen::randomUniform(rows * cols, 0, 100);
    for (double v : raw) values.append(std::round(v));

    auto* hm = plot.addHeatmap(values, rows, cols);
    hm->colormap = Colormap(Colormap::Viridis);
    const CoordinateRange range = hm->dataRange();

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const double v = hm->value(r, c);
            auto* t = plot.addText(QString::number(v, 'f', 0), c + 0.5, rows - r - 0.5);
            t->alignment = Qt::AlignHCenter | Qt::AlignVCenter;
            const double frac = (v - range.min) / range.span();
            t->label.font.color = frac > 0.6 ? QColor(0, 0, 0) : QColor(255, 255, 255);
            t->label.font.bold = true;
        }
    }
    plot.addColorbar(*hm);
    plot.grid.isVisible = false;
    plot.setAxisLimits(0, cols, 0, rows);
}

QTPLOT_RECIPE("場", "熱圖", "手動指定色階範圍",
              "colorRange 沒設時色階用資料的 min／max，所以任何資料都會用滿整條色階 —— "
              "兩張熱圖的顏色因此沒有可比性。左右是同一份資料（0.2–0.6）："
              "左邊自動範圍，看起來對比很強；右邊固定在 0–1，才看得出其實差異不大。") {
    const int rows = 12, cols = 12;
    QVector<double> values = gen::gaussian2D(rows, cols, 0.35);
    double vmin = 1e300, vmax = -1e300;
    for (double v : values) {
        vmin = std::min(vmin, v);
        vmax = std::max(vmax, v);
    }
    for (double& v : values) v = 0.2 + 0.4 * (v - vmin) / (vmax - vmin);

    auto* left = plot.addHeatmap(values, rows, cols);
    left->extent = CoordinateRect(0, 12, 0, 12);

    auto* right = plot.addHeatmap(values, rows, cols);
    right->extent = CoordinateRect(14, 26, 0, 12);
    right->colorRange = CoordinateRange(0, 1);

    auto* lt = plot.addText(RT("自動（0.2–0.6）"), 6, 12.4);
    lt->alignment = Qt::AlignHCenter | Qt::AlignBottom;
    auto* rt = plot.addText(RT("固定 colorRange = 0–1"), 20, 12.4);
    rt->alignment = Qt::AlignHCenter | Qt::AlignBottom;

    plot.addColorbar(*right);
    plot.grid.isVisible = false;
    plot.equalAxisScale = true;
    plot.setAxisLimits(-0.5, 26.5, -0.5, 14);
}

// ═══════════════════════════ 訊號與函數 ═══════════════════════════

QTPLOT_RECIPE("圖種", "散點與線", "訊號的位移與縮放",
              "Signal 的 X 是 index × period + xOffset，Y 再加上 yOffset —— "
              "不必複製陣列就能換算成秒、或把好幾個通道錯開排成腦波圖那樣。"
              "這裡的取樣率是 250 Hz，所以 period = 1/250。") {
    const double fs = 250;
    const QStringList names{QStringLiteral("Fp1"), QStringLiteral("C3"), QStringLiteral("O1"),
                            QStringLiteral("T4")};
    QVector<double> pos;
    for (int ch = 0; ch < names.size(); ++ch) {
        QVector<double> ys = gen::addNoise(gen::sin(1000, 0.6, 8 + ch * 3, ch), 0.25);
        auto* s = plot.addSignal(ys, 1.0 / fs);
        s->xOffset = 0.5;            // 記錄從第 0.5 秒開始
        s->yOffset = -ch * 3.0;      // 每個通道往下錯開 3 個單位
        s->line.width = 1.0;
        pos.append(-ch * 3.0);
    }
    setCategoryTicks(plot.leftAxis(), pos, names);
    plot.bottomAxis()->label.text = RT("時間 (s)");
}

QTPLOT_RECIPE("圖種", "散點與線", "函數只畫一段",
              "FunctionPlot 預設跟著 X 軸可視範圍求值。設 xLimits 就只在那一段畫 —— "
              "分段函數就是幾個 FunctionPlot 各管一段。平移到範圍外，曲線不會跟著延伸。") {
    auto* a = plot.addFunction([](double x) { return -x; }, QColor(31, 119, 180));
    a->xLimits = CoordinateRange(-6, 0);
    a->legendText = RT("x < 0：−x");
    auto* b = plot.addFunction([](double x) { return 2 * std::sin(x * 1.5); },
                               QColor(214, 39, 40));
    b->xLimits = CoordinateRange(0, 6);
    b->legendText = RT("x ≥ 0：2·sin(1.5x)");
    for (FunctionPlot* f : {a, b}) f->line.width = 2.5;

    plot.legend.isVisible = true;
    plot.setAxisLimits(-10, 10, -4, 7);
}

// ═══════════════════════════ 參考線 ═══════════════════════════

QTPLOT_RECIPE("圖種", "參考線與區間", "參考線進圖例、不撐開範圍",
              "AxisLine 設了 legendText 就會進圖例。includeInAxisLimits = false 的線"
              "不參與自動範圍 —— 遠方的設計極限（y = 80）不會把視野拉過去，"
              "資料才不會被壓成一條扁線。縮小視野就看得到它。") {
    plot.addSignal(gen::randomWalk(300))->legendText = RT("量測值");

    auto* mean = plot.addHorizontalLine(0, QColor(80, 80, 80));
    mean->line.pattern = LinePattern::Dashed;
    mean->legendText = RT("基準");

    auto* warn = plot.addHorizontalLine(15, QColor(255, 140, 0));
    warn->legendText = RT("警戒（撐開範圍）");

    auto* limit = plot.addHorizontalLine(80, QColor(214, 39, 40));
    limit->line.width = 2;
    limit->includeInAxisLimits = false;
    limit->legendText = RT("設計極限（不撐開範圍）");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignBottom;
}

// ═══════════════════════════ 即時 ═══════════════════════════

QTPLOT_RECIPE_LIVE("即時", "累積記錄", "DataLogger：一次加一批",
                   "硬體通常是一包一包送資料過來，用 addRange() 整批加比逐點 add() 省事。"
                   "這裡模擬 1 kHz 的取樣，每次計時器觸發收到 20 筆；period = 0.001 讓 X 直接是秒，"
                   "Slide 模式固定顯示最近 2 秒。", 20) {
    if (frame == 0) {
        auto* log = plot.addDataLogger(QColor(44, 160, 44));
        log->period = 0.001;
        log->view = LiveView::Slide;
        log->viewWidth = 2.0;
        log->legendText = RT("1 kHz，每批 20 筆");
        plot.legend.isVisible = true;
        plot.bottomAxis()->label.text = RT("時間 (s)");
        return;
    }
    if (auto* log = firstOf<DataLogger>(plot)) {
        QVector<double> batch;
        for (int i = 0; i < 20; ++i) {
            const double t = (log->count() + i) * 0.001;
            batch.append(std::sin(2 * kPi * 3 * t) + 0.3 * std::sin(2 * kPi * 17 * t) +
                         0.1 * std::sin(i * 7.3 + frame));
        }
        log->addRange(batch);
    }
}

// ═══════════════════════════ 頻譜 ═══════════════════════════

QTPLOT_RECIPE("場", "頻譜", "窗函數的形狀",
              "windowValues() 取出窗函數的係數。越往兩端收得越快的窗，旁波壓得越低，"
              "但主波也越寬。圖例裡的數字是相干增益（係數平均）—— "
              "從頻譜讀振幅時要除掉它，否則 Hann 窗量出來的振幅會只剩一半。") {
    const int n = 101;
    const QVector<double> xs = gen::linspace(0, 1, n);
    const QStringList names = windowFunctionNames();
    const WindowFunction all[] = {WindowFunction::Rectangular, WindowFunction::Hann,
                                  WindowFunction::Hamming,     WindowFunction::Blackman,
                                  WindowFunction::BlackmanHarris, WindowFunction::FlatTop};
    for (int i = 0; i < 6; ++i) {
        auto* s = plot.addScatter(xs, windowValues(all[i], n));
        s->marker = MarkerStyle::none();
        s->line.width = 2;
        s->legendText = RT("%1（%2）")
                            .arg(names.value(i))
                            .arg(windowCoherentGain(all[i], n), 0, 'f', 3);
    }
    plot.legend.isVisible = true;
    plot.bottomAxis()->label.text = RT("窗內位置（0–1）");
    plot.leftAxis()->label.text = RT("係數");
}

// ═══════════════════════════ 子圖 ═══════════════════════════

QTPLOT_RECIPE_MULTI("版面", "子圖", "上下堆疊共用 X 軸",
                    "shareXAll() 讓每一格的 X 範圍都一樣。上下堆疊的時間序列一定要這樣：\n"
                    "中間那格的氣壓計只記錄了 100–400 分鐘，各自縮放的話它的 X 軸會被拉滿，"
                    "同一個時間點在三格裡就不在同一條垂直線上。") {
    mp.resize(3, 1);
    mp.title.text = RT("氣象站記錄");
    mp.title.font.size = 13;
    mp.title.font.bold = true;

    Plot* temp = mp.at(0, 0);
    auto* t = temp->addSignal(gen::addNoise(gen::sin(500, 4, 2, 0, 22), 0.4));
    t->line.color = QColor(214, 39, 40);
    temp->leftAxis()->label.text = RT("溫度 (°C)");

    Plot* press = mp.at(1, 0);
    QVector<double> px, py;
    for (int i = 100; i <= 400; ++i) {
        px.append(i);
        py.append(1013 + 6 * std::sin(i / 60.0));
    }
    auto* p = press->addScatter(px, py, QColor(31, 119, 180));
    p->marker = MarkerStyle::none();
    press->leftAxis()->label.text = RT("氣壓 (hPa)");

    Plot* hum = mp.at(2, 0);
    auto* h = hum->addSignal(gen::addNoise(gen::cos(500, 15, 2, 0, 60), 1.5));
    h->line.color = QColor(44, 160, 44);
    hum->leftAxis()->label.text = RT("濕度 (%)");
    hum->bottomAxis()->label.text = RT("時間 (分鐘)");

    mp.shareXAll();
}

// ═══════════════════════════ 圖例與版面 ═══════════════════════════

QTPLOT_RECIPE("樣式", "格線與圖例", "手動指定圖例項目",
              "legend.manualItems 不是空的時候，圖例就只列這些，不再從圖元自動收集。"
              "適合「一個圖元裡有好幾種意思」的情況：這裡每個點依是否合格上色，"
              "但點本身是兩組 Markers，圖例要說的是顏色代表什麼，加上規格線的意思。") {
    const QVector<double> xs = gen::consecutive(60);
    const QVector<double> ys = gen::addNoise(QVector<double>(60, 10.0), 1.2);
    QVector<double> okX, okY, ngX, ngY;
    for (int i = 0; i < xs.size(); ++i) {
        if (ys[i] <= 12) {
            okX.append(xs[i]);
            okY.append(ys[i]);
        } else {
            ngX.append(xs[i]);
            ngY.append(ys[i]);
        }
    }
    const QColor okColor(44, 160, 44), ngColor(214, 39, 40);
    plot.addMarkers(okX, okY, okColor);
    plot.addMarkers(ngX, ngY, ngColor)->marker.shape = MarkerShape::FilledTriangleUp;
    auto* usl = plot.addHorizontalLine(12, QColor(120, 120, 120));
    usl->line.pattern = LinePattern::Dashed;

    LegendItem ok;
    ok.text = RT("合格");
    ok.line = LineStyle::none();
    ok.marker = MarkerStyle(MarkerShape::FilledCircle, 8, okColor);
    LegendItem ng;
    ng.text = RT("超出規格");
    ng.line = LineStyle::none();
    ng.marker = MarkerStyle(MarkerShape::FilledTriangleUp, 9, ngColor);
    LegendItem spec;
    spec.text = RT("規格上限 USL = 12");
    spec.line = LineStyle(QColor(120, 120, 120), 1.5, LinePattern::Dashed);
    plot.legend.manualItems = {ok, ng, spec};
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
}

QTPLOT_RECIPE("樣式", "版面與框線", "無框線的圖",
              "把四根軸都設 isVisible = false、關掉格線、邊距歸零，繪圖區就佔滿整張圖。"
              "要把圖嵌進儀表板的小格子、或當成縮圖時用。資料還在原本的座標上，平移縮放照常。") {
    const QVector<double> values = gen::sin2D(60, 90, 3);
    auto* hm = plot.addHeatmap(values, 60, 90);
    hm->colormap = Colormap(Colormap::Turbo);
    hm->smooth = true;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.grid.isVisible = false;
    plot.paddingLeft = plot.paddingRight = plot.paddingTop = plot.paddingBottom = 0;
    plot.setAxisLimits(0, 90, 0, 60);
}

QTPLOT_RECIPE("樣式", "版面與框線", "固定邊距",
              "paddingLeft／Right／Top／Bottom 是繪圖區四周的最小留白（像素）。"
              "好幾張圖要上下排在一起時，給同樣的邊距，繪圖區的左右邊才會對齊；"
              "軸標籤需要更多空間時版面仍會自動加大，不會把字切掉。") {
    plot.addSignal(gen::randomWalk(500));
    plot.paddingLeft = 90;
    plot.paddingRight = 60;
    plot.paddingTop = 40;
    plot.paddingBottom = 30;
    plot.style.figureBackground = QColor(225, 232, 242);
    plot.style.dataBackground = QColor(255, 255, 255);
    plot.title.text = RT("左 90、右 60、上 40、下 30");
}

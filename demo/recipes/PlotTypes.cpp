// PlotTypes.cpp — 各種圖元的範例。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

// ───────────────────── 散點與線 ─────────────────────

QTPLOT_RECIPE("圖種", "散點與線", "只有標記", "Markers 不連線，純粹畫點。") {
    plot.addMarkers(gen::randomUniform(60, 0, 10), gen::randomUniform(60, 0, 10));
}

QTPLOT_RECIPE("圖種", "散點與線", "階梯線",
              "step 設成 Horizontal 或 Vertical 可以畫階梯圖，"
              "適合表示取樣保持或離散狀態的訊號。"
              "三條線走過同一組點，差別只在兩點之間怎麼連。") {
    const QVector<double> xs = gen::consecutive(20);
    const QVector<double> ys = gen::randomWalk(20);

    // 三條疊在一起會看不出差別，所以上下錯開
    auto offsetBy = [&](double d) {
        QVector<double> out = ys;
        for (double& v : out) v += d;
        return out;
    };

    auto* straight = plot.addScatter(xs, offsetBy(6));
    straight->legendText = QStringLiteral("Straight（直接連）");

    auto* h = plot.addScatter(xs, offsetBy(3));
    h->step = Scatter::StepStyle::Horizontal;
    h->legendText = QStringLiteral("StepHorizontal（先水平再垂直）");

    auto* v = plot.addScatter(xs, ys);
    v->step = Scatter::StepStyle::Vertical;
    v->legendText = QStringLiteral("StepVertical（先垂直再水平）");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("圖種", "散點與線", "填到基準線",
              "Scatter 設 fillToY 就會把曲線與該水平線之間填起來。") {
    auto* s = plot.addScatter(gen::consecutive(100), gen::sin(100, 1.0, 2.0));
    s->marker = MarkerStyle::none();
    s->fillToY = 0;
    s->fill = FillStyle(QColor(31, 119, 180, 70));
    plot.addHorizontalLine(0, QColor(80, 80, 80));
}

QTPLOT_RECIPE("圖種", "散點與線", "斷開的線（NaN）",
              "資料裡的 NaN 會把線斷開，而不是連到螢幕外。缺值就放 NaN 即可。") {
    QVector<double> ys = gen::sin(100, 1.0, 2.0);
    for (int i = 30; i < 45; ++i) ys[i] = std::numeric_limits<double>::quiet_NaN();
    auto* s = plot.addScatter(gen::consecutive(100), ys);
    s->marker = MarkerStyle::none();
    plot.addAnnotation(QStringLiteral("index 30–44 是 NaN"), Qt::AlignRight | Qt::AlignTop);
}

QTPLOT_RECIPE("圖種", "散點與線", "已排序的長序列（SignalXY）",
              "X 遞增但不等間距時用 SignalXY：會用二分搜尋只處理可視範圍，"
              "並在點太密時抽樣。") {
    plot.addSignalXY(gen::randomSample(200'000, 0, 1000), gen::randomWalk(200'000));
    plot.title.text = QStringLiteral("200,000 點，X 不等間距");
}

// ───────────────────── 長條與統計 ─────────────────────

QTPLOT_RECIPE("圖種", "長條與統計", "長條加誤差線與標籤",
              "逐根長條設定 value、error 與 label，可以做出帶誤差線與數值標籤的圖。") {
    QVector<Bar> bars;
    const QVector<double> values{23, 41, 18, 35};
    const QVector<double> errors{3, 5, 2, 4};
    const QStringList names{QStringLiteral("甲"), QStringLiteral("乙"),
                            QStringLiteral("丙"), QStringLiteral("丁")};
    for (int i = 0; i < values.size(); ++i) {
        Bar b;
        b.position = i;
        b.value = values[i];
        b.error = errors[i];
        b.label = QString::number(values[i], 'f', 0);
        b.fillColor = plot.style.palette.color(i);
        bars.append(b);
    }
    plot.addBars(bars);

    // 類別軸：把刻度換成名稱
    auto manual = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < names.size(); ++i) manual->ticks.append({double(i), names[i], true});
    plot.bottomAxis()->tickGenerator = manual;
    plot.leftAxis()->label.text = QStringLiteral("數量");
}

QTPLOT_RECIPE("圖種", "長條與統計", "水平長條",
              "horizontal 設成 true 長條就朝右長，類別名稱比較長時好讀。") {
    auto* b = plot.addBars({14, 28, 9, 33});
    b->horizontal = true;
}

QTPLOT_RECIPE("圖種", "長條與統計", "堆疊長條",
              "valueBase 設成前一層的頂端就是堆疊圖。") {
    const QVector<double> lower{10, 14, 8, 12};
    const QVector<double> upper{6, 4, 9, 7};
    QVector<Bar> a, b;
    for (int i = 0; i < lower.size(); ++i) {
        Bar x;
        x.position = i;
        x.value = lower[i];
        x.fillColor = plot.style.palette.color(0);
        a.append(x);

        Bar y;
        y.position = i;
        y.valueBase = lower[i];
        y.value = lower[i] + upper[i];
        y.fillColor = plot.style.palette.color(1);
        b.append(y);
    }
    plot.addBars(a)->legendText = QStringLiteral("第一層");
    plot.addBars(b)->legendText = QStringLiteral("第二層");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("圖種", "長條與統計", "直方圖",
              "Histogram::fromValues 會分箱（箱數不給就用 Sturges 公式），"
              "再轉成長條。") {
    plot.addHistogram(gen::randomNormal(2000, 50, 12), 25);
    plot.title.text = QStringLiteral("常態分布 2000 筆，25 個箱");
    plot.leftAxis()->label.text = QStringLiteral("次數");
}

QTPLOT_RECIPE("圖種", "長條與統計", "誤差線",
              "ErrorBar 可以單獨用，也可以疊在散點圖上。X 與 Y 方向可各自設定。") {
    const QVector<double> xs = gen::consecutive(12);
    const QVector<double> ys = gen::sin(12, 5, 1.0, 0, 10);
    auto* e = plot.addErrorBar(xs, ys, gen::randomUniform(12, 0.5, 2.0));
    e->xErrorsNegative = gen::randomUniform(12, 0.1, 0.3);
    e->xErrorsPositive = e->xErrorsNegative;
    plot.addMarkers(xs, ys, QColor(31, 119, 180));
}

// ───────────────────── 熱圖 ─────────────────────

QTPLOT_RECIPE("圖種", "熱圖", "熱圖",
              "2D 陣列以 row-major 傳入。內部會先轉成 QImage 再由 Qt 縮放，"
              "比逐格畫矩形快一個數量級。") {
    const int n = 40;
    plot.addHeatmap(gen::gaussian2D(n, n, 0.25), n, n);
    plot.title.text = QStringLiteral("2D 高斯");
}

QTPLOT_RECIPE("圖種", "熱圖", "色階與平滑",
              "colormap 換成別的色階，smooth 打開會做雙線性插值。") {
    const int n = 20;
    auto* h = plot.addHeatmap(gen::sin2D(n, n, 1.5), n, n);
    h->colormap = Colormap(Colormap::Turbo);
    h->smooth = true;
    h->extent = CoordinateRect(0, 10, 0, 10);
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("圖種", "熱圖", "缺值（NaN）",
              "NaN 的格子預設畫成透明，所以底下的背景會透出來，"
              "不會被誤認成色階最低的值。") {
    const int n = 15;
    QVector<double> v = gen::gaussian2D(n, n, 0.3);
    for (int i = 0; i < v.size(); i += 7) v[i] = std::numeric_limits<double>::quiet_NaN();
    auto* h = plot.addHeatmap(v, n, n);
    h->colormap = Colormap(Colormap::Blues);
    plot.style.dataBackground = QColor(240, 240, 240);
}

QTPLOT_RECIPE("圖種", "熱圖", "上下左右翻轉",
              "陣列的第 0 列預設畫在「上面」、第 0 行畫在「左邊」—— "
              "這和影像一致，但和「Y 往上長」的數學慣例相反。"
              "資料若是由下往上掃出來的就要 flipVertically。"
              "兩個旗標互相獨立，可以同時開。") {
    const int n = 24;
    // 左下角特別亮的不對稱圖樣，翻轉之後亮點會跑到別的角落，一眼看得出差別
    QVector<double> v(n * n);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            v[r * n + c] = std::exp(-(r * r + c * c) / 90.0);

    struct Panel {
        bool flipH;
        bool flipV;
        const char* name;
        CoordinateRect at;
    };
    const QVector<Panel> panels{
        {false, false, "原樣",        CoordinateRect(0, 10, 11, 21)},
        {true,  false, "flipX",       CoordinateRect(11, 21, 11, 21)},
        {false, true,  "flipY",       CoordinateRect(0, 10, 0, 10)},
        {true,  true,  "flipX+flipY", CoordinateRect(11, 21, 0, 10)},
    };
    for (const Panel& p : panels) {
        auto* h = plot.addHeatmap(v, n, n);
        h->extent = p.at;
        h->colormap = Colormap(Colormap::Viridis);
        h->flipHorizontally = p.flipH;
        h->flipVertically = p.flipV;

        auto* t = plot.addText(QString::fromUtf8(p.name), p.at.left + 0.3, p.at.top - 0.3);
        t->alignment = Qt::AlignLeft | Qt::AlignTop;
        t->label.font.color = Qt::white;
        t->label.font.bold = true;
    }
    plot.equalAxisScale = true;
}

// ───────────────────── 等角投影（2.5D） ─────────────────────

QTPLOT_RECIPE("圖種", "等角投影", "等角長條圖",
              "柱子站在一塊等角的地面上。三個面用不同亮度 —— "
              "全部同色的話，投影出來只是一堆重疊的平行四邊形，看不出是柱子。\n"
              "先講清楚代價：等角投影之下，頂面是斜的，讀者很難判斷柱頂對到哪個刻度；"
              "前排還會遮住後排。要比較數值請用一般的 addBars，"
              "這一族的用途是簡報版型。") {
    auto* b = plot.addIsoBars({25, 50, 75, 85});
    const QVector<QColor> colors{QColor(60, 90, 190), QColor(70, 190, 120),
                                 QColor(250, 200, 60), QColor(235, 90, 130)};
    const QStringList labels{QStringLiteral("25%"), QStringLiteral("50%"),
                             QStringLiteral("75%"), QStringLiteral("85%")};
    for (int i = 0; i < b->bars.size(); ++i) {
        b->bars[i].color = colors[i];
        b->bars[i].label = labels[i];
        b->bars[i].x = i * 1.6;
        b->bars[i].sizeX = 0.9;
        b->bars[i].sizeY = 0.9;
    }
    b->projection.heightScale = 0.06;
    b->labelFont.bold = true;
    b->showGround = false;

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("圖種", "等角投影", "等角格狀長條圖",
              "柱子排成格子時，繪製順序就不能照資料順序了：x + y 愈大代表愈靠近觀眾，"
              "要由小到大畫。順序反了的話後排會蓋住前排，立體感整個顛倒。") {
    gen::setSeed(9);
    const int cols = 5, rows = 5;
    QVector<IsoBar> bars;
    const Colormap cmap(Colormap::Turbo);
    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            // 兩個方向的正弦疊起來，做出高低起伏的地形
            const double h = 1.4 + std::sin(i * 0.9) * 0.8 + std::cos(j * 0.8) * 0.7 +
                             gen::randomUniform(1, 0, 0.3).first();
            bars.append({static_cast<double>(i), static_cast<double>(j), h, 0.78, 0.78,
                         cmap.lookup(std::clamp(h / 3.2, 0.0, 1.0)), QString()});
        }
    }
    auto* b = plot.addIsoBars({});
    b->bars = bars;
    b->projection.heightScale = 0.55;
    b->groundFill = FillStyle(QColor(235, 242, 250));
    b->edgeLine = LineStyle(QColor(255, 255, 255, 120), 1.0);

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("圖種", "等角投影", "等角金字塔",
              "IsoStack 一層一層疊上去，每層可以有自己的底／頂半徑。"
              "sides 決定底面是幾邊形：4 是方錐、48 以上看起來就是圓的 —— "
              "所以金字塔、圓柱、圓錐都是同一個類別。\n"
              "側面拆成 n 片各自上明暗（Lambert），而不是整片同色："
              "同色的話圓錐看起來會是一個平的三角形。") {
    const QStringList names{QStringLiteral("願景"), QStringLiteral("策略"),
                            QStringLiteral("戰術"), QStringLiteral("執行")};
    const QVector<QColor> colors{QColor(235, 90, 110), QColor(250, 190, 70),
                                 QColor(60, 190, 180), QColor(110, 200, 110)};

    QVector<IsoLayer> layers;
    const int n = names.size();
    for (int i = 0; i < n; ++i) {
        // 由下往上愈來愈細：第 i 層的底半徑是 (n-i)/n
        IsoLayer l;
        l.height = 1.0;
        l.bottomRadius = static_cast<double>(n - i) / n * 2.4;
        l.topRadius = static_cast<double>(n - i - 1) / n * 2.4;
        l.color = colors[n - 1 - i];
        l.label = names[n - 1 - i];
        layers.append(l);
    }

    auto* st = plot.addIsoStack(layers);
    st->sides = 4;
    st->rotationDegrees = 45;  // 方錐要轉 45 度才會正對讀者
    st->projection.heightScale = 1.1;
    st->labelFont.bold = true;

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("圖種", "等角投影", "等角圓柱與圓錐",
              "同一個 IsoStack，改 sides 與上下半徑就得到不同的形狀："
              "上下等寬是圓柱、頂面 0 是圓錐、上下不等是平截頭。"
              "四根並排當作百分比的示意。") {
    const QVector<double> values{35, 70, 55, 90};
    const QVector<QColor> colors{QColor(60, 190, 180), QColor(235, 90, 110),
                                 QColor(250, 200, 70), QColor(110, 200, 110)};
    for (int i = 0; i < values.size(); ++i) {
        IsoLayer l;
        l.height = values[i] / 28.0;
        l.bottomRadius = 0.72;
        // 前兩根做成圓錐、後兩根是圓柱，一眼看得出差別
        l.topRadius = (i < 2) ? 0.0 : 0.72;
        l.color = colors[i];
        l.label = QStringLiteral("%1%").arg(values[i]);

        auto* st = plot.addIsoStack({l});
        st->x = i * 2.2;
        st->y = -i * 2.2;   // 沿著等角的「往右」方向排，不然會前後重疊
        st->sides = 48;
        st->projection.heightScale = 0.8;
        st->labelFont.bold = true;
        st->labelGapPx = 6;
    }

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.equalAxisScale = true;
}

// ───────────────────── 商務圖表 ─────────────────────

namespace {

/// 把一串名字設成類別軸的刻度。
void setCategoryTicks(Plot& plot, const QStringList& names, Edge edge = Edge::Bottom) {
    QVector<Tick> ticks;
    for (int i = 0; i < names.size(); ++i) ticks.append({static_cast<double>(i), names[i]});
    auto gen = std::make_shared<ManualTickGenerator>();
    gen->ticks = ticks;
    (edge == Edge::Bottom ? plot.bottomAxis() : plot.leftAxis())->tickGenerator = gen;
}

}  // namespace

QTPLOT_RECIPE("圖種", "商務圖表", "群組長條",
              "同一個類別裡並排幾根長條，就是群組（clustered）長條圖。"
              "引擎不需要特別支援 —— 把每一組的位置各自偏移 size 的倍數即可。"
              "關鍵是偏移量要讓整組對稱地落在類別中心上，不然刻度會對不準長條。") {
    const QStringList names{QStringLiteral("Q1"), QStringLiteral("Q2"), QStringLiteral("Q3"),
                            QStringLiteral("Q4")};
    const QVector<QVector<double>> series{{18, 24, 20, 29}, {12, 19, 25, 22}, {9, 7, 14, 16}};
    const QStringList seriesNames{QStringLiteral("北區"), QStringLiteral("中區"),
                                  QStringLiteral("南區")};
    const Palette palette(Palette::Category10);

    const int g = series.size();
    const double barSize = 0.8 / g;
    for (int k = 0; k < g; ++k) {
        QVector<Bar> bars;
        for (int i = 0; i < names.size(); ++i) {
            Bar b;
            // 整組置中：第 k 根相對中心的偏移是 (k - (g-1)/2) * barSize
            b.position = i + (k - (g - 1) / 2.0) * barSize;
            b.value = series[k][i];
            b.size = barSize;
            b.fillColor = palette.color(k);
            bars.append(b);
        }
        auto* bp = plot.addBars(bars);
        bp->legendText = seriesNames[k];
    }
    setCategoryTicks(plot, names);
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("圖種", "商務圖表", "分歧長條",
              "同一根軸上同時有正負值，向左向右分開。"
              "用兩種顏色區分方向，再把 0 那條基準線畫出來 —— "
              "沒有基準線的話，讀者得自己找正負的分界在哪。") {
    const QStringList names{QStringLiteral("交通"), QStringLiteral("住房"),
                            QStringLiteral("食品"), QStringLiteral("能源"),
                            QStringLiteral("醫療"), QStringLiteral("教育")};
    const QVector<double> values{-4.2, 6.8, 2.1, -7.5, 3.4, -1.2};

    QVector<Bar> bars;
    for (int i = 0; i < values.size(); ++i) {
        Bar b;
        b.position = i;
        b.value = values[i];
        b.size = 0.7;
        b.fillColor = values[i] >= 0 ? QColor(56, 142, 60) : QColor(211, 47, 47);
        bars.append(b);
    }
    auto* bp = plot.addBars(bars);
    bp->horizontal = true;

    setCategoryTicks(plot, names, Edge::Left);
    plot.addVerticalLine(0)->line = LineStyle(QColor(0, 0, 0, 160), 1.5);
    plot.bottomAxis()->label.text = QStringLiteral("年增率 (%)");
}

QTPLOT_RECIPE("圖種", "商務圖表", "漏斗圖",
              "每段的寬度正比於它的數值，一層一層篩下來。\n"
              "這種圖容易被誤讀：讀者會把「面積」當成數量，但梯形的面積和它代表的值"
              "並不成正比（上下底不同）。要比較數量請用長條圖；"
              "漏斗圖的用途是表達「一層一層篩下來」這件事本身。") {
    const QStringList names{QStringLiteral("造訪"), QStringLiteral("瀏覽商品"),
                            QStringLiteral("加入購物車"), QStringLiteral("結帳"),
                            QStringLiteral("完成付款")};
    auto* f = plot.addFunnel({10000, 6200, 3100, 1400, 920}, names);
    f->extent = CoordinateRect(0, 10, 0, 10);
    f->labelFont.size = 11;
    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
}

QTPLOT_RECIPE("圖種", "商務圖表", "金字塔圖",
              "和漏斗圖是同一個類別：widthMode 改成 Linear（寬度線性遞減、與數值無關）、"
              "inverted 設 true（最寬的在下面），外廓就是三角形。") {
    const QStringList names{QStringLiteral("自我實現"), QStringLiteral("尊重"),
                            QStringLiteral("社交"), QStringLiteral("安全"),
                            QStringLiteral("生理")};
    auto* f = plot.addFunnel({5, 4, 3, 2, 1}, names);
    f->widthMode = Funnel::WidthMode::Linear;
    f->inverted = true;
    f->showValues = false;
    f->extent = CoordinateRect(0, 10, 0, 10);
    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
}

QTPLOT_RECIPE("圖種", "商務圖表", "瀑布圖",
              "從起點一步步加減，看得出每一項對結果的貢獻。"
              "每根長條的底是前一根的頂，所以中間那些是「浮」在空中的 —— "
              "這正是瀑布圖和堆疊長條圖的差別。"
              "isTotal 的那幾根從 0 畫起，代表小計或總計；"
              "它只改「從哪裡畫」，不改算術，所以期初餘額寫成 delta = 120 + isTotal。") {
    QVector<WaterfallStep> steps{
        {120, QStringLiteral("期初"), true, QColor()},
        {45, QStringLiteral("新客戶"), false, QColor()},
        {18, QStringLiteral("加購"), false, QColor()},
        {-22, QStringLiteral("流失"), false, QColor()},
        {-13, QStringLiteral("降級"), false, QColor()},
        {0, QStringLiteral("期末"), true, QColor()},
    };
    auto* w = plot.addWaterfall(steps);
    w->valueFont.bold = true;

    QStringList names;
    for (const WaterfallStep& s : steps) names.append(s.label);
    setCategoryTicks(plot, names);
    plot.leftAxis()->label.text = QStringLiteral("月費收入（萬）");
    // 上限要留給最高那根的數值標籤，不然 autoScale 貼合資料之後標籤會被切掉
    plot.setAxisLimits(-0.6, 5.6, 0, 200);
}

QTPLOT_RECIPE("圖種", "商務圖表", "靶心圖（Bullet）",
              "用很小的版面同時表達「實際值、目標、好壞分級」。"
              "常用來取代儀表板上的量表 —— 量表佔一大塊地方卻只放得下一個數字，"
              "靶心圖一列就講完同樣的事，好幾列疊起來還可以直接互相比較。") {
    QVector<BulletRow> rows{
        {QStringLiteral("營收"), 275, 250, {150, 225, 300}, QColor(40, 40, 40)},
        {QStringLiteral("毛利"), 32, 38, {20, 28, 45}, QColor(40, 40, 40)},
        {QStringLiteral("訂單數"), 1850, 2000, {1000, 1600, 2200}, QColor(40, 40, 40)},
        {QStringLiteral("滿意度"), 88, 85, {60, 75, 95}, QColor(40, 40, 40)},
    };
    // 四列的量級差很多，各自除以自己的滿分換算成百分比才比得起來
    for (BulletRow& r : rows) {
        const double full = r.ranges.last();
        r.value = r.value / full * 100;
        r.target = r.target / full * 100;
        for (double& b : r.ranges) b = b / full * 100;
    }
    plot.addBullet(rows);

    QStringList names;
    for (const BulletRow& r : rows) names.append(r.label);
    setCategoryTicks(plot, names, Edge::Left);
    plot.bottomAxis()->label.text = QStringLiteral("達成率（%，以各自滿分正規化）");
    plot.setAxisLimits(0, 105, -0.6, rows.size() - 0.4);
}

QTPLOT_RECIPE("圖種", "商務圖表", "氣泡圖",
              "散點圖再加上第三個維度（泡泡大小）。\n"
              "預設讓「面積」正比於 size，不是直徑 —— 直徑正比的話，size 差兩倍的"
              "兩個泡泡面積會差四倍，讀者會把差距讀成平方。這是氣泡圖最常見的錯誤。"
              "大的泡泡先畫，小的才不會被蓋住。") {
    const QVector<double> gdp{1.9, 3.9, 2.6, 2.1, 1.4, 17.7, 3.4, 0.5};
    const QVector<double> life{82.9, 81.2, 83.0, 80.9, 83.5, 78.2, 84.6, 83.2};
    const QVector<double> pop{67, 84, 60, 48, 38, 334, 125, 5.5};
    const QStringList names{QStringLiteral("英"), QStringLiteral("德"), QStringLiteral("義"),
                            QStringLiteral("西"), QStringLiteral("波"), QStringLiteral("美"),
                            QStringLiteral("日"), QStringLiteral("芬")};

    auto* b = plot.addBubbles(gdp, life, pop);
    for (int i = 0; i < names.size(); ++i) b->bubbles[i].label = names[i];
    b->labelFont.bold = true;
    b->maxDiameterPx = 90;
    b->minDiameterPx = 16;

    plot.bottomAxis()->label.text = QStringLiteral("GDP（兆美元）");
    plot.leftAxis()->label.text = QStringLiteral("平均壽命（歲）");
    plot.title.text = QStringLiteral("泡泡大小 = 人口");
}

QTPLOT_RECIPE("圖種", "商務圖表", "面積正比 vs 直徑正比",
              "同一組資料畫兩次。上排 areaProportional = true（正確），"
              "下排 false。最大的那個 size 是最小的 16 倍 —— "
              "直徑正比時它看起來會大到離譜，面積正比才讀得出真正的倍數。") {
    const QVector<double> sizes{1, 2, 4, 8, 16};
    QVector<double> xs;
    for (int i = 0; i < sizes.size(); ++i) xs.append(i);

    const QVector<double> upper(sizes.size(), 1.0);
    const QVector<double> lower(sizes.size(), 0.0);

    auto* a = plot.addBubbles(xs, upper, sizes);
    a->areaProportional = true;
    a->maxDiameterPx = 70;
    a->minDiameterPx = 70.0 / 4;  // sqrt(16) = 4

    auto* d = plot.addBubbles(xs, lower, sizes);
    d->areaProportional = false;
    d->maxDiameterPx = 70;
    d->minDiameterPx = 70.0 / 16;

    QVector<Tick> ticks{{1, QStringLiteral("面積正比（對）")},
                        {0, QStringLiteral("直徑正比（錯）")}};
    auto gen = std::make_shared<ManualTickGenerator>();
    gen->ticks = ticks;
    plot.leftAxis()->tickGenerator = gen;
    plot.setAxisLimits(-0.7, 4.7, -0.6, 1.6);
}

QTPLOT_RECIPE("圖種", "商務圖表", "氣泡大小圖例",
              "showSizeLegend 畫一組同心圓說明泡泡大小代表什麼。"
              "一般的圖例做不到這件事 —— 它只有一個固定大小的色塊。\n"
              "同心圓是「底部對齊」而不是圓心對齊：圓心對齊的話，"
              "小圓會浮在大圓中間，不容易比出直徑。") {
    gen::setSeed(17);
    const int n = 26;
    const QVector<double> xs = gen::randomUniform(n, 8, 78);
    const QVector<double> ys = gen::randomUniform(n, 1, 9.5);
    const QVector<double> sizes = gen::randomUniform(n, 5, 100);

    auto* b = plot.addBubbles(xs, ys, sizes);
    const QVector<QColor> palette{QColor(120, 170, 240), QColor(150, 235, 230),
                                  QColor(245, 140, 215)};
    for (int i = 0; i < b->bubbles.size(); ++i) b->bubbles[i].fillColor = palette[i % 3];
    b->maxDiameterPx = 76;
    b->minDiameterPx = 18;
    b->fillOpacity = 0.75;
    b->border = LineStyle::none();

    b->showSizeLegend = true;
    b->sizeLegendValues = {100, 50, 10};
    b->sizeLegendTitle = QStringLiteral("成功機率");
    b->sizeLegendCorner = Qt::AlignRight | Qt::AlignTop;
    b->sizeLegendFont.size = 9;

    plot.bottomAxis()->label.text = QStringLiteral("投入成本（萬）");
    plot.leftAxis()->label.text = QStringLiteral("預期營收（百萬）");
    plot.title.text = QStringLiteral("Q1 機會的風險評估");
    plot.setAxisLimits(0, 115, 0, 11);
}

QTPLOT_RECIPE("圖種", "商務圖表", "瀑布圖加累計值",
              "valueLabelInside 把增減量印在長條裡面，showCumulativeLabels 再在"
              "長條上方標出當下的累計值。\n"
              "兩個一起看才完整：增減量回答「這一步改變了多少」，"
              "累計值回答「到這一步為止是多少」—— 只有後者能和 Y 軸的刻度對起來。"
              "長條太矮塞不下時，增減量會自己退回長條外側。") {
    QVector<WaterfallStep> steps{
        {4000, QStringLiteral("期初"), true, QColor(230, 215, 175)},
        {1707, QStringLiteral("一月"), false, QColor()},
        {-1425, QStringLiteral("二月"), false, QColor()},
        {-1030, QStringLiteral("三月"), false, QColor()},
        {1812, QStringLiteral("四月"), false, QColor()},
        {-1067, QStringLiteral("五月"), false, QColor()},
        {-1481, QStringLiteral("六月"), false, QColor()},
        {1228, QStringLiteral("七月"), false, QColor()},
        {1176, QStringLiteral("八月"), false, QColor()},
        {1146, QStringLiteral("九月"), false, QColor()},
        {1205, QStringLiteral("十月"), false, QColor()},
        {-1388, QStringLiteral("十一月"), false, QColor()},
        {1492, QStringLiteral("十二月"), false, QColor()},
        {0, QStringLiteral("期末"), true, QColor(230, 215, 175)},
    };
    auto* w = plot.addWaterfall(steps);
    w->riseColor = QColor(135, 185, 150);
    w->fallColor = QColor(230, 125, 95);
    w->barSize = 0.7;
    w->valueLabelInside = true;
    w->valueFont.color = Qt::white;
    w->valueFont.bold = true;
    w->valueFont.size = 8;
    w->showCumulativeLabels = true;
    w->cumulativeFont.size = 8;
    w->borderLine = LineStyle(QColor(90, 90, 90), 1.0);

    QStringList names;
    for (const WaterfallStep& s : steps) names.append(s.label);
    setCategoryTicks(plot, names);
    plot.leftAxis()->label.text = QStringLiteral("金額");
    plot.setAxisLimits(-0.7, steps.size() - 0.3, 0, 8600);
}

QTPLOT_RECIPE("圖種", "商務圖表", "甘特圖",
              "橫條代表一段期間，由上往下排成一列一列。"
              "和一般的水平長條圖不同之處在於「列」是版面概念而不是資料："
              "同一列可以放好幾項不重疊的工作，列號由使用者指定。"
              "dependsOn 會畫一條折線箭頭連到前置工作。") {
    QVector<GanttTask> tasks{
        {QStringLiteral("需求訪談"), 0, 8, 0, 1.0, QColor(), {}},
        {QStringLiteral("系統設計"), 8, 20, 1, 0.8, QColor(), {0}},
        {QStringLiteral("介面設計"), 10, 22, 2, 0.6, QColor(), {0}},
        {QStringLiteral("後端開發"), 20, 44, 3, 0.35, QColor(), {1}},
        {QStringLiteral("前端開發"), 24, 46, 4, 0.25, QColor(), {2}},
        {QStringLiteral("整合測試"), 46, 58, 5, 0.0, QColor(), {3, 4}},
        {QStringLiteral("上線"), 58, 62, 6, 0.0, QColor(), {5}},
    };
    auto* g = plot.addGantt(tasks);
    g->labelFont.bold = true;

    QStringList names;
    for (const GanttTask& t : tasks) names.append(t.label);
    setCategoryTicks(plot, names, Edge::Left);
    plot.leftAxis()->inverted = true;  // 第一項排在最上面
    plot.bottomAxis()->label.text = QStringLiteral("第幾天");
    plot.setAxisLimits(-2, 72, -0.6, tasks.size() - 0.4);
}

QTPLOT_RECIPE("圖種", "商務圖表", "文氏圖",
              "兩個或三個互相重疊的圓。圓用半透明疊在一起，重疊處自然會變深，"
              "不必分別算交集的多邊形 —— 真的去算要做圓弧的布林運算，"
              "為了一張示意圖不值得。\n"
              "要注意這是「示意」而不是「比例」：圓的大小與重疊面積都是固定版型。"
              "要讓面積正比於集合大小，在三個集合時通常無解。") {
    auto* v = plot.addVenn({QStringLiteral("會寫程式"), QStringLiteral("懂統計"),
                            QStringLiteral("懂領域知識")});
    // 鍵是位元遮罩：1 = 只有第一個，3 = 第一∩第二，7 = 三個都有
    v->regionLabels = {{1, QStringLiteral("工程師")},
                       {2, QStringLiteral("統計學家")},
                       {4, QStringLiteral("領域專家")},
                       {3, QStringLiteral("機器學習")},
                       {5, QStringLiteral("資料工程")},
                       {6, QStringLiteral("傳統研究")},
                       {7, QStringLiteral("資料科學")}};
    v->countLabelFont.size = 9;
    v->countLabelFont.bold = true;
    plot.equalAxisScale = true;
    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
}

QTPLOT_RECIPE("圖種", "商務圖表", "兩集合文氏圖",
              "兩個集合時版型比較單純：左右擺，圓心距取半徑，"
              "交集大約佔每個圓的三分之一。") {
    auto* v = plot.addVenn({QStringLiteral("A"), QStringLiteral("B")});
    v->regionLabels = {{1, QStringLiteral("只有 A\n120")},
                       {2, QStringLiteral("只有 B\n85")},
                       {3, QStringLiteral("兩者皆有\n45")}};
    v->countLabelFont.bold = true;
    plot.equalAxisScale = true;
    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
}

QTPLOT_RECIPE("圖種", "商務圖表", "柏拉圖（Pareto）",
              "長條由大到小排，右軸疊一條累計百分比的折線，再畫一條 80% 參考線。"
              "用途是找出「少數幾項造成多數問題」的那幾項 —— "
              "累計線穿過 80% 的位置之前的項目就是重點。\n"
              "排序是重點而不是裝飾：沒排序的話累計線會上上下下，看不出轉折。") {
    QStringList names{QStringLiteral("刮傷"), QStringLiteral("尺寸不符"),
                      QStringLiteral("色差"),  QStringLiteral("毛邊"),
                      QStringLiteral("變形"),  QStringLiteral("污漬"),
                      QStringLiteral("其他")};
    QVector<double> counts{142, 96, 58, 31, 18, 11, 7};

    // 由大到小排。資料本來就排好了，但實務上不會，所以示範怎麼排。
    QVector<int> order;
    for (int i = 0; i < counts.size(); ++i) order.append(i);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return counts[a] > counts[b]; });
    QStringList sortedNames;
    QVector<double> sortedCounts;
    for (int i : order) {
        sortedNames.append(names[i]);
        sortedCounts.append(counts[i]);
    }

    auto* bars = plot.addBars(sortedCounts, QColor(70, 120, 185));
    for (Bar& b : bars->bars) b.size = 0.75;
    plot.leftAxis()->label.text = QStringLiteral("件數");

    double total = 0;
    for (double v : sortedCounts) total += v;
    QVector<double> xs, cumPercent;
    double running = 0;
    for (int i = 0; i < sortedCounts.size(); ++i) {
        running += sortedCounts[i];
        xs.append(i);
        cumPercent.append(running / total * 100);
    }

    auto* right = plot.addAxis(Edge::Right);
    right->showTickLabels = true;
    right->majorTickLength = 5;
    right->label.text = QStringLiteral("累計百分比");
    right->setRange(0, 105);

    auto* line = plot.addScatter(xs, cumPercent, QColor(214, 39, 40));
    line->yAxis = right;
    line->line.width = 2;
    line->marker = MarkerStyle(MarkerShape::FilledCircle, 6, QColor(214, 39, 40));

    auto* ref = plot.addHorizontalLine(80, QColor(120, 120, 120));
    ref->yAxis = right;
    ref->line.pattern = LinePattern::Dashed;
    ref->text.text = QStringLiteral("80%");

    setCategoryTicks(plot, sortedNames);
    plot.leftAxis()->setRange(0, total * 0.52);
}

QTPLOT_RECIPE("圖種", "商務圖表", "微型走勢圖（Sparkline）",
              "一行一條的極簡折線：沒有座標軸、沒有格線、沒有圖例，"
              "只保留形狀、端點的值，以及最高最低兩點。\n"
              "它不是用來讀數值的，是用來在一堆指標裡快速找出「哪一條不對勁」。"
              "要讀數值請用一般的折線圖。") {
    gen::setSeed(3);
    const QStringList names{QStringLiteral("營收"), QStringLiteral("訂單"),
                            QStringLiteral("退貨"), QStringLiteral("客訴"),
                            QStringLiteral("庫存")};
    const int n = 40;
    const double rowGap = 1.0;

    for (int k = 0; k < names.size(); ++k) {
        QVector<double> xs(n), ys(n);
        const QVector<double> walk = gen::randomWalk(n, 1.0, 0.0);
        double lo = walk[0], hi = walk[0];
        for (double v : walk) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        const double span = std::max(1e-9, hi - lo);
        // 每一條各自正規化到 0–0.7，再往下錯開一行 ——
        // 不正規化的話，數量級大的那條會把其他幾條壓成直線
        const double baseY = -k * rowGap;
        for (int i = 0; i < n; ++i) {
            xs[i] = i;
            ys[i] = baseY + (walk[i] - lo) / span * 0.7;
        }

        auto* s = plot.addScatter(xs, ys, QColor(60, 90, 160));
        s->marker = MarkerStyle::none();
        s->line.width = 1.4;

        // 最高、最低、最後一點：sparkline 慣例只標這三個
        auto* extremes = plot.addMarkers(
            {xs[walk.indexOf(hi)], xs[walk.indexOf(lo)]},
            {baseY + 0.7, baseY});
        extremes->marker = MarkerStyle(MarkerShape::FilledCircle, 5, QColor(200, 60, 60));

        auto* last = plot.addMarkers({xs.last()}, {ys.last()});
        last->marker = MarkerStyle(MarkerShape::FilledCircle, 6, QColor(30, 30, 30));

        auto* nameText = plot.addText(names[k], -1.5, baseY + 0.35);
        nameText->alignment = Qt::AlignRight | Qt::AlignVCenter;

        auto* valueText =
            plot.addText(QStringLiteral("%1").arg(walk.last(), 0, 'f', 1), n + 0.5, ys.last());
        valueText->alignment = Qt::AlignLeft | Qt::AlignVCenter;
        valueText->label.font.bold = true;
    }

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-9, n + 6, -names.size() * rowGap + 0.2, 1.0);
}

QTPLOT_RECIPE("圖種", "商務圖表", "馬賽克圖（Marimekko）",
              "欄寬正比於該欄的總量，欄內再依比例堆疊，所以每個方塊的「面積」"
              "正比於它的值。和一般的百分比堆疊長條圖差在欄寬 —— "
              "一般的堆疊圖每欄一樣寬，看不出哪一欄本身比較大。\n"
              "代價是很難比較不同欄之間的同一段：它們寬度不同、起點也不同。"
              "要逐段比較請用分組長條圖。") {
    const QStringList cats{QStringLiteral("牛肉"), QStringLiteral("海鮮"),
                           QStringLiteral("蔬果"), QStringLiteral("乾貨"),
                           QStringLiteral("其他肉"), QStringLiteral("乳製品"),
                           QStringLiteral("特殊品"), QStringLiteral("其他")};
    const QVector<QVector<double>> data{
        {141658, 82634, 44268, 26561}, {105358, 87798, 30102, 27594},
        {177072, 35414, 23610},        {86323, 47957, 28774, 28774},
        {70829, 41317, 35414},         {87798, 15494},
        {29512, 44268},                {56663, 49508, 44268, 26561}};

    QVector<MosaicColumn> cols;
    for (int i = 0; i < cats.size(); ++i) {
        MosaicColumn c;
        c.label = cats[i];
        c.values = data[i];
        for (int k = 0; k < data[i].size(); ++k)
            c.segmentLabels.append(QStringLiteral("$%1").arg(data[i][k], 0, 'f', 0));
        cols.append(c);
    }

    auto* m = plot.addMosaic(cols);
    m->extent = CoordinateRect(0, 10, 0, 10);
    m->labelFont.size = 8;
    m->columnLabelFont.size = 9;
    m->segmentColors = {QColor(45, 55, 72),  QColor(140, 190, 70), QColor(245, 150, 60),
                        QColor(130, 150, 190)};

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-0.4, 10.4, -1.1, 11.2);
    plot.title.text = QStringLiteral("各類別的採購金額（寬度 = 該類別佔比）");
}

QTPLOT_RECIPE("圖種", "商務圖表", "人口金字塔",
              "男女背對背的橫向長條：一邊畫成負值，另一邊正值。"
              "X 軸的刻度要用 labelFormatter 取絕對值 —— "
              "不然左半邊會印出一排負數，但人口沒有負的。") {
    const QStringList ages{
        QStringLiteral("0-9"),   QStringLiteral("10-19"), QStringLiteral("20-29"),
        QStringLiteral("30-39"), QStringLiteral("40-49"), QStringLiteral("50-59"),
        QStringLiteral("60-69"), QStringLiteral("70-79"), QStringLiteral("80-89"),
        QStringLiteral("90-99")};
    const QVector<double> male{20.1, 21.4, 22.7, 21.9, 20.8, 19.5, 15.2, 9.6, 4.3, 1.1};
    const QVector<double> female{19.3, 20.5, 22.2, 22.0, 21.3, 20.0, 16.8, 11.5, 6.2, 2.3};

    QVector<Bar> bars;
    for (int i = 0; i < ages.size(); ++i) {
        bars.append({static_cast<double>(i), -male[i], 0, 0, 0.72,
                     QColor(250, 170, 160), QColor(0, 0, 0, 0), QString()});
        bars.append({static_cast<double>(i), female[i], 0, 0, 0.72,
                     QColor(240, 70, 50), QColor(0, 0, 0, 0), QString()});
    }
    auto* bp = plot.addBars(bars);
    bp->horizontal = true;

    // 兩側的數值標籤
    for (int i = 0; i < ages.size(); ++i) {
        auto* l = plot.addText(QStringLiteral("%1 M").arg(male[i], 0, 'f', 1), -male[i] - 0.6, i);
        l->alignment = Qt::AlignRight | Qt::AlignVCenter;
        l->label.font.size = 9;
        auto* r = plot.addText(QStringLiteral("%1 M").arg(female[i], 0, 'f', 1),
                               female[i] + 0.6, i);
        r->alignment = Qt::AlignLeft | Qt::AlignVCenter;
        r->label.font.size = 9;
    }

    QVector<Tick> ticks;
    for (int i = 0; i < ages.size(); ++i) ticks.append({static_cast<double>(i), ages[i]});
    auto cat = std::make_shared<ManualTickGenerator>();
    cat->ticks = ticks;
    plot.leftAxis()->tickGenerator = cat;

    auto xgen = std::make_shared<AutomaticTickGenerator>();
    xgen->labelFormatter = [](double v, double spacing) {
        return formatTickLabel(std::abs(v), spacing, std::abs(v));
    };
    plot.bottomAxis()->tickGenerator = xgen;
    plot.bottomAxis()->label.text = QStringLiteral("人口（百萬）");

    plot.addVerticalLine(0)->line = LineStyle(QColor(0, 0, 0, 120), 1.0);
    plot.title.text = QStringLiteral("高齡層女性明顯多於男性");
    plot.setAxisLimits(-30, 30, -0.7, ages.size() - 0.3);
}

QTPLOT_RECIPE("圖種", "階層與多變量", "六角分箱（Hexbin）",
              "散點上萬筆之後，點會互相疊到看不出密度 —— 最密的地方和次密的地方"
              "都是一片實心。分箱把「有多少點」變成讀得出來的量。\n"
              "為什麼用六角而不是方格：六角格的每個鄰居距離都相同，"
              "方格的對角鄰居比較遠，所以方格會在對角方向低估密度，"
              "畫出來會有隱約的十字紋。") {
    gen::setSeed(44);
    const int n = 30000;
    QVector<double> xs, ys;
    xs.reserve(n);
    ys.reserve(n);
    for (int i = 0; i < n; ++i) {
        // 兩團高斯，一團大一團小
        const bool second = (i % 5) == 0;
        const double cx = second ? 2.2 : -0.6;
        const double cy = second ? 1.6 : -0.3;
        const double sd = second ? 0.55 : 1.0;
        xs.append(cx + gen::randomNormal(1, 0, sd).first());
        ys.append(cy + gen::randomNormal(1, 0, sd).first());
    }

    auto* h = plot.addHexbin(xs, ys, 34);
    h->colormap = Colormap(Colormap::Viridis);
    plot.addColorbar(*h)->label.text = QStringLiteral("點數");

    plot.bottomAxis()->label.text = QStringLiteral("X");
    plot.leftAxis()->label.text = QStringLiteral("Y");
    plot.title.text = QStringLiteral("三萬筆散點的密度");
}

QTPLOT_RECIPE("圖種", "階層與多變量", "啞鈴圖",
              "每一列兩個點加一條連線，表示「從哪裡變到哪裡」。\n"
              "比前後兩根長條好讀：長條要比較兩個「長度」，啞鈴比較的是兩個「位置」"
              "加中間那段「距離」—— 而變化量正是那段距離，直接看得到。"
              "代價是看不出絕對量的大小感，那是長條面積的作用。") {
    const QStringList items{QStringLiteral("行動裝置"), QStringLiteral("桌機"),
                            QStringLiteral("平板"),     QStringLiteral("電視"),
                            QStringLiteral("穿戴"),     QStringLiteral("主機")};
    const QVector<double> before{38, 52, 21, 14, 6, 11};
    const QVector<double> after{61, 44, 25, 17, 19, 9};

    QVector<DumbbellRow> rows;
    for (int i = 0; i < items.size(); ++i)
        rows.append({items[i], before[i], after[i], QColor(), QColor()});

    auto* db = plot.addDumbbell(rows);
    db->fromMarker.fillColor = QColor(150, 190, 225);
    db->toMarker.fillColor = QColor(215, 85, 65);
    db->fromLegendText = QStringLiteral("2020");
    db->toLegendText = QStringLiteral("2025");
    db->showValues = true;
    db->valueFont.size = 9;

    setCategoryTicks(plot, items, Edge::Left);
    plot.bottomAxis()->label.text = QStringLiteral("使用率 (%)");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
    plot.setAxisLimits(0, 72, -0.6, items.size() - 0.4);
}

QTPLOT_RECIPE("圖種", "階層與多變量", "斜率圖",
              "兩個時間點之間各畫一條線，斜率就是變化。"
              "和啞鈴圖的分工：啞鈴圖一列一個項目、好讀絕對值；"
              "斜率圖把全部項目疊在兩根軸之間，名次的交叉一眼看得到。\n"
              "項目名稱直接標在線的兩端，不用圖例 —— "
              "線一多的話，圖例要來回對照顏色反而更慢。") {
    const QStringList items{QStringLiteral("日本"), QStringLiteral("德國"),
                            QStringLiteral("英國"), QStringLiteral("法國"),
                            QStringLiteral("韓國"), QStringLiteral("加拿大")};
    const QVector<double> y2015{78, 71, 66, 62, 54, 49};
    const QVector<double> y2025{59, 74, 58, 65, 72, 46};
    const Palette palette(Palette::Category10);

    for (int i = 0; i < items.size(); ++i) {
        const QColor c = palette.color(i);
        auto* line = plot.addScatter({0, 1}, {y2015[i], y2025[i]}, c);
        line->line.width = 2;
        line->marker = MarkerStyle(MarkerShape::FilledCircle, 8, c);

        auto* l = plot.addText(QStringLiteral("%1 %2").arg(items[i]).arg(y2015[i]), -0.04,
                               y2015[i]);
        l->alignment = Qt::AlignRight | Qt::AlignVCenter;
        l->label.font.color = c;
        auto* r = plot.addText(QStringLiteral("%1 %2").arg(y2025[i]).arg(items[i]), 1.04,
                               y2025[i]);
        r->alignment = Qt::AlignLeft | Qt::AlignVCenter;
        r->label.font.color = c;
    }

    QVector<Tick> ticks{{0, QStringLiteral("2015")}, {1, QStringLiteral("2025")}};
    auto gen2 = std::make_shared<ManualTickGenerator>();
    gen2->ticks = ticks;
    plot.bottomAxis()->tickGenerator = gen2;
    plot.leftAxis()->isVisible = false;
    plot.grid.isVisible = false;
    plot.setAxisLimits(-0.42, 1.42, 40, 84);
    plot.title.text = QStringLiteral("十年間的名次變化");
}

// ───────────────────── 階層與多變量 ─────────────────────

QTPLOT_RECIPE("圖種", "階層與多變量", "樹狀圖（Treemap）",
              "矩形的面積精確正比於數值，階層用巢狀表示。\n"
              "版面用 squarified 演算法：盡量讓每塊接近正方形。"
              "長條狀的矩形很難比較面積 —— 讀者會改用長邊去比，那就讀錯了。\n"
              "和旭日圖的分工：項目多、要比大小用樹狀圖；層級深、要看結構用旭日圖。") {
    QVector<TreemapNode> roots{
        {QStringLiteral("亞洲"), 0, QColor(60, 120, 200), {
            {QStringLiteral("中國"), 1760, QColor(), {}},
            {QStringLiteral("日本"), 420, QColor(), {}},
            {QStringLiteral("印度"), 350, QColor(), {}},
            {QStringLiteral("韓國"), 180, QColor(), {}},
            {QStringLiteral("其他"), 310, QColor(), {}}}},
        {QStringLiteral("歐洲"), 0, QColor(220, 110, 50), {
            {QStringLiteral("德國"), 430, QColor(), {}},
            {QStringLiteral("英國"), 310, QColor(), {}},
            {QStringLiteral("法國"), 290, QColor(), {}},
            {QStringLiteral("其他"), 480, QColor(), {}}}},
        {QStringLiteral("北美"), 0, QColor(70, 160, 90), {
            {QStringLiteral("美國"), 2100, QColor(), {}},
            {QStringLiteral("加拿大"), 220, QColor(), {}}}},
        {QStringLiteral("其他"), 0, QColor(150, 110, 190), {
            {QStringLiteral("巴西"), 190, QColor(), {}},
            {QStringLiteral("澳洲"), 160, QColor(), {}},
            {QStringLiteral("其餘"), 240, QColor(), {}}}},
    };

    auto* tm = plot.addTreemap(roots);
    tm->extent = CoordinateRect(0, 16, 0, 10);
    tm->labelFont.size = 9;
    tm->groupLabelFont.size = 11;

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-0.2, 16.2, -0.2, 10.2);
    plot.title.text = QStringLiteral("各區營收（面積 = 金額）");
}

QTPLOT_RECIPE("圖種", "階層與多變量", "平行座標圖",
              "每個維度一根垂直軸，一筆資料畫成一條折線，各軸各自正規化 —— "
              "所以單位完全不同的維度可以放在一起。\n"
              "限制要知道：只看得出「相鄰」兩軸之間的關係，不相鄰的要自己換軸順序；"
              "線一多就糊成一片，要靠半透明與分群著色才讀得出來。") {
    gen::setSeed(31);
    const QStringList dims{QStringLiteral("排氣量"), QStringLiteral("馬力"),
                           QStringLiteral("車重"),   QStringLiteral("加速"),
                           QStringLiteral("油耗")};
    const QStringList groups{QStringLiteral("4 缸"), QStringLiteral("6 缸"),
                             QStringLiteral("8 缸")};
    const QVector<QColor> groupColors{QColor(60, 150, 90), QColor(220, 150, 40),
                                      QColor(200, 60, 70)};

    QVector<QVector<double>> rows;
    QVector<QColor> colors;
    QStringList rowGroups;
    for (int g = 0; g < 3; ++g) {
        for (int k = 0; k < 14; ++k) {
            const double base = g;  // 缸數愈多，排氣量／馬力／車重愈大，油耗愈差
            rows.append({1.6 + base * 1.5 + gen::randomNormal(1, 0, 0.35).first(),
                         95 + base * 70 + gen::randomNormal(1, 0, 18).first(),
                         1100 + base * 380 + gen::randomNormal(1, 0, 110).first(),
                         11.5 - base * 2.4 + gen::randomNormal(1, 0, 0.8).first(),
                         18.5 - base * 4.2 + gen::randomNormal(1, 0, 1.1).first()});
            colors.append(groupColors[g]);
            rowGroups.append(groups[g]);
        }
    }

    auto* pc = plot.addParallelCoordinates(dims, rows);
    pc->rowColors = colors;
    pc->rowGroups = rowGroups;
    pc->extent = CoordinateRect(0, 10, 0, 10);
    pc->lineStyle = LineStyle(QColor(0, 0, 0, 130), 1.3);
    // 「加速」與「油耗」是愈小愈好，翻過來讓「上面 = 比較好」一致
    pc->axes[3].inverted = true;
    pc->axes[4].inverted = true;

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-1.6, 11.2, -0.6, 11.4);
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
}

// ───────────────────── 流量與關係 ─────────────────────

QTPLOT_RECIPE("圖種", "流量與關係", "河流圖（Streamgraph）",
              "堆疊面積的變形：基準線不固定在 0，而是讓各層的「斜率變動」最小"
              "（Byron–Wattenberg 的 wiggle）。\n"
              "為什麼要這樣：基準線固定在 0 的話，最下面那層的起伏會把上面所有層"
              "一起推上推下，讀者會把那個起伏誤讀成上層自己的變化。"
              "代價是沒有基準線可讀，所以河流圖只適合看「相對比重的消長」，"
              "不適合讀絕對值。") {
    gen::setSeed(52);
    const int n = 60;
    const QStringList genres{QStringLiteral("動作"), QStringLiteral("喜劇"),
                             QStringLiteral("劇情"), QStringLiteral("科幻"),
                             QStringLiteral("恐怖"), QStringLiteral("動畫")};
    const Colormap cmap(Colormap::Turbo);

    QVector<double> xs;
    for (int i = 0; i < n; ++i) xs.append(1965 + i);

    QVector<QVector<double>> series(genres.size());
    for (int g = 0; g < genres.size(); ++g) {
        // 每個類別一個「流行期」：高峰落在不同年份
        const double peak = 8 + g * 8.5;
        const double width = 11 + g * 2.0;
        for (int i = 0; i < n; ++i) {
            const double d = (i - peak) / width;
            series[g].append(std::max(0.0, 30 * std::exp(-d * d) +
                                               gen::randomUniform(1, 0, 3).first()));
        }
    }

    QVector<QColor> colors;
    for (int g = 0; g < genres.size(); ++g)
        colors.append(cmap.lookup(static_cast<double>(g) / (genres.size() - 1)));

    const QVector<FillY*> bands = plot.addStream(xs, series, StackOffset::Wiggle, colors);
    for (int g = 0; g < bands.size(); ++g) bands[g]->legendText = genres[g];

    plot.leftAxis()->isVisible = false;   // 河流圖沒有基準線，Y 軸的數字沒有意義
    plot.grid.isVisible = false;
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
    plot.bottomAxis()->tickGenerator = std::make_shared<AutomaticTickGenerator>();
    plot.title.text = QStringLiteral("各類型片的消長");
}

QTPLOT_RECIPE("圖種", "流量與關係", "弧線圖（Arc diagram）",
              "節點排成一條線，關係畫成上方的弧。"
              "好處是節點有固定順序（可以照名稱、照大小排），"
              "力導向則每次位置都不一樣、不好對照；代價是邊一多，弧會互相穿越。\n"
              "弧的高度用「兩端距離」的比例，跨得遠的弧才會高一點、不會全部疊在一起。") {
    const QStringList people{
        QStringLiteral("Ana"),  QStringLiteral("Ben"),  QStringLiteral("Cara"),
        QStringLiteral("Dan"),  QStringLiteral("Eve"),  QStringLiteral("Finn"),
        QStringLiteral("Gia"),  QStringLiteral("Hal"),  QStringLiteral("Ivy"),
        QStringLiteral("Jon")};
    const QVector<QPair<int, int>> links{{0, 1}, {0, 3}, {0, 7}, {1, 2}, {1, 5},
                                         {2, 4}, {2, 9}, {3, 4}, {3, 6}, {4, 5},
                                         {5, 8}, {6, 7}, {6, 9}, {7, 8}, {8, 9}};

    QVector<GraphNode> nodes;
    const Palette palette(Palette::Category10);
    for (int i = 0; i < people.size(); ++i) {
        // 節點大小 = 連幾條邊
        int degree = 0;
        for (const auto& l : links)
            if (l.first == i || l.second == i) ++degree;
        nodes.append({people[i], static_cast<double>(degree), palette.color(i), {0, 0}, false});
    }
    QVector<GraphEdge> edges;
    for (const auto& l : links) edges.append({l.first, l.second, 1.0, QColor()});

    auto* g = plot.addNetwork(nodes, edges);
    g->layout = NetworkGraph::Layout::Linear;
    g->edgeShape = NetworkGraph::EdgeShape::Arc;
    g->arcHeightFraction = 0.8;
    g->extent = CoordinateRect(0, 10, 0, 10);
    g->nodeMinDiameterPx = 14;
    g->nodeMaxDiameterPx = 30;
    g->showLabels = false;
    g->edgeLine = LineStyle(QColor(90, 120, 180, 150), 1.6);

    // 名字標在線的下方
    for (int i = 0; i < people.size(); ++i) {
        const double x = people.size() > 1 ? 10.0 * i / (people.size() - 1) : 5.0;
        auto* t = plot.addText(people[i], x, 4.6);
        t->alignment = Qt::AlignHCenter | Qt::AlignTop;
        t->label.font.color = palette.color(i);
        t->label.font.bold = true;
    }

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-0.8, 10.8, 3.4, 10.4);
}

QTPLOT_RECIPE("圖種", "流量與關係", "桑基圖",
              "節點排成由左到右的幾層，連線的粗細正比於流量。\n"
              "節點的高度取「流進來」與「流出去」兩者的較大值 —— "
              "中間的節點常常不平衡（有損耗、有新增），取小的那邊會讓連線擠不下、"
              "互相重疊。層號要自己指定，不由連線推導：推導在有環或跨層連線時"
              "會得到很奇怪的結果。") {
    const Palette palette(Palette::Category10);
    QVector<SankeyNode> nodes{
        {QStringLiteral("煤"),     0, QColor(90, 90, 90)},
        {QStringLiteral("天然氣"), 0, QColor(237, 139, 0)},
        {QStringLiteral("再生"),   0, QColor(60, 160, 90)},
        {QStringLiteral("核能"),   0, QColor(150, 80, 170)},
        {QStringLiteral("發電"),   1, QColor(70, 120, 180)},
        {QStringLiteral("直接使用"), 1, QColor(170, 150, 110)},
        {QStringLiteral("工業"),   2, QColor(200, 70, 70)},
        {QStringLiteral("住商"),   2, QColor(80, 160, 200)},
        {QStringLiteral("運輸"),   2, QColor(120, 170, 70)},
        {QStringLiteral("損耗"),   2, QColor(160, 160, 160)},
    };
    QVector<SankeyLink> links{
        {0, 4, 28, QColor()}, {0, 5, 6, QColor()},
        {1, 4, 18, QColor()}, {1, 5, 22, QColor()},
        {2, 4, 24, QColor()}, {3, 4, 10, QColor()},
        {4, 6, 26, QColor()}, {4, 7, 22, QColor()}, {4, 8, 8, QColor()}, {4, 9, 24, QColor()},
        {5, 6, 10, QColor()}, {5, 7, 8, QColor()},  {5, 8, 10, QColor()},
    };
    auto* sk = plot.addSankey(nodes, links);
    sk->extent = CoordinateRect(0, 10, 0, 10);
    sk->labelFont.bold = true;

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-1.6, 11.2, -0.3, 10.3);
}

QTPLOT_RECIPE("圖種", "流量與關係", "網路圖",
              "節點加連線，用力導向版面（Fruchterman–Reingold）把關係攤開。"
              "節點大小是面積正比、連線粗細是權重。\n"
              "版面用自己的亂數引擎而不是 DataGen 的全域引擎：圖的樣子必須只取決於"
              "資料與 seed，不能被「前面跑過什麼」影響，否則同一張圖每次畫出來都不一樣。\n"
              "限制也要知道：節點超過一兩百個之後會變成一團毛球，看不出任何結構。") {
    const QStringList brands{QStringLiteral("集團"),  QStringLiteral("Audi"),
                             QStringLiteral("保時捷"), QStringLiteral("Škoda"),
                             QStringLiteral("SEAT"),  QStringLiteral("蘭博基尼"),
                             QStringLiteral("Ducati"), QStringLiteral("Scania"),
                             QStringLiteral("MAN"),   QStringLiteral("CUPRA")};
    QVector<GraphNode> nodes;
    QVector<GraphEdge> edges;
    nodes.append({brands[0], 10, QColor(225, 45, 45), {0, 0}, false});
    for (int i = 1; i < brands.size(); ++i) {
        // 邊用的是「nodes 裡的索引」，不是 brands 的索引 —— 中間穿插了車型節點，
        // 兩者早就錯開了。直接拿迴圈變數當節點索引會把邊接到別人身上。
        const int brandIndex = static_cast<int>(nodes.size());
        nodes.append({brands[i], 4, QColor(50, 120, 220), {0, 0}, false});
        edges.append({0, brandIndex, 1.0, QColor()});
        // 每個品牌再掛幾個車型（不標名字，只表示規模）
        for (int k = 0; k < 3; ++k) {
            nodes.append({QString(), 1, QColor(90, 150, 230), {0, 0}, false});
            edges.append({brandIndex, static_cast<int>(nodes.size()) - 1, 0.5, QColor()});
        }
    }

    auto* g = plot.addNetwork(nodes, edges);
    g->extent = CoordinateRect(0, 10, 0, 10);
    g->seed = 7;
    g->iterations = 400;
    g->nodeMinDiameterPx = 9;
    g->nodeMaxDiameterPx = 40;
    g->labelFont.size = 8;
    g->labelFont.color = Qt::white;
    g->edgeLine = LineStyle(QColor(120, 170, 230, 160), 1.0);

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("圖種", "流量與關係", "沖積比較圖",
              "同一群人在幾個期間的排名變化。每一期是一層，同一個人在相鄰兩期之間"
              "連一條線 —— 線的粗細是該期的量，交叉就代表名次換了。\n"
              "這其實就是桑基圖：把「期間」當成層、「人」當成節點。"
              "差別只在每個人在每一期都有一個節點，而不是只出現一次。") {
    const QStringList people{QStringLiteral("Alan"), QStringLiteral("David"),
                             QStringLiteral("Boris"), QStringLiteral("Lewis")};
    const QVector<QColor> colors{QColor(150, 195, 230), QColor(165, 190, 165),
                                 QColor(240, 160, 130), QColor(225, 225, 150)};
    const QStringList months{QStringLiteral("三月"), QStringLiteral("四月"),
                             QStringLiteral("五月")};
    // [期間][人]
    const QVector<QVector<double>> sales{{90, 82, 72, 66}, {85, 62, 73, 50}, {40, 60, 76, 80}};

    QVector<SankeyNode> nodes;
    QVector<SankeyLink> links;
    // nodeIndex[期][人] = 這個人在這一期的節點索引
    QVector<QVector<int>> nodeIndex(months.size(), QVector<int>(people.size(), -1));

    for (int m = 0; m < months.size(); ++m) {
        // 每一期各自依當期的量由大到小排。這一步是整張圖的重點 ——
        // 不排的話每個人都停在同一列，緞帶全部平行，完全看不出名次換過。
        QVector<int> order;
        for (int i = 0; i < people.size(); ++i) order.append(i);
        std::stable_sort(order.begin(), order.end(),
                         [&](int a, int b) { return sales[m][a] > sales[m][b]; });

        double total = 0;
        for (double v : sales[m]) total += v;
        for (int i : order) {
            nodeIndex[m][i] = static_cast<int>(nodes.size());
            nodes.append({QStringLiteral("%1 %2% (%3)")
                              .arg(people[i])
                              .arg(sales[m][i] / total * 100, 0, 'f', 0)
                              .arg(sales[m][i], 0, 'f', 0),
                          m, colors[i]});
        }
    }
    for (int m = 0; m + 1 < months.size(); ++m) {
        for (int i = 0; i < people.size(); ++i) {
            // 線的粗細取兩期的較小值，兩端才接得上各自的節點
            links.append({nodeIndex[m][i], nodeIndex[m + 1][i],
                          std::min(sales[m][i], sales[m + 1][i]), colors[i]});
        }
    }

    auto* sk = plot.addSankey(nodes, links);
    sk->extent = CoordinateRect(0, 10, 0, 10);
    sk->nodeWidthFraction = 0.14;
    sk->nodePaddingFraction = 0.015;
    sk->linkOpacity = 0.35;
    sk->labelFont.size = 8;
    sk->labelGapPx = -4;   // 負值把標籤放到節點方塊「裡面」

    // 期間的標題
    for (int m = 0; m < months.size(); ++m) {
        double total = 0;
        for (double v : sales[m]) total += v;
        auto* t = plot.addText(QStringLiteral("%1 (%2)").arg(months[m]).arg(total, 0, 'f', 0),
                               m * 4.3 + 0.7, 10.4);
        t->alignment = Qt::AlignHCenter | Qt::AlignBottom;
        t->label.font.bold = true;
    }

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-0.4, 10.4, -0.3, 11.4);
    plot.title.text = QStringLiteral("業務員月對月訂單排名");
}

QTPLOT_RECIPE("圖種", "流量與關係", "環形版面的網路圖",
              "節點少、邊多的時候，力導向會把所有節點擠成一團；"
              "排成一個圓反而看得清楚每一條連線。"
              "layout 換成 Circular 即可，其餘設定都一樣。") {
    const QStringList cities{
        QStringLiteral("首爾"), QStringLiteral("釜山"), QStringLiteral("濟州"),
        QStringLiteral("大邱"), QStringLiteral("光州"), QStringLiteral("蔚山"),
        QStringLiteral("浦項"), QStringLiteral("原州"), QStringLiteral("群山"),
        QStringLiteral("清州")};
    const QVector<double> sizes{9, 7, 10, 4, 3, 3, 5, 2, 2, 4};

    QVector<GraphNode> nodes;
    for (int i = 0; i < cities.size(); ++i) {
        // 大的紅、中的黃、小的綠
        const QColor c = sizes[i] >= 7 ? QColor(220, 75, 75)
                                       : (sizes[i] >= 4 ? QColor(245, 200, 90)
                                                        : QColor(70, 175, 120));
        nodes.append({cities[i], sizes[i], c, {0, 0}, false});
    }
    QVector<GraphEdge> edges;
    const QVector<QPair<int, int>> routes{{0, 1}, {0, 2}, {0, 3}, {0, 5}, {0, 6},
                                          {1, 2}, {2, 3}, {2, 4}, {2, 6}, {2, 7},
                                          {2, 8}, {2, 9}, {1, 4}, {3, 7}, {5, 9}};
    for (const auto& r : routes) edges.append({r.first, r.second, 1.0, QColor()});

    auto* g = plot.addNetwork(nodes, edges);
    g->layout = NetworkGraph::Layout::Circular;
    g->extent = CoordinateRect(0, 10, 0, 10);
    g->nodeMinDiameterPx = 26;
    g->nodeMaxDiameterPx = 62;
    g->labelFont.size = 9;
    g->labelFont.color = Qt::white;
    g->edgeLine = LineStyle(QColor(150, 150, 150, 180), 1.2);

    plot.grid.isVisible = false;
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("圖種", "流量與關係", "弦圖",
              "節點排成一個圓，兩兩之間的往來畫成穿過圓心的緞帶。"
              "緞帶的控制點放在圓心，所以中間會收束成一束。\n"
              "和桑基圖的分工：桑基圖適合有方向、有階段的流程；"
              "弦圖適合兩兩互有往來、沒有明顯先後的關係（貿易、遷徙、轉換矩陣）。") {
    const QStringList regions{QStringLiteral("北美"), QStringLiteral("歐洲"),
                              QStringLiteral("東亞"), QStringLiteral("南亞"),
                              QStringLiteral("非洲")};
    // matrix[i][j] = i 流向 j。不對稱代表兩個方向的量不同。
    const QVector<QVector<double>> flow{
        {0,  32, 28, 11,  6},
        {30,  0, 19,  9, 14},
        {35, 22,  0, 17,  8},
        {12,  8, 21,  0,  5},
        { 5, 16,  7,  4,  0},
    };
    auto* ch = plot.addChord(regions, flow);
    ch->labelFont.bold = true;
    ch->gapDegrees = 3;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("圖種", "流量與關係", "弦圖的對角線",
              "對角線 matrix[i][i] 是「流回自己」的量，會畫成只貼著自己那一段弧的"
              "小緞帶。一般的流量矩陣對角線通常是 0；轉換矩陣（例如狀態機停留在"
              "原狀態）則用得到。") {
    const QStringList states{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")};
    const QVector<QVector<double>> m{
        {40, 25, 10},
        {15, 50,  8},
        {12,  6, 35},
    };
    auto* ch = plot.addChord(states, m);
    ch->labelFont.bold = true;
    ch->labelFont.size = 13;
    ch->arcThicknessFraction = 0.11;
    plot.equalAxisScale = true;
}

// ───────────────────── 立體長條 ─────────────────────

QTPLOT_RECIPE("圖種", "立體長條", "立體長條圖",
              "Bars3D 把每根長條往固定方向推一段距離，畫出前面、頂面、側面三個多邊形。"
              "這是「看起來像 3D」而不是真的 3D：沒有相機也沒有透視，"
              "真正的投影反而會讓長度不能直接比較。\n"
              "注意這種圖本身是會誤導的 —— 頂面是斜的，讀者不容易判斷頂端對到哪個刻度，"
              "而且前排會遮住後排。要精確比較數值請用一般的 addBars。") {
    auto* b = plot.addBars3D({16, 21, 38, 72, 89});
    for (int i = 0; i < b->bars.size(); ++i)
        b->bars[i].label = QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0'));
    b->labelFont.color = Qt::white;
    b->labelFont.bold = true;
    b->labelFont.size = 14;
}

QTPLOT_RECIPE("圖種", "立體長條", "擠出的深度與方向",
              "depthPx 是「像素」而不是資料單位 —— 縮放時立體感才不會跟著變形。"
              "代價是 axisLimits() 不知道該留多少空間（那取決於繪圖區多大），"
              "所以它用 depthPadFraction 留一個比例的餘裕。"
              "視野抓得很緊的時候請自己 setAxisLimits。") {
    const QVector<double> values{40, 65, 50, 80};
    const QVector<double> depths{10, 22, 34, 46};
    // depthPx 是整個圖元共用的，所以四種深度要分成四個圖元
    for (int i = 0; i < values.size(); ++i) {
        auto* one = plot.addBars3D({values[i]});
        one->bars[0].position = i;
        one->bars[0].fillColor = QColor(31, 119, 180);
        one->bars[0].label = QStringLiteral("%1 px").arg(depths[i]);
        one->depthPx = depths[i];
        one->labelFont.color = Qt::white;
        one->labelFont.bold = true;
    }
    plot.setAxisLimits(-0.6, 4.2, 0, 100);
}

QTPLOT_RECIPE("圖種", "立體長條", "資訊圖表風格",
              "立體長條配上引線標註就是常見的簡報版型。"
              "長條本身只負責高度與編號，百分比與說明交給 Callout —— "
              "文字放在長條外面才不會被遮住，而且長條再矮也擺得下。") {
    const QVector<double> values{16, 21, 38, 72, 89};
    const QStringList notes{QStringLiteral("Maecenas vel\nmauris nunc"),
                            QStringLiteral("Fusce tincidunt\nlobortis nulla"),
                            QStringLiteral("Aenean rutrum\nlacinia ipsum"),
                            QStringLiteral("Donec gravida\nnulla quis"),
                            QStringLiteral("Vivamus diam\ntortor")};

    auto* b = plot.addBars3D(values);
    b->depthPx = 30;
    b->edgeLine = LineStyle(QColor(255, 255, 255, 70), 1.0);
    for (int i = 0; i < b->bars.size(); ++i) {
        b->bars[i].fillColor = QColor(26, 115, 232);
        b->bars[i].label = QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0'));
    }
    b->labelFont.color = Qt::white;
    b->labelFont.bold = true;
    b->labelFont.size = 15;

    // 引線從長條頂端拉到左上方的百分比
    for (int i = 0; i < values.size(); ++i) {
        auto* c = plot.addCallout(QStringLiteral("%1%\n%2").arg(values[i]).arg(notes[i]),
                                  i - 0.3, values[i] + 4, i - 1.05, values[i] + 24);
        c->label.font.color = QColor(21, 101, 192);
        c->label.font.bold = true;
        c->background = FillStyle::none();
        c->border = LineStyle::none();
        c->leader = LineStyle(QColor(21, 101, 192), 1.0);
        c->arrowHead = false;
    }

    plot.title.text = QStringLiteral("BUSINESS INFOGRAPHIC");
    plot.title.font.color = QColor(21, 101, 192);
    plot.title.font.size = 18;
    plot.grid.isVisible = false;
    // 四根軸全關。只關左下兩根的話外框還在 —— 上／右軸預設就是當框線用的
    for (Axis* a : plot.axes()) a->isVisible = false;
    plot.setAxisLimits(-1.6, 5.0, 0, 135);
}

// ───────────────────── 基本形狀 ─────────────────────

QTPLOT_RECIPE("圖種", "基本形狀", "橢圓扇形",
              "Sector 畫的是橢圓內部由一段弧和圓心圍出來的區域。"
              "radiusX 和 radiusY 不等就是橢圓扇形，相等就是一般的圓扇形。"
              "角度是「參數角」：點的位置是 (rx·cos θ, ry·sin θ)，"
              "rx != ry 時它和幾何角並不相同，但這是畫橢圓弧的通用約定。") {
    auto* sec = plot.addSector(0, 0, 2.0, 0, 135, 1.0);
    sec->fill = FillStyle(QColor(180, 180, 245, 160));
    sec->line = LineStyle(QColor(0, 0, 0), 2.0);
    plot.setAxisLimits(-2.2, 2.2, -1.6, 1.6);
}

QTPLOT_RECIPE("圖種", "基本形狀", "環狀扇形",
              "innerRadius 給正值就會在中間挖一個洞，變成兩個同心圓之間的一段。"
              "內弧是反向繞回來接上的，所以填色區域只有兩圓之間那一圈。") {
    auto* sec = plot.addSector(0, 0, 2.5, 0, 135);
    sec->setInnerRadius(1.2);
    sec->fill = FillStyle(QColor(180, 180, 245, 160));
    sec->line = LineStyle(QColor(0, 0, 0), 2.0);
    plot.equalAxisScale = true;
    plot.setAxisLimits(-3, 3, -2.4, 2.4);
}

QTPLOT_RECIPE("圖種", "基本形狀", "環狀橢圓扇形",
              "內外半徑各自分 X / Y 兩個方向，就是兩個橢圓之間的一段。"
              "四種形狀（圓扇形、橢圓扇形、環狀扇形、環狀橢圓扇形）用的是同一個類別，"
              "差別只在內半徑給不給、兩個方向的半徑等不等。") {
    auto* sec = plot.addSector(0, 0, 3.0, 0, 135, 2.0);
    sec->innerRadiusX = 1.5;
    sec->innerRadiusY = 1.0;
    sec->fill = FillStyle(QColor(180, 180, 245, 160));
    sec->line = LineStyle(QColor(0, 0, 0), 2.0);
    plot.setAxisLimits(-3.4, 3.4, -0.6, 3.2);
}

QTPLOT_RECIPE("圖種", "基本形狀", "圓弧",
              "Arc 是一條開放的曲線，不填色也不連回圓心 —— 封起來會多一條弦。"
              "0° 在正右方，逆時針為正。底下疊一個整圓當對照。") {
    auto* disc = plot.addSector(0, 0, 1.0, 0, 360);
    disc->fill = FillStyle(QColor(185, 185, 245, 160));
    disc->line = LineStyle::none();

    auto* arc = plot.addArc(0, 0, 1.0, 45, 135);
    arc->line = LineStyle(QColor(0, 0, 0), 3.0);

    plot.equalAxisScale = true;
    plot.setAxisLimits(-1.5, 1.5, -1.2, 1.2);
}

QTPLOT_RECIPE("圖種", "基本形狀", "橢圓弧",
              "兩個半徑不等就是橢圓弧。和 Sector 共用同一組角度約定，"
              "所以同一組角度畫出來的弧線會正好貼在扇形的外緣上。") {
    auto* disc = plot.addSector(0, 0, 1.4, 0, 360, 1.0);
    disc->fill = FillStyle(QColor(185, 185, 245, 160));
    disc->line = LineStyle::none();

    auto* arc = plot.addArc(0, 0, 1.4, 20, 140, 1.0);
    arc->line = LineStyle(QColor(0, 0, 0), 3.0);

    plot.setAxisLimits(-1.8, 1.8, -1.2, 1.6);
}

// ───────────────────── 參考線與區間 ─────────────────────

QTPLOT_RECIPE("圖種", "參考線與區間", "水平與垂直線",
              "AxisLine 會橫跨整張圖，縮放時固定在它的座標上。"
              "text 設了就會在線端印標籤。") {
    plot.addSignal(gen::randomWalk(200));
    auto* h = plot.addHorizontalLine(0, QColor(200, 0, 0));
    h->line.pattern = LinePattern::Dashed;
    h->text.text = QStringLiteral("基準");
    h->text.font.color = QColor(200, 0, 0);
    auto* v = plot.addVerticalLine(100, QColor(0, 140, 0));
    v->text.text = QStringLiteral("事件");
    v->text.font.color = QColor(0, 140, 0);
}

QTPLOT_RECIPE("圖種", "參考線與區間", "區間標示",
              "AxisSpan 用來標出一段範圍，例如異常時段或可接受區間。") {
    plot.addSignal(gen::randomWalk(300));
    auto* span = plot.addHorizontalSpan(-5, 5, QColor(0, 150, 0));
    span->legendText = QStringLiteral("可接受範圍");
    plot.addVerticalSpan(120, 160, QColor(200, 0, 0))->legendText = QStringLiteral("異常時段");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignBottom;
}

// ───────────────────── 形狀與標註 ─────────────────────

QTPLOT_RECIPE("圖種", "形狀與標註", "矩形、橢圓、多邊形",
              "三種形狀共用一個 Shape 類別，用 kind 切換。座標都是資料座標，"
              "所以會隨縮放一起變形。") {
    plot.addRectangle(1, 4, 1, 3);
    plot.addEllipse(7, 2, 2, 1);
    plot.addPolygon({{1, 5}, {3, 8}, {5, 5}, {4, 4}, {2, 4}});
    plot.setAxisLimits(0, 10, 0, 9);
}

QTPLOT_RECIPE("圖種", "形狀與標註", "文字與箭頭",
              "Text 釘在資料座標上（隨縮放移動），Annotation 釘在繪圖區角落（不動）。") {
    plot.addSignal(gen::sin(100, 1.0, 2.0));
    auto* t = plot.addText(QStringLiteral("波峰在這"), 25, 1.0);
    t->alignment = Qt::AlignHCenter | Qt::AlignBottom;
    t->offsetY = -8;
    t->background = FillStyle(QColor(255, 255, 200));
    t->border = LineStyle(QColor(120, 120, 80), 1.0);
    plot.addArrow(25, 1.0, 40, 0.6, QColor(200, 0, 0));
    plot.addAnnotation(QStringLiteral("Annotation 固定在角落"),
                       Qt::AlignRight | Qt::AlignBottom);
}

QTPLOT_RECIPE("圖種", "形狀與標註", "十字線",
              "Crosshair 通常接在 PlotWidget 的 mouseMoved 訊號上跟著游標跑；"
              "這裡固定一個位置示意。") {
    plot.addSignal(gen::randomWalk(200));
    auto* c = plot.addCrosshair(100, 0);
    c->position.y = plot.dataLimits().y.center();
}

// ───────────────────────── 魚骨圖 ─────────────────────────

QTPLOT_RECIPE("圖種", "流程與結構", "魚骨圖（石川圖）",
              "右端是要解決的問題，脊椎往左延伸，主骨是原因的「類別」，"
              "類別底下再掛具體原因，原因還可以再分一層。\n"
              "品管上最常用的分類是 6M：人、機、料、法、環、測。"
              "Fishbone::sixM() 直接給這六根空骨架，填 causes 就好。\n"
              "這是一張結構圖，沒有數值也沒有刻度 —— 版面完全由 extent 決定，"
              "要調大小請改 extent，不要去縮座標軸。") {
    QVector<FishboneCategory> cats = Fishbone::sixM();
    cats[0].causes = {{QStringLiteral("新人未受訓"), {QStringLiteral("輪班交接")}},
                      {QStringLiteral("作業手勢不一"), {}}};
    cats[1].causes = {{QStringLiteral("主軸軸承磨耗"), {QStringLiteral("潤滑週期過長")}},
                      {QStringLiteral("夾具鬆動"), {}}};
    cats[2].causes = {{QStringLiteral("來料厚度超差"), {QStringLiteral("供應商換線")}},
                      {QStringLiteral("批次混料"), {}}};
    cats[3].causes = {{QStringLiteral("切削參數未更新"), {}},
                      {QStringLiteral("抽檢頻率不足"), {QStringLiteral("SOP 未修訂")}}};
    cats[4].causes = {{QStringLiteral("車間溫差大"), {}},
                      {QStringLiteral("粉塵沉積"), {}}};
    cats[5].causes = {{QStringLiteral("量規未校正"), {QStringLiteral("校期逾期")}},
                      {QStringLiteral("量測者差異"), {}}};

    auto* fb = plot.addFishbone(QStringLiteral("外徑尺寸\n不良率偏高"), cats);
    fb->extent = {0, 10, 0, 6};

    plot.title.text = QStringLiteral("魚骨圖：外徑不良的可能原因");
    plot.setAxisLimits(-0.3, 10.3, -0.4, 6.4);
}

QTPLOT_RECIPE("圖種", "流程與結構", "魚骨圖的版面參數",
              "三個參數決定好不好讀。\n"
              "boneAngleDegrees 是主骨和脊椎的夾角：角度小的話骨頭躺得平、往左伸得遠，"
              "容易和左邊的鄰居打架；角度大則骨頭接近垂直，掛在上面的原因文字會擠成一團。\n"
              "alternateSides 關掉就全部排在上面，類別少的時候比較整齊。\n"
              "showSubCauses 在原因一多的時候關掉 —— 第二層會擠到看不清楚，"
              "與其畫出來看不懂，不如不畫。") {
    QVector<FishboneCategory> cats{
        {QStringLiteral("流程"), {{QStringLiteral("步驟過多"), {}},
                                  {QStringLiteral("缺少覆核"), {}}}, QColor()},
        {QStringLiteral("系統"), {{QStringLiteral("介面難用"), {}},
                                  {QStringLiteral("回應太慢"), {}}}, QColor()},
        {QStringLiteral("人員"), {{QStringLiteral("訓練不足"), {}},
                                  {QStringLiteral("人力吃緊"), {}}}, QColor()},
    };

    auto* fb = plot.addFishbone(QStringLiteral("結案時間過長"), cats);
    fb->extent = {0, 10, 0, 6};
    fb->boneAngleDegrees = 75;      // 骨頭立起來，左右不會互相擠
    fb->alternateSides = false;     // 三根全部排在上面
    fb->boneLengthFraction = 0.85;
    fb->boxCategoryLabels = true;

    plot.title.text = QStringLiteral("立起來的骨頭、全部排在同一側");
    plot.setAxisLimits(-0.3, 10.3, -0.4, 6.4);
}

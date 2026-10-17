// Statistics.cpp — 統計圖的範例：箱型圖、棒棒糖圖、分布圖、小提琴圖、迴歸。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

namespace {

/// 幾組形狀不同的樣本，用來看統計圖畫不畫得出差異。
QVector<QVector<double>> sampleGroups() {
    gen::setSeed(42);
    QVector<double> a = gen::randomNormal(80, 50, 8);
    QVector<double> b = gen::randomNormal(80, 58, 14);
    QVector<double> c = gen::randomNormal(40, 42, 4) + gen::randomNormal(40, 72, 4);  // 雙峰
    QVector<double> d = gen::randomNormal(80, 52, 9);
    d.append({92, 95, 14});  // 幾個離群值
    return {a, b, c, d};
}

// 用函式而不是檔案層級的常數：常數只在程式啟動時建一次，換語言之後還是舊的
QStringList groupNames() {
    return {RT("對照組"), RT("低劑量"), RT("中劑量"), RT("高劑量")};
}

}  // namespace

QTPLOT_RECIPE("統計", "箱型與分布", "箱型圖",
              "箱是四分位數、中間粗線是中位數、鬚延伸到 1.5 倍 IQR 之內最遠的資料點，"
              "超出的畫成離群值 —— 這是 Tukey 的原始定義。"
              "四分位數用線性內插法，和 R 的 type 7、numpy.percentile 的預設一致。") {
    plot.addBoxPlot(sampleGroups(), groupNames());
    plot.leftAxis()->label.text = RT("量測值");
    plot.title.text = RT("四組樣本的分布");
}

QTPLOT_RECIPE("統計", "箱型與分布", "橫向箱型圖與平均值",
              "horizontal 讓箱子橫躺，類別名稱長的時候好讀。"
              "meanMarker 預設不畫 —— 箱中間那條線是中位數，再加一個平均很容易被誤讀，"
              "要畫就該讓它明顯不同。") {
    auto* bp = plot.addBoxPlot(sampleGroups(), {});
    bp->horizontal = true;
    bp->meanMarker = MarkerStyle(MarkerShape::FilledDiamond, 9, QColor(220, 20, 20));

    auto manual = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < groupNames().size(); ++i)
        manual->ticks.append({double(i), groupNames()[i], true});
    plot.leftAxis()->tickGenerator = manual;
    plot.bottomAxis()->tickGenerator = std::make_shared<AutomaticTickGenerator>();
    plot.addAnnotation(RT("紅色菱形 = 平均值"), Qt::AlignRight | Qt::AlignBottom);
}

QTPLOT_RECIPE("統計", "箱型與分布", "小提琴圖",
              "箱型圖只看得到五個數字，看不出分布的形狀。"
              "小提琴圖疊上核密度曲線，第三組的雙峰就露出來了 —— 它在箱型圖裡"
              "只是一個略高的箱子，完全看不出中間是空的。"
              "注意核密度估計的頻寬：預設的 Silverman 經驗法則是為單峰分布設計的，"
              "兩個峰靠得近時它會把中間的谷抹平，看起來就像單峰。"
              "懷疑有多峰時要自己把頻寬調小再看一次。") {
    plot.addViolin(sampleGroups(), groupNames());
    plot.leftAxis()->label.text = RT("量測值");
}

QTPLOT_RECIPE("統計", "箱型與分布", "分布圖的成分組合",
              "Population 把原始點、箱、誤差線、密度曲線拆成可自由開關的成分。"
              "樣本數少的時候，直接畫出每一個點比任何摘要都誠實。") {
    gen::setSeed(7);
    auto* pop = plot.addPopulation({gen::randomNormal(14, 50, 7), gen::randomNormal(14, 60, 10),
                                    gen::randomNormal(14, 55, 4)},
                                   {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    pop->showPoints = true;
    pop->showBox = true;
    pop->showErrorBar = true;
    pop->errorKind = Population::ErrorKind::StdErr;
    pop->jitter = 0.5;
    plot.addAnnotation(RT("點 = 原始資料，箱 = 四分位數，線 = 平均 ± 標準誤"),
                       Qt::AlignLeft | Qt::AlignTop);
}

QTPLOT_RECIPE("統計", "箱型與分布", "核密度估計",
              "kernelDensity() 可以單獨用。頻寬不給就用 Silverman 經驗法則："
              "1.06 × min(σ, IQR/1.34) × n^(-1/5)，有離群值時不會把曲線抹得太平。") {
    gen::setSeed(3);
    QVector<double> data = gen::randomNormal(300, 40, 6);
    data += gen::randomNormal(200, 65, 9);

    // 先自己算直方圖，才拿得到真正的箱寬 —— 用猜的範圍去推箱寬，
    // 曲線的尺度就會對不上長條，看起來像 KDE 算錯。
    const Histogram hist = Histogram::fromValues(data, 40);
    plot.addBars(hist.toBars(QColor(200, 200, 210)));

    const double lo = hist.binEdges.first() - hist.binWidth * 3;
    const double hi = hist.binEdges.last() + hist.binWidth * 3;
    const QVector<double> xs = gen::linspace(lo, hi, 300);
    const QVector<double> dens = kernelDensity(data, xs);
    // 密度是機率密度，要乘上「總數 × 箱寬」才和直方圖的次數同一個尺度
    const double scale = data.size() * hist.binWidth;
    QVector<double> counts;
    for (double d : dens) counts.append(d * scale);

    auto* curve = plot.addScatter(xs, counts, QColor(200, 30, 30));
    curve->marker = MarkerStyle::none();
    curve->line.width = 2;
    curve->legendText = RT("核密度估計");
    plot.legend.isVisible = true;
    plot.leftAxis()->label.text = RT("次數");
}

QTPLOT_RECIPE("統計", "棒棒糖與排名", "棒棒糖圖",
              "細桿加一個圓頭。類別多但數值差異小時比長條圖好讀 —— "
              "長條那一大片顏色會把差異蓋掉。") {
    const QStringList names{RT("甲"), RT("乙"), RT("丙"),
                            RT("丁"), RT("戊"), RT("己"),
                            RT("庚"), RT("辛")};
    plot.addLollipop({82, 79, 88, 75, 91, 84, 77, 86});

    auto manual = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < names.size(); ++i) manual->ticks.append({double(i), names[i], true});
    plot.bottomAxis()->tickGenerator = manual;
    plot.leftAxis()->label.text = RT("分數");
}

QTPLOT_RECIPE("統計", "棒棒糖與排名", "橫向排名",
              "horizontal 加上排序過的資料就是排名圖。baseline 可以移到別的值，"
              "例如以平均為基準看誰高於平均。") {
    QVector<double> values{8.2, -3.1, 5.4, -6.8, 2.3, 9.1, -1.5};
    const QStringList names{RT("北區"), RT("南區"), RT("中區"),
                            RT("東區"), RT("西區"), RT("離島"),
                            RT("海外")};
    QVector<LollipopItem> items;
    for (int i = 0; i < values.size(); ++i) {
        LollipopItem it;
        it.position = i;
        it.value = values[i];
        it.color = values[i] >= 0 ? QColor(44, 160, 44) : QColor(214, 39, 40);
        items.append(it);
    }
    auto* l = plot.emplace<Lollipop>();
    l->items = items;
    l->horizontal = true;
    l->showValueLabels = true;
    l->labelFont.size = 9;

    auto manual = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < names.size(); ++i) manual->ticks.append({double(i), names[i], true});
    plot.leftAxis()->tickGenerator = manual;
    plot.addVerticalLine(0, QColor(120, 120, 120));
    plot.bottomAxis()->label.text = RT("年增率 (%)");
}

QTPLOT_RECIPE("統計", "迴歸", "線性迴歸",
              "linearFit() 回傳斜率、截距與 R²；addRegressionLine 直接把線畫出來。"
              "迴歸線會畫到資料範圍外一成，縮放時才不會看到線突然斷掉。") {
    gen::setSeed(11);
    const QVector<double> xs = gen::randomSample(60, 0, 100);
    QVector<double> ys;
    for (double x : xs) ys.append(0.8 * x + 12);
    ys = gen::addNoise(ys, 9);

    plot.addMarkers(xs, ys)->legendText = RT("觀測值");

    LinearFit fit;
    auto* line = plot.addRegressionLine(xs, ys, &fit, QColor(214, 39, 40));
    line->line.width = 2;
    line->legendText = RT("最小平方擬合");

    plot.addAnnotation(QStringLiteral("y = %1x + %2\nR² = %3")
                           .arg(fit.slope, 0, 'f', 3)
                           .arg(fit.intercept, 0, 'f', 2)
                           .arg(fit.rSquared, 0, 'f', 4),
                       Qt::AlignLeft | Qt::AlignTop);
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
}

QTPLOT_RECIPE("統計", "迴歸", "殘差圖",
              "迴歸線本身看不出模型對不對；把殘差對 x 畫出來，"
              "有系統性的彎曲就表示線性模型不適合。這組資料其實是二次的。") {
    gen::setSeed(5);
    const QVector<double> xs = gen::linspace(0, 10, 60);
    QVector<double> ys;
    for (double x : xs) ys.append(0.6 * x * x - 2 * x + 5);
    ys = gen::addNoise(ys, 1.5);

    const LinearFit fit = linearFit(xs, ys);
    QVector<double> residuals;
    for (int i = 0; i < xs.size(); ++i) residuals.append(ys[i] - fit.predict(xs[i]));

    plot.addMarkers(xs, residuals, QColor(31, 119, 180));
    plot.addHorizontalLine(0, QColor(80, 80, 80))->line.pattern = LinePattern::Dashed;
    plot.leftAxis()->label.text = RT("殘差");
    plot.bottomAxis()->label.text = QStringLiteral("x");
    plot.addAnnotation(RT("殘差呈 U 形 → 線性模型不適合（R² = %1 仍然偏高）")
                           .arg(fit.rSquared, 0, 'f', 3),
                       Qt::AlignHCenter | Qt::AlignTop);
}

QTPLOT_RECIPE("統計", "箱型與分布", "分組箱型圖加平均標記",
              "每個類別並排兩個箱子（本期 vs 上期），位置各自偏移半個箱寬。"
              "meanMarker 把平均值也畫出來 —— 預設不畫是因為箱子的橫線是"
              "中位數，兩條線擺在一起很容易被誤讀成同一個東西，"
              "所以要畫就得用「形狀不同」的標記，不要再畫一條橫線。") {
    gen::setSeed(12);
    const QStringList people{QStringLiteral("John"), QStringLiteral("Smith"),
                             QStringLiteral("Rhonda"), QStringLiteral("Simon")};
    const QVector<double> currentCenter{157, 134, 153, 101};
    const QVector<double> previousCenter{137, 146, 145, 134};
    const QColor currentColor(70, 130, 180);
    const QColor previousColor(240, 150, 60);

    QVector<Box> boxes;
    for (int i = 0; i < people.size(); ++i) {
        for (int which = 0; which < 2; ++which) {
            const double center = which == 0 ? currentCenter[i] : previousCenter[i];
            QVector<double> samples;
            for (int k = 0; k < 24; ++k)
                samples.append(center + gen::randomNormal(1, 0, 22).first());

            Box b;
            b.position = i + (which == 0 ? -0.2 : 0.2);
            b.size = 0.34;
            b.stats = BoxStats::fromValues(samples);
            b.fillColor = which == 0 ? currentColor : previousColor;
            if (i == 0) b.legendText = which == 0 ? RT("本期訂單")
                                                  : RT("上期訂單");
            boxes.append(b);
        }
    }

    auto* bp = plot.addBoxPlot(boxes);
    bp->meanMarker = MarkerStyle(MarkerShape::Eks, 8, QColor(60, 60, 60));

    // 平均值標在箱子旁邊
    for (const Box& b : boxes) {
        auto* t = plot.addText(QStringLiteral("%1").arg(b.stats.mean, 0, 'f', 1),
                               b.position, b.stats.mean);
        t->alignment = Qt::AlignHCenter | Qt::AlignBottom;
        t->offsetY = -6;
        t->label.font.size = 8;
    }

    QVector<Tick> ticks;
    for (int i = 0; i < people.size(); ++i) ticks.append({static_cast<double>(i), people[i]});
    auto gen2 = std::make_shared<ManualTickGenerator>();
    gen2->ticks = ticks;
    plot.bottomAxis()->tickGenerator = gen2;
    plot.bottomAxis()->label.text = RT("業務員");
    plot.leftAxis()->label.text = RT("訂單數");
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
    plot.title.text = RT("員工訂單分析");
}

QTPLOT_RECIPE("統計", "箱型與分布", "Likert 量表圖",
              "問卷的同意度：負面選項往左堆、正面往右堆，中性的那一段跨在 0 上。"
              "基準線放在「中性的中點」而不是 0 的邊緣 —— "
              "中性本身沒有方向，整段推到任一側都會讓那一邊看起來比較多。\n"
              "每一列的總寬不同是正常的：各題的回收樣本數本來就不一樣，"
              "這裡畫的是百分比。") {
    const QStringList questions{
        RT("整體 (2.93k)"),
        RT("清潔人員態度 (1.01k)"),
        RT("清潔服務品質 (761)"),
        RT("是否會推薦 (1.16k)")};
    // 七個等級：非常不同意 → 非常同意，中間第 4 級是中性
    const QVector<QVector<double>> pct{
        {18, 7, 12, 18, 13, 15, 17},
        {33, 2, 11, 9, 15, 17, 22},
        {9, 10, 11, 31, 12, 20, 7},
        {20, 4, 12, 21, 11, 11, 20}};
    const QVector<QColor> levelColors{
        QColor(230, 30, 30),  QColor(240, 90, 90),  QColor(250, 150, 150),
        QColor(200, 200, 200),
        QColor(170, 225, 170), QColor(105, 205, 105), QColor(40, 180, 40)};

    QVector<Bar> bars;
    for (int q = 0; q < questions.size(); ++q) {
        // 先算出這一列的起點：負面三段的總和，再加上中性的一半
        double left = 0;
        for (int k = 0; k < 3; ++k) left += pct[q][k];
        left += pct[q][3] / 2;

        double cursor = -left;
        for (int k = 0; k < pct[q].size(); ++k) {
            Bar b;
            b.position = questions.size() - 1 - q;  // 第一題排在最上面
            b.valueBase = cursor;
            cursor += pct[q][k];
            b.value = cursor;
            b.size = 0.62;
            b.fillColor = levelColors[k];
            bars.append(b);
        }
    }
    auto* bp = plot.addBars(bars);
    bp->horizontal = true;
    bp->borderLine = LineStyle(QColor(255, 255, 255), 1.0);

    QVector<Tick> ticks;
    for (int q = 0; q < questions.size(); ++q)
        ticks.append({static_cast<double>(questions.size() - 1 - q), questions[q]});
    auto cat = std::make_shared<ManualTickGenerator>();
    cat->ticks = ticks;
    plot.leftAxis()->tickGenerator = cat;

    auto xgen = std::make_shared<AutomaticTickGenerator>();
    xgen->labelFormatter = [](double v, double spacing) {
        return formatTickLabel(std::abs(v), spacing, std::abs(v)) + QStringLiteral("%");
    };
    plot.bottomAxis()->tickGenerator = xgen;

    plot.addVerticalLine(0)->line = LineStyle(QColor(90, 90, 90), 1.5);
    plot.setAxisLimits(-62, 62, -0.6, questions.size() - 0.4);
    plot.title.text = RT("滿意度調查");
}

QTPLOT_RECIPE("統計", "箱型與分布", "蜂群圖",
              "每一筆原始資料都畫出來，點互相讓開不重疊。\n"
              "和箱型圖的分工：箱型圖把分布摘要成五個數字，樣本少的時候那會藏住"
              "重要的細節 —— 雙峰、離群值、資料其實只有幾個相異值，箱型圖都看不出來。"
              "這裡故意讓第三組是雙峰的：箱型圖畫出來和單峰幾乎一樣。\n"
              "樣本再多就不適用了，點會排到比箱子還寬。") {
    gen::setSeed(61);
    QVector<QVector<double>> groups(3);
    for (int i = 0; i < 70; ++i) groups[0].append(gen::randomNormal(1, 50, 9).first());
    for (int i = 0; i < 70; ++i) groups[1].append(gen::randomNormal(1, 58, 14).first());
    // 第三組：雙峰，但平均與四分位數和單峰的很接近
    for (int i = 0; i < 70; ++i)
        groups[2].append(gen::randomNormal(1, (i % 2) ? 36 : 70, 6).first());

    const QStringList names{RT("A 組"), RT("B 組"),
                            RT("C 組（雙峰）")};

    // 底下先放淡色的箱型圖當對照
    auto* bx = plot.addBoxPlot(groups, names);
    for (Box& b : bx->boxes) b.fillColor = QColor(0, 0, 0, 18);
    bx->outlierMarker = MarkerStyle::none();
    bx->borderLine = LineStyle(QColor(140, 140, 140), 1.0);
    bx->medianLine = LineStyle(QColor(90, 90, 90), 1.6);

    auto* sw = plot.addBeeswarm(groups, names);
    sw->marker = MarkerStyle(MarkerShape::FilledCircle, 7, QColor());
    sw->marker.outline = LineStyle(QColor(255, 255, 255, 180), 0.8);
    sw->spacingPx = 1.2;

    plot.leftAxis()->label.text = RT("分數");
    plot.title.text = RT("箱型圖藏住的東西");
}

QTPLOT_RECIPE("統計", "分群", "樹狀聚類圖",
              "凝聚式階層分群：每次把最近的兩群併起來，合併的高度就是當時的距離。"
              "橫線的高度愈低，代表那兩群愈像。\n"
              "葉子的順序不是原始順序，是從合併結果推出來的 —— "
              "照原始索引排的話，同一群的葉子會散在兩端，看起來像分群失敗。\n"
              "colorThreshold 在指定的距離把樹切開，切出來的每一群各一個顏色。") {
    const QStringList cities{
        RT("台北"), RT("桃園"), RT("新竹"),
        RT("台中"), RT("彰化"), RT("台南"),
        RT("高雄"), RT("屏東"), RT("花蓮"),
        RT("台東")};
    // 沿著西岸由北到南的大致位置，東岸另成一群
    const QVector<QPair<double, double>> coords{
        {0, 0},   {0.4, 0.3}, {0.6, 0.9}, {1.0, 2.4}, {1.1, 2.9},
        {1.5, 4.3}, {1.7, 4.9}, {1.9, 5.6}, {3.6, 2.2}, {3.9, 4.6}};

    QVector<QVector<double>> dist(cities.size(), QVector<double>(cities.size(), 0.0));
    for (int i = 0; i < cities.size(); ++i)
        for (int j = 0; j < cities.size(); ++j)
            dist[i][j] = std::hypot(coords[i].first - coords[j].first,
                                    coords[i].second - coords[j].second);

    const QVector<ClusterMerge> merges = hierarchicalCluster(dist, Linkage::Average);
    auto* dg = plot.addDendrogram(merges, cities.size(), cities);
    dg->colorThreshold = 1.8;
    dg->line.width = 1.8;
    dg->labelFont.bold = true;

    plot.bottomAxis()->isVisible = false;
    plot.leftAxis()->label.text = RT("群間距離");
    plot.grid.isVisible = false;
    plot.setAxisLimits(-0.7, cities.size() - 0.3, -0.45, dg->maxDistance() * 1.08);
}

QTPLOT_RECIPE("統計", "分群", "三種連結方式的差別",
              "同一份資料、三種 linkage，樹的形狀差很多。\n"
              "Single 取最近的一對成員，容易把兩群用一條「鏈」串起來；"
              "Complete 取最遠的一對，傾向切出緊湊、大小相近的群；"
              "Average 介於兩者之間。沒有哪個一定對 —— "
              "要看你認為「一群」的定義是什麼。") {
    gen::setSeed(70);
    const int n = 14;
    // 兩團點，中間放幾個橋接的點 —— Single 會被它們串起來
    QVector<QPair<double, double>> pts;
    for (int i = 0; i < 5; ++i)
        pts.append({gen::randomNormal(1, 0, 0.35).first(),
                    gen::randomNormal(1, 0, 0.35).first()});
    for (int i = 0; i < 5; ++i)
        pts.append({gen::randomNormal(1, 5, 0.35).first(),
                    gen::randomNormal(1, 0, 0.35).first()});
    for (int i = 0; i < 4; ++i) pts.append({1.1 + i * 0.95, 0.1});

    QVector<QVector<double>> dist(n, QVector<double>(n, 0.0));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            dist[i][j] = std::hypot(pts[i].first - pts[j].first,
                                    pts[i].second - pts[j].second);

    QStringList labels;
    for (int i = 0; i < n; ++i) labels.append(QString::number(i));

    const QVector<Linkage> kinds{Linkage::Single, Linkage::Complete, Linkage::Average};
    const QStringList names{QStringLiteral("Single"), QStringLiteral("Complete"),
                            QStringLiteral("Average")};
    double maxD = 0;
    for (Linkage k : kinds)
        for (const ClusterMerge& m : hierarchicalCluster(dist, k))
            maxD = std::max(maxD, m.distance);

    // 三棵樹並排。每棵都在自己的座標裡畫 0..n-1，要擺到不同的位置，
    // 靠的是給它一根「平移過的」隱藏 X 軸：
    // 把軸的範圍整個往左推 k·slot，這棵樹就出現在第 k 格。
    const double slot = n + 2;
    const double mainLo = -0.7, mainHi = -0.7 + slot * kinds.size();

    for (int k = 0; k < kinds.size(); ++k) {
        auto* shifted = plot.addAxis(Edge::Bottom);
        shifted->isVisible = false;
        shifted->setRange(mainLo - k * slot, mainHi - k * slot);

        auto* dg = plot.addDendrogram(hierarchicalCluster(dist, kinds[k]), n, labels);
        dg->xAxis = shifted;
        dg->labelFont.size = 7;
        dg->line.width = 1.2;
        dg->colorThreshold = 1.6;

        auto* title = plot.addText(names[k], (n - 1) / 2.0, maxD * 1.03);
        title->xAxis = shifted;
        title->alignment = Qt::AlignHCenter | Qt::AlignBottom;
        title->label.font.bold = true;
    }

    plot.bottomAxis()->isVisible = false;
    plot.leftAxis()->label.text = RT("群間距離");
    plot.grid.isVisible = false;
    plot.setAxisLimits(mainLo, mainHi, -0.5, maxD * 1.14);
}

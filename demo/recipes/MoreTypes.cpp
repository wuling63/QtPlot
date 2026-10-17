// MoreTypes.cpp — 柏拉圖、鬆餅圖、斜率／排名圖、山脊圖、日曆熱圖、沖積圖。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

// ───────────────────────── 柏拉圖 ─────────────────────────

QTPLOT_RECIPE("圖種", "商務圖表", "柏拉圖",
              "長條由大到小，加上一條累積百分比折線 —— 品管三大工具的第三個"
              "（另兩個是魚骨圖與管制圖）。\n"
              "重點是「少數幾項佔了大部分」：累積線先碰到 80% 的那幾項，"
              "就是投入同樣力氣能回收最多的地方。\n"
              "折線必須綁在自己的百分比軸上。和長條共用左軸的話，"
              "「80%」會被當成數量 80 畫上去 —— 兩條線的尺根本不同。") {
    QVector<ParetoItem> items{
        {RT("刮傷"), 186, QColor()},  {RT("尺寸超差"), 121, QColor()},
        {RT("毛邊"), 74, QColor()},   {RT("色差"), 41, QColor()},
        {RT("變形"), 28, QColor()},   {RT("污漬"), 19, QColor()},
        {RT("缺料"), 11, QColor()},   {RT("其他"), 8, QColor()},
    };
    auto* pr = plot.addPareto(items);
    pr->showValueLabels = true;

    const int n = pr->itemsToReach(80);
    auto* note = plot.addAnnotation(
        RT("前 %1 項就佔了 80%\n先處理這幾項").arg(n),
        Qt::AlignRight | Qt::AlignBottom);
    note->label.font.size = 10;

    plot.leftAxis()->label.text = RT("不良件數");
    plot.title.text = RT("不良原因的柏拉圖");
    plot.bottomAxis()->tickLabelRotation = -30;
    plot.grid.isVisible = false;
}

// ───────────────────────── 鬆餅圖 ─────────────────────────

QTPLOT_RECIPE("圖種", "商務圖表", "鬆餅圖",
              "用一格一格的方塊表示比例。和圓餅圖的分工：圓餅要比的是角度，"
              "而人判斷角度很差；鬆餅是「數格子」，10% 就是十格，看一眼就知道。\n"
              "代價是只適合少數幾類、比例也不能太零碎。\n"
              "格子的配額用最大餘額法分 —— 直接四捨五入的話格子總數會對不上，"
              "十個 3.33% 可以湊出 30 格也可以湊出 40 格。") {
    QVector<WaffleCategory> cats{
        {RT("已完成"), 62, QColor(70, 150, 90)},
        {RT("進行中"), 23, QColor(230, 170, 50)},
        {RT("未開始"), 15, QColor(190, 80, 65)},
    };
    plot.addWaffle(cats, 10, 10);
    plot.title.text = RT("專案進度（一格 1%）");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignVCenter;
}

// ───────────────────────── 斜率圖與排名圖 ─────────────────────────

QTPLOT_RECIPE("圖種", "商務圖表", "斜率圖",
              "兩期之間的變化。重點不是絕對值，是「誰升誰降」—— "
              "線的斜率一眼就讀得出來，而同樣的資料畫成分組長條圖就得逐組比對。\n"
              "highlight 把要講的那幾條加粗，其餘自動淡化。"
              "全部一樣濃的話，強調就沒有意義了。") {
    QVector<SlopeSeries> series{
        {RT("北區"), {412, 468}, QColor(), false},
        {RT("中區"), {365, 352}, QColor(), false},
        {RT("南區"), {298, 374}, QColor(200, 70, 50), true},
        {RT("東區"), {143, 138}, QColor(), false},
        {RT("離島"), {87, 96}, QColor(), false},
    };
    auto* sl = plot.addSlope({RT("去年"), RT("今年")}, series);
    sl->labelFont.size = 10;

    plot.leftAxis()->label.text = RT("營收（百萬）");
    plot.title.text = RT("南區是唯一大幅成長的");
    plot.setAxisLimits(-0.55, 1.55, 60, 500);
}

QTPLOT_RECIPE("圖種", "商務圖表", "排名變化圖（bump chart）",
              "多期的斜率圖。useRanks 改畫名次而不是數值 —— "
              "數值跨好幾個數量級時，看名次比看原始值清楚。\n"
              "Y 軸要配 inverted = true 讓第 1 名在最上面，"
              "否則第 1 名會畫在最底下，和「第一」的直覺相反。") {
    QVector<SlopeSeries> series{
        {RT("甲隊"), {1, 2, 2, 4, 3}, QColor(), false},
        {RT("乙隊"), {2, 1, 1, 1, 1}, QColor(45, 110, 185), true},
        {RT("丙隊"), {3, 4, 5, 2, 2}, QColor(), false},
        {RT("丁隊"), {4, 3, 3, 3, 5}, QColor(), false},
        {RT("戊隊"), {5, 5, 4, 5, 4}, QColor(), false},
    };
    // 名次愈小愈好，所以這裡直接把「值」當成名次：給 useRanks 的話會再排一次，
    // 變成「名次的名次」。資料本來就是名次時維持 useRanks = false。
    for (SlopeSeries& s : series)
        for (double& v : s.values) v = 6 - v;  // 轉成「分數」讓排名函式去排

    auto* sl = plot.addSlope({RT("第 1 季"), RT("第 2 季"),
                              RT("第 3 季"), RT("第 4 季"),
                              RT("年度")},
                             series);
    sl->useRanks = true;
    sl->showEndValues = true;
    sl->marker.size = 9;

    plot.leftAxis()->inverted = true;
    plot.leftAxis()->label.text = RT("名次");
    plot.title.text = RT("乙隊從第 2 名一路守到年度冠軍");
    plot.setAxisLimits(-0.75, 4.75, 0.4, 5.6);
}

// ───────────────────────── 山脊圖 ─────────────────────────

QTPLOT_RECIPE("統計", "箱型與分布", "山脊圖",
              "好幾條密度曲線上下錯開、彼此略微重疊。\n"
              "和並排的小提琴圖的分工：山脊圖把所有分布放在同一根 X 軸上，"
              "所以峰值左右移動一眼就看得出來 —— 那正是看分布隨時間變化時要看的。\n"
              "重疊是這種圖的重點：不重疊就只是一疊很扁的曲線，"
              "重疊太多則後面的完全被蓋住。\n"
              "畫的順序也有關係：要由下往上畫，上面那條才壓得住下面那條的尾巴；"
              "反過來的話重疊處會看起來像破洞。") {
    gen::setSeed(44);
    const QStringList months{RT("一月"), RT("三月"),
                             RT("五月"), RT("七月"),
                             RT("九月"), RT("十一月")};
    const QVector<double> means{14, 19, 25, 30, 26, 18};
    const QVector<double> spreads{3.0, 3.4, 2.8, 2.2, 3.0, 3.6};

    QVector<RidgeGroup> groups;
    for (int i = 0; i < months.size(); ++i) {
        RidgeGroup g;
        g.label = months[i];
        g.values = gen::randomNormal(300, means[i], spreads[i]);
        groups.append(g);
    }
    auto* r = plot.addRidgeline(groups);
    r->overlap = 1.1;

    plot.bottomAxis()->label.text = RT("日均溫（°C）");
    plot.title.text = RT("一年之中氣溫分布的移動");
}

// ───────────────────────── 日曆熱圖 ─────────────────────────

QTPLOT_RECIPE("場", "熱圖", "日曆熱圖",
              "一格一天、一欄一週，用顏色表示當天的值。\n"
              "和折線圖的分工：它放棄了「看得出大小」換來「看得出週期」——"
              "週末低、月底衝刺這類規律在折線上要盯著看，在日曆上直接變成直的條紋。\n"
              "沒有資料的日子會鋪成灰色而不是留白：留白看起來像那幾天不存在，"
              "而不是「那幾天沒量到」。") {
    gen::setSeed(99);
    const QDate start(2025, 1, 1);
    const QDate end(2025, 6, 30);
    QVector<CalendarValue> values;
    for (QDate d = start; d <= end; d = d.addDays(1)) {
        // 週末低、月底衝刺，再加上雜訊
        double v = 18 + gen::randomNormal(1, 0, 4).first();
        if (d.dayOfWeek() >= 6) v *= 0.25;
        if (d.day() >= 25) v *= 1.5;
        if (d.month() == 2 && d.day() < 10) continue;  // 刻意缺一段
        values.append({d, std::max(0.0, v)});
    }

    auto* cal = plot.addCalendarHeatmap(values);
    cal->colormap = Colormap(Colormap::Blues);
    plot.addColorbar(cal->colormap, cal->effectiveColorRange())->label.text =
        RT("提交次數");

    plot.title.text = RT("2025 上半年的每日活動量");
}

// ───────────────────────── 沖積圖 ─────────────────────────

QTPLOT_RECIPE("圖種", "流量與關係", "沖積圖（多階段流向）",
              "沖積圖和桑基圖底層是同一個東西，差別只在資料長相："
              "桑基圖給的是「節點與流量」，沖積圖給的是「一筆一筆的個案」。\n"
              "alluvialFromRecords() 把後者攤成前者。手動攤很容易算錯，"
              "尤其中間階段的節點要同時當入口和出口。\n"
              "分類在各階段內照「第一次出現的順序」排，不是字典序 —— "
              "字典序會把「高／中／低」排成「中高低」。") {
    gen::setSeed(61);
    const QStringList regions{RT("北區"), RT("中區"),
                              RT("南區")};
    const QStringList plans{RT("基本方案"), RT("進階方案")};
    const QStringList outcomes{RT("續約"), RT("升級"),
                               RT("流失")};

    QVector<QStringList> records;
    for (int i = 0; i < 400; ++i) {
        const int r = static_cast<int>(gen::randomUniform(1, 0, 3).first()) % 3;
        const bool advanced = gen::randomUniform(1, 0, 1).first() < (r == 0 ? 0.6 : 0.3);
        const double u = gen::randomUniform(1, 0, 1).first();
        int o = 0;
        if (advanced) o = (u < 0.70) ? 0 : (u < 0.92 ? 1 : 2);
        else o = (u < 0.55) ? 0 : (u < 0.68 ? 1 : 2);
        records.append({regions[r], plans[advanced ? 1 : 0], outcomes[o]});
    }

    const AlluvialResult al = alluvialFromRecords(
        records, {RT("區域"), RT("方案"), RT("結果")});
    auto* sk = plot.addSankey(al.nodes, al.links);
    sk->nodeWidthFraction = 0.04;

    plot.title.text = RT("客戶從區域到方案再到續約結果");
}

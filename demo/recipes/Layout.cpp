// Layout.cpp — 子圖（Multiplot）、嵌入小圖（inset）、向量輸出。
//
// 這一章和別章不同：主角不是某一種圖，而是「一張畫布上怎麼擺好幾張圖」。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

QTPLOT_RECIPE_MULTI("版面", "子圖", "2×2 子圖",
                    "Multiplot 把畫布切成格子，每一格就是一張完整的 Plot —— "
                    "所有 add*、樣式、軸設定都和單張圖一模一樣。\n"
                    "alignAxes 預設開著：先量一遍每張子圖的刻度標籤要讓出多少，"
                    "再把同一欄（列）補到一樣寬。不補的話，Y 軸刻度位數不同的兩張圖，"
                    "繪圖區左緣會差上幾十個像素 —— 並排比較時那個錯位比資料的差異還顯眼。") {
    mp.resize(2, 2);
    mp.title.text = QStringLiteral("四種分布的樣子");
    mp.title.font.size = 14;
    mp.title.font.bold = true;

    gen::setSeed(12);
    struct Panel { QString name; QVector<double> data; QColor color; };
    const QVector<Panel> panels{
        {QStringLiteral("常態"), gen::randomNormal(400, 0, 1), QColor(45, 110, 185)},
        {QStringLiteral("均勻"), gen::randomUniform(400, -2, 2), QColor(200, 80, 50)},
        {QStringLiteral("右偏"), {}, QColor(90, 150, 80)},
        {QStringLiteral("雙峰"), {}, QColor(150, 100, 180)},
    };
    QVector<double> skewed;
    for (double x : gen::randomNormal(400, 0, 1)) skewed.append(std::exp(x * 0.6) - 1);
    QVector<double> bimodal = gen::randomNormal(200, -1.3, 0.45);
    bimodal += gen::randomNormal(200, 1.3, 0.45);

    for (int i = 0; i < 4; ++i) {
        Plot* p = mp.at(i / 2, i % 2);
        const QVector<double>& data = (i == 2) ? skewed : (i == 3) ? bimodal : panels[i].data;
        p->addHistogram(data, 28, panels[i].color);
        p->title.text = panels[i].name;
        p->title.font.size = 11;
        p->leftAxis()->label.text = QStringLiteral("次數");
    }
}

QTPLOT_RECIPE_MULTI("版面", "子圖", "共用座標軸",
                    "shareY 把一組子圖的 Y 範圍統一成聯集。\n"
                    "這是並排比較時最容易出錯的地方：各自 autoScale 的話，"
                    "兩張子圖的 Y 刻度不同，一樣高的長條代表的卻是不一樣的數值。\n"
                    "上排共用 Y、下排各自縮放，同一份資料畫兩次 —— "
                    "下排看起來三個廠的表現差不多，上排才看得出來丙廠只有甲廠的三分之一。") {
    mp.resize(2, 3);
    mp.title.text = QStringLiteral("同一份資料：共用 Y 軸（上）與各自縮放（下）");
    mp.title.font.size = 13;
    mp.title.font.bold = true;
    mp.rowWeights = {1.0, 1.0};

    const QStringList names{QStringLiteral("甲廠"), QStringLiteral("乙廠"),
                            QStringLiteral("丙廠")};
    const QVector<QVector<double>> output{{92, 88, 95, 90, 97, 94},
                                          {61, 58, 66, 63, 60, 64},
                                          {31, 28, 34, 30, 33, 29}};
    const QVector<QColor> colors{QColor(45, 110, 185), QColor(200, 130, 50),
                                 QColor(90, 150, 80)};

    QVector<Plot*> topRow;
    for (int c = 0; c < 3; ++c) {
        Plot* top = mp.at(0, c);
        top->addBars(output[c], colors[c]);
        top->title.text = names[c] + QStringLiteral("（共用 Y）");
        top->title.font.size = 10;
        topRow.append(top);

        Plot* bottom = mp.at(1, c);
        bottom->addBars(output[c], colors[c]);
        bottom->title.text = names[c] + QStringLiteral("（各自縮放）");
        bottom->title.font.size = 10;
    }
    mp.shareY(topRow);
}

QTPLOT_RECIPE_MULTI("版面", "子圖", "跨格與權重",
                    "span() 讓一格往右／往下吃掉幾格，rowWeights／columnWeights 調各列各欄的比例。\n"
                    "典型用法是「主圖大、附圖小」：上面一張寬的看趨勢，"
                    "下面兩張窄的看組成。\n"
                    "被跨格吃掉的格子如果已經有圖會被移除 —— "
                    "留著的話兩張圖會畫在同一塊畫布上互相覆蓋，而且是後畫的贏。") {
    mp.resize(2, 2);
    mp.rowWeights = {2.0, 1.0};
    mp.title.text = QStringLiteral("主圖大、附圖小");
    mp.title.font.size = 13;
    mp.title.font.bold = true;

    gen::setSeed(21);
    Plot* main = mp.span(0, 0, 1, 2);
    const QVector<double> xs = gen::consecutive(60);
    main->addScatter(xs, gen::randomWalk(60, 1.0, 100))->marker = MarkerStyle::none();
    main->title.text = QStringLiteral("月營收走勢");
    main->title.font.size = 11;
    main->leftAxis()->label.text = QStringLiteral("百萬元");

    Plot* left = mp.at(1, 0);
    left->addPie({42, 28, 18, 12},
                 {QStringLiteral("北"), QStringLiteral("中"), QStringLiteral("南"),
                  QStringLiteral("東")});
    left->title.text = QStringLiteral("區域佔比");
    left->title.font.size = 10;

    Plot* right = mp.at(1, 1);
    auto* bars = right->addBars({18, 24, 31, 27});
    for (Bar& b : bars->bars) b.label = QString::number(b.value, 'f', 0);
    right->title.text = QStringLiteral("各季毛利");
    right->title.font.size = 10;
}

QTPLOT_RECIPE_MULTI("版面", "子圖", "邊際分布（joint plot）",
                    "中間是散點，上方與右方各掛一張邊際分布的直方圖，"
                    "三者共用座標軸所以對得起來。\n"
                    "散點圖看得出相關性，但看不出每一軸自己的分布形狀 —— "
                    "一團點到底是常態還是雙峰，從散佈的濃淡很難判斷。邊際圖補的就是這個。\n"
                    "權重設成 1:4 讓邊際圖只佔一小條；shareX／shareY 保證三張圖的尺是同一把。") {
    mp.resize(2, 2);
    mp.rowWeights = {1.0, 4.0};
    mp.columnWeights = {4.0, 1.0};
    mp.spacingX = 3;
    mp.spacingY = 3;

    gen::setSeed(33);
    const int n = 500;
    QVector<double> xs = gen::randomNormal(n / 2, -1.0, 0.7);
    xs += gen::randomNormal(n / 2, 1.4, 0.5);
    QVector<double> ys;
    for (int i = 0; i < xs.size(); ++i)
        ys.append(xs[i] * 0.8 + gen::randomNormal(1, 0, 0.6).first());

    Plot* joint = mp.at(1, 0);
    auto* pts = joint->addMarkers(xs, ys, QColor(45, 110, 185, 110));
    pts->marker.size = 5;
    pts->marker.outline = LineStyle::none();
    joint->bottomAxis()->label.text = QStringLiteral("X");
    joint->leftAxis()->label.text = QStringLiteral("Y");

    Plot* topHist = mp.at(0, 0);
    topHist->addHistogram(xs, 36, QColor(45, 110, 185, 160));
    topHist->leftAxis()->isVisible = false;
    topHist->bottomAxis()->showTickLabels = false;
    topHist->grid.isVisible = false;

    Plot* rightHist = mp.at(1, 1);
    auto* rh = rightHist->addHistogram(ys, 36, QColor(45, 110, 185, 160));
    rh->horizontal = true;
    rightHist->bottomAxis()->isVisible = false;
    rightHist->leftAxis()->showTickLabels = false;
    rightHist->grid.isVisible = false;

    // 右上角那一格不放東西：留白比硬塞一個圖例好看
    mp.shareX({joint, topHist});
    mp.shareY({joint, rightHist});
}

QTPLOT_RECIPE("版面", "嵌入小圖", "放大局部",
              "InsetPlot 是一個圖元，內層就是一張完整的 Plot —— 什麼都能放。\n"
              "主圖看整體，角落那張看細節。預設會在主圖上框出放大的那一段並連兩條線過去："
              "不畫的話，讀者得自己比對座標才知道小圖看的是哪裡，"
              "而那正是最容易看錯的地方。\n"
              "位置預設用「佔繪圖區的比例」，所以縮放主圖時小圖不會跑掉；"
              "要讓它黏著某一段資料就改用 useDataCoordinates。") {
    gen::setSeed(77);
    const int n = 2000;
    QVector<double> xs = gen::linspace(0, 10, n);
    QVector<double> ys;
    for (int i = 0; i < n; ++i) {
        const double x = xs[i];
        // 慢變的基線加上一小段高頻突波
        double v = std::sin(x * 0.8) * 2.0;
        if (x > 6.2 && x < 6.6) v += 0.45 * std::sin(x * 180.0);
        ys.append(v + gen::randomNormal(1, 0, 0.02).first());
    }

    auto* main = plot.addScatter(xs, ys, QColor(45, 110, 185));
    main->marker = MarkerStyle::none();
    main->line.width = 1.0;
    plot.title.text = QStringLiteral("整體看不出來的那一小段");
    plot.bottomAxis()->label.text = QStringLiteral("時間（秒）");

    auto* inset = addInset(plot);
    inset->rectFraction = QRectF(0.08, 0.06, 0.40, 0.34);
    // 突波疊在慢變基線上，那一段的基線在 y ≈ −1.9 —— 範圍要框在那裡，
    // 框在 0 附近只會放大一塊空白。
    inset->sourceRegion = {6.15, 6.65, -2.5, -1.25};

    auto* zoom = inset->inner().addScatter(xs, ys, QColor(200, 70, 50));
    zoom->marker = MarkerStyle::none();
    zoom->line.width = 1.0;
    inset->inner().setAxisLimits(6.15, 6.65, -2.5, -1.25);
    inset->inner().grid.isVisible = false;
}

QTPLOT_RECIPE("版面", "嵌入小圖", "黏在資料上的小圖",
              "useDataCoordinates 改用資料座標定位：小圖的四個角由 dataRect 決定，"
              "所以縮放主圖時它會跟著資料一起跑、一起縮。\n"
              "想指著某一筆資料講話就用這個；想固定在角落當附圖就用預設的比例定位。") {
    gen::setSeed(5);
    const QVector<double> xs = gen::consecutive(36);
    const QVector<double> ys = gen::randomWalk(36, 1.0, 40);
    auto* line = plot.addScatter(xs, ys, QColor(60, 60, 60));
    line->marker.shape = MarkerShape::FilledCircle;
    line->marker.size = 5;

    auto* inset = addInset(plot);
    inset->useDataCoordinates = true;
    inset->dataRect = {4, 17, 44, 58};
    inset->showSourceRegion = false;

    inset->inner().addPie({38, 27, 21, 14});
    inset->inner().title.text = QStringLiteral("第 20 期的組成");
    inset->inner().title.font.size = 8;

    auto* arrow = plot.addArrow(xs[20], ys[20], 15.5, 46);
    arrow->line.color = QColor(120, 120, 120);

    plot.title.text = QStringLiteral("小圖跟著資料座標走");
    plot.setAxisLimits(-1, 36, 28, 60);
}

// Polar.cpp — 極座標族的範例：圓餅、甜甜圈、玫瑰圖、雷達圖、環形量表、極座標軸。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "圓餅圖",
              "扇形角度正比於數值。addPie 會自動把圖切成極座標模式："
              "等比例軸、不畫直角刻度與格線。") {
    plot.addPie({38, 27, 18, 12, 5},
                {QStringLiteral("台北"), QStringLiteral("台中"), QStringLiteral("高雄"),
                 QStringLiteral("台南"), QStringLiteral("其他")});
    plot.title.text = QStringLiteral("各地佔比");
}

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "標籤模式與分離扇形",
              "labelMode 可以印名稱、數值或百分比；labelDistance 大於 1 會把標籤畫到圓外。"
              "單一扇形設 explode 就會沿著自己的中線往外推。") {
    auto* pie = plot.addPie({45, 25, 20, 10},
                            {QStringLiteral("已完成"), QStringLiteral("進行中"),
                             QStringLiteral("待處理"), QStringLiteral("已取消")});
    pie->labelMode = Pie::LabelMode::LabelAndPercent;
    pie->labelDistance = 1.18;
    pie->slices[1].explode = 0.12;
    pie->labelFont.size = 10;
}

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "甜甜圈圖",
              "donutFraction 挖出中間的洞，centerLabel 可以在洞裡放總計之類的字。") {
    auto* pie = plot.addDonut({52, 23, 15, 10},
                              {QStringLiteral("行動"), QStringLiteral("桌機"),
                               QStringLiteral("平板"), QStringLiteral("其他")},
                              0.55);
    pie->labelMode = Pie::LabelMode::Percent;
    pie->labelDistance = 0.5;
    pie->centerLabel.text = QStringLiteral("12,480\n造訪");
    pie->centerLabel.font.size = 13;
    pie->centerLabel.font.bold = true;
    pie->legendText = QStringLiteral("x");  // 讓圖例收集到扇形
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignVCenter;
}

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "玫瑰圖（Nightingale）",
              "每塊扇形角度相同，改用半徑表示大小。areaProportional 預設為 true："
              "半徑取 sqrt(值)，所以「面積」才正比於數值 —— "
              "半徑直接正比會把大的值誇大成平方倍。") {
    const QStringList months{QStringLiteral("一月"), QStringLiteral("二月"), QStringLiteral("三月"),
                             QStringLiteral("四月"), QStringLiteral("五月"), QStringLiteral("六月"),
                             QStringLiteral("七月"), QStringLiteral("八月"), QStringLiteral("九月"),
                             QStringLiteral("十月"), QStringLiteral("十一月"), QStringLiteral("十二月")};
    plot.addCoxcomb({18, 22, 35, 48, 62, 78, 95, 90, 70, 50, 32, 20}, months);
    plot.title.text = QStringLiteral("月平均用電量");
}

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "半徑正比 vs 面積正比",
              "同一組資料畫兩次。左邊半徑直接正比於數值，大的值被誇大；"
              "右邊取平方根讓面積正比，才是正確的讀法。") {
    const QVector<double> v{10, 20, 40, 80};
    const QStringList lab{QStringLiteral("10"), QStringLiteral("20"),
                          QStringLiteral("40"), QStringLiteral("80")};
    auto* wrong = plot.addCoxcomb(v, lab);
    wrong->areaProportional = false;
    wrong->center = {-1.3, 0};

    auto* right = plot.addCoxcomb(v, lab);
    right->center = {1.3, 0};

    plot.addText(QStringLiteral("半徑正比（誇大）"), -1.3, -1.35)->alignment =
        Qt::AlignHCenter | Qt::AlignTop;
    plot.addText(QStringLiteral("面積正比（正確）"), 1.3, -1.35)->alignment =
        Qt::AlignHCenter | Qt::AlignTop;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "雷達圖",
              "每個維度一條放射軸，一組資料畫成一個多邊形。"
              "背景的網格是獨立的 PolarAxis 圖元，疊在資料下面。") {
    const QStringList dims{QStringLiteral("攻擊"), QStringLiteral("防禦"), QStringLiteral("速度"),
                           QStringLiteral("耐力"), QStringLiteral("技巧"), QStringLiteral("智力")};
    auto* grid = plot.addPolarAxis(1.0, dims.size());
    grid->spokeLabels = dims;
    grid->labelFont.color = QColor(0, 0, 0, 0);  // 標籤交給 Radar 畫，避免重複
    grid->ringCount = 4;
    grid->showRingLabels = false;

    auto* radar = plot.addRadar({{85, 60, 70, 55, 90, 45},
                                 {50, 90, 45, 85, 60, 75}},
                                dims);
    radar->maxValue = 100;
    radar->series[0].legendText = QStringLiteral("角色 A");
    radar->series[1].legendText = QStringLiteral("角色 B");
    radar->marker = MarkerStyle(MarkerShape::FilledCircle, 6, QColor());
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "各維度獨立正規化",
              "維度的單位不同時（分數、秒數、公斤…），用同一個上限正規化沒有意義。"
              "independentAxes 會讓每個維度各自以自己的最大值為滿格。") {
    const QStringList dims{QStringLiteral("營收(億)"), QStringLiteral("員工數"),
                           QStringLiteral("毛利率(%)"), QStringLiteral("客戶數"),
                           QStringLiteral("專利數")};
    auto* grid = plot.addPolarAxis(1.0, dims.size());
    grid->spokeLabels = dims;
    grid->labelFont.color = QColor(0, 0, 0, 0);
    grid->showRingLabels = false;

    auto* radar = plot.addRadar({{120, 3400, 42, 85000, 230},
                                 {95, 5100, 28, 62000, 410}},
                                dims);
    radar->independentAxes = true;
    radar->series[0].legendText = QStringLiteral("甲公司");
    radar->series[1].legendText = QStringLiteral("乙公司");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "環形量表",
              "每個值一個同心環，從十二點鐘開始順時針走。"
              "底下的淡色環是滿刻度，看得出還差多少。") {
    auto* g = plot.addRadialGauge({82, 64, 45, 91},
                                  {QStringLiteral("CPU"), QStringLiteral("記憶體"),
                                   QStringLiteral("磁碟"), QStringLiteral("網路")},
                                  100);
    g->valueMode = RadialGauge::ValueMode::Percent;
    g->labelFont.size = 10;
    plot.title.text = QStringLiteral("系統負載");
}

QTPLOT_RECIPE("極座標", "雷達與量表", "量表的底環",
              "showBackground 畫出每個環的滿刻度，一眼看得出還差多少。"
              "值小的時候名稱塞不進弧裡，會自動改畫到弧的外側並換回一般文字色。") {
    auto* g = plot.addRadialGauge({12, 8, 23, 5},
                                  {QStringLiteral("錯誤"), QStringLiteral("逾時"),
                                   QStringLiteral("重試"), QStringLiteral("丟棄")},
                                  100);
    g->valueMode = RadialGauge::ValueMode::Value;
    g->backgroundColor = QColor(0, 0, 0, 22);
    g->labelFont.size = 10;
    plot.title.text = QStringLiteral("每分鐘事件數（滿刻度 100）");
}

QTPLOT_RECIPE("極座標", "雷達與量表", "堆疊量表",
              "Stacked 模式把所有值畫在同一個環上依序接續，"
              "適合表示「一個總量怎麼被分掉」。") {
    auto* g = plot.addRadialGauge({35, 25, 20, 12},
                                  {QStringLiteral("運算"), QStringLiteral("儲存"),
                                   QStringLiteral("網路"), QStringLiteral("其他")},
                                  100);
    g->mode = RadialGauge::Mode::Stacked;
    g->innerRadius = 0.6;
    g->roundCaps = false;
    g->valueMode = RadialGauge::ValueMode::None;
    plot.addText(QStringLiteral("92%\n已使用"), 0, 0)->alignment =
        Qt::AlignHCenter | Qt::AlignVCenter;
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("極座標", "極座標軸", "極座標上畫資料",
              "PolarAxis 只畫格線，資料用 grid->coordinates(r, θ) 換成 (x, y) "
              "再丟給一般的 Scatter。用它而不是 fromPolar()：coordinates() 知道這根軸"
              "轉過多少、往哪邊轉，所以資料和刻度標籤永遠對得上。"
              "這是四葉玫瑰線 r = cos(2θ)。") {
    auto* grid = plot.addPolarAxis(1.0, 12);
    grid->ringCount = 4;
    grid->startAngle = 0;
    grid->clockwise = false;

    QVector<double> xs, ys;
    for (int i = 0; i <= 720; ++i) {
        const double deg = i * 0.5;
        const double r = std::abs(std::cos(2 * deg * kPi / 180.0));
        const Coordinates c = grid->coordinates(r, deg);
        xs.append(c.x);
        ys.append(c.y);
    }
    auto* s = plot.addScatter(xs, ys);
    s->marker = MarkerStyle::none();
    s->line.width = 2;
    plot.title.text = QStringLiteral("r = |cos(2θ)|");
}

// ───────────────────── 極座標軸的細節 ─────────────────────

namespace {

/// 一條由內往外繞出去的螺線，上色由藍到紅。幾支範例共用。
void addSpiral(Plot& plot, PolarAxis& grid, int points = 28) {
    const Colormap cmap(Colormap::Turbo);
    for (int i = 0; i < points; ++i) {
        const double frac = static_cast<double>(i) / (points - 1);
        const Coordinates c = grid.coordinates(frac, -360.0 * frac);
        auto* m = plot.addMarkers({c.x}, {c.y});
        m->marker = MarkerStyle(MarkerShape::FilledCircle, 5 + 9 * (1 - frac),
                                cmap.lookup(frac));
    }
}

}  // namespace

QTPLOT_RECIPE("極座標", "極座標軸", "極座標軸",
              "先加一根 PolarAxis，再用它的 coordinates(r, θ) 把資料換成 (x, y)，"
              "任何既有的圖元（標記、線、散點）都可以疊上去。"
              "coordinates() 收的角度就是放射線標籤印出來的那個角度。") {
    auto* grid = plot.addPolarAxis(1.0, 12);
    grid->startAngle = 0;
    grid->clockwise = false;
    grid->showRingLabels = false;
    addSpiral(plot, *grid);
}

QTPLOT_RECIPE("極座標", "極座標軸", "旋轉極座標軸",
              "startAngle 決定「0 度」指向哪裡。因為資料也是經由 coordinates() 擺上去的，"
              "改 startAngle 會讓刻度和資料一起轉 —— 不會只轉了格線、資料留在原地。") {
    auto* grid = plot.addPolarAxis(1.0, 12);
    grid->startAngle = 90;   // 0 度轉到正上方
    grid->clockwise = false;
    grid->showRingLabels = false;
    addSpiral(plot, *grid);
}

QTPLOT_RECIPE("極座標", "極座標軸", "極座標上的箭頭",
              "箭頭的根部放在圓心、尖端用 coordinates() 指到極座標上的某一點，"
              "就能表示「大小加方向」。相量圖（Phasor）用的就是這個做法。") {
    auto* grid = plot.addPolarAxis(1.0, 12);
    grid->startAngle = 0;
    grid->clockwise = false;
    grid->showRingLabels = false;
    grid->ringLine.pattern = LinePattern::Dashed;
    grid->spokeLine.pattern = LinePattern::Dashed;

    struct Vec { double r, deg; QColor color; };
    const QVector<Vec> arrows{{0.75, 15, QColor(114, 158, 206)},
                              {0.90, 115, QColor(255, 140, 50)},
                              {0.85, 250, QColor(70, 170, 70)}};
    for (const Vec& v : arrows) {
        const Coordinates tip = grid->coordinates(v.r, v.deg);
        // addArrow 的參數順序是「先尖端、後根部」
        auto* a = plot.addArrow(tip.x, tip.y, grid->center.x, grid->center.y);
        a->line = LineStyle(v.color, 9.0);
        a->headLength = 20;
        a->headWidth = 20;
    }
}

QTPLOT_RECIPE("極座標", "極座標軸", "放射線逐條設定樣式",
              "spokes 可以逐條指定角度、長度、線條樣式與標籤顏色。"
              "非空時它會完全取代 spokeCount 與 spokeLabels。") {
    auto* grid = plot.addPolarAxis(1.0, 12);
    grid->startAngle = 0;
    grid->clockwise = false;
    grid->showRingLabels = false;
    grid->ringLine = LineStyle(QColor(120, 170, 150, 160), 1.0, LinePattern::Dashed);

    const Palette palette(Palette::Category10);
    for (int i = 0; i < 12; ++i) {
        PolarSpoke sp;
        sp.angle = i * 30.0;
        sp.line = LineStyle(palette.color(i), 2.0);
        sp.labelColor = palette.color(i);
        grid->spokes.append(sp);
    }
    grid->labelFont.bold = true;
}

QTPLOT_RECIPE("極座標", "極座標軸", "極座標的底色",
              "background 填在格線之下。它也吃 FillStyle 的漸層，"
              "所以要做成由內而外變淡之類的效果也可以。") {
    auto* grid = plot.addPolarAxis(1.0, 12);
    grid->startAngle = 0;
    grid->clockwise = false;
    grid->showRingLabels = false;
    grid->background = FillStyle(QColor(185, 185, 245, 170));
    grid->ringLine.color = QColor(60, 60, 110, 150);
    grid->spokeLine.color = QColor(60, 60, 110, 150);
}

QTPLOT_RECIPE("極座標", "極座標軸", "放射線的長度",
              "spokeLengthFraction 以半徑的比例指定長度，逐條設定時由 PolarSpoke 覆蓋。"
              "注意標籤永遠貼在最外圈，不跟著縮進去 —— 跟著縮的話一圈文字會參差不齊。") {
    auto* grid = plot.addPolarAxis(1.0, 12);
    grid->startAngle = 0;
    grid->clockwise = false;
    grid->showRingLabels = false;
    grid->ringCount = 5;
    grid->spokeLengthFraction = 0.78;
}

QTPLOT_RECIPE("極座標", "極座標軸", "放射線的標籤",
              "spokeLabels 逐條指定文字，數量也決定放射線有幾條。"
              "留空的那幾條會退回去印角度。") {
    auto* grid = plot.addPolarAxis(1.0, 5);
    grid->startAngle = 0;
    grid->clockwise = false;
    grid->showRingLabels = false;
    grid->ringCount = 4;
    grid->spokeLabels = {QStringLiteral("alpha"), QStringLiteral("beta"),
                         QStringLiteral("gamma"), QStringLiteral("delta"),
                         QStringLiteral("epsilon")};
}

// ───────────────────── 立體與引線標籤 ─────────────────────

QTPLOT_RECIPE("極座標", "圓餅進階", "立體圓餅圖",
              "tiltFraction 把圓壓扁（模擬從斜上方看），depthPx 擠出厚度。"
              "側壁只畫前半圈，而且整體依「前後」重排繪製順序，"
              "後面的扇形才不會蓋住前面的側壁。\n"
              "要先知道：立體圓餅是有名的壞圖表。壓扁之後，同樣角度的扇形在前面"
              "看起來比後面大，讀者沒辦法用面積比較數值。要精確比較請用平的圓餅"
              "或長條圖；這個選項是給簡報版型用的。") {
    auto* pie = plot.addPie({14.9, 27.9, 6.7, 16.6, 12.7, 8.5, 7.9, 4.8},
                            {QStringLiteral("Russia"), QStringLiteral("Canada"),
                             QStringLiteral("Brazil"), QStringLiteral("Australia"),
                             QStringLiteral("India"), QStringLiteral("China"),
                             QStringLiteral("America"), QStringLiteral("Japan")});
    pie->tiltFraction = 0.62;
    pie->depthPx = 26;
    pie->labelMode = Pie::LabelMode::None;
    pie->sliceBorder = LineStyle(QColor(255, 255, 255), 2.0);
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "圓餅進階", "爆炸式立體圓餅",
              "每塊的 explode 把扇形沿著自己的中線往外推。"
              "推的方向也要跟著壓扁 —— 不然前後兩塊推出去的距離看起來會不一樣。"
              "推開之後看得到扇形被切開的那兩個側面。") {
    const QStringList names{QStringLiteral("Russia"),  QStringLiteral("Canada"),
                            QStringLiteral("Brazil"),  QStringLiteral("Australia"),
                            QStringLiteral("India"),   QStringLiteral("China"),
                            QStringLiteral("America"), QStringLiteral("Japan")};
    auto* pie = plot.addPie({14.9, 27.9, 6.7, 16.6, 12.7, 8.5, 7.9, 4.8}, names);
    pie->tiltFraction = 0.6;
    pie->depthPx = 24;
    for (PieSlice& s : pie->slices) s.explode = 0.12;
    pie->labelMode = Pie::LabelMode::LabelAndPercent;
    pie->useLeaderLines = true;
    pie->leaderLengthFraction = 0.3;
    pie->labelFont.bold = true;
    pie->sliceBorder = LineStyle(QColor(255, 255, 255), 2.0);

    plot.title.text = QStringLiteral("Poor Choice");
    plot.title.font.color = QColor(40, 40, 200);
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "圓餅進階", "引線標籤（很多塊）",
              "扇形一多，貼在扇形上的標籤會疊成一團。useLeaderLines 把標籤拉到圓外，"
              "左右各排一列，再把同一側上下推開。\n"
              "推開的演算法要掃兩趟：只從上往下推的話，被推到最下面的那幾個會擠出"
              "繪圖區；掃回來才會把整串往回壓。") {
    gen::setSeed(5);
    const QStringList names{
        QStringLiteral("California"), QStringLiteral("Texas"),     QStringLiteral("New York"),
        QStringLiteral("Florida"),    QStringLiteral("Illinois"),  QStringLiteral("Pennsylvania"),
        QStringLiteral("Ohio"),       QStringLiteral("Michigan"),  QStringLiteral("Georgia"),
        QStringLiteral("N. Carolina"), QStringLiteral("New Jersey"), QStringLiteral("Virginia"),
        QStringLiteral("Washington"), QStringLiteral("Arizona"),   QStringLiteral("Massachusetts"),
        QStringLiteral("Indiana"),    QStringLiteral("Tennessee"), QStringLiteral("Missouri"),
        QStringLiteral("Maryland"),   QStringLiteral("Wisconsin")};
    const QVector<double> values{37, 24, 19, 18, 13, 12, 12, 10, 9.6, 9.5,
                                 8.7, 7.8, 6.5, 6.4, 6.5, 6.4, 6.2, 5.9, 5.7, 5.6};

    auto* pie = plot.addPie(values, names);
    pie->labelMode = Pie::LabelMode::LabelAndPercent;
    pie->useLeaderLines = true;
    pie->leaderLengthFraction = 0.4;
    pie->labelFont.size = 9;
    pie->sliceBorder = LineStyle(QColor(255, 255, 255), 1.0);
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "圓餅進階", "深色底的引線圓餅",
              "引線與文字的顏色跟著主題走。深色底上引線要夠亮才看得到 —— "
              "預設的半透明黑在深色背景上等於沒畫。") {
    plot.setStyle(PlotStyle::dark());
    const QStringList names{QStringLiteral("Evraz"),          QStringLiteral("AGNC"),
                            QStringLiteral("Gladstone Inv."), QStringLiteral("Gladstone Com."),
                            QStringLiteral("Pembina"),        QStringLiteral("Std Life"),
                            QStringLiteral("Main Street"),    QStringLiteral("British Tobacco"),
                            QStringLiteral("AT&T"),           QStringLiteral("Legal & General"),
                            QStringLiteral("Vodafone"),       QStringLiteral("BP Plc"),
                            QStringLiteral("Phoenix"),        QStringLiteral("IBM"),
                            QStringLiteral("PayPoint"),       QStringLiteral("Shaw Comm."),
                            QStringLiteral("Realty Income")};
    const QVector<double> values{9.7, 7.7, 7.0, 6.7, 6.5, 6.5, 6.4, 6.1, 5.9,
                                 5.5, 5.2, 5.2, 4.9, 4.5, 4.4, 4.1, 3.7};

    auto* pie = plot.addPie(values, names);
    pie->labelMode = Pie::LabelMode::LabelAndPercent;
    pie->useLeaderLines = true;
    pie->leaderLengthFraction = 0.35;
    pie->labelFont.size = 9;
    pie->leaderLine = LineStyle(QColor(200, 200, 200, 180), 1.0);
    pie->sliceBorder = LineStyle(QColor(30, 30, 30), 1.0);
    plot.equalAxisScale = true;
}

// ───────────────────── 旭日圖 ─────────────────────

QTPLOT_RECIPE("極座標", "旭日圖", "旭日圖",
              "一層一個環：最內圈是最上層的分類，往外每一圈是它的下一層。"
              "每個扇形的角度正比於它底下所有葉節點的總和，"
              "所以同一圈的角度加起來剛好是整圈。\n"
              "父節點不必自己填數字 —— 填了也會被忽略，改用子節點的總和。"
              "讓父節點自己填一個對不上子節點的值，整圈的角度就會兜不攏。") {
    QVector<SunburstNode> roots{
        {QStringLiteral("North"), 0, QColor(), {
            {QStringLiteral("Food"), 0, QColor(), {
                {QStringLiteral("Pizza"), 42, QColor(), {}},
                {QStringLiteral("Burger"), 28, QColor(), {}}}},
            {QStringLiteral("Beverages"), 0, QColor(), {
                {QStringLiteral("Coffee"), 18, QColor(), {}},
                {QStringLiteral("Juice"), 12, QColor(), {}}}}}},
        {QStringLiteral("South"), 0, QColor(), {
            {QStringLiteral("Food"), 0, QColor(), {
                {QStringLiteral("Pizza"), 36, QColor(), {}},
                {QStringLiteral("Sandwich"), 22, QColor(), {}}}},
            {QStringLiteral("Beverages"), 0, QColor(), {
                {QStringLiteral("Tea"), 14, QColor(), {}}}}}},
        {QStringLiteral("East"), 0, QColor(), {
            {QStringLiteral("Food"), 0, QColor(), {
                {QStringLiteral("Pasta"), 20, QColor(), {}}}}}},
    };
    auto* sb = plot.addSunburst(roots);
    sb->labelFont.bold = true;
    plot.title.text = QStringLiteral("SUN BURST CHART");
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "旭日圖", "旭日圖的配色與層級",
              "最內圈取調色盤，往外每層在父節點的顏色上調亮 —— "
              "同一個分支就看得出是一家人。lightenPerLevel 控制每層亮多少。"
              "弧太窄的扇形會自動不印標籤：印了也只會疊成一團。") {
    const QStringList teams{QStringLiteral("Mana."), QStringLiteral("Acco."),
                            QStringLiteral("Tech."), QStringLiteral("Mark."),
                            QStringLiteral("Sales")};
    const QStringList roles{QStringLiteral("Devel."), QStringLiteral("Teste."),
                            QStringLiteral("Wind."), QStringLiteral("Web")};

    gen::setSeed(11);
    QVector<SunburstNode> roots;
    const QStringList countries{QStringLiteral("USA"), QStringLiteral("India"),
                                QStringLiteral("Germany"), QStringLiteral("UK")};
    for (int c = 0; c < countries.size(); ++c) {
        SunburstNode country;
        country.label = countries[c];
        for (int tIdx = 0; tIdx < teams.size(); ++tIdx) {
            SunburstNode team;
            team.label = teams[tIdx];
            // 只有技術部門再往下分角色，其他部門就是葉節點 ——
            // 旭日圖不要求每一支都一樣深
            if (teams[tIdx] == QStringLiteral("Tech.")) {
                for (const QString& r : roles) {
                    SunburstNode role;
                    role.label = r;
                    role.value = gen::randomUniform(1, 6, 20).first();
                    team.children.append(role);
                }
            } else {
                team.value = gen::randomUniform(1, 4, 14).first();
            }
            country.children.append(team);
        }
        roots.append(country);
    }

    auto* sb = plot.addSunburst(roots);
    sb->innerFraction = 0.18;
    sb->lightenPerLevel = 26;
    sb->labelFont.size = 8;
    sb->labelFont.color = QColor(40, 40, 40);
    // 圖例只列最內圈。每一層都列的話，圖例會比圖還大。
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
    plot.equalAxisScale = true;
}

// ───────────────────── 玫瑰圖的變化 ─────────────────────

QTPLOT_RECIPE("極座標", "玫瑰圖", "極座標長條圖",
              "sectorWidthFraction 讓每一格只畫一部分，兩側留出空隙，"
              "看起來就像從圓心長出來的長條。配一根 PolarAxis 當背景，"
              "半徑才讀得出刻度 —— 沒有刻度的話，讀者只能比長短，說不出數值。") {
    const QStringList names{QStringLiteral("Product A"), QStringLiteral("Product B"),
                            QStringLiteral("Product C"), QStringLiteral("Product D"),
                            QStringLiteral("Product E")};
    const QVector<double> values{105, 62, 148, 45, 132};

    auto* grid = plot.addPolarAxis(1.0, names.size());
    grid->spokeLabels = names;
    grid->labelFont.color = QColor(0, 0, 0, 0);  // 標籤交給玫瑰圖畫，避免重複
    grid->ringCount = 5;
    grid->ringRadii = {0.2, 0.4, 0.6, 0.8, 1.0};
    // 半徑是正規化過的 0–1，但要標的是銷售額，所以用 ringLabels 自己給文字 ——
    // 印半徑本身會變成 0.2、0.4，對讀者沒有意義
    grid->ringLabels = {QStringLiteral("30"), QStringLiteral("60"), QStringLiteral("90"),
                        QStringLiteral("120"), QStringLiteral("150")};
    grid->showRingLabels = true;
    grid->spokeLine = LineStyle(QColor(0, 0, 0, 0), 1.0);

    QVector<PieSlice> slices;
    const QVector<QColor> colors{QColor(240, 80, 100), QColor(60, 140, 225),
                                 QColor(200, 70, 240), QColor(250, 200, 70),
                                 QColor(60, 190, 180)};
    for (int i = 0; i < names.size(); ++i)
        slices.append({values[i], names[i], colors[i], 0, QString()});

    auto* rose = plot.addCoxcomb(slices);
    rose->sectorWidthFraction = 0.45;
    rose->areaProportional = false;  // 要能直接讀半徑對應的刻度，就不能開根號
    rose->sliceBorder = LineStyle::none();

    plot.title.text = QStringLiteral("Monthly Sales Distribution");
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "玫瑰圖", "甜甜圈玫瑰圖",
              "donutFraction 在中間挖一個洞。半徑從洞的邊緣起算，"
              "不然小的值會整個藏進洞裡、變成看不見。") {
    const QStringList names{
        QStringLiteral("MS Zaandam"), QStringLiteral("Burundi"),  QStringLiteral("Bahamas"),
        QStringLiteral("Sudan"),      QStringLiteral("Algeria"),  QStringLiteral("Belgium"),
        QStringLiteral("Mauritania"), QStringLiteral("Sint Maarten"), QStringLiteral("UK"),
        QStringLiteral("Italy"),      QStringLiteral("Antigua"),  QStringLiteral("France"),
        QStringLiteral("Zimbabwe"),   QStringLiteral("Malawi"),   QStringLiteral("Netherlands")};
    const QVector<double> values{22.2, 20.0, 16.7, 15.2, 15.1, 14.7, 14.3, 14.1,
                                 13.5, 13.2, 13.0, 12.6, 12.5, 11.8, 11.4};

    QVector<PieSlice> slices;
    const Colormap cmap(Colormap::Turbo);
    for (int i = 0; i < names.size(); ++i) {
        const double f = static_cast<double>(i) / (names.size() - 1);
        slices.append({values[i], QStringLiteral("%1 %2%").arg(names[i]).arg(values[i]),
                       cmap.lookup(1.0 - f), 0, QString()});
    }

    auto* rose = plot.addCoxcomb(slices);
    rose->donutFraction = 0.28;
    rose->areaProportional = false;
    rose->sliceBorder = LineStyle(QColor(255, 255, 255), 1.5);
    rose->labelFont.size = 8;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "玫瑰圖", "堆疊玫瑰圖",
              "stacks 讓每一格由內往外疊好幾段。\n"
              "堆疊時一定要把 areaProportional 關掉：面積正比是對「累計值」開根號，"
              "開著的話各段的厚度就不再正比於自己的值，逐段比較會得到錯的結論。") {
    gen::setSeed(42);
    const int spokes = 24;
    const int layers = 6;
    const QStringList layerNames{QStringLiteral("資金揭露"), QStringLiteral("利益衝突"),
                                 QStringLiteral("跨領域團隊"), QStringLiteral("文獻檢索"),
                                 QStringLiteral("證據分級"), QStringLiteral("外部審查")};
    const Colormap cmap(Colormap::Viridis);

    QVector<PieSlice> slices;
    QVector<QVector<double>> stacks;
    for (int i = 0; i < spokes; ++i) {
        slices.append({1, QString(), QColor(), 0, QString()});
        QVector<double> parts;
        for (int k = 0; k < layers; ++k)
            parts.append(gen::randomUniform(1, 0.3, 1.0).first());
        stacks.append(parts);
    }

    auto* rose = plot.addCoxcomb(slices);
    rose->stacks = stacks;
    rose->stackLabels = layerNames;
    for (int k = 0; k < layers; ++k)
        rose->stackColors.append(cmap.lookup(static_cast<double>(k) / (layers - 1)));
    rose->areaProportional = false;
    rose->sectorWidthFraction = 0.82;
    rose->donutFraction = 0.06;
    rose->sliceBorder = LineStyle(QColor(255, 255, 255), 1.0);
    rose->showLabels = false;

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignTop;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "玫瑰圖", "南丁格爾圖",
              "Florence Nightingale 1858 年的原圖：每個月一格，面積代表死亡人數，"
              "由內而外疊三類死因。她用這張圖讓國會相信「死於可預防疾病的人"
              "遠多於死於戰傷」。\n"
              "原圖的半徑取平方根（面積正比），所以這裡 areaProportional 開著 —— "
              "代價是各段的厚度不能逐段比較，只能看總量。") {
    const QStringList months{
        QStringLiteral("四月"), QStringLiteral("五月"), QStringLiteral("六月"),
        QStringLiteral("七月"), QStringLiteral("八月"), QStringLiteral("九月"),
        QStringLiteral("十月"), QStringLiteral("十一月"), QStringLiteral("十二月"),
        QStringLiteral("一月"), QStringLiteral("二月"), QStringLiteral("三月")};
    // 1854-04 到 1855-03：可預防疾病／戰傷／其他（每千人月死亡率）
    const QVector<double> zymotic{1.0, 1.2, 1.1, 1.5, 1.8, 11.0, 23.0, 32.0,
                                  48.0, 57.0, 60.0, 17.0};
    const QVector<double> wounds{0.0, 0.0, 0.0, 0.0, 0.0, 0.2, 1.0, 3.0,
                                 4.5, 5.5, 6.5, 3.0};
    const QVector<double> other{5.0, 4.0, 2.0, 2.5, 2.0, 2.5, 5.0, 7.0,
                                9.5, 10.0, 11.0, 5.0};

    QVector<PieSlice> slices;
    QVector<QVector<double>> stacks;
    for (int i = 0; i < months.size(); ++i) {
        slices.append({1, months[i], QColor(), 0, QString()});
        stacks.append({zymotic[i], wounds[i], other[i]});
    }

    auto* rose = plot.addCoxcomb(slices);
    rose->stacks = stacks;
    rose->stackColors = {QColor(120, 190, 235), QColor(210, 40, 40), QColor(30, 30, 30)};
    rose->stackLabels = {QStringLiteral("可預防疾病"), QStringLiteral("戰傷"),
                         QStringLiteral("其他")};
    rose->areaProportional = true;
    rose->clockwise = true;
    rose->sliceBorder = LineStyle(QColor(255, 255, 255), 1.0);
    rose->labelFont.size = 8;

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignBottom;
    plot.title.text = QStringLiteral("1854-55 年克里米亞戰爭英軍死因");
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "玫瑰圖", "風花圖",
              "氣象用的標準圖：每個方位一格，半徑是該方位的出現頻率，"
              "由內而外依風速分層堆疊。中間標出「無風」的比例 —— "
              "無風沒有方向，放不進任何一格，不標出來的話各格加起來不等於 100%。\n"
              "方位用兩圈標籤：外圈是度數、內圈是 N/NE/E… 的方位名。") {
    gen::setSeed(8);
    const int dirs = 16;
    const QStringList bins{QStringLiteral("2.0-4.9"),   QStringLiteral("5.0-6.9"),
                           QStringLiteral("7.0-9.9"),   QStringLiteral("10.0-14.9"),
                           QStringLiteral("15.0-19.9"), QStringLiteral("20.0+")};
    const QVector<QColor> binColors{QColor(20, 20, 220),  QColor(70, 200, 200),
                                    QColor(165, 220, 170), QColor(255, 205, 50),
                                    QColor(250, 150, 50),  QColor(205, 50, 50)};
    const QStringList compass{
        QStringLiteral("N"),  QStringLiteral("NNE"), QStringLiteral("NE"), QStringLiteral("ENE"),
        QStringLiteral("E"),  QStringLiteral("ESE"), QStringLiteral("SE"), QStringLiteral("SSE"),
        QStringLiteral("S"),  QStringLiteral("SSW"), QStringLiteral("SW"), QStringLiteral("WSW"),
        QStringLiteral("W"),  QStringLiteral("WNW"), QStringLiteral("NW"), QStringLiteral("NNW")};

    // 背景只放同心圓。半徑刻度要畫在資料「上面」才看得見，所以另外拆一根軸，
    // 排在玫瑰圖後面加入 —— 圖元是依加入順序疊的。
    auto* rings = plot.addPolarAxis(1.0, 16);
    rings->startAngle = 90;      // 0° 指向正北（上）
    rings->clockwise = true;     // 羅盤是順時針的
    rings->spokeLine = LineStyle::none();
    rings->ringLine = LineStyle(QColor(0, 0, 0, 45), 1.0);
    rings->ringRadii = {0.25, 0.5, 0.75, 1.0};
    rings->showRingLabels = false;
    rings->labelFont.color = QColor(0, 0, 0, 0);

    // 資料：每個方位一格，六個風速層
    QVector<PieSlice> slices;
    QVector<QVector<double>> stacks;
    for (int d = 0; d < dirs; ++d) {
        slices.append({1, QString(), QColor(), 0, QString()});
        // 西風偏多：繞一圈的基礎頻率用餘弦調變。加一個底值，
        // 否則東邊那幾格會縮到幾乎看不見，圖會整個偏掉。
        const double bias = 1.0 + 0.55 * std::cos((d - 12) * 2 * kPi / dirs);
        QVector<double> parts;
        for (int k = 0; k < bins.size(); ++k) {
            // 風速越高越少見
            const double scale = 1.0 / (1.0 + k * 0.9);
            parts.append(bias * scale * gen::randomUniform(1, 0.5, 1.2).first());
        }
        stacks.append(parts);
    }

    auto* rose = plot.addCoxcomb(slices);
    rose->stacks = stacks;
    rose->stackColors = binColors;
    rose->stackLabels = bins;
    rose->startAngle = 90;
    rose->clockwise = true;
    rose->areaProportional = false;   // 半徑要能對上刻度，不能開根號
    rose->sectorWidthFraction = 0.72;
    rose->sliceBorder = LineStyle(QColor(255, 255, 255), 0.8);
    rose->showLabels = false;
    rose->showValueLabels = true;
    rose->valueLabelSuffix = QStringLiteral("%");
    rose->valueLabelFont.size = 7;
    rose->centerLabel.text = QStringLiteral("無風\n10.99%");
    rose->centerLabel.font.bold = true;
    rose->centerLabel.font.size = 11;
    rose->donutFraction = 0.13;

    // 半徑刻度：疊在資料上面
    auto* scale = plot.addPolarAxis(1.0, 16);
    scale->startAngle = 90;
    scale->clockwise = true;
    scale->spokeLine = LineStyle::none();
    scale->ringLine = LineStyle::none();
    scale->labelFont.color = QColor(0, 0, 0, 0);
    scale->ringRadii = {0.25, 0.5, 0.75, 1.0};
    scale->ringLabels = {QStringLiteral("1.5%"), QStringLiteral("3.0%"),
                         QStringLiteral("4.5%"), QStringLiteral("6.0%")};
    scale->ringLabelAngle = 11.25;   // 擺在 N 和 NNE 的縫隙上，不壓到扇形
    scale->ringLabelFont.size = 8;
    scale->ringLabelFont.bold = true;

    // 兩圈方位標籤：方位名貼著圓周，度數再往外一圈。
    // 兩者的 labelPaddingPx 要拉開夠遠，不然長的名稱（WNW）會和度數疊在一起。
    auto* names = plot.addPolarAxis(1.0, 16);
    names->startAngle = 90;
    names->clockwise = true;
    names->spokeLabels = compass;
    names->spokeLine = LineStyle::none();
    names->ringLine = LineStyle::none();
    names->showRingLabels = false;
    names->labelPaddingPx = 10;
    names->labelFont.bold = true;

    auto* degrees = plot.addPolarAxis(1.0, 16);
    degrees->startAngle = 90;
    degrees->clockwise = true;
    degrees->spokeLine = LineStyle::none();
    degrees->ringLine = LineStyle::none();
    degrees->showRingLabels = false;
    degrees->labelPaddingPx = 42;
    degrees->labelFont.size = 8;

    plot.title.text = QStringLiteral("風花圖：某地三月 1973–2022");
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    // 擺左上角：下方被 S / SSE / SSW 那幾個方位標籤佔著了
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "圓餅進階", "每塊半徑不同",
              "PieSlice::radiusFraction 讓某幾塊特別突出。\n"
              "要注意這樣一來「角度」和「半徑」各自代表一個維度，"
              "讀者很容易把面積當成唯一的量 —— 當作版面效果就好，"
              "需要精確比較時請用一般的圓餅或長條圖。") {
    auto* pie = plot.addPie({30, 20, 25, 10, 15},
                            {QStringLiteral("30%"), QStringLiteral("20%"),
                             QStringLiteral("25%"), QStringLiteral("10%"),
                             QStringLiteral("15%")});
    pie->donutFraction = 0.22;
    pie->startAngle = 90;
    pie->clockwise = true;
    pie->labelMode = Pie::LabelMode::Label;
    pie->labelDistance = 0.7;
    pie->labelFont.color = Qt::white;
    pie->labelFont.bold = true;
    pie->labelFont.size = 14;
    pie->sliceBorder = LineStyle(QColor(255, 255, 255), 2.0);

    const QVector<QColor> colors{QColor(30, 150, 160), QColor(150, 190, 50),
                                 QColor(225, 85, 135), QColor(225, 85, 135),
                                 QColor(235, 155, 55)};
    const QVector<double> radii{1.0, 0.82, 0.78, 0.6, 1.0};
    for (int i = 0; i < pie->slices.size(); ++i) {
        pie->slices[i].fillColor = colors[i];
        pie->slices[i].radiusFraction = radii[i];
    }
    // 把最後一塊推出去
    pie->slices.last().explode = 0.12;

    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "雙尺度雷達圖",
              "兩組資料的單位完全不同（人數 vs 百分比），"
              "independentAxes 讓每個維度各自以自己的最大值為滿格。\n"
              "要小心這種圖很容易誤導：兩個多邊形疊在一起，讀者會直覺比較"
              "「誰的面積大」，但它們根本不在同一個尺度上。"
              "一定要把兩套刻度都標出來，而且說明各自代表什麼。") {
    const QStringList subjects{QStringLiteral("物理"), QStringLiteral("化學"),
                               QStringLiteral("數學"), QStringLiteral("生物"),
                               QStringLiteral("英文")};
    const QVector<double> enrolment{56, 48, 40, 52, 66};   // 人數
    const QVector<double> passRate{96, 98, 72, 95, 70};    // 及格率 %

    auto* grid = plot.addPolarAxis(1.0, subjects.size());
    grid->spokeLabels = subjects;
    grid->labelFont.color = QColor(0, 0, 0, 0);  // 名稱交給 Radar 畫
    grid->ringCount = 4;
    grid->showRingLabels = false;

    auto* radar = plot.addRadar({enrolment, passRate}, subjects);
    radar->independentAxes = true;
    radar->series[0].lineColor = QColor(60, 90, 220);
    radar->series[0].legendText = QStringLiteral("修課人數");
    radar->series[1].lineColor = QColor(60, 160, 80);
    radar->series[1].legendText = QStringLiteral("及格率 (%)");

    // 兩套刻度各標一邊：人數沿著正上方，百分比沿著右下
    auto* left = plot.addPolarAxis(1.0, subjects.size());
    left->spokeLine = LineStyle::none();
    left->ringLine = LineStyle::none();
    left->labelFont.color = QColor(0, 0, 0, 0);
    left->ringRadii = {0.5, 1.0};
    left->ringLabels = {QStringLiteral("28"), QStringLiteral("56")};
    left->ringLabelAngle = 0;
    left->ringLabelFont.color = QColor(60, 90, 220);

    auto* right = plot.addPolarAxis(1.0, subjects.size());
    right->spokeLine = LineStyle::none();
    right->ringLine = LineStyle::none();
    right->labelFont.color = QColor(0, 0, 0, 0);
    right->ringRadii = {0.5, 1.0};
    right->ringLabels = {QStringLiteral("50%"), QStringLiteral("100%")};
    right->ringLabelAngle = 288;   // 落在化學與數學之間的縫隙
    right->ringLabelFont.color = QColor(60, 160, 80);

    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
    plot.title.text = QStringLiteral("學生表現分析");
    plot.equalAxisScale = true;
}

// ───────────────────── 環狀圖的文字排版 ─────────────────────

QTPLOT_RECIPE("極座標", "旭日圖", "風味輪（徑向文字）",
              "多層旭日圖的外圈用 Radial：文字沿著半徑、由內往外讀。"
              "咖啡、香水、葡萄酒的風味輪都是這個排法 —— 外圈的項目名很長，"
              "沿著弧排的話根本塞不下。\n"
              "判斷「放不放得下」要看文字排的方向：沿弧排就比弧長、沿半徑排就比環的厚度。"
              "用錯方向的話，外圈的長標籤會被誤判成放得下，然後整段溢出去壓到隔壁。") {
    QVector<SunburstNode> roots{
        {QStringLiteral("酸味"), 0, QColor(120, 170, 70), {
            {QStringLiteral("花香"), 0, QColor(), {
                {QStringLiteral("茶玫瑰"), 1, QColor(), {}},
                {QStringLiteral("咖啡花"), 1, QColor(), {}}}},
            {QStringLiteral("果香"), 0, QColor(), {
                {QStringLiteral("檸檬"), 1, QColor(), {}},
                {QStringLiteral("蘋果"), 1, QColor(), {}},
                {QStringLiteral("莓果"), 1, QColor(), {}}}},
            {QStringLiteral("草本"), 0, QColor(), {
                {QStringLiteral("洋蔥"), 1, QColor(), {}},
                {QStringLiteral("豌豆"), 1, QColor(), {}}}}}},
        {QStringLiteral("焦糖"), 0, QColor(225, 150, 50), {
            {QStringLiteral("堅果"), 0, QColor(), {
                {QStringLiteral("花生"), 1, QColor(), {}},
                {QStringLiteral("核桃"), 1, QColor(), {}}}},
            {QStringLiteral("麥芽"), 0, QColor(), {
                {QStringLiteral("吐司"), 1, QColor(), {}},
                {QStringLiteral("榛果"), 1, QColor(), {}}}},
            {QStringLiteral("糖香"), 0, QColor(), {
                {QStringLiteral("蜂蜜"), 1, QColor(), {}},
                {QStringLiteral("楓糖"), 1, QColor(), {}}}}}},
        {QStringLiteral("乾餾"), 0, QColor(150, 80, 160), {
            {QStringLiteral("樹脂"), 0, QColor(), {
                {QStringLiteral("松木"), 1, QColor(), {}},
                {QStringLiteral("黑醋栗"), 1, QColor(), {}}}},
            {QStringLiteral("辛香"), 0, QColor(), {
                {QStringLiteral("肉桂"), 1, QColor(), {}},
                {QStringLiteral("丁香"), 1, QColor(), {}}}},
            {QStringLiteral("炭燒"), 0, QColor(), {
                {QStringLiteral("菸草"), 1, QColor(), {}},
                {QStringLiteral("焦炭"), 1, QColor(), {}}}}}},
    };

    auto* sb = plot.addSunburst(roots);
    sb->innerFraction = 0.2;
    sb->labelOrientation = Sunburst::LabelOrientation::Radial;
    sb->labelFont.size = 8;
    sb->labelFont.color = QColor(30, 30, 30);
    sb->lightenPerLevel = 22;
    sb->border = LineStyle(QColor(80, 80, 80), 0.8);
    sb->minLabelArcPx = 10;

    plot.title.text = QStringLiteral("咖啡風味輪");
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("極座標", "旭日圖", "沿弧彎曲的分類帶",
              "外圈的分類名用 drawTextOnArc 逐字沿弧排。\n"
              "和「整段文字轉同一個角度」的差別：弧一長，整段轉同一個角度就會"
              "離開弧線、兩端翹起來。分類帶的文字通常很長，一定要逐字排。\n"
              "這裡也示範了環狀圖的分層：內圈是標的、外圈是資產類別。") {
    struct Holding { QString name; double amount; QColor color; };
    struct Category { QString name; QVector<Holding> items; QColor color; };
    const QVector<Category> cats{
        {QStringLiteral("英國股票"), {{QStringLiteral("A"), 5, QColor()},
                                      {QStringLiteral("B"), 6, QColor()},
                                      {QStringLiteral("C"), 6, QColor()},
                                      {QStringLiteral("D"), 7, QColor()}},
         QColor(215, 60, 45)},
        {QStringLiteral("全球股票"), {{QStringLiteral("E"), 7, QColor()},
                                      {QStringLiteral("F"), 4, QColor()},
                                      {QStringLiteral("G"), 4, QColor()},
                                      {QStringLiteral("H"), 4, QColor()},
                                      {QStringLiteral("I"), 6, QColor()}},
         QColor(45, 125, 200)},
        {QStringLiteral("固定收益"), {{QStringLiteral("J"), 5, QColor()},
                                      {QStringLiteral("K"), 4, QColor()},
                                      {QStringLiteral("L"), 5, QColor()},
                                      {QStringLiteral("M"), 4, QColor()},
                                      {QStringLiteral("N"), 5, QColor()},
                                      {QStringLiteral("O"), 4, QColor()},
                                      {QStringLiteral("P"), 4, QColor()},
                                      {QStringLiteral("Q"), 4, QColor()}},
         QColor(95, 175, 85)},
        {QStringLiteral("不動產"), {{QStringLiteral("R"), 5, QColor()}}, QColor(130, 85, 60)},
        {QStringLiteral("另類投資"), {{QStringLiteral("S"), 5, QColor()},
                                      {QStringLiteral("T"), 5, QColor()}},
         QColor(140, 90, 180)},
        {QStringLiteral("現金"), {{QStringLiteral("U"), 1, QColor()}}, QColor(150, 150, 150)},
    };

    QVector<SunburstNode> roots;
    double grand = 0;
    for (const Category& c : cats) {
        SunburstNode node;
        node.label = QString();   // 分類名改用外圈的弧形文字
        node.fillColor = c.color;
        for (const Holding& h : c.items) {
            node.children.append({QStringLiteral("£%1k").arg(h.amount), h.amount, QColor(), {}});
            grand += h.amount;
        }
        roots.append(node);
    }

    auto* sb = plot.addSunburst(roots);
    sb->innerFraction = 0.34;
    sb->startAngle = 90;
    sb->clockwise = true;
    sb->labelOrientation = Sunburst::LabelOrientation::Radial;
    sb->labelFont.size = 9;
    sb->labelFont.color = Qt::white;
    sb->labelFont.bold = true;
    sb->lightenPerLevel = 0;   // 子項沿用分類色，分類帶才和內容對得上

    auto* center = plot.addText(QStringLiteral("組合\n£%1k").arg(grand), 0, 0);
    center->alignment = Qt::AlignHCenter | Qt::AlignVCenter;
    center->label.font.size = 15;
    center->label.font.bold = true;

    // 外圈的分類帶：每一段的角度就是該分類佔的比例
    double cursor = sb->startAngle;
    for (const Category& c : cats) {
        double sum = 0;
        for (const Holding& h : c.items) sum += h.amount;
        const double sweep = -360.0 * sum / grand;   // clockwise
        auto* band = plot.addSector(0, 0, 1.17, cursor, sweep);
        band->innerRadiusX = band->innerRadiusY = 1.04;
        band->fill = FillStyle(c.color.lighter(170));
        band->line = LineStyle(c.color, 1.0);
        band->arcLabel.text = QStringLiteral("%1 %2%")
                                  .arg(c.name)
                                  .arg(sum / grand * 100, 0, 'f', 0);
        band->arcLabel.font.color = c.color.darker(130);
        band->arcLabel.font.bold = true;
        band->arcLabel.font.size = 9;
        cursor += sweep;
    }

    plot.title.text = QStringLiteral("投資組合（外圈為資產類別）");
    plot.equalAxisScale = true;
    plot.setAxisLimits(-1.35, 1.35, -1.35, 1.35);
}

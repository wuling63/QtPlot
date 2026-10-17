// Supplement3.cpp — 對照 ScottPlot 5 Cookbook 第三次補上的範例（函式庫新功能）。
//
// 這一批都是先補了函式庫才做得出來的：刻度的密度與數量、日期刻度的自訂文字、
// 軸的副標題與影像標題、固定繪圖區、圖例的標記覆寫與多個圖例、繪製順序、手繪線、
// 髮絲線、行距、參考線的標籤位置與長度、長條的花紋、散點的縮放與上下填色、
// 可拖的端點／邊緣／資料點、趨勢線、量表的各種模式、三角圖的樣式。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <cmath>

using namespace qtplot;

namespace {

/// 2024-01-01 00:00 起算的 Unix 秒。
double epoch2024() {
    return static_cast<double>(QDateTime(QDate(2024, 1, 1), QTime(0, 0)).toSecsSinceEpoch());
}

/// 取得圖上第一個 T（即時範例在 frame > 0 時要拿它來加資料）。
template <typename T>
T* firstOf(Plot& plot) {
    for (IPlottable* p : plot.plottables())
        if (auto* t = dynamic_cast<T*>(p)) return t;
    return nullptr;
}

}  // namespace

// ───────────────────────────── 座標軸 ─────────────────────────────

QTPLOT_RECIPE("座標軸", "標籤與刻度", "軸的副標題",
              "subLabel 是排在軸標題外側的一行小字，適合放單位、量測條件或資料來源。"
              "它有自己的字型，所以可以做得比標題小、顏色淡一點，主從關係一看就懂。") {
    plot.addSignal(gen::sin(200, 2.0, 3));
    plot.bottomAxis()->label.text = RT("時間");
    plot.bottomAxis()->subLabel.text = RT("取樣率 1 kHz，起點為觸發時刻");
    plot.leftAxis()->label.text = RT("電壓");
    plot.leftAxis()->subLabel.text = RT("單位 mV（已扣除直流偏移）");
    plot.leftAxis()->subLabel.font.italic = true;
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "用影像當軸標題",
              "labelImage 讓軸標題換成一張圖 —— 排好版的公式、圖示、Logo 都可以。"
              "這裡用 QPainter 現畫一張小圖；實際使用時多半是從檔案或資源載入。") {
    plot.addScatter(gen::linspace(0, 10, 50), gen::addNoise(gen::linspace(0, 20, 50), 1.0));

    QImage img(190, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter g(&img);
        g.setRenderHint(QPainter::Antialiasing);
        g.setBrush(QColor(31, 119, 180));
        g.setPen(Qt::NoPen);
        g.drawEllipse(QRectF(2, 5, 20, 20));
        QFont f(QStringLiteral("Times New Roman"), 13);
        f.setItalic(true);
        g.setFont(f);
        g.setPen(QColor(40, 40, 40));
        g.drawText(QRectF(30, 0, 160, 30), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("v(t) = v₀ + a·t"));
    }
    plot.bottomAxis()->labelImage = img;
    plot.leftAxis()->label.text = RT("速度");
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "自訂日期刻度文字",
              "labelFormatter 收到刻度的時間，回傳要印的文字。"
              "這裡把月份印成中文、每年一月改印年份 —— format 字串做不到這種「看情況」的寫法。") {
    const double t0 = epoch2024();
    QVector<double> xs, ys = gen::randomWalk(400, 1.0, 50);
    for (int i = 0; i < 400; ++i) xs.append(t0 + i * 86400.0 * 2);
    plot.addScatter(xs, ys)->marker = MarkerStyle::none();

    auto dt = std::make_shared<DateTimeTickGenerator>();
    dt->labelFormatter = [](const QDateTime& t, double) {
        const int m = t.date().month();
        return m == 1 ? RT("%1 年").arg(t.date().year()) : RT("%1 月").arg(m);
    };
    plot.bottomAxis()->tickGenerator = dt;
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "依縮放層級換日期格式",
              "labelFormatter 的第二個參數是刻度間距（秒）。用它判斷目前看的是「幾年」還是「幾小時」，"
              "換一種寫法：放大到小時就只印時間，縮小到月就只印年月。在 Cookbook 裡用滾輪縮放看看。") {
    const double t0 = epoch2024();
    QVector<double> xs, ys = gen::randomWalk(2000);
    for (int i = 0; i < 2000; ++i) xs.append(t0 + i * 3600.0 * 6);
    plot.addSignalXY(xs, ys);

    auto dt = std::make_shared<DateTimeTickGenerator>();
    dt->labelFormatter = [](const QDateTime& t, double step) {
        if (step < 3600 * 6)       return t.toString(QStringLiteral("HH:mm"));
        if (step < 86400 * 2)      return t.toString(QStringLiteral("M/d\nHH:mm"));
        if (step < 86400 * 40)     return t.toString(QStringLiteral("M/d"));
        return t.toString(QStringLiteral("yyyy-MM"));
    };
    plot.bottomAxis()->tickGenerator = dt;
}

QTPLOT_RECIPE("座標軸", "調整刻度", "刻度的疏密倍率",
              "tickDensity 是相對於預設的倍率：2 約密一倍、0.5 約疏一倍。"
              "刻度仍然取 1／2／5 的好看間距，標籤也仍然保證不重疊 —— 所以是「大約」幾倍。") {
    plot.addSignal(gen::sin(500, 10, 2));
    auto denseX = std::make_shared<AutomaticTickGenerator>();
    denseX->tickDensity = 2.0;
    plot.bottomAxis()->tickGenerator = denseX;
    plot.bottomAxis()->label.text = QStringLiteral("tickDensity = 2");

    auto sparseY = std::make_shared<AutomaticTickGenerator>();
    sparseY->tickDensity = 0.4;
    plot.leftAxis()->tickGenerator = sparseY;
    plot.leftAxis()->label.text = QStringLiteral("tickDensity = 0.4");
}

QTPLOT_RECIPE("座標軸", "調整刻度", "指定刻度數量",
              "targetTickCount 指定「大約幾根」主刻度，不管軸有多長。"
              "儀表板上一排小圖要有一致的刻度數時很好用。實際數量會為了好看的間距差一兩根。") {
    plot.addScatter(gen::linspace(0, 97, 30), gen::randomUniform(30, 0, 1000));
    auto x = std::make_shared<AutomaticTickGenerator>();
    x->targetTickCount = 3;
    plot.bottomAxis()->tickGenerator = x;
    auto y = std::make_shared<AutomaticTickGenerator>();
    y->targetTickCount = 10;
    plot.leftAxis()->tickGenerator = y;
    plot.title.text = RT("X 軸約 3 根、Y 軸約 10 根");
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "對數軸的自訂次刻度",
              "minorMultipliers 決定每個 10 的次方之間畫哪幾根次刻度。預設是 2～9 全畫；"
              "工程上常見只畫 2 和 5（對應 1-2-5 系列），格子比較乾淨。"
              "配合次格線看得更清楚。") {
    // 對數軸的做法是先把資料取 log10 再畫，刻度產生器負責把 10^n 標回去
    QVector<double> xs, logYs;
    for (int i = 0; i <= 60; ++i) {
        xs.append(i / 10.0);
        logYs.append(i / 12.0 + 0.3 * std::sin(i / 4.0));
    }
    plot.addScatter(xs, logYs);

    auto lg = std::make_shared<LogTickGenerator>();
    lg->minorMultipliers = {2, 5};
    plot.leftAxis()->tickGenerator = lg;
    plot.leftAxis()->label.text = RT("數值（log，次刻度只在 2、5）");
    plot.grid.minorLine = LineStyle(QColor(0, 0, 0, 25), 1.0);
}

// ───────────────────────────── 樣式 ─────────────────────────────

QTPLOT_RECIPE("樣式", "版面與框線", "固定繪圖區",
              "fixedDataRect 直接指定繪圖區的像素位置與大小，不讓刻度標籤的寬度去推動它。"
              "幾張圖要疊在一起或上下對齊時，自動版面會因為數字寬度不同而各自偏一點，固定下來就不會。") {
    plot.addSignal(gen::sin(100, 1000));
    plot.fixedDataRect = QRectF(120, 50, 520, 400);
    plot.style.figureBackground = QColor(245, 245, 250);
    plot.title.text = RT("繪圖區固定在 (120, 50)，520 × 400 像素");
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例的標記形狀覆寫",
              "overrideMarkerShape 讓圖例的每一項都畫成同一種實心標記，不畫線段。"
              "線很細、顏色又接近時，一小段線在圖例裡很難分辨，換成大一點的方塊就清楚了。") {
    for (int i = 0; i < 4; ++i)
        plot.addSignal(gen::sin(100, 1, 1, i * 0.4))->legendText = RT("相位 %1").arg(i);
    plot.legend.isVisible = true;
    plot.legend.overrideMarkerShape = true;
    plot.legend.markerShapeOverride = MarkerShape::FilledSquare;
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例標記覆寫（各種形狀）",
              "覆寫的是形狀；顏色還是各自的。原本沒有標記的項目（只有線、或只有色塊）"
              "會借線或填色的顏色。這裡用圓點，配上長條與折線混在一起的圖。") {
    plot.addBars(QVector<double>{3, 5, 4, 6})->legendText = RT("營收");
    auto* s = plot.addScatter({0, 1, 2, 3}, {2.5, 4, 3.5, 5.5});
    s->legendText = RT("目標");
    s->line.width = 1;
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
    plot.legend.overrideMarkerShape = true;
    plot.legend.markerShapeOverride = MarkerShape::FilledCircle;
}

QTPLOT_RECIPE("樣式", "格線與圖例", "多個圖例",
              "主圖例只有一個。兩組圖元要分開說明時，再加一個 LegendPanel，"
              "它只列你指定的那幾個圖元。這裡左上說明實測、右下說明模型。") {
    gen::setSeed(9);
    QVector<IPlottable*> measured, model;
    for (int i = 0; i < 3; ++i) {
        auto* m = plot.addMarkers(gen::linspace(0, 10, 20),
                                  gen::addNoise(gen::linspace(i * 3, i * 3 + 6, 20), 0.6));
        m->legendText = RT("實測 %1").arg(i + 1);
        measured.append(m);
    }
    for (int i = 0; i < 3; ++i) {
        auto* l = plot.addLine(0, i * 3, 10, i * 3 + 6);
        l->line.pattern = LinePattern::Dashed;
        l->legendText = RT("模型 %1").arg(i + 1);
        model.append(l);
    }
    auto* a = plot.addLegendPanel(measured, Qt::AlignLeft | Qt::AlignTop);
    a->style.title.text = RT("量測");
    auto* b = plot.addLegendPanel(model, Qt::AlignRight | Qt::AlignBottom);
    b->style.title.text = RT("理論");
}

QTPLOT_RECIPE("入門", "圖元管理", "調整繪製順序",
              "後加入的圖元畫在上面。moveToFront／moveToBack 可以事後調整，"
              "不用重新加一次。這裡把先加的藍色大圓移到最上層。") {
    auto* blue = plot.addEllipse(0, 0, 3, 3);
    blue->fill = FillStyle(QColor(31, 119, 180, 230));
    auto* orange = plot.addEllipse(2, 1, 3, 3);
    orange->fill = FillStyle(QColor(255, 127, 14, 230));
    auto* green = plot.addEllipse(1, 3, 3, 3);
    green->fill = FillStyle(QColor(44, 160, 44, 230));
    plot.moveToFront(blue);
    plot.moveToBack(green);
    plot.equalAxisScale = true;
    plot.title.text = RT("藍色移到最上、綠色移到最下");
}

QTPLOT_RECIPE("樣式", "主題與配色", "反色的調色盤",
              "inverted() 把每個顏色取反色。為淺色底挑的調色盤直接搬到深色底上，"
              "偏深的那幾個顏色會融進背景；取反色之後亮暗也跟著反過來。") {
    PlotStyle s = PlotStyle::dark();
    s.palette = Palette(Palette::Category10).inverted();
    plot.setStyle(s);
    for (int i = 0; i < 6; ++i)
        plot.addSignal(gen::sin(100, 1 + i * 0.3, 1, i * 0.3))->line.width = 2.5;
}

QTPLOT_RECIPE("樣式", "線與標記", "手繪風格的線",
              "LineStyle::handDrawn 給線加上幾個像素的平滑抖動，看起來像手畫的。"
              "這是在告訴讀者「這是示意，不要讀小數點」。抖動由線的位置決定，每次重畫都一樣。") {
    auto* a = plot.addScatter(gen::linspace(0, 10, 40), gen::sin(40, 3, 1));
    a->line.handDrawn = 3.0;
    a->line.width = 2.5;
    a->marker = MarkerStyle::none();
    auto* b = plot.addScatter(gen::linspace(0, 10, 40), gen::cos(40, 2, 1.5));
    b->line.handDrawn = 3.0;
    b->line.width = 2.5;
    b->marker = MarkerStyle::none();
    plot.setFontFamily(QStringLiteral("Comic Sans MS"));
    plot.title.text = RT("只是示意");
}

QTPLOT_RECIPE("樣式", "版面與框線", "髮絲線模式",
              "hairline 讓座標軸、刻度與格線不論輸出放大幾倍都是 1 個裝置像素。"
              "用 savePng(..., scale = 4) 出印刷圖時，一般的線會跟著變成 4 像素粗，"
              "資料線粗一點沒關係，但框線和格線還是細的比較好看。") {
    plot.addSignal(gen::sin(100, 1, 2))->line.width = 2;
    plot.hairline = true;
    plot.title.text = RT("框線、刻度、格線一律 1 裝置像素");
}

QTPLOT_RECIPE("樣式", "版面與框線", "標題置中在整張圖",
              "標題預設置中在繪圖區上方。左軸刻度很寬、或右邊有色階圖例時，"
              "繪圖區的中線會偏離整張圖的中線；titleCenteredOnFigure 改成以整張圖為準。") {
    plot.addSignal(gen::sin(100, 1e6));
    plot.leftAxis()->label.text = RT("很長很長的左軸標題（單位）");
    plot.title.text = RT("置中在整張圖上");
    plot.titleCenteredOnFigure = true;
}

QTPLOT_RECIPE("樣式", "版面與框線", "整張圖的外框",
              "figureBorder 在整張圖最外圍畫一圈框線。圖要貼進文件或投影片、"
              "背景又剛好是白色時，有框線才看得出圖的邊界。") {
    plot.addSignal(gen::sin(100));
    plot.figureBorder = LineStyle(QColor(31, 119, 180), 4);
}

QTPLOT_RECIPE("標註", "文字", "多行文字的行距",
              "FontStyle::lineHeight 是字型原本行距的倍率：0.8 收緊、1.5 是一行半。"
              "0（預設）沿用字型自己的行距。標註塞了好幾行時，拉開一點比較好讀。") {
    const double heights[] = {0, 0.8, 1.5, 2.2};
    for (int i = 0; i < 4; ++i) {
        auto* t = plot.addText(RT("第一行\n第二行\n第三行"), i * 2.5, 0);
        t->label.font.lineHeight = heights[i];
        t->label.font.size = 13;
        t->alignment = Qt::AlignHCenter | Qt::AlignVCenter;
        t->background = FillStyle(QColor(255, 250, 220));
        t->border = LineStyle(QColor(180, 160, 90), 1);
        t->padding = 6;
        auto* cap = plot.addText(heights[i] == 0 ? RT("預設")
                                                 : QStringLiteral("lineHeight = %1").arg(heights[i]),
                                 i * 2.5, -1.5);
        cap->alignment = Qt::AlignHCenter | Qt::AlignTop;
    }
    plot.setAxisLimits(-1.5, 9, -2.5, 2);
}

QTPLOT_RECIPE("場", "熱圖", "半透明的色階",
              "色階的顏色可以帶透明度：Colormap::fromColors 給的節點有 alpha，查表時就會一起內插。"
              "這裡低值完全透明、高值不透明，熱圖疊在底圖上只露出熱點。") {
    gen::setSeed(2);
    plot.addScatter(gen::linspace(0, 50, 60), gen::addNoise(gen::linspace(5, 45, 60), 4));
    auto* hm = plot.addHeatmap(gen::gaussian2D(40, 40, 0.25), 40, 40);
    hm->extent = CoordinateRect(0, 50, 0, 50);
    hm->colormap = Colormap::fromColors({QColor(255, 0, 0, 0), QColor(255, 120, 0, 120),
                                         QColor(255, 220, 0, 230)});
    plot.addColorbar(*hm);
}

QTPLOT_RECIPE("樣式", "主題與配色", "離散色階",
              "stepped(n) 把連續色階切成 n 個色塊。連續的色階讀不出「這一格到底是多少」，"
              "切成幾段之後才對得回色階圖例。") {
    auto* hm = plot.addHeatmap(gen::sin2D(60, 60, 2), 60, 60);
    hm->colormap = Colormap(Colormap::Turbo).stepped(6);
    plot.addColorbar(*hm);
}

// ───────────────────────────── 統計 ─────────────────────────────

QTPLOT_RECIPE("統計", "直方圖", "固定箱寬的直方圖",
              "Histogram::fromBinWidth 從 min 起每 binWidth 一箱。"
              "考試分數用 10 分一箱、年齡用 5 歲一箱 —— 箱子的邊界本身就有意義時，"
              "就不要讓程式自動決定箱數。") {
    gen::setSeed(5);
    const QVector<double> scores = gen::randomNormal(400, 72, 12);
    const Histogram h = Histogram::fromBinWidth(scores, 0, 100, 10, true);
    plot.addBars(h.toBars(QColor(31, 119, 180)))->borderLine = LineStyle(Qt::white, 1);
    QVector<Tick> ticks;
    for (double e : h.binEdges) ticks.append({e, QString::number(e), true});
    auto m = std::make_shared<ManualTickGenerator>();
    m->ticks = ticks;
    plot.bottomAxis()->tickGenerator = m;
    plot.bottomAxis()->label.text = RT("分數（每 10 分一箱）");
    plot.leftAxis()->label.text = RT("人數");
}

QTPLOT_RECIPE("統計", "直方圖", "兩組資料共用箱子",
              "fromRange 固定範圍與箱數。兩組資料要疊在一起比的時候一定要這樣做 —— "
              "各自自動分箱的話箱子對不齊，比的是兩把不同的尺。") {
    gen::setSeed(8);
    const QVector<double> a = gen::randomNormal(500, 50, 10), b = gen::randomNormal(300, 62, 8);
    const Histogram ha = Histogram::fromRange(a, 10, 100, 30);
    const Histogram hb = Histogram::fromRange(b, 10, 100, 30);
    auto* ba = plot.addBars(ha.toBars(QColor(31, 119, 180, 140)));
    ba->legendText = RT("A 組");
    auto* bb = plot.addBars(hb.toBars(QColor(214, 39, 40, 140)));
    bb->legendText = RT("B 組");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("統計", "直方圖", "機率直方圖",
              "probabilities() 回傳每一箱佔全部的比例（總和為 1）。"
              "樣本數不同的兩組資料要比形狀時，用比例而不是次數。") {
    gen::setSeed(6);
    const Histogram h = Histogram::fromRange(gen::randomNormal(1000, 0, 1), -4, 4, 32);
    QVector<Bar> bars = h.toBars(QColor(44, 160, 44));
    const QVector<double> pr = h.probabilities();
    for (int i = 0; i < bars.size(); ++i) bars[i].value = pr[i] * 100;
    plot.addBars(bars);
    plot.leftAxis()->label.text = RT("佔全部的百分比 (%)");
}

QTPLOT_RECIPE("統計", "箱型與分布", "箱子代表什麼",
              "boxMode 決定箱子的上下緣：四分位數（一般箱型圖）、平均 ± 標準差、平均 ± 標準誤、"
              "或鬚的兩端。同一組資料四種畫法並排，看得出標準誤的箱子小得多 —— "
              "報告上一定要寫清楚畫的是哪一種。") {
    gen::setSeed(12);
    const QVector<double> data = gen::randomNormal(60, 50, 12);
    const Population::BoxMode modes[] = {Population::BoxMode::Quartiles, Population::BoxMode::MeanStdDev,
                                         Population::BoxMode::MeanStdErr, Population::BoxMode::Whiskers};
    const QStringList names{RT("四分位數"), RT("平均±標準差"),
                            RT("平均±標準誤"), RT("鬚的兩端")};
    for (int i = 0; i < 4; ++i) {
        auto* p = plot.addPopulation({data}, {});
        p->groups[0].position = i;
        p->groups[0].color = plot.style.palette.color(i);
        p->boxMode = modes[i];
        p->showErrorBar = false;
        p->showPoints = false;
    }
    QVector<Tick> ticks;
    for (int i = 0; i < 4; ++i) ticks.append({static_cast<double>(i), names[i], true});
    auto m = std::make_shared<ManualTickGenerator>();
    m->ticks = ticks;
    plot.bottomAxis()->tickGenerator = m;
}

QTPLOT_RECIPE("統計", "箱型與分布", "雨雲圖（成分並排）",
              "pointsOffset、boxOffset、distributionOffset 讓三種成分各自往旁邊挪：點在左、"
              "箱子在中、半邊密度在右。同一組資料三種畫法互不遮擋，這就是「雨雲圖」。") {
    gen::setSeed(21);
    auto* p = plot.addPopulation({gen::randomNormal(80, 10, 2), gen::randomNormal(80, 13, 3),
                                  gen::randomNormal(80, 9, 1.5)},
                                 {RT("對照"), RT("藥物 A"), RT("藥物 B")});
    p->showDistribution = true;
    p->mirrored = false;
    p->showErrorBar = false;
    p->jitter = 0.25;
    p->pointsOffset = -0.3;
    p->boxOffset = -0.02;
    p->distributionOffset = 0.05;
    p->groupWidth = 0.8;
}

// ───────────────────────────── 參考線與長條 ─────────────────────────────

QTPLOT_RECIPE("圖種", "參考線與區間", "參考線標籤的位置",
              "textPosition 決定標籤放哪裡：線的尾端、起點、中段，或像游標讀數那樣壓在座標軸上的色塊。"
              "OnAxis 的好處是標籤和刻度在同一排，讀者不用在圖裡找。") {
    plot.addSignal(gen::randomWalk(200, 1, 10));
    auto* a = plot.addHorizontalLine(20, QColor(214, 39, 40));
    a->text.text = RT("尾端（預設）");
    a->text.font.color = QColor(214, 39, 40);
    auto* b = plot.addHorizontalLine(5, QColor(44, 160, 44));
    b->text.text = RT("起點");
    b->text.font.color = QColor(44, 160, 44);
    b->textPosition = AxisLine::TextPosition::InsideStart;
    auto* c = plot.addHorizontalLine(-10, QColor(148, 103, 189));
    c->text.text = QStringLiteral("−10");
    c->text.font.color = Qt::white;
    c->textPosition = AxisLine::TextPosition::OnAxis;
    auto* d = plot.addVerticalLine(120, QColor(255, 127, 14));
    d->text.text = QStringLiteral("t = 120");
    d->text.font.color = Qt::white;
    d->textPosition = AxisLine::TextPosition::OnAxis;
    auto* e = plot.addVerticalLine(60, QColor(23, 190, 207));
    e->text.text = RT("轉 90°，放中段");
    e->text.font.color = QColor(23, 150, 170);
    e->text.font.rotation = -90;
    e->textPosition = AxisLine::TextPosition::InsideCenter;
}

QTPLOT_RECIPE("圖種", "參考線與區間", "只畫一段的參考線",
              "extent 讓參考線只畫在另一軸的某段範圍內。"
              "例如目標值只適用於某幾個月、門檻只對某段時間有效。") {
    plot.addSignal(gen::randomWalk(120, 1, 20));
    auto* goal = plot.addHorizontalLine(25, QColor(214, 39, 40));
    goal->extent = CoordinateRange(40, 90);
    goal->line.width = 3;
    goal->text.text = RT("第二季目標");
    goal->text.font.color = QColor(214, 39, 40);
    auto* v = plot.addVerticalLine(30, QColor(80, 80, 80));
    v->extent = CoordinateRange(15, 35);
    v->line.pattern = LinePattern::Dashed;
}

QTPLOT_RECIPE("圖種", "長條與統計", "標籤畫在最上層",
              "長條重疊（寬度大於間距、或浮動長條交錯）時，後畫的長條會蓋住前一根的標籤。"
              "labelsOnTop 先畫完全部長條再畫標籤，數字就不會被吃掉。") {
    QVector<Bar> bars;
    const double values[] = {5, 8, 6, 9, 7};
    for (int i = 0; i < 5; ++i) {
        Bar b;
        b.position = i * 0.8;
        b.value = values[i];
        b.size = 1.1;  // 故意比間距寬
        b.fillColor = Palette().color(i);
        b.label = RT("數值 %1").arg(values[i]);
        b.labelPlacement = Bar::LabelPlacement::Center;
        b.labelColor = Qt::white;
        bars.append(b);
    }
    auto* bp = plot.addBars(bars);
    bp->labelsOnTop = true;
    bp->valueLabelFont.bold = true;
}

QTPLOT_RECIPE("圖種", "長條與統計", "長條的花紋與框線",
              "Bar::fill 是完整的 FillStyle：花紋、漸層都能用；borderWidth 逐根指定框線粗細。"
              "黑白印刷時顏色分不出來，就靠花紋區分。") {
    const HatchPattern hatches[] = {HatchPattern::None, HatchPattern::DiagonalUp, HatchPattern::Checker,
                                    HatchPattern::Dots, HatchPattern::Horizontal};
    QVector<Bar> bars;
    for (int i = 0; i < 5; ++i) {
        Bar b;
        b.position = i;
        b.value = 3 + i;
        b.fillColor = QColor(200, 220, 240);
        b.fill.color = QColor(200, 220, 240);
        b.fill.hatch = hatches[i];
        b.fill.hatchColor = QColor(30, 70, 130);
        b.borderColor = QColor(30, 70, 130);
        b.borderWidth = 1 + i;
        bars.append(b);
    }
    plot.addBars(bars);
}

// ───────────────────────────── 散點與訊號 ─────────────────────────────

QTPLOT_RECIPE("圖種", "散點與線", "線與標記的上下順序",
              "預設線先畫、標記後畫，標記壓在線上。markersBelowLine 反過來，"
              "線穿過標記 —— 標記是空心或很大時，兩種效果差很多。") {
    auto* a = plot.addScatter(gen::linspace(0, 10, 11), gen::sin(11, 1, 1, 0, 2));
    a->marker = MarkerStyle(MarkerShape::FilledCircle, 18, QColor(255, 200, 100));
    a->line.width = 4;
    a->legendText = RT("標記在上（預設）");
    auto* b = plot.addScatter(gen::linspace(0, 10, 11), gen::sin(11, 1, 1, 0, -1));
    b->marker = MarkerStyle(MarkerShape::FilledCircle, 18, QColor(255, 200, 100));
    b->line.width = 4;
    b->markersBelowLine = true;
    b->legendText = QStringLiteral("markersBelowLine");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("圖種", "散點與線", "基準線上下不同顏色",
              "fillAboveColor／fillBelowColor 讓 fillToY 以上和以下各用一個顏色，"
              "盈虧、溫度距平、高於或低於目標都是這種圖。交界剛好落在基準線上。") {
    gen::setSeed(4);
    auto* s = plot.addScatter(gen::consecutive(200), gen::randomWalk(200));
    s->marker = MarkerStyle::none();
    s->line.color = QColor(60, 60, 60);
    s->fillToY = 0;
    s->fill = FillStyle(QColor(0, 0, 0, 60));
    s->fillAboveColor = QColor(44, 160, 44, 120);
    s->fillBelowColor = QColor(214, 39, 40, 120);
    plot.addHorizontalLine(0, QColor(0, 0, 0));
}

QTPLOT_RECIPE("圖種", "散點與線", "散點的縮放與位移",
              "xScale／yScale／xOffset／yOffset 讓畫出來的位置變成 x·scale + offset，"
              "原始陣列不動。單位換算（秒 → 毫秒）、把幾條曲線上下錯開，都不必另外複製一份資料。") {
    const QVector<double> xs = gen::linspace(0, 1, 50), ys = gen::sin(50, 1, 2);
    for (int i = 0; i < 4; ++i) {
        auto* s = plot.addScatter(xs, ys);
        s->xScale = 1000;      // 秒 → 毫秒
        s->yOffset = i * 2.5;  // 一條一條往上錯開
        s->yScale = 1 + i * 0.3;
        s->legendText = QStringLiteral("yOffset = %1, yScale = %2").arg(i * 2.5).arg(1 + i * 0.3);
    }
    plot.bottomAxis()->label.text = RT("時間 (ms)");
    plot.legend.isVisible = true;
    plot.legend.placement = Legend::Placement::OutsideRight;
}

QTPLOT_RECIPE("圖種", "散點與線", "訊號的倍率",
              "Signal::yScale 把每個值乘上一個倍率再畫。ADC 讀值轉電壓、V 轉 mV，"
              "改一個數字就好，百萬點的資料不必整個重算一次。") {
    const QVector<double> raw = gen::sin(1000, 2048, 3, 0, 2048);  // 12-bit ADC
    auto* s = plot.addSignal(raw);
    s->yScale = 3.3 / 4096 * 1000;  // → mV
    plot.leftAxis()->label.text = RT("電壓 (mV)");
    plot.bottomAxis()->label.text = RT("取樣點");
}

QTPLOT_RECIPE("圖種", "散點與線", "SignalXY 的位移",
              "SignalXY 也有 xOffset／yOffset。同一段波形在不同時間重複出現時，"
              "加好幾次、各自設位移就好。") {
    const QVector<double> xs = gen::linspace(0, 10, 200), ys = gen::sin(200, 1, 3);
    for (int i = 0; i < 3; ++i) {
        auto* s = plot.addSignalXY(xs, ys);
        s->xOffset = i * 12;
        s->yOffset = i * 0.8;
    }
}

QTPLOT_RECIPE("圖種", "散點與線", "SignalXY 的倍率",
              "yScale 只縮放數值方向，X 不動。和 yOffset 一起用就能把好幾條量級不同的訊號放在同一張圖上比形狀。") {
    const QVector<double> xs = gen::linspace(0, 10, 300);
    auto* a = plot.addSignalXY(xs, gen::sin(300, 1, 2));
    a->legendText = RT("原始");
    auto* b = plot.addSignalXY(xs, gen::sin(300, 1, 2));
    b->yScale = 3;
    b->legendText = QStringLiteral("yScale = 3");
    plot.legend.isVisible = true;
}

// ───────────────────────────── 場 ─────────────────────────────

QTPLOT_RECIPE("場", "等值線", "不規則散點的等值線",
              "量測點不在格點上（氣象站、鑽井、感測器）時，addContourScattered 先用反距離加權"
              "內插到規則格點，再畫等值線。黑點是原始量測位置 —— 離它們越遠，等值線越不可信。") {
    gen::setSeed(31);
    const QVector<double> xs = gen::randomUniform(60, -2, 2), ys = gen::randomUniform(60, -2, 2);
    QVector<Coordinates> pts;
    QVector<double> zs;
    for (int i = 0; i < 60; ++i) {
        pts.append({xs[i], ys[i]});
        zs.append(std::exp(-(xs[i] * xs[i] + ys[i] * ys[i])) * 10 + xs[i] * 2);
    }
    auto* c = plot.addContourScattered(pts, zs, 10);
    c->useColormap = true;
    c->showLabels = true;
    plot.addMarkers(xs, ys, QColor(0, 0, 0))->marker.size = 5;
}

QTPLOT_RECIPE("場", "向量場", "箭頭的長度上限",
              "maxArrowLengthPx 限制箭頭最長幾個像素。少數幾個極大值會把縮放拉長、"
              "其他箭頭都縮成點；設上限之後大的箭頭被截短，小的才看得到方向。") {
    auto* vf = plot.addVectorField(
        [](double x, double y) {
            const double r2 = x * x + y * y + 0.05;
            return Vector2{-y / r2, x / r2};  // 中心附近非常大
        },
        CoordinateRect(-2, 2, -2, 2), 17, 17);
    vf->maxArrowLengthPx = 22;
    vf->useColormap = true;
    vf->anchor = VectorField::Anchor::Center;
    plot.equalAxisScale = true;
}

// ───────────────────────────── 財金 ─────────────────────────────

QTPLOT_RECIPE("財金", "K 線", "自訂時間週期的 K 線",
              "resampleOhlc 把日線合成週線（開盤取第一根、收盤取最後一根、高低取極值、量相加）。"
              "用真實時間當 X 時，barDuration 指定一根佔多少秒，蠟燭的寬度才對 —— "
              "不設的話會用頭尾時間平均，遇到假日缺口就估錯。") {
    const QVector<Ohlc> daily = randomOhlcSeries(120);
    const QVector<Ohlc> weekly = resampleOhlc(daily, 7 * 86400.0);
    auto* c = plot.addCandlestick(weekly);
    c->useIndexX = false;
    c->barDuration = 7 * 86400.0;
    auto dt = std::make_shared<DateTimeTickGenerator>();
    plot.bottomAxis()->tickGenerator = dt;
    plot.title.text = RT("120 根日線 → %1 根週線").arg(weekly.size());
}

// ───────────────────────────── 互動 ─────────────────────────────

QTPLOT_RECIPE("互動", "拖曳", "可拉端點的線段",
              "Arrow 設 headShape = None 就是一條線段；draggableEndpoints 讓兩個端點各自可拖。"
              "拿來在圖上量距離、比斜率。在 Cookbook 裡拖拖看兩端的圓點。") {
    plot.addSignal(gen::randomWalk(200));
    auto* seg = plot.addArrow(150, 5, 30, -5, QColor(214, 39, 40));
    seg->headShape = ArrowHead::None;
    seg->draggableEndpoints = true;
    seg->line.width = 3;
    plot.addMarkers({30, 150}, {-5, 5}, QColor(214, 39, 40))->marker.size = 10;
    plot.addAnnotation(RT("拖兩端改變線段"), Qt::AlignRight | Qt::AlignTop);
}

QTPLOT_RECIPE("互動", "拖曳", "水平線段：拉兩端、拖中間",
              "draggable 和 draggableEndpoints 都開：抓端點是拉長縮短，抓中間是整段平移。"
              "標示一段時間區間或一個價格帶時，這比無限長的參考線更精確。") {
    plot.addSignal(gen::randomWalk(200, 1, 10));
    auto* seg = plot.addArrow(140, 15, 60, 15, QColor(31, 119, 180));
    seg->headShape = ArrowHead::None;
    seg->draggable = true;
    seg->draggableEndpoints = true;
    seg->line.width = 5;
    seg->line.cap = Qt::RoundCap;
}

QTPLOT_RECIPE("互動", "拖曳", "垂直線段：拉兩端、拖中間",
              "同樣的兩個開關，換成垂直方向。拖的時候不會限制方向 —— 要鎖住方向請在"
              "plottableDragged 訊號裡把 X 改回來。") {
    plot.addSignal(gen::randomWalk(200, 1, 10));
    auto* seg = plot.addArrow(100, 30, 100, -5, QColor(44, 160, 44));
    seg->headShape = ArrowHead::None;
    seg->draggable = true;
    seg->draggableEndpoints = true;
    seg->line.width = 5;
    seg->line.cap = Qt::RoundCap;
}

QTPLOT_RECIPE("互動", "拖曳", "可調寬度的區間",
              "AxisSpan::resizable 讓區間兩側的邊可以拖，改變範圍；draggable 讓中間可以整段平移。"
              "游標移到邊緣附近（6 像素內）就會抓到邊。") {
    plot.addSignal(gen::randomWalk(300));
    auto* h = plot.addVerticalSpan(80, 160, QColor(255, 127, 14));
    h->resizable = true;
    h->draggable = true;
    h->line = LineStyle(QColor(255, 127, 14), 2);
    auto* v = plot.addHorizontalSpan(-5, 5, QColor(31, 119, 180));
    v->resizable = true;
    v->line = LineStyle(QColor(31, 119, 180), 2);
}

QTPLOT_RECIPE("互動", "拖曳", "可拖動的資料點",
              "Markers::draggablePoints 讓每個點都可以單獨拖，拖的是原始資料 —— 放開之後 xs／ys 就是新值。"
              "dragX = false 鎖住水平方向，只能調整數值，適合手動修正量測值。") {
    auto* m = plot.addMarkers(gen::consecutive(10), gen::randomUniform(10, 2, 8), QColor(214, 39, 40));
    m->marker.size = 14;
    m->draggablePoints = true;
    m->dragX = false;
    plot.setAxisLimits(-1, 10, 0, 10);
    plot.addAnnotation(RT("上下拖動每個點"), Qt::AlignRight | Qt::AlignTop);
}

QTPLOT_RECIPE("互動", "拖曳", "可調大小的矩形",
              "Shape::resizable 讓矩形與橢圓的邊和角都能拖：抓邊改一個方向、抓角同時改兩個方向，"
              "抓中間（draggable 開著時）整個平移。") {
    plot.addSignal(gen::randomWalk(200));
    auto* r = plot.addRectangle(50, 120, -8, 4);
    r->resizable = true;
    r->draggable = true;
    r->fill = FillStyle(QColor(148, 103, 189, 60));
    r->line = LineStyle(QColor(148, 103, 189), 2);
}

QTPLOT_RECIPE("互動", "拖曳", "趨勢線",
              "TrendLine 通過兩個點、畫滿整個繪圖區。兩個點都能拖，抓線本身則整條平移。"
              "目測趨勢、對齊一條理論直線時，讀者要看的是整條線，不是兩點間那一段。") {
    gen::setSeed(14);
    const QVector<double> xs = gen::linspace(0, 100, 60);
    QVector<double> ys = gen::addNoise(xs, 12);
    plot.addMarkers(xs, ys, QColor(31, 119, 180));
    auto* tl = plot.addTrendLine({10, 5}, {90, 95}, QColor(214, 39, 40));
    tl->legendText = RT("目測趨勢（可拖）");
    plot.legend.isVisible = true;
}

// ───────────────────────────── 即時 ─────────────────────────────

QTPLOT_RECIPE_LIVE("即時", "累積記錄", "DataLogger：修改已記錄的值",
                   "最新一筆常常是「暫定值」：一分鐘還沒走完、統計還在累加。"
                   "setY 可以回頭改掉已經記下來的值 —— 這裡每一格先記一個暫定值，"
                   "下一格再用最終值蓋掉。紅點就是還在變動的那一筆。", 60) {
    auto* log = firstOf<DataLogger>(plot);
    if (frame == 0 || !log) {
        log = plot.addDataLogger();
        log->view = LiveView::Slide;
        log->viewWidth = 60;
        log->add(0);
        return;
    }
    const double finalValue = std::sin(frame * 0.15) * 10 + std::sin(frame * 0.031) * 5;
    log->setY(log->count() - 1, finalValue);           // 上一筆定案
    log->add(finalValue + (frame % 7 - 3) * 1.5);       // 新的暫定值
}

// ───────────────────────────── 極座標 ─────────────────────────────

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "圓餅的花紋",
              "PieSlice::fill 可以帶花紋：fillColor 當底色、fill.hatch 疊上去。"
              "要強調某一塊、或要黑白印刷時用。") {
    auto* pie = plot.addPie({35, 25, 20, 20},
                            {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D")});
    pie->slices[1].fill.hatch = HatchPattern::DiagonalUp;
    pie->slices[1].fill.hatchColor = QColor(255, 255, 255, 200);
    pie->slices[3].fill.hatch = HatchPattern::Dots;
    pie->slices[3].fill.hatchColor = QColor(0, 0, 0, 120);
    for (auto& s : pie->slices) s.legendText = s.label;
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("極座標", "極座標軸", "逐圈設定樣式",
              "ringStyles 依順序指定每一圈的線條。最外圈粗、中間一圈虛線當參考值，"
              "其他保持淡淡的 —— 放射線已經能逐條設定，同心圓也可以。") {
    auto* ax = plot.addPolarAxis(1.0, 12);
    ax->ringRadii = {0.25, 0.5, 0.75, 1.0};
    ax->ringStyles = {LineStyle(QColor(0, 0, 0, 40), 1), LineStyle(QColor(214, 39, 40), 2, LinePattern::Dashed),
                      LineStyle(QColor(0, 0, 0, 40), 1), LineStyle(QColor(0, 0, 0), 3)};
    ax->ringLabels = {QStringLiteral("25"), RT("目標 50"), QStringLiteral("75"),
                      QStringLiteral("100")};
}

QTPLOT_RECIPE("極座標", "雷達與量表", "多邊形格線的雷達圖",
              "polygonRings 把同心圓換成以放射線為頂點的多邊形，雷達圖常見的外觀。"
              "資料本身就是折線，配多邊形格線比較一致。") {
    const QStringList dims{RT("速度"), RT("耐力"), RT("力量"),
                           RT("技巧"), RT("反應"), RT("心理")};
    auto* ax = plot.addPolarAxis(1.0, dims.size());
    ax->polygonRings = true;
    ax->spokeLabels = dims;
    ax->labelFont.color = QColor(0, 0, 0, 0);
    ax->showRingLabels = false;
    ax->background = FillStyle(QColor(240, 244, 250));
    auto* r = plot.addRadar({{8, 6, 7, 9, 5, 7}, {5, 9, 6, 6, 8, 6}}, dims);
    r->maxValue = 10;
    r->series[0].legendText = RT("選手 A");
    r->series[1].legendText = RT("選手 B");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "負值的量表",
              "RadialGauge 接受負值：負的往反方向畫。同期比較的漲跌、預算的超支與結餘，"
              "一眼就看得出方向。") {
    auto* g = plot.addRadialGauge({45, -30, 70, -60},
                                  {RT("北區"), RT("南區"), RT("東區"),
                                   RT("西區")},
                                  100);
    g->startAngle = 90;
    for (int i = 0; i < g->values.size(); ++i)
        g->colors[i] = g->values[i] >= 0 ? QColor(44, 160, 44) : QColor(214, 39, 40);
}

QTPLOT_RECIPE("極座標", "雷達與量表", "接續的環形量表",
              "Mode::Cumulative：每個值一個環，但從上一環結束的角度接著畫。"
              "看的是「累積到哪裡了」—— 每一階段各花了多少，最後一環的末端就是總進度。") {
    auto* g = plot.addRadialGauge({20, 15, 25, 10},
                                  {RT("設計"), RT("開發"), RT("測試"),
                                   RT("上線")},
                                  100);
    g->mode = RadialGauge::Mode::Cumulative;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "量表的順序反過來",
              "預設第一個值在最外圈。reverseOrder 讓它在最內圈 —— 內圈半徑小、弧短，"
              "比較次要的項目放外圈反而顯眼時用。") {
    auto* g = plot.addRadialGauge({80, 60, 40, 20},
                                  {RT("第一"), RT("第二"), RT("第三"),
                                   RT("第四")},
                                  100);
    g->reverseOrder = true;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "不滿一圈的量表",
              "sweepAngle 指定滿刻度對應幾度。270 度、缺口朝下的儀表最常見（startAngle = 225，順時針）。"
              "底環也只畫到 270 度。") {
    auto* g = plot.addRadialGauge({72, 45, 88},
                                  {QStringLiteral("CPU"), RT("記憶體"), RT("磁碟")}, 100);
    g->sweepAngle = 270;
    g->startAngle = 225;
    g->clockwise = true;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "半圓的量表",
              "sweepAngle = 180、startAngle = 180 就是半圓儀表。背景環只鋪在 180 度的範圍內，"
              "讀者一看就知道滿格在哪裡。") {
    auto* g = plot.addRadialGauge({65, 30},
                                  {RT("達成率"), RT("退貨率")}, 100);
    g->sweepAngle = 180;
    g->startAngle = 180;
    g->clockwise = true;
    g->valueMode = RadialGauge::ValueMode::Percent;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "數值標籤的位置",
              "valueLabelPosition 決定數值文字落在弧上的哪裡：1 是末端（預設）、0.5 是正中間、0 是起點。") {
    auto* g = plot.addRadialGauge({70, 55, 85}, {}, 100);
    g->valueLabelPosition = 0.5;
    g->valueMode = RadialGauge::ValueMode::Percent;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "數值字級跟著環的粗細",
              "valueFontFraction 讓數值文字的大小等於環厚度的某個比例。"
              "視窗縮放時環會變粗變細，用固定字級的話小視窗會塞不下；用比例就一直剛好。") {
    auto* g = plot.addRadialGauge({60, 75, 40}, {}, 100);
    g->valueFontFraction = 0.6;
    g->gaugeSpacing = 0.1;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "數值標籤的顏色",
              "valueFont 是數值文字自己的字型，不再寫死成白色。淺色的弧配深色字才讀得到。") {
    auto* g = plot.addRadialGauge({60, 80, 45}, {}, 100);
    g->colors = {QColor(255, 230, 150), QColor(200, 240, 200), QColor(200, 220, 250)};
    g->valueFont.color = QColor(40, 40, 40);
    g->valueFont.bold = true;
}

// ───────────────────────────── 特殊座標 ─────────────────────────────

QTPLOT_RECIPE("特殊座標", "三角圖", "三角圖的刻度方向",
              "clockwise 切換刻度的讀法：逆時針（預設）是底邊讀 B、右邊讀 C、左邊讀 A；"
              "順時針則是底邊讀 A、右邊讀 B、左邊讀 C。兩種教科書都有，配錯會把成分讀反。"
              "tickLabelsOnAllEdges 讓三邊都標刻度。") {
    auto* ax = plot.addTriangularAxis({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    ax->clockwise = true;
    ax->tickLabelsOnAllEdges = true;
    ax->gridCount = 5;
    const Coordinates p = TriangularAxis::toCartesian(0.2, 0.5, 0.3);
    plot.addMarkers({p.x}, {p.y}, QColor(214, 39, 40))->marker.size = 10;
    plot.addText(RT("A 20%、B 50%、C 30%"), p.x + 0.03, p.y);
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("特殊座標", "三角圖", "三角圖的底色與格線",
              "background 填三角形內部，gridLine 調格線。淡底色把三角形和外面的空白分開，"
              "格線淡一點才不會搶資料的戲。") {
    auto* ax = plot.addTriangularAxis({RT("砂"), RT("粉砂"), RT("黏土")});
    ax->background = FillStyle(QColor(250, 245, 230));
    ax->gridLine = LineStyle(QColor(150, 120, 60, 90), 1, LinePattern::Dashed);
    ax->gridCount = 10;
    ax->edgeLine = LineStyle(QColor(150, 120, 60), 2);
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("特殊座標", "三角圖", "三角圖的逐邊樣式",
              "edgeStyles 依「底邊、右邊、左邊」的順序各給一種線條，"
              "顏色對應那條邊讀的成分，讀者比較不會讀錯方向。") {
    auto* ax = plot.addTriangularAxis({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    ax->edgeStyles = {LineStyle(QColor(31, 119, 180), 3), LineStyle(QColor(44, 160, 44), 3),
                      LineStyle(QColor(214, 39, 40), 3)};
    ax->tickLabelsOnAllEdges = true;
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("特殊座標", "三角圖", "三角圖的邊標題",
              "edgeTitles 沿著每一條邊寫標題（底邊、右邊、左邊），文字跟著邊的角度轉。"
              "角落標籤說「這個角是什麼」，邊標題說「這條邊讀什麼」。") {
    auto* ax = plot.addTriangularAxis({RT("石英"), RT("長石"), RT("岩屑")});
    ax->edgeTitles = {RT("長石 (%) →"), RT("岩屑 (%) →"), RT("← 石英 (%)")};
    ax->edgeTitleFont.bold = true;
    ax->edgeTitleFont.color = QColor(80, 80, 80);
    ax->tickLabelsOnAllEdges = true;
    plot.equalAxisScale = true;
    plot.setAxisLimits(-0.3, 1.3, -0.25, 1.1);
}

// Advanced.cpp — 進階樣式與畫法：顏色漸層、旋轉形狀、背景影像、平滑曲線、
//                 垂直訊號、部分繪製、文字格式。
#include "../Recipe.h"
#include "qtplot/DataGen.h"
#include "qtplot/Statistics.h"

#include <QImage>
#include <QPainter>

#include <cmath>

using namespace qtplot;

namespace {

/// 做一張有內容的底圖（不依賴外部檔案，範例才能到處跑）。
QImage backdrop(int w = 480, int h = 320) {
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient g(0, 0, w, h);
    g.setColorAt(0.0, QColor(20, 40, 90));
    g.setColorAt(0.5, QColor(30, 110, 110));
    g.setColorAt(1.0, QColor(200, 180, 60));
    p.fillRect(img.rect(), g);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 18; ++i) {
        const double f = i / 17.0;
        p.setBrush(QBrush(QColor(255, 255, 255, 26)));
        p.drawEllipse(QPointF(w * f, h * (0.5 + 0.35 * std::sin(f * 6.0))), w * 0.09, h * 0.13);
    }
    p.end();
    return img;
}

}  // namespace

// ───────────────────── 顏色與形狀 ─────────────────────

QTPLOT_RECIPE("樣式", "進階樣式", "顏色漸層",
              "blendColors() 在兩個顏色之間線性內插，gradientColors() 一次給你整串。"
              "在 sRGB 分量上直接插 —— 色彩學上「正確」的做法是先轉到線性光再插，"
              "但那樣中段會偏亮，和大多數繪圖庫的結果對不起來。") {
    const QVector<QColor> colors = gradientColors(QColor(20, 40, 220), QColor(20, 150, 60), 11);
    for (int i = 0; i <= 10; ++i) {
        const double fraction = i / 10.0;
        auto* e = plot.addEllipse(i, std::sin(2 * kPi * fraction), 1.6, 1.0);
        e->fill = FillStyle(QColor(colors[i].red(), colors[i].green(), colors[i].blue(), 150));
        e->line = LineStyle(colors[i], 1.5);
    }
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("樣式", "進階樣式", "旋轉的形狀",
              "rotationDegrees 讓矩形、橢圓、多邊形繞自己的中心轉。"
              "旋轉是在「資料座標」上做的，所以兩軸比例不同時轉過的圓會變成斜橢圓 —— "
              "這是正確的（資料空間的旋轉本來就該這樣），要看起來像旋轉請開 equalAxisScale。") {
    for (int i = 0; i < 10; ++i) {
        auto* e = plot.addEllipse(0, 0, 10, 1.2);
        e->rotationDegrees = i * 18;
        e->fill = FillStyle(QColor(0, 0, 0, 0));
        e->line = LineStyle(QColor(40, 40, 220, 60 + i * 18), 1.2);
    }
    plot.equalAxisScale = true;
    plot.title.text = QStringLiteral("每個橢圓轉 18°");
}

QTPLOT_RECIPE("樣式", "進階樣式", "旋轉的矩形與多邊形",
              "三種形狀都支援旋轉。axisLimits 用的是旋轉後的頂點，"
              "所以自動範圍不會把轉出去的部分切掉。") {
    auto* r = plot.addRectangle(-4, 4, -1.5, 1.5);
    r->rotationDegrees = 30;
    r->legendText = QStringLiteral("矩形轉 30°");

    auto* tri = plot.addPolygon({{0, 3}, {-2.6, -1.5}, {2.6, -1.5}});
    tri->rotationDegrees = -20;
    tri->legendText = QStringLiteral("三角形轉 -20°");

    auto* e = plot.addEllipse(0, 0, 5, 2);
    e->rotationDegrees = 70;
    e->fill = FillStyle(QColor(0, 0, 0, 0));
    e->legendText = QStringLiteral("橢圓轉 70°");

    plot.equalAxisScale = true;
    plot.legend.isVisible = true;
}

// ───────────────────── 背景影像 ─────────────────────

QTPLOT_RECIPE("樣式", "背景影像", "整張圖的背景影像",
              "figureBackgroundImage 鋪滿整個畫布（含座標軸區域）。"
              "opacity 調低一點當底圖，資料才看得清楚。") {
    BackgroundImage bg;
    bg.image = backdrop();
    bg.opacity = 0.9;
    plot.figureBackgroundImage = bg;

    // 背景是深色的，所以把軸與文字換成淺色
    PlotStyle s = PlotStyle::dark();
    s.figureBackground = QColor(0, 0, 0, 0);  // 透明，讓背景影像透出來
    s.dataBackground = QColor(0, 0, 0, 0);
    plot.setStyle(s);
    plot.grid.majorLine = LineStyle(QColor(255, 255, 255, 50), 1.0);

    auto* a = plot.addSignal(gen::sin(200, 1.0, 2.0), 1.0, QColor(80, 170, 255));
    a->line.width = 2.5;
    auto* b = plot.addSignal(gen::cos(200, 1.0, 2.0), 1.0, QColor(255, 150, 60));
    b->line.width = 2.5;
}

QTPLOT_RECIPE("樣式", "背景影像", "只鋪在繪圖區",
              "dataBackgroundImage 只鋪在繪圖區裡，不含座標軸。"
              "和 ImageRect 的差別：ImageRect 釘在「資料座標」上會跟著平移縮放，"
              "這個釘在「繪圖區」上，縮放時不動。地圖底圖要用 ImageRect，"
              "裝飾性的底紋用這個 —— 試著縮放看差別。") {
    BackgroundImage bg;
    bg.image = backdrop();
    bg.opacity = 0.35;
    bg.keepAspectRatio = false;
    plot.dataBackgroundImage = bg;

    plot.addScatter(gen::consecutive(40), gen::randomWalk(40))->line.width = 2;
    plot.grid.isVisible = false;
}

// ───────────────────── 平滑曲線 ─────────────────────

QTPLOT_RECIPE("圖種", "散點與線", "平滑曲線",
              "interpolation 設成 Smooth 會用 Catmull–Rom 把折線變成曲線。"
              "smoothTension 控制彎的程度，0 接近直線、1 最彎。") {
    const QVector<double> xs = gen::consecutive(12);
    const QVector<double> ys = gen::randomWalk(12);

    auto* straight = plot.addScatter(xs, ys, QColor(180, 180, 180));
    straight->line.width = 1.0;
    straight->marker = MarkerStyle(MarkerShape::FilledCircle, 7, QColor(120, 120, 120));
    straight->legendText = QStringLiteral("直線相連");

    for (double tension : {0.3, 0.7}) {
        auto* smooth = plot.addScatter(xs, ys);
        smooth->interpolation = Scatter::Interpolation::Smooth;
        smooth->smoothTension = tension;
        smooth->marker = MarkerStyle::none();
        smooth->line.width = 2;
        smooth->legendText = QStringLiteral("Smooth，tension = %1").arg(tension);
    }
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("圖種", "散點與線", "平滑但不過衝",
              "Catmull–Rom 會在轉折處衝出原始資料的範圍 —— "
              "資料從 0 漲到 1 再平下來，曲線會先衝過 1 再回來，"
              "看起來像真的量到過 1.05。量測資料不能這樣畫。"
              "SmoothMonotone 用單調三次內插，保證不超出原始範圍。") {
    const QVector<double> xs{0, 1, 2, 3, 4, 5, 6, 7};
    const QVector<double> ys{0, 0, 0, 1, 1, 1, 0, 0};

    auto* over = plot.addScatter(xs, ys, QColor(214, 39, 40));
    over->interpolation = Scatter::Interpolation::Smooth;
    over->smoothTension = 0.8;
    over->marker = MarkerStyle::none();
    over->line.width = 2;
    over->legendText = QStringLiteral("Smooth（會過衝）");

    auto* mono = plot.addScatter(xs, ys, QColor(44, 160, 44));
    mono->interpolation = Scatter::Interpolation::SmoothMonotone;
    mono->marker = MarkerStyle::none();
    mono->line.width = 2;
    mono->legendText = QStringLiteral("SmoothMonotone（不過衝）");

    auto* raw = plot.addScatter(xs, ys, QColor(60, 60, 60));
    raw->line = LineStyle::none();
    raw->marker = MarkerStyle(MarkerShape::FilledCircle, 8, QColor(60, 60, 60));
    raw->legendText = QStringLiteral("原始資料");

    plot.addHorizontalLine(0.0, QColor(150, 150, 150))->line.pattern = LinePattern::Dashed;
    plot.addHorizontalLine(1.0, QColor(150, 150, 150))->line.pattern = LinePattern::Dashed;

    // 自動範圍只看「資料點」，紅線衝出去的部分會被切掉 ——
    // 那正是要給人看的東西，所以手動把 Y 放寬
    plot.setAxisLimits(-0.4, 7.4, -0.35, 1.35);
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignVCenter;
}

// ───────────────────── 垂直訊號與部分繪製 ─────────────────────

QTPLOT_RECIPE("圖種", "散點與線", "垂直方向的訊號",
              "vertical 讓索引往上長、數值往右。井深剖面、層序地層、"
              "頻譜瀑布圖這類資料本來就該直著看。"
              "抽樣與可視範圍裁切都會跟著改用 Y 軸，一百萬點照樣順。") {
    auto* s = plot.addSignal(gen::sin(5000, 1.0, 4.0));
    s->vertical = true;
    s->legendText = QStringLiteral("vertical = true");
    plot.leftAxis()->label.text = QStringLiteral("取樣點（索引）");
    plot.bottomAxis()->label.text = QStringLiteral("數值");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("圖種", "散點與線", "垂直訊號加反向 X 軸",
              "再把 X 軸反向，正值就跑到左邊 —— 地質柱狀圖與某些儀器的慣例。"
              "兩個設定是獨立的：vertical 轉資料的方向，inverted 轉軸的方向。") {
    auto* s = plot.addSignalXY(gen::consecutive(5000), gen::sin(5000, 1.0, 4.0));
    s->vertical = true;
    s->marker = MarkerStyle::none();
    plot.bottomAxis()->inverted = true;
    plot.leftAxis()->label.text = QStringLiteral("深度");
    plot.bottomAxis()->label.text = QStringLiteral("數值（向左為正）");
}

QTPLOT_RECIPE("圖種", "散點與線", "只畫一段",
              "renderFirst / renderLast 限制要畫的索引範圍。"
              "資料很長但只想看其中一段時，比另外複製一份子陣列省事也省記憶體 —— "
              "尤其是資料還在持續增長的時候。") {
    const QVector<double> ys = gen::randomWalk(1000);

    auto* all = plot.addSignal(ys, 1.0, QColor(200, 200, 200));
    all->legendText = QStringLiteral("全部 1000 點");

    auto* part = plot.addSignal(ys, 1.0, QColor(214, 39, 40));
    part->renderFirst = 300;
    part->renderLast = 500;
    part->line.width = 2.5;
    part->legendText = QStringLiteral("只畫 300–500");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

// ───────────────────── 文字格式 ─────────────────────

QTPLOT_RECIPE("標註", "文字", "文字的完整格式",
              "字級、粗體、斜體、旋轉、文字色、背景色、外框、內距、"
              "以及相對於資料點的對齊方式與像素偏移，全部分開設定。") {
    plot.setAxisLimits(40.8, 43.2, 67.8, 70.2);

    auto* t = plot.addText(QStringLiteral("Hello, World"), 42, 69);
    t->label.font.size = 26;
    t->label.font.bold = true;
    t->label.font.rotation = -45;
    t->label.font.color = QColor(255, 230, 0);
    t->background = FillStyle(QColor(190, 90, 220));
    t->border = LineStyle(QColor(90, 30, 110), 2.0);
    t->padding = 10;
    t->alignment = Qt::AlignHCenter | Qt::AlignVCenter;

    plot.addMarkers({42}, {69}, QColor(40, 40, 40))->marker.size = 9;
    plot.addAnnotation(QStringLiteral("黑點是文字錨定的資料座標 (42, 69)"),
                       Qt::AlignLeft | Qt::AlignTop);
}

QTPLOT_RECIPE("標註", "文字", "對齊與像素偏移",
              "alignment 決定文字相對錨點的位置，offsetX / offsetY 再補一個像素偏移 —— "
              "資料點旁邊的標註通常要兩者搭配，才不會壓在點上。") {
    const QVector<double> xs{1, 2, 3, 4, 5};
    const QVector<double> ys{3, 1, 4, 1, 5};
    plot.addMarkers(xs, ys, QColor(31, 119, 180))->marker.size = 10;

    const QVector<Qt::Alignment> aligns{
        Qt::AlignHCenter | Qt::AlignBottom, Qt::AlignLeft | Qt::AlignVCenter,
        Qt::AlignHCenter | Qt::AlignTop, Qt::AlignRight | Qt::AlignVCenter,
        Qt::AlignHCenter | Qt::AlignBottom};
    const QStringList names{QStringLiteral("上方"), QStringLiteral("右側"),
                            QStringLiteral("下方"), QStringLiteral("左側"),
                            QStringLiteral("上方＋偏移")};

    for (int i = 0; i < xs.size(); ++i) {
        auto* t = plot.addText(names[i], xs[i], ys[i]);
        t->alignment = aligns[i];
        t->offsetY = (aligns[i] & Qt::AlignBottom) ? -9 : ((aligns[i] & Qt::AlignTop) ? 9 : 0);
        t->offsetX = (aligns[i] & Qt::AlignLeft) ? 9 : ((aligns[i] & Qt::AlignRight) ? -9 : 0);
        if (i == 4) t->offsetY -= 18;  // 再往上推一點
        t->label.font.size = 10;
    }
    plot.setAxisLimits(0.4, 5.6, 0.2, 6.2);
}

// ───────────────────── 漸層與堆疊 ─────────────────────

QTPLOT_RECIPE("進階", "漸層與堆疊", "漸層填色（水平）",
              "fill.gradient 設成 Horizontal，顏色就沿著 X 方向鋪過去。"
              "漸層鋪的範圍是「填色區域」而不是整個繪圖區 —— "
              "用繪圖區的話，曲線只佔畫面一半時會只看到色階的一半。") {
    const QVector<double> xs = gen::consecutive(51);
    const QVector<double> ys = gen::sin(51, 1.0, 1.0);
    auto* sc = plot.addScatter(xs, ys);
    sc->marker = MarkerStyle::none();
    sc->line = LineStyle::none();
    sc->fillToY = 0;
    sc->fill = FillStyle::linearGradient(GradientDirection::Horizontal,
                                         Colormap(Colormap::Jet).sampleColors(16));
    plot.setAxisLimits(0, 50, -1.1, 1.1);
}

QTPLOT_RECIPE("進階", "漸層與堆疊", "漸層填色（垂直）",
              "方向改成 Vertical 就沿著 Y 方向。注意 QLinearGradient 的座標是像素、"
              "像素的 y 往下長，所以「由下往上」的漸層必須從 bottom 畫到 top，"
              "否則顏色會上下顛倒 —— 這一步在 FillStyle::brush() 裡處理掉了。") {
    const QVector<double> xs = gen::consecutive(51);
    const QVector<double> ys = gen::sin(51, 1.0, 1.0);
    auto* sc = plot.addScatter(xs, ys);
    sc->marker = MarkerStyle::none();
    sc->line = LineStyle::none();
    sc->fillToY = 0;
    sc->fill = FillStyle::linearGradient(
        GradientDirection::Vertical,
        {QColor(200, 0, 0), QColor(40, 40, 220), QColor(255, 60, 180)});
    plot.setAxisLimits(0, 50, -1.1, 1.1);
}

QTPLOT_RECIPE("進階", "漸層與堆疊", "堆疊面積圖",
              "把每一層的「累積值」各畫成一條填到 0 的線，再由大到小疊上去，"
              "看起來就是堆疊面積圖。順序很重要：先畫總和、最後畫最底層，"
              "後畫的才會蓋在前面上面。") {
    const QVector<double> xs{1, 2, 3, 4};
    const QVector<QVector<double>> layers{
        {1, 3, 1, 2},   // 最底層
        {3, 7, 3, 1},
        {5, 2, 5, 6},
    };
    const QVector<QColor> colors{QColor(0, 140, 0), QColor(255, 165, 0), QColor(0, 0, 230)};

    // 先算每一層的累積值，再反過來畫
    QVector<QVector<double>> cumulative;
    QVector<double> running(xs.size(), 0.0);
    for (const QVector<double>& layer : layers) {
        for (int i = 0; i < xs.size(); ++i) running[i] += layer[i];
        cumulative.append(running);
    }
    for (int i = cumulative.size() - 1; i >= 0; --i) {
        auto* sc = plot.addScatter(xs, cumulative[i], colors[i]);
        sc->marker = MarkerStyle::none();
        sc->fillToY = 0;
        sc->fill = FillStyle(colors[i]);
        sc->line = LineStyle(QColor(0, 0, 0), 1.0);
    }
    plot.setAxisLimits(1, 4, 0, 13);
}

QTPLOT_RECIPE("進階", "漸層與堆疊", "堆疊階梯面積圖",
              "同樣的堆疊手法配上 step：每一層都是階梯狀的填色區。"
              "類別型的資料用階梯比用斜線誠實 —— 兩個類別之間並沒有中間值。") {
    gen::setSeed(7);
    const QStringList names{QStringLiteral("Alfred"), QStringLiteral("Ralph"),
                            QStringLiteral("Don"), QStringLiteral("James")};
    const QVector<QVector<double>> layers{
        {8.3, 7.0, 6.2, 4.5},
        {7.7, 6.5, 6.6, 6.1},
        {6.2, 7.2, 5.4, 3.8},
    };
    const QVector<QColor> colors{QColor(114, 158, 206), QColor(255, 158, 74),
                                 QColor(103, 191, 92)};

    // 階梯要畫到最後一個類別的右邊，所以多補一個 X
    QVector<double> xs;
    for (int i = 0; i <= names.size(); ++i) xs.append(i);

    QVector<double> running(names.size(), 0.0);
    QVector<QVector<double>> cumulative;
    for (const QVector<double>& layer : layers) {
        for (int i = 0; i < names.size(); ++i) running[i] += layer[i];
        QVector<double> padded = running;
        padded.append(running.last());  // 最後一段要有終點才畫得出來
        cumulative.append(padded);
    }
    for (int i = cumulative.size() - 1; i >= 0; --i) {
        auto* sc = plot.addScatter(xs, cumulative[i], colors[i]);
        sc->marker = MarkerStyle::none();
        sc->step = Scatter::StepStyle::Horizontal;
        sc->fillToY = 0;
        sc->fill = FillStyle(colors[i]);
        sc->line = LineStyle(colors[i].darker(140), 1.0);
    }

    QVector<Tick> ticks;
    for (int i = 0; i < names.size(); ++i) ticks.append({i + 0.5, names[i]});
    auto gen = std::make_shared<ManualTickGenerator>();
    gen->ticks = ticks;
    plot.bottomAxis()->tickGenerator = gen;
    plot.setAxisLimits(0, names.size(), 0, 24);
}

QTPLOT_RECIPE("進階", "漸層與堆疊", "錯位堆疊（脊線圖）",
              "每條曲線往上也往右錯開一點，再各自填色，就是脊線圖（ridgeline）。"
              "後畫的蓋住前一條的下半部，看起來像一疊山稜線。"
              "適合比較很多條形狀相近的曲線 —— 疊在一起會糊成一片。") {
    gen::setSeed(31);
    const int lines = 14;
    const int n = 100;
    const Colormap cmap(Colormap::Turbo);

    // 由上往下畫：先畫最後面（最上面）那條，後畫的才會蓋在前面
    for (int i = lines - 1; i >= 0; --i) {
        const double frac = static_cast<double>(i) / (lines - 1);
        QVector<double> xs(n), ys(n);
        for (int j = 0; j < n; ++j) {
            // 往右錯位：靠上的曲線整條右移，形成斜斜的一疊
            xs[j] = j + frac * 6.0;
            // 底色是一條往右抬升的曲線，加一點雜訊
            ys[j] = 4.5 + frac * 4.0 + 1.1 / (1.0 + std::exp(-(j - 55) / 9.0))
                    + gen::randomNormal(1, 0, 0.055).first();
        }
        auto* sc = plot.addScatter(xs, ys);
        sc->marker = MarkerStyle::none();
        sc->line = LineStyle(QColor(20, 20, 60), 1.0);
        sc->fillToY = 4.0;
        sc->fill = FillStyle(cmap.lookup(1.0 - frac));
    }
    plot.setAxisLimits(0, 100, 4.0, 10.2);
}

// ───────────────────── 組合與標註 ─────────────────────

QTPLOT_RECIPE("進階", "組合與標註", "三軸組合圖",
              "面積、長條、折線三種圖疊在一起，各自綁自己的 Y 軸。"
              "三個量綱完全不同（金額、百分比、百分比），共用一根軸會讓其中兩個"
              "壓成貼著底線的直線。\n"
              "每根軸的顏色要和它的資料一致 —— 三根軸並排時，"
              "讀者沒有別的線索分辨哪根配哪條。") {
    const QStringList names{QStringLiteral("Product A"), QStringLiteral("Product B"),
                            QStringLiteral("Product C"), QStringLiteral("Product D"),
                            QStringLiteral("Product E")};
    const QVector<double> revenue{1180, 1520, 2010, 1700, 1760};
    const QVector<double> markup{19, 15, 24, 16, 18};
    const QVector<double> margin{15.5, 9.5, 20.5, 11.5, 17.5};

    const QColor revColor(110, 165, 225);
    const QColor markColor(245, 160, 85);
    const QColor marginColor(70, 160, 90);

    QVector<double> xs;
    for (int i = 0; i < names.size(); ++i) xs.append(i);

    // 1) 面積（左軸）
    auto* area = plot.addScatter(xs, revenue, revColor);
    area->marker = MarkerStyle::none();
    area->interpolation = Scatter::Interpolation::Smooth;
    area->fillToY = 0;
    area->fill = FillStyle(QColor(revColor.red(), revColor.green(), revColor.blue(), 110));
    area->legendText = QStringLiteral("營收 ($)");
    plot.leftAxis()->setRange(0, 2080);
    plot.leftAxis()->label.text = QStringLiteral("營收 ($)");
    plot.leftAxis()->label.font.color = revColor;
    plot.leftAxis()->tickFont.color = revColor;

    // 2) 長條（第二根左軸）
    auto* markupAxis = plot.addAxis(Edge::Left);
    markupAxis->showTickLabels = true;
    markupAxis->majorTickLength = 5;
    markupAxis->setRange(0, 26);
    markupAxis->label.text = QStringLiteral("加價率 (%)");
    markupAxis->label.font.color = markColor;
    markupAxis->tickFont.color = markColor;
    markupAxis->majorTickLine.color = markColor;

    QVector<Bar> bars;
    for (int i = 0; i < names.size(); ++i)
        bars.append({static_cast<double>(i), markup[i], 0, 0, 0.42, markColor,
                     QColor(0, 0, 0, 0), QString()});
    auto* bp = plot.addBars(bars);
    bp->yAxis = markupAxis;
    bp->legendText = QStringLiteral("加價率 (%)");

    // 3) 折線（右軸）
    auto* marginAxis = plot.addAxis(Edge::Right);
    marginAxis->showTickLabels = true;
    marginAxis->majorTickLength = 5;
    marginAxis->setRange(0, 21);
    marginAxis->label.text = QStringLiteral("毛利率 (%)");
    marginAxis->label.font.color = marginColor;
    marginAxis->tickFont.color = marginColor;
    marginAxis->majorTickLine.color = marginColor;

    auto* line = plot.addScatter(xs, margin, marginColor);
    line->yAxis = marginAxis;
    line->line.width = 2;
    line->interpolation = Scatter::Interpolation::Smooth;
    line->marker = MarkerStyle(MarkerShape::FilledCircle, 8, marginColor);
    line->legendText = QStringLiteral("毛利率 (%)");

    QVector<Tick> ticks;
    for (int i = 0; i < names.size(); ++i) ticks.append({static_cast<double>(i), names[i]});
    auto gen = std::make_shared<ManualTickGenerator>();
    gen->ticks = ticks;
    plot.bottomAxis()->tickGenerator = gen;
    plot.setAxisLimits(-0.6, names.size() - 0.4, 0, 2080);

    plot.title.text = QStringLiteral("加價率 vs 毛利率");
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
}

QTPLOT_RECIPE("進階", "組合與標註", "散點加資料標籤",
              "pointLabels 在每個點旁邊標字。點很多的時候標籤會糊成一片，"
              "所以超過 maxLabels 就自動不標 —— 寧可不標也不要讓圖變得讀不了。") {
    const QStringList names{
        QStringLiteral("Joyce"),  QStringLiteral("James"), QStringLiteral("Alice"),
        QStringLiteral("Louise"), QStringLiteral("Jane"),  QStringLiteral("Thomas"),
        QStringLiteral("Jeffrey"), QStringLiteral("John"), QStringLiteral("Carol"),
        QStringLiteral("Janet"),  QStringLiteral("Judy"),  QStringLiteral("Barbara"),
        QStringLiteral("Mary"),   QStringLiteral("Henry"), QStringLiteral("Robert"),
        QStringLiteral("William"), QStringLiteral("Ronald"), QStringLiteral("Alfred"),
        QStringLiteral("Philip")};
    const QVector<double> heights{51.3, 56.3, 56.5, 56.8, 59.8, 57.5, 62.5, 59.0, 62.8,
                                  62.5, 64.3, 65.3, 66.5, 65.3, 64.8, 66.5, 67.0, 69.0, 72.0};
    const QVector<double> weights{50.5, 77.0, 84.0, 77.0, 84.5, 85.0, 84.0, 99.5, 102.5,
                                  112.5, 90.0, 98.0, 112.0, 98.0, 128.0, 112.0, 133.0,
                                  112.5, 150.0};

    auto* m = plot.addMarkers(heights, weights);
    m->marker = MarkerStyle(MarkerShape::OpenCircle, 9, QColor(40, 70, 190));
    m->pointLabels.texts = names;
    m->pointLabels.font.color = QColor(40, 70, 190);
    m->pointLabels.font.size = 9;

    plot.bottomAxis()->label.text = QStringLiteral("身高");
    plot.leftAxis()->label.text = QStringLiteral("體重");
    plot.title.text = QStringLiteral("體重對身高");
    plot.setAxisLimits(49, 75, 40, 160);
}

QTPLOT_RECIPE("進階", "組合與標註", "分組散點加各組迴歸線",
              "三組資料各自做線性迴歸。整體看起來是一條斜線的資料，"
              "分組之後每一組的斜率可能完全不同（甚至反向）—— "
              "這是辛普森悖論，不分組就看不出來。"
              "圖例把每一組的截距與斜率也列出來。") {
    gen::setSeed(21);
    const QStringList groups{QStringLiteral("Setosa"), QStringLiteral("Versicolor"),
                             QStringLiteral("Virginica")};
    const QVector<QColor> colors{QColor(60, 80, 160), QColor(185, 60, 50),
                                 QColor(20, 125, 110)};
    const QVector<double> baseX{15, 43, 56};
    const QVector<double> baseY{49, 59, 66};
    const QVector<double> slopes{0.54, 0.83, 1.0};

    for (int g = 0; g < groups.size(); ++g) {
        const int n = 50;
        QVector<double> xs(n), ys(n);
        for (int i = 0; i < n; ++i) {
            xs[i] = baseX[g] + gen::randomNormal(1, 0, 4.0).first();
            ys[i] = baseY[g] + (xs[i] - baseX[g]) * slopes[g] +
                    gen::randomNormal(1, 0, 2.2).first();
        }
        auto* pts = plot.addMarkers(xs, ys);
        pts->marker = MarkerStyle(g == 0 ? MarkerShape::OpenCircle
                                         : (g == 1 ? MarkerShape::Plus : MarkerShape::Eks),
                                  7, colors[g]);

        const LinearFit fit = linearFit(xs, ys);
        double lo = xs[0], hi = xs[0];
        for (double v : xs) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        auto* line = plot.addScatter({lo, hi}, {fit.predict(lo), fit.predict(hi)}, colors[g]);
        line->marker = MarkerStyle::none();
        line->line.width = 2;
        line->legendText = QStringLiteral("%1  截距 %2  斜率 %3")
                               .arg(groups[g])
                               .arg(fit.intercept, 0, 'f', 2)
                               .arg(fit.slope, 0, 'f', 3);
    }

    plot.bottomAxis()->label.text = QStringLiteral("花瓣長 (mm)");
    plot.leftAxis()->label.text = QStringLiteral("花萼長 (mm)");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("進階", "組合與標註", "偏態:平均數被尾巴拉走",
              "左偏與右偏各一張，標出平均數與中位數。\n"
              "重點在兩條線的相對位置：平均數會被長尾拉過去，中位數不會。"
              "右偏時平均 > 中位數，左偏時相反 —— "
              "所以報薪資、房價這種右偏的資料，用平均數會系統性地高估「典型值」。") {
    gen::setSeed(4);
    const int n = 20000;

    // 右偏：對數常態。左偏：把它鏡射過來。
    QVector<double> right, left;
    for (int i = 0; i < n; ++i) {
        const double v = std::exp(gen::randomNormal(1, 0, 0.6).first());
        if (v < 4.0) right.append(v);
    }
    for (double v : right) left.append(4.0 - v);

    /// 畫一張直方圖（轉成密度），回傳它的平均數與中位數。
    auto addPanel = [&](const QVector<double>& data, double xOffset, QColor color) {
        const Histogram h = Histogram::fromValues(data, 45);
        // 轉成密度，兩張圖的高度才可比：樣本數不同時，次數的高度沒有可比性
        const QVector<double> dens = h.densities();

        QVector<Bar> bars;
        for (int i = 0; i < h.binCenters.size(); ++i)
            bars.append({h.binCenters[i] + xOffset, dens[i], 0, 0, h.binWidth, color,
                         QColor(0, 0, 0, 0), QString()});
        auto* bp = plot.addBars(bars);
        bp->borderLine = LineStyle(QColor(255, 255, 255), 1.0);

        const BoxStats st = BoxStats::fromValues(data);
        auto* mLine = plot.addVerticalLine(st.mean + xOffset, QColor(230, 80, 60));
        mLine->line.width = 2;
        auto* dLine = plot.addVerticalLine(st.median + xOffset, QColor(20, 30, 120));
        dLine->line.width = 2;
        dLine->line.pattern = LinePattern::Dashed;
        return QPair<double, double>(st.mean, st.median);
    };

    const QPair<double, double> l = addPanel(left, 0.0, QColor(140, 190, 70));
    const QPair<double, double> r = addPanel(right, 5.0, QColor(70, 130, 200));

    plot.addText(QStringLiteral("左偏：平均 %1 < 中位數 %2")
                     .arg(l.first, 0, 'f', 2).arg(l.second, 0, 'f', 2),
                 2.0, 0.95)->alignment = Qt::AlignHCenter | Qt::AlignBottom;
    plot.addText(QStringLiteral("右偏：平均 %1 > 中位數 %2")
                     .arg(r.first, 0, 'f', 2).arg(r.second, 0, 'f', 2),
                 7.0, 0.95)->alignment = Qt::AlignHCenter | Qt::AlignBottom;

    plot.leftAxis()->label.text = QStringLiteral("密度");
    plot.title.text = QStringLiteral("尾巴把平均數拉走");
    plot.setAxisLimits(-0.3, 9.3, 0, 1.08);
}

QTPLOT_RECIPE("進階", "漸層與堆疊", "含負值的堆疊面積圖",
              "addStackedArea 正值往上疊、負值往下疊，兩邊各自累積。\n"
              "不能直接用「累積和」：有負值的時候，那一層會把它上面所有層一起"
              "往下拉，層與層之間還會互相穿越，畫出來的面積就不再是各層的值了。"
              "每個 x 各自維護「正累積」和「負累積」才對。") {
    const QStringList regions{QStringLiteral("東北"), QStringLiteral("中西"),
                              QStringLiteral("南部"), QStringLiteral("西部")};
    const QVector<QColor> colors{QColor(140, 200, 240), QColor(50, 130, 215),
                                 QColor(230, 110, 40), QColor(250, 215, 110)};

    QVector<double> years;
    for (int y = 1996; y <= 2015; ++y) years.append(y);

    // 西部在 2001 年之後轉虧，南部在 2010 年之後也跌到 0 附近
    QVector<QVector<double>> series(4);
    gen::setSeed(14);
    for (int i = 0; i < years.size(); ++i) {
        const double t = i / 19.0;
        series[0].append(85 + 60 * t + gen::randomNormal(1, 0, 12).first());
        series[1].append(80 - 30 * t + gen::randomNormal(1, 0, 14).first());
        series[2].append(i < 14 ? 110 + gen::randomNormal(1, 0, 25).first()
                                : std::max(0.0, 40.0 - (i - 14) * 12.0));
        series[3].append(i < 5 ? 150 - i * 32 : -(30 + (i - 5) * 9 +
                                                  gen::randomNormal(1, 0, 18).first()));
    }

    const QVector<FillY*> bands = plot.addStackedArea(years, series, colors);
    for (int k = 0; k < bands.size(); ++k) bands[k]->legendText = regions[k];

    plot.addHorizontalLine(0)->line = LineStyle(QColor(90, 90, 90), 1.2);
    plot.leftAxis()->label.text = QStringLiteral("利潤");
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignTop;
    plot.title.text = QStringLiteral("各區利潤趨勢");
    plot.bottomAxis()->tickGenerator = std::make_shared<AutomaticTickGenerator>();
}

QTPLOT_RECIPE("進階", "漸層與堆疊", "堆疊面積加原始曲線",
              "堆疊面積只看得出「總量」和「每層的厚度」，看不出各層自己的走勢 —— "
              "一層的厚度會被它下面所有層的起伏推上推下。\n"
              "把各層的原始值另外用細線畫在同一張圖上，兩個問題就一起解決了："
              "面積看總量與組成，線看各層自己的趨勢。") {
    gen::setSeed(27);
    const int n = 25;
    const QStringList names{QStringLiteral("A 類"), QStringLiteral("B 類"),
                            QStringLiteral("C 類"), QStringLiteral("D 類")};
    const QVector<QColor> colors{QColor(45, 125, 170), QColor(245, 150, 40),
                                 QColor(175, 210, 240), QColor(110, 110, 110)};

    QVector<double> xs;
    for (int i = 0; i < n; ++i) xs.append(1986 + i);

    QVector<QVector<double>> series(4);
    for (int i = 0; i < n; ++i) {
        const double wave = std::sin(i * 0.55) * 0.35 + 1.0;
        series[0].append((0.85 + 0.05 * i) * wave * 1e6);
        series[1].append((1.1 + 0.03 * i) * wave * 1e6);
        series[2].append((1.0 - 0.01 * i) * wave * 1e6);
        series[3].append((0.55 - 0.012 * i) * wave * 1e6);
    }

    const QVector<FillY*> bands = plot.addStackedArea(xs, series, colors);
    for (int k = 0; k < bands.size(); ++k) {
        bands[k]->legendText = names[k];
        bands[k]->line = LineStyle(colors[k].darker(130), 1.0);
    }

    // 各層自己的值：細線，顏色和對應的面積一致
    for (int k = 0; k < series.size(); ++k) {
        auto* line = plot.addScatter(xs, series[k], colors[k].darker(120));
        line->marker = MarkerStyle::none();
        line->line.width = 1.4;
    }

    plot.leftAxis()->label.text = QStringLiteral("金額");
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
    plot.bottomAxis()->tickGenerator = std::make_shared<AutomaticTickGenerator>();
}

QTPLOT_RECIPE("進階", "漸層與堆疊", "地平線圖（Horizon）",
              "把一條序列「折疊」成幾層：值域切成 bands 層，每層都畫在同一個高度裡，"
              "愈高的層顏色愈深；負值鏡射回來、換一組顏色。\n"
              "用途是在很矮的高度裡塞進一條序列，好幾條疊起來比較形狀 —— "
              "這裡六條指標疊在一起，用一般折線圖得佔六倍的高度。\n"
              "代價是讀不出數值，只能比深淺與形狀；層數超過三四層顏色也分不出來了。") {
    gen::setSeed(77);
    const int n = 220;
    const QStringList names{QStringLiteral("CPU"), QStringLiteral("記憶體"),
                            QStringLiteral("磁碟"), QStringLiteral("網路"),
                            QStringLiteral("延遲"), QStringLiteral("錯誤率")};

    QVector<double> xs;
    for (int i = 0; i < n; ++i) xs.append(i);

    const double rowHeight = 1.0;
    for (int k = 0; k < names.size(); ++k) {
        QVector<double> ys;
        double v = 0;
        for (int i = 0; i < n; ++i) {
            // 隨機漫步加一個週期成分，每條的週期不同
            v = v * 0.93 + gen::randomNormal(1, 0, 1.0).first();
            ys.append(v + 2.5 * std::sin((i + k * 30) * 0.06));
        }

        auto* h = plot.addHorizon(xs, ys, 3);
        // 由上往下排，每條佔一格高
        const double bottom = (names.size() - 1 - k) * rowHeight;
        h->extent = CoordinateRect(0, n - 1, bottom, bottom + rowHeight * 0.92);

        auto* label = plot.addText(names[k], -4, bottom + rowHeight * 0.46);
        label->alignment = Qt::AlignRight | Qt::AlignVCenter;
    }

    plot.leftAxis()->isVisible = false;
    plot.grid.isVisible = false;
    plot.setAxisLimits(-26, n + 2, -0.1, names.size() * rowHeight);
    plot.title.text = QStringLiteral("六條指標疊在一起看");
}

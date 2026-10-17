// Specialty.cpp — 標註類與特殊座標系的範例。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

#include <QImage>
#include <QPainter>

using namespace qtplot;

namespace {

/// 做一張有辨識度的測試影像（不依賴外部檔案，範例才能到處跑）。
QImage makeTestImage(int w = 240, int h = 160) {
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    QLinearGradient g(0, 0, w, h);
    g.setColorAt(0, QColor(70, 110, 200));
    g.setColorAt(1, QColor(220, 120, 70));
    p.fillRect(img.rect(), g);
    p.setPen(QPen(QColor(255, 255, 255, 150), 2));
    for (int i = 1; i < 6; ++i) {
        p.drawLine(w * i / 6, 0, w * i / 6, h);
        p.drawLine(0, h * i / 6, w, h * i / 6);
    }
    p.setPen(QPen(Qt::white, 3));
    p.drawEllipse(QPointF(w / 2.0, h / 2.0), w / 5.0, h / 5.0);
    p.end();
    return img;
}

}  // namespace

// ───────────────────────── 標註 ─────────────────────────

QTPLOT_RECIPE("標註", "引線與括號", "引線標註",
              "Callout 把文字和被標註的點分開，中間用引線連回去。"
              "點附近很擠的時候，直接把文字放在點上會蓋住資料。"
              "引線是從文字框的邊緣出發的，不是從中心穿出來。") {
    const QVector<double> xs = gen::consecutive(100);
    const QVector<double> ys = gen::sin(100, 1.0, 2.0);
    auto* s = plot.addScatter(xs, ys);
    s->marker = MarkerStyle::none();

    plot.addCallout(RT("第一個波峰"), 12, 1.0, 35, 1.35);
    plot.addCallout(RT("過零點"), 25, 0.0, 52, 0.55);
    auto* c = plot.addCallout(RT("最低點"), 37, -1.0, 60, -1.3);
    c->label.font.bold = true;
}

QTPLOT_RECIPE("標註", "氣泡標註", "氣泡標註",
              "Tooltip 的尾巴是方框的一部分，不是另外一條引線 —— 框和尾巴合成同一條"
              "外框線、填同一個色。資料點密的時候一塊實心氣泡比細引線好認；"
              "要標很遠的點、或不想遮住資料，改用 Callout。") {
    const QVector<double> xs = gen::consecutive(100);
    const QVector<double> ys = gen::sin(100, 1.0, 2.0);
    auto* s = plot.addScatter(xs, ys);
    s->marker = MarkerStyle::none();

    plot.addTooltip(RT("第一個波峰"), 12, 1.0, 30, 1.4);
    plot.addTooltip(RT("過零點"), 25, 0.0, 48, 0.6);
    auto* t = plot.addTooltip(RT("最低點"), 37, -1.0, 62, -1.3);
    t->label.font.bold = true;

    plot.setAxisLimits(-5, 100, -1.8, 1.8);
}

QTPLOT_RECIPE("標註", "氣泡標註", "尾巴寬度",
              "tailWidth 是比例不是像素：以方框的「短邊」為基準。"
              "用比例是為了讓尾巴跟著框等比例縮放 —— 文字一多框變寬，"
              "固定寬度的尾巴看起來就會過細。"
              "取短邊是因為尾巴可能長在任何一條邊上，用長邊當基準時，"
              "貼在短邊上的尾巴會寬到超出那條邊。") {
    const QVector<double> widths{1.0, 0.7, 0.5, 0.3};
    for (int i = 0; i < widths.size(); ++i) {
        const double y = (widths.size() - 1 - i) * 2.0;
        auto* t = plot.addTooltip(QStringLiteral("tailWidth=%1").arg(widths[i]), 0, y, 3, y + 0.7);
        t->tailWidth = widths[i];
    }
    plot.setAxisLimits(-1.5, 7.5, -1.2, 8.2);
}

QTPLOT_RECIPE("標註", "氣泡標註", "尾巴會自己轉向",
              "尾巴貼在框的哪一邊是算出來的，不用設：框心指向 tip 的方向落在哪條邊上，"
              "尾巴就從那裡長出去。所以 tip 繞著框轉一圈，尾巴會跟著繞，"
              "斜角方向時還會自然地跨過圓角。") {
    const Palette palette(Palette::Category10);
    const int n = 12;
    for (int i = 0; i < n; ++i) {
        const double deg = 360.0 * i / n;
        // 全部的尾巴都指向原點，方框排在半徑 1 的圓上
        const Coordinates at = fromPolar(1.0, deg);
        auto* t = plot.addTooltip(QStringLiteral("%1°").arg(deg, 0, 'f', 0), 0, 0, at.x, at.y);

        QColor c = palette.color(i);
        c.setAlpha(190);
        t->background = FillStyle(c);
        t->border.color = c.darker(130);
        t->label.font.bold = true;
        t->tailWidth = 0.6;
    }
    plot.setAxisLimits(-1.5, 1.5, -1.5, 1.5);
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("標註", "引線與括號", "區間括號",
              "Bracket 標出「從這裡到那裡」的一段。tipLength 的正負號決定鬚往哪一側伸，"
              "文字會自動放在開口的另一側。") {
    plot.addBars({12, 19, 24, 21, 9});
    plot.setAxisLimits(-0.8, 4.8, 0, 32);

    auto* b1 = plot.addBracket(RT("成長期"), 0, 27, 2, 27);
    b1->tipLength = 8;

    auto* b2 = plot.addBracket(RT("衰退"), 3, 27, 4, 27);
    b2->tipLength = 8;

    auto* v = plot.addBracket(RT("差距 12"), 4.55, 9, 4.55, 21);
    v->horizontal = false;
    v->tipLength = -8;
}

QTPLOT_RECIPE("標註", "比例尺與影像", "影像與比例尺",
              "ImageRect 把現成的 QImage 畫在資料座標的矩形裡，常當底圖。"
              "ScaleBar 釘在角落，長度代表固定的資料距離 —— "
              "影像或地圖類的圖通常不畫座標軸，改用比例尺說明尺度。") {
    plot.addImage(makeTestImage(), 0, 240, 0, 160);
    plot.equalAxisScale = true;

    // 比例尺是釘在「繪圖區」的角落，不是影像的角落。equalAxisScale 會把繪圖區
    // 撐得比影像寬，所以角落可能落在影像外的空白上 —— 顏色要選在兩種底色上都讀得到的。
    auto* bar = plot.addScaleBar(50, 30);
    bar->widthLabel = QStringLiteral("50 µm");
    bar->heightLabel = QStringLiteral("30 µm");
    bar->line.width = 3;
    bar->line.color = QColor(20, 20, 20);
    bar->font.color = QColor(20, 20, 20);
    bar->font.bold = true;

    // 在影像上標出感興趣的位置
    auto* m = plot.addMarkers({120}, {80}, QColor(255, 255, 0));
    m->marker.shape = MarkerShape::OpenCircle;
    m->marker.size = 18;
    m->marker.outline.width = 2;
    plot.addCallout(RT("目標"), 120, 80, 185, 135);

    plot.bottomAxis()->isVisible = false;
    plot.leftAxis()->isVisible = false;
    plot.grid.isVisible = false;
}

QTPLOT_RECIPE("標註", "比例尺與影像", "保持長寬比",
              "keepAspectRatio 讓影像在指定的矩形內置中，不被拉伸。"
              "左邊關掉（影像被壓扁），右邊打開。") {
    const QImage img = makeTestImage();

    auto* stretched = plot.addImage(img, 0, 100, 0, 100);
    stretched->keepAspectRatio = false;
    stretched->border = LineStyle(QColor(200, 0, 0), 1.5);

    auto* fitted = plot.addImage(img, 120, 220, 0, 100);
    fitted->keepAspectRatio = true;
    fitted->border = LineStyle(QColor(0, 150, 0), 1.5);

    plot.addText(QStringLiteral("keepAspectRatio = false"), 50, -8)->alignment =
        Qt::AlignHCenter | Qt::AlignTop;
    plot.addText(QStringLiteral("keepAspectRatio = true"), 170, -8)->alignment =
        Qt::AlignHCenter | Qt::AlignTop;
    plot.equalAxisScale = true;
}

// ───────────────────── 特殊座標 ─────────────────────

QTPLOT_RECIPE("特殊座標", "三角圖", "三角圖",
              "三個成分加起來固定為 100%。土壤分類、合金成分、地質都用這種圖。"
              "TriangularAxis 只畫座標系，資料用 toCartesian() 換算後拿一般的 Markers 畫。") {
    plot.addTriangularAxis({RT("砂"), RT("粉砂"), RT("黏土")});

    // 幾組土壤樣本（砂 / 粉砂 / 黏土）
    const QVector<QVector<double>> samples{
        {70, 20, 10}, {20, 60, 20}, {15, 25, 60}, {45, 35, 20}, {33, 33, 34}, {80, 10, 10}};
    QVector<double> xs, ys;
    for (const auto& s : samples) {
        const Coordinates c = TriangularAxis::toCartesian(s[0], s[1], s[2]);
        xs.append(c.x);
        ys.append(c.y);
    }
    auto* m = plot.addMarkers(xs, ys, QColor(214, 39, 40));
    m->marker.size = 10;
    plot.title.text = RT("土壤成分");
}

QTPLOT_RECIPE("特殊座標", "三角圖", "三角圖上的分區",
              "用 Shape 的多邊形在三角圖上框出分類區域，再把樣本點疊上去。") {
    plot.addTriangularAxis({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});

    auto corner = [](double a, double b, double c) { return TriangularAxis::toCartesian(a, b, c); };
    // 中央的「均衡」區
    plot.addPolygon({corner(50, 25, 25), corner(25, 50, 25), corner(25, 25, 50)})->legendText =
        RT("均衡區");
    // 靠近 A 角的「A 主導」區
    plot.addPolygon({corner(100, 0, 0), corner(60, 40, 0), corner(60, 0, 40)})->legendText =
        RT("A 主導");

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignTop;
}

QTPLOT_RECIPE("特殊座標", "史密斯圖", "史密斯圖",
              "射頻阻抗匹配用。等電阻圓的圓心在 (r/(1+r), 0)、半徑 1/(1+r)；"
              "等電抗弧的圓心在 (1, 1/x)、半徑 |1/x|，只取落在單位圓內的那一段。"
              "資料用 toCartesian(r, x) 把正規化阻抗換成反射係數座標。") {
    plot.addSmithChart();

    // 一條匹配路徑：從 0.3 + j0.5 經過串聯電感再並聯電容走到圓心
    QVector<double> xs, ys;
    const QVector<QPair<double, double>> path{
        {0.3, 0.5}, {0.3, 0.9}, {0.4, 1.2}, {0.6, 1.3}, {1.0, 1.0}, {1.0, 0.5}, {1.0, 0.0}};
    for (const auto& z : path) {
        const Coordinates c = SmithChartAxis::toCartesian(z.first, z.second);
        xs.append(c.x);
        ys.append(c.y);
    }
    auto* s = plot.addScatter(xs, ys, QColor(214, 39, 40));
    s->line.width = 2;
    s->marker = MarkerStyle(MarkerShape::FilledCircle, 7, QColor(214, 39, 40));
    s->legendText = RT("匹配路徑");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("特殊座標", "相量圖", "相量圖",
              "交流電的電壓電流關係。showProjections 畫出每個相量在實軸與虛軸上的投影，"
              "看得出各分量的大小。") {
    auto* grid = plot.addPolarAxis(1.2, 12);
    grid->showRingLabels = false;
    // 相量用的是「0° 指向右、逆時針為正」的絕對角度，
    // 把極座標軸也設成同一套，刻度標籤才會對得上
    grid->startAngle = 0;
    grid->clockwise = false;

    auto* ph = plot.addPhasors({
        {1.0, 0, QColor(31, 119, 180), QStringLiteral("V")},
        {0.8, -30, QColor(214, 39, 40), QStringLiteral("I")},
        {0.45, 60, QColor(44, 160, 44), QStringLiteral("I_C")},
    });
    ph->showProjections = true;
    plot.title.text = RT("電壓與電流（I 落後 V 30°）");
}

// ───────────────────── 不規則三角網 ─────────────────────

QTPLOT_RECIPE("特殊座標", "三角網", "TIN（不規則三角網）",
              "散佈的取樣點用 Delaunay 三角化之後上色。"
              "和 Heatmap 的差別是資料不必落在規則格點上 —— "
              "地形測量、感測器佈點本來就是散的，內插成格點反而會造假。") {
    gen::setSeed(23);
    const QVector<double> xs = gen::randomUniform(120, 0, 10);
    const QVector<double> ys = gen::randomUniform(120, 0, 10);
    QVector<Coordinates> pts;
    QVector<double> zs;
    for (int i = 0; i < xs.size(); ++i) {
        pts.append({xs[i], ys[i]});
        // 一個有兩個高點的地形
        const double a = std::exp(-(std::pow(xs[i] - 3, 2) + std::pow(ys[i] - 7, 2)) / 8);
        const double b = std::exp(-(std::pow(xs[i] - 7, 2) + std::pow(ys[i] - 3, 2)) / 12);
        zs.append(100 * a + 70 * b);
    }
    auto* tin = plot.addTIN(pts, zs);
    tin->colormap = Colormap(Colormap::Viridis);
    tin->marker = MarkerStyle(MarkerShape::FilledCircle, 3, QColor(0, 0, 0, 140));
    plot.equalAxisScale = true;
    plot.title.text = RT("120 個散佈取樣點的地形");
}

QTPLOT_RECIPE("特殊座標", "三角網", "三角網線框",
              "關掉填色只看三角化本身。Delaunay 的特性是盡量避免細長的三角形 —— "
              "任何一點都不會落在其他三角形的外接圓內。") {
    gen::setSeed(5);
    const QVector<double> xs = gen::randomUniform(60, 0, 10);
    const QVector<double> ys = gen::randomUniform(60, 0, 10);
    QVector<Coordinates> pts;
    QVector<double> zs;
    for (int i = 0; i < xs.size(); ++i) {
        pts.append({xs[i], ys[i]});
        zs.append(0.0);
    }
    auto* tin = plot.addTIN(pts, zs);
    tin->filled = false;
    tin->wireframe = LineStyle(QColor(31, 119, 180), 1.0);
    tin->marker = MarkerStyle(MarkerShape::FilledCircle, 5, QColor(214, 39, 40));
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("特殊座標", "史密斯圖", "標出幾個阻抗點",
              "圓圖上的每一點都是一個正規化阻抗。四個角落的特例值得記住："
              "圓心是 1 + j0（完美匹配）、最右是開路、最左是短路、"
              "圓周上一圈都是 |Γ| = 1（純電抗，全反射）。") {
    auto* sm = plot.addSmithChart();
    sm->labelFont.size = 8;

    const QVector<QPair<QPair<double, double>, QString>> points{
        {{3.0, 1.0}, QStringLiteral("3.0 + j1.0")},
        {{1.0, -0.5}, QStringLiteral("1.0 - j0.5")},
        {{0.2, 0.0}, QStringLiteral("0.2 + j0")},
        {{0.0, 0.7}, QStringLiteral("0 + j0.7")},
        {{0.0, -1.6}, QStringLiteral("0 - j1.6")},
    };
    for (const auto& pt : points) {
        const Coordinates c = sm->coordinates(pt.first.first, pt.first.second);
        auto* m = plot.addMarkers({c.x}, {c.y});
        m->marker = MarkerStyle(MarkerShape::FilledCircle, 13, QColor(220, 30, 30));

        auto* txt = plot.addText(pt.second, c.x, c.y);
        txt->label.font.color = QColor(200, 20, 20);
        txt->label.font.bold = true;
        txt->label.font.size = 10;
        // 貼在圓周上的點（純電抗）要把文字往內放：外面是電抗刻度的地盤，
        // 往外推會和它們撞在一起。圖內的點則往外推，避開格線密集處。
        const double len = std::hypot(c.x, c.y);
        const bool onRim = len > 0.92;
        const double ux = (len > 1e-9) ? c.x / len : 1.0;
        const double dir = onRim ? -1.0 : 1.0;
        txt->offsetX = (ux >= 0 ? 12 : -12) * dir;
        txt->offsetY = onRim ? 14 : -14;
        txt->alignment = ((ux >= 0) == onRim ? Qt::AlignRight : Qt::AlignLeft) |
                         Qt::AlignVCenter;
    }
    plot.equalAxisScale = true;
}

QTPLOT_RECIPE("特殊座標", "史密斯圖", "等駐波比圓",
              "vswrCircles 畫出以圓心為心的同心圓。給的是 VSWR 而不是 |Γ| —— "
              "射頻量測讀的是 VSWR，換算 |Γ| = (S-1)/(S+1) 由圖元自己做。\n"
              "阻抗落在哪個圈裡，就代表駐波比不會超過那個值。") {
    auto* sm = plot.addSmithChart();
    sm->vswrCircles = {1.5, 2.0, 3.0, 5.0};
    sm->labelFont.size = 8;
    sm->showReactanceLabels = false;
    sm->minorGridLine = LineStyle::none();  // 留白一點，條紋才看得清楚

    // 幾個落在不同圈裡的阻抗
    const QVector<QPair<double, double>> zs{{1.0, 0.0}, {1.6, 0.4}, {0.5, -0.6}, {2.6, 1.4}};
    QVector<double> xs, ys;
    for (const auto& z : zs) {
        const Coordinates c = sm->coordinates(z.first, z.second);
        xs.append(c.x);
        ys.append(c.y);
    }
    auto* m = plot.addMarkers(xs, ys);
    m->marker = MarkerStyle(MarkerShape::FilledDiamond, 10, QColor(30, 90, 200));

    plot.title.text = RT("虛線由內而外：VSWR 1.5 / 2 / 3 / 5");
    plot.equalAxisScale = true;
}

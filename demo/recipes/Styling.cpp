// Styling.cpp — 配色、線型、標記、格線、圖例的範例。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

QTPLOT_RECIPE("樣式", "主題與配色", "深色主題",
              "PlotStyle::dark() 會一次換掉底色、軸色、文字色與調色盤。"
              "setStyle 會把顏色套到已存在的軸上。") {
    plot.setStyle(PlotStyle::dark());
    const QVector<double> xs = gen::consecutive(100);
    for (int i = 0; i < 4; ++i) {
        auto* s = plot.addScatter(xs, gen::sin(100, 1.0, 2.0, i * 0.1));
        s->marker = MarkerStyle::none();
        s->legendText = QStringLiteral("相位 %1").arg(i);
    }
    plot.legend.isVisible = true;
    plot.title.text = QStringLiteral("深色主題");
}

QTPLOT_RECIPE("樣式", "主題與配色", "調色盤一覽",
              "六組內建調色盤。改 plot.style.palette 之後新加的圖元就會用新的顏色。") {
    const QStringList names = Palette::presetNames();
    QVector<Bar> bars;
    for (int pi = 0; pi < names.size(); ++pi) {
        const Palette pal = Palette::fromPresetName(names[pi]);
        for (int ci = 0; ci < 10; ++ci) {
            Bar b;
            b.position = ci;
            b.valueBase = pi;
            b.value = pi + 0.8;
            b.size = 0.9;
            b.fillColor = pal.color(ci);
            bars.append(b);
        }
    }
    plot.addBars(bars);
    auto manual = std::make_shared<ManualTickGenerator>();
    for (int pi = 0; pi < names.size(); ++pi)
        manual->ticks.append({pi + 0.4, names[pi], true});
    plot.leftAxis()->tickGenerator = manual;
    plot.bottomAxis()->label.text = QStringLiteral("顏色編號");
}

QTPLOT_RECIPE("樣式", "主題與配色", "色階一覽",
              "七組連續色階，Heatmap 與等值線用。") {
    const QStringList names = Colormap::presetNames();
    const int cols = 64;
    // 每一列要用不同色階，所以一列畫一張熱圖
    for (int r = 0; r < names.size(); ++r) {
        QVector<double> row;
        row.reserve(cols);
        for (int c = 0; c < cols; ++c) row.append(c);
        auto* h = plot.addHeatmap(row, 1, cols);
        h->colormap = Colormap::fromPresetName(names[r]);
        h->extent = CoordinateRect(0, cols, r, r + 0.85);
    }
    auto manual = std::make_shared<ManualTickGenerator>();
    for (int r = 0; r < names.size(); ++r) manual->ticks.append({r + 0.42, names[r], true});
    plot.leftAxis()->tickGenerator = manual;
    plot.setAxisLimits(0, cols, 0, names.size());
}

QTPLOT_RECIPE("樣式", "線與標記", "線型",
              "六種虛線樣式。間距以線寬的倍數指定，所以線加粗之後虛線不會糊掉。"
              "Dashed 與 DenselyDashed 的差別只在間距，線很短的時候才看得出來。") {
    const QVector<LinePattern> patterns{LinePattern::Solid,   LinePattern::Dashed,
                                        LinePattern::DenselyDashed, LinePattern::Dotted,
                                        LinePattern::DashDot, LinePattern::DashDotDot};
    const QStringList names{QStringLiteral("Solid"),   QStringLiteral("Dashed"),
                            QStringLiteral("DenselyDashed"), QStringLiteral("Dotted"),
                            QStringLiteral("DashDot"), QStringLiteral("DashDotDot")};
    const Palette palette(Palette::Category10);
    for (int i = 0; i < patterns.size(); ++i) {
        // 由上往下排，順序才和下面的名稱清單一致
        const double y = patterns.size() - 1 - i;
        const QColor c = palette.color(i);
        auto* s = plot.addLine(0, y, 7, y);
        s->line.pattern = patterns[i];
        s->line.width = 2;
        s->line.color = c;

        // 名稱直接寫在線的右邊、用同一個顏色。比圖例好讀 ——
        // 讀者不用在圖例和線之間來回對照顏色。
        auto* t = plot.addText(names[i], 7.3, y);
        t->label.font.color = c;
        t->label.font.bold = true;
        t->alignment = Qt::AlignLeft | Qt::AlignVCenter;
    }
    plot.setAxisLimits(-0.3, 12.5, -0.7, patterns.size() - 0.3);
}

QTPLOT_RECIPE("樣式", "線與標記", "標記形狀",
              "十六種標記。Filled* 會填色，Open* 只畫外框，其餘是線條型。") {
    const QVector<MarkerShape> shapes{
        MarkerShape::FilledCircle, MarkerShape::OpenCircle,
        MarkerShape::FilledSquare, MarkerShape::OpenSquare,
        MarkerShape::FilledTriangleUp, MarkerShape::OpenTriangleUp,
        MarkerShape::FilledTriangleDown, MarkerShape::OpenTriangleDown,
        MarkerShape::FilledDiamond, MarkerShape::OpenDiamond,
        MarkerShape::Plus, MarkerShape::Asterisk,
        MarkerShape::HashTag, MarkerShape::Eks,
        MarkerShape::VerticalBar, MarkerShape::HorizontalBar};
    for (int i = 0; i < shapes.size(); ++i) {
        const int row = i / 6;
        const int col = i % 6;
        auto* m = plot.addMarkers({double(col)}, {double(-row)},
                                  plot.style.palette.color(i));
        m->marker.shape = shapes[i];
        m->marker.size = 14;
        m->marker.outline.width = 1.5;
    }
    plot.setAxisLimits(-1, 6, -3, 1);
    plot.grid.isVisible = false;
}

QTPLOT_RECIPE("樣式", "線與標記", "填色花紋",
              "FillStyle 除了純色，也可以用花紋（列印黑白稿時很有用）。") {
    const QVector<HatchPattern> hatches{HatchPattern::Horizontal, HatchPattern::Vertical,
                                        HatchPattern::DiagonalUp, HatchPattern::DiagonalDown,
                                        HatchPattern::Checker, HatchPattern::Dots};
    for (int i = 0; i < hatches.size(); ++i) {
        auto* s = plot.addRectangle(i, i + 0.85, 0, 1);
        s->fill.color = QColor(0, 0, 0, 0);
        s->fill.hatch = hatches[i];
        s->fill.hatchColor = plot.style.palette.color(i);
        s->line = LineStyle(QColor(80, 80, 80), 1.0);
    }
    plot.setAxisLimits(-0.3, hatches.size(), -0.1, 1.2);
}

QTPLOT_RECIPE("樣式", "格線與圖例", "格線",
              "主格線預設開、次格線預設關。onTop 可以把格線畫到圖元之上。") {
    plot.addHeatmap(gen::gaussian2D(20, 20, 0.3), 20, 20);
    plot.grid.minorLine = LineStyle(QColor(255, 255, 255, 60), 1.0);
    plot.grid.majorLine = LineStyle(QColor(255, 255, 255, 120), 1.0);
    plot.grid.onTop = true;
    plot.addAnnotation(QStringLiteral("grid.onTop = true"));
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例位置與方向",
              "alignment 決定圖例貼哪個角，horizontal 可以排成一列。") {
    const QVector<double> xs = gen::consecutive(50);
    for (int i = 0; i < 3; ++i) {
        auto* s = plot.addScatter(xs, gen::sin(50, 1.0, 1.0, i * 0.15));
        s->marker = MarkerStyle::none();
        s->legendText = QStringLiteral("曲線 %1").arg(i + 1);
    }
    plot.legend.isVisible = true;
    plot.legend.horizontal = true;
    plot.legend.alignment = Qt::AlignHCenter | Qt::AlignBottom;
}

// ───────────────────── 格線的細節 ─────────────────────

namespace {

/// 兩條反相的正弦，當格線範例的背景資料。
void addTwoWaves(Plot& plot) {
    const QVector<double> xs = gen::consecutive(51);
    auto* a = plot.addScatter(xs, gen::sin(51, 1.0, 1.0));
    a->marker = MarkerStyle::none();
    auto* b = plot.addScatter(xs, gen::cos(51, 1.0, 1.0));
    b->marker = MarkerStyle::none();
    plot.setAxisLimits(0, 50, -1.1, 1.1);
}

}  // namespace

QTPLOT_RECIPE("樣式", "格線與圖例", "格線的樣式",
              "plot.grid 的 majorLine 與 minorLine 會套到所有軸上。"
              "次格線預設是關的（透明色），給它顏色就會畫出來。") {
    addTwoWaves(plot);
    plot.grid.majorLine = LineStyle(QColor(90, 150, 70, 220), 1.0);
    plot.grid.minorLine = LineStyle(QColor(90, 150, 70, 90), 1.0);
    plot.leftAxis()->tickGenerator = std::make_shared<AutomaticTickGenerator>();
}

QTPLOT_RECIPE("樣式", "格線與圖例", "逐軸設定格線",
              "每根軸都可以用 gridMajorLine / gridMinorLine 蓋掉 plot.grid 的設定。"
              "這兩個欄位是 optional 而不是「透明色代表沿用」—— "
              "透明色本身是個有意義的設定（這根軸不要畫格線），兩者必須分得開。") {
    addTwoWaves(plot);
    // 橫向格線：穩重的綠色實線
    plot.leftAxis()->gridMajorLine = LineStyle(QColor(110, 150, 110), 1.0);
    // 縱向格線不畫線，改成下一支範例的填色條帶
    plot.bottomAxis()->gridMajorLine = LineStyle::none();
    plot.bottomAxis()->gridFillColor1 = QColor(0, 0, 0, 0);
    plot.bottomAxis()->gridFillColor2 = QColor(250, 225, 245);
}

QTPLOT_RECIPE("樣式", "格線與圖例", "格線的交替填色",
              "gridFillColor1 / gridFillColor2 會把相鄰兩條主格線之間的區域交替填色。"
              "兩根軸都設的話條帶會交叉疊成格子，所以要用半透明色 —— "
              "不透明的話後畫的那根會把前一根整個蓋掉。") {
    addTwoWaves(plot);
    plot.bottomAxis()->gridFillColor1 = QColor(0, 0, 0, 12);
    plot.bottomAxis()->gridFillColor2 = QColor(0, 0, 0, 0);
    plot.leftAxis()->gridFillColor1 = QColor(0, 0, 0, 12);
    plot.leftAxis()->gridFillColor2 = QColor(0, 0, 0, 0);
    plot.grid.majorLine = LineStyle(QColor(0, 0, 0, 35), 1.0);
    plot.grid.minorLine = LineStyle(QColor(0, 0, 0, 18), 1.0);
}

QTPLOT_RECIPE("樣式", "格線與圖例", "深色主題的格線填色",
              "深色底上條帶要用「比底色亮一點」的半透明白，而不是加深 —— "
              "在接近純黑的底色上再加黑是看不出來的。") {
    plot.setStyle(PlotStyle::dark());
    const int n = 1000;
    QVector<double> xs(n), ys(n);
    for (int i = 0; i < n; ++i) {
        xs[i] = i;
        // 方波的前幾項傅立葉級數：看得到吉布斯現象的那些小振盪
        double v = 0;
        for (int k = 1; k <= 9; k += 2) v += std::sin(2 * kPi * k * i / 500.0) / k;
        ys[i] = v;
    }
    // 正規化成峰值 1。截斷的級數會在跳變處衝過頭（吉布斯現象），
    // 直接乘 4/pi 的話峰值會超過 1，上下兩排小振盪就被視野切掉
    double peak = 0;
    for (double v : ys) peak = std::max(peak, std::abs(v));
    if (peak > 0)
        for (double& v : ys) v /= peak;

    auto* s = plot.addScatter(xs, ys, QColor(60, 200, 60));
    s->marker = MarkerStyle::none();
    s->line.width = 2;

    plot.bottomAxis()->gridFillColor1 = QColor(255, 255, 255, 14);
    plot.bottomAxis()->gridFillColor2 = QColor(0, 0, 0, 0);
    plot.leftAxis()->gridFillColor1 = QColor(255, 255, 255, 14);
    plot.leftAxis()->gridFillColor2 = QColor(0, 0, 0, 0);
    plot.grid.majorLine = LineStyle(QColor(255, 255, 255, 40), 1.0);
    plot.setAxisLimits(0, 1000, -1.15, 1.15);
}

// ───────────────────────── 文字 ─────────────────────────

QTPLOT_RECIPE("樣式", "文字", "字型的每一個欄位",
              "FontStyle 控制一處文字的全部外觀：家族、字級、字重、斜體、底線、"
              "刪除線、字距、橫向拉伸、角度、顏色。\n"
              "字重和 bold 是兩個欄位：weight 預設 Auto，也就是看 bold；"
              "一旦把 weight 設成別的值就蓋過 bold。兩個都在是為了相容，"
              "但順序講死了才不會出現「我設了 Light 怎麼還是粗的」。\n"
              "要注意大多數字型家族其實只裝了 Normal 和 Bold 兩個字重 —— "
              "這張圖上 Bold 和 Black 看起來一模一樣就是這個原因，"
              "中間與更重的那幾級能不能畫出差別，完全看手上的字型裝了什麼。") {
    struct Row { QString text; std::function<void(FontStyle&)> apply; };
    const QVector<Row> rows{
        {QStringLiteral("預設"), [](FontStyle&) {}},
        {QStringLiteral("size = 20"), [](FontStyle& f) { f.size = 20; }},
        {QStringLiteral("weight = Light"), [](FontStyle& f) { f.weight = FontStyle::Light; }},
        {QStringLiteral("weight = Bold"), [](FontStyle& f) { f.weight = FontStyle::Bold; }},
        {QStringLiteral("weight = Black"), [](FontStyle& f) { f.weight = FontStyle::Black; }},
        {QStringLiteral("italic"), [](FontStyle& f) { f.italic = true; }},
        {QStringLiteral("underline"), [](FontStyle& f) { f.underline = true; }},
        {QStringLiteral("strikeOut"), [](FontStyle& f) { f.strikeOut = true; }},
        {QStringLiteral("letterSpacing = 4"), [](FontStyle& f) { f.letterSpacing = 4; }},
        {QStringLiteral("letterSpacing = -1.5"), [](FontStyle& f) { f.letterSpacing = -1.5; }},
        {QStringLiteral("stretch = 60（扁）"), [](FontStyle& f) { f.stretch = 60; }},
        {QStringLiteral("stretch = 170（寬）"), [](FontStyle& f) { f.stretch = 170; }},
        {QStringLiteral("rotation = -15"), [](FontStyle& f) { f.rotation = -15; }},
        {QStringLiteral("family = Courier New"),
         [](FontStyle& f) { f.family = QStringLiteral("Courier New"); }},
    };

    // 列距給 1.4 而不是 1：轉過角度的那一列在垂直方向佔的高度是平放的好幾倍，
    // 照一列一單位排會壓到隔壁。
    const double spacing = 1.4;
    const double top = rows.size() * spacing;
    for (int i = 0; i < rows.size(); ++i) {
        const double y = top - i * spacing;
        auto* name = plot.addText(rows[i].text, 0.4, y);
        name->alignment = Qt::AlignLeft | Qt::AlignVCenter;
        name->label.font.size = 9;
        name->label.font.color = QColor(120, 120, 120);

        auto* sample = plot.addText(QStringLiteral("Sample 範例 123"), 5.2, y);
        sample->alignment = Qt::AlignLeft | Qt::AlignVCenter;
        sample->label.font.size = 13;
        rows[i].apply(sample->label.font);
    }

    plot.bottomAxis()->isVisible = false;
    plot.leftAxis()->isVisible = false;
    plot.grid.isVisible = false;
    plot.title.text = QStringLiteral("FontStyle 的每一個欄位");
    plot.setAxisLimits(0, 11, top - rows.size() * spacing, top + spacing);
}

QTPLOT_RECIPE("樣式", "文字", "一次改掉整張圖的字",
              "圖上的文字散在很多地方：標題、四根軸的軸名與刻度、圖例、"
              "每個圖元自己的標籤。要全部換成同一套字型，一個一個設會漏。\n"
              "Plot::visitFonts() 走過其中每一處，setFontFamily()／scaleFontSizes()／"
              "setFontWeight() 都是它的包裝。這張圖的每一處文字都是用 scaleFontSizes(1.35) "
              "加 setFontFamily() 一次設定的。\n"
              "字級用「乘的」而不是「全部設成同一個數字」：標題本來就該比刻度大，"
              "一律設成同一個字級會把這個層次壓掉。\n"
              "注意順序 —— 圖元是在加進來之後才被走到的，所以要先加圖元再改字型。") {
    gen::setSeed(4);
    plot.addScatter(gen::consecutive(12), gen::randomWalk(12, 1.0, 10))
        ->legendText = QStringLiteral("A 線");
    plot.addScatter(gen::consecutive(12), gen::randomWalk(12, 1.0, 6))
        ->legendText = QStringLiteral("B 線");
    auto* bars = plot.addBars({4, 7, 5, 8, 6});
    for (Bar& b : bars->bars) b.label = QString::number(b.value, 'f', 0);
    bars->legendText = QStringLiteral("月產量");

    plot.title.text = QStringLiteral("一次設定好所有文字");
    plot.bottomAxis()->label.text = QStringLiteral("月份");
    plot.leftAxis()->label.text = QStringLiteral("數量");
    plot.legend.isVisible = true;

    // 圖元都加完了才動字型
    plot.setFontFamily(QStringLiteral("Georgia"));
    plot.scaleFontSizes(1.35);

    // 走完整體之後，個別再調仍然有效 —— 兩者不衝突
    plot.title.font.weight = FontStyle::Black;
    plot.title.font.letterSpacing = 2;
}

QTPLOT_RECIPE("樣式", "文字", "只改符合條件的那些字",
              "visitFonts() 收的是一個函式，所以可以挑著改。\n"
              "這裡做兩件事：把所有「原本就是粗體」的文字再放大兩成並染色，"
              "其餘維持原樣。一個一個找哪幾處是粗體要翻遍整份程式，用 visitFonts 兩行就好。\n"
              "setFontColor() 也是同一套：它會保留每一處原本的透明度，"
              "因為次要說明文字是靠調淡來分層的，一律設成不透明會讓它們跟主要文字一樣搶眼。") {
    gen::setSeed(8);
    auto* s = plot.addScatter(gen::consecutive(20), gen::randomWalk(20, 1.0, 50));
    s->marker.shape = MarkerShape::FilledCircle;
    s->legendText = QStringLiteral("量測");

    plot.title.text = QStringLiteral("只有粗體的字被挑出來改");
    plot.title.font.bold = true;
    plot.bottomAxis()->label.text = QStringLiteral("這根軸名是粗體");
    plot.bottomAxis()->label.font.bold = true;
    plot.leftAxis()->label.text = QStringLiteral("這根不是");
    plot.legend.isVisible = true;

    auto* note = plot.addAnnotation(
        QStringLiteral("粗體的被放大 20% 並染紅\n其餘不動"),
        Qt::AlignLeft | Qt::AlignBottom);
    note->label.font.size = 9;

    plot.visitFonts([](FontStyle& f) {
        if (f.effectiveWeight() < FontStyle::DemiBold) return;
        f.size *= 1.2;
        f.color = QColor(190, 50, 45);
    });
}

QTPLOT_RECIPE("樣式", "文字", "轉角度",
              "每一處文字都有 rotation（度，順時針）。\n"
              "刻度標籤是例外：它每次 render 才現產生，沒有一個長期存在的 Label 可以設，"
              "所以角度要設在 Axis::tickLabelRotation 上 —— 版面得先知道角度，"
              "才留得出旋轉後外接框需要的厚度，否則文字會被切掉。"
              "（tickFont.rotation 也會被採用，這樣 setFontRotation() 才不會漏掉刻度。）") {
    const QStringList months{
        QStringLiteral("一月的營收"), QStringLiteral("二月的營收"), QStringLiteral("三月的營收"),
        QStringLiteral("四月的營收"), QStringLiteral("五月的營收"), QStringLiteral("六月的營收")};
    auto* bars = plot.addBars({42, 55, 38, 61, 49, 58});
    for (Bar& b : bars->bars) b.label = QStringLiteral("%1 百萬").arg(b.value, 0, 'f', 0);
    bars->valueLabelFont.rotation = -90;
    bars->valueLabelFont.size = 9;

    auto ticks = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < months.size(); ++i)
        ticks->ticks.append({static_cast<double>(i), months[i], true});
    plot.bottomAxis()->tickGenerator = ticks;
    plot.bottomAxis()->tickLabelRotation = -40;

    plot.leftAxis()->label.text = QStringLiteral("百萬元");
    plot.leftAxis()->label.font.rotation = -90;  // 垂直軸的預設就是這個，寫出來備查
    plot.title.text = QStringLiteral("長標籤轉個角度就擠得下");
    plot.setAxisLimits(-0.7, months.size() - 0.3, 0, 72);
}

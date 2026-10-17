// Supplement4.cpp — 對照 ScottPlot 5 Cookbook 第三次補上的範例（函式庫原本就做得到的）。
//
// 這一批不需要新功能，只是 Cookbook 裡還沒有示範：讀取視野、從色階取色、
// 只用右軸或上軸、自訂刻度產生器、各種標註與十字線的樣式、熱圖的色階變化、
// 財金圖的細節、量表與雷達圖的參數、訊號圖的樣式、史密斯圖的匹配路徑、自訂字型。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

#include <QDateTime>
#include <QFile>
#include <QFontDatabase>
#include <complex>
#include <cmath>

using namespace qtplot;

namespace {

double epoch2024() {
    return static_cast<double>(QDateTime(QDate(2024, 1, 1), QTime(0, 0)).toSecsSinceEpoch());
}

/// 固定間隔的刻度：從 0 開始每 interval 一根，文字加上單位。
/// 示範怎麼自己寫一個 TickGenerator —— 只要回傳一串 Tick 就好。
class FixedIntervalTicks : public TickGenerator {
public:
    double interval = 25;
    QString unit;
    QVector<Tick> generate(const CoordinateRange& range, double, const FontStyle&, bool) override {
        QVector<Tick> out;
        if (!(interval > 0)) return out;
        const double first = std::ceil(std::min(range.min, range.max) / interval) * interval;
        for (double v = first; v <= std::max(range.min, range.max) && out.size() < 200; v += interval)
            out.append({v, QString::number(v) + unit, true});
        return out;
    }
};

/// 把一組類別名稱放到某根軸的刻度上（0, 1, 2, …）。
void categoryTicks(Axis* axis, const QStringList& names) {
    auto m = std::make_shared<ManualTickGenerator>();
    for (int i = 0; i < names.size(); ++i) m->ticks.append({static_cast<double>(i), names[i], true});
    axis->tickGenerator = m;
}

/// 從檔案載入字型，回傳字型家族名稱。檔案不在就回傳空字串（沿用系統字型）。
QString loadFontFile(const QString& path) {
    if (!QFile::exists(path)) return QString();
    const int id = QFontDatabase::addApplicationFont(path);
    const QStringList fams = QFontDatabase::applicationFontFamilies(id);
    return fams.isEmpty() ? QString() : fams.first();
}

}  // namespace

// ───────────────────────────── 入門與座標軸 ─────────────────────────────

QTPLOT_RECIPE("座標軸", "調整範圍", "讀取目前的範圍",
              "plot.axisLimits() 回傳主要兩軸目前的視野；每根軸也可以直接讀 range。"
              "要在加完資料之後才知道範圍，就先 autoScale() 再讀。") {
    plot.addSignal(gen::sin(100, 3, 2));
    plot.autoScale();
    const AxisLimits lim = plot.axisLimits();
    plot.addAnnotation(RT("X: %1 ～ %2\nY: %3 ～ %4")
                           .arg(lim.x.min, 0, 'f', 2).arg(lim.x.max, 0, 'f', 2)
                           .arg(lim.y.min, 0, 'f', 2).arg(lim.y.max, 0, 'f', 2));
}

QTPLOT_RECIPE("樣式", "主題與配色", "從色階取顏色",
              "Colormap::sampleColors(n) 從色階上等距取 n 個顏色。系列很多、又有先後順序"
              "（年份、溫度、劑量）時，用色階上色比調色盤更能表達「漸進」。") {
    const QVector<QColor> colors = Colormap(Colormap::Viridis).sampleColors(10);
    for (int i = 0; i < 10; ++i) {
        auto* s = plot.addSignal(gen::sin(100, 1 + i * 0.2, 1, i * 0.15), 1.0, colors[i]);
        s->line.width = 2;
    }
}

QTPLOT_RECIPE("樣式", "格線與圖例", "格線跟著上方的軸",
              "格線畫在「有刻度標籤」的那幾根軸上。把資料綁到上軸、打開上軸的刻度、"
              "關掉底軸，格線就改跟上軸走。") {
    auto* s = plot.addScatter(gen::linspace(0, 1000, 30), gen::randomUniform(30, 0, 10));
    s->xAxis = plot.topAxis();
    plot.topAxis()->showTickLabels = true;
    plot.topAxis()->label.text = RT("上軸（格線跟著它）");
    plot.bottomAxis()->isVisible = false;
}

QTPLOT_RECIPE("座標軸", "多軸與範圍", "只用右側的 Y 軸",
              "把圖元的 yAxis 換成 rightAxis()，打開右軸的刻度、隱藏左軸。"
              "財金圖、儀表板常把數值放右邊，眼睛從最新的資料（右端）直接看到刻度。") {
    auto* s = plot.addSignal(gen::randomWalk(200, 1, 100));
    s->yAxis = plot.rightAxis();
    plot.rightAxis()->showTickLabels = true;
    plot.rightAxis()->label.text = RT("數值");
    plot.leftAxis()->isVisible = false;
}

QTPLOT_RECIPE("入門", "圖元管理", "清空整張圖",
              "clear() 移除全部圖元，軸與樣式留著。要連 addAxis() 加的軸、標題、圖例一起清掉，"
              "用 reset()。這裡先加了三條線，clear 之後只剩最後加的那條。") {
    for (int i = 0; i < 3; ++i) plot.addSignal(gen::sin(50, 1, 1, i * 0.25));
    plot.clear();
    plot.addSignal(gen::cos(50, 2, 2))->legendText = RT("clear() 之後才加的");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("樣式", "版面與框線", "放大倍率輸出",
              "renderToImage／savePng 的 scale 參數會把整張圖（字、線、標記）一起放大，"
              "版面不變、只是更清晰。左邊是 1 倍、右邊是 3 倍，貼到同樣大小時右邊銳利得多。") {
    Plot small;
    small.addSignal(gen::sin(40, 1, 1))->marker = MarkerStyle(MarkerShape::FilledCircle, 5, QColor(31, 119, 180));
    small.title.text = RT("小圖");
    small.autoScale();  // 沒有放進 PlotWidget 的 Plot 要自己算範圍
    const QImage x1 = small.renderToImage(320, 240, 1.0);
    const QImage x3 = small.renderToImage(320, 240, 3.0);
    plot.addImage(x1, 0, 4, 0, 3);
    plot.addImage(x3, 4.4, 8.4, 0, 3);
    plot.addText(QStringLiteral("scale = 1"), 2, 3.1)->alignment = Qt::AlignHCenter | Qt::AlignBottom;
    plot.addText(QStringLiteral("scale = 3"), 6.4, 3.1)->alignment = Qt::AlignHCenter | Qt::AlignBottom;
    plot.equalAxisScale = true;
    plot.setAxisLimits(-0.3, 8.7, -0.3, 3.6);
}

QTPLOT_RECIPE("樣式", "主題與配色", "色階取出的離散色",
              "sampleColors 取出來的就是一般的 QColor，可以拿去給長條、扇形任何東西上色。"
              "等距取樣保證顏色深淺均勻遞增。") {
    const QVector<QColor> c = Colormap(Colormap::Coolwarm).sampleColors(9);
    QVector<Bar> bars;
    for (int i = 0; i < 9; ++i) {
        Bar b;
        b.position = i;
        b.value = 1 + i * 0.5;
        b.fillColor = c[i];
        bars.append(b);
    }
    plot.addBars(bars);
}

QTPLOT_RECIPE("樣式", "主題與配色", "自己定義的漸層色階",
              "Colormap::fromColors 用幾個節點顏色建一個色階，中間自動內插。"
              "公司色、或想要「綠 → 黃 → 紅」的紅綠燈色階都可以這樣做。") {
    const Colormap map = Colormap::fromColors({QColor(30, 150, 80), QColor(250, 210, 40), QColor(210, 40, 40)});
    auto* f = plot.addFillY(gen::linspace(0, 10, 100), QVector<double>(100, 0), gen::sin(100, 3, 1, 0, 4));
    f->fill = FillStyle::linearGradient(GradientDirection::Horizontal, map.sampleColors(20));
    f->line = LineStyle::none();
}

QTPLOT_RECIPE("樣式", "版面與框線", "隱藏標題但保留文字",
              "把標題的字型顏色設成全透明，文字還在（匯出 SVG 時可以當描述），"
              "版面也不會替它留空間。要恢復只要把顏色改回來。") {
    plot.addSignal(gen::sin(100));
    plot.title.text = RT("這段文字不會畫出來");
    plot.title.font.color = QColor(0, 0, 0, 0);
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "自己寫的刻度產生器",
              "TickGenerator 只有一個函式：給範圍，回傳一串刻度。"
              "這裡寫了一個「每 5 一根、加單位」的產生器。縮放時一樣會重算。") {
    plot.addSignal(gen::randomWalk(300, 1, 50));
    auto t = std::make_shared<FixedIntervalTicks>();
    t->interval = 5;
    t->unit = RT(" 單位");
    plot.leftAxis()->tickGenerator = t;
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "指定日期的刻度",
              "ManualTickGenerator 的位置就是 Unix 秒，所以要把刻度放在特定日期上，"
              "換算成秒就好。這裡只標出幾個重要日子。") {
    const double t0 = epoch2024();
    QVector<double> xs;
    for (int i = 0; i < 365; ++i) xs.append(t0 + i * 86400.0);
    plot.addScatter(xs, gen::randomWalk(365))->marker = MarkerStyle::none();
    auto m = std::make_shared<ManualTickGenerator>();
    const QDate days[] = {QDate(2024, 1, 1), QDate(2024, 2, 10), QDate(2024, 6, 10), QDate(2024, 9, 17),
                          QDate(2024, 12, 31)};
    const QStringList names{RT("元旦"), RT("春節"), RT("端午"),
                            RT("中秋"), RT("除夕")};
    for (int i = 0; i < 5; ++i)
        m->ticks.append({static_cast<double>(QDateTime(days[i], QTime(0, 0)).toSecsSinceEpoch()), names[i], true});
    plot.bottomAxis()->tickGenerator = m;
}

QTPLOT_RECIPE("入門", "快速上手", "多個正弦波疊加",
              "gen::sin 產生正弦，gen::addNoise 加雜訊。幾個頻率疊在一起再加雜訊，"
              "就是頻譜分析、濾波範例最常用的測試訊號。") {
    QVector<double> ys(500, 0.0);
    const double freqs[] = {2, 5, 13};
    const double amps[] = {1.0, 0.5, 0.25};
    for (int k = 0; k < 3; ++k) {
        const QVector<double> s = gen::sin(500, amps[k], freqs[k]);
        for (int i = 0; i < 500; ++i) ys[i] += s[i];
    }
    plot.addSignal(gen::addNoise(ys, 0.15));
}

// ───────────────────────────── 標註 ─────────────────────────────

QTPLOT_RECIPE("標註", "文字", "角落標註的樣式",
              "Annotation 的底色、框線、內距、字型都能改。常拿來放「資料來源」或「目前狀態」。") {
    plot.addSignal(gen::sin(100));
    auto* a = plot.addAnnotation(RT("資料來源：中央氣象署"), Qt::AlignRight | Qt::AlignBottom);
    a->background = FillStyle(QColor(255, 240, 200));
    a->border = LineStyle(QColor(200, 140, 30), 2);
    a->padding = 10;
    a->label.font.size = 12;
    a->label.font.bold = true;
    a->label.font.color = QColor(140, 80, 0);
}

QTPLOT_RECIPE("標註", "文字", "角落標註的九個位置",
              "corner 接受上中下 × 左中右九種組合；marginX／marginY 是離繪圖區邊緣的距離。") {
    plot.addSignal(gen::sin(100));
    const Qt::Alignment hs[] = {Qt::AlignLeft, Qt::AlignHCenter, Qt::AlignRight};
    const Qt::Alignment vs[] = {Qt::AlignTop, Qt::AlignVCenter, Qt::AlignBottom};
    const QStringList hn{RT("左"), RT("中"), RT("右")};
    const QStringList vn{RT("上"), RT("中"), RT("下")};
    for (int v = 0; v < 3; ++v)
        for (int h = 0; h < 3; ++h) plot.addAnnotation(vn[v] + hn[h], hs[h] | vs[v]);
}

QTPLOT_RECIPE("圖種", "參考線與區間", "區間的花紋與框線",
              "AxisSpan::fill 是 FillStyle，可以用花紋；line 是兩側的框線。"
              "「預測區間」「資料不完整」這類要跟一般區間分開的，用斜線花紋最直覺。") {
    plot.addSignal(gen::randomWalk(200));
    auto* s = plot.addVerticalSpan(140, 200, QColor(0, 0, 0, 0));
    s->fill.hatch = HatchPattern::DiagonalUp;
    s->fill.hatchColor = QColor(214, 39, 40, 120);
    s->line = LineStyle(QColor(214, 39, 40), 1.5, LinePattern::Dashed);
    s->legendText = RT("預測區間");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("圖種", "長條與統計", "長條上的自訂文字",
              "Bar::label 只能放一段文字；要在長條旁邊放更多東西（說明、徽章），"
              "就用 Text 放在長條的座標上。") {
    plot.addBars(QVector<double>{12, 18, 9, 22});
    const QStringList notes{RT("新品"), RT("熱銷"), RT("缺貨"),
                            RT("冠軍")};
    const double vals[] = {12, 18, 9, 22};
    for (int i = 0; i < 4; ++i) {
        auto* t = plot.addText(notes[i], i, vals[i] / 2);
        t->alignment = Qt::AlignHCenter | Qt::AlignVCenter;
        t->label.font.color = Qt::white;
        t->label.font.bold = true;
        t->label.font.rotation = -90;
    }
}

QTPLOT_RECIPE("圖種", "長條與統計", "多段範圍長條（直向）",
              "每根長條用 valueBase 到 value 表示一段範圍。同一個位置放好幾根，"
              "就是一個項目有好幾段（班表、設備運轉時段、溫度帶）。") {
    QVector<Bar> bars;
    const double ranges[3][2][2] = {{{1, 3}, {5, 8}}, {{0, 2}, {4, 6}}, {{2, 7}, {8, 9}}};
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 2; ++k) {
            Bar b;
            b.position = i;
            b.valueBase = ranges[i][k][0];
            b.value = ranges[i][k][1];
            b.fillColor = k == 0 ? QColor(31, 119, 180) : QColor(255, 127, 14);
            bars.append(b);
        }
    auto* bp = plot.addBars(bars);
    bp->includeZero = false;
    categoryTicks(plot.bottomAxis(), {RT("機台 A"), RT("機台 B"), RT("機台 C")});
}

QTPLOT_RECIPE("圖種", "長條與統計", "多段範圍長條（橫向）",
              "同樣的資料改成 horizontal = true，類別放左邊、時間放底下，比較像時間軸。") {
    QVector<Bar> bars;
    const double ranges[3][2][2] = {{{8, 12}, {13, 17}}, {{9, 11.5}, {14, 18}}, {{7, 10}, {12, 16}}};
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 2; ++k) {
            Bar b;
            b.position = i;
            b.valueBase = ranges[i][k][0];
            b.value = ranges[i][k][1];
            b.size = 0.6;
            b.fillColor = k == 0 ? QColor(44, 160, 44) : QColor(148, 103, 189);
            bars.append(b);
        }
    auto* bp = plot.addBars(bars);
    bp->horizontal = true;
    bp->includeZero = false;
    categoryTicks(plot.leftAxis(), {RT("小明"), RT("小華"), RT("小美")});
    plot.bottomAxis()->label.text = RT("時刻");
}

QTPLOT_RECIPE("互動", "命中與提示", "十字線的樣式",
              "Crosshair 的線條、兩軸上的標籤底色與字型都能改；showHorizontal／showVertical 可以只留一條。") {
    plot.addSignal(gen::sin(100, 5, 2));
    auto* c = plot.addCrosshair(40, 2);
    c->lineStyle = LineStyle(QColor(31, 119, 180), 2, LinePattern::DashDot);
    c->labelBackground = QColor(31, 119, 180);
    c->labelFont.color = Qt::white;
    c->labelFont.bold = true;
}

QTPLOT_RECIPE("圖種", "散點與線", "兩條散點之間填色",
              "FillY 只要三個陣列：X、上緣、下緣。兩條已經存在的曲線，"
              "直接把它們的 xs／ys 傳進去就好。") {
    auto* a = plot.addScatter(gen::linspace(0, 10, 50), gen::sin(50, 1, 1, 0, 2));
    auto* b = plot.addScatter(gen::linspace(0, 10, 50), gen::cos(50, 1, 1, 0, -1));
    auto* f = plot.addFillY(a->xs, a->ys, b->ys, QColor(150, 150, 150));
    f->fill = FillStyle(QColor(150, 150, 150, 70));
    f->line = LineStyle::none();
    plot.moveToBack(f);
}

QTPLOT_RECIPE("圖種", "散點與線", "填色帶的樣式",
              "FillY::fill 可以用漸層或花紋，line 是上下兩條邊線。信賴區間用淡色、"
              "越往右越不確定就讓顏色越淡，讀者一看就知道要小心。") {
    const QVector<double> xs = gen::linspace(0, 10, 80);
    QVector<double> lo, hi;
    for (double x : xs) {
        lo.append(std::sin(x) - 0.3 - x * 0.05);
        hi.append(std::sin(x) + 0.3 + x * 0.05);
    }
    auto* f = plot.addFillY(xs, lo, hi);
    f->fill = FillStyle::linearGradient(GradientDirection::Horizontal,
                                        {QColor(31, 119, 180, 160), QColor(31, 119, 180, 30)});
    f->line = LineStyle(QColor(31, 119, 180), 1, LinePattern::Dotted);
}

// ───────────────────────────── 財金 ─────────────────────────────

QTPLOT_RECIPE("財金", "K 線", "價格放在右邊",
              "看盤軟體的價格刻度都在右邊 —— 最新的 K 線在最右端，眼睛不用橫越整張圖。"
              "把 K 線綁到 rightAxis()、隱藏左軸就好。") {
    auto* c = plot.addCandlestick(randomOhlcSeries(60));
    c->yAxis = plot.rightAxis();
    plot.rightAxis()->showTickLabels = true;
    plot.leftAxis()->isVisible = false;
}

QTPLOT_RECIPE("財金", "K 線", "股票代號浮水印",
              "先加一個大字、淡色、置中的 Annotation，再加 K 線 —— 先加的畫在下面，"
              "就成了背景浮水印。") {
    auto* wm = plot.addAnnotation(QStringLiteral("2330"), Qt::AlignHCenter | Qt::AlignVCenter);
    wm->label.font.size = 72;
    wm->label.font.bold = true;
    wm->label.font.color = QColor(0, 0, 0, 30);
    wm->background = FillStyle::none();
    wm->border = LineStyle::none();
    plot.addCandlestick(randomOhlcSeries(80));
}

QTPLOT_RECIPE("財金", "K 線", "多行的浮水印",
              "文字裡放換行就是多行；lineHeight 把行距收緊一點，代號和名稱看起來是一組。") {
    auto* wm = plot.addAnnotation(RT("2330\n台積電"), Qt::AlignHCenter | Qt::AlignVCenter);
    wm->label.font.size = 48;
    wm->label.font.bold = true;
    wm->label.font.lineHeight = 0.9;
    wm->label.font.color = QColor(0, 0, 0, 30);
    wm->background = FillStyle::none();
    wm->border = LineStyle::none();
    plot.addCandlestick(randomOhlcSeries(80));
}

QTPLOT_RECIPE("財金", "K 線", "深色主題的 K 線",
              "PlotStyle::dark() 配上亮一點的漲跌色。深色底上原本的綠紅要調亮，"
              "否則和背景的對比不夠。") {
    plot.setStyle(PlotStyle::dark());
    auto* c = plot.addCandlestick(randomOhlcSeries(70));
    c->risingColor = QColor(0, 230, 160);
    c->fallingColor = QColor(255, 90, 110);
}

// ───────────────────────────── 熱圖 ─────────────────────────────

QTPLOT_RECIPE("場", "熱圖", "反轉的色階",
              "Colormap::reversed() 把色階頭尾倒過來。「數值越小越重要」的資料"
              "（誤差、排名、延遲）用深色表示小值比較直覺。") {
    auto* hm = plot.addHeatmap(gen::sin2D(40, 40, 2), 40, 40);
    hm->colormap = Colormap(Colormap::Viridis).reversed();
    plot.addColorbar(*hm);
}

QTPLOT_RECIPE("場", "熱圖", "自訂色階的熱圖",
              "Colormap::fromColors 給幾個節點顏色。白 → 藍 → 深藍這種單色系色階，"
              "在只關心「多或少」的資料上比彩虹色階好讀。") {
    auto* hm = plot.addHeatmap(gen::gaussian2D(40, 60, 0.3), 40, 60);
    hm->colormap = Colormap::fromColors({QColor(255, 255, 255), QColor(120, 170, 220), QColor(10, 40, 100)});
    plot.addColorbar(*hm);
}

QTPLOT_RECIPE("場", "熱圖", "色階圖例的刻度文字",
              "Colorbar::tickGenerator 和座標軸的一樣，所以 labelFormatter 也能用。"
              "這裡把 0–1 的數值印成百分比。") {
    auto* hm = plot.addHeatmap(gen::gaussian2D(40, 40, 0.3), 40, 40);
    auto* cb = plot.addColorbar(*hm);
    auto t = std::make_shared<AutomaticTickGenerator>();
    t->labelFormatter = [](double v, double) { return QStringLiteral("%1%").arg(v * 100, 0, 'f', 0); };
    cb->tickGenerator = t;
}

QTPLOT_RECIPE("場", "熱圖", "沒有框的熱圖",
              "把四根軸都隱藏、四周邊距設成 0，熱圖就鋪滿整張圖，像一張影像。") {
    plot.addHeatmap(gen::sin2D(80, 120, 3), 80, 120)->colormap = Colormap(Colormap::Turbo);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.paddingLeft = plot.paddingRight = plot.paddingTop = plot.paddingBottom = 0;
    plot.grid.isVisible = false;
    plot.setAxisLimits(0, 120, 0, 80);
}

QTPLOT_RECIPE("場", "熱圖", "指定格子大小",
              "Heatmap::extent 是整張熱圖佔的資料範圍；格子大小 = 範圍 ÷ 格數。"
              "要每格 2.5 × 0.5 單位，就把 extent 設成 cols × 2.5、rows × 0.5。") {
    const int rows = 8, cols = 12;
    auto* hm = plot.addHeatmap(gen::randomUniform(rows * cols), rows, cols);
    const double cw = 2.5, ch = 0.5;
    hm->extent = CoordinateRect(0, cols * cw, 0, rows * ch);
    plot.bottomAxis()->label.text = RT("每格 2.5 單位寬");
    plot.leftAxis()->label.text = RT("每格 0.5 單位高");
}

QTPLOT_RECIPE("場", "熱圖", "日期軸的熱圖",
              "extent 的 X 用 Unix 秒，底軸換成 DateTimeTickGenerator，熱圖的每一欄就是一天。") {
    const int days = 60, hours = 24;
    QVector<double> v;
    for (int h = 0; h < hours; ++h)
        for (int d = 0; d < days; ++d)
            v.append(std::sin(h / 24.0 * 6.283 - 1.5) + 0.3 * std::sin(d / 7.0 * 6.283));
    auto* hm = plot.addHeatmap(v, hours, days);
    hm->flipVertically = true;  // 第 0 列（0 點）放在下面
    const double t0 = epoch2024();
    hm->extent = CoordinateRect(t0, t0 + days * 86400.0, 0, 24);
    plot.bottomAxis()->tickGenerator = std::make_shared<DateTimeTickGenerator>();
    plot.leftAxis()->label.text = RT("時");
    plot.addColorbar(*hm);
}

// ───────────────────────────── 統計 ─────────────────────────────

QTPLOT_RECIPE("統計", "棒棒糖與排名", "棒棒糖的位置",
              "LollipopItem::position 可以自己指定，不一定要 0, 1, 2…。"
              "不等距的時間點（量測日）就直接放實際的位置。") {
    auto* l = plot.addLollipop(QVector<double>{});
    const double pos[] = {1, 2.5, 3, 6, 7.5, 10};
    const double val[] = {4, 7, 5, 9, 6, 3};
    for (int i = 0; i < 6; ++i) l->items.append({pos[i], val[i], QColor(31, 119, 180), QString()});
}

QTPLOT_RECIPE("統計", "棒棒糖與排名", "棒棒糖的樣式",
              "stem 是桿子的線條，marker 是頭的形狀；橫向、加數值標籤都可以。") {
    auto* l = plot.addLollipop(QVector<double>{5, 8, 3, 6, 9, 4});
    l->horizontal = true;
    l->stem = LineStyle(QColor(214, 39, 40), 3, LinePattern::Dotted);
    l->marker = MarkerStyle(MarkerShape::FilledDiamond, 16, QColor(214, 39, 40));
    l->showValueLabels = true;
}

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "圓餅的起始角度",
              "startAngle 決定第一塊從哪裡開始（度，0 是三點鐘方向）。"
              "預設 90 是十二點鐘；這裡改成 0、並改成逆時針排。") {
    auto* pie = plot.addPie({40, 30, 20, 10});
    pie->startAngle = 0;
    pie->clockwise = false;
    pie->labelMode = Pie::LabelMode::Percent;
}

QTPLOT_RECIPE("極座標", "圓餅與玫瑰", "扇形標籤和圖例不同",
              "PieSlice::label 畫在扇形上、legendText 放在圖例裡，兩者可以不一樣："
              "扇形上只放百分比、圖例放完整名稱，圖面才不會擠。") {
    auto* pie = plot.addPie({45, 25, 20, 10});
    const QStringList full{RT("北部營業處"), RT("中部營業處"),
                           RT("南部營業處"), RT("東部營業處")};
    for (int i = 0; i < 4; ++i) {
        pie->slices[i].label = QStringLiteral("%1%").arg(pie->slices[i].value);
        pie->slices[i].legendText = full[i];
    }
    pie->labelMode = Pie::LabelMode::Label;
    pie->labelFont.color = Qt::white;
    pie->labelFont.bold = true;
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("統計", "箱型與分布", "平均值長條的樣式",
              "showMeanBar 畫一根到平均值的長條，fillAlpha 調它的透明度。"
              "長條配上誤差線就是最傳統的「平均 ± 標準差」圖。") {
    gen::setSeed(3);
    auto* p = plot.addPopulation({gen::randomNormal(30, 10, 2), gen::randomNormal(30, 14, 3),
                                  gen::randomNormal(30, 8, 1.5)},
                                 {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
    p->showMeanBar = true;
    p->showBox = false;
    p->fillAlpha = 200;
    p->pointMarker.size = 4;
}

QTPLOT_RECIPE("統計", "箱型與分布", "箱子的樣式",
              "boxBorder 是箱子與中位數線的線條；fillAlpha 控制箱子的填色。") {
    gen::setSeed(4);
    auto* p = plot.addPopulation({gen::randomNormal(50, 10, 2), gen::randomNormal(50, 12, 2.5)},
                                 {RT("前"), RT("後")});
    p->boxBorder = LineStyle(QColor(40, 40, 40), 2);
    p->fillAlpha = 160;
    p->showPoints = false;
    p->showErrorBar = false;
}

QTPLOT_RECIPE("統計", "箱型與分布", "資料點的樣式",
              "pointMarker 是個別資料點的標記；jitter 是左右抖動的寬度。"
              "點多的時候抖開來，才看得出密集的地方。") {
    gen::setSeed(5);
    auto* p = plot.addPopulation({gen::randomNormal(120, 10, 2), gen::randomNormal(120, 12, 2)},
                                 {RT("甲"), RT("乙")});
    p->pointMarker = MarkerStyle(MarkerShape::OpenCircle, 7, QColor(0, 0, 0, 160));
    p->jitter = 0.8;
    p->showBox = false;
}

QTPLOT_RECIPE("統計", "箱型與分布", "分組的分布圖",
              "每組的 position 自己指定：同一類別的兩組各往左右偏一點，"
              "就成了「兩種處理 × 三個時間點」的分組圖。顏色代表處理、位置代表時間。") {
    gen::setSeed(6);
    auto* p = plot.addPopulation({}, {});
    const QColor colors[] = {QColor(31, 119, 180), QColor(255, 127, 14)};
    for (int t = 0; t < 3; ++t)
        for (int k = 0; k < 2; ++k) {
            Population::Group g;
            g.values = gen::randomNormal(40, 10 + t * 2 + k * 3, 2);
            g.position = t + (k == 0 ? -0.2 : 0.2);
            g.color = colors[k];
            if (t == 0) g.legendText = k == 0 ? RT("對照") : RT("處理");
            p->groups.append(g);
        }
    p->groupWidth = 0.35;
    p->showErrorBar = false;
    categoryTicks(plot.bottomAxis(), {RT("第 1 週"), RT("第 2 週"), RT("第 3 週")});
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("統計", "箱型與分布", "重疊很多的山脊圖",
              "overlap 大於 1 就讓每條曲線伸進上一條的範圍；配白色邊線把前後分開。"
              "很多組（十幾組）擠在一起時，重疊多一點反而看得出整體走勢。") {
    gen::setSeed(7);
    QVector<RidgeGroup> groups;
    for (int i = 0; i < 12; ++i)
        groups.append({RT("%1 月").arg(i + 1),
                       gen::randomNormal(200, 10 + 8 * std::sin(i / 12.0 * 6.283), 3), QColor()});
    auto* r = plot.addRidgeline(groups);
    r->overlap = 2.5;
    r->line = LineStyle(Qt::white, 1.5);
    r->fillAlpha = 235;
}

// ───────────────────────────── 極座標 ─────────────────────────────

QTPLOT_RECIPE("極座標", "雷達與量表", "格線畫在資料上面",
              "畫的順序就是加入的順序。先加雷達、再加 PolarAxis，格線就壓在填色上，"
              "填色很深的時候格線還是看得到。") {
    const QStringList dims{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
                           QStringLiteral("D"), QStringLiteral("E")};
    auto* r = plot.addRadar({{4, 5, 3, 4, 5}}, dims);
    r->series[0].fillColor = QColor(31, 119, 180, 220);
    auto* ax = plot.addPolarAxis(1.0, dims.size());
    ax->spokeLine = LineStyle(QColor(255, 255, 255, 180), 1);
    ax->ringLine = LineStyle(QColor(255, 255, 255, 180), 1);
    ax->labelFont.color = QColor(0, 0, 0, 0);
    ax->showRingLabels = false;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "每個維度各自的刻度",
              "independentAxes 讓每個維度以自己的最大值為滿格；這時候一定要把每條放射線"
              "各自的刻度標出來。做法是每個維度疊一根只畫半徑標籤的 PolarAxis，"
              "ringLabelAngle 對準那條放射線。") {
    const QStringList dims{RT("價格(萬)"), RT("馬力"), RT("油耗"),
                           RT("座位"), RT("後車廂(L)")};
    const QVector<double> maxes{200, 400, 25, 8, 600};
    auto* grid = plot.addPolarAxis(1.0, dims.size());
    grid->spokeLabels = dims;
    grid->labelFont.color = QColor(0, 0, 0, 0);
    grid->showRingLabels = false;
    auto* r = plot.addRadar({{120, 250, 14, 5, 420}, {80, 150, 20, 7, 550}}, dims);
    r->independentAxes = true;
    for (int i = 0; i < dims.size(); ++i) {
        auto* ax = plot.addPolarAxis(1.0, dims.size());
        ax->spokeLine = LineStyle::none();
        ax->ringLine = LineStyle::none();
        ax->labelFont.color = QColor(0, 0, 0, 0);
        ax->ringRadii = {0.5, 1.0};
        ax->ringLabels = {QString::number(maxes[i] / 2), QString::number(maxes[i])};
        ax->ringLabelAngle = 360.0 / dims.size() * i;
        ax->ringLabelFont.size = 8;
        ax->ringLabelFont.color = QColor(90, 90, 90);
    }
}

QTPLOT_RECIPE("極座標", "雷達與量表", "量表的顏色",
              "colors 逐環指定顏色；backgroundColor 是沒走到那段的底環。") {
    auto* g = plot.addRadialGauge({85, 60, 35}, {}, 100);
    g->colors = {QColor(0, 150, 136), QColor(255, 193, 7), QColor(233, 30, 99)};
    g->backgroundColor = QColor(0, 0, 0, 15);
}

QTPLOT_RECIPE("極座標", "雷達與量表", "量表的方向",
              "clockwise = false 改成逆時針走。左右對稱的版面裡，左邊那個量表逆時針、"
              "右邊順時針，看起來比較平衡。") {
    auto* g = plot.addRadialGauge({70, 45, 90}, {}, 100);
    g->clockwise = false;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "量表的粗細與間距",
              "innerRadius 決定中間空多大、gaugeSpacing 決定環與環之間的空隙（佔環厚度的比例）。") {
    auto* g = plot.addRadialGauge({70, 45, 90, 60}, {}, 100);
    g->innerRadius = 0.6;
    g->gaugeSpacing = 0.4;
}

QTPLOT_RECIPE("極座標", "雷達與量表", "量表的起始角度",
              "startAngle 是 0% 的位置（度，0 是三點鐘、90 是十二點鐘）。"
              "設成 270 就從六點鐘開始。") {
    auto* g = plot.addRadialGauge({70, 45, 90}, {}, 100);
    g->startAngle = 270;
}

QTPLOT_RECIPE("極座標", "旭日圖", "投資組合旭日圖",
              "每個節點自己指定顏色：大類一種色系、子項用同色系的深淺。"
              "中間的洞放總額 —— 旭日圖沒有中心文字欄位，直接在 (0, 0) 放一個 Text。") {
    auto node = [](const QString& n, double v, QColor c) {
        SunburstNode s;
        s.label = n;
        s.value = v;
        s.fillColor = c;
        return s;
    };
    SunburstNode stock = node(RT("股票"), 0, QColor(31, 119, 180));
    stock.children = {node(RT("台股"), 35, QColor(70, 150, 210)),
                      node(RT("美股"), 25, QColor(110, 175, 225))};
    SunburstNode bond = node(RT("債券"), 0, QColor(44, 160, 44));
    bond.children = {node(RT("公債"), 15, QColor(90, 190, 90)),
                     node(RT("公司債"), 10, QColor(140, 210, 140))};
    SunburstNode cash = node(RT("現金"), 15, QColor(255, 127, 14));
    plot.addSunburst({stock, bond, cash});
    auto* t = plot.addText(RT("總額\n$1,000萬"), 0, 0);
    t->alignment = Qt::AlignHCenter | Qt::AlignVCenter;
    t->label.font.bold = true;
    t->label.font.size = 12;
}

// ───────────────────────────── 標註與特殊座標 ─────────────────────────────

QTPLOT_RECIPE("標註", "比例尺與影像", "比例尺的樣式",
              "ScaleBar 的線條、字型、位置都能改。地圖、顯微影像常用粗一點的白線配黑底。") {
    plot.addHeatmap(gen::sin2D(60, 80, 4), 60, 80)->colormap = Colormap(Colormap::Grayscale);
    auto* sb = plot.addScaleBar(10, 10);
    sb->widthLabel = QStringLiteral("10 µm");
    sb->heightLabel = QStringLiteral("10 µm");
    sb->line = LineStyle(Qt::white, 4);
    sb->font.color = Qt::white;
    sb->font.bold = true;
    sb->corner = Qt::AlignLeft | Qt::AlignBottom;
}

QTPLOT_RECIPE("標註", "比例尺與影像", "只有一個方向的比例尺",
              "height 給 0 就只畫水平那段 —— 地圖通常只需要水平比例尺。") {
    plot.addSignal(gen::randomWalk(300));
    auto* sb = plot.addScaleBar(50, 0);
    sb->widthLabel = RT("50 公尺");
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
}

QTPLOT_RECIPE("圖種", "散點與線", "訊號圖的樣式",
              "Signal 的 line 和 marker 都能改。點不多的時候畫上標記，讀者才知道哪裡是真的取樣點。") {
    auto* s = plot.addSignal(gen::sin(40, 1, 2));
    s->line = LineStyle(QColor(214, 39, 40), 2, LinePattern::Dashed);
    s->marker = MarkerStyle(MarkerShape::FilledSquare, 7, QColor(214, 39, 40));
}

QTPLOT_RECIPE("圖種", "散點與線", "放大才出現的標記",
              "autoMarkerSpacingPx：點與點的間距超過這個像素數，就自動畫標記。"
              "縮小看全貌時只有線，放大到看得出個別點時標記才出現。用滾輪放大試試。") {
    auto* s = plot.addSignal(gen::sin(2000, 1, 20));
    s->autoMarkerSpacingPx = 6;
}

QTPLOT_RECIPE("圖種", "散點與線", "日期軸的訊號圖",
              "Signal 的 period 用秒、xOffset 用起點的 Unix 秒，底軸換成 DateTimeTickGenerator。"
              "每日資料 period = 86400。") {
    auto* s = plot.addSignal(gen::randomWalk(365));
    s->period = 86400;
    s->xOffset = epoch2024();
    plot.bottomAxis()->tickGenerator = std::make_shared<DateTimeTickGenerator>();
}

QTPLOT_RECIPE("圖種", "散點與線", "SignalXY 只畫一段",
              "renderFirst／renderLast 指定要畫的索引範圍，資料不用切。"
              "這裡淡淡畫出全部，再用另一個 SignalXY 只畫中間那段並加粗。") {
    const QVector<double> xs = gen::linspace(0, 20, 400), ys = gen::randomWalk(400);
    plot.addSignalXY(xs, ys, QColor(180, 180, 180));
    auto* part = plot.addSignalXY(xs, ys, QColor(214, 39, 40));
    part->renderFirst = 150;
    part->renderLast = 250;
    part->line.width = 3;
}

QTPLOT_RECIPE("圖種", "散點與線", "直向 SignalXY 配反向 Y 軸",
              "vertical = true 讓 xs 對到 Y 軸；再把 Y 軸反過來，深度就是由上往下增加 —— "
              "井測、海洋剖面都是這樣畫的。") {
    auto* s = plot.addSignalXY(gen::linspace(0, 500, 300), gen::addNoise(gen::sin(300, 5, 3, 0, 20), 0.12));
    s->vertical = true;
    plot.leftAxis()->inverted = true;
    plot.leftAxis()->label.text = RT("深度 (m)");
    plot.bottomAxis()->label.text = RT("溫度 (°C)");
}

QTPLOT_RECIPE("圖種", "散點與線", "SignalXY 加標記",
              "SignalXY::marker 有設就畫（點很多、被抽樣的時候會自動略過）。") {
    gen::setSeed(10);
    const QVector<double> steps = gen::randomUniform(40, 0.2, 1.2);
    QVector<double> xs;
    double x = 0;
    for (double s : steps) xs.append(x += s);
    auto* s = plot.addSignalXY(xs, gen::randomWalk(40));
    s->marker = MarkerStyle(MarkerShape::FilledCircle, 7, QColor(31, 119, 180));
}

QTPLOT_RECIPE("特殊座標", "史密斯圖", "阻抗匹配路徑",
              "負載 z = 0.3 + j0.6，先串一個電容沿著等電阻圓 r = 0.3 走到 g = 1 的圓上，"
              "再並一個電感沿著等電導圓走回圓心（z = 1，完全匹配）。"
              "SmithChartAxis::coordinates(r, x) 把阻抗換成圖上的位置。") {
    auto* sc = plot.addSmithChart();
    auto at = [&](std::complex<double> z) { return sc->coordinates(z.real(), z.imag()); };
    QVector<double> x1, y1, x2, y2;
    const double xEnd = -std::sqrt(0.3 - 0.09);
    for (int i = 0; i <= 40; ++i) {  // 串聯：r 不變，x 從 0.6 走到 -0.458
        const Coordinates c = at({0.3, 0.6 + (xEnd - 0.6) * i / 40.0});
        x1.append(c.x);
        y1.append(c.y);
    }
    const std::complex<double> y0 = 1.0 / std::complex<double>(0.3, xEnd);
    for (int i = 0; i <= 40; ++i) {  // 並聯：g 不變，b 走到 0
        const std::complex<double> yy(y0.real(), y0.imag() * (1 - i / 40.0));
        const Coordinates c = at(1.0 / yy);
        x2.append(c.x);
        y2.append(c.y);
    }
    auto* a = plot.addScatter(x1, y1, QColor(214, 39, 40));
    a->marker = MarkerStyle::none();
    a->line.width = 3;
    a->legendText = RT("串聯電容");
    auto* b = plot.addScatter(x2, y2, QColor(31, 119, 180));
    b->marker = MarkerStyle::none();
    b->line.width = 3;
    b->legendText = RT("並聯電感");
    const Coordinates load = at({0.3, 0.6});
    plot.addMarkers({load.x}, {load.y}, QColor(0, 0, 0))->marker.size = 10;
    plot.addText(RT("負載"), load.x + 0.04, load.y);
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("標註", "氣泡標註", "氣泡標註的文字樣式",
              "Tooltip::label 是完整的 Label：字型、大小、粗細、多行都可以。") {
    plot.addSignal(gen::sin(100, 3, 1));
    auto* t = plot.addTooltip(RT("峰值\n3.00"), 25, 3, 45, 4.2);
    t->label.font.size = 14;
    t->label.font.bold = true;
    t->label.font.lineHeight = 1.2;
    t->padding = 8;
}

QTPLOT_RECIPE("標註", "氣泡標註", "氣泡標註的顏色",
              "background、border、cornerRadius 決定外框的長相；深色底配白字最醒目。") {
    plot.addSignal(gen::sin(100, 3, 1));
    auto* t = plot.addTooltip(RT("谷底 −3.00"), 75, -3, 55, -1.5);
    t->background = FillStyle(QColor(214, 39, 40));
    t->border = LineStyle(QColor(150, 20, 20), 2);
    t->cornerRadius = 10;
    t->label.font.color = Qt::white;
    t->label.font.bold = true;
}

// ───────────────────────────── 字型 ─────────────────────────────

QTPLOT_RECIPE("樣式", "文字", "從檔案載入字型",
              "QFontDatabase::addApplicationFont 載入一個 .ttf／.otf，回傳的家族名稱"
              "交給 setFontFamily 就套到整張圖。實際使用時字型檔通常放在資源檔（:/fonts/...）裡。"
              "這裡用 Windows 內建的 Consolas 當示範，找不到檔案就沿用系統字型。") {
    const QString fam = loadFontFile(QStringLiteral("C:/Windows/Fonts/consola.ttf"));
    plot.addSignal(gen::sin(100));
    plot.setFontFamily(fam);
    plot.title.text = fam.isEmpty() ? RT("（找不到字型檔，沿用系統字型）")
                                    : QStringLiteral("Font: %1").arg(fam);
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例的自訂字型（自動項目）",
              "只改圖例的字型：legend.font.family。項目仍然從圖元自動收集。") {
    const QString fam = loadFontFile(QStringLiteral("C:/Windows/Fonts/georgia.ttf"));
    for (int i = 0; i < 3; ++i)
        plot.addSignal(gen::sin(100, 1, 1, i * 0.25))->legendText = QStringLiteral("Series %1").arg(i + 1);
    plot.legend.isVisible = true;
    plot.legend.font.family = fam;
    plot.legend.font.size = 13;
    plot.legend.font.italic = true;
}

QTPLOT_RECIPE("樣式", "格線與圖例", "圖例的自訂字型（手動項目）",
              "manualItems 自己列出圖例項目，字型一樣由 legend.font 決定。"
              "圖例要說明的東西和圖元不是一對一時（例如顏色代表等級），就用手動項目。") {
    const QString fam = loadFontFile(QStringLiteral("C:/Windows/Fonts/georgia.ttf"));
    plot.addBars(QVector<double>{3, 7, 5, 9});
    LegendItem good, bad;
    good.text = QStringLiteral("Above target");
    good.fill = FillStyle(QColor(44, 160, 44));
    good.line = LineStyle::none();
    bad.text = QStringLiteral("Below target");
    bad.fill = FillStyle(QColor(214, 39, 40));
    bad.line = LineStyle::none();
    plot.legend.manualItems = {good, bad};
    plot.legend.isVisible = true;
    plot.legend.font.family = fam;
    plot.legend.font.bold = true;
}

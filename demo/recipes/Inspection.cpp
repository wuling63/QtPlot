// Inspection.cpp — 視覺檢測：混淆矩陣、異常熱圖疊在影像上、OK／NG 分數分布與門檻。
//
// 資料是合成的（InspectionData.h）：換成自己的影像、模型輸出的異常圖與分數即可。
#include "../InspectionData.h"
#include "../Recipe.h"

using namespace qtplot;
using namespace qtplot::demo;

namespace {

/// 五類瑕疵分類器的混淆矩陣（列是實際、欄是預測）。
QVector<QVector<double>> defectCounts() {
    return {{412, 6, 3, 2, 7},
            {9, 61, 4, 0, 2},
            {5, 3, 48, 6, 1},
            {4, 1, 9, 33, 0},
            {11, 2, 0, 1, 26}};
}

QStringList defectClasses() {
    return {RT("良品"), RT("刮痕"), RT("黑點"), RT("凹坑"), RT("污漬")};
}

/// 等寬分箱：兩組分數用同一組箱子，才疊得起來比較。
BarPlot* addScoreHistogram(Plot& plot, const QVector<double>& scores, QColor color, int bins = 40) {
    QVector<Bar> bars;
    QVector<int> counts(bins, 0);
    for (double v : scores) counts[std::clamp(static_cast<int>(v * bins), 0, bins - 1)]++;
    for (int i = 0; i < bins; ++i) {
        Bar b;
        b.position = (i + 0.5) / bins;
        b.value = counts[i];
        b.size = 1.0 / bins;
        b.fillColor = color;
        bars.append(b);
    }
    return plot.addBars(bars);
}

/// 讀門檻線的位置，算出漏檢率與誤判率，寫在右上角。
///
/// 每次重畫都重算：拖門檻線時 widget 會重畫，數字就跟著變，不必接任何訊號。
class ThresholdReadout : public IPlottable {
public:
    const AxisLine* line = nullptr;
    InspectionScores scores;

    AxisLimits axisLimits() const override { return {}; }
    void render(RenderPack& rp) override {
        if (!line) return;
        const ThresholdStats s = thresholdStats(scores.ok, scores.ng, line->position);
        const QString text = RT("門檻 %1\n漏檢率 %2%（%3／%4 片 NG 被放過）\n誤判率 %5%（%6／%7 片 OK 被打掉）")
                                 .arg(line->position, 0, 'f', 3)
                                 .arg(s.missRate * 100, 0, 'f', 1).arg(s.fn).arg(s.fn + s.tp)
                                 .arg(s.falseAlarmRate * 100, 0, 'f', 1).arg(s.fp).arg(s.fp + s.tn);
        Label lb;
        lb.text = text;
        lb.font.size = 9.5 * rp.scaleFactor;
        const QSizeF sz = measureText(lb);
        const double pad = 6 * rp.scaleFactor;
        const QRectF box(rp.dataRect.right - sz.width() - 2 * pad - 8 * rp.scaleFactor, rp.dataRect.top + 8 * rp.scaleFactor,
                         sz.width() + 2 * pad, sz.height() + 2 * pad);
        QPainter& p = *rp.painter;
        p.setPen(QPen(QColor(0, 0, 0, 90), 1));
        p.setBrush(QColor(255, 255, 255, 235));
        p.drawRoundedRect(box, 4, 4);
        drawText(p, box.center(), lb, Qt::AlignCenter);
    }
};

}  // namespace

// ───────────────────────────── 混淆矩陣 ─────────────────────────────

QTPLOT_RECIPE("視覺檢測", "混淆矩陣", "混淆矩陣",
              "列是實際類別、欄是模型的預測，對角線（加框的格子）是判對的。看非對角線的格子就知道錯在哪："
              "這裡最多的錯是「污漬」被判成良品（漏檢）與「凹坑」被判成黑點（類別混淆）。"
              "addConfusionMatrix 會把類別名放到兩軸、軸名設成「預測」「實際」。") {
    plot.addConfusionMatrix(defectClasses(), defectCounts());
    plot.title.text = RT("瑕疵分類的混淆矩陣（筆數）");
}

QTPLOT_RECIPE("視覺檢測", "混淆矩陣", "依列正規化（看召回率）",
              "良品有 430 片、凹坑只有 47 片，原始筆數的色階會被良品那一格整個佔走，小類別全是淺色。"
              "Normalize::Row 讓每一列加總為 1：對角線就是各類的召回率（真的是這類的有幾成被找出來），"
              "每一類都看得出錯誤集中在哪一欄。Normalize::Column 則是精確率。") {
    auto* cm = plot.addConfusionMatrix(defectClasses(), defectCounts());
    cm->normalize = ConfusionMatrix::Normalize::Row;
    cm->showCounts = false;
    const QVector<ClassMetrics> m = classMetrics(defectCounts());
    QStringList lines;
    const QStringList names = defectClasses();
    for (int i = 0; i < m.size(); ++i)
        lines << RT("%1：召回率 %2%、精確率 %3%、F1 %4")
                     .arg(names[i]).arg(m[i].recall * 100, 0, 'f', 1).arg(m[i].precision * 100, 0, 'f', 1)
                     .arg(m[i].f1, 0, 'f', 2);
    auto* note = plot.addAnnotation(lines.join(QLatin1Char('\n')), Qt::AlignRight | Qt::AlignVCenter);
    note->label.font.size = 8.5;
    // 右邊多留一塊空白放這些數字，不要蓋到矩陣
    plot.setAxisLimits(-0.5, 4.5 + 3.4, -0.5, 4.5);
    plot.title.text = RT("依列正規化：對角線是召回率（正確率 %1%）").arg(accuracy(defectCounts()) * 100, 0, 'f', 1);
}

QTPLOT_RECIPE("視覺檢測", "混淆矩陣", "OK／NG 的混淆矩陣",
              "二分類（良品／不良品）在某個門檻下的結果。右上那格是過殺（OK 被判 NG），左下那格是漏檢"
              "（NG 被放過）—— 產線通常寧可過殺也不能漏檢。thresholdStats 由兩組分數與門檻直接算出這張表。") {
    const InspectionScores s = makeInspectionScores();
    const ThresholdStats st = thresholdStats(s.ok, s.ng, 0.48);
    plot.addConfusionMatrix({QStringLiteral("OK"), QStringLiteral("NG")}, st.counts());
    plot.title.text = RT("門檻 0.48：漏檢 %1 片、過殺 %2 片").arg(st.fn).arg(st.fp);
}

// ───────────────────────────── 異常熱圖 ─────────────────────────────

QTPLOT_RECIPE("視覺檢測", "異常熱圖", "異常熱圖疊在影像上",
              "ImageRect 放原圖，上面疊一張半透明的 Heatmap（模型輸出的異常圖）。hideBelow 讓低於門檻的格子透明，"
              "只露出可疑的區域；再用 ContourLines 在門檻上描一圈輪廓（firstRowAtBottom = false 讓它和熱圖同方向）。"
              "異常圖的解析度通常比影像低，兩者用同一個 extent 疊起來就會對齊。") {
    const InspectionSample s = makeInspectionSample(2);
    const double threshold = 0.45;
    const CoordinateRect extent(0, s.cols, 0, s.rows);

    auto* img = plot.addImage(s.image, extent.left, extent.right, extent.bottom, extent.top);
    img->keepAspectRatio = false;

    auto* heat = plot.addHeatmap(s.anomaly, s.rows, s.cols);
    heat->colormap = Colormap(Colormap::Hot);
    heat->colorRange = {0, 1};
    heat->opacity = 0.6;
    heat->smooth = true;
    heat->hideBelow = threshold;

    auto* contour = plot.addContour(s.anomaly, s.rows, s.cols);
    contour->firstRowAtBottom = false;
    contour->extent = ContourLines::latticeExtentFor(extent, s.rows, s.cols);
    contour->levels = {threshold};
    contour->line = LineStyle(QColor(0, 255, 255), 2);

    plot.addColorbar(*heat);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
    plot.equalAxisScale = true;
    plot.setAxisLimits(extent.left, extent.right, extent.bottom, extent.top);
    plot.title.text = RT("刮痕：異常分數 ≥ %1 的區域").arg(threshold);
}

QTPLOT_RECIPE_WIDGET("視覺檢測", "異常熱圖", "OK／NG 分數分布與門檻",
                     "良品與不良品的異常分數各畫一個直方圖，兩個分布重疊的地方就是門檻怎麼放都會錯的地方。"
                     "門檻線可以用滑鼠左右拖：往左移漏檢變少、過殺變多，往右反過來。右上角的數字每次重畫都"
                     "重新計算（ThresholdReadout），不必接任何訊號。") {
    Plot& plot = widget.plot();
    const InspectionScores s = makeInspectionScores();
    addScoreHistogram(plot, s.ok, QColor(44, 160, 44, 150))->legendText = RT("良品（OK）");
    addScoreHistogram(plot, s.ng, QColor(214, 39, 40, 150))->legendText = RT("不良品（NG）");

    auto* line = plot.addVerticalLine(0.48, QColor(30, 30, 30));
    line->draggable = true;
    line->line.width = 2;
    line->line.pattern = LinePattern::Dashed;

    auto readout = std::make_unique<ThresholdReadout>();
    readout->line = line;
    readout->scores = s;
    plot.add(std::move(readout));

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
    plot.bottomAxis()->label.text = RT("異常分數");
    plot.leftAxis()->label.text = RT("片數");
    plot.setAxisLimits(0, 1, 0, 60);
    plot.title.text = RT("拖動門檻線：漏檢與過殺的取捨");
}

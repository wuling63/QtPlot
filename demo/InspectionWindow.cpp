#include "InspectionWindow.h"

#include "Recipe.h"  // rt()

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>

namespace qtplot {
namespace demo {

namespace {
constexpr int kSliderSteps = 1000;  // 滑桿 0–1000 對到門檻 0–1
// 下拉選單的名稱，順序和 makeInspectionSample 的編號一致
const char* const kSampleNames[] = {QT_TRANSLATE_NOOP("qtplot::demo::InspectionWindow", "良品 A"),
                                    QT_TRANSLATE_NOOP("qtplot::demo::InspectionWindow", "良品 B"),
                                    QT_TRANSLATE_NOOP("qtplot::demo::InspectionWindow", "刮痕"),
                                    QT_TRANSLATE_NOOP("qtplot::demo::InspectionWindow", "黑點"),
                                    QT_TRANSLATE_NOOP("qtplot::demo::InspectionWindow", "凹坑")};
}

InspectionWindow::InspectionWindow(bool dark, QWidget* parent) : QWidget(parent), m_dark(dark) {
    setWindowTitle(tr("瑕疵檢測門檻示範"));
    resize(1280, 900);
    m_scores = makeInspectionScores();

    m_imagePlot = new PlotWidget(this);
    m_histPlot = new PlotWidget(this);
    m_matrixPlot = new PlotWidget(this);
    m_rocPlot = new PlotWidget(this);
    for (PlotWidget* w : {m_imagePlot, m_histPlot, m_matrixPlot, m_rocPlot}) {
        w->plot().setStyle(dark ? PlotStyle::dark() : PlotStyle::light());
        w->animationDurationMs = 0;
    }
    // 影像、矩陣、ROC 不需要平移縮放：拖了只會把圖拖跑
    for (PlotWidget* w : {m_matrixPlot, m_rocPlot}) {
        w->panEnabled = false;
        w->zoomEnabled = false;
    }

    m_sampleBox = new QComboBox(this);
    for (int i = 0; i < inspectionSampleCount(); ++i) m_sampleBox->addItem(tr(kSampleNames[i]));
    m_sampleBox->setCurrentIndex(2);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, kSliderSteps);
    m_summary = new QLabel(this);
    m_summary->setTextFormat(Qt::RichText);

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(tr("影像："), this));
    controls->addWidget(m_sampleBox);
    controls->addSpacing(16);
    controls->addWidget(new QLabel(tr("門檻："), this));
    controls->addWidget(m_slider, 1);

    auto* grid = new QGridLayout;
    grid->addWidget(m_imagePlot, 0, 0);
    grid->addWidget(m_histPlot, 0, 1);
    grid->addWidget(m_matrixPlot, 1, 0);
    grid->addWidget(m_rocPlot, 1, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(m_summary);
    layout->addLayout(grid, 1);

    buildHistogram();
    buildRoc();
    loadSample(m_sampleBox->currentIndex());

    connect(m_sampleBox, &QComboBox::currentIndexChanged, this, [this](int i) { loadSample(i); });
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        if (!m_updating) applyThreshold(double(v) / kSliderSteps);
    });
    // 門檻線是在分布圖上直接拖的：滑鼠一動就看線的位置有沒有變
    auto followLine = [this] {
        if (m_line && std::abs(m_line->position - m_threshold) > 1e-12)
            applyThreshold(std::clamp(m_line->position, 0.0, 1.0), true);
    };
    connect(m_histPlot, &PlotWidget::mouseMoved, this, followLine);
    connect(m_histPlot, &PlotWidget::plottableDragged, this, followLine);
}

void InspectionWindow::loadSample(int index) {
    m_sample = makeInspectionSample(index);
    buildImagePlot();
    applyThreshold(m_threshold);
}

void InspectionWindow::buildImagePlot() {
    m_imagePlot->resetPlot();
    Plot& plot = m_imagePlot->plot();
    plot.setStyle(m_dark ? PlotStyle::dark() : PlotStyle::light());
    const CoordinateRect extent(0, m_sample.cols, 0, m_sample.rows);
    auto* img = plot.addImage(m_sample.image, extent.left, extent.right, extent.bottom, extent.top);
    img->keepAspectRatio = false;

    m_heat = plot.addHeatmap(m_sample.anomaly, m_sample.rows, m_sample.cols);
    m_heat->colormap = Colormap(Colormap::Hot);
    m_heat->colorRange = {0, 1};
    m_heat->opacity = 0.6;
    m_heat->smooth = true;

    m_contour = plot.addContour(m_sample.anomaly, m_sample.rows, m_sample.cols);
    m_contour->firstRowAtBottom = false;
    m_contour->extent = ContourLines::latticeExtentFor(extent, m_sample.rows, m_sample.cols);
    m_contour->line = LineStyle(QColor(0, 255, 255), 2);

    plot.addColorbar(*m_heat);
    for (Axis* a : {plot.bottomAxis(), plot.leftAxis(), plot.topAxis(), plot.rightAxis()}) a->isVisible = false;
    plot.grid.isVisible = false;
    plot.equalAxisScale = true;
    plot.setAxisLimits(extent.left, extent.right, extent.bottom, extent.top);
}

void InspectionWindow::buildHistogram() {
    Plot& plot = m_histPlot->plot();
    const int bins = 40;
    auto addHist = [&](const QVector<double>& scores, QColor color, const QString& name) {
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
        plot.addBars(bars)->legendText = name;
    };
    addHist(m_scores.ok, QColor(44, 160, 44, 150), rt("良品（OK）"));
    addHist(m_scores.ng, QColor(214, 39, 40, 150), rt("不良品（NG）"));
    m_line = plot.addVerticalLine(m_threshold, m_dark ? QColor(230, 230, 230) : QColor(30, 30, 30));
    m_line->draggable = true;
    m_line->line.width = 2;
    m_line->line.pattern = LinePattern::Dashed;
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignTop;
    plot.bottomAxis()->label.text = rt("異常分數");
    plot.leftAxis()->label.text = rt("片數");
    plot.setAxisLimits(0, 1, 0, 60);
    plot.title.text = tr("分數分布（拖動虛線改門檻）");
}

void InspectionWindow::buildRoc() {
    Plot& plot = m_rocPlot->plot();
    QVector<double> scores = m_scores.ok;
    scores += m_scores.ng;
    QVector<bool> labels(m_scores.ok.size(), false);
    labels += QVector<bool>(m_scores.ng.size(), true);
    const ClassifierCurve roc = rocCurve(scores, labels);
    plot.addClassifierCurve(roc, QColor(31, 119, 180), true);
    plot.addLine(0, 0, 1, 1, QColor(150, 150, 150))->line.pattern = LinePattern::Dashed;
    m_rocPoint = plot.addMarkers({0}, {0}, QColor(214, 39, 40));
    m_rocPoint->marker = MarkerStyle(MarkerShape::FilledCircle, 11, QColor(214, 39, 40));
    plot.bottomAxis()->label.text = tr("誤判率（OK 被判 NG）");
    plot.leftAxis()->label.text = tr("檢出率（NG 被抓到）");
    plot.setAxisLimits(0, 1, 0, 1.02);
    plot.title.text = tr("ROC（AUC = %1）").arg(roc.area, 0, 'f', 3);
}

void InspectionWindow::applyThreshold(double t, bool fromLine) {
    m_threshold = t;
    m_updating = true;
    m_slider->setValue(static_cast<int>(std::lround(t * kSliderSteps)));
    m_updating = false;
    if (!fromLine && m_line) m_line->position = t;

    // 影像：只露出超過門檻的地方，並描出輪廓
    if (m_heat) m_heat->hideBelow = t;
    if (m_contour) m_contour->levels = {t};
    double maxScore = 0;
    for (double v : m_sample.anomaly) maxScore = std::max(maxScore, v);
    const bool judgedNg = maxScore >= t;
    m_imagePlot->plot().title.text =
        tr("%1：最高異常分數 %2 → 判定 %3").arg(m_sampleBox->currentText()).arg(maxScore, 0, 'f', 3)
            .arg(judgedNg ? QStringLiteral("NG") : QStringLiteral("OK"));

    // 這個門檻下的整批結果
    const ThresholdStats s = thresholdStats(m_scores.ok, m_scores.ng, t);
    m_matrixPlot->resetPlot();
    Plot& mp = m_matrixPlot->plot();
    mp.setStyle(m_dark ? PlotStyle::dark() : PlotStyle::light());
    mp.addConfusionMatrix({QStringLiteral("OK"), QStringLiteral("NG")}, s.counts());
    mp.title.text = tr("混淆矩陣（%1 片 OK、%2 片 NG）").arg(s.tn + s.fp).arg(s.tp + s.fn);

    if (m_rocPoint) {
        m_rocPoint->xs = {s.falseAlarmRate};
        m_rocPoint->ys = {1 - s.missRate};
    }

    m_summary->setText(
        tr("門檻 <b>%1</b>　　漏檢率 <b style='color:#d62728'>%2%</b>（%3 片 NG 被放過）　　"
           "誤判率 <b>%4%</b>（%5 片 OK 被打掉）　　精確率 %6%　　正確率 %7%")
            .arg(t, 0, 'f', 3)
            .arg(s.missRate * 100, 0, 'f', 1).arg(s.fn)
            .arg(s.falseAlarmRate * 100, 0, 'f', 1).arg(s.fp)
            .arg(s.precision * 100, 0, 'f', 1)
            .arg(s.accuracy * 100, 0, 'f', 1));

    for (PlotWidget* w : {m_imagePlot, m_histPlot, m_matrixPlot, m_rocPlot}) w->refresh();
}

}  // namespace demo
}  // namespace qtplot

// InspectionWindow.h — 瑕疵檢測門檻示範：四張圖跟著同一個門檻一起動。
//
//   左上：影像疊上異常熱圖，只露出超過門檻的區域並描出輪廓
//   右上：OK／NG 分數分布，門檻線可以直接拖
//   左下：這個門檻下的 OK／NG 混淆矩陣
//   右下：ROC 曲線與目前的工作點
//
// 一支範例只拿得到一個 PlotWidget，四張圖要聯動就得是獨立的視窗（和共用座標軸示範一樣）。
#pragma once

#include "InspectionData.h"
#include "qtplot/PlotWidget.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QSlider;

namespace qtplot {
namespace demo {

class InspectionWindow : public QWidget {
    Q_OBJECT
public:
    explicit InspectionWindow(bool dark, QWidget* parent = nullptr);

private:
    void buildImagePlot();
    void buildHistogram();
    void buildRoc();
    /// 門檻改了：四張圖與文字一起更新。fromLine 表示是拖門檻線造成的（不要再回頭設線的位置）。
    void applyThreshold(double t, bool fromLine = false);
    void loadSample(int index);

    bool m_dark = false;
    InspectionScores m_scores;
    InspectionSample m_sample;
    double m_threshold = 0.48;

    PlotWidget* m_imagePlot = nullptr;
    PlotWidget* m_histPlot = nullptr;
    PlotWidget* m_matrixPlot = nullptr;
    PlotWidget* m_rocPlot = nullptr;
    QComboBox* m_sampleBox = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_summary = nullptr;

    Heatmap* m_heat = nullptr;
    ContourLines* m_contour = nullptr;
    AxisLine* m_line = nullptr;
    Markers* m_rocPoint = nullptr;
    bool m_updating = false;
};

}  // namespace demo
}  // namespace qtplot

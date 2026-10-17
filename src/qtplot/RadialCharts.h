// RadialCharts.h — 環狀長條圖與環狀熱圖。
//
// 兩者都是「把一張直角座標的圖彎成一圈」：環狀長條圖的每個類別是一道同心環，
// 數值是沿環的弧長；環狀熱圖則是「環 × 扇區」的格子，一個扇區一個個體（例如一州）、
// 一道環一個屬性。
#pragma once

#include "Plottable.h"

namespace qtplot {

/// 環狀長條圖：每個類別一道同心環，數值是沿環畫出的弧，底下有一條灰色的軌道。
///
/// 同樣的角度在外環比內環長，讀者會高估外環的長條；
/// equalArcLength 讓「弧長」（而不是角度）和數值成正比。
class RadialBarChart : public IPlottable {
public:
    QVector<double> values;
    QStringList labels;
    /// 每條的顏色。缺的用 barColor；barColor 無效時用調色盤。
    QVector<QColor> colors;
    QColor barColor;
    Palette palette;

    Coordinates center{0, 0};
    double radius = 1;
    /// 中空部分佔半徑的比例。
    double innerRadiusFraction = 0.3;
    /// 相鄰兩環之間空出來的比例。
    double ringGapFraction = 0.3;

    /// 每條的起點（度，從三點鐘方向逆時針量；90 是十二點鐘）。
    double startAngle = 90;
    bool clockwise = true;
    /// 最大值（以及軌道）佔的角度。
    double sweepAngle = 270;
    /// 填滿整個 sweepAngle 的數值。NaN 時用最大值。
    double maxValue = std::numeric_limits<double>::quiet_NaN();
    /// 第一個數值畫在最外環。
    bool firstOutermost = true;
    /// 距離正確：弧長和數值成正比。
    bool equalArcLength = false;
    bool roundCaps = true;

    bool showTracks = true;
    QColor trackColor{232, 232, 232};

    bool showLabels = true;
    /// 標籤後面加上數值。
    bool showValues = true;
    /// 數值的小數位數。
    int valueDecimals = 0;
    /// 標籤格式。%1 是名稱、%2 是數值。
    QString labelFormat = QStringLiteral("%1 %2");
    FontStyle labelFont;

    Label centerLabel;
    /// 每一條都列進圖例。
    bool showInLegend = false;

    QColor color(int index) const;
    /// 某一條所在的環：中線半徑與厚度（皆為 radius 的比例）。
    void ring(int index, double& middle, double& thickness) const;
    /// 每一條畫出的角度（度）。
    QVector<double> sweeps() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(centerLabel.font);
    }
};

/// 環狀熱圖：「環 × 扇區」的格子彎成一圈。
///
/// 一個扇區是一個個體（例如一州），一道環是一個屬性，同一個扇區由內往外讀。
/// 放射狀的分隔線與群組名稱把扇區分成幾區。
class CircularHeatmap : public IPlottable {
public:
    int rings = 0;
    int sectors = 0;
    /// 每格的顏色，row-major 的 [ring][sector]，大小 rings × sectors；第 0 道環在最外面。
    /// 無效的顏色畫成 emptyColor。
    QVector<QColor> cells;
    /// 畫上放射狀細紋的格子（例如「明文禁止」），大小同 cells；空的話不用。
    QVector<bool> striped;
    QColor stripeColor{120, 160, 210};
    QColor emptyColor{236, 236, 236};

    QStringList sectorLabels;
    FontStyle sectorLabelFont;

    Coordinates center{0, 0};
    double radius = 1;
    double innerRadiusFraction = 0.4;
    double startAngle = 90;
    bool clockwise = true;
    /// 所有扇區加起來佔的角度（360 是整圈，180 是半圓的扇形圖）。
    double sweepAngle = 360;
    LineStyle cellBorder{QColor(255, 255, 255), 1.0};

    /// 每一格中央的文字，row-major 的 [環][扇區]，大小同 cells；空的話不畫。
    QStringList cellLabels;
    FontStyle cellLabelFont;
    /// 格子文字依底色自動選黑或白。
    bool autoContrastCellLabels = true;

    /// 在這些扇區之前畫一條放射狀分隔線。
    QVector<int> separators;
    LineStyle separatorLine{QColor(0, 0, 0), 2.5};
    /// 分隔線伸出圓外的長度（radius 的比例）。
    double separatorOverhang = 0.12;

    /// 群組名稱與它在第幾個扇區（可以是小數）。
    struct GroupLabel {
        QString text;
        double sector = 0;
    };
    QVector<GroupLabel> groupLabels;
    FontStyle groupLabelFont;

    Label centerLabel;
    /// 手動的圖例項目（例如每道環代表什麼）。
    QVector<LegendItem> legend;

    QColor cell(int ring, int sector) const;
    /// 某個（可為小數的）扇區位置對應的角度（度）。
    double sectorAngle(double sector) const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override { return legend; }
    void visitFonts(const FontVisitor& visit) override {
        visit(sectorLabelFont);
        visit(groupLabelFont);
        visit(cellLabelFont);
        visit(centerLabel.font);
    }
};

}  // namespace qtplot

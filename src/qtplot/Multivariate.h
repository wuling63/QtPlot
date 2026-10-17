// Multivariate.h — 多變量圖表：平行座標圖。
#pragma once

#include "Plottable.h"

namespace qtplot {

/// 平行座標圖的一個維度（一根軸）。
struct ParallelAxisSpec {
    QString label;
    /// 這根軸的範圍。無效時由資料自動決定。
    CoordinateRange range;
    /// 反向：大的值畫在下面。有些維度「小的比較好」，翻過來比較好讀。
    bool inverted = false;
    /// 刻度標籤要印幾個。<= 1 代表只印兩端。
    int tickCount = 5;
};

/// 平行座標圖：每個維度一根垂直軸，一筆資料畫成一條折線。
///
/// 多變量資料的標準工具。散點矩陣一次只能看兩個維度、而且面板數是維度的平方；
/// 平行座標一次看得到全部維度，代價是：
///
///   * 只看得出「相鄰」兩軸之間的關係，不相鄰的要自己換軸順序；
///   * 線一多就糊成一片，要靠半透明與著色分群才讀得出來。
///
/// 每根軸各自正規化到 [0, 1]，所以單位不同的維度可以放在一起。
class ParallelCoordinates : public IPlottable {
public:
    /// 維度定義。數量決定有幾根軸。
    QVector<ParallelAxisSpec> axes;
    /// 每一筆資料在各維度上的值。rows[i].size() 應該等於 axes.size()。
    QVector<QVector<double>> rows;
    /// 每一筆的顏色。不足的部分用 lineStyle 的顏色。
    QVector<QColor> rowColors;
    /// 每一筆的群組名稱，給圖例用。相同名稱只列一次。
    QStringList rowGroups;

    /// 整張圖畫在這個資料矩形裡。
    CoordinateRect extent{0, 1, 0, 1};

    LineStyle lineStyle{QColor(60, 110, 190, 110), 1.2};
    LineStyle axisLine{QColor(80, 80, 80), 1.2};
    FontStyle axisLabelFont;
    FontStyle tickFont;
    /// 軸標籤畫在軸的上方多遠（像素）。
    double axisLabelGapPx = 8;
    /// 刻度標籤畫在軸的左邊多遠（像素）。
    double tickGapPx = 5;

    /// 每根軸實際用的範圍（有指定就用指定的，否則取資料的極值）。
    QVector<CoordinateRange> resolvedRanges() const;
    /// 把第 dim 維的值換算成 extent 裡的 y 座標。
    double valueToY(int dim, double value) const;
    /// 第 dim 根軸的 x 座標。
    double axisX(int dim) const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(axisLabelFont);
        visit(tickFont);
    }
};

}  // namespace qtplot

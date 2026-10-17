// Bars3D.h — 立體（擠出）長條圖。
//
// 這是「看起來像 3D」而不是真的 3D：沒有相機、沒有透視，只是把每根長條往固定
// 方向平移一段距離，再把前面、頂面、側面三個多邊形畫出來。資訊圖表要的就是這個，
// 真正的 3D 投影反而會讓長度不能直接比較。
//
// 立體長條圖本身是會誤導的圖表：頂面是斜的，讀者不容易判斷頂端對到哪個刻度，
// 而且前排會遮住後排。要精確比較數值請用一般的 BarPlot；這個是給簡報與
// 資訊圖表用的。
#pragma once

#include "Plottable.h"

namespace qtplot {

/// 立體長條圖的一根長條。
struct Bar3D {
    double position = 0;   ///< 沿著類別軸的位置
    double value = 0;      ///< 長條頂端
    double valueBase = 0;  ///< 長條底端
    double size = 0.8;     ///< 寬度（類別軸的資料單位）
    QColor fillColor = QColor(31, 119, 180);
    QString label;         ///< 印在前面上的文字（例如編號）
};

/// 立體長條圖。
///
/// 每根長條畫成三個面：
///   * 前面 —— 正對讀者的矩形，可以加上下漸層
///   * 頂面 —— 往擠出方向推出去的平行四邊形，調亮
///   * 側面 —— 右側的平行四邊形，調暗
///
/// 擠出的量是「像素」而不是資料單位，縮放時立體感才不會跟著變形。代價是
/// axisLimits() 無法精確知道要留多少空間（那取決於繪圖區有多大），所以它用
/// depthPadFraction 留一個比例的餘裕。視野抓得很緊的時候請自己 setAxisLimits。
class Bars3D : public IPlottable {
public:
    QVector<Bar3D> bars;

    /// 擠出的長度（像素）。
    double depthPx = 26;
    /// 擠出的方向（度）。0 指向 +X，逆時針為正；預設 45 是往右上。
    double depthAngleDegrees = 45;

    /// 頂面調亮多少（QColor::lighter 的係數，100 = 不變）。
    int topLighten = 135;
    /// 側面調暗多少（QColor::darker 的係數，100 = 不變）。
    int sideDarken = 130;

    /// 前面由下往上的漸層強度。0 代表純色，1 代表上緣亮到接近白。
    double frontGradient = 0.35;

    LineStyle edgeLine{QColor(0, 0, 0, 0), 1.0};
    /// 長條上文字的樣式。
    FontStyle labelFont;
    /// 文字離長條底端多遠，以長條高度的比例表示。
    double labelPositionFraction = 0.08;

    /// 自動把資料範圍含進 0（長條圖從 0 起算才不會誤讀）。
    bool includeZero = true;
    /// 替擠出的部分多留的空間，佔資料範圍的比例。
    ///
    /// 擠出是以像素計的，所以正確的餘裕要知道繪圖區多大 —— 算 axisLimits 的
    /// 時候還不知道。這個比例是個夠用的估計值，不是精確解。
    double depthPadFraction = 0.12;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 由數值序列建一組等距長條（位置 0, 1, 2, ...），依序取調色盤的顏色。
    static QVector<Bar3D> fromValues(const QVector<double>& values, const Palette& palette);

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

}  // namespace qtplot

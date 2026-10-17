// Isometric.h — 等角投影（2.5D）的圖元族。
//
// 這一族和其他圖元的關係要先講清楚：它們畫的是「看起來立體」的簡報用圖，
// 不是用來精確讀數值的。等角投影之下：
//
//   * 同樣高的柱子，位置不同投影出來的長度一樣，但「頂面」是斜的，
//     讀者很難判斷頂端對到哪個刻度；
//   * 前排會遮住後排；
//   * 面積與體積都不正比於數值。
//
// 要比較數值請用 BarPlot、Funnel 這些平面的圖元。這一族的用途是版型。
//
// 投影約定：
//   screenX = (x - y) * cos(angle)
//   screenY = -(x + y) * sin(angle) + z * heightScale
// 也就是 +x 往右下、+y 往左下、+z 往上。x + y 愈大代表愈靠近觀眾，
// 所以畫的順序是 x + y 由小到大（先畫後面的）。
#pragma once

#include "Plottable.h"

namespace qtplot {

/// 等角投影：把世界座標 (x, y, z) 投到繪圖用的 2D 資料座標。
struct IsoProjection {
    /// 水平方向的夾角（度）。標準的等角投影是 30。
    double angleDegrees = 30;
    /// 高度在畫面上的縮放。調小會讓整體看起來比較扁。
    double heightScale = 1.0;
    /// 投影結果再平移到這裡。
    Coordinates origin{0, 0};

    Coordinates project(double x, double y, double z) const;
    /// 深度排序用的鍵：愈大代表愈靠近觀眾，要愈晚畫。
    static double depth(double x, double y) { return x + y; }
};

// ───────────────────────── 等角長條圖 ─────────────────────────

/// 等角長條圖的一根柱子。
struct IsoBar {
    /// 地面上的格座標。
    double x = 0;
    double y = 0;
    double height = 0;
    double sizeX = 0.7;
    double sizeY = 0.7;
    QColor color;
    QString label;  ///< 印在柱子頂端上方
};

/// 等角長條圖：柱子站在一塊地面上，可以排成一列也可以排成格子。
class IsoBars : public IPlottable {
public:
    QVector<IsoBar> bars;
    IsoProjection projection;

    /// 頂面調亮、兩個側面調暗的係數（QColor::lighter / darker 的單位）。
    ///
    /// 三個面用不同亮度是立體感的來源 —— 全部同色的話，
    /// 投影出來只是一堆重疊的平行四邊形，看不出是柱子。
    int topLighten = 118;
    int rightDarken = 118;
    int leftDarken = 152;

    LineStyle edgeLine{QColor(0, 0, 0, 0), 1.0};
    FontStyle labelFont;

    /// 地面格線。範圍自動取柱子的範圍再外擴半格。
    bool showGround = true;
    LineStyle groundLine{QColor(0, 0, 0, 45), 1.0};
    FillStyle groundFill;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 由一串數值建一列柱子（y 固定、x = 0, 1, 2, ...）。
    static QVector<IsoBar> fromValues(const QVector<double>& values, const Palette& palette,
                                      double y = 0);

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 等角堆疊體 ─────────────────────────

/// 堆疊體的一層。
struct IsoLayer {
    /// 這一層的高度。
    double height = 1;
    /// 底面與頂面的半徑。兩者相等就是柱體，頂面 0 就是錐體。
    double bottomRadius = 1;
    double topRadius = 1;
    QColor color;
    QString label;
};

/// 等角的堆疊體：一層一層疊上去的柱／錐／平截頭。
///
/// sides 決定底面是幾邊形：4 是方錐、6 是六角柱、48 以上看起來就是圓的。
/// 所以金字塔、圓柱、圓錐、漏斗都用同一個類別。
class IsoStack : public IPlottable {
public:
    QVector<IsoLayer> layers;
    IsoProjection projection;
    /// 擺在地面上的哪個位置。
    double x = 0;
    double y = 0;

    /// 底面的邊數。>= 24 看起來就是圓的。
    int sides = 48;
    /// 底面多邊形的起始角度（度）。方錐要轉 45 度才會正對讀者。
    double rotationDegrees = 0;

    /// 側面的明暗範圍：正對光源的那一面乘 maxLighten，背光面乘 minLighten。
    int maxLighten = 125;
    int minLighten = 62;
    /// 光源的方位角（度，0 = +x 方向）。
    ///
    /// 不能設成和視線同方向（45 度）：那樣兩個看得到的面受光完全相同，
    /// 畫出來是平的。預設 135 是從左上前方打光，兩面才分得出亮暗。
    double lightAngleDegrees = 135;
    /// 頂面的明暗。
    int topLighten = 135;

    LineStyle edgeLine{QColor(0, 0, 0, 0), 1.0};
    FontStyle labelFont;
    /// 標籤畫在層的右側多遠（像素）。
    double labelGapPx = 10;
    bool showLabels = true;

    /// 全部層的總高。
    double totalHeight() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};


// ───────────────────────── 立體曲面 ─────────────────────────

/// 立體曲面／線框圖。
///
/// 資料是一張 rows × cols 的高度格網，和 Heatmap 同一種排法（row-major）。
///
/// 要先講清楚它不是什麼：這是「等角投影」畫出來的 2.5D，不是真正的 3D 繪圖。
/// 沒有深度緩衝、沒有透視、不能互動旋轉 —— 遮擋靠的是畫家演算法
/// （由遠而近一格一格畫上去）。做簡報用的曲面圖夠用，
/// 要可旋轉檢視的工程圖請用真正的 3D 函式庫。
///
/// 畫家演算法的限制也要知道：格子彼此不相交時才正確。
/// 曲面自己摺回來穿過自己的話，會出現排序錯誤。一般的 z = f(x, y) 不會有這個問題。
class Surface3D : public IPlottable {
public:
    /// 高度值，row-major，大小必須是 rows × cols。
    QVector<double> values;
    int rows = 0;
    int cols = 0;

    /// 格網在世界座標 x / y 上的範圍。
    CoordinateRange xRange{0, 1};
    CoordinateRange yRange{0, 1};

    IsoProjection projection;

    enum class Mode {
        Surface,    ///< 填色的面，加上格線
        Wireframe,  ///< 只畫格線，看得到背面
        Points      ///< 只畫格點
    };
    Mode mode = Mode::Surface;

    Colormap colormap{Colormap::Viridis};
    /// 色階範圍。未設定時用資料的 min/max。
    CoordinateRange colorRange;

    LineStyle mesh{QColor(0, 0, 0, 90), 0.8};
    MarkerStyle pointMarker{MarkerShape::FilledCircle, 4, QColor(60, 90, 160)};

    /// 依朝向調整亮度（簡單的平行光）。0 關閉。
    ///
    /// 不打光的話，只靠顏色分不出「同樣高度但朝向不同」的兩片面，
    /// 曲面會看起來像一張皺掉的色紙而不是立體的。
    double shading = 0.35;
    /// 光從哪個方向來（度，和等角投影的畫面方向一致）。
    double lightAngleDegrees = 135;

    /// 只畫每隔幾格的格線（1 代表每格都畫）。格網很密時調大，否則線會糊成一片。
    int meshStride = 1;

    double value(int row, int col) const {
        return (row >= 0 && row < rows && col >= 0 && col < cols)
                   ? values.at(static_cast<qsizetype>(row) * cols + col)
                   : std::numeric_limits<double>::quiet_NaN();
    }
    /// 資料的高度範圍。
    CoordinateRange zRange() const;
    /// 實際用的色階範圍。
    CoordinateRange effectiveColorRange() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};

}  // namespace qtplot

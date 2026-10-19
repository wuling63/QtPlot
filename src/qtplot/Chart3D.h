// Chart3D.h — 真正的 3D 圖表：可從任意方向觀看的座標盒，裡面放散點、折線、曲面、瀑布曲線與立體柱。
//
// 和 Isometric.h 的等角投影不同：這裡的視角（方位角／仰角）可以任意調，
// 也可以開透視；滑鼠拖曳旋轉用 enableMouseRotation()。
//
// 所有東西投影到繪圖區之後由遠而近畫（畫家演算法）。這張圖不用 Plot 本身的座標軸
// —— 座標盒、格線、刻度都是自己畫的，addChart3D() 會把 Plot 的軸與格線關掉。
#pragma once

#include "Plottable.h"
#include "Specialty.h"  // Triangle、delaunay()

#include <QObject>
#include <functional>
#include <memory>

namespace qtplot {

class PlotWidget;

/// 3D 空間中的一點。
struct Point3D {
    double x = 0;
    double y = 0;
    double z = 0;
};

/// 3D 圖表裡的一組資料。
class Series3D {
public:
    virtual ~Series3D() = default;

    bool isVisible = true;
    /// 圖例文字。空字串不進圖例。
    QString name;

    /// 三軸的資料範圍（決定座標盒的大小）。沒有資料時三個範圍都無效。
    virtual void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const = 0;
    /// 要對到色階的數值範圍；不用色階的序列回傳無效範圍。
    virtual CoordinateRange colorRange() const { return {}; }
    virtual QVector<LegendItem> legendItems() const { return {}; }
};

/// 3D 散點。可依 Z 上色，也可以畫一條垂線到地板 —— 平面投影看不出點有多高，垂線補回這個資訊。
class Scatter3D : public Series3D {
public:
    QVector<Point3D> points;
    QColor color{31, 119, 180};
    double markerSize = 6;
    bool colorByZ = false;
    bool dropLines = false;
    QColor dropLineColor{128, 128, 128, 110};
    /// 標記的外框。寬度 <= 0 不畫。
    QColor edgeColor{255, 255, 255, 180};
    double edgeWidth = 0.6;
    /// 依深度調淡：越遠的點越透明（Matplotlib 的 depthshade）。
    ///
    /// 靜態的圖裡分不出點的前後，調淡之後一眼就看得出哪些點在後面。
    bool depthShade = false;
    /// 最遠的點剩下多少不透明度（0–1）。
    double depthShadeMinAlpha = 0.3;

    void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const override;
    CoordinateRange colorRange() const override;
    QVector<LegendItem> legendItems() const override;
};

/// 3D 折線（軌跡）。
class Line3D : public Series3D {
public:
    QVector<Point3D> points;
    QColor color{214, 39, 40};
    double width = 2;
    bool colorByZ = false;

    void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const override;
    CoordinateRange colorRange() const override;
    QVector<LegendItem> legendItems() const override;
};

/// 規則格網上的曲面 z = f(x, y)。zs 是 row-major，大小 ys.size() × xs.size()（列對應 y）。
class SurfaceSeries3D : public Series3D {
public:
    QVector<double> xs;
    QVector<double> ys;
    QVector<double> zs;
    /// 依每格的平均高度填色。false 時只畫線框，看得到背面。
    bool fill = true;
    /// 單一填色（不用色階）。無效時用色階。
    QColor fillColor;
    /// 依朝向調亮暗，沒有它曲面看起來像一張皺掉的色紙。
    bool lighting = true;
    QColor wireColor{0, 0, 0, 60};
    double wireWidth = 0.5;

    double z(int row, int col) const;
    void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const override;
    CoordinateRange colorRange() const override;
};

/// 三角網格曲面：任意的頂點加三角形（Matplotlib 的 plot_trisurf）。
///
/// 參數曲面（莫比烏斯帶、環面、球面、管子）要用這個：它們不是 z = f(x, y)，
/// 同一個 (x, y) 可能對到好幾個高度，SurfaceSeries3D 的規則格網畫不出來。
/// 做法和 Matplotlib 一樣：先在參數空間 (u, v) 做三角剖分（triangulateParameters），
/// 再把每個頂點算成 3D 座標放進 vertices。
///
/// 每個三角形依三個頂點的平均高度取色、依朝向打光。打光是雙面的 ——
/// 莫比烏斯帶這種曲面沒有正反面之分，只照一面的話有一半會是黑的。
class TriSurface3D : public Series3D {
public:
    QVector<Point3D> vertices;
    QVector<Triangle> triangles;  ///< 頂點索引
    bool fill = true;
    /// 單一填色（不用色階）。無效時用色階。
    QColor fillColor;
    bool lighting = true;
    /// 三角形的邊。寬度 <= 0 時不畫邊。
    QColor edgeColor{0, 0, 0, 110};
    double edgeWidth = 0.4;

    /// 在參數座標上做 Delaunay 三角剖分，結果放進 triangles。
    /// uv 的第 i 點對應 vertices 的第 i 點（和 Matplotlib 的 Triangulation(u, v) 相同）。
    void triangulateParameters(const QVector<Coordinates>& uv);

    void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const override;
    CoordinateRange colorRange() const override;
};

/// 3D 等值線：每一條等值線抬到它自己的高度（Matplotlib 的 contour3D）。
///
/// 格網和 SurfaceSeries3D 一樣：zs 是 row-major，大小 ys.size() × xs.size()，xs、ys 要等間距。
/// 等值線多畫幾條（30–50 條）就看得出曲面的形狀，又不會像填色曲面那樣擋住後面。
class Contour3D : public Series3D {
public:
    QVector<double> xs;
    QVector<double> ys;
    QVector<double> zs;
    /// 等值線的條數（在 Z 的範圍內等距取）。levels 不是空的時候用 levels。
    int levelCount = 10;
    QVector<double> levels;
    /// 依高度用色階上色；false 時一律用 color。
    bool colorByLevel = true;
    QColor color{31, 119, 180};
    double width = 1;

    /// 實際用的等值。
    QVector<double> effectiveLevels() const;
    void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const override;
    CoordinateRange colorRange() const override;
};

/// 一條條曲線沿 Y 往後排，每條填到地板（3D 瀑布圖／3D 山脊圖）。
class Waterfall3D : public Series3D {
public:
    QVector<double> xs;           ///< 所有曲線共用的 X
    QVector<double> ys;           ///< 每條曲線的 Y 位置
    QVector<QVector<double>> zs;  ///< 每條曲線的 Z
    Palette palette;
    int fillAlpha = 215;
    QColor edgeColor{255, 255, 255};
    double edgeWidth = 1.2;

    /// 每條曲線沿 Y 的厚度（資料單位）。0 是平面曲線；大於 0 時每條變成緞帶
    /// （fillToFloor 關掉，像試算表的 3D 折線圖）或一塊實心的板子（3D 面積圖）。
    double thickness = 0;
    /// 每條曲線填到地板。有厚度但關掉時只畫頂面的緞帶。
    bool fillToFloor = true;
    /// 每條曲線在圖例裡的名稱（空字串不進圖例）。
    QStringList curveNames;

    void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const override;
    QVector<LegendItem> legendItems() const override;
};

/// 立在地板上的立體柱。每根柱的 (x, y) 是底面中心，z 是高度。
class Columns3D : public Series3D {
public:
    QVector<Point3D> columns;
    /// 柱寬佔最小間距的比例。
    double widthFraction = 0.7;
    QColor color{66, 133, 194};
    bool colorByZ = false;
    QColor edgeColor{0, 0, 0, 70};

    double columnWidth() const;
    void limits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const override;
    CoordinateRange colorRange() const override;
    QVector<LegendItem> legendItems() const override;
};

class Chart3D : public IPlottable {
public:
    std::vector<std::unique_ptr<Series3D>> series;

    /// 繞垂直軸的旋轉角（度）。
    double azimuth = -55;
    /// 視點高度（度）。0 從側面看，90 從正上方看。
    double elevation = 25;
    /// 透視投影（遠的東西看起來比較小）。
    bool perspective = false;
    /// 座標盒 X / Y / Z 三邊的相對長度。
    double aspectX = 1, aspectY = 1, aspectZ = 0.7;

    /// 固定的軸範圍。無效時用資料範圍。
    CoordinateRange xRange, yRange, zRange;

    Colormap colormap{Colormap::Viridis};
    /// 色階範圍。無效時用所有上色序列的聯集。
    CoordinateRange colorRange;

    QString xLabel = QStringLiteral("X");
    QString yLabel = QStringLiteral("Y");
    QString zLabel = QStringLiteral("Z");
    /// 自訂刻度文字（例如月份名稱）。沒設時用數值。
    std::function<QString(double)> xTickFormatter, yTickFormatter, zTickFormatter;

    bool showBox = true;
    bool showGrid = true;
    QColor paneColor{242, 242, 245};
    QColor gridColor{205, 205, 210};
    QColor boxEdgeColor{150, 150, 155};
    FontStyle tickFont;
    FontStyle axisLabelFont;
    /// 繪圖區四周留給刻度與軸名的比例。
    double margin = 0.2;

    Chart3D();

    /// 加一組資料，回傳同一個指標方便繼續設樣式。
    template <typename T>
    T* add(std::unique_ptr<T> s) {
        T* raw = s.get();
        series.push_back(std::move(s));
        return raw;
    }

    /// 實際用的三軸範圍。
    void effectiveLimits(CoordinateRange& x, CoordinateRange& y, CoordinateRange& z) const;
    CoordinateRange effectiveColorRange() const;

    /// 1 / 2 / 2.5 / 5 × 10^n 的整齊刻度。
    static QVector<double> niceTicks(const CoordinateRange& range, int target = 5);

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    void visitFonts(const FontVisitor& visit) override {
        visit(tickFont);
        visit(axisLabelFont);
    }
};

/// 讓滑鼠拖曳旋轉這張 3D 圖（左右拖改方位角、上下拖改仰角，滾輪也改仰角），
/// 並關掉 widget 的平移與縮放 —— 兩種拖曳行為同時開著會打架。
void enableMouseRotation(PlotWidget& widget, Chart3D* chart);

}  // namespace qtplot

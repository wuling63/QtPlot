// Specialty.h — 特殊座標系：史密斯圖、三角圖、相量圖、不規則三角網。
//
// 這幾個和極座標族一樣，底下仍然是直角座標：各自把自己的座標系換算成 (x, y)
// 再交給 Transform。所以縮放、平移、輸出全部不用特別處理。
#pragma once

#include "Plottable.h"

namespace qtplot {

// ───────────────────────── 不規則三角網 ─────────────────────────

/// 三角形（存的是點的索引）。
struct Triangle {
    int a = 0;
    int b = 0;
    int c = 0;
};

/// Delaunay 三角化（Bowyer–Watson）。
///
/// 回傳的三角形以索引表示。少於 3 點、或所有點共線時回傳空的結果 ——
/// 共線的點沒有唯一的三角化，硬算會得到退化的三角形。
QVector<Triangle> delaunay(const QVector<Coordinates>& points);

/// 不規則三角網（TIN）：把散佈的 (x, y, z) 取樣點三角化後上色。
///
/// 和 Heatmap 的差別是資料不必落在規則格點上 —— 地形測量、感測器佈點
/// 這類資料本來就是散的，內插成格點反而會造假。
class TIN : public IPlottable {
public:
    QVector<Coordinates> points;
    QVector<double> zs;  ///< 長度必須和 points 相同

    Colormap colormap{Colormap::Viridis};
    /// 色階範圍。未設定時用 zs 的 min/max。
    CoordinateRange colorRange;
    /// 填滿三角形（以三個頂點 z 的平均取色）。
    bool filled = true;
    /// 畫出三角形的邊。
    LineStyle wireframe{QColor(0, 0, 0, 60), 0.8};
    /// 畫出取樣點。
    MarkerStyle marker;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 取得三角化結果（會快取，points 改變後自己呼叫 invalidate()）。
    const QVector<Triangle>& triangles() const;
    void invalidate() { m_cached = false; }

private:
    mutable QVector<Triangle> m_triangles;
    mutable bool m_cached = false;
};

// ───────────────────────── 三角圖 ─────────────────────────

/// 三角圖（ternary）的座標軸：三個成分加起來固定為 100%。
///
/// 常用於土壤分類、合金成分、地質。資料用 TriangularAxis::toCartesian()
/// 換算成 (x, y) 之後，用一般的 Scatter／Markers 畫上去。
class TriangularAxis : public IPlottable {
public:
    /// 三個角的標籤（順序：下左、下右、上）。
    QStringList cornerLabels{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")};
    /// 內部格線的層數。
    int gridCount = 4;

    LineStyle edgeLine{QColor(0, 0, 0), 1.5};
    LineStyle gridLine{QColor(0, 0, 0, 45), 1.0};
    FontStyle labelFont;
    bool showTickLabels = true;

    /// 三角形內部的底色。
    FillStyle background;
    /// 逐邊指定樣式（順序：底邊、右邊、左邊）。不足的用 edgeLine。
    QVector<LineStyle> edgeStyles;
    /// 每一邊的標題，沿著邊寫（順序：底邊、右邊、左邊）。
    QStringList edgeTitles;
    FontStyle edgeTitleFont;
    /// 三邊都標刻度（預設只標底邊）。
    bool tickLabelsOnAllEdges = false;
    /// 刻度讀法的方向。
    ///
    /// false（逆時針）：底邊讀 B、右邊讀 C、左邊讀 A —— 每條邊標的是它「走向」的那個角。
    /// true（順時針）：底邊讀 A、右邊讀 B、左邊讀 C。兩種慣例教科書都有，
    /// 讀者用哪一種就要配哪一種，否則會把成分讀反。
    bool clockwise = false;

    /// 把三成分換算成直角座標。a + b + c 不必等於 1，會自動正規化。
    /// 回傳的三角形底邊從 (0,0) 到 (1,0)，頂點在 (0.5, √3/2)。
    static Coordinates toCartesian(double a, double b, double c);

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(edgeTitleFont);
    }
};

// ───────────────────────── 史密斯圖 ─────────────────────────

/// 史密斯圖的座標軸：等電阻圓與等電抗弧。
///
/// 射頻阻抗匹配用。資料用 SmithChartAxis::toCartesian() 把正規化阻抗
/// 換成反射係數座標後畫上去。
class SmithChartAxis : public IPlottable {
public:
    /// 要畫哪幾條等電阻圓（正規化電阻 r）。
    QVector<double> resistances{0.2, 0.5, 1.0, 2.0, 5.0};
    /// 要畫哪幾條等電抗弧（正規化電抗 x，會同時畫 +x 與 -x）。
    QVector<double> reactances{0.2, 0.5, 1.0, 2.0, 5.0};

    /// 細格線。實際的史密斯圖紙上這一層密得多，讀圖時才內插得出中間的值。
    QVector<double> minorResistances{0.1, 0.3, 0.4, 0.6, 0.7, 0.8, 0.9,
                                     1.2, 1.4, 1.6, 1.8, 3.0, 4.0, 10.0, 20.0};
    QVector<double> minorReactances{0.1, 0.3, 0.4, 0.6, 0.7, 0.8, 0.9,
                                    1.2, 1.4, 1.6, 1.8, 3.0, 4.0, 10.0, 20.0};

    /// 等駐波比圓（以 |Γ| 為半徑的同心圓）。留空則不畫。
    ///
    /// 給的是 VSWR 而不是 |Γ| —— 射頻量測讀的是 VSWR，換算 |Γ| = (S-1)/(S+1)
    /// 由圖元自己做。
    QVector<double> vswrCircles;

    Coordinates center{0, 0};
    double radius = 1.0;

    LineStyle outerLine{QColor(0, 0, 0), 1.5};
    LineStyle gridLine{QColor(0, 0, 0, 70), 1.0};
    LineStyle minorGridLine{QColor(0, 0, 0, 30), 1.0};
    LineStyle vswrLine{QColor(200, 60, 60, 160), 1.0, LinePattern::Dashed};
    FontStyle labelFont;
    bool showLabels = true;
    /// 電抗值要不要標在圓周外側。
    bool showReactanceLabels = true;

    /// 正規化阻抗 z = r + jx 換算成反射係數 Γ = (z-1)/(z+1)。
    static Coordinates toCartesian(double r, double x);

    /// 同上，但套用了 center 與 radius，可以直接拿來畫資料。
    Coordinates coordinates(double r, double x) const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 相量圖 ─────────────────────────

/// 一個相量（複數）。
struct Phasor {
    double magnitude = 1;
    double angleDegrees = 0;
    QColor color;
    QString label;

    Coordinates toCartesian() const { return fromPolar(magnitude, angleDegrees); }
};

/// 相量圖：從原點射出的一組向量，用來表示交流電的電壓電流關係。
class PhasorPlot : public IPlottable {
public:
    QVector<Phasor> phasors;
    double lineWidth = 2.0;
    double headLength = 11;
    double headWidth = 8;
    bool showLabels = true;
    FontStyle labelFont;
    /// 畫出每個相量在實軸／虛軸上的投影（虛線）。
    bool showProjections = false;
    LineStyle projectionLine{QColor(0, 0, 0, 90), 1.0, LinePattern::Dotted};

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

}  // namespace qtplot

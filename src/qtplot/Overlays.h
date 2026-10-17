// Overlays.h — 疊在散點圖上的輔助圖元：沿線文字的直線、每格自訂顏色的格子。
#pragma once

#include "Plottable.h"

#include <QImage>

namespace qtplot {

/// 一段直線，沿著線寫一段文字（例如迴歸線旁的「r = -0.52」）。
///
/// 文字角度是在畫的當下依「螢幕上」的斜率算的 —— 用資料座標算的話，兩軸比例不同，
/// 文字會和線歪成不同角度；視窗一縮放就更對不上。
class LabeledLine : public IPlottable {
public:
    Coordinates start;
    Coordinates end;
    LineStyle line{QColor(0, 0, 0), 1.5};

    QString text;
    FontStyle font;
    /// 文字放在線上的哪裡：0 是起點、1 是終點。
    double textPosition = 0.9;
    /// 文字離線多遠（像素）。負值放在線的下方。
    double textOffsetPx = 4;
    /// 文字用線的顏色。
    bool textUsesLineColor = true;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    void visitFonts(const FontVisitor& visit) override { visit(font); }
};

/// 通過兩點、往兩端無限延伸的直線（趨勢線）。兩個點都可以用滑鼠拖。
///
/// 和 Arrow 拉出來的線段不同，它畫滿整個繪圖區 —— 拿來「目測」趨勢、
/// 或比對某條理論直線時，使用者要看的是整條線，不是兩點之間那一段。
class TrendLine : public IPlottable {
public:
    Coordinates point1{0, 0};
    Coordinates point2{1, 1};
    LineStyle line{QColor(214, 39, 40), 2.0};
    /// 兩個控制點的樣子。不想顯示就設成 MarkerStyle::none()。
    MarkerStyle handleMarker{MarkerShape::FilledCircle, 9, QColor(214, 39, 40)};
    /// 可以拖動控制點（以及抓住線本身平移整條線）。
    bool draggable = true;

    /// 斜率（資料單位）。兩點的 X 相同時是無限大。
    double slope() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
    bool isDraggable() const override { return draggable; }
    bool isGrabbed(Coordinates at, const Transform& t, double tolerancePx) const override;
    void beginDrag(Coordinates at, const Transform& t, double tolerancePx) override;
    void dragBy(Coordinates delta) override;

private:
    int m_dragPart = 0;  ///< 0 整條、1 第一點、2 第二點
};

/// 每一格各自指定顏色的格子（熱圖則是經過色階換算）。
///
/// 分類區域、類別地圖、或任何事先算好顏色的影像式資料都用這個。
/// colors 是 row-major，第 0 列畫在 extent 的最上面。
class ColorGrid : public IPlottable {
public:
    int rows = 0;
    int cols = 0;
    QVector<QColor> colors;
    /// 整張格子佔的資料範圍（格子的外緣，不是中心）。
    CoordinateRect extent{0, 1, 0, 1};
    /// 相鄰格子平滑過渡，而不是一塊一塊的方格。
    bool smooth = false;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};

}  // namespace qtplot

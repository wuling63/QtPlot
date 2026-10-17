// Decorations.h — 裝飾類圖元：色階圖例、影像標記、浮動座標軸。
//
// 這幾個的共同點是「不屬於資料本身」，而是幫助讀圖的輔助元素。
#pragma once

#include "Plottable.h"

#include <QImage>

namespace qtplot {

/// 色階圖例：一條色帶加刻度，說明 Heatmap／ContourLines／TIN 的顏色代表什麼值。
///
/// 畫在繪圖區「外側」，版面會自動替它留出空間（outsideMargin）。
/// 沒有它的話，熱圖的顏色只能看出相對高低，讀不出實際數值。
class Colorbar : public IPlottable {
public:
    Colormap colormap{Colormap::Viridis};
    /// 色帶對應的數值範圍。無效時不畫。
    CoordinateRange range;

    /// 貼在哪一邊。目前支援 Right 與 Left（垂直色帶）、Bottom 與 Top（水平色帶）。
    Edge edge = Edge::Right;
    /// 色帶的厚度（像素）。
    double thickness = 18;
    /// 離繪圖區多遠（像素）。
    double gap = 12;
    /// 佔該邊長度的比例（0–1），置中對齊。
    double lengthFraction = 1.0;

    Label label;  ///< 色帶旁的說明文字（垂直色帶會自動轉 90°）
    FontStyle tickFont;
    /// 刻度產生器。和座標軸用同一套 —— 直接把範圍等分會得到
    /// 0.0713 / 0.303 / 0.535 這種刻度，整齊的數字要靠 1/2/5 的挑選規則。
    /// 想自己指定刻度就換成 ManualTickGenerator。
    std::shared_ptr<TickGenerator> tickGenerator;
    LineStyle border{QColor(0, 0, 0, 120), 1.0};
    /// 刻度線的長度（像素）。
    double tickLength = 4;

    /// 直接沿用某個熱圖的色階與數值範圍，不必手動對齊兩邊的設定。
    void follow(const class Heatmap& heatmap);
    void follow(const class ContourLines& contour);
    void follow(const class TIN& tin);
    void follow(const class Hexbin& hexbin);
    void follow(const class Chart3D& chart);

    Colorbar();

    bool isVertical() const { return edge == Edge::Left || edge == Edge::Right; }

    AxisLimits axisLimits() const override { return AxisLimits(); }
    bool clipToDataArea() const override { return false; }
    double outsideMargin(Edge e) const override;
    void setOutsideOffset(Edge e, double px) override;
    void render(RenderPack& rp) override;

private:
    /// 上一次畫出來的色帶長度（像素）。版面計算要用它估刻度標籤有多寬，
    /// 而長度又取決於版面 —— 和座標軸一樣靠兩段式版面收斂。
    mutable double m_lastLengthPx = 300;
    /// 同一邊在這個色帶之前已經被佔掉多少像素（座標軸、其他色帶）。
    double m_offsetPx = 0;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(label.font);
        visit(tickFont);
    }
};

/// 用影像當標記：在每個資料點畫一張小圖。
///
/// 地圖上的圖釘、流程圖的節點圖示、散點圖用縮圖代表每個樣本都靠這個。
class ImageMarker : public IPlottable {
public:
    QVector<double> xs;
    QVector<double> ys;
    QImage image;
    /// 繪製尺寸（像素）。<= 0 代表用影像的原始大小。
    double width = 0;
    double height = 0;
    /// 以資料點為中心（false 則以左上角對齊）。
    bool centered = true;
    double opacity = 1.0;
    bool smooth = true;
    /// 每隔幾個點畫一個（資料很多時避免糊成一片）。
    int stride = 1;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    HitResult hitTest(Coordinates target, const Transform& t,
                      HitMode mode = HitMode::NearestXY) const override;
};

/// 浮動座標軸：畫在繪圖區「內部」任意位置的一根軸。
///
/// 把 X 軸畫在 y = 0 上（而不是圖的底部）這種需求就用它 —— 資料有正有負時，
/// 軸畫在零線上比畫在邊緣好讀。
class FloatingAxis : public IPlottable {
public:
    /// true 為水平軸（畫在某個 y 上），false 為垂直軸（畫在某個 x 上）。
    bool horizontal = true;
    /// 軸所在的位置（水平軸是 y 值，垂直軸是 x 值）。
    double position = 0;
    /// 軸的範圍。無效時用所綁軸的完整範圍。
    CoordinateRange extent;

    LineStyle line{QColor(0, 0, 0), 1.2};
    LineStyle tickLine{QColor(0, 0, 0), 1.0};
    double tickLength = 4;
    /// 刻度與標籤畫在軸的哪一側（正負號決定方向）。
    double labelSide = 1;
    bool showTickLabels = true;
    FontStyle tickFont;
    Label label;
    /// 刻度產生器。預設用自動的那個。
    std::shared_ptr<TickGenerator> tickGenerator;

    FloatingAxis();

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(tickFont);
        visit(label.font);
    }
};

}  // namespace qtplot

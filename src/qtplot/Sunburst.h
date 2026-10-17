// Sunburst.h — 階層式圖表：旭日圖（環圈）與樹狀圖（矩形）。
//
// 一層一個環：最內圈是最上層的分類，往外每一圈是它的下一層。每個扇形的角度
// 正比於它底下所有葉節點的總和，所以同一圈的角度加起來剛好是整圈。
//
// 和樹狀圖（treemap）比，旭日圖的好處是「層級」一眼看得出來；壞處是外圈的
// 扇形半徑大、面積被放大，容易讓人高估外層的量。要比較數量請看角度，不是面積。
#pragma once

#include "Plottable.h"

namespace qtplot {

/// 旭日圖的一個節點。
struct SunburstNode {
    QString label;
    /// 葉節點的數值。有子節點時這個值被忽略，改用子節點的總和 ——
    /// 父節點自己填一個對不上子節點的數字，整圈的角度就會兜不攏。
    double value = 0;
    /// 無效色代表自動配色：最內圈取調色盤，往外每層在父節點的顏色上調亮。
    QColor fillColor;
    QVector<SunburstNode> children;

    /// 這個節點代表的總量（葉節點是 value，否則是子節點的總和）。
    double total() const;
    /// 以這個節點為根的層數（自己算一層）。
    int depth() const;
};

/// 旭日圖。
class Sunburst : public IPlottable {
public:
    QVector<SunburstNode> roots;

    Coordinates center{0, 0};
    double radius = 1.0;
    /// 中間留白的半徑，佔 radius 的比例。0 就是從圓心開始畫。
    double innerFraction = 0.25;

    double startAngle = 90;
    bool clockwise = true;

    LineStyle border{QColor(255, 255, 255), 1.5};
    FontStyle labelFont;
    /// 弧長短於這個像素數就不印標籤（印了也只會疊成一團）。
    double minLabelArcPx = 16;
    /// 標籤怎麼擺。
    enum class LabelOrientation {
        Tangential,  ///< 整段文字轉到該處的切線方向（內圈、字短時夠用）
        Curved,      ///< 逐字沿著弧彎曲（外圈的長標籤要用這個才貼得住弧）
        Radial,      ///< 沿著半徑、由內往外讀（風味輪那種多層圖的外圈慣用）
        Horizontal   ///< 一律水平
    };
    LabelOrientation labelOrientation = LabelOrientation::Tangential;

    /// 舊介面：true 等同 Tangential、false 等同 Horizontal。
    /// 設了 labelOrientation 之後就不看它了。
    bool rotateLabels = true;
    /// 每往外一層，顏色調亮多少（QColor::lighter 的係數增量）。
    int lightenPerLevel = 18;
    /// 最內圈的自動配色來源。子節點由父節點的顏色調亮而來。
    Palette palette{Palette::Category10};

    /// 整張圖共有幾層。
    int depth() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 樹狀圖（Treemap） ─────────────────────────

/// 樹狀圖的一個節點。
struct TreemapNode {
    QString label;
    /// 葉節點的數值。有子節點時被忽略，改用子節點的總和。
    double value = 0;
    QColor fillColor;
    QVector<TreemapNode> children;

    double total() const;
};

/// 樹狀圖：用矩形的面積表示數量，階層用巢狀表示。
///
/// 和旭日圖的分工：旭日圖的「層級」一眼看得出來，但外圈的扇形半徑大、面積被
/// 放大；樹狀圖的面積精確正比於數值，但層級要靠巢狀與留白才看得出來。
/// 項目很多、而且要比較大小的時候用樹狀圖；層級深、要看結構的時候用旭日圖。
///
/// 版面用 squarified 演算法（Bruls–Huizing–van Wijk）：盡量讓每個矩形接近正方形。
/// 長條狀的矩形很難比較面積 —— 讀者會改用長邊去比，那就讀錯了。
class Treemap : public IPlottable {
public:
    QVector<TreemapNode> roots;

    /// 整張圖畫在這個資料矩形裡。
    CoordinateRect extent{0, 1, 0, 1};

    /// 同一層的矩形之間留的空隙，佔 extent 短邊的比例。
    double paddingFraction = 0.004;
    /// 進入子層時再往內縮的留白，佔 extent 短邊的比例。層級就是靠這個看出來的。
    double groupPaddingFraction = 0.012;

    LineStyle border{QColor(255, 255, 255), 1.0};
    FontStyle labelFont;
    /// 群組（非葉節點）的標籤字型。
    FontStyle groupLabelFont;
    /// 矩形小於這個像素面積就不標文字。
    double minLabelAreaPx = 700;
    /// 數值要不要接在標籤後面。
    bool showValues = true;
    /// 每往內一層，顏色調亮多少。沒指定顏色的子節點由父節點的顏色推出來。
    int lightenPerLevel = 14;
    /// 最外層的自動配色來源。
    Palette palette{Palette::Category10};

    /// 版面算出來的一塊。
    struct Cell {
        CoordinateRect rect;
        int depth = 0;       ///< 0 是最外層
        bool isLeaf = true;
        QString label;
        double value = 0;
        QColor color;
    };

    /// 算出所有矩形（含群組本身）。外層在前、內層在後，照這個順序畫就對了。
    ///
    /// 公開是為了能單獨驗證 —— squarified 的面積分配只要錯一點，
    /// 圖上看起來還是一堆方塊，但比例已經不對了，眼睛分不出來。
    QVector<Cell> layout() const;

    /// 全部的總量。
    double grandTotal() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(groupLabelFont);
    }
};

}  // namespace qtplot

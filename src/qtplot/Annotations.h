// Annotations.h — 標註類圖元：引線標註、區間括號、比例尺、影像。
#pragma once

#include "Plottable.h"

#include <QImage>
#include <QPolygonF>

namespace qtplot {

/// 引線標註：一個文字框加一條線指向資料上的某一點。
///
/// 和 Text 的差別是文字和被標註的點可以分開 —— 點附近很擠的時候，
/// 文字放遠一點再用引線連回去，才不會蓋住資料。
class Callout : public IPlottable {
public:
    /// 引線指向的資料點。
    Coordinates target;
    /// 文字框的位置。
    Coordinates textPosition;
    Label label;

    FillStyle background{QColor(255, 255, 255, 235)};
    LineStyle border{QColor(0, 0, 0, 120), 1.0};
    LineStyle leader{QColor(0, 0, 0, 160), 1.0};
    double padding = 4;
    /// 引線末端畫箭頭。
    bool arrowHead = true;
    double headLength = 8;
    double headWidth = 6;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(label.font);
    }
};

/// 氣泡標註：一個圓角方框帶著一條尖尾巴指向資料上的某一點。
///
/// 和 Callout 的差別在尾巴是「框的一部分」而不是另外一條引線 —— 框和尾巴
/// 合成同一條外框線、填同一個色，看起來像對話框。資料點很密的時候，
/// 一塊實心的氣泡比一條細引線好認；要標很遠的點、或不想遮住資料，用 Callout。
///
/// 尾巴貼在框的哪一邊是算出來的，不用設：框心指向 tip 的方向落在哪條邊上，
/// 尾巴就從那裡長出去。所以 tip 繞著框轉一圈，尾巴會跟著繞。
class Tooltip : public IPlottable {
public:
    /// 尾巴尖端指向的資料點。
    Coordinates tip;
    /// 方框的中心位置。
    Coordinates labelPosition;
    Label label;

    FillStyle background{QColor(255, 255, 255, 235)};
    LineStyle border{QColor(0, 0, 0, 160), 1.0};
    /// 文字與框線之間的留白（像素）。
    double padding = 5;
    /// 圓角半徑（像素）。0 就是直角。
    double cornerRadius = 4;

    /// 尾巴根部的寬度，以「框的短邊」的比例表示（0–1）。
    ///
    /// 用短邊而不是固定像素，是為了讓尾巴跟著框等比例縮放：文字一多框變寬，
    /// 尾巴若用固定寬度看起來就會過細。取短邊則是因為尾巴可能長在任何一條邊上，
    /// 用長邊當基準時，貼在短邊上的尾巴會寬到超出那條邊。
    double tailWidth = 0.5;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 算出尾巴的三角形（像素座標）：兩個根部點在 box 邊界上，頂點就是 tip。
    /// tip 在 box 裡面時回傳空的多邊形（沒有尾巴可畫）。
    ///
    /// 公開是為了能單獨驗證幾何 —— 這段算錯的時候，圖上只是「看起來怪怪的」，
    /// 肉眼很難判斷根部到底寬了幾個像素。
    static QPolygonF tailPolygon(const QRectF& box, QPointF tip, double tailWidth);

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(label.font);
    }
};

/// 區間括號：標出「從這裡到那裡」的一段，中間放文字。
///
/// 常用來標顯著性差異、時間區段、尺寸標註。
class Bracket : public IPlottable {
public:
    Coordinates start;
    Coordinates end;
    Label label;
    LineStyle line{QColor(0, 0, 0), 1.2};
    /// 兩端往外伸出的鬚長（像素）。正負號決定往哪一側伸。
    double tipLength = 8;
    /// 文字離括號多遠（像素）。
    double labelGap = 3;
    /// true 時括號是水平的（鬚朝上或下），false 則是垂直的。
    bool horizontal = true;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(label.font);
    }
};

/// 比例尺：釘在角落、長度代表固定資料距離的一條帶刻度的線。
///
/// 影像或地圖類的圖通常不畫座標軸，改用比例尺說明尺度。
class ScaleBar : public IPlottable {
public:
    /// 水平方向代表的資料長度。<= 0 代表不畫水平的那一段。
    double width = 1.0;
    /// 垂直方向代表的資料長度。<= 0 代表不畫垂直的那一段。
    double height = 0.0;
    QString widthLabel;   ///< 空字串則自動用數值
    QString heightLabel;

    Qt::Alignment corner = Qt::AlignRight | Qt::AlignBottom;
    double marginX = 14;
    double marginY = 14;
    LineStyle line{QColor(0, 0, 0), 2.0};
    FontStyle font;
    double labelGap = 3;

    AxisLimits axisLimits() const override { return AxisLimits(); }
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(font);
    }
};

/// 把一張影像畫在資料座標的矩形裡。
///
/// 和 Heatmap 的差別是這裡的來源是現成的 QImage（照片、地圖、顯微影像），
/// 不是數值陣列。常當作底圖，上面再疊資料。
class ImageRect : public IPlottable {
public:
    QImage image;
    CoordinateRect rect;
    /// 保持影像的長寬比，在 rect 內置中（像 QImage::KeepAspectRatio）。
    bool keepAspectRatio = true;
    /// 放大時做平滑內插。
    bool smooth = true;
    double opacity = 1.0;
    LineStyle border{QColor(0, 0, 0, 0), 1.0};

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 實際會畫到的資料矩形（keepAspectRatio 時會比 rect 小）。
    CoordinateRect fittedRect(const Transform& t) const;
};

}  // namespace qtplot

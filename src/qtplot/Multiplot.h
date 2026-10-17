// Multiplot.h — 一張畫布放多張圖（子圖），以及嵌在圖裡的小圖。
//
// Plot 本身就能畫到任意 QPainter 的任意矩形，所以這裡不需要另一套渲染路徑：
// Multiplot 只負責把畫布切成格子、對齊各子圖的邊界，再請每張 Plot 自己畫。
// InsetPlot 同理 —— 它是個圖元，render() 時把內層的 Plot 畫進一個小矩形。
#pragma once

#include "Plot.h"

#include <QWidget>

#include <memory>
#include <vector>

namespace qtplot {

// ───────────────────────────── 子圖 ─────────────────────────────

/// 一張畫布上的多張圖。
///
/// 格子用「列 × 欄」指定，可以跨格。各格的 Plot 由 Multiplot 擁有，
/// 用 at() 取出來之後就和單獨一張圖完全一樣 —— 所有 add*、樣式、軸設定都通用。
class Multiplot {
public:
    explicit Multiplot(int rows = 1, int cols = 1);
    ~Multiplot();
    Multiplot(const Multiplot&) = delete;
    Multiplot& operator=(const Multiplot&) = delete;

    int rowCount() const { return m_rows; }
    int columnCount() const { return m_cols; }
    /// 改變格數。既有的子圖會保留，落在新範圍外的會被丟掉。
    void resize(int rows, int cols);

    /// 取得某一格的 Plot（第一次呼叫時才建立）。超出範圍回傳 nullptr。
    Plot* at(int row, int col);
    /// 讓某一格往右／往下吃掉幾格。回傳那一格的 Plot。
    ///
    /// 被吃掉的格子如果已經有圖，會被移除 —— 兩張圖疊在同一塊畫布上
    /// 只會互相覆蓋，不如直接拿掉並讓呼叫端發現。
    Plot* span(int row, int col, int rowSpan, int colSpan);
    /// 目前已建立的子圖，依建立順序。
    QVector<Plot*> plots() const;
    /// 某一格的 Plot，沒有就回傳 nullptr（不會順手建立）。
    Plot* existing(int row, int col) const;

    /// 列高與欄寬的權重。空的代表等分；長度不足的部分當成 1。
    QVector<double> rowWeights;
    QVector<double> columnWeights;

    /// 子圖之間的間距（像素）。
    double spacingX = 12;
    double spacingY = 12;
    /// 整張畫布四周的留白（像素）。
    double paddingLeft = 4, paddingRight = 4, paddingTop = 4, paddingBottom = 4;

    /// 整張畫布的總標題。
    Label title;
    /// 畫布底色。
    QColor background{255, 255, 255};

    /// 讓各子圖的繪圖區邊界對齊。
    ///
    /// 不對齊的話，刻度標籤長短不同的兩張子圖，繪圖區左緣會差上幾十個像素；
    /// 並排比較時那個錯位比資料本身的差異還顯眼。
    /// 做法是先量一遍每張子圖要留多少邊，取同一欄（列）的最大值再補回去。
    bool alignAxes = true;

    /// 共用座標範圍：render 時把這一組子圖的範圍統一成聯集。
    ///
    /// 要的是「同一把尺」。各自 autoScale 的話，兩張子圖的 Y 軸刻度不同，
    /// 一樣高的長條代表不一樣的數值 —— 這是並排比較最常見的誤導。
    void shareX(const QVector<Plot*>& group);
    void shareY(const QVector<Plot*>& group);
    /// 全部子圖共用。
    void shareXAll();
    void shareYAll();
    /// 清掉所有共用設定。
    void clearSharing();

    /// 走過每一張子圖的每一處字型設定。
    void visitFonts(const FontVisitor& visit);

    void render(QPainter& painter, const QRectF& target);
    QImage renderToImage(int width, int height, double scale = 1.0);
    bool savePng(const QString& path, int width = 800, int height = 600, double scale = 1.0);
    bool saveSvg(const QString& path, int width = 800, int height = 600);
    /// 向量 PDF。width/height 的單位是點（1/72 吋）。
    bool savePdf(const QString& path, int width = 800, int height = 600);

    /// 某一格在 target 裡佔的像素矩形（還沒扣掉軸與標題）。
    QRectF cellRect(const QRectF& target, int row, int col, int rowSpan = 1,
                    int colSpan = 1) const;

private:
    struct Cell {
        int row = 0, col = 0, rowSpan = 1, colSpan = 1;
        std::unique_ptr<Plot> plot;
    };
    int m_rows = 1;
    int m_cols = 1;
    // std::vector 而不是 QVector：Cell 持有 unique_ptr，是「只能搬不能複製」的型別，
    // 而 QList 的實作在幾個路徑上要求可複製。
    std::vector<Cell> m_cells;
    QVector<QVector<Plot*>> m_shareX;
    QVector<QVector<Plot*>> m_shareY;

    Cell* findCell(int row, int col) const;
    void applySharing();
    double titleHeight() const;
};

/// 顯示 Multiplot 的 widget。
///
/// 刻意不做互動：平移縮放一旦跨子圖就牽涉到「拖的是哪一張、共用軸要不要一起動」，
/// 那是另一個題目。需要互動請用 PlotWidget 配 linkAxes()。
class MultiplotWidget : public QWidget {
    Q_OBJECT
public:
    explicit MultiplotWidget(QWidget* parent = nullptr);
    ~MultiplotWidget() override;

    Multiplot& multiplot() { return *m_multiplot; }
    const Multiplot& multiplot() const { return *m_multiplot; }

    /// 丟掉目前的內容換一張全新的空白 Multiplot。
    /// 和 PlotWidget::resetPlot() 同樣的用意：換圖時要先回到空白狀態。
    void resetMultiplot(int rows = 1, int cols = 1);

    void refresh() { update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    std::unique_ptr<Multiplot> m_multiplot;
};

// ───────────────────────────── 嵌入小圖 ─────────────────────────────

/// 嵌在另一張圖的繪圖區裡的小圖（inset）。
///
/// 典型用途是放大局部：主圖看整體，角落那張看細節。
/// 內層就是一張完整的 Plot，什麼都能放。
class InsetPlot : public IPlottable {
public:
    InsetPlot();
    ~InsetPlot() override;

    /// 內層的圖。InsetPlot 擁有它。
    Plot& inner() { return *m_inner; }
    const Plot& inner() const { return *m_inner; }

    /// 位置與大小，佔繪圖區的比例（0–1，左上角為原點，和像素一樣 y 向下）。
    QRectF rectFraction{0.56, 0.06, 0.40, 0.36};

    /// 改用資料座標定位。
    ///
    /// 比例定位的小圖永遠待在繪圖區的同一個角落；資料座標定位的會跟著縮放跑。
    /// 想指著某一段資料就用資料座標，想固定在角落就用比例。
    bool useDataCoordinates = false;
    CoordinateRect dataRect{0, 1, 0, 1};

    /// 小圖的外框與陰影。
    LineStyle border{QColor(80, 80, 80), 1.2};
    /// 外框和主圖之間的留白（像素），讓小圖不要貼著格線。
    double marginPx = 0;

    /// 要不要在主圖上框出小圖放大的那一段，並連兩條線過去。
    ///
    /// 不畫的話，讀者得自己比對座標才知道小圖看的是哪裡 —— 那正是最容易看錯的地方。
    bool showSourceRegion = true;
    /// 放大的那一段（主圖的資料座標）。無效時用內層圖目前的軸範圍。
    CoordinateRect sourceRegion{0, 0, 0, 0};
    LineStyle sourceLine{QColor(120, 120, 120), 1.0, LinePattern::Dashed};
    FillStyle sourceFill{QColor(0, 0, 0, 18)};
    LineStyle connectorLine{QColor(120, 120, 120), 1.0, LinePattern::Dashed};

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    void visitFonts(const FontVisitor& visit) override;

private:
    std::unique_ptr<Plot> m_inner;
};

/// 在 host 上加一張嵌入小圖，並把它的樣式對齊主圖。
InsetPlot* addInset(Plot& host);

}  // namespace qtplot

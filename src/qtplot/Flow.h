// Flow.h — 流量／關係類圖表：桑基圖、弦圖。
//
// 這兩種都在畫「誰流向誰、流了多少」。差別在版面：桑基圖把節點排成由左到右的
// 幾層，適合有方向、有階段的流程；弦圖把節點排成一個圓，適合兩兩之間互有往來、
// 沒有明顯先後的關係（貿易、遷徙、轉換矩陣）。
#pragma once

#include "Plottable.h"

namespace qtplot {

// ───────────────────────── 桑基圖 ─────────────────────────

/// 桑基圖的一個節點。
struct SankeyNode {
    QString label;
    /// 排在第幾層（由左到右，0 是最左邊）。層號由使用者指定，不由連線推導 ——
    /// 推導在有環或跨層連線時會得到很奇怪的結果。
    int layer = 0;
    QColor color;
};

/// 桑基圖的一條連線。
struct SankeyLink {
    int from = 0;  ///< nodes 的索引
    int to = 0;
    double value = 0;
    /// 無效色代表沿用來源節點的顏色。
    QColor color;
};

/// 桑基圖：節點排成由左到右的幾層，連線的粗細正比於流量。
///
/// 節點的高度取「流進來的總和」與「流出去的總和」兩者的較大值。用較大值而不是
/// 其中一個，是因為中間的節點常常不平衡（有損耗、有新增），取小的那邊會讓
/// 連線擠不下、互相重疊。
class Sankey : public IPlottable {
public:
    QVector<SankeyNode> nodes;
    QVector<SankeyLink> links;

    /// 整張圖畫在這個資料矩形裡。
    CoordinateRect extent{0, 1, 0, 1};
    /// 節點方塊的寬度，佔整張圖寬度的比例。
    double nodeWidthFraction = 0.035;
    /// 同一層節點之間的間距，佔整張圖高度的比例。
    double nodePaddingFraction = 0.03;

    double linkOpacity = 0.45;
    LineStyle nodeBorder{QColor(0, 0, 0, 0), 1.0};
    FontStyle labelFont;
    /// 節點文字離方塊多遠（像素）。
    double labelGapPx = 6;

    /// 共有幾層。
    int layerCount() const;
    /// 每個節點的「流量」（進出兩邊取較大值）。
    QVector<double> nodeValues() const;

    /// 版面計算的結果（資料座標）。
    ///
    /// 公開是為了能單獨驗證 —— 桑基圖最容易出錯的地方是「連線配置的順序」，
    /// 而順序錯了只會讓圖上多幾個交叉，看圖分不出是資料本來就該交叉、
    /// 還是程式排錯了。
    struct Layout {
        struct Box {
            double left = 0, right = 0, top = 0, bottom = 0;
            bool valid = false;
        };
        QVector<Box> boxes;            ///< 依節點索引
        QVector<double> sourceTop;     ///< 依連線索引：在來源端佔的上緣
        QVector<double> targetTop;     ///< 依連線索引：在目的端佔的上緣
        QVector<bool> drawn;           ///< 這條連線是否有效、會畫出來
        double scale = 0;              ///< 一單位流量畫多高（資料座標）
    };
    Layout computeLayout() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 弦圖 ─────────────────────────

/// 弦圖：節點排成一個圓，兩兩之間的往來畫成穿過圓心的緞帶。
///
/// 資料是一個方陣：matrix[i][j] 是 i 流向 j 的量。對稱的矩陣就是無向的關係，
/// 不對稱則是有向的 —— 兩個方向會畫成同一條緞帶的兩端，粗細各自不同。
class Chord : public IPlottable {
public:
    QStringList labels;
    /// n×n 的流量矩陣。非方陣或空的就不畫。
    QVector<QVector<double>> matrix;
    /// 每個節點的顏色。不足的部分由調色盤補。
    QVector<QColor> colors;

    Coordinates center{0, 0};
    double radius = 1.0;
    /// 外圈弧帶的厚度，佔半徑的比例。
    double arcThicknessFraction = 0.08;
    /// 相鄰兩個節點之間留的空隙（度）。
    double gapDegrees = 2.0;
    double startAngle = 90;
    bool clockwise = true;

    double ribbonOpacity = 0.6;
    LineStyle arcBorder{QColor(255, 255, 255), 1.0};
    LineStyle ribbonBorder{QColor(0, 0, 0, 0), 1.0};
    FontStyle labelFont;
    double labelGapPx = 8;

    /// 每個節點的總量（該列與該行的總和）。
    QVector<double> nodeTotals() const;
    /// 矩陣是不是合法的方陣。
    bool isValid() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 網路圖 ─────────────────────────

/// 網路圖的一個節點。
struct GraphNode {
    QString label;
    /// 節點的大小（相對值）。換算成直徑的方式和氣泡圖一樣：面積正比。
    double size = 1;
    QColor color;
    /// 版面算完之後的位置；layout 設成 Manual 時改成由使用者指定。
    Coordinates position{0, 0};
    /// 釘住不動（力導向版面會跳過它）。
    bool pinned = false;
};

/// 網路圖的一條邊。
struct GraphEdge {
    int from = 0;
    int to = 0;
    double weight = 1;
    QColor color;  ///< 無效色代表沿用 edgeLine
};

/// 網路圖（node-link diagram）：節點加連線，用版面演算法把關係攤開。
///
/// 要先知道它的限制：節點超過一兩百個之後，力導向版面會變成一團毛球，
/// 看不出任何結構。那時候該換成鄰接矩陣熱圖或階層式的圖。
class NetworkGraph : public IPlottable {
public:
    QVector<GraphNode> nodes;
    QVector<GraphEdge> edges;

    enum class Layout {
        ForceDirected,  ///< Fruchterman–Reingold 力導向
        Circular,       ///< 全部排在一個圓上（適合節點少、邊多）
        Linear,         ///< 全部排在一條水平線上（弧線圖）
        Manual          ///< 直接用 GraphNode::position
    };
    Layout layout = Layout::ForceDirected;

    /// 連線畫成什麼形狀。
    enum class EdgeShape {
        Straight,  ///< 直線
        Arc        ///< 半橢圓弧。配 Linear 版面就是弧線圖（arc diagram）
    };
    EdgeShape edgeShape = EdgeShape::Straight;
    /// 弧的高度，以「兩端距離」的比例表示。負值代表畫在線的下方。
    ///
    /// 弧線圖把節點排成一條線，關係畫成上方的弧。好處是節點有固定順序
    /// （可以照名稱、照大小排），力導向則每次位置都不一樣、不好對照；
    /// 代價是邊一多，弧會互相穿越。
    double arcHeightFraction = 0.5;

    /// 力導向的迭代次數。愈多愈收斂，但也愈慢。
    int iterations = 300;
    /// 理想邊長的係數，乘在 sqrt(面積 / 節點數) 上。
    ///
    /// 這個值控制「鬆緊」：Fruchterman–Reingold 的斥力是 k²/d、引力是 d²/k，
    /// 所以 k 調小會同時放大引力、縮小斥力，整張圖收得更緊。
    /// 星狀（一個中心連出很多葉子）這種稀疏的圖，用原始的 k 會讓葉子被斥力
    /// 推到框邊擠成一排 —— 那些葉子彼此沒有邊，只有斥力在作用。
    double edgeLengthFactor = 0.45;
    /// 往中心的向心力，避免外圍節點貼到框邊。0 代表不加。
    double gravity = 0.9;
    /// 力導向的亂數種子。
    ///
    /// 用自己的引擎而不是 DataGen 的全域引擎：版面必須只取決於資料與這個種子，
    /// 不能被「前面跑過什麼」影響，否則同一張圖每次畫出來都不一樣。
    unsigned int seed = 1;

    /// 整張圖畫在這個資料矩形裡。
    CoordinateRect extent{0, 1, 0, 1};

    /// 節點直徑的範圍（像素）。
    double nodeMinDiameterPx = 10;
    double nodeMaxDiameterPx = 44;
    LineStyle nodeBorder{QColor(255, 255, 255), 1.5};

    LineStyle edgeLine{QColor(120, 120, 120, 150), 1.0};
    /// 權重最大的那條邊畫多粗（像素）。
    double edgeMaxWidthPx = 3.5;

    FontStyle labelFont;
    bool showLabels = true;
    /// 節點直徑小於這個值就不標文字。
    double minLabelDiameterPx = 16;

    /// 算出每個節點的位置（資料座標）。
    ///
    /// 公開是為了能單獨驗證 —— 版面演算法只要有一點不確定性，
    /// 輸出的 PNG 就不可重現，視覺回歸會整個失效。
    QVector<Coordinates> computeLayout() const;
    /// 每個節點的直徑（像素）。
    double diameterFor(double size) const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};


// ───────────────────────── 沖積圖 ─────────────────────────

/// alluvialFromRecords() 的輸出：直接餵給 Plot::addSankey()。
struct AlluvialResult {
    QVector<SankeyNode> nodes;
    QVector<SankeyLink> links;
};

/// 由「一筆一筆的分類紀錄」建出沖積圖（多階段的桑基圖）。
///
/// records 的每一列是一個個體在各階段的分類，例如
/// {"北區", "方案A", "續約"}；stageNames 是各階段的名稱，只用來決定階段數。
///
/// 沖積圖和桑基圖的底層是同一個東西 —— 差別只在資料長相：
/// 桑基圖給的是「節點與流量」，沖積圖給的是「一筆一筆的個案」。
/// 手動把後者攤成前者很容易算錯，尤其是中間階段的節點要同時當入口和出口。
///
/// 分類在各階段內的排序是「第一次出現的順序」，不是字典序 ——
/// 字典序會把「高／中／低」排成「中高低」。
///
/// 長度和 stageNames 不同的紀錄會被跳過。
AlluvialResult alluvialFromRecords(const QVector<QStringList>& records,
                                   const QStringList& stageNames);

}  // namespace qtplot

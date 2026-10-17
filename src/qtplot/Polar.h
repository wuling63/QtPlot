// Polar.h — 極座標族圖元：極座標軸、圓餅、玫瑰圖、雷達圖、環形量表。
//
// 這些圖看起來不是直角座標，但底下仍然是：每個點都先用 fromPolar() 換成
// (x, y) 再交給既有的 Transform。好處是縮放、平移、多軸、輸出 PNG/SVG 全部
// 不用特別處理；代價是兩軸比例不同時圓會變成橢圓，所以這類圖通常要搭配
// plot.equalAxisScale = true。
//
// 角度約定（整個檔案一致）：
//   * 角度單位是「度」。
//   * 0° 指向 +X（三點鐘方向），逆時針為正 —— 數學慣例。
//   * 圓餅、雷達、量表的 startAngle 預設 90（十二點鐘），clockwise 預設 true，
//     因為這是一般人讀這類圖的習慣方向。
#pragma once

#include "Plottable.h"

namespace qtplot {

/// 一塊扇形／一條雷達軸／一個量表共用的資料。
struct PieSlice {
    double value = 1;       ///< 佔比或半徑的來源，必須 >= 0
    QString label;          ///< 畫在扇形上的文字
    QColor fillColor;       ///< 無效色代表由 Plot 的調色盤自動指定
    double explode = 0;     ///< 沿著自己的角度往外推多遠（資料單位）
    QString legendText;     ///< 空字串代表不進圖例
    /// 這一塊的外半徑佔 Pie::radius 的比例。<= 0 代表沿用整體的半徑。
    ///
    /// 讓某幾塊特別突出用的。要注意這樣一來「角度」和「半徑」各自代表一個
    /// 維度，讀者很容易把面積當成唯一的量 —— 當作版面效果就好，
    /// 需要精確比較時不要用。
    double radiusFraction = 0;
    /// 完整的填色（花紋、漸層）。有設就取代 fillColor。
    ///
    /// 黑白列印時顏色分不出來，用花紋區分各塊。
    FillStyle fill;
};

/// 一條放射線的個別設定。只在需要「每條不一樣」的時候才用。
struct PolarSpoke {
    /// 這條線的角度（度），用的是這根軸自己的角度系統 ——
    /// 也就是可以直接傳給 PolarAxis::coordinates() 的那個值。
    double angle = 0;
    /// 末端的文字。留空則印角度。
    QString label;
    /// 長度，佔半徑的比例。1 畫到最外圈，0.5 只畫一半。
    double lengthFraction = 1.0;
    /// 線的樣式。顏色透明代表沿用 PolarAxis::spokeLine。
    LineStyle line{QColor(0, 0, 0, 0), 1.0};
    /// 標籤顏色。無效色代表沿用 PolarAxis::labelFont。
    QColor labelColor;
};

/// 極座標的格線：放射線（spoke）與同心圓（ring）。
///
/// 它本身只畫背景格線，資料要另外疊上去 —— 用 coordinates() 把 (r, θ) 換成
/// (x, y)，再交給一般的 Scatter、Markers、Arrow。
///
/// 角度系統：coordinates() 收的角度和放射線標籤印出來的是同一個值。
/// startAngle 決定「0 度」指向哪裡，clockwise 決定角度往哪邊增加，
/// 所以轉動整張圖只要改 startAngle，資料和標籤會一起轉。
class PolarAxis : public IPlottable {
public:
    Coordinates center{0, 0};
    double radius = 1.0;

    /// 放射線數量。spokeLabels 或 spokes 有給就以它們的數量為準。
    int spokeCount = 8;
    /// 每條放射線末端的文字。留空則印角度。
    QStringList spokeLabels;
    /// 逐條設定放射線（角度、長度、顏色、標籤）。
    /// 非空時完全取代 spokeCount 與 spokeLabels。
    QVector<PolarSpoke> spokes;
    /// 所有放射線的預設長度，佔半徑的比例。逐條設定時由 PolarSpoke 覆蓋。
    double spokeLengthFraction = 1.0;
    /// 圓盤的底色（畫在格線之下）。
    FillStyle background;
    /// 同心圓的半徑。留空則在 0..radius 之間自動分 ringCount 層。
    QVector<double> ringRadii;
    int ringCount = 4;
    /// 同心圓畫成多邊形（頂點落在每條放射線上），雷達圖常用這種外觀。
    bool polygonRings = false;
    /// 逐圈指定線條樣式（依 ringRadii 的順序）。不足的用 ringLine。
    QVector<LineStyle> ringStyles;
    /// 同心圓旁邊要不要標半徑數值。
    bool showRingLabels = true;
    /// 半徑標籤畫在哪個角度上。
    ///
    /// 用的是這根軸自己的角度系統（和 coordinates() 同一套），所以預設的 0
    /// 就是「0 度那根放射線的方向」。預設設定（startAngle = 90、順時針）下
    /// 那個方向是正上方，和多數工具的習慣一致。
    double ringLabelAngle = 0;
    /// 半徑標籤的文字（依 ringRadii 的順序）。留空則印半徑本身的數值。
    ///
    /// 用得到的場合：半徑是正規化過的（0–1），但要標的是原始單位
    /// （銷售額、人數…）。這時候印半徑等於印 0.2、0.4，沒有意義。
    QStringList ringLabels;
    /// 半徑標籤的字型。和 labelFont 分開 —— 放射線的標籤常常要交給疊在上面的
    /// 圖元去畫（於是把 labelFont 設成透明），半徑標籤卻還是要留著。
    FontStyle ringLabelFont;

    double startAngle = 90;   ///< 第一條放射線的角度
    bool clockwise = true;

    LineStyle spokeLine{QColor(0, 0, 0, 90), 1.0};
    LineStyle ringLine{QColor(0, 0, 0, 60), 1.0};
    FontStyle labelFont;
    /// 放射線標籤離圓周多遠（像素）。
    double labelPaddingPx = 8;

    /// 把極座標 (radius, angleDeg) 換成資料座標。
    ///
    /// 已經套用了 center、startAngle 與 clockwise，所以傳進去的角度就是
    /// 放射線標籤上印的那個角度。要把資料疊到極座標圖上請用這個，
    /// 不要直接用 fromPolar() —— 那個不知道這根軸轉過多少。
    Coordinates coordinates(double radius, double angleDeg) const;

    /// 實際會畫出來的放射線清單（spokes 為空時依 spokeCount 自動產生）。
    QVector<PolarSpoke> resolvedSpokes() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(ringLabelFont);
        visit(labelFont);
    }
};

/// 圓餅圖／甜甜圈圖。扇形的角度正比於 value。
class Pie : public IPlottable {
public:
    QVector<PieSlice> slices;
    Coordinates center{0, 0};
    double radius = 1.0;
    /// 0 畫實心圓餅，0.5 畫內徑為一半的甜甜圈。
    double donutFraction = 0.0;
    double startAngle = 90;
    bool clockwise = true;
    LineStyle sliceBorder{QColor(255, 255, 255), 1.5};

    enum class LabelMode { None, Value, Percent, Label, LabelAndPercent };
    LabelMode labelMode = LabelMode::Label;
    FontStyle labelFont;
    /// 標籤的位置，以半徑為單位。0.6 在扇形中段，1.15 在圓外面。
    double labelDistance = 0.6;
    /// 甜甜圈中央的文字（donutFraction > 0 才看得到）。
    Label centerLabel;

    // ── 立體（擠出）──
    /// 垂直方向壓扁的比例，模擬「從斜上方看」。1 是正圓，0.5 壓成一半。
    double tiltFraction = 1.0;
    /// 往下擠出的厚度（像素）。> 0 時會畫出側壁。
    ///
    /// 立體圓餅是有名的壞圖表：壓扁之後，同樣角度的扇形在前面看起來比後面大，
    /// 讀者沒辦法用面積比較數值。要精確比較請用平的圓餅或長條圖；
    /// 這個選項是給簡報版型用的。
    double depthPx = 0;

    // ── 引線標籤 ──
    /// 把標籤拉到圓外，用引線連回扇形，並自動把重疊的標籤上下推開。
    ///
    /// 扇形很多、或有幾塊特別薄的時候一定要開 —— 貼在扇形上的標籤會疊成一團。
    bool useLeaderLines = false;
    /// 引線水平段的長度，佔半徑的比例。
    double leaderLengthFraction = 0.35;
    LineStyle leaderLine{QColor(0, 0, 0, 140), 1.0};
    /// 兩個標籤之間至少要留幾個像素。排不下時會往上下擠。
    double leaderMinSpacingPx = 2;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    double total() const;

    /// 每一塊扇形依 labelMode 應該顯示的文字。空字串代表不畫。
    QStringList sliceTexts() const;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(centerLabel.font);
    }
};

/// 玫瑰圖（Nightingale / Coxcomb）：每塊扇形角度相同，改用半徑表示大小。
class Coxcomb : public IPlottable {
public:
    QVector<PieSlice> slices;
    Coordinates center{0, 0};
    double radius = 1.0;
    /// true 時半徑取 sqrt(value)，讓「面積」正比於數值 —— 這才是正確的讀法，
    /// 半徑直接正比於數值會把大的值誇大成平方倍。
    bool areaProportional = true;
    double startAngle = 90;
    bool clockwise = true;
    LineStyle sliceBorder{QColor(255, 255, 255), 1.5};
    FontStyle labelFont;
    bool showLabels = true;

    /// 每一格佔的角度中，實際畫出來的比例。
    /// 1 表示相鄰兩格緊貼，0.6 則兩側各留 20% 當空隙。
    double sectorWidthFraction = 1.0;

    /// 格子的名稱沿半徑方向轉（像放射狀的刻度），而不是一律水平。左半邊的會翻過來，不會倒著寫。
    bool radialLabels = false;
    /// 名稱放在每一格自己的尖端外面，而不是整張圖的外圈。
    bool labelsFollowValue = false;
    /// 名稱離圓心的距離（radius 的比例）。labelsFollowValue 時不用。
    double labelDistance = 1.04;
    /// 中間挖一個洞，佔半徑的比例。0 是實心。
    double donutFraction = 0.0;

    /// 堆疊模式：每一格由內往外疊的數值。
    ///
    /// 非空時取代 slices 的 value（slices 仍然決定有幾格與每一格的標籤）。
    /// stacks[i] 是第 i 格的各段。段數可以不一樣，短的那幾格就少疊幾層。
    ///
    /// 注意堆疊玫瑰圖在 areaProportional 開啟時會自相矛盾：面積正比要對
    /// 「累計值」開根號，各段的厚度就不再正比於自己的值了。要能逐段比較大小，
    /// 堆疊時請把 areaProportional 關掉。
    QVector<QVector<double>> stacks;
    /// 各段的顏色（依段的索引）。不足的部分由調色盤補。
    QVector<QColor> stackColors;
    /// 各段的名稱，給圖例用。
    QStringList stackLabels;

    /// 甜甜圈中央的文字（donutFraction > 0 才看得到）。
    Label centerLabel;

    /// 在每一格的外緣標出它的總量。
    ///
    /// 和 showLabels（格子的名稱）分開：風花圖這類圖兩種都要，
    /// 名稱排在最外圈、數值貼著扇形的尖端。
    bool showValueLabels = false;
    FontStyle valueLabelFont;
    /// 數值文字離扇形外緣多遠（像素）。
    double valueLabelGapPx = 4;
    /// 接在數值後面的單位（例如 "%"）。
    QString valueLabelSuffix;

    /// 每一格的總量（堆疊模式是各段的和，否則就是 value）。
    QVector<double> sliceTotals() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(centerLabel.font);
        visit(valueLabelFont);
    }
};

/// 雷達圖的一組資料。
struct RadarSeries {
    QVector<double> values;
    QColor lineColor;
    QColor fillColor;   ///< 無效色代表用 lineColor 加透明度
    QString legendText;
};

/// 雷達圖／蜘蛛網圖。每個維度一條放射軸。
class Radar : public IPlottable {
public:
    QVector<RadarSeries> series;
    QStringList axisLabels;
    Coordinates center{0, 0};
    double radius = 1.0;
    /// 各維度的最大值（用來正規化）。<= 0 代表取全部資料的最大值。
    double maxValue = 0;
    /// 每個維度各自正規化（維度的單位不同時要開）。
    bool independentAxes = false;
    bool filled = true;
    double lineWidth = 2.0;
    MarkerStyle marker;
    FontStyle labelFont;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    int dimensionCount() const;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

/// 環形量表。每個值畫成一段圓弧。
class RadialGauge : public IPlottable {
public:
    /// 每個環一個值。
    QVector<double> values;
    QVector<QColor> colors;
    QStringList labels;

    enum class Mode {
        Sequential,  ///< 每個值一個同心環，都從 startAngle 開始
        Stacked,     ///< 全部畫在同一個環上，依序接續（總和不應超過 maximum）
        Cumulative   ///< 每個值一個同心環，但從上一環結束的角度接著畫（看累積進度）
    };
    Mode mode = Mode::Sequential;

    Coordinates center{0, 0};
    double innerRadius = 0.35;
    double outerRadius = 1.0;
    /// 相鄰兩環之間留的空隙，佔單環厚度的比例。
    double gaugeSpacing = 0.15;

    /// values 的滿刻度。
    double maximum = 100;
    double startAngle = 90;
    bool clockwise = true;
    /// 滿刻度對應幾度。360 是整圈；270 是常見的儀表（缺口朝下時 startAngle 設 225）。
    double sweepAngle = 360;
    /// 第一個值放在最內環（預設在最外環）。
    bool reverseOrder = false;
    /// 圓弧端點畫成圓頭。
    bool roundCaps = true;
    /// 畫出沒走到的那段（淡色底環）。
    bool showBackground = true;
    QColor backgroundColor{0, 0, 0, 28};

    bool showLabels = true;
    FontStyle labelFont;
    /// 數值文字（印在圓弧末端）。留空代表不印。
    enum class ValueMode { None, Value, Percent };
    ValueMode valueMode = ValueMode::Value;
    /// 數值文字落在弧上的哪裡：1 是弧的末端、0 是起點、0.5 是正中間。
    double valueLabelPosition = 1.0;
    /// 數值文字的大小佔環厚度的比例（0 代表用 valueFont.size）。
    /// 環的粗細會隨視窗大小變，用比例的話字永遠塞得進環裡。
    double valueFontFraction = 0;
    /// 數值文字的字型。預設白色（壓在色弧上）。
    FontStyle valueFont = [] { FontStyle f; f.color = QColor(Qt::white); return f; }();

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(valueFont);
    }
};

}  // namespace qtplot

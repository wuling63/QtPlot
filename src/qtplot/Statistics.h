// Statistics.h — 統計圖元（箱型圖、棒棒糖圖、分布圖）與它們用到的統計計算。
//
// 統計量的定義會直接影響畫出來的圖，所以每個都寫清楚用哪一種：
// 換一種四分位數定義，箱子的上下緣就會移動，別人拿去跟 R 或 numpy 比對時
// 對不起來會很難查。
#pragma once

#include "Plottable.h"

namespace qtplot {

// ───────────────────────── 統計計算 ─────────────────────────

/// 一組樣本的五數概括與離群值。
struct BoxStats {
    double min = 0;      ///< 下鬚端（不是資料最小值，是 q1 - factor * IQR 之內的最小值）
    double q1 = 0;
    double median = 0;
    double q3 = 0;
    double max = 0;      ///< 上鬚端
    double mean = 0;
    double stdDev = 0;   ///< 樣本標準差（分母 n-1）
    double stdErr = 0;   ///< 標準誤 = stdDev / sqrt(n)
    int count = 0;
    QVector<double> outliers;  ///< 落在鬚之外的原始值

    double iqr() const { return q3 - q1; }
    bool isValid() const { return count > 0; }

    /// 從原始資料算出來。
    ///
    /// 四分位數用線性內插法（R 的 type 7、numpy.percentile 的預設），
    /// 所以和那兩者的結果一致。鬚延伸到 1.5 倍 IQR 之內的最遠資料點，
    /// 超出的列為離群值 —— 這是 Tukey 的原始定義。
    /// whiskerIqrFactor 設 0 代表鬚拉到資料的極值，不判離群值。
    static BoxStats fromValues(const QVector<double>& values, double whiskerIqrFactor = 1.5);
};

/// 線性迴歸的結果。
struct LinearFit {
    double slope = 0;
    double intercept = 0;
    double rSquared = 0;
    int count = 0;
    double predict(double x) const { return slope * x + intercept; }
    bool isValid() const { return count >= 2; }
};

/// 最小平方法線性迴歸。非有限的點會被跳過。
LinearFit linearFit(const QVector<double>& xs, const QVector<double>& ys);

/// 線性內插的樣本分位數（R 的 type 7 / numpy.percentile 的預設）。
///
/// sortedValues 必須已經排好序 —— 這裡不排，因為呼叫端通常要連取好幾個分位數，
/// 每次都排一遍很浪費。空陣列回傳 NaN。
double sampleQuantile(const QVector<double>& sortedValues, double fraction);

/// Silverman 經驗法則的頻寬：1.06 * min(σ, IQR/1.34) * n^(-1/5)。
double silvermanBandwidth(const QVector<double>& values);

/// 核密度估計用的核函數。
///
/// 換核函數對曲線形狀的影響遠小於換頻寬：高斯最平滑；Epanechnikov 在均方誤差意義下最佳，
/// 而且有界（遠處的點完全不貢獻）；均勻核會出現階梯狀的轉折。
enum class KernelType { Gaussian, Epanechnikov, Uniform, Triangular, Cosine };

/// 核密度估計。bandwidth <= 0 時用 silvermanBandwidth()。
/// 回傳值與 evalPoints 等長，是機率密度（對 x 積分為 1）。
QVector<double> kernelDensity(const QVector<double>& values, const QVector<double>& evalPoints,
                              double bandwidth = 0, KernelType kernel = KernelType::Gaussian);

/// 核函數在標準化距離 u 上的值（對 u 積分為 1）。
double kernelValue(KernelType kernel, double u);

// ───────────────────────── 箱型圖 ─────────────────────────

/// 一個箱子。
struct Box {
    double position = 0;   ///< 沿著類別軸的位置
    double size = 0.6;     ///< 箱寬（類別軸的資料單位）
    BoxStats stats;
    QColor fillColor;
    QString label;
    QString legendText;
};

/// 箱型圖（箱鬚圖）。
class BoxPlot : public IPlottable {
public:
    QVector<Box> boxes;
    bool horizontal = false;

    LineStyle borderLine{QColor(0, 0, 0), 1.0};
    LineStyle whiskerLine{QColor(0, 0, 0), 1.0};
    LineStyle medianLine{QColor(0, 0, 0), 2.0};
    /// 鬚端帽的寬度，佔箱寬的比例。
    double capFraction = 0.5;
    /// 離群值的標記。shape = None 代表不畫。
    MarkerStyle outlierMarker{MarkerShape::OpenCircle, 6, QColor(0, 0, 0)};
    /// 平均值的標記。預設不畫 —— 箱型圖的橫線是中位數，再畫一條平均很容易被誤讀。
    MarkerStyle meanMarker;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 由多組原始資料建箱子（位置 0, 1, 2, ...）。
    static QVector<Box> fromGroups(const QVector<QVector<double>>& groups,
                                   const QStringList& labels = {},
                                   double whiskerIqrFactor = 1.5);
};

// ───────────────────────── 階層式分群 ─────────────────────────

/// 兩群合併成一群的紀錄。
struct ClusterMerge {
    /// 合併的兩邊。< n 是葉節點的索引，>= n 代表第 (index − n) 次合併的結果。
    /// 這和 SciPy 的 linkage 矩陣是同一套編號。
    int left = 0;
    int right = 0;
    /// 合併時兩群的距離。樹狀圖的「高度」就是它。
    double distance = 0;
    /// 合併後的成員數。
    int size = 0;
};

/// 群與群之間的距離怎麼定義。
enum class Linkage {
    Single,    ///< 最近的一對成員（容易產生鏈狀的長串群）
    Complete,  ///< 最遠的一對成員（傾向產生緊湊、大小相近的群）
    Average    ///< 所有成員對的平均（UPGMA，兩者之間）
};

/// 凝聚式階層分群。distances 是 n×n 的對稱距離矩陣。
///
/// 回傳 n−1 次合併，順序就是合併發生的順序（距離遞增）。
/// 非方陣或 n < 2 時回傳空。
///
/// 距離更新用 Lance–Williams 公式，而不是每次重新掃全部成員 ——
/// 後者是 O(n³) 的常數大很多，而且浮點累加的順序會隨成員順序改變，
/// 結果就不穩定了。
QVector<ClusterMerge> hierarchicalCluster(const QVector<QVector<double>>& distances,
                                          Linkage linkage = Linkage::Average);

/// 從合併結果推出葉節點該排的順序。
///
/// 樹狀圖的葉子必須照這個順序排，枝幹才不會互相交叉 ——
/// 照原始索引排的話，同一群的葉子會散在兩端，看起來像分群失敗。
QVector<int> dendrogramLeafOrder(const QVector<ClusterMerge>& merges, int leafCount);

/// 樹狀聚類圖。
class Dendrogram : public IPlottable {
public:
    /// hierarchicalCluster() 的輸出。
    QVector<ClusterMerge> merges;
    /// 葉節點的數量與名稱。
    int leafCount = 0;
    QStringList leafLabels;

    /// 葉子的排列順序。留空則自動用 dendrogramLeafOrder()。
    QVector<int> leafOrder;

    /// true 時樹由左往右長（葉子在左邊）。
    bool horizontal = false;

    LineStyle line{QColor(60, 60, 60), 1.4};
    FontStyle labelFont;
    /// 葉子標籤離軸多遠（像素）。
    double labelGapPx = 5;

    /// 在這個距離把樹切開，切出來的每一群用不同顏色。<= 0 代表不切。
    double colorThreshold = 0;
    Palette palette{Palette::Category10};

    /// 每個葉子在類別軸上的位置（依 leafOrder 推出來）。
    QVector<double> leafPositions() const;
    /// 最大的合併距離。
    double maxDistance() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 蜂群圖 ─────────────────────────

/// 蜂群圖的一組資料。
struct SwarmGroup {
    QString label;
    QVector<double> values;
    QColor fillColor;
    QString legendText;
};

/// 蜂群圖：一維的散點，點互相讓開不重疊。
///
/// 和箱型圖、小提琴圖的分工：那兩種把分布摘要成幾個數字或一條曲線，
/// 蜂群圖把每一筆原始資料都畫出來。樣本數少（幾十到幾百）的時候，
/// 摘要會藏住重要的細節 —— 兩個模態、離群值、資料其實只有五個相異值，
/// 箱型圖都看不出來。樣本數再多就不適用了，點會排到比箱子還寬。
class Beeswarm : public IPlottable {
public:
    QVector<SwarmGroup> groups;
    /// true 時類別在 Y 軸、數值在 X 軸。
    bool horizontal = false;

    MarkerStyle marker{MarkerShape::FilledCircle, 7, QColor(60, 120, 200)};
    /// 點與點之間額外留的間隙（像素）。
    double spacingPx = 1.0;
    /// 離類別中心最遠能擺多遠，以「一格類別」為單位。
    ///
    /// 排不下的點會被夾在這個範圍內，於是重疊 —— 這是刻意的：
    /// 讓它無限往外長的話，相鄰兩組會疊在一起，那更糟。
    double maxSpreadFraction = 0.45;

    /// 版面算出來的一個點。
    struct SwarmPoint {
        int group = 0;
        double offset = 0;  ///< 相對類別中心的位移（類別軸的資料單位）
        double value = 0;
    };

    /// 算出每個點該擺在哪。
    ///
    /// valueRadius / categoryRadius 是「一個標記的半徑」換算到兩個軸的資料單位。
    /// 由 render 從 Transform 算出來再傳進來 —— 版面必須知道標記在畫面上多大，
    /// 否則不同縮放下會得到不同的疏密。公開是為了能單獨驗證。
    QVector<SwarmPoint> layout(double valueRadius, double categoryRadius) const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
};

// ───────────────────────── 棒棒糖圖 ─────────────────────────

/// 一根棒棒糖。
struct LollipopItem {
    double position = 0;
    double value = 0;
    QColor color;
    QString label;
};

/// 棒棒糖圖：細桿加一個圓頭。
/// 類別多但數值差異小時比長條圖好讀 —— 長條的大片色塊會蓋掉差異。
class Lollipop : public IPlottable {
public:
    QVector<LollipopItem> items;
    bool horizontal = false;
    /// 桿子的起點。
    double baseline = 0;
    LineStyle stem{QColor(120, 120, 120), 1.5};
    MarkerStyle marker{MarkerShape::FilledCircle, 11, QColor(31, 119, 180)};
    bool showValueLabels = false;
    FontStyle labelFont;
    /// 自動把 baseline 含進範圍。
    bool includeBaseline = true;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    static QVector<LollipopItem> fromValues(const QVector<double>& values, QColor color);

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 分布圖 ─────────────────────────

/// 一組樣本在一個位置上的完整呈現：原始點、箱、誤差線、密度曲線可任意組合。
///
/// 對應 ScottPlot 的 Population。只開 distribution 並開 mirrored 就是小提琴圖。
class Population : public IPlottable {
public:
    struct Group {
        QVector<double> values;
        double position = 0;
        QColor color;
        QString label;
        QString legendText;
    };
    QVector<Group> groups;
    /// 一組佔的寬度（類別軸的資料單位）。
    double groupWidth = 0.7;

    // ── 要畫哪些成分 ──
    bool showPoints = true;        ///< 個別資料點
    bool showBox = true;           ///< 箱型（四分位數）
    bool showMeanBar = false;      ///< 平均值的長條
    bool showErrorBar = true;      ///< 平均 ± 誤差
    bool showDistribution = false; ///< 核密度曲線
    /// 密度曲線左右對稱 —— 配上 showDistribution 就是小提琴圖。
    bool mirrored = true;

    enum class ErrorKind { StdDev, StdErr, Iqr };
    ErrorKind errorKind = ErrorKind::StdDev;

    /// 箱子的上下緣代表什麼。Quartiles 是一般的箱型圖；其他幾種畫的是平均值與它的離散程度。
    enum class BoxMode { Quartiles, MeanStdDev, MeanStdErr, Whiskers };  ///< Whiskers：鬚的兩端（1.5 IQR 內的極值）
    BoxMode boxMode = BoxMode::Quartiles;

    /// 各成分往旁邊挪多少（佔 groupWidth 的比例，正值往右）。
    ///
    /// 「雨雲圖」就是點往左挪、箱與半邊密度往右挪：同一組資料的三種畫法並排，
    /// 互不遮擋。
    double pointsOffset = 0;
    double boxOffset = 0;
    double distributionOffset = 0;

    /// 資料點的水平抖動量，佔 groupWidth 的比例。0 代表全部畫在同一條線上。
    double jitter = 0.45;
    MarkerStyle pointMarker{MarkerShape::FilledCircle, 5, QColor(0, 0, 0, 110)};
    LineStyle boxBorder{QColor(0, 0, 0), 1.0};
    LineStyle errorLine{QColor(0, 0, 0), 1.5};
    LineStyle distributionLine{QColor(0, 0, 0, 150), 1.0};
    /// 密度曲線的取樣點數。
    int distributionSamples = 128;
    /// 密度曲線往資料範圍外延伸多少（佔資料全距的比例）。
    /// axisLimits() 會把它算進去，否則小提琴的頭尾會被裁掉。
    double distributionPadFraction = 0.15;
    /// 箱與密度曲線的填色透明度。
    int fillAlpha = 90;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
};


// ───────────────────────── 山脊圖 ─────────────────────────

/// 山脊圖的一組資料。
struct RidgeGroup {
    QString label;
    QVector<double> values;
    QColor color;
};

/// 山脊圖（joyplot）：好幾條密度曲線上下錯開、彼此略微重疊。
///
/// 和並排的小提琴圖比，山脊圖把所有分布放在「同一根 X 軸」上，
/// 所以峰值左右移動一眼就看得出來 —— 那正是看一組分布隨時間／類別變化時要看的。
/// 代價是重疊會遮住後面那條的尾巴，組數太多（超過十幾組）就不適用了。
class Ridgeline : public IPlottable {
public:
    QVector<RidgeGroup> groups;

    /// 相鄰兩條重疊多少。0 是完全不重疊，1 是一條的高度等於兩格。
    ///
    /// 重疊是這種圖的重點：不重疊就只是一疊很扁的密度曲線，
    /// 重疊太多則後面的完全被蓋住。0.6–1.0 之間通常最好看。
    double overlap = 0.8;

    /// 核密度的頻寬。<= 0 時用 Silverman 經驗法則。
    double bandwidth = 0;
    /// 每條曲線的取樣點數。
    int samples = 180;
    /// 曲線往資料範圍外延伸多少（佔全距的比例）。
    double padFraction = 0.08;

    /// 第一組畫在最上面（表格的習慣）。
    bool topToBottom = true;

    LineStyle line{QColor(255, 255, 255), 1.2};
    /// 填色的透明度。
    int fillAlpha = 215;
    Palette palette{Palette::Category10};
    /// 每一條底下要不要畫一條基線。
    bool showBaseline = true;
    LineStyle baseline{QColor(0, 0, 0, 60), 1.0};

    /// 每一組的基線位置（類別軸的資料座標）。
    QVector<double> groupPositions() const;
    /// 組名，依 groupPositions 的順序（給 setCategoryTicks 用）。
    QStringList orderedLabels() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
};

}  // namespace qtplot

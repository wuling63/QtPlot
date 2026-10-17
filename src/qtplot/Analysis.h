// Analysis.h — 統計推論與診斷圖：常態分位數、Q-Q、ROC/PR、存活分析、管制圖、森林圖。
//
// 這個檔案裡大部分是「計算」而不是「畫」。會分出來是因為這些圖的價值全在算得對：
// 一條 ROC 曲線畫得再漂亮，遇到同分的樣本沒處理好就會隨著輸入順序變形；
// 管制圖的 σ 用錯一種估法，該被抓出來的漂移就會自己把管制界限撐寬到抓不到。
// 畫的部分反而多半是現成圖元（Scatter 的階梯線、FillY、AxisLine）組起來就好。
#pragma once

#include "Statistics.h"

namespace qtplot {

// ───────────────────────── 常態分布 ─────────────────────────

/// 標準常態的機率密度。
double normalPdf(double x, double mean = 0, double sd = 1);

/// 常態分布的累積機率 Φ((x−mean)/sd)。
double normalCdf(double x, double mean = 0, double sd = 1);

/// 常態分布的分位數（Φ 的反函數）。p 要在 (0, 1) 之內，否則回傳 ±infinity。
///
/// 用 Wichura 的 AS241（PPND16），相對誤差約 1e-16 —— 比常見的
/// Beasley–Springer–Moro（約 1e-9）準得多。差別在尾端才看得出來：
/// 畫 Q-Q 圖時兩端那幾個點正是最受注意的地方，用低精度的版本會歪掉。
double normalQuantile(double p, double mean = 0, double sd = 1);

// ───────────────────────── Q-Q 圖 ─────────────────────────

/// 第 i 個順序統計量對應哪個累積機率。
///
/// n 很大時三者幾乎沒差，n 小（20 以下）時兩端的點會明顯移動。
enum class PlottingPosition {
    Blom,     ///< (i − 0.375) / (n + 0.25)，常態分布下最接近期望值，是 R qqnorm 的做法
    Hazen,    ///< (i − 0.5) / n
    Weibull   ///< i / (n + 1)，分布自由但兩端偏保守
};

/// Q-Q 圖的資料。
struct QQResult {
    QVector<double> theoretical;  ///< 理論分位數（X 軸）
    QVector<double> sample;       ///< 排序後的樣本（Y 軸）
    /// 參考線。不是最小平方迴歸，是穿過兩組資料的第一與第三四分位數的直線 ——
    /// 迴歸線會被離群值拉走，而 Q-Q 圖正是用來找離群值的，那就本末倒置了。
    LinearFit referenceLine;
};

/// 樣本對常態分布的 Q-Q 圖。非有限值會被跳過。
QQResult normalQQ(const QVector<double>& values, PlottingPosition pp = PlottingPosition::Blom);

// ───────────────────── 分類器的 ROC / PR 曲線 ─────────────────────

/// 一條分類器曲線。
struct ClassifierCurve {
    QVector<double> xs;          ///< ROC 是假陽率 FPR；PR 是召回率 Recall
    QVector<double> ys;          ///< ROC 是真陽率 TPR；PR 是精確率 Precision
    QVector<double> thresholds;  ///< 每個點對應的門檻（第一個點是「全部判為陰性」，給 infinity）
    double area = 0;             ///< ROC 是 AUC；PR 是平均精確率 AP
    int positives = 0;
    int negatives = 0;
};

/// ROC 曲線。scores 是分類器給的分數（愈大愈像陽性），labels 是真實類別。
///
/// 同分的樣本必須一起處理：一次只推進一個樣本的話，一群同分樣本之間會畫出
/// 一段階梯，而階梯的形狀取決於它們在輸入陣列裡的先後 —— 同一份資料換個順序
/// 就得到不一樣的 AUC。正確的做法是把同分的整批算完再落一個點，
/// 於是那一批在圖上是一條斜線（代表「無從分辨」），AUC 也不再看輸入順序。
///
/// area 是梯形法的 AUC。兩個陣列長度不同或沒有陽性/陰性樣本時回傳空曲線。
ClassifierCurve rocCurve(const QVector<double>& scores, const QVector<bool>& labels);

/// Precision–Recall 曲線。同分的處理方式和 rocCurve 相同。
///
/// area 是平均精確率 AP = Σ (R_n − R_{n−1}) · P_n，不是梯形面積。
/// PR 曲線在相鄰兩點之間並不是直線（精確率不隨召回率線性變化），
/// 梯形法會系統性地高估 —— sklearn 的 average_precision_score 也是用這個定義。
ClassifierCurve prCurve(const QVector<double>& scores, const QVector<bool>& labels);

/// 梯形法的曲線下面積。xs 必須單調。
double trapezoidArea(const QVector<double>& xs, const QVector<double>& ys);

// ───────────────────────── 存活分析 ─────────────────────────

/// 存活曲線上的一個時間點。
struct SurvivalPoint {
    double time = 0;
    double survival = 1;  ///< S(t)
    double lower = 1;     ///< 信賴區間下界
    double upper = 1;     ///< 信賴區間上界
    int atRisk = 0;       ///< 這個時間點之前仍在追蹤的人數
    int events = 0;       ///< 這個時間點發生的事件數
    int censored = 0;     ///< 這個時間點被設限的人數
};

/// Kaplan–Meier 估計的結果。
struct SurvivalCurve {
    /// 階梯的轉折點，含 t = 0（S = 1）。畫成 Scatter 並把 step 設成 Horizontal。
    ///
    /// 每個「有事情發生」的時間都收一個點，只有設限的時間也算在內 ——
    /// 那種點的 S 不變，但 atRisk 會掉，而設限記號正是要畫在那裡。
    /// 所以索引不等於「第幾個事件」，要找特定時間請照 time 欄位找。
    QVector<SurvivalPoint> points;
    /// 設限記號要畫在哪裡（時間與當時的 S）。
    QVector<double> censorTimes;
    QVector<double> censorSurvival;
    /// S 第一次掉到 0.5 或以下的時間。曲線沒掉到一半就是 NaN。
    double medianSurvival = std::numeric_limits<double>::quiet_NaN();
};

/// Kaplan–Meier 存活曲線。
///
/// times 是每個個體的觀察時間，events[i] 為 true 代表「事件發生了」、
/// false 代表「設限」（追蹤結束時還沒發生，或中途失聯）。
///
/// 信賴區間用 Greenwood 變異數加 log(−log S) 轉換。直接用 S ± z·SE 的話，
/// S 接近 0 或 1 時區間會跑到 [0, 1] 之外 —— 存活率畫到 1.05 很顯然是錯的，
/// 而 log-log 轉換出來的區間永遠落在範圍內。
///
/// 同一個時間同時有事件與設限時，照慣例先算事件再移除設限者。
SurvivalCurve kaplanMeier(const QVector<double>& times, const QVector<bool>& events,
                          double confidence = 0.95);

// ───────────────────────── 管制圖 ─────────────────────────

/// σ 要怎麼估。
enum class SigmaEstimate {
    /// 平均移動全距 / 1.128（d2，n = 2）。
    ///
    /// SPC 的標準做法，只看相鄰兩點的差，所以不受長期漂移影響。
    MovingRange,
    /// 全部資料的樣本標準差。
    ///
    /// 看起來更自然，但製程真的在漂的時候，漂移本身會被算進變異裡，
    /// 管制界限跟著被撐寬，於是該報警的點落在界限內 —— 正好把要找的問題藏起來。
    /// 只有在確定製程穩定時才適用。
    StandardDeviation
};

/// 管制圖的中心線與一個 σ。
struct ControlLimits {
    double center = 0;
    double sigma = 0;
    bool isValid() const { return sigma > 0 && std::isfinite(center); }
    double at(double k) const { return center + k * sigma; }
    double upper() const { return at(3); }
    double lower() const { return at(-3); }
};

/// 從資料算出中心線與 σ。資料少於 2 筆時回傳無效的結果。
ControlLimits controlLimits(const QVector<double>& values,
                            SigmaEstimate estimate = SigmaEstimate::MovingRange);

/// Nelson 的八條判讀規則（Nelson 1984）。
///
/// 只看規則 1 的話，只抓得到「突然壞掉」；製程慢慢漂、變異慢慢縮小這類問題
/// 每一點都還在界限內，要靠後面幾條規則才看得出來。
enum class ControlRule {
    Beyond3Sigma = 1,            ///< 1 點超出 3σ
    NineSameSide = 2,            ///< 連續 9 點落在中心線同一側
    SixTrending = 3,             ///< 連續 6 點持續上升或持續下降
    FourteenAlternating = 4,     ///< 連續 14 點上下交替
    TwoOfThreeBeyond2Sigma = 5,  ///< 3 點中有 2 點在同一側超出 2σ
    FourOfFiveBeyond1Sigma = 6,  ///< 5 點中有 4 點在同一側超出 1σ
    FifteenWithin1Sigma = 7,     ///< 連續 15 點都在 ±1σ 之內（變異異常小，常是量測失靈）
    EightOutside1Sigma = 8       ///< 連續 8 點都在 ±1σ 之外（雙峰，常是兩台機器混在一起）
};

/// 一個被規則抓到的點。
struct RuleViolation {
    int index = 0;
    ControlRule rule = ControlRule::Beyond3Sigma;
};

/// 跑一遍規則。enabled 留空代表只用規則 1。
///
/// 回傳的是「觸發規則的那一串點裡的最後一點」—— 規則 2 到 8 說的是一整串點的形狀，
/// 把整串都標紅會讓圖上一片紅、看不出是哪一條規則在哪裡觸發。
/// 同一點被多條規則抓到就回傳多筆，順序依 index 再依規則編號。
QVector<RuleViolation> nelsonRules(const QVector<double>& values, const ControlLimits& limits,
                                   const QVector<ControlRule>& enabled = {});

/// 規則的中文說明（給圖例或註解用）。
QString controlRuleName(ControlRule rule);

/// 管制圖（SPC）。
///
/// 自己畫中心線、±1σ/±2σ/±3σ 界限與分區帶，並把觸發規則的點標出來。
/// 分區帶會從 limits 推出來，不是另外設的 —— 手動組的時候最常出錯的就是
/// 改了 σ 卻忘了跟著改分區，於是圖上的帶和界限對不起來。
class ControlChart : public IPlottable {
public:
    QVector<double> values;
    /// X 座標。留空則用 0, 1, 2, ...
    QVector<double> xs;

    /// 中心線與 σ。sigma <= 0 時每次 render 由 values 自動算。
    ControlLimits limits;
    SigmaEstimate sigmaEstimate = SigmaEstimate::MovingRange;

    /// 要跑哪些規則。留空只跑規則 1。
    QVector<ControlRule> rules;

    LineStyle line{QColor(50, 90, 150), 1.4};
    MarkerStyle marker{MarkerShape::FilledCircle, 6, QColor(50, 90, 150)};
    /// 被規則抓到的點改用這個標記。
    MarkerStyle violationMarker{MarkerShape::FilledCircle, 10, QColor(205, 50, 45)};
    /// 要不要在被抓到的點旁邊標規則編號。
    bool showViolationLabels = true;
    FontStyle violationFont;

    LineStyle centerLine{QColor(70, 70, 70), 1.2};
    LineStyle controlLine{QColor(205, 50, 45), 1.2, LinePattern::Dashed};
    /// ±1σ 與 ±2σ 的界線。
    LineStyle sigmaLine{QColor(0, 0, 0, 55), 1.0, LinePattern::Dotted};
    /// ±1σ / ±2σ / ±3σ 三層分區的底色（由內往外）。空的顏色代表那一層不填。
    QVector<QColor> zoneColors{QColor(90, 160, 90, 28), QColor(225, 185, 70, 30),
                               QColor(205, 50, 45, 26)};
    /// 要不要在右邊標 CL / UCL / LCL。
    bool showLimitLabels = true;
    FontStyle limitFont;

    /// 實際用的界限（limits 有效就用它，否則現算）。
    ControlLimits effectiveLimits() const;
    /// 跑規則的結果。
    QVector<RuleViolation> violations() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(violationFont);
        visit(limitFont);
    }
};

// ───────────────────────── 森林圖 ─────────────────────────

/// 森林圖的一列。
struct ForestRow {
    QString label;
    double estimate = 0;
    double lower = 0;
    double upper = 0;
    /// 方塊的大小（通常是研究的權重或樣本數）。<= 0 則所有方塊一樣大。
    double weight = 0;
    /// true 時畫成菱形（合併估計值），而且不受 weight 影響。
    bool isSummary = false;
    QColor color;
    /// 右邊要印的文字（例如「2.14 [1.30, 3.52]」）。空字串代表不印。
    QString annotation;
};

/// 森林圖：每一列一個點估計加信賴區間，最底下通常是合併估計值的菱形。
///
/// 比值類的效應量（OR、RR、HR）要畫在對數軸上，否則「兩倍」和「一半」
/// 在圖上的長度不一樣，視覺上會偏向其中一邊。做法是自己先取 log10()
/// 再把軸換成 LogTickGenerator —— 這個函式庫的對數軸是這樣用的，
/// 圖元本身不做轉換。nullValue 也要記得一起取 log（比值的無效線 1 → 0）。
class ForestPlot : public IPlottable {
public:
    QVector<ForestRow> rows;
    /// 第一列畫在最上面（表格的習慣）。
    bool topToBottom = true;

    LineStyle intervalLine{QColor(40, 40, 40), 1.4};
    /// 區間兩端的帽子半長（像素）。0 代表不畫帽子。
    double capSizePx = 0;
    /// 方塊的邊長（像素）。weight 最大的那一列用這個大小，其餘按面積等比縮小 ——
    /// 按邊長縮的話，權重差兩倍看起來像差四倍。
    double maxBoxSizePx = 14;
    double minBoxSizePx = 5;
    QColor defaultColor{60, 90, 160};
    /// 菱形（合併值）的高度，佔一列高度的比例。
    double summaryHeightFraction = 0.55;
    QColor summaryColor{40, 40, 40};

    /// 無效應線的位置。NaN 代表不畫。
    double nullValue = 0;
    LineStyle nullLine{QColor(120, 120, 120), 1.0, LinePattern::Dashed};

    /// 右邊的數值欄。畫在繪圖區「之內」靠右緣的位置，所以要記得把 X 軸的右界
    /// 留寬一點，否則數字會壓到區間上。
    FontStyle annotationFont;
    double annotationOffsetPx = 6;

    /// 區間被繪圖區切掉時，在邊緣畫一個箭頭表示「還更長」。
    bool showClippedArrows = true;

    /// 每一列在類別軸上的位置。
    QVector<double> rowPositions() const;
    /// 把列名拿出來給 setCategoryTicks 用（依 rowPositions 的順序）。
    QStringList rowLabels() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(annotationFont);
    }
};


// ───────────────────────── 兩種量測方法的一致性 ─────────────────────────

/// Bland–Altman 圖的資料。
struct BlandAltman {
    QVector<double> means;        ///< 兩法的平均（X 軸）
    QVector<double> differences;  ///< 兩法的差（Y 軸）
    double bias = 0;              ///< 差的平均，也就是系統性偏差
    double sd = 0;                ///< 差的標準差
    double lowerLimit = 0;        ///< bias − 1.96·sd
    double upperLimit = 0;        ///< bias + 1.96·sd
    int count = 0;
};

/// 兩種量測方法的一致性分析。
///
/// 不要用相關係數或迴歸來判斷兩種方法可不可以互換：相關係數量的是
/// 「一起變動」，兩把尺差了固定的 5 單位，相關係數照樣是 1。
/// 要看的是「差」隨「平均」怎麼變 —— 那就是這張圖。
///
/// 一致性界限用 bias ± 1.96·sd（差呈常態時涵蓋 95%）。
/// agreementZ 可以改成別的倍數。
BlandAltman blandAltman(const QVector<double>& methodA, const QVector<double>& methodB,
                        double agreementZ = 1.959963985);

// ───────────────────── 後設分析的漏斗圖 ─────────────────────

/// 後設分析漏斗圖的一個研究。
///
/// 注意這和商務的「漏斗圖」(Funnel) 完全是兩回事：那個畫的是流程各階段的轉換率，
/// 這個是用來看發表偏倚的散點圖。
struct MetaStudy {
    double effect = 0;        ///< 效應量（比值類請先取 log）
    double standardError = 0; ///< 標準誤
};

/// 漏斗圖的輔助線。
struct FunnelContour {
    double pooledEffect = 0;      ///< 固定效應合併值（倒變異加權）
    QVector<double> seLevels;     ///< 由 0 到最大 SE 的取樣點
    QVector<double> leftBound95;  ///< 95% 偽信賴區間的左界
    QVector<double> rightBound95; ///< 右界
    /// Egger 迴歸的截距。離 0 愈遠代表漏斗愈不對稱（可能有發表偏倚）。
    ///
    /// 做法是把「標準常態離差」對「精確度」做迴歸：
    /// effect/se = a + b·(1/se)。截距 a 就是不對稱的程度。
    double eggerIntercept = 0;
    double eggerSlope = 0;
};

/// 由各研究算出合併值與漏斗的 95% 輪廓。研究少於 2 個時回傳預設值。
FunnelContour funnelContour(const QVector<MetaStudy>& studies, int samples = 48,
                            double z = 1.959963985);

// ───────────────────────── 不均等程度 ─────────────────────────

/// 勞倫茲曲線與吉尼係數。
struct LorenzCurve {
    QVector<double> populationFraction;  ///< 累積人口比例，從 0 開始
    QVector<double> valueFraction;       ///< 累積財富（或其他量）比例，從 0 開始
    /// 吉尼係數：0 是完全平均、1 是完全集中在一個人身上。
    ///
    /// 用梯形法算曲線下面積再換算（G = 1 − 2A）。這和「平均絕對差」的定義
    /// 在樣本上會差一個 n/(n−1) 的小因子 —— 這裡用的是曲線面積的定義，
    /// 因為畫出來的圖和算出來的數字必須是同一回事。
    double gini = 0;
};

/// 由一組非負的值算勞倫茲曲線。負值與非有限值會被跳過。
LorenzCurve lorenzCurve(const QVector<double>& values);

// ───────────────────────── 機率紙 ─────────────────────────

/// 機率紙上的一組點。
struct ProbabilityPlot {
    /// X：樣本值（Weibull 時是 ln(value)）。
    QVector<double> xs;
    /// Y：轉換過的累積機率。常態是 Φ⁻¹(p)，Weibull 是 ln(−ln(1−p))。
    QVector<double> ys;
    /// 穿過四分位數的參考線。
    LinearFit referenceLine;
    /// Weibull 專用：由參考線推出的形狀參數 β 與尺度參數 η。
    double weibullShape = 0;
    double weibullScale = 0;
};

/// 機率紙要畫哪一種分布。
enum class ProbabilityPaper {
    Normal,   ///< Y = Φ⁻¹(p)；資料是常態時成一直線
    Weibull   ///< X = ln(x)、Y = ln(−ln(1−p))；資料是 Weibull 時成一直線
};

/// 機率紙。
///
/// 和 Q-Q 圖的分工：Q-Q 圖把樣本對上「理論分位數」，兩軸都是原始單位；
/// 機率紙把 Y 軸換成讓目標分布變成直線的刻度，所以除了判斷配不配得上，
/// 還能直接從直線的斜率與截距讀出參數 —— Weibull 的 β 就是斜率。
ProbabilityPlot probabilityPlot(const QVector<double>& values, ProbabilityPaper paper,
                                PlottingPosition pp = PlottingPosition::Blom);

}  // namespace qtplot

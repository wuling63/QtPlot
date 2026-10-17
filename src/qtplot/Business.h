// Business.h — 商務／示意類圖表：漏斗、金字塔、瀑布、靶心、甘特、文氏。
//
// 這幾種的共同點是「類別軸是一串名字、數值軸才是數字」，而且版型的細節
// （哪一段對齊哪裡、連接線怎麼接）比數學更重要。放在一起是因為它們都不屬於
// 既有的任何一族，各自又不大。
#pragma once

#include "Plottable.h"

#include <QMap>

namespace qtplot {

// ───────────────────────── 漏斗圖／金字塔圖 ─────────────────────────

/// 漏斗／金字塔的一段。
struct FunnelStage {
    double value = 0;
    QString label;
    QColor fillColor;  ///< 無效色代表由 Plot 的調色盤指定
    /// 寫在漏斗右側、和這一段對齊的說明（可多行）。
    QString description;
};

/// 漏斗圖與金字塔圖。兩者是同一種圖，差在寬度怎麼決定、哪一頭比較寬。
///
/// 要注意這種圖很容易被誤讀：讀者會把「面積」當成數量，但梯形的面積和
/// 它代表的值並不成正比（上下底不同）。真正要比較數量請用長條圖；
/// 漏斗圖的用途是表達「一層一層篩下來」這件事本身。
class Funnel : public IPlottable {
public:
    QVector<FunnelStage> stages;

    enum class WidthMode {
        Value,   ///< 每段的寬度正比於它的 value（典型的轉換漏斗）
        Linear   ///< 寬度線性遞減，和數值無關（金字塔的三角形外廓）
    };
    WidthMode widthMode = WidthMode::Value;

    /// true 時最寬的那一段在下面（金字塔），false 時在上面（漏斗）。
    bool inverted = false;

    /// 整個圖形畫在這個資料矩形裡。
    CoordinateRect extent{0, 1, 0, 1};
    /// 段與段之間留的空隙，佔單段高度的比例。
    double gapFraction = 0.06;

    LineStyle border{QColor(255, 255, 255), 1.5};
    FontStyle labelFont;
    /// 是否把 value 接在標籤後面。
    bool showValues = true;

    /// 把每一段畫成圓錐的一截：上下緣是橢圓，頂面是一個完整的橢圓（資訊圖表風格）。
    bool cone3D = false;
    /// 頂面的畫法。
    enum class CapStyle {
        Light,  ///< 淺色的實心頂面，像從上方看一塊切片
        Hollow  ///< 深色的內面加一圈淺色邊，像一個空杯子
    };
    CapStyle cap = CapStyle::Light;
    /// 上下緣橢圓的高度，佔該段半寬的比例。
    double ellipseRatio = 0.12;

    /// 說明文字（FunnelStage::description）的字型。
    FontStyle descriptionFont;
    /// 說明文字離漏斗右緣多遠（資料單位）。
    double descriptionGap = 0.05;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 由數值序列建一組段落，依序取調色盤的顏色。
    static QVector<FunnelStage> fromValues(const QVector<double>& values,
                                           const QStringList& labels,
                                           const Palette& palette);

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(descriptionFont);
    }
};

// ───────────────────────── 瀑布圖 ─────────────────────────

/// 瀑布圖的一根長條。
struct WaterfallStep {
    /// 這一步的變化量。isTotal 為 true 時這個值被忽略。
    double delta = 0;
    QString label;
    /// true 代表這根是「小計／總計」：從 0 畫到目前的累計值，而不是一段增減。
    ///
    /// 它只改「從哪裡畫」，不改算術 —— delta 照樣計入累計。期初餘額就是
    /// delta = 120、isTotal = true（從 0 畫到 120，而且讓累計從 120 開始）；
    /// 中途的小計是 delta = 0、isTotal = true（不加東西，只是把累計畫出來）。
    bool isTotal = false;
    QColor fillColor;  ///< 無效色代表依正負自動取色
};

/// 瀑布圖：從起點開始，一步一步加減，看得出每一項對結果的貢獻。
///
/// 每根長條的底是前一根的頂，所以中間那些長條是「浮」在空中的；
/// 這正是瀑布圖和堆疊長條圖的差別。
class Waterfall : public IPlottable {
public:
    QVector<WaterfallStep> steps;

    QColor riseColor{76, 175, 80};
    QColor fallColor{229, 57, 53};
    QColor totalColor{66, 133, 244};

    /// 長條寬度（類別軸的資料單位）。
    double barSize = 0.6;
    LineStyle borderLine{QColor(0, 0, 0, 0), 1.0};
    /// 連接相鄰兩根的細線。畫在前一根的頂端高度上。
    LineStyle connectorLine{QColor(0, 0, 0, 110), 1.0, LinePattern::Dashed};

    bool showValueLabels = true;
    FontStyle valueFont;
    /// 把增減量印在長條「裡面」而不是外側。長條太矮塞不下時仍會退回外側。
    bool valueLabelInside = false;
    /// 另外在每根長條的上方標出當下的累計值。
    ///
    /// 和增減量一起看才完整：增減量回答「這一步改變了多少」，
    /// 累計值回答「到這一步為止是多少」。只有後者能和 Y 軸的刻度對起來。
    bool showCumulativeLabels = false;
    FontStyle cumulativeFont;

    /// 每一根的累計高度（第 i 項是做完第 i 步之後的總值）。
    QVector<double> cumulative() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(valueFont);
        visit(cumulativeFont);
    }
};

// ───────────────────────── 靶心圖（Bullet） ─────────────────────────

/// 靶心圖的一列。
struct BulletRow {
    QString label;
    double value = 0;   ///< 實際值（中間那根粗長條）
    double target = 0;  ///< 目標（垂直記號）
    /// 由小到大的分級邊界，畫成由深到淺的背景帶（例如 {60, 80, 100}）。
    QVector<double> ranges;
    QColor valueColor{40, 40, 40};
};

/// 靶心圖：用很小的版面同時表達「實際值、目標、好壞分級」。
///
/// 常用來取代儀表板上的量表 —— 量表佔一大塊地方卻只放得下一個數字，
/// 靶心圖一列就講完同樣的事，而且好幾列疊起來可以直接互相比較。
class Bullet : public IPlottable {
public:
    QVector<BulletRow> rows;

    /// 一列佔的高度（資料單位）。
    double rowHeight = 0.7;
    /// 中間那根粗長條佔整列高度的比例。
    double valueHeightFraction = 0.38;
    /// 背景分級帶的基色。由深到淺自動產生。
    QColor rangeColor{130, 130, 130};
    LineStyle targetLine{QColor(20, 20, 20), 2.5};
    /// 目標記號的高度，佔整列高度的比例。
    double targetHeightFraction = 0.8;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};

// ───────────────────────── 啞鈴圖 ─────────────────────────

/// 啞鈴圖的一列。
struct DumbbellRow {
    QString label;
    double from = 0;   ///< 起點（例如去年）
    double to = 0;     ///< 終點（例如今年）
    QColor fromColor;
    QColor toColor;
};

/// 啞鈴圖：每一列兩個點加一條連線，表示「從哪裡變到哪裡」。
///
/// 比「前後兩根長條」好讀：長條要比較的是兩個「長度」，啞鈴要比較的是
/// 兩個「位置」加中間那段「距離」—— 而變化量正是那段距離，直接看得到。
/// 代價是看不出絕對量的大小感（長條的面積有那個作用）。
class Dumbbell : public IPlottable {
public:
    QVector<DumbbellRow> rows;
    /// true 時每一列是水平的（類別在 Y 軸）。
    bool horizontal = true;

    LineStyle connector{QColor(150, 150, 150), 3.0};
    MarkerStyle fromMarker{MarkerShape::FilledCircle, 11, QColor(140, 180, 220)};
    MarkerStyle toMarker{MarkerShape::FilledCircle, 11, QColor(220, 90, 70)};

    /// 在兩端標出數值。
    bool showValues = false;
    FontStyle valueFont;
    double valueGapPx = 8;
    /// 把變化量標在連線的中間。
    bool showDelta = false;
    FontStyle deltaFont;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 兩端的圖例文字。空字串代表那一端不進圖例。
    QString fromLegendText;
    QString toLegendText;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(valueFont);
        visit(deltaFont);
    }
};

// ───────────────────────── 甘特圖 ─────────────────────────

/// 甘特圖的一項工作。
struct GanttTask {
    QString label;
    /// 起訖時間。用時間戳當單位時，搭配 DateTimeTickGenerator。
    double start = 0;
    double end = 0;
    /// 排在第幾列（由上往下數，0 是最上面）。同一列可以放多個不重疊的工作。
    int row = 0;
    /// 完成度 0–1。> 0 時會在長條內疊一段深色。
    double progress = 0;
    QColor fillColor;
    /// 這項工作要等哪幾項做完（索引）。會畫一條箭頭連過去。
    QVector<int> dependsOn;
};

/// 甘特圖：橫條代表一段期間，由上往下排成一列一列。
///
/// 和一般的水平長條圖不同之處在於「列」是版面概念而不是資料：同一列可以放好幾項
/// 不重疊的工作，列號也由使用者指定，不是由資料順序決定。
class Gantt : public IPlottable {
public:
    QVector<GanttTask> tasks;

    /// 長條高度（資料單位，一列算 1）。
    double barHeight = 0.62;
    double cornerRadiusPx = 3;
    LineStyle borderLine{QColor(0, 0, 0, 90), 1.0};
    /// 完成度那一段的壓暗程度（QColor::darker 的係數）。
    int progressDarken = 150;
    /// 相依箭頭。顏色透明代表不畫。
    LineStyle dependencyLine{QColor(80, 80, 80, 200), 1.2};
    double arrowHeadPx = 7;

    FontStyle labelFont;
    /// 把工作名稱印在長條裡面。關掉就只靠左側的軸刻度。
    bool showTaskLabels = true;

    /// 用到的列數。
    int rowCount() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

// ───────────────────────── 馬賽克圖（Marimekko） ─────────────────────────

/// 馬賽克圖的一欄。
struct MosaicColumn {
    QString label;
    /// 這一欄裡各段的值。欄寬正比於它們的總和。
    QVector<double> values;
    /// 各段的標籤。少於 values 的部分就不標。
    QStringList segmentLabels;
};

/// 馬賽克圖（Marimekko）：欄寬正比於該欄的總量，欄內再依比例堆疊。
///
/// 和一般的百分比堆疊長條圖差在「欄寬」—— 一般的堆疊圖每欄一樣寬，
/// 看不出哪一欄本身比較大。馬賽克圖讓每個方塊的「面積」正比於它的值，
/// 所以一眼看得出誰佔的份量重。
///
/// 代價是很難比較不同欄之間的同一段：它們寬度不同、起點也不同。
/// 要逐段比較請用分組長條圖。
class Mosaic : public IPlottable {
public:
    QVector<MosaicColumn> columns;
    /// 各段的顏色（依段的索引）。不足的部分由調色盤補。
    QVector<QColor> segmentColors;
    /// 各段的名稱，給圖例用。
    QStringList segmentNames;

    /// 整張圖畫在這個資料矩形裡。
    CoordinateRect extent{0, 1, 0, 1};
    /// 欄與欄之間的間隔，佔整寬的比例。
    double columnGapFraction = 0.004;

    LineStyle border{QColor(255, 255, 255), 1.0};
    FontStyle labelFont;
    /// 方塊小於這個像素面積就不標文字。
    double minLabelAreaPx = 900;
    /// 欄名要不要印在下面。
    bool showColumnLabels = true;
    /// 欄的總量要不要印在上面。
    bool showColumnTotals = true;
    FontStyle columnLabelFont;

    /// 每一欄的總量。
    QVector<double> columnTotals() const;
    /// 全部的總量。
    double grandTotal() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
        visit(columnLabelFont);
    }
};

// ───────────────────────── 文氏圖 ─────────────────────────

/// 文氏圖的一個集合。
struct VennSet {
    QString label;
    QColor fillColor;
};

/// 文氏圖：兩個或三個互相重疊的圓。
///
/// 這裡畫的是「示意」而不是「比例」—— 圓的大小與重疊面積都是固定版型，
/// 不隨數字變化。要讓面積正比於集合大小（比例文氏圖）在三個集合時
/// 通常無解，所以多數工具也都畫示意版。
class Venn : public IPlottable {
public:
    /// 2 或 3 個集合。超過 3 個就不是文氏圖能表達的了。
    QVector<VennSet> sets;

    Coordinates center{0, 0};
    double radius = 1.0;
    double fillOpacity = 0.55;
    LineStyle border{QColor(0, 0, 0, 0), 1.0};

    FontStyle setLabelFont;   ///< 集合名稱（畫在圓外）
    FontStyle countLabelFont; ///< 各區塊的數字（畫在區塊中央）

    /// 每個區塊的文字。鍵是位元遮罩：1 = 只有 A、3 = A∩B、7 = A∩B∩C。
    ///
    /// 用 QMap 而不是 QHash：QHash 的走訪順序每個行程都不一樣，
    /// 輸出的 PNG 會因此不可重現，視覺回歸就沒得比了。
    QMap<int, QString> regionLabels;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 各個圓的圓心（依 sets 的順序）。版型是固定的。
    QVector<Coordinates> circleCenters() const;
    /// 某個區塊的文字該放在哪裡。mask 的意義同 regionLabels。
    Coordinates regionAnchor(int mask) const;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(setLabelFont);
        visit(countLabelFont);
    }
};


// ───────────────────────── 魚骨圖（石川圖） ─────────────────────────

/// 一條原因。
struct FishboneCause {
    QString text;
    /// 再往下分一層。空的話就只畫這一條。
    QStringList subCauses;
};

/// 一根主骨：一個原因類別。
struct FishboneCategory {
    QString name;
    QVector<FishboneCause> causes;
    /// 空的話由調色盤配。
    QColor color;
};

/// 魚骨圖（石川圖、因果圖）。
///
/// 右端是要解決的問題，脊椎往左延伸，主骨是原因的「類別」，
/// 類別底下再掛具體原因。品管上常用的分類是 6M：
/// 人（Man）、機（Machine）、料、法、環、測。
///
/// 這張圖沒有數值，是一張結構圖 —— 版面完全由 extent 決定，
/// 和座標軸的刻度無關。要調大小就改 extent，不要去縮軸。
class Fishbone : public IPlottable {
public:
    /// 右端的問題描述。
    QString effect;
    QVector<FishboneCategory> categories;
    Palette palette{Palette::Category10};

    /// 整張圖畫在這個資料矩形裡。
    CoordinateRect extent{0, 1, 0, 1};

    /// 類別上下交錯排。關掉的話全部排在上面。
    bool alternateSides = true;

    /// 主骨和脊椎的夾角（度）。
    ///
    /// 角度小的話骨頭躺得平、往左伸得遠，容易和左邊的鄰居打架；
    /// 角度大則骨頭接近垂直，掛在上面的原因文字會擠成一團。60 度是常見的折衷。
    double boneAngleDegrees = 60;

    /// 主骨的長度，佔半高的比例。
    double boneLengthFraction = 0.78;
    /// 原因的橫線長度，佔整寬的比例。
    double causeLineFraction = 0.085;
    /// 魚頭（問題方框）佔整寬的比例。
    double headFraction = 0.17;

    LineStyle spine{QColor(60, 60, 60), 2.2};
    LineStyle boneLine{QColor(60, 60, 60), 1.8};
    LineStyle causeLine{QColor(110, 110, 110), 1.1};

    FontStyle effectFont;
    FontStyle categoryFont;
    FontStyle causeFont;
    FontStyle subCauseFont;

    /// 魚頭方框的樣式。
    FillStyle headFill{QColor(240, 240, 240)};
    LineStyle headBorder{QColor(60, 60, 60), 1.6};
    /// 類別名稱後面的方框。
    bool boxCategoryLabels = true;

    /// 要不要畫第二層原因。原因一多，第二層會擠到看不清楚，那就關掉。
    bool showSubCauses = true;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 便利建構：6M 的空骨架，填 causes 就好。
    static QVector<FishboneCategory> sixM();

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(effectFont);
        visit(categoryFont);
        visit(causeFont);
        visit(subCauseFont);
    }
};


// ───────────────────────── 柏拉圖 ─────────────────────────

/// 柏拉圖的一項。
struct ParetoItem {
    QString label;
    double value = 0;
    QColor color;
};

/// 柏拉圖：長條由大到小，加上一條累積百分比折線。
///
/// 品管三大工具之一（另兩個是魚骨圖與管制圖）。重點是「少數幾項佔了大部分」——
/// 累積線先碰到 80% 的那幾項就是該先處理的。
///
/// 長條用左軸（數量），折線用右軸（0–100%）。右軸要自己綁：
/// Plot::addPareto() 會建好並綁上去。
class Pareto : public IPlottable {
public:
    QVector<ParetoItem> items;
    /// 自動由大到小排序。關掉的話就照給的順序畫 ——
    /// 柏拉圖的定義就是排序過的，關掉只有在資料已經排好時才該用。
    bool sortDescending = true;

    /// 折線要綁的那根軸（0–100 的百分比軸）。nullptr 時不畫折線。
    Axis* cumulativeAxis = nullptr;

    LineStyle barBorder{QColor(255, 255, 255), 1.0};
    double barWidth = 0.8;
    Palette palette{Palette::Category10};
    /// 所有長條用同一個顏色（柏拉圖通常不需要用顏色區分項目）。
    QColor uniformColor{70, 110, 170};
    /// true 時每一項各自配色。
    bool colorByItem = false;

    LineStyle cumulativeLine{QColor(200, 70, 50), 1.8};
    MarkerStyle cumulativeMarker{MarkerShape::FilledCircle, 6, QColor(200, 70, 50)};
    /// 參考線的百分比。設 0 代表不畫。
    double referencePercent = 80;
    LineStyle referenceLine{QColor(120, 120, 120), 1.0, LinePattern::Dashed};

    bool showValueLabels = false;
    FontStyle valueFont;

    /// 排序後的項目（sortDescending 關掉時就是原順序）。
    QVector<ParetoItem> sortedItems() const;
    /// 累積百分比，和 sortedItems() 等長。
    QVector<double> cumulativePercents() const;
    /// 累積到達 percent 所需的項目數。資料為空時回傳 0。
    int itemsToReach(double percent) const;
    double total() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override { visit(valueFont); }
};

// ───────────────────────── 鬆餅圖 ─────────────────────────

/// 鬆餅圖的一類。
struct WaffleCategory {
    QString label;
    double value = 0;
    QColor color;
};

/// 鬆餅圖：用一格一格的方塊表示比例。
///
/// 和圓餅圖的分工：圓餅要比的是角度，人對角度的判斷很差；鬆餅是「數格子」，
/// 10% 就是十格，看一眼就知道。代價是只適合少數幾類、而且比例不能太零碎。
class Waffle : public IPlottable {
public:
    QVector<WaffleCategory> categories;

    int rows = 10;
    int columns = 10;
    /// 格子之間的間隙，佔一格的比例。
    double gapFraction = 0.12;
    /// 格子的圓角半徑，佔一格的比例。0 是直角。
    double cornerFraction = 0.15;
    /// 整張圖畫在這個資料矩形裡。
    CoordinateRect extent{0, 10, 0, 10};
    /// 由下往上填（一般的進度感），關掉則由上往下。
    bool fillFromBottom = true;
    /// 沒被任何類別填到的格子的顏色。
    QColor emptyColor{225, 225, 225};
    LineStyle border{QColor(255, 255, 255), 0.0};
    Palette palette{Palette::Category10};

    /// 每一格屬於哪一類（-1 代表空格），長度是 rows × columns。
    ///
    /// 配額用「最大餘額法」分：先各自取整數部分，剩下的格子依小數部分由大到小補。
    /// 直接四捨五入的話，格子總數會對不上（10 個 3.33% 會變成 30 格或 40 格）。
    QVector<int> cellAssignment() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;
};

// ───────────────────────── 斜率圖與排名圖 ─────────────────────────

/// 一條線：一個項目在各期的值。
struct SlopeSeries {
    QString label;
    QVector<double> values;  ///< 和 Slope::periodLabels 等長
    QColor color;
    /// true 時用粗線與深色強調（其餘自動淡化）。
    bool highlight = false;
};

/// 斜率圖／排名變化圖（bump chart）。
///
/// 兩期就是斜率圖，多期就是排名圖。重點不是絕對值，是「誰升誰降」——
/// 折線的交叉一眼就看得出來，而同樣的資料畫成分組長條圖就得逐組比對。
class Slope : public IPlottable {
public:
    QVector<SlopeSeries> series;
    QStringList periodLabels;

    /// 畫的是名次而不是數值。
    ///
    /// 開啟後每一期各自排名（1 是最大），Y 軸要配 inverted = true 讓第 1 名在上面。
    /// 數值跨好幾個數量級時，看名次比看原始值清楚。
    bool useRanks = false;

    LineStyle line{QColor(120, 120, 120), 1.6};
    /// 沒被強調的線用這個透明度。
    int dimAlpha = 70;
    MarkerStyle marker{MarkerShape::FilledCircle, 7, QColor(120, 120, 120)};
    Palette palette{Palette::Category10};

    /// 左右兩端要不要標項目名。
    bool showEndLabels = true;
    FontStyle labelFont;
    /// 端點標籤離端點多遠（像素）。
    double labelGapPx = 8;
    /// 要不要在端點標籤後面附上數值。
    bool showEndValues = true;

    /// 每一期的名次（1 起算）。series 的順序不變。
    QVector<QVector<double>> ranks() const;
    /// 實際畫出來的 Y 值（useRanks 時是名次，否則是原始值）。
    QVector<QVector<double>> plotValues() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override { visit(labelFont); }
};

}  // namespace qtplot

// Axis.h — 座標軸與刻度產生器。
//
// 刻度是整個繪圖庫最容易做醜的地方，所以產生器拆成獨立介面：
// 自動數值、手動指定、對數、日期時間各自實作，軸本身只管量尺寸與畫。
#pragma once

#include <optional>

#include "Core.h"

#include <QDateTime>
#include <QImage>
#include <QTimeZone>

#include <functional>
#include <memory>

namespace qtplot {

/// 一根刻度。label 為空代表只畫刻度線不印字（次刻度）。
struct Tick {
    double position = 0;
    QString label;
    bool isMajor = true;
};

/// 刻度產生器介面。
class TickGenerator {
public:
    virtual ~TickGenerator() = default;
    /// range：軸的資料範圍；lengthPx：軸在螢幕上的長度；
    /// vertical：垂直軸（標籤高度決定密度）或水平軸（標籤寬度決定密度）。
    virtual QVector<Tick> generate(const CoordinateRange& range, double lengthPx,
                                   const FontStyle& font, bool vertical) = 0;
};

/// 自動挑 1 / 2 / 5 × 10^n 的「好看」間距，並確保標籤不會互相重疊。
class AutomaticTickGenerator : public TickGenerator {
public:
    int minorTicksPerMajor = 5;   ///< 每個主刻度之間的次刻度數
    double labelPaddingPx = 6;    ///< 相鄰標籤之間至少要留的空白
    int maxTicks = 50;            ///< 保險上限，避免極端範圍產生數萬根刻度
    /// 刻度密度倍率。1 是預設；2 讓刻度約密一倍，0.5 約疏一倍（標籤仍不會重疊）。
    double tickDensity = 1.0;
    /// 想要大約幾根主刻度。0 代表依軸長自動決定。設了之後實際數量仍會取「好看」的間距，
    /// 所以可能差一兩根。
    int targetTickCount = 0;

    QVector<Tick> generate(const CoordinateRange& range, double lengthPx,
                           const FontStyle& font, bool vertical) override;

    /// 自訂標籤文字（例如印成百分比或加單位）。回傳空字串代表不印。
    std::function<QString(double value, double spacing)> labelFormatter;

    /// 數值很大或很小時，把共同的 10 的次方抽出來放在軸的端點。
    ///
    /// 不開的話，0 到 1,000,000 的軸會印成 0 / 200000 / … / 1e+06，又長又不一致。
    /// 開著會變成 0 / 0.2 / … / 1.0 再加一個「×10⁶」的標示，這是 matplotlib
    /// 的做法。labelFormatter 有設的時候不會套用。
    bool allowMultiplier = true;
    /// 超過幾位數才抽倍率。
    int multiplierThreshold = 5;
    /// 上一次 generate() 算出的倍率標示。沒有倍率時為空字串。
    QString multiplierLabel;
};

/// 完全由使用者指定刻度位置與文字。
class ManualTickGenerator : public TickGenerator {
public:
    QVector<Tick> ticks;
    QVector<Tick> generate(const CoordinateRange&, double, const FontStyle&, bool) override {
        return ticks;
    }
};

/// 對數軸（底數可調）。主刻度在 10^n，次刻度在 2..9 × 10^n。
class LogTickGenerator : public TickGenerator {
public:
    double base = 10.0;
    /// 每個十的次方之間要畫的次刻度倍數（預設 2..9）。只要 2、5 的話就給 {2, 5}；空的就不畫次刻度。
    QVector<double> minorMultipliers{2, 3, 4, 5, 6, 7, 8, 9};
    QVector<Tick> generate(const CoordinateRange& range, double lengthPx,
                           const FontStyle& font, bool vertical) override;
};

/// 日期時間軸。資料座標是 Unix 秒數（double，可含小數）。
class DateTimeTickGenerator : public TickGenerator {
public:
    QString format;  ///< 空字串代表依縮放層級自動挑格式
    /// 自訂刻度文字。第二個參數是刻度間距（秒），可以依縮放層級換不同寫法。
    /// 有設就蓋過 format。
    std::function<QString(const QDateTime& time, double stepSeconds)> labelFormatter;

    /// 固定的刻度間隔。Auto 依縮放層級自動挑；其他值則永遠每 intervalCount 個單位一根，
    /// 例如每週一一根（Weeks）、每 6 小時一根（Hours, 6）。
    ///
    /// 週報、班表這類資料，刻度要對到「週一」「整點換班」才有意義，自動挑的間距對不上。
    /// 縮放時刻度數量會跟著變多變少，太密的話標籤會擠在一起 —— 這是固定間隔的代價。
    enum class Interval { Auto, Seconds, Minutes, Hours, Days, Weeks, Months, Years };
    Interval interval = Interval::Auto;
    int intervalCount = 1;
    QVector<Tick> generate(const CoordinateRange& range, double lengthPx,
                           const FontStyle& font, bool vertical) override;
};

enum class Edge { Left, Right, Bottom, Top };

/// 一根座標軸：範圍、刻度、標籤，以及它在版面上要佔多厚。
class Axis {
public:
    explicit Axis(Edge edge);

    Edge edge() const { return m_edge; }
    bool isVertical() const { return m_edge == Edge::Left || m_edge == Edge::Right; }
    bool isHorizontal() const { return !isVertical(); }

    // ── 範圍 ──
    /// 目前的視野。互動（平移、縮放）會直接改這個值。
    /// 要「指定」視野請用 setRange()，直接寫 range 不會被 autoScale 尊重。
    CoordinateRange range{-10, 10};
    /// 使用者是否指定過範圍。設過之後 Plot::autoScale() 就不會覆蓋這根軸，
    /// 但平移與縮放仍然可用（這點和 locked 不同）。
    bool rangeWasSet = false;
    void setRange(double min, double max) {
        range = {min, max};
        rangeWasSet = true;
    }
    /// 讓這根軸重新接受 autoScale。
    void clearRangeOverride() { rangeWasSet = false; }
    /// 鎖住的軸不受平移、縮放與自動範圍影響。
    bool locked = false;
    /// 反向軸（值由大到小）。
    bool inverted = false;

    // ── 截斷 ──
    /// 截掉 [from, to] 這一段：那段不佔長度，留一道空隙畫截斷記號。
    ///
    /// 數值差距很大的資料用（例如 0–10 和 1000–1010 畫在同一張圖上）。
    /// 重疊的截斷會自動合併，from > to 會自動對調。刻度在每一段各自產生，
    /// 所以兩段都有整齊的刻度，不會只剩頭尾兩個。
    void addBreak(double from, double to);
    void clearBreaks() { m_breaks.breaks.clear(); }
    bool hasBreaks() const { return !m_breaks.isEmpty(); }
    const AxisBreaks& breaks() const { return m_breaks; }
    /// 截斷空隙的寬度（像素）。
    double breakGapPx() const { return m_breaks.gapPx; }
    void setBreakGapPx(double px) { m_breaks.gapPx = std::max(0.0, px); }
    /// 截斷記號（框線上的兩道斜線）。
    LineStyle breakMarkLine{QColor(0, 0, 0), 1.2};
    double breakMarkSize = 6;
    /// 空隙橫跨整個繪圖區塗成底色，穿過截斷的線看起來才是「斷開」的。
    bool fillBreakGap = true;
    /// 畫截斷的空隙與記號。由 Plot 在圖元與軸都畫完之後呼叫。
    void renderBreaks(QPainter& painter, const PixelRect& dataRect, QColor background) const;

    // ── 外觀 ──
    Label label;                       ///< 軸標題（垂直軸會自動轉 90°）
    /// 副標題：排在軸標題外側的一行小字（單位、資料來源）。預設灰色小字。
    Label subLabel;
    /// 用一張影像當軸標題（例如排好版的公式、圖示）。有設就取代 label 的文字。
    QImage labelImage;
    FontStyle tickFont;
    LineStyle frameLine{QColor(0, 0, 0), 1.0};
    LineStyle majorTickLine{QColor(0, 0, 0), 1.0};
    LineStyle minorTickLine{QColor(0, 0, 0), 1.0};
    double majorTickLength = 5;
    double minorTickLength = 2;
    bool showTickLabels = true;
    /// 刻度標籤旋轉幾度（順時針為正，和 FontStyle::rotation 同一個約定）。
    ///
    /// 類別名稱太長時，水平排會互相重疊，轉 -45 度就擠得下。
    /// 版面會依旋轉後的外接框加大軸的厚度，所以文字不會被切掉。
    ///
    /// 刻度標籤是每次 render 現產生的，沒有一個長期存在的 Label 可以設，
    /// 所以角度要設在這裡而不是 tickFont 上。不過 tickFont.rotation 也會被
    /// 採用（見 effectiveTickRotation），這樣「一次轉掉整張圖的文字」才不會漏掉刻度。
    double tickLabelRotation = 0;

    /// 刻度標籤實際用的角度：tickLabelRotation 優先，為 0 時退回 tickFont.rotation。
    double effectiveTickRotation() const {
        return (tickLabelRotation != 0) ? tickLabelRotation : tickFont.rotation;
    }

    /// 走過這根軸上的每一個字型設定（軸標題與刻度）。
    void visitFonts(const FontVisitor& visit) {
        visit(label.font);
        visit(tickFont);
    }
    bool showFrame = true;
    /// 這根軸是否參與版面（false 時不佔厚度，也不畫）。
    bool isVisible = true;

    // ── 格線 ──
    /// 這根軸畫出來的格線樣式。沒設就沿用 Plot::grid 的。
    ///
    /// 用 optional 而不是「透明色代表沿用」：透明色是個有意義的設定
    /// （這根軸不要畫格線），兩者必須分得開。
    std::optional<LineStyle> gridMajorLine;
    std::optional<LineStyle> gridMinorLine;
    /// 相鄰兩條主格線之間交替填這兩個顏色。無效色代表那一條帶不填。
    ///
    /// 兩根軸都設的話，條帶會交叉疊成格子，所以通常要用半透明色 ——
    /// 不透明的話後畫的那根會把前一根整個蓋掉。
    QColor gridFillColor1;
    QColor gridFillColor2;

    // ── 刻度 ──
    std::shared_ptr<TickGenerator> tickGenerator;
    /// 取得上一次 render/measure 時算出的刻度。
    const QVector<Tick>& ticks() const { return m_ticks; }
    /// 依目前範圍與長度重算刻度。版面計算與繪製都會呼叫。
    void regenerateTicks(double lengthPx);

    // ── 座標換算（單軸）──
    double getPixel(double coord, const PixelRect& dataRect) const;
    double getCoordinate(double pixel, const PixelRect& dataRect) const;

    /// 同一邊有多根軸時，這根要再往外讓開多少像素。
    ///
    /// 由 Plot 在版面計算時填入。沒有它的話，第二根左軸會和第一根畫在同一個位置，
    /// 刻度數字疊成一團。座標換算不受影響 —— 位移只影響「畫在哪」，不影響「對到哪」。
    double renderOffsetPx = 0;

    /// 這根軸在版面上要佔的厚度（含刻度線、刻度標籤、軸標題）。
    double measureThickness() const;

    /// 刻度產生器算出的倍率標示（例如「×10⁶」）。沒有就是空字串。
    QString multiplierLabel() const;
    /// 垂直軸的倍率標示畫在繪圖區上方，版面要替它留出高度，
    /// 否則會被圖的上緣切掉（上邊距通常只有幾個像素）。水平軸回傳 0。
    double multiplierHeadroom() const;

    /// 畫刻度線、刻度標籤、框線與軸標題。dataRect 是繪圖區。
    void render(QPainter& painter, const PixelRect& dataRect) const;

    /// 畫格線（在圖元之下畫，所以和 render 分開）。
    /// 傳進來的是 Plot 的預設樣式；這根軸自己有設 gridMajorLine / gridMinorLine 就蓋過去。
    void renderGridlines(QPainter& painter, const PixelRect& dataRect,
                         const LineStyle& majorStyle, const LineStyle& minorStyle) const;

    /// 畫主格線之間交替的填色條帶（在格線之下）。
    void renderGridFill(QPainter& painter, const PixelRect& dataRect) const;

private:
    Edge m_edge;
    QVector<Tick> m_ticks;
    AxisBreaks m_breaks;
};

}  // namespace qtplot

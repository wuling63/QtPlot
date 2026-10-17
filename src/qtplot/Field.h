// Field.h — 純量場與向量場：等值線（marching squares）與箭頭場。
//
// 格點的位置約定（和 Heatmap 不一樣，很容易踩到）：
//   * Heatmap 的每個值是一個「格子」，extent 是全部格子涵蓋的範圍。
//   * 等值線的每個值是一個「格點」，extent 是第一個格點到最後一個格點的範圍。
// 兩者差了半格。要把等值線疊在熱圖上時用 ContourLines::latticeExtentFor()
// 把熱圖的 extent 內縮半格，線才會落在正確的位置。
#pragma once

#include "Plottable.h"

#include <QDate>

namespace qtplot {

/// 2D 向量。
struct Vector2 {
    double x = 0;
    double y = 0;
    double magnitude() const { return std::hypot(x, y); }
    double angleDegrees() const { return std::atan2(y, x) * 180.0 / kPi; }
};

/// marching squares：從格點純量場抽出某個等值的輪廓。
///
/// values 是 row-major 的 rows × cols 格點值，extent 是第一個格點到最後一個格點
/// 涵蓋的資料範圍。回傳一串折線（已把零散的線段接起來），每條是資料座標的點序列。
/// NaN 的格點會讓周圍的格子被跳過，所以缺資料的地方不會畫出假的輪廓。
QVector<QVector<Coordinates>> marchingSquares(const QVector<double>& values, int rows, int cols,
                                              double level, const CoordinateRect& extent);

/// 等值線。
class ContourLines : public IPlottable {
public:
    QVector<double> values;  ///< row-major 的 rows × cols 格點值
    int rows = 0;
    int cols = 0;
    /// 第一個格點到最後一個格點涵蓋的範圍。未設定時用 (0, cols-1, 0, rows-1)。
    CoordinateRect extent{0, 0, 0, 0};
    /// 第 0 列畫在下方（和 Heatmap 的預設相反，數學上的習慣）。
    bool firstRowAtBottom = true;

    /// 要畫哪些等值。留空則在資料的 min/max 之間自動分 levelCount 層。
    QVector<double> levels;
    int levelCount = 8;

    /// 線的顏色。useColormap 為 true 時改用色階依等值上色。
    LineStyle line{QColor(0, 0, 0, 160), 1.2};
    bool useColormap = false;
    Colormap colormap{Colormap::Viridis};
    /// 色階的對應範圍。未設定時用 levels 的頭尾。
    CoordinateRange colorRange;

    /// 在線上標數值。
    bool showLabels = false;
    FontStyle labelFont;
    /// 標籤要跟著線的方向轉。
    bool rotateLabels = true;
    /// 標籤底下用這個顏色把線蓋掉一小段，文字才不會疊在線上。
    /// 應該設成繪圖區的底色；Plot::addContour() 會自動帶入目前主題的值。
    QColor labelBackground{255, 255, 255};
    /// 一條折線至少要有這麼多點才值得標（太短的標了也看不清）。
    int minPointsForLabel = 12;
    /// 標籤的有效位數。
    ///
    /// 不能沿用刻度標籤那套「依間距決定小數位數」的規則：刻度值是挑過的整齊數字
    /// （1/2/5 × 10^n），等值卻是從資料範圍均分出來的任意實數，那套規則會算出
    /// 十位小數，印出 2.6716084556 這種東西。
    int labelPrecision = 3;

    /// 實際會用到的等值（levels 為空時依資料算出來）。
    QVector<double> effectiveLevels() const;
    CoordinateRect effectiveExtent() const;
    CoordinateRange dataRange() const;

    /// 把 Heatmap 的 extent 換算成格點 extent（內縮半格）。
    /// Heatmap 的值是格子、等值線的值是格點，差半格；不換算等值線會整體偏移。
    static CoordinateRect latticeExtentFor(const CoordinateRect& heatmapExtent, int rows, int cols);

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override {
        visit(labelFont);
    }
};

/// 向量場（箭頭圖）。
class VectorField : public IPlottable {
public:
    /// 箭頭的根部座標。
    QVector<Coordinates> positions;
    /// 對應的向量。長度必須和 positions 相同。
    QVector<Vector2> vectors;

    /// 箭頭長度的縮放。<= 0 代表自動：讓最長的箭頭剛好等於格點間距。
    double scale = 0;
    /// 全部箭頭等長，只用方向（大小改由顏色表示）。
    bool normalize = false;
    /// 箭頭以根部、中心還是尖端對齊 positions。
    enum class Anchor { Base, Center, Tip };
    Anchor anchor = Anchor::Base;

    LineStyle line{QColor(31, 119, 180), 1.4};
    /// 箭頭頭部的長度與寬度（像素）。
    double headLength = 7;
    double headWidth = 5;
    /// 箭頭最長幾個像素（0 代表不限）。少數幾個極大值會把其他箭頭壓成點時用。
    double maxArrowLengthPx = 0;

    /// 依向量大小上色。
    bool useColormap = false;
    Colormap colormap{Colormap::Turbo};
    CoordinateRange colorRange;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    QVector<LegendItem> legendItems() const override;

    /// 在矩形範圍上鋪一組格點，用 f(x, y) 算出每一點的向量。
    static VectorField fromFunction(const std::function<Vector2(double, double)>& f,
                                    const CoordinateRect& area, int nx, int ny);
};

/// 在矩形範圍上取樣 f(x, y)，產生等值線用的格點陣列（row-major，第 0 列對應 bottom）。
QVector<double> sampleField(const std::function<double(double, double)>& f,
                            const CoordinateRect& area, int rows, int cols);

/// 把散布的量測點內插到規則格點上（反距離加權，IDW），之後就能畫等值線或熱圖。
///
/// 等值線演算法要的是格點；氣象站、鑽井這類資料的位置卻是散的。
/// IDW 的每個格點都是附近量測值的加權平均，權重是距離的 -power 次方 ——
/// power 越大越「只信最近的那幾點」，曲面越像一塊塊高原。
/// 注意它不會外插：離所有量測點都遠的地方，值會趨近全體平均。
/// 回傳的排列和 sampleField 相同（row-major，第 0 列對應 area.bottom）。
QVector<double> gridScattered(const QVector<Coordinates>& points, const QVector<double>& values,
                              const CoordinateRect& area, int rows, int cols, double power = 2.0);

// ───────────────────────── 六角分箱 ─────────────────────────

/// 一個六角格。
struct HexCell {
    /// 格心的資料座標。
    double x = 0;
    double y = 0;
    /// 落在這一格裡的點數。
    int count = 0;
};

/// 六角分箱：把大量散點聚合成六角格，用顏色表示密度。
///
/// 散點上萬筆之後，點會互相疊到看不出密度 —— 中間最密的地方和次密的地方
/// 都是一片實心。分箱把「有多少點」變成可以讀的量。
///
/// 為什麼用六角而不是方格：六角格的每個鄰居距離都相同，方格的對角鄰居比較遠，
/// 所以方格會在對角方向上低估密度，畫出來會有隱約的十字紋。
class Hexbin : public IPlottable {
public:
    QVector<double> xs;
    QVector<double> ys;

    /// 橫向要分幾格。縱向依六角的幾何自動推算，格子才會是正六邊形。
    int columns = 30;
    /// 分箱的範圍。無效時取資料的範圍。
    CoordinateRect extent;

    Colormap colormap{Colormap::Viridis};
    /// 顏色對應的計數範圍。無效時取 0 到最大計數。
    CoordinateRange countRange;
    /// 計數少於這個值的格子不畫。1 代表空格不畫。
    int minCount = 1;
    LineStyle border{QColor(0, 0, 0, 0), 1.0};
    /// 格子畫多大，佔格距的比例。調小會留出縫隙。
    double sizeFraction = 1.0;

    /// 算出所有非空的格子。公開以便驗證與自訂繪製。
    QVector<HexCell> bins() const;
    /// 最大的計數。
    int maxCount() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
};


// ───────────────────────── 日曆熱圖 ─────────────────────────

/// 某一天的值。
struct CalendarValue {
    QDate date;
    double value = 0;
};

/// 日曆熱圖：一格一天，一欄一週，用顏色表示當天的值。
///
/// 和一般的時間序列折線圖比，它放棄了「看得出大小」換來「看得出週期」——
/// 週末低、月初高這類規律在折線上要盯著看，在日曆上直接變成直的條紋。
/// 每天只有一格，所以值的差異只分得出粗略幾級，要精確讀數請配折線圖。
class CalendarHeatmap : public IPlottable {
public:
    QVector<CalendarValue> values;

    /// 要畫的日期範圍。無效時用資料的頭尾。
    QDate firstDate;
    QDate lastDate;

    Colormap colormap{Colormap::Blues};
    /// 色階範圍。未設定時用資料的 min/max。
    CoordinateRange colorRange;
    /// 沒有資料的那幾天的顏色。
    QColor missingColor{235, 235, 235};

    /// 整張圖畫在這個資料矩形裡。左上角是第一週的星期一。
    CoordinateRect extent{0, 53, 0, 7};
    /// 格子之間的間隙，佔一格的比例。
    double gapFraction = 0.12;
    double cornerFraction = 0.2;
    LineStyle border{QColor(0, 0, 0, 0), 0.0};

    /// 一週從星期一開始。關掉則從星期日開始（美式日曆）。
    bool weekStartsMonday = true;
    /// 要不要在上方標月份、左邊標星期。
    bool showMonthLabels = true;
    bool showWeekdayLabels = true;
    FontStyle labelFont;
    /// 月份之間要不要畫分隔線。
    bool showMonthSeparators = true;
    LineStyle monthSeparator{QColor(120, 120, 120, 140), 1.0};

    /// 這張圖佔幾週。
    int weekCount() const;
    /// 某一天落在第幾週、星期幾（0 是該週的第一天）。日期超出範圍時回傳 (-1, -1)。
    QPoint cellFor(const QDate& date) const;
    /// 實際用的日期範圍。
    QDate effectiveFirst() const;
    QDate effectiveLast() const;
    /// 實際用的色階範圍。
    CoordinateRange effectiveColorRange() const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;

    /// 這個圖元身上的每一個字型設定。
    void visitFonts(const FontVisitor& visit) override { visit(labelFont); }
};

}  // namespace qtplot

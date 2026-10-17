// Plot.h — 一張圖的模型：軸、圖元、圖例、標題、版面與渲染。
//
// Plot 不是 QWidget，純粹是個可以畫到任何 QPainter 上的模型。
// 互動與事件在 PlotWidget；輸出 PNG／SVG 也走同一條渲染路徑，
// 所以螢幕上看到的和存出來的一定一致。
#pragma once

#include "Bars3D.h"
#include "Chart3D.h"
#include "Business.h"
#include "Sunburst.h"
#include "Flow.h"
#include "Isometric.h"
#include "Multivariate.h"
#include "Plottables.h"
#include "Polar.h"
#include "Overlays.h"
#include "RadialCharts.h"
#include "Annotations.h"
#include "Decorations.h"
#include "Field.h"
#include "Live.h"
#include "Specialty.h"
#include "Finance.h"
#include "Statistics.h"
#include "Analysis.h"
#include "Spectral.h"

#include <functional>
#include <memory>

namespace qtplot {

/// 圖例。
struct Legend {
    bool isVisible = false;
    Qt::Alignment alignment = Qt::AlignRight | Qt::AlignTop;
    bool horizontal = false;
    FontStyle font;
    FillStyle background{QColor(255, 255, 255, 235)};
    LineStyle border{QColor(0, 0, 0, 70), 1.0};
    double padding = 6;        ///< 外框與內容的間距
    double itemSpacing = 4;    ///< 兩列之間的間距
    double symbolWidth = 26;   ///< 左邊符號區的寬度
    double marginX = 10;       ///< 離繪圖區邊緣的距離
    double marginY = 10;
    /// 手動指定項目（留空則自動從圖元收集）。
    QVector<LegendItem> manualItems;

    /// 被隱藏的圖元也要列出來（畫成灰色）。
    ///
    /// 點圖例切換顯示時一定要開著：關掉的話，點一下隱藏之後那一列就消失了，
    /// 使用者再也點不回來。
    bool showHiddenSeries = true;
    /// 灰掉那幾列的透明度。
    int dimmedAlpha = 80;

    /// 圖例標題（畫在項目上方）。空字串不畫。
    Label title;

    /// 分欄。直向排列時是「分幾欄」，橫向排列時是「每列最多幾項」（0 或 1 代表全部擠在同一列）。
    ///
    /// 項目一多，直向圖例會比繪圖區還高、橫向圖例會比繪圖區還寬，分欄才放得下。
    int columns = 1;

    /// 圖例放在哪裡。
    enum class Placement {
        Inside,         ///< 繪圖區裡面（alignment 決定角落）
        OutsideRight,   ///< 繪圖區右側外面，版面會讓出寬度
        OutsideBottom   ///< 繪圖區下方外面（座標軸之下），版面會讓出高度
    };
    /// 放在外面就不會蓋到資料 —— 資料滿到角落、沒有空位可放圖例時用。
    Placement placement = Placement::Inside;

    /// 圖例裡的符號一律畫成這個形狀的標記（不畫線段與色塊）。
    /// 線很細、顏色又接近時，用一個大一點的實心標記比較好認。
    bool overrideMarkerShape = false;
    MarkerShape markerShapeOverride = MarkerShape::FilledSquare;

    /// 圖例身上的字型設定。
    void visitFonts(const FontVisitor& visit) {
        visit(font);
        visit(title.font);
    }
};

/// 第二個（第三個……）圖例。
///
/// 主圖例只有一個，而且列的是全部圖元。兩組資料要分開說明時（例如「顏色代表地區、
/// 形狀代表年份」），就再放一個 LegendPanel。它只畫在繪圖區裡面。
class LegendPanel : public IPlottable {
public:
    /// 外觀與位置（alignment、margin、font、title、columns……）。isVisible 不看。
    Legend style;
    /// 要列的項目。留空的話就從 sources 收集。
    QVector<LegendItem> items;
    /// 只列這幾個圖元的圖例項目。
    QVector<IPlottable*> sources;

    AxisLimits axisLimits() const override { return AxisLimits(); }
    void render(RenderPack& rp) override;
    void visitFonts(const FontVisitor& visit) override { style.visitFonts(visit); }
};

/// 格線樣式。
struct Grid {
    bool isVisible = true;
    LineStyle majorLine{QColor(0, 0, 0, 40), 1.0};
    LineStyle minorLine{QColor(0, 0, 0, 0), 1.0};  ///< 預設不畫次格線
    bool onTop = false;  ///< true 時格線畫在圖元之上
};

/// 背景影像。
struct BackgroundImage {
    QImage image;
    /// 保持長寬比並置中（false 則拉伸填滿）。
    bool keepAspectRatio = false;
    /// 0–1。調低一點當底圖，資料才看得清楚。
    double opacity = 1.0;
    bool smooth = true;
    bool isValid() const { return !image.isNull() && opacity > 0; }
};

/// 整張圖的配色主題。
struct PlotStyle {
    QColor figureBackground{255, 255, 255};
    QColor dataBackground{255, 255, 255};
    QColor axisColor{0, 0, 0};
    QColor gridColor{0, 0, 0, 40};
    QColor textColor{0, 0, 0};
    Palette palette{Palette::Category10};

    /// 整張圖的字型家族。空字串代表不動（沿用每個元素原本的設定）。
    QString fontFamily;
    /// 整張圖的基準字級。<= 0 代表不動。
    ///
    /// 套用時是「等比縮放」而不是「全部設成同一個數字」：標題本來就比刻度大，
    /// 一律設成同一個字級會把這個層次壓掉。基準是 11 point。
    double fontSize = 0;

    static PlotStyle light();
    static PlotStyle dark();
    static PlotStyle hardcopy();  ///< 列印用：細線、無底色
};

/// addSurvivalCurve() 一次建出來的幾個圖元。
///
/// 存活曲線在畫面上是三樣東西疊起來的：階梯線、信賴區間的帶、設限記號。
/// 回傳整組而不是只回傳階梯線，才改得動其中任何一個的樣式。
struct SurvivalPlottables {
    Scatter* curve = nullptr;
    FillY* band = nullptr;          ///< showBand 為 false 時是 nullptr
    Markers* censorMarks = nullptr; ///< 沒有設限資料時是 nullptr
};

/// 一張圖。
class Plot {
public:
    Plot();
    ~Plot();
    Plot(const Plot&) = delete;
    Plot& operator=(const Plot&) = delete;

    // ───────────── 軸 ─────────────
    Axis* bottomAxis() { return m_bottom; }
    Axis* leftAxis() { return m_left; }
    Axis* topAxis() { return m_top; }
    Axis* rightAxis() { return m_right; }

    /// 多軸：新增一根軸，回傳的指標由 Plot 擁有。
    ///
    /// 注意這根軸會活到 reset() 或 Plot 被解構為止。clear() 只清圖元，
    /// 不會把它收回去 —— 想換一張全新的圖請用 reset()。
    Axis* addAxis(Edge edge);
    const QVector<Axis*>& axes() const { return m_axes; }

    /// 設定主要兩軸的範圍。
    void setAxisLimits(double xMin, double xMax, double yMin, double yMax);
    void setAxisLimits(const AxisLimits& lim);
    AxisLimits axisLimits() const;
    /// 使用者是否呼叫過 setAxisLimits。Cookbook 用它決定要不要自動範圍 ——
    /// 無條件 autoScale 會把範例刻意設的視野蓋掉。
    bool hasExplicitAxisLimits() const;

    /// 掃過所有圖元取得資料範圍（不改變目前視野）。
    /// 注意這會把綁在不同軸上的圖元混在一起算 —— 多軸圖請改用逐軸的版本。
    AxisLimits dataLimits() const;
    /// 只看綁在這根軸上的圖元。autoScale 用的就是這個。
    CoordinateRange dataLimits(const Axis* axis) const;
    /// 把視野設成資料範圍，兩端各留 fraction 的邊。
    /// 預設會跳過 locked 的軸，以及用 setAxisLimits()／Axis::setRange() 指定過的軸。
    /// force = true 連指定過的軸也一起重算（使用者主動按「自動範圍」時用），
    /// 但 locked 的軸任何情況都不動。
    void autoScale(double fraction = 0.05, bool force = false);
    /// 下次 render 時自動套用 autoScale（加完資料不必自己呼叫）。
    void queueAutoScale() { m_autoScalePending = true; }

    /// 讓兩軸的「每資料單位對應像素數」相同（圓不會變橢圓）。
    bool equalAxisScale = false;

    // ───────────── 圖元 ─────────────
    /// 接管所有權，回傳同一個指標方便繼續設樣式。
    template <typename T>
    T* add(std::unique_ptr<T> plottable) {
        T* raw = plottable.get();
        attach(raw);
        m_plottables.push_back(std::move(plottable));
        return raw;
    }
    /// 就地建構一個圖元。
    template <typename T, typename... Args>
    T* emplace(Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = p.get();
        attach(raw);
        m_plottables.push_back(std::move(p));
        return raw;
    }

    void remove(IPlottable* plottable);
    void clear();
    /// 移到最上層（最後畫）。
    void moveToFront(IPlottable* plottable);
    /// 移到最下層（最先畫）。
    void moveToBack(IPlottable* plottable);

    /// 把整張圖倒回「剛 new 出來」的狀態：圖元、額外的軸、標題、圖例、格線、
    /// 樣式、背景影像、配色計數全部清掉，只留下四根預設軸。
    ///
    /// clear() 不夠用的場合就是這個：clear() 只清圖元，addAxis() 加上去的軸
    /// 會留到下一張圖 —— 連續換圖時刻度會一層層疊上去。要在同一個 Plot 上
    /// 反覆畫不同的東西（Cookbook 切換範例、重畫報表），每次都要先 reset()。
    ///
    /// 注意：reset() 之後先前 addAxis()、add*() 拿到的指標全部失效。
    void reset();
    QVector<IPlottable*> plottables() const;
    int plottableCount() const { return static_cast<int>(m_plottables.size()); }

    // ───────────── 常用圖元的捷徑 ─────────────
    Scatter* addScatter(QVector<double> xs, QVector<double> ys, QColor color = QColor());
    Scatter* addLine(double x1, double y1, double x2, double y2, QColor color = QColor());
    Markers* addMarkers(QVector<double> xs, QVector<double> ys, QColor color = QColor());
    Signal* addSignal(QVector<double> ys, double period = 1.0, QColor color = QColor());
    SignalXY* addSignalXY(QVector<double> xs, QVector<double> ys, QColor color = QColor());
    FunctionPlot* addFunction(std::function<double(double)> f, QColor color = QColor());
    /// 堆疊面積圖。正值往上疊、負值往下疊，每一層回傳一個 FillY。
    ///
    /// 層的繪製順序是輸入的順序，所以後面的層畫在前面的層之上。
    QVector<FillY*> addStackedArea(const QVector<double>& xs,
                                   const QVector<QVector<double>>& series,
                                   const QVector<QColor>& colors = {});
    /// 地平線圖：把一條序列折疊成幾層，用顏色深淺代替高度。
    Horizon* addHorizon(QVector<double> xs, QVector<double> ys, int bands = 3);
    /// 樹狀聚類圖。
    Dendrogram* addDendrogram(QVector<ClusterMerge> merges, int leafCount,
                              QStringList leafLabels = {});
    /// 河流圖（streamgraph）。基準線由 offset 決定。
    QVector<FillY*> addStream(const QVector<double>& xs,
                              const QVector<QVector<double>>& series,
                              StackOffset offset = StackOffset::Wiggle,
                              const QVector<QColor>& colors = {});
    FillY* addFillY(QVector<double> xs, QVector<double> ys1, QVector<double> ys2,
                    QColor color = QColor());
    BarPlot* addBars(QVector<double> values, QColor color = QColor());
    BarPlot* addBars(QVector<Bar> bars);
    BarPlot* addHistogram(const QVector<double>& values, int binCount = 0, QColor color = QColor());
    ErrorBar* addErrorBar(QVector<double> xs, QVector<double> ys, QVector<double> yErr);
    Heatmap* addHeatmap(QVector<double> values, int rows, int cols);
    AxisLine* addHorizontalLine(double y, QColor color = QColor());
    /// 一段直線，沿著線寫字（文字角度依螢幕上的斜率自動算）。
    LabeledLine* addLabeledLine(Coordinates start, Coordinates end, const QString& text, QColor color = QColor());
    /// 每一格各自指定顏色的格子。colors 是 row-major，第 0 列在 extent 的最上面。
    ColorGrid* addColorGrid(int rows, int cols, QVector<QColor> colors, CoordinateRect extent);
    AxisLine* addVerticalLine(double x, QColor color = QColor());
    AxisSpan* addHorizontalSpan(double y1, double y2, QColor color = QColor());
    AxisSpan* addVerticalSpan(double x1, double x2, QColor color = QColor());
    Crosshair* addCrosshair(double x, double y);
    Text* addText(const QString& text, double x, double y);
    Annotation* addAnnotation(const QString& text,
                             Qt::Alignment corner = Qt::AlignLeft | Qt::AlignTop);
    Arrow* addArrow(double tipX, double tipY, double baseX, double baseY, QColor color = QColor());
    Shape* addRectangle(double left, double right, double bottom, double top);
    Shape* addEllipse(double centerX, double centerY, double radiusX, double radiusY);
    Shape* addPolygon(QVector<Coordinates> points);
    /// 立體（擠出）長條圖。給簡報與資訊圖表用 —— 要精確比較數值請用 addBars。
    Bars3D* addBars3D(const QVector<double>& values);
    /// 漏斗圖。inverted = true 且 widthMode = Linear 就是金字塔圖。
    Funnel* addFunnel(const QVector<double>& values, const QStringList& labels = {});
    /// 瀑布圖。
    Waterfall* addWaterfall(QVector<WaterfallStep> steps);
    /// 靶心圖。
    Bullet* addBullet(QVector<BulletRow> rows);
    /// 旭日圖（階層式環圈）。
    Sunburst* addSunburst(QVector<SunburstNode> roots);
    /// 樹狀圖（階層式矩形）。
    Treemap* addTreemap(QVector<TreemapNode> roots);
    /// 六角分箱：大量散點聚合成六角格，用顏色表示密度。
    Hexbin* addHexbin(QVector<double> xs, QVector<double> ys, int columns = 30);
    /// 蜂群圖：一維散點，點互相讓開不重疊。
    Beeswarm* addBeeswarm(const QVector<QVector<double>>& groups, QStringList labels = {});
    /// 啞鈴圖：每一列兩個點加一條連線。
    Dumbbell* addDumbbell(QVector<DumbbellRow> rows);
    /// 平行座標圖。每個維度一根軸，一筆資料一條折線。
    ParallelCoordinates* addParallelCoordinates(const QStringList& axisLabels,
                                                QVector<QVector<double>> rows);
    /// 氣泡圖：散點再加上第三個維度（泡泡大小）。
    BubbleChart* addBubbles(const QVector<double>& xs, const QVector<double>& ys,
                            const QVector<double>& sizes);
    /// 甘特圖。
    Gantt* addGantt(QVector<GanttTask> tasks);
    /// 文氏圖（2 或 3 個集合）。
    Venn* addVenn(const QStringList& labels);
    /// 魚骨圖（石川圖）。會順便把座標軸與格線關掉 —— 這是結構圖，沒有刻度可讀。
    Fishbone* addFishbone(const QString& effect, QVector<FishboneCategory> categories);
    /// 柏拉圖。會建一根 0–100% 的右軸給累積折線用，並把類別名放到底軸刻度上。
    Pareto* addPareto(QVector<ParetoItem> items);
    /// 鬆餅圖。會順便關掉座標軸與格線並設成等比例。
    Waffle* addWaffle(QVector<WaffleCategory> categories, int rows = 10, int columns = 10);
    /// 斜率圖（兩期）或排名變化圖（多期）。會把期別名放到底軸刻度上。
    Slope* addSlope(QStringList periodLabels, QVector<SlopeSeries> series);
    /// 立體曲面（等角投影的 2.5D，不是真 3D）。會關掉座標軸與格線並設成等比例。
    Surface3D* addSurface3D(QVector<double> values, int rows, int cols);
    /// 真 3D 圖表（可任意旋轉的座標盒）。資料用 chart->add(...) 加進去。
    /// 會關掉 Plot 本身的座標軸與格線 —— 座標盒自己畫刻度。
    Chart3D* addChart3D();
    /// 環狀長條圖（第一個數值在最外環）。會關掉座標軸與格線並設成等比例。
    RadialBarChart* addRadialBarChart(QVector<double> values, QStringList labels);
    /// 環狀熱圖。cells 是 row-major 的 [環][扇區]，第 0 道環在最外面。
    /// 會關掉座標軸與格線並設成等比例。
    CircularHeatmap* addCircularHeatmap(int rings, int sectors, QVector<QColor> cells,
                                        QStringList sectorLabels = {});
    /// 山脊圖。會把組名放到左軸刻度上。
    Ridgeline* addRidgeline(QVector<RidgeGroup> groups);
    /// 日曆熱圖。會順便關掉座標軸與格線並設成等比例。
    CalendarHeatmap* addCalendarHeatmap(QVector<CalendarValue> values);
    /// 桑基圖。節點的 layer 要自己指定。
    Sankey* addSankey(QVector<SankeyNode> nodes, QVector<SankeyLink> links);
    /// 弦圖。matrix 必須是 n×n 的方陣。
    Chord* addChord(const QStringList& labels, QVector<QVector<double>> matrix);
    /// 網路圖。
    NetworkGraph* addNetwork(QVector<GraphNode> nodes, QVector<GraphEdge> edges);
    /// 馬賽克圖（Marimekko）。
    Mosaic* addMosaic(QVector<MosaicColumn> columns);
    /// 等角（2.5D）長條圖。給簡報版型用 —— 要精確比較數值請用 addBars。
    IsoBars* addIsoBars(const QVector<double>& values);
    /// 等角堆疊體（金字塔／圓柱／圓錐）。
    IsoStack* addIsoStack(QVector<IsoLayer> layers);
    /// 扇形。radiusY <= 0 代表正圓（沿用 radiusX）。
    /// 設 innerRadiusX / innerRadiusY 就變成環狀扇形。
    Sector* addSector(double centerX, double centerY, double radiusX, double startAngle,
                      double sweepAngle, double radiusY = 0);
    /// 圓弧／橢圓弧（開放曲線，不填色）。radiusY <= 0 代表正圓。
    Arc* addArc(double centerX, double centerY, double radiusX, double startAngle,
                double sweepAngle, double radiusY = 0);

    // 極座標族。這些圖的兩軸應該等比例，否則圓會變橢圓 ——
    // 下面幾個捷徑都會自動把 equalAxisScale 打開，並隱藏直角座標軸與格線。
    PolarAxis* addPolarAxis(double radius = 1.0, int spokeCount = 8);
    Pie* addPie(QVector<double> values, QStringList labels = {});
    Pie* addDonut(QVector<double> values, QStringList labels = {}, double holeFraction = 0.5);
    Coxcomb* addCoxcomb(QVector<double> values, QStringList labels = {});
    /// 逐格指定顏色、標籤的版本。堆疊玫瑰圖要用這個再設 stacks。
    Coxcomb* addCoxcomb(QVector<PieSlice> slices);
    Radar* addRadar(QVector<QVector<double>> series, QStringList axisLabels = {});
    RadialGauge* addRadialGauge(QVector<double> values, QStringList labels = {},
                                double maximum = 100);

    // 標註
    Callout* addCallout(const QString& text, double targetX, double targetY,
                        double textX, double textY);
    /// 氣泡標註。tip 是尾巴指的點，label 是方框的中心。
    Tooltip* addTooltip(const QString& text, double tipX, double tipY,
                        double labelX, double labelY);
    Bracket* addBracket(const QString& text, double x1, double y1, double x2, double y2);
    ScaleBar* addScaleBar(double width, double height = 0);
    ImageRect* addImage(const QImage& image, double left, double right, double bottom, double top);

    // 特殊座標
    TIN* addTIN(QVector<Coordinates> points, QVector<double> zs);
    TriangularAxis* addTriangularAxis(QStringList cornerLabels = {});
    SmithChartAxis* addSmithChart();
    PhasorPlot* addPhasors(QVector<Phasor> phasors);

    // 裝飾
    /// 色階圖例。沿用某個熱圖／等值線／TIN 的色階與範圍。
    Colorbar* addColorbar(const Heatmap& source, Edge edge = Edge::Right);
    Colorbar* addColorbar(const ContourLines& source, Edge edge = Edge::Right);
    Colorbar* addColorbar(const TIN& source, Edge edge = Edge::Right);
    Colorbar* addColorbar(const Hexbin& source, Edge edge = Edge::Right);
    /// 3D 圖表的色階（依高度上色的散點、曲面、立體柱）。
    Colorbar* addColorbar(const Chart3D& source, Edge edge = Edge::Right);
    Colorbar* addColorbar(Colormap map, CoordinateRange range, Edge edge = Edge::Right);
    ImageMarker* addImageMarkers(QVector<double> xs, QVector<double> ys, const QImage& image);
    /// 畫在繪圖區內部的浮動軸（例如把 X 軸畫在 y = 0 上）。
    FloatingAxis* addFloatingAxis(bool horizontal, double position);
    /// 另一個圖例，只列 sources 這幾個圖元。
    LegendPanel* addLegendPanel(QVector<IPlottable*> sources,
                                Qt::Alignment corner = Qt::AlignLeft | Qt::AlignTop);

    // 即時資料
    DataLogger* addDataLogger(QColor color = QColor());
    DataStreamer* addDataStreamer(int capacity = 1000, QColor color = QColor());
    DataStreamerXY* addDataStreamerXY(int capacity = 1000, QColor color = QColor());

    // 場
    /// 等值線。levelCount <= 0 代表用 ContourLines 的預設層數。
    ContourLines* addContour(QVector<double> values, int rows, int cols, int levelCount = 8);
    /// 由 f(x, y) 取樣後畫等值線。
    ContourLines* addContour(const std::function<double(double, double)>& f,
                             const CoordinateRect& area, int rows = 100, int cols = 100,
                             int levelCount = 8);
    /// 趨勢線：通過兩點、畫滿整個繪圖區，兩個點都可以拖。
    TrendLine* addTrendLine(Coordinates p1, Coordinates p2, QColor color = QColor());
    /// 散布的量測點畫等值線：先用 gridScattered 內插到 gridSize × gridSize 的格點。
    ContourLines* addContourScattered(const QVector<Coordinates>& points, const QVector<double>& values,
                                      int levelCount = 8, int gridSize = 60);
    VectorField* addVectorField(QVector<Coordinates> positions, QVector<Vector2> vectors);
    /// 由 f(x, y) 在格點上取樣出向量場。
    VectorField* addVectorField(const std::function<Vector2(double, double)>& f,
                                const CoordinateRect& area, int nx = 15, int ny = 15);

    // 財金圖。預設用序號當 X 並把底軸換成 IndexedDateTickGenerator，
    // 週末與假日才不會在 K 線之間留下空白。
    Candlestick* addCandlestick(QVector<Ohlc> bars);
    Candlestick* addOhlc(QVector<Ohlc> bars);
    Volume* addVolume(QVector<Ohlc> bars);

    // 統計圖
    BoxPlot* addBoxPlot(const QVector<QVector<double>>& groups, QStringList labels = {});
    /// 逐箱指定位置、寬度與顏色的版本。分組箱型圖（同一類別並排兩個箱子）要用這個。
    BoxPlot* addBoxPlot(QVector<Box> boxes);
    Lollipop* addLollipop(QVector<double> values, QColor color = QColor());
    Population* addPopulation(const QVector<QVector<double>>& groups, QStringList labels = {});
    /// 小提琴圖：只是打開密度曲線並左右對稱的 Population。
    Population* addViolin(const QVector<QVector<double>>& groups, QStringList labels = {});
    /// 線性迴歸線。會畫滿整個 X 軸可視範圍，並回傳擬合結果。
    Scatter* addRegressionLine(const QVector<double>& xs, const QVector<double>& ys,
                               LinearFit* fitOut = nullptr, QColor color = QColor());

    // 診斷與推論圖
    /// 管制圖（SPC）。rules 留空只跑規則 1（單點超出 3σ）。
    ControlChart* addControlChart(QVector<double> values,
                                  QVector<ControlRule> rules = {});
    /// 森林圖。順便把左軸的刻度換成每一列的名稱。
    ForestPlot* addForestPlot(QVector<ForestRow> rows);
    /// 常態 Q-Q 圖：樣本點加一條穿過四分位數的參考線。
    /// 回傳那些點，參考線可以用 qqLineOut 取回。
    Markers* addNormalQQ(const QVector<double>& values, Scatter** qqLineOut = nullptr,
                         QColor color = QColor());
    /// 分類器曲線（ROC 或 PR）。會把曲線本身與可選的填色一起加進去。
    Scatter* addClassifierCurve(const ClassifierCurve& curve, QColor color = QColor(),
                                bool fillBelow = false);
    /// Kaplan–Meier 存活曲線加信賴區間與設限記號。
    SurvivalPlottables addSurvivalCurve(const SurvivalCurve& km, QColor color = QColor(),
                                        bool showBand = true, bool showCensorMarks = true);
    /// 頻譜圖。X 軸是秒、Y 軸是 Hz，並把 flipVertically 設好讓低頻在下面。
    Heatmap* addSpectrogram(const Spectrogram& sp, Colormap map = Colormap(Colormap::Viridis));

    /// 把這張圖切成「極座標模式」：等比例、不畫直角軸與格線。
    /// 上面幾個 add* 捷徑會自動呼叫，手動建圖元時要自己叫。
    void usePolarLayout();

    // ───────────── 文字 ─────────────

    /// 上一次 render 時，圖例每一列佔的像素矩形與對應的圖元。
    ///
    /// 給「點圖例切換顯示」用。圖例的位置是算出來的，不是設定的，
    /// 所以只有畫過一次之後才知道哪一列在哪裡。
    struct LegendHit {
        QRectF rect;
        IPlottable* plottable = nullptr;
    };
    const QVector<LegendHit>& legendHitAreas() const { return m_legendHits; }
    /// 這個像素點落在圖例的哪一列上。沒有就回傳 nullptr。
    IPlottable* legendPlottableAt(QPointF pixel) const;

    /// 走過整張圖的每一個字型設定：標題、每根軸的軸名與刻度、圖例，
    /// 以及每個圖元身上的每一處文字。
    ///
    /// 下面那幾個 setFont* 都是它的包裝。要做更細的事（例如「只把粗體的那些放大」）
    /// 就直接用這個：
    /// ```
    /// plot.visitFonts([](FontStyle& f) { if (f.bold) f.size *= 1.5; });
    /// ```
    /// 圖元是在加進來之後才被走到的，所以順序有關係 —— 先設字型再加圖元的話，
    /// 後加的那些不會被改到。
    void visitFonts(const FontVisitor& visit);

    /// 換掉整張圖的字型家族。空字串恢復系統預設。
    void setFontFamily(const QString& family);
    /// 每個字級乘上同一個倍率（維持標題比刻度大的層次）。
    void scaleFontSizes(double factor);
    /// 把整張圖的字級設成同一個值。會把原本的層次壓平，通常 scaleFontSizes 比較合用。
    void setFontSize(double points);
    /// 字重。FontStyle::Normal、FontStyle::Bold 等等。
    void setFontWeight(int weight);
    /// 整張圖的文字顏色。會蓋掉原本刻意調淡的那幾處（次要說明、單位）。
    void setFontColor(QColor color);
    /// 整張圖的文字角度。刻度標籤也會跟著轉（版面會重新留空間）。
    void setFontRotation(double degrees);

    // ───────────── 外觀 ─────────────
    Label title;
    Legend legend;
    Grid grid;
    PlotStyle style;
    /// 繪圖區四周的最小邊界（像素）。軸需要更多空間時會自動加大。
    double paddingLeft = 8, paddingRight = 12, paddingTop = 8, paddingBottom = 8;
    /// 抗鋸齒。關掉在超大資料量時比較快。
    bool antiAlias = true;
    /// 標題置中在整張圖上（預設置中在繪圖區上 —— 左軸很寬時兩者差很多）。
    bool titleCenteredOnFigure = false;
    /// 髮絲線模式：座標軸框線、刻度線與格線不管輸出放大幾倍都是 1 個裝置像素。
    ///
    /// 高解析度輸出（savePng 的 scale = 4）時，一般的線會跟著變成 4 像素粗；
    /// 印刷用的圖通常要的是細線，資料線粗一點、格線維持最細。
    bool hairline = false;
    /// 整張圖外圍的框線。預設不畫。
    LineStyle figureBorder = LineStyle::none();
    /// 固定繪圖區的位置與大小（相對於畫布左上角的像素）。
    ///
    /// 無效（空的）時依座標軸自動排版。好幾張圖要上下對齊、或要和別的東西疊在一起時，
    /// 自動排版會因為刻度標籤寬度不同而各自錯開一點，固定下來就不會。
    QRectF fixedDataRect;

    /// 整張圖的背景影像（鋪滿整個畫布，含座標軸區域）。
    BackgroundImage figureBackgroundImage;
    /// 只鋪在繪圖區裡的背景影像（不含座標軸）。
    ///
    /// 和 ImageRect 的差別：ImageRect 釘在「資料座標」上，會跟著平移縮放；
    /// 這個釘在「繪圖區」上，縮放時不動。地圖底圖要用 ImageRect，
    /// 裝飾性的底紋用這個。
    BackgroundImage dataBackgroundImage;

    /// 取下一個自動配色。每次呼叫都前進一格。
    QColor nextColor();
    /// 重設自動配色的計數。
    void resetColorCycle() { m_colorIndex = 0; }

    void setStyle(const PlotStyle& s);

    /// 把 add*() 自動填的文字換成目前的語言（預設軸名、魚骨圖的 6M）。
    ///
    /// render() 發現語言換過（qtplot::languageGeneration() 變了）就會自己呼叫，
    /// 一般不必手動叫。使用者改過的文字不會被蓋掉 —— 只有「還是當初自動填的那個字」才重翻。
    void retranslate();

    // ───────────── 渲染 ─────────────
    /// 畫到指定的像素矩形上。
    void render(QPainter& painter, const QRectF& target);
    /// 畫成圖片。scale > 1 可輸出高解析度版本。
    QImage renderToImage(int width, int height, double scale = 1.0);
    bool savePng(const QString& path, int width = 800, int height = 600, double scale = 1.0);
    bool saveSvg(const QString& path, int width = 800, int height = 600);
    /// 向量 PDF。width / height 的單位是點（1/72 吋），和上面那幾個函式的數字意義一致。
    ///
    /// 要印出來或放進排版軟體就用這個，不要用放大倍率的 PNG ——
    /// 點陣圖放大到印刷解析度，字和細線一定會糊。
    bool savePdf(const QString& path, int width = 800, int height = 600);

    /// 只算版面不畫圖，回傳繪圖區的像素矩形。
    ///
    /// Multiplot 用它對齊各子圖的邊界：得先知道每張圖的刻度標籤要讓出多少，
    /// 才補得出一致的留白。注意它會重算刻度，所以不是個「純查詢」。
    PixelRect measureDataRect(const QRectF& target);

    /// 在 dataRect 上畫一個圖例框。主圖例與 LegendPanel 共用這一份。
    void drawLegendBox(QPainter& painter, const Legend& lg, const QVector<LegendItem>& items,
                       const PixelRect& dataRect, double outsideOffset,
                       QVector<LegendHit>* hits) const;

    // ───────────── 互動 ─────────────

    /// 找出離指定資料座標最近的資料點（掃過所有可命中的圖元）。
    /// maxDistancePx <= 0 代表不限距離。回傳 isHit() == false 表示沒命中。
    HitResult hitTest(Coordinates target, double maxDistancePx = 0,
                      HitMode mode = HitMode::NearestXY) const;
    /// 找出落在某個資料矩形內的所有點（框選用）。
    /// 回傳的每一筆都帶著是哪個圖元、第幾筆。
    QVector<HitResult> pointsInRect(const CoordinateRect& rect) const;

    /// 找出游標抓住的可拖圖元。沒有就回傳 nullptr。
    /// 後加入的圖元畫在上面，所以反向找 —— 視覺上在最上層的要優先抓到。
    IPlottable* grabbedPlottable(Coordinates at, double tolerancePx = 6) const;
    /// 建立對應某根軸對的座標換算（用上一次 render 的繪圖區）。
    Transform transformFor(const Axis* xAxis, const Axis* yAxis) const;

    /// 上一次 render 算出的繪圖區，互動要用它做座標換算。
    PixelRect lastDataRect() const { return m_lastDataRect; }
    /// 上一次 render 的耗時（毫秒），給效能顯示用。
    double lastRenderMs() const { return m_lastRenderMs; }

private:
    /// add*() 自動填、語言切換時要重翻的一段文字。
    ///
    /// target 每次用時才取指標：魚骨圖的類別是放在 QVector 裡的，存原始指標的話
    /// 使用者改了類別陣列之後就是懸空指標。owner 用來在圖元被移除時一併清掉。
    struct AutoText {
        const void* owner = nullptr;
        std::function<QString*()> target;
        const char* source = nullptr;
        const char* comment = nullptr;
        QString applied;  ///< 上次填進去的字；目前的字不等於它，就是使用者改過了
    };
    /// 填上翻譯過的文字，並記下來給 retranslate() 用。
    void setAutoText(const void* owner, std::function<QString*()> target, const char* source,
                     const char* comment = nullptr);
    void forgetAutoTexts(const void* owner);
    QVector<AutoText> m_autoTexts;
    int m_languageGeneration = 0;

    /// 把所有成員設成「剛建構好」的狀態，並建立四根預設軸。
    /// 建構子與 reset() 共用，兩條路徑才不會漸漸走岔。
    void initDefaults();

    void attach(IPlottable* p);
    PixelRect computeDataRect(const PixelRect& figure);
    void renderLegend(QPainter& painter, const PixelRect& dataRect);
    /// 圖例分欄後的版面：每欄寬、列高、標題高與外框大小。
    struct LegendGrid {
        int columns = 1;
        int rows = 0;
        QVector<double> columnWidths;
        double rowHeight = 0;
        double titleHeight = 0;
        double width = 0;
        double height = 0;
    };
    LegendGrid measureLegend(const Legend& lg, const QVector<LegendItem>& items) const;
    /// 圖例放在外面時，同一邊在它之前已經被座標軸與色階圖例佔掉多少像素。
    double m_legendOutsideOffset = 0;
    QVector<LegendItem> collectLegendItems() const;

    Axis* m_bottom = nullptr;
    Axis* m_left = nullptr;
    Axis* m_top = nullptr;
    Axis* m_right = nullptr;
    QVector<Axis*> m_axes;
    std::vector<std::unique_ptr<Axis>> m_axisOwner;
    std::vector<std::unique_ptr<IPlottable>> m_plottables;
    int m_colorIndex = 0;
    bool m_autoScalePending = false;
    bool m_explicitLimits = false;
    PixelRect m_lastDataRect;
    QVector<LegendHit> m_legendHits;
    double m_lastRenderMs = 0;
};

}  // namespace qtplot

// AxisCustom.cpp — 座標軸的各種擺法與完整的外觀調整。
//
// 這一組回答「我要改某個東西，該動哪個欄位」：四邊刻度、同側多軸、
// 刻度的數量與樣式、範圍的設定方式、標題與字型、框線與背景。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

// ───────────────────── 刻度畫在哪幾邊 ─────────────────────

QTPLOT_RECIPE("座標軸", "刻度畫在哪幾邊", "左右兩側都有刻度",
              "右軸預設只當框線（showTickLabels = false）。把它打開就會鏡像左軸的刻度 —— "
              "兩邊是同一個範圍，寬的圖在右側也讀得到數值，不必把視線拉回左邊。"
              "autoScale 會自動讓右軸跟著左軸，不必自己同步。") {
    plot.addSignal(gen::sin(200, 1.0, 3.0));

    auto* right = plot.rightAxis();
    right->showTickLabels = true;
    right->majorTickLength = 5;
    right->minorTickLength = 2;

    plot.leftAxis()->label.text = RT("振幅");
    plot.title.text = RT("左右鏡像刻度");
}

QTPLOT_RECIPE("座標軸", "刻度畫在哪幾邊", "上下兩側都有刻度",
              "同樣的做法套在上軸。四邊都開就是科學期刊常見的「完整框線加刻度」樣式。") {
    plot.addScatter(gen::consecutive(60), gen::randomWalk(60));

    for (Axis* a : {plot.topAxis(), plot.rightAxis()}) {
        a->showTickLabels = true;
        a->majorTickLength = 5;
        a->minorTickLength = 2;
    }
    plot.bottomAxis()->label.text = QStringLiteral("X");
    plot.leftAxis()->label.text = QStringLiteral("Y");
    plot.title.text = RT("四邊都有刻度");
}

QTPLOT_RECIPE("座標軸", "刻度畫在哪幾邊", "刻度朝內",
              "majorTickLength 設負值刻度就往繪圖區內部長。"
              "資料靠近邊緣時朝內的刻度會壓到資料，但版面比較緊湊。") {
    plot.addSignal(gen::sin(200, 1.0, 2.0));
    for (Axis* a : plot.axes()) {
        a->majorTickLength = -5;
        a->minorTickLength = -2;
    }
    plot.topAxis()->showTickLabels = false;
    plot.rightAxis()->showTickLabels = false;
    plot.grid.isVisible = false;
}

QTPLOT_RECIPE("座標軸", "刻度畫在哪幾邊", "同一側兩根軸",
              "同一邊可以疊多根軸。第一根貼著繪圖區，第二根以後自動往外讓開 —— "
              "沒有這個位移的話兩根會畫在同一個位置，刻度數字糊成一團。"
              "常見用途是同一筆資料配兩種單位（攝氏／華氏、公尺／英尺）。") {
    const QVector<double> xs = gen::consecutive(80);
    const QVector<double> celsius = gen::randomWalk(80, 1.5, 22);

    auto* s = plot.addScatter(xs, celsius);
    s->marker = MarkerStyle::none();

    plot.leftAxis()->label.text = RT("攝氏 (°C)");

    // 第二根左軸：同一筆資料，只是換個單位刻度
    auto* fahrenheit = plot.addAxis(Edge::Left);
    fahrenheit->showTickLabels = true;
    fahrenheit->majorTickLength = 5;
    fahrenheit->label.text = RT("華氏 (°F)");
    fahrenheit->label.font.color = QColor(200, 80, 0);
    fahrenheit->tickFont.color = QColor(200, 80, 0);
    fahrenheit->majorTickLine.color = QColor(200, 80, 0);

    // 攝氏範圍換算成華氏，兩根軸就會對齊同一筆資料
    plot.autoScale();
    const CoordinateRange c = plot.leftAxis()->range;
    fahrenheit->setRange(c.min * 9 / 5 + 32, c.max * 9 / 5 + 32);
}

QTPLOT_RECIPE("座標軸", "刻度畫在哪幾邊", "同一側兩根軸（不同資料）",
              "兩根右軸各自綁一組資料。數量級差很多的量放在一起時，"
              "這樣比硬擠進同一個範圍清楚。autoScale 是逐軸算的，各軸會貼合自己的資料。") {
    const QVector<double> xs = gen::consecutive(100);

    auto* base = plot.addScatter(xs, gen::sin(100, 1.0, 2.0));
    base->marker = MarkerStyle::none();
    base->legendText = RT("訊號（左軸）");
    plot.leftAxis()->label.text = RT("訊號");

    auto* axis1 = plot.addAxis(Edge::Right);
    axis1->showTickLabels = true;
    axis1->majorTickLength = 5;
    axis1->label.text = RT("壓力 (kPa)");
    auto* p1 = plot.addScatter(xs, gen::randomWalk(100, 8, 300));
    p1->yAxis = axis1;
    p1->marker = MarkerStyle::none();
    p1->legendText = RT("壓力（右軸 1）");

    auto* axis2 = plot.addAxis(Edge::Right);
    axis2->showTickLabels = true;
    axis2->majorTickLength = 5;
    axis2->label.text = RT("流量 (L/min)");
    auto* p2 = plot.addScatter(xs, gen::randomWalk(100, 0.4, 12));
    p2->yAxis = axis2;
    p2->marker = MarkerStyle::none();
    p2->legendText = RT("流量（右軸 2）");

    // 顏色對應到各自的軸，才看得出哪條線配哪根刻度
    axis1->tickFont.color = p1->line.color;
    axis1->label.font.color = p1->line.color;
    axis1->majorTickLine.color = p1->line.color;
    axis2->tickFont.color = p2->line.color;
    axis2->label.font.color = p2->line.color;
    axis2->majorTickLine.color = p2->line.color;

    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

// ───────────────────── 刻度本身 ─────────────────────

QTPLOT_RECIPE("座標軸", "調整刻度", "刻度的疏密與長度",
              "AutomaticTickGenerator 依標籤寬度決定疏密，但可以調整："
              "minorTicksPerMajor 改次刻度數、labelPaddingPx 改最小間隔（調大就變疏）、"
              "maxTicks 是保險上限。刻度線的長度與顏色在 Axis 上。") {
    plot.addSignal(gen::sin(300, 1.0, 4.0));

    auto bottom = std::make_shared<AutomaticTickGenerator>();
    bottom->minorTicksPerMajor = 10;   // 每個主刻度之間切十等分
    bottom->labelPaddingPx = 30;       // 標籤之間至少留 30px → 主刻度變疏
    plot.bottomAxis()->tickGenerator = bottom;

    plot.bottomAxis()->majorTickLength = 9;
    plot.bottomAxis()->minorTickLength = 4;
    plot.bottomAxis()->majorTickLine = LineStyle(QColor(0, 0, 0), 1.5);
    plot.bottomAxis()->minorTickLine = LineStyle(QColor(0, 0, 0, 110), 1.0);

    plot.grid.minorLine = LineStyle(QColor(0, 0, 0, 18), 1.0);
    plot.addAnnotation(RT("底軸：主刻度變疏、次刻度十等分"),
                       Qt::AlignLeft | Qt::AlignTop);
}

QTPLOT_RECIPE("座標軸", "調整刻度", "刻度文字的格式",
              "labelFormatter 可以完全接管標籤文字：加單位、改成百分比、"
              "換成中文數字都行。設了它之後共同倍率不會套用（避免兩套格式打架）。") {
    plot.addSignal(gen::randomUniform(60, 0, 1));

    auto pct = std::make_shared<AutomaticTickGenerator>();
    pct->labelFormatter = [](double v, double) {
        return QStringLiteral("%1%").arg(v * 100, 0, 'f', 0);
    };
    plot.leftAxis()->tickGenerator = pct;
    plot.leftAxis()->label.text = RT("完成率");

    auto unit = std::make_shared<AutomaticTickGenerator>();
    unit->labelFormatter = [](double v, double) {
        return RT("%1 秒").arg(v, 0, 'f', 0);
    };
    plot.bottomAxis()->tickGenerator = unit;
}

QTPLOT_RECIPE("座標軸", "調整刻度", "刻度字型與顏色",
              "tickFont 控制刻度數字，label.font 控制軸標題，兩者各自獨立。"
              "軸線、刻度線、格線的顏色也都分開設。") {
    plot.addScatter(gen::consecutive(40), gen::randomWalk(40));

    Axis* left = plot.leftAxis();
    left->tickFont.size = 12;
    left->tickFont.bold = true;
    left->tickFont.color = QColor(30, 90, 160);
    left->label.text = RT("數值");
    left->label.font.size = 14;
    left->label.font.italic = true;
    left->label.font.color = QColor(30, 90, 160);
    left->frameLine = LineStyle(QColor(30, 90, 160), 2.0);
    left->majorTickLine = LineStyle(QColor(30, 90, 160), 1.5);

    Axis* bottom = plot.bottomAxis();
    bottom->tickFont.size = 11;
    bottom->tickFont.color = QColor(150, 60, 0);
    bottom->label.text = RT("取樣");
    bottom->label.font.color = QColor(150, 60, 0);
    bottom->frameLine = LineStyle(QColor(150, 60, 0), 2.0);

    plot.title.text = RT("刻度與標題各自的字型");
    plot.title.font.size = 17;
    plot.title.font.color = QColor(60, 60, 60);
}

// ───────────────────── 範圍 ─────────────────────

QTPLOT_RECIPE("座標軸", "調整範圍", "設定範圍的四種方式",
              "setAxisLimits 一次設兩軸；Axis::setRange 只設一根；"
              "autoScale 貼合資料；locked 完全鎖死。"
              "用 setter 設過的軸不會被 autoScale 蓋掉，但平移縮放照常 —— "
              "這點和 locked 不同。") {
    plot.addSignal(gen::sin(200, 1.0, 2.0));

    // 只指定 Y，X 交給 autoScale
    plot.leftAxis()->setRange(-2, 2);

    plot.addAnnotation(
        RT("leftAxis()->setRange(-2, 2)　X 交給 autoScale\n"
                       "setRange 設過的軸不會被 autoScale 蓋掉，但仍然可以拖曳縮放"),
        Qt::AlignLeft | Qt::AlignTop);
}

QTPLOT_RECIPE("座標軸", "調整範圍", "自動範圍的邊距",
              "autoScale(fraction) 的參數是兩端各留多少邊距（佔資料全距的比例）。"
              "0 就是緊貼資料 —— 線會壓在框線上，通常不好看。") {
    const QVector<double> xs = gen::consecutive(40);
    const QVector<double> ys = gen::sin(40, 1.0, 1.0);

    auto* tight = plot.addScatter(xs, ys, QColor(31, 119, 180));
    tight->marker = MarkerStyle::none();
    tight->legendText = RT("autoScale(0.0) 緊貼資料");

    plot.autoScale(0.0);
    plot.addAnnotation(RT("兩端完全沒有留白，線貼在框線上"),
                       Qt::AlignHCenter | Qt::AlignBottom);
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("座標軸", "調整範圍", "反向與鎖定",
              "inverted 讓軸由大到小；locked 讓它完全不動（平移、縮放、"
              "autoScale 都無效）。試著在圖上拖曳：只有 X 會動。") {
    plot.addScatter(gen::consecutive(30), gen::randomWalk(30, 1.0, 50));
    plot.leftAxis()->inverted = true;
    plot.leftAxis()->setRange(0, 100);
    plot.leftAxis()->locked = true;
    plot.leftAxis()->label.text = RT("排名（1 在上）");
    plot.addAnnotation(RT("Y 軸已反向並鎖定"), Qt::AlignRight | Qt::AlignTop);
}

// ───────────────────── 整體外觀 ─────────────────────

QTPLOT_RECIPE("樣式", "版面與框線", "標題、背景與框線",
              "標題、繪圖區底色、整張圖底色、格線、圖例外框都是分開的設定。"
              "paddingLeft 之類的是「最小」邊界，軸需要更多空間時會自動加大。") {
    plot.addSignal(gen::sin(120, 1.0, 2.0));
    plot.addSignal(gen::cos(120, 0.7, 2.0));

    plot.title.text = RT("量測結果");
    plot.title.font.size = 18;
    plot.title.font.bold = true;

    plot.style.figureBackground = QColor(248, 248, 250);
    plot.style.dataBackground = QColor(255, 255, 255);
    plot.grid.majorLine = LineStyle(QColor(0, 0, 0, 30), 1.0);

    for (Axis* a : plot.axes()) a->frameLine = LineStyle(QColor(90, 90, 100), 1.2);

    plot.legend.isVisible = true;
    plot.legend.border = LineStyle(QColor(90, 90, 100), 1.0);
    plot.legend.background = FillStyle(QColor(255, 255, 255, 230));
    plot.legend.padding = 8;

    plot.paddingLeft = plot.paddingRight = 16;
    plot.paddingTop = plot.paddingBottom = 14;
}

QTPLOT_RECIPE("樣式", "版面與框線", "只留左下兩根軸",
              "期刊常見的開放式框線：上軸與右軸整根關掉，格線也拿掉，"
              "畫面比完整框線乾淨。isVisible = false 的軸不佔版面空間。") {
    plot.addScatter(gen::randomUniform(60, 0, 10), gen::randomUniform(60, 0, 10));

    plot.topAxis()->isVisible = false;
    plot.rightAxis()->isVisible = false;
    plot.grid.isVisible = false;
    plot.bottomAxis()->label.text = QStringLiteral("X");
    plot.leftAxis()->label.text = QStringLiteral("Y");
}

QTPLOT_RECIPE("座標軸", "調整刻度", "旋轉刻度標籤",
              "類別名稱太長時水平排會互相重疊，tickLabelRotation 轉個角度就擠得下。"
              "軸的厚度是依「旋轉後的外接框」算的，所以文字不會被圖的下緣切掉 —— "
              "長標籤轉 45 度之後往下伸的距離，比一行字高好幾倍。") {
    const QStringList names{QStringLiteral("First Long Title"), QStringLiteral("Second Long Title"),
                            QStringLiteral("Third Long Title"), QStringLiteral("Fourth Long Title"),
                            QStringLiteral("Fifth Long Title"), QStringLiteral("Sixth Long Title")};
    plot.addBars({5, 10, 7, 13, 25, 60});

    QVector<Tick> ticks;
    for (int i = 0; i < names.size(); ++i) ticks.append({static_cast<double>(i), names[i]});
    auto gen = std::make_shared<ManualTickGenerator>();
    gen->ticks = ticks;
    plot.bottomAxis()->tickGenerator = gen;
    plot.bottomAxis()->tickLabelRotation = -45;
}

// ───────────────────── 截斷 ─────────────────────

QTPLOT_RECIPE("座標軸", "截斷", "截斷 Y 軸",
              "數值差距很大時（大部分在 0–12，少數在 1000 上下），整根軸照比例畫的話"
              "小的那群會被壓成一條線。addBreak(from, to) 把中間那段截掉：那段不佔長度，"
              "兩段各自產生刻度，空隙兩側畫上截斷記號。穿過截斷的長條在空隙裡被蓋掉，"
              "讀者一眼就知道那根不是照比例畫的。") {
    plot.addBars({3, 7, 5, 1006, 9, 4, 1002, 6});
    plot.leftAxis()->addBreak(12, 995);
    plot.setAxisLimits(-0.6, 7.6, 0, 1010);
    plot.leftAxis()->label.text = RT("數量");
    plot.title.text = RT("截斷 Y 軸（12–995）");
}

QTPLOT_RECIPE("座標軸", "截斷", "截斷 X 軸（跳過空檔）",
              "量測中間停機了一大段：照時間畫的話兩次量測擠在兩端，中間一大片空白。"
              "截掉空檔之後兩段資料都看得清楚。折線穿過截斷時不會斷開 —— "
              "截斷區裡的值一律對到空隙的正中間。breakGapPx 調空隙寬度，"
              "breakMarkLine／breakMarkSize 調記號。") {
    QVector<double> xs, ys;
    for (int i = 0; i <= 60; ++i) {
        xs.append(i);
        ys.append(std::sin(i * 0.2) * 3 + i * 0.05);
    }
    for (int i = 0; i <= 60; ++i) {
        xs.append(500 + i);
        ys.append(std::sin(i * 0.25) * 2 + 1);
    }
    plot.addScatter(xs, ys)->marker.size = 4;
    Axis* x = plot.bottomAxis();
    x->addBreak(63, 497);
    x->setBreakGapPx(14);
    x->breakMarkLine = LineStyle(QColor(200, 40, 40), 1.5);
    x->label.text = RT("時間 (min)");
    plot.title.text = RT("截斷 X 軸");
}

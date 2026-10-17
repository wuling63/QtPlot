// AxesLayout.cpp — 座標軸、刻度與版面的範例。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

#include <QDateTime>

using namespace qtplot;

QTPLOT_RECIPE("座標軸", "標籤與刻度", "軸標籤與標題",
              "標題與兩軸的標籤都是 Label，字型、顏色、粗體各自可設。"
              "版面會自動留出需要的空間，不用手調邊界。") {
    plot.addSignal(gen::sin(100, 1.0, 2.0));
    plot.title.text = RT("訊號量測結果");
    plot.title.font.size = 16;
    plot.bottomAxis()->label.text = RT("時間（秒）");
    plot.leftAxis()->label.text = RT("電壓（伏特）");
    plot.leftAxis()->label.font.bold = true;
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "手動指定刻度",
              "ManualTickGenerator 完全接管刻度位置與文字，適合類別軸。") {
    plot.addBars({5, 9, 3, 7, 6});
    auto manual = std::make_shared<ManualTickGenerator>();
    const QStringList days{RT("週一"), RT("週二"),
                           RT("週三"), RT("週四"),
                           RT("週五")};
    for (int i = 0; i < days.size(); ++i) manual->ticks.append({double(i), days[i], true});
    plot.bottomAxis()->tickGenerator = manual;
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "自訂刻度文字",
              "AutomaticTickGenerator 的 labelFormatter 可以改寫標籤，"
              "但間距還是自動算的。") {
    plot.addSignal(gen::randomUniform(50, 0, 1));
    auto gen2 = std::make_shared<AutomaticTickGenerator>();
    gen2->labelFormatter = [](double v, double) {
        return QStringLiteral("%1%").arg(v * 100, 0, 'f', 0);
    };
    plot.leftAxis()->tickGenerator = gen2;
    plot.leftAxis()->label.text = RT("百分比");
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "日期時間軸",
              "資料的 X 用 Unix 秒數，軸換成 DateTimeTickGenerator 就會印成日期，"
              "而且會依縮放層級自動換格式（年 → 月 → 日 → 時分）。") {
    const double start = QDateTime(QDate(2026, 1, 1), QTime(0, 0)).toSecsSinceEpoch();
    const int n = 365;
    QVector<double> xs, ys = gen::randomWalk(n, 1.0, 100);
    xs.reserve(n);
    for (int i = 0; i < n; ++i) xs.append(start + i * 86400.0);
    auto* s = plot.addSignalXY(xs, ys);
    s->marker = MarkerStyle::none();
    plot.bottomAxis()->tickGenerator = std::make_shared<DateTimeTickGenerator>();
    plot.leftAxis()->label.text = RT("價格");
}

QTPLOT_RECIPE("座標軸", "標籤與刻度", "對數軸",
              "資料先自己取 log10，再把軸換成 LogTickGenerator，"
              "刻度就會落在 10 的次方上並補 2–9 的次刻度。") {
    QVector<double> xs, ys;
    for (int i = 1; i <= 100; ++i) {
        xs.append(i);
        ys.append(std::log10(std::pow(i, 2.5)));
    }
    auto* s = plot.addScatter(xs, ys);
    s->marker = MarkerStyle::none();
    plot.leftAxis()->tickGenerator = std::make_shared<LogTickGenerator>();
    plot.leftAxis()->label.text = RT("y（對數）");
    plot.grid.minorLine = LineStyle(QColor(0, 0, 0, 18), 1.0);
}

QTPLOT_RECIPE("座標軸", "多軸與範圍", "左右雙 Y 軸",
              "addAxis 可以多開一根軸，再把圖元的 yAxis 指過去。"
              "兩組資料的數量級差很多時就需要這個。") {
    const QVector<double> xs = gen::consecutive(100);
    auto* small = plot.addScatter(xs, gen::sin(100, 1.0, 2.0));
    small->marker = MarkerStyle::none();
    small->legendText = RT("訊號（左軸）");

    auto* right = plot.addAxis(Edge::Right);
    right->showTickLabels = true;
    right->majorTickLength = 5;
    right->label.text = RT("溫度（右軸）");
    right->setRange(0, 1200);

    auto* big = plot.addScatter(xs, gen::randomWalk(100, 20, 600));
    big->yAxis = right;
    big->marker = MarkerStyle::none();
    big->legendText = RT("溫度（右軸）");

    // 兩軸的數量級差太多，各自指定範圍；setRange 設過的軸不會被 autoScale 蓋掉
    plot.leftAxis()->setRange(-1.2, 1.2);
    plot.leftAxis()->label.text = RT("訊號（左軸）");
    plot.legend.isVisible = true;
}

QTPLOT_RECIPE("座標軸", "多軸與範圍", "反向軸",
              "inverted 設 true 會讓軸由大到小。深度、排名之類的資料需要這樣畫。") {
    plot.addScatter(gen::consecutive(20), gen::randomWalk(20, 1.0, 10));
    plot.leftAxis()->inverted = true;
    plot.leftAxis()->label.text = RT("深度（公尺，向下為正）");
}

QTPLOT_RECIPE("座標軸", "多軸與範圍", "等比例軸",
              "equalAxisScale 讓兩軸的每單位像素數相同，圓才不會變成橢圓。") {
    QVector<double> xs, ys;
    for (int i = 0; i <= 200; ++i) {
        const double t = 2 * M_PI * i / 200.0;
        xs.append(std::cos(t) * 5);
        ys.append(std::sin(t) * 5);
    }
    auto* c = plot.addScatter(xs, ys);
    c->marker = MarkerStyle::none();
    plot.equalAxisScale = true;
    plot.addAnnotation(QStringLiteral("equalAxisScale = true"));
}

QTPLOT_RECIPE("座標軸", "多軸與範圍", "鎖住某一軸",
              "locked 的軸不受平移、縮放與自動範圍影響。"
              "試著在圖上拖曳：只有 X 會動。") {
    plot.addSignal(gen::sin(200, 1.0, 3.0));
    plot.leftAxis()->setRange(-1.5, 1.5);
    plot.leftAxis()->locked = true;
    plot.addAnnotation(RT("Y 軸已鎖定，拖曳只會平移 X"));
}

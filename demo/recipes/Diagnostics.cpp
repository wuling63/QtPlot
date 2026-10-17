// Diagnostics.cpp — 一致性、偏倚、不均等、機率紙、立體曲面、互動。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

// ───────────────────────── Bland–Altman ─────────────────────────

QTPLOT_RECIPE("統計", "診斷圖", "Bland–Altman 一致性圖",
              "兩種量測方法能不能互換？不要用相關係數或迴歸來判斷 —— "
              "相關量的是「一起變動」，兩把尺差了固定的 5 單位，相關係數照樣是 1。\n"
              "要看的是「差」隨「平均」怎麼變：中間實線是系統性偏差，"
              "上下虛線是 bias ± 1.96·SD 的一致性界限。\n"
              "這組資料的差隨著數值變大而變大（右邊的點散得比左邊開），"
              "代表兩法的差異是比例性的，不是固定的偏移 —— "
              "這種情形下一條固定寬度的界限其實已經不適用了，該先取 log 再畫。") {
    gen::setSeed(52);
    const int n = 120;
    QVector<double> a, b;
    for (int i = 0; i < n; ++i) {
        const double truth = 40 + gen::randomUniform(1, 0, 110).first();
        a.append(truth + gen::randomNormal(1, 0, 1.5 + truth * 0.02).first());
        b.append(truth * 1.03 + gen::randomNormal(1, -1.2, 1.5 + truth * 0.02).first());
    }

    const BlandAltman ba = blandAltman(a, b);
    auto* pts = plot.addMarkers(ba.means, ba.differences, QColor(45, 110, 185, 150));
    pts->marker.size = 6;
    pts->marker.outline = LineStyle::none();

    auto* bias = plot.addHorizontalLine(ba.bias, QColor(60, 60, 60));
    bias->line.width = 1.5;
    bias->text.text = QStringLiteral("偏差 %1").arg(ba.bias, 0, 'f', 2);

    for (double y : {ba.lowerLimit, ba.upperLimit}) {
        auto* l = plot.addHorizontalLine(y, QColor(200, 70, 50));
        l->line.pattern = LinePattern::Dashed;
        l->text.text = QStringLiteral("%1").arg(y, 0, 'f', 2);
    }

    plot.bottomAxis()->label.text = QStringLiteral("兩法的平均");
    plot.leftAxis()->label.text = QStringLiteral("兩法的差（A − B）");
    plot.title.text = QStringLiteral("Bland–Altman：差隨著數值放大");
}

// ───────────────────── 後設分析的漏斗圖 ─────────────────────

QTPLOT_RECIPE("統計", "後設分析", "漏斗圖（發表偏倚）",
              "每個點是一個研究：X 是效應量，Y 是標準誤（小研究在下面）。\n"
              "沒有偏倚時，點應該對稱地散在合併值兩側、形成一個倒三角 —— "
              "小研究精確度差所以散得開，大研究集中在頂端。\n"
              "缺了左下角那一塊通常代表「小規模又沒有效果的研究沒被發表」。\n"
              "Egger 迴歸把不對稱量化：截距離 0 愈遠愈不對稱。"
              "注意它和商務的「漏斗圖」(Funnel) 完全是兩回事。\n"
              "Y 軸要反向，標準誤小（研究大）的才會在上面。") {
    struct Study { double effect; double se; };
    // 刻意做出左下角的缺口：小研究只留下效果大的那些
    const QVector<Study> studies{
        {0.42, 0.06}, {0.38, 0.08}, {0.45, 0.09}, {0.33, 0.10}, {0.52, 0.12},
        {0.49, 0.14}, {0.61, 0.16}, {0.58, 0.18}, {0.72, 0.21}, {0.69, 0.23},
        {0.88, 0.26}, {0.81, 0.29}, {1.02, 0.33}, {0.95, 0.36},
    };

    QVector<MetaStudy> meta;
    QVector<double> xs, ys;
    for (const Study& s : studies) {
        meta.append({s.effect, s.se});
        xs.append(s.effect);
        ys.append(s.se);
    }
    const FunnelContour fc = funnelContour(meta);

    // 漏斗的兩條邊
    auto* left = plot.addScatter(fc.leftBound95, fc.seLevels, QColor(140, 140, 140));
    left->marker = MarkerStyle::none();
    left->line.pattern = LinePattern::Dashed;
    auto* right = plot.addScatter(fc.rightBound95, fc.seLevels, QColor(140, 140, 140));
    right->marker = MarkerStyle::none();
    right->line.pattern = LinePattern::Dashed;

    auto* pooled = plot.addVerticalLine(fc.pooledEffect, QColor(60, 60, 60));
    pooled->line.width = 1.4;

    auto* pts = plot.addMarkers(xs, ys, QColor(45, 110, 185));
    pts->marker.size = 8;

    auto* note = plot.addAnnotation(
        QStringLiteral("合併效應 %1\nEgger 截距 %2（離 0 愈遠愈不對稱）")
            .arg(fc.pooledEffect, 0, 'f', 3)
            .arg(fc.eggerIntercept, 0, 'f', 2),
        Qt::AlignLeft | Qt::AlignBottom);
    note->label.font.size = 9;

    plot.leftAxis()->inverted = true;
    plot.leftAxis()->label.text = QStringLiteral("標準誤");
    plot.bottomAxis()->label.text = QStringLiteral("效應量");
    plot.title.text = QStringLiteral("左下角缺了一塊：小而無效的研究沒被發表");
}

// ───────────────────────── 勞倫茲曲線 ─────────────────────────

QTPLOT_RECIPE("統計", "診斷圖", "勞倫茲曲線與吉尼係數",
              "橫軸是由少到多排序後的累積人口比例，縱軸是累積財富比例。\n"
              "對角線是完全平均（最窮的 50% 擁有 50% 的財富）。"
              "曲線離對角線愈遠愈不均；吉尼係數就是兩者之間面積的兩倍。\n"
              "面積用梯形法算，和畫出來的折線是同一條 —— "
              "圖上看到的和算出來的數字一定是同一回事。") {
    gen::setSeed(70);
    struct Case { QString name; QVector<double> values; QColor color; };
    QVector<double> even, skewed;
    for (int i = 0; i < 400; ++i) {
        even.append(40 + gen::randomNormal(1, 0, 8).first());
        // 對數常態：少數人拿走大部分
        skewed.append(std::exp(gen::randomNormal(1, 3.0, 1.1).first()));
    }

    const QVector<Case> cases{
        {QStringLiteral("較平均"), even, QColor(70, 150, 90)},
        {QStringLiteral("較集中"), skewed, QColor(200, 80, 50)},
    };
    for (const Case& c : cases) {
        const LorenzCurve lc = lorenzCurve(c.values);
        auto* line = plot.addScatter(lc.populationFraction, lc.valueFraction, c.color);
        line->marker = MarkerStyle::none();
        line->line.width = 2.0;
        line->fillToY = 0;
        QColor f = c.color;
        f.setAlpha(35);
        line->fill = FillStyle(f);
        line->legendText = QStringLiteral("%1（吉尼 %2）").arg(c.name).arg(lc.gini, 0, 'f', 3);
    }

    auto* diag = plot.addScatter({0, 1}, {0, 1}, QColor(120, 120, 120));
    diag->marker = MarkerStyle::none();
    diag->line.pattern = LinePattern::Dashed;
    diag->legendText = QStringLiteral("完全平均（吉尼 0）");

    plot.bottomAxis()->label.text = QStringLiteral("累積人口比例");
    plot.leftAxis()->label.text = QStringLiteral("累積所得比例");
    plot.title.text = QStringLiteral("勞倫茲曲線");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
    plot.setAxisLimits(0, 1, 0, 1);
}

// ───────────────────────── 機率紙 ─────────────────────────

QTPLOT_RECIPE("統計", "診斷圖", "Weibull 機率紙",
              "和 Q-Q 圖的分工：Q-Q 圖兩軸都是原始單位，只回答「像不像」；"
              "機率紙把兩軸都換成讓目標分布變成直線的刻度，所以除了「像不像」，"
              "還能直接從直線讀出參數。\n"
              "Weibull 紙上 y = β·ln(x) − β·ln(η)：斜率就是形狀參數 β，"
              "截距換算出尺度參數 η。β < 1 是早期失效、β ≈ 1 是隨機失效、"
              "β > 1 是磨耗失效 —— 可靠度工程判斷故障機制就是看這個數字。\n"
              "參考線一樣穿過四分位數而不是最小平方：機率紙正是用來找尾端偏離的，"
              "迴歸線會被尾端的點拉走。") {
    gen::setSeed(88);
    // 由均勻亂數反推 Weibull 樣本：x = η·(−ln U)^(1/β)
    const double trueBeta = 2.4, trueEta = 950;
    QVector<double> hours;
    for (int i = 0; i < 60; ++i) {
        const double u = gen::randomUniform(1, 0.001, 0.999).first();
        hours.append(trueEta * std::pow(-std::log(u), 1.0 / trueBeta));
    }

    const ProbabilityPlot pp = probabilityPlot(hours, ProbabilityPaper::Weibull);
    auto* pts = plot.addMarkers(pp.xs, pp.ys, QColor(45, 110, 185));
    pts->marker.size = 6;
    pts->legendText = QStringLiteral("壽命樣本（n = 60）");

    if (pp.referenceLine.isValid()) {
        CoordinateRange xr(pp.xs.first(), pp.xs.last());
        xr.pad(0.1);
        auto* ref = plot.addScatter(
            {xr.min, xr.max},
            {pp.referenceLine.predict(xr.min), pp.referenceLine.predict(xr.max)},
            QColor(200, 70, 50));
        ref->marker = MarkerStyle::none();
        ref->line.pattern = LinePattern::Dashed;
        ref->legendText = QStringLiteral("四分位參考線");
    }

    auto* note = plot.addAnnotation(
        QStringLiteral("估計 β = %1（真值 %2）\n估計 η = %3（真值 %4）\nβ > 1：磨耗型失效")
            .arg(pp.weibullShape, 0, 'f', 2)
            .arg(trueBeta, 0, 'f', 1)
            .arg(pp.weibullScale, 0, 'f', 0)
            .arg(trueEta, 0, 'f', 0),
        Qt::AlignLeft | Qt::AlignTop);
    note->label.font.size = 9;

    plot.bottomAxis()->label.text = QStringLiteral("ln(壽命小時)");
    plot.leftAxis()->label.text = QStringLiteral("ln(−ln(1 − F))");
    plot.title.text = QStringLiteral("Weibull 機率紙：斜率就是形狀參數");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
}

// ───────────────────────── 立體曲面 ─────────────────────────

QTPLOT_RECIPE("圖種", "等角投影", "立體曲面",
              "等角投影畫出來的 2.5D 曲面。遮擋靠畫家演算法 —— 由遠而近一格一格畫上去。\n"
              "要先講清楚它不是什麼：沒有深度緩衝、沒有透視、不能互動旋轉。"
              "做簡報用的曲面夠用，要可旋轉檢視的工程圖請用真正的 3D 函式庫。\n"
              "打光不是裝飾：不打光的話，同樣高度但朝向不同的兩片面顏色一樣，"
              "曲面會看起來像一張皺掉的色紙而不是立體的。") {
    const int n = 40;
    QVector<double> z(n * n);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            const double x = (c / double(n - 1)) * 6 - 3;
            const double y = (r / double(n - 1)) * 6 - 3;
            const double d = std::hypot(x, y);
            z[r * n + c] = std::exp(-d * d / 6.0) * std::cos(d * 2.2) * 2.4;
        }

    auto* s = plot.addSurface3D(z, n, n);
    s->xRange = {0, 10};
    s->yRange = {0, 10};
    s->projection.heightScale = 1.0;
    s->colormap = Colormap(Colormap::Turbo);
    s->mesh = LineStyle(QColor(0, 0, 0, 55), 0.6);

    plot.title.text = QStringLiteral("z = exp(−r²/6)·cos(2.2r)");
}

QTPLOT_RECIPE("圖種", "等角投影", "線框與格點",
              "同一份資料的三種畫法。\n"
              "Surface 看得出形狀但擋住後面；Wireframe 看得到背面，"
              "適合檢查格網本身有沒有破洞；Points 只畫格點，"
              "資料是散落的量測而不是連續曲面時用它比較誠實。\n"
              "meshStride 在格網很密時調大，否則格線會糊成一片黑。") {
    const int n = 28;
    QVector<double> z(n * n);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            const double x = (c / double(n - 1)) * 4 - 2;
            const double y = (r / double(n - 1)) * 4 - 2;
            z[r * n + c] = (x * x - y * y) * 0.6;  // 馬鞍面
        }

    auto* s = plot.addSurface3D(z, n, n);
    s->xRange = {0, 8};
    s->yRange = {0, 8};
    s->mode = Surface3D::Mode::Wireframe;
    s->mesh = LineStyle(QColor(60, 90, 160, 170), 0.9);

    plot.title.text = QStringLiteral("線框的馬鞍面（看得到背面）");
}

// ───────────────────────── 互動 ─────────────────────────

QTPLOT_RECIPE_WIDGET("互動", "視野與圖例", "點圖例切換顯示",
                     "點圖例的任一列可以把那一筆資料關掉或打開。\n"
                     "被關掉的那一列會畫成灰色加刪除線，而不是消失 —— "
                     "消失的話使用者就再也點不回來了。\n"
                     "圖例畫在繪圖區「裡面」，所以這個點擊必須在平移之前攔下來，"
                     "否則按在圖例上會變成拖曳整張圖。") {
    Plot& plot = widget.plot();
    gen::setSeed(15);
    const QVector<double> xs = gen::consecutive(60);
    const QStringList names{QStringLiteral("感測器 A"), QStringLiteral("感測器 B"),
                            QStringLiteral("感測器 C"), QStringLiteral("感測器 D")};
    for (int i = 0; i < names.size(); ++i) {
        auto* s = plot.addScatter(xs, gen::randomWalk(60, 1.0, 20 + i * 12));
        s->marker = MarkerStyle::none();
        s->legendText = names[i];
    }
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignTop;
    plot.title.text = QStringLiteral("點圖例把某一條關掉");
    plot.leftAxis()->label.text = QStringLiteral("讀值");

    widget.legendToggleEnabled = true;
}

QTPLOT_RECIPE_WIDGET("互動", "視野與圖例", "視野的復原與重做",
                     "Ctrl + Z 回到上一個視野，Ctrl + Y 再往前。\n"
                     "沒有這個功能的話，縮過頭就只能按「自動範圍」—— "
                     "而那會跳回整體，使用者想回的其實是上一個視野。\n"
                     "每根軸的範圍都要記，不是只記主要四根："
                     "addAxis() 加出來的軸也會被平移縮放動到，"
                     "漏掉的話倒回去之後那幾根軸還停在原處，資料和刻度就對不上了。\n"
                     "滾輪是連續的，所以只在「開始滾」時記一步，"
                     "否則一次滾動就把整段歷程洗掉了。") {
    Plot& plot = widget.plot();
    gen::setSeed(26);
    auto* s = plot.addScatter(gen::consecutive(500), gen::randomWalk(500, 1.0, 100));
    s->marker = MarkerStyle::none();
    plot.title.text = QStringLiteral("縮放之後按 Ctrl + Z 回上一個視野");
    plot.leftAxis()->label.text = QStringLiteral("數值");

    widget.viewHistoryEnabled = true;
    widget.setFocusPolicy(Qt::StrongFocus);  // 要收得到鍵盤才按得到 Ctrl+Z
}

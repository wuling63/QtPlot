// Analysis.cpp — 診斷與推論圖的範例：管制圖、Q-Q、ROC/PR、存活曲線、森林圖、頻譜。
//
// 這一章的圖和前面的「圖種」不太一樣：它們的重點不在形狀，而在背後那個數字算得對。
// 所以每一支範例都刻意放進一個「算錯就會看出來」的對照，而不是只畫一條好看的曲線。
#include "../Recipe.h"
#include "qtplot/DataGen.h"

using namespace qtplot;

namespace {

/// 基準期量到的製程參數。
///
/// 實務上管制界限來自一段確認過穩定的「基準期」，不是來自正在監控的那批資料 ——
/// 用正在監控的資料現算，製程一旦偏掉，偏掉的量自己會把界限推走。
constexpr double kBaselineCenter = 10.0;
constexpr double kBaselineSigma = 0.25;

/// 一條從第 30 點起整體往上偏 1.3σ 的製程資料，共 60 點。
///
/// 偏得不多，所以單點幾乎碰不到 3σ —— 這組資料的用意就是讓「只看規則 1」失效。
/// 偏移後留 30 點是刻意的：1.3σ 的偏移本來就要累積一段才看得出來，
/// 只留十幾點的話連規則 2 都還來不及湊滿連續 9 點。
QVector<double> shiftedProcess() {
    gen::setSeed(17);
    QVector<double> v = gen::randomNormal(60, kBaselineCenter, kBaselineSigma);
    for (int i = 30; i < v.size(); ++i) v[i] += 1.3 * kBaselineSigma;
    return v;
}

/// 一條從第 10 點起持續往上漂的製程資料。
///
/// 和 shiftedProcess 的步階偏移不同：持續的漂移才會明顯撐大「全部資料的標準差」，
/// 步階偏移只膨脹約一成，看不出兩種 σ 估法的差別。
QVector<double> driftingProcess() {
    gen::setSeed(17);
    QVector<double> v = gen::randomNormal(40, kBaselineCenter, kBaselineSigma);
    for (int i = 10; i < v.size(); ++i) v[i] += (i - 9) * 0.06;
    return v;
}

/// 一個不怎麼厲害的分類器：分數和真實類別有關但不完全一致。
void toyClassifier(QVector<double>& scores, QVector<bool>& labels) {
    gen::setSeed(9);
    const int nPos = 60, nNeg = 140;
    const QVector<double> pos = gen::randomNormal(nPos, 1.1, 1.0);
    const QVector<double> neg = gen::randomNormal(nNeg, 0.0, 1.0);
    for (double s : pos) {
        scores.append(s);
        labels.append(true);
    }
    for (double s : neg) {
        scores.append(s);
        labels.append(false);
    }
}

}  // namespace

// ───────────────────────── 品質管制 ─────────────────────────

QTPLOT_RECIPE("統計", "品質管制", "管制圖與 Nelson 規則",
              "中心線與 ±3σ 界限來自基準期，不是現場現算 —— 實務上就是這樣用的，"
              "拿正在監控的資料現算界限，製程一旦偏掉，偏掉的量自己會把界限推走。\n"
              "這組資料從第 30 點起整體往上偏 1.3σ，偏得不多，所以單點很難碰到 3σ。"
              "左下角列的是每條規則第一次報警的位置：規則 6 在偏移後第 11 點就察覺，"
              "規則 2 在第 19 點，而規則 1 拖到第 29 點才報 —— 整整晚了十幾個樣本，"
              "這段期間做出來的全是偏掉的東西。\n"
              "背景三層帶是 1σ／2σ／3σ 分區，點上的數字是觸發的規則編號。") {
    ControlLimits baseline;
    baseline.center = kBaselineCenter;
    baseline.sigma = kBaselineSigma;

    auto* c = plot.addControlChart(shiftedProcess(),
                                   {ControlRule::Beyond3Sigma, ControlRule::NineSameSide,
                                    ControlRule::TwoOfThreeBeyond2Sigma,
                                    ControlRule::FourOfFiveBeyond1Sigma});
    c->limits = baseline;
    c->legendText = RT("量測值");

    auto* shift = plot.addVerticalLine(29.5);
    shift->line = LineStyle(QColor(120, 120, 120, 170), 1.2, LinePattern::Dashed);

    // 說明文字直接從實際跑出來的結果生，才不會和圖上畫的對不起來
    QMap<int, int> firstHit;  // 規則編號 → 第一次觸發的索引
    for (const RuleViolation& v : c->violations()) {
        const int r = static_cast<int>(v.rule);
        if (!firstHit.contains(r)) firstHit.insert(r, v.index);
    }
    QStringList lines;
    for (auto it = firstHit.cbegin(); it != firstHit.cend(); ++it)
        lines << RT("規則 %1 第 %2 點：%3")
                     .arg(it.key())
                     .arg(it.value())
                     .arg(controlRuleName(static_cast<ControlRule>(it.key())));
    auto* note = plot.addAnnotation(lines.join(QLatin1Char('\n')),
                                    Qt::AlignLeft | Qt::AlignBottom);
    note->label.font.size = 9;

    plot.leftAxis()->label.text = RT("直徑（mm）");
    plot.bottomAxis()->label.text = RT("樣本序號");
    plot.title.text = RT("製程管制圖（I 圖）：第 30 點起偏移 1.3σ");
    plot.grid.isVisible = false;
}

QTPLOT_RECIPE("統計", "品質管制", "σ 估法決定抓不抓得到漂移",
              "同一份資料、同一條規則，只換 σ 的估法，結論完全相反。\n"
              "畫出來的分區帶與實心界限用平均移動全距（R̄/1.128）：它只看相鄰兩點的差，"
              "漂移影響不到它，所以界限窄，後段的點超出去被抓到。\n"
              "外面那兩條點線是「用全部資料的標準差」會得到的界限。"
              "漂移本身被算進變異裡，σ 大了快三倍，界限跟著被撐寬到連一個點都不報 —— "
              "要找的問題被自己撐寬的界限藏起來了。\n"
              "注意兩種估法共用同一條中心線，而中心線是全部資料的平均 —— "
              "被後段的漂移往上拉了，於是前段那些其實正常的點反而掉到 LCL 以下被標紅。"
              "σ 換估法救不了這件事，真正的解法是像前一支範例那樣用基準期的界限。") {
    const QVector<double> v = driftingProcess();

    auto* c = plot.addControlChart(v, {ControlRule::Beyond3Sigma});
    c->sigmaEstimate = SigmaEstimate::MovingRange;
    c->legendText = RT("量測值");

    const ControlLimits mr = c->effectiveLimits();
    const ControlLimits sd = controlLimits(v, SigmaEstimate::StandardDeviation);

    // 把標準差法的界限疊上去對照
    for (double y : {sd.upper(), sd.lower()}) {
        auto* line = plot.addHorizontalLine(y);
        line->line = LineStyle(QColor(150, 110, 60), 1.4, LinePattern::Dotted);
    }
    auto* wide = plot.addText(RT("標準差法的 UCL"), 1, sd.upper());
    wide->alignment = Qt::AlignLeft | Qt::AlignBottom;
    wide->label.font.color = QColor(150, 110, 60);
    wide->label.font.size = 9;

    auto* note = plot.addAnnotation(
        RT("移動全距法  σ = %1  → 抓到 %2 點\n"
                       "標準差法    σ = %3  → 抓到 %4 點")
            .arg(mr.sigma, 0, 'f', 3)
            .arg(c->violations().size())
            .arg(sd.sigma, 0, 'f', 3)
            .arg(nelsonRules(v, sd).size()),
        Qt::AlignLeft | Qt::AlignBottom);
    note->label.font.size = 9;

    plot.leftAxis()->label.text = RT("直徑（mm）");
    plot.bottomAxis()->label.text = RT("樣本序號");
    plot.title.text = RT("漂移把標準差法的界限撐寬到報不出來");
    plot.grid.isVisible = false;
}

// ───────────────────────── 診斷圖 ─────────────────────────

QTPLOT_RECIPE("統計", "診斷圖", "常態 Q-Q 圖",
              "把排序後的樣本對上常態分布的理論分位數。資料真的是常態的話，"
              "點會落在那條參考線上。\n"
              "參考線穿過兩組資料的第一與第三四分位數，不是最小平方迴歸線 —— "
              "迴歸線會被離群值拉走，而 Q-Q 圖正是用來找離群值的，那就本末倒置了。") {
    gen::setSeed(31);
    const QVector<double> v = gen::randomNormal(200, 170, 7);

    Scatter* refLine = nullptr;
    auto* pts = plot.addNormalQQ(v, &refLine, QColor(45, 110, 185));
    pts->marker.size = 5;
    pts->marker.outline = LineStyle(QColor(255, 255, 255, 170), 0.7);
    pts->legendText = RT("樣本（n = 200）");
    if (refLine) refLine->legendText = RT("四分位參考線");

    plot.leftAxis()->label.text = RT("身高（cm）");
    plot.title.text = RT("常態 Q-Q 圖：點貼著線就是常態");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
}

QTPLOT_RECIPE("統計", "診斷圖", "Q-Q 圖認偏態與厚尾",
              "Q-Q 圖的用處是「偏離的方向告訴你偏離的種類」。\n"
              "右偏（對數常態）：右端往上翹 —— 大的那一側比常態還要大。\n"
              "厚尾（兩端混進幾個極端值）：左端往下、右端往上，整體成 S 形。\n"
              "均勻分布則相反，兩端往中間收，因為它根本沒有尾巴。\n"
              "三組都先標準化成平均 0、標準差 1，否則三條線的斜率差太多，"
              "看到的是尺度差異而不是形狀差異。") {
    gen::setSeed(5);
    const int n = 150;

    auto standardize = [](QVector<double> v) {
        double mean = 0;
        for (double x : v) mean += x;
        mean /= v.size();
        double ss = 0;
        for (double x : v) ss += (x - mean) * (x - mean);
        const double sd = std::sqrt(ss / (v.size() - 1));
        for (double& x : v) x = (x - mean) / sd;
        return v;
    };

    QVector<double> lognormal;
    for (double x : gen::randomNormal(n, 0, 1)) lognormal.append(std::exp(x));

    QVector<double> heavy = gen::randomNormal(n - 10, 0, 1);
    for (int i = 0; i < 5; ++i) {
        heavy.append(4.5 + i * 0.4);
        heavy.append(-4.5 - i * 0.4);
    }

    struct Series { QString name; QVector<double> data; QColor color; MarkerShape shape; };
    const QVector<Series> series{
        {RT("右偏（對數常態）"), standardize(lognormal), QColor(200, 80, 50),
         MarkerShape::FilledCircle},
        {RT("厚尾"), standardize(heavy), QColor(70, 110, 190),
         MarkerShape::FilledTriangleUp},
        {RT("均勻分布"), standardize(gen::randomUniform(n, 0, 1)),
         QColor(90, 150, 80), MarkerShape::FilledSquare},
    };

    for (const Series& s : series) {
        const QQResult qq = normalQQ(s.data);
        auto* m = plot.addMarkers(qq.theoretical, qq.sample, s.color);
        m->marker.shape = s.shape;
        m->marker.size = 5;
        m->legendText = s.name;
    }

    // y = x 是「標準常態」該有的樣子，三組都該和它比
    auto* ref = plot.addScatter({-3.2, 3.2}, {-3.2, 3.2}, QColor(110, 110, 110));
    ref->marker = MarkerStyle::none();
    ref->line.pattern = LinePattern::Dashed;
    ref->legendText = RT("標準常態 (y = x)");

    plot.bottomAxis()->label.text = RT("理論分位數");
    plot.leftAxis()->label.text = RT("標準化後的樣本分位數");
    plot.title.text = RT("偏離的方向告訴你偏離的種類");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignLeft | Qt::AlignTop;
    plot.setAxisLimits(-3.4, 3.4, -4.2, 5.2);
}

QTPLOT_RECIPE("統計", "診斷圖", "ROC 曲線",
              "每個門檻對應一組（假陽率, 真陽率）。曲線愈靠左上愈好，"
              "對角線是亂猜。AUC 是曲線下面積，等於「隨便抓一個陽性和一個陰性，"
              "陽性分數比較高」的機率。\n"
              "同分的樣本必須整批一起算，否則那一群之間會畫出一段階梯，"
              "而階梯的形狀取決於它們在輸入陣列裡的先後 —— "
              "同一份資料換個順序就得到不一樣的 AUC。整批算完才落一個點，"
              "於是同分的那一段在圖上是一條斜線（代表「無從分辨」）。") {
    QVector<double> scores;
    QVector<bool> labels;
    toyClassifier(scores, labels);
    const ClassifierCurve roc = rocCurve(scores, labels);

    auto* diag = plot.addScatter({0, 1}, {0, 1}, QColor(140, 140, 140));
    diag->marker = MarkerStyle::none();
    diag->line.pattern = LinePattern::Dashed;
    diag->legendText = RT("亂猜（AUC = 0.5）");

    auto* curve = plot.addClassifierCurve(roc, QColor(190, 60, 50), true);
    curve->legendText = RT("模型（AUC = %1）").arg(roc.area, 0, 'f', 3);

    // 把 Youden 指數最大的那個門檻標出來 —— 這是最常用的「切在哪裡」準則
    int best = 0;
    double bestJ = -1;
    for (int i = 0; i < roc.xs.size(); ++i) {
        const double j = roc.ys[i] - roc.xs[i];
        if (j > bestJ) {
            bestJ = j;
            best = i;
        }
    }
    auto* mark = plot.addMarkers({roc.xs[best]}, {roc.ys[best]}, QColor(40, 40, 40));
    mark->marker.shape = MarkerShape::OpenCircle;
    mark->marker.size = 11;
    mark->marker.outline.width = 2;
    auto* tip = plot.addCallout(
        RT("最佳門檻 %1\nTPR %2 / FPR %3")
            .arg(roc.thresholds[best], 0, 'f', 2)
            .arg(roc.ys[best], 0, 'f', 2)
            .arg(roc.xs[best], 0, 'f', 2),
        roc.xs[best], roc.ys[best], roc.xs[best] + 0.22, roc.ys[best] - 0.22);
    tip->label.font.size = 9;

    plot.bottomAxis()->label.text = RT("假陽率 FPR");
    plot.leftAxis()->label.text = RT("真陽率 TPR");
    plot.title.text = RT("ROC 曲線");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignBottom;
    plot.setAxisLimits(-0.02, 1.02, -0.02, 1.02);
}

QTPLOT_RECIPE("統計", "診斷圖", "Precision–Recall 曲線",
              "類別很不平衡時 PR 比 ROC 有用：ROC 的假陽率分母是全部陰性樣本，"
              "陰性一多，多報幾個假陽性對 FPR 幾乎沒影響，曲線看起來還是很漂亮。"
              "PR 的分母是「被判為陽性的」，假陽性一多精確率馬上掉。\n"
              "這裡兩組資料的陽性率分別是 30% 與 3%，分辨能力差不多 —— "
              "圖例裡的 AUC 只差 0.03，看 ROC 會以為兩個模型一樣好；"
              "但 AP 從 0.61 掉到 0.20，PR 曲線整條壓下來。\n"
              "面積用的是平均精確率 AP = Σ(ΔR)·P，不是梯形面積 —— "
              "PR 曲線在相鄰兩點之間不是直線，梯形法會系統性高估。") {
    auto make = [](int nPos, int nNeg, unsigned seed) {
        gen::setSeed(seed);
        QVector<double> scores;
        QVector<bool> labels;
        for (double s : gen::randomNormal(nPos, 1.1, 1.0)) {
            scores.append(s);
            labels.append(true);
        }
        for (double s : gen::randomNormal(nNeg, 0.0, 1.0)) {
            scores.append(s);
            labels.append(false);
        }
        return QPair<QVector<double>, QVector<bool>>(scores, labels);
    };

    const auto balanced = make(300, 700, 11);
    const auto skewed = make(30, 970, 11);

    const ClassifierCurve prB = prCurve(balanced.first, balanced.second);
    const ClassifierCurve prS = prCurve(skewed.first, skewed.second);
    const ClassifierCurve rocB = rocCurve(balanced.first, balanced.second);
    const ClassifierCurve rocS = rocCurve(skewed.first, skewed.second);

    auto* b = plot.addClassifierCurve(prB, QColor(45, 110, 185));
    b->legendText = RT("陽性 30%：AP %1（AUC %2）")
                        .arg(prB.area, 0, 'f', 3)
                        .arg(rocB.area, 0, 'f', 3);
    auto* s = plot.addClassifierCurve(prS, QColor(200, 80, 50));
    s->legendText = RT("陽性 3%：AP %1（AUC %2）")
                        .arg(prS.area, 0, 'f', 3)
                        .arg(rocS.area, 0, 'f', 3);

    // 基準線 = 陽性率。亂猜的分類器的 PR 曲線就是這條水平線。
    for (const ClassifierCurve& c : {prB, prS}) {
        const double base = static_cast<double>(c.positives) / (c.positives + c.negatives);
        auto* h = plot.addHorizontalLine(base);
        h->line = LineStyle(QColor(140, 140, 140, 170), 1.0, LinePattern::Dotted);
    }
    auto* note = plot.addAnnotation(
        RT("虛線 = 亂猜的基準（就是陽性率）"), Qt::AlignLeft | Qt::AlignBottom);
    note->label.font.size = 9;

    plot.bottomAxis()->label.text = RT("召回率 Recall");
    plot.leftAxis()->label.text = RT("精確率 Precision");
    plot.title.text = RT("類別不平衡時 PR 比 ROC 敏感");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignTop;
    plot.setAxisLimits(-0.02, 1.02, -0.02, 1.05);
}

// ───────────────────────── 存活分析 ─────────────────────────

QTPLOT_RECIPE("統計", "存活分析", "Kaplan–Meier 存活曲線",
              "階梯只在「事件發生」時往下掉，掉多少看當時還有多少人在追蹤。"
              "小豎線是設限：追蹤結束時還沒發生事件的人，他們不讓曲線往下掉，"
              "但會把後面的分母變小，所以之後每一步掉得更深。\n"
              "信賴區間用 Greenwood 變異數加 log(−log S) 轉換。"
              "直接用 S ± 1.96·SE 的話，兩端的區間會跑到 0 以下或 1 以上 —— "
              "存活率畫到 1.05 很顯然是錯的。\n"
              "帶也必須是階梯形：照轉折點直接連直線的話，帶會斜著拉過去，"
              "和階梯線中間露出一塊白。") {
    gen::setSeed(23);
    // 兩組：治療組的風險比較低
    auto cohort = [](int n, double scale, double followUp) {
        QVector<double> times;
        QVector<bool> events;
        for (int i = 0; i < n; ++i) {
            // 指數分布的存活時間
            const double u = gen::randomUniform(1, 0.001, 0.999).first();
            const double t = -scale * std::log(u);
            const double cut = followUp * gen::randomUniform(1, 0.55, 1.0).first();
            times.append(std::min(t, cut));
            events.append(t <= cut);
        }
        return QPair<QVector<double>, QVector<bool>>(times, events);
    };

    const auto treated = cohort(70, 26, 36);
    const auto control = cohort(70, 13, 36);

    const SurvivalCurve kmT = kaplanMeier(treated.first, treated.second);
    const SurvivalCurve kmC = kaplanMeier(control.first, control.second);

    const SurvivalPlottables t = plot.addSurvivalCurve(kmT, QColor(45, 125, 190));
    t.curve->legendText = RT("治療組（n = 70）");
    const SurvivalPlottables c = plot.addSurvivalCurve(kmC, QColor(200, 75, 55));
    c.curve->legendText = RT("對照組（n = 70）");

    auto* half = plot.addHorizontalLine(0.5);
    half->line = LineStyle(QColor(130, 130, 130, 180), 1.0, LinePattern::Dashed);

    QStringList medians;
    if (!std::isnan(kmC.medianSurvival))
        medians << RT("對照組中位 %1 個月").arg(kmC.medianSurvival, 0, 'f', 1);
    if (!std::isnan(kmT.medianSurvival))
        medians << RT("治療組中位 %1 個月").arg(kmT.medianSurvival, 0, 'f', 1);
    else
        medians << RT("治療組未達中位存活");
    auto* note = plot.addAnnotation(medians.join(QLatin1Char('\n')),
                                    Qt::AlignLeft | Qt::AlignBottom);
    note->label.font.size = 9;

    plot.bottomAxis()->label.text = RT("追蹤時間（月）");
    plot.title.text = RT("Kaplan–Meier 存活曲線（陰影為 95% CI）");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignTop;
    plot.setAxisLimits(0, 37, 0, 1.03);
}

// ───────────────────────── 後設分析 ─────────────────────────

QTPLOT_RECIPE("統計", "後設分析", "森林圖",
              "每一列一個研究：方塊是點估計、橫線是 95% 信賴區間、"
              "方塊的大小代表權重（按面積等比，不是按邊長 —— "
              "按邊長縮的話，權重差兩倍看起來像差四倍）。最底下的菱形是合併估計值。\n"
              "勝算比這類比值畫在對數軸上：否則「兩倍」和「一半」在圖上長度不一樣，"
              "視覺上會偏向其中一邊。做法是自己先取 log10 再把軸換成 LogTickGenerator，"
              "無效應線（比值 1）也要跟著取 log 變成 0。") {
    struct Study { QString name; double or_; double lo; double hi; double weight; };
    const QVector<Study> studies{
        {QStringLiteral("Chen 2019"), 1.82, 1.05, 3.15, 8},
        {QStringLiteral("Dubois 2020"), 2.41, 1.48, 3.92, 14},
        {QStringLiteral("Edwards 2020"), 0.94, 0.52, 1.70, 11},
        {QStringLiteral("Fischer 2021"), 1.65, 1.21, 2.25, 23},
        {QStringLiteral("García 2021"), 3.10, 1.12, 8.58, 5},
        {QStringLiteral("Hollis 2022"), 1.38, 1.02, 1.87, 27},
        {QStringLiteral("Iwata 2023"), 0.71, 0.18, 2.80, 3},
    };

    // 固定效應合併：權重 = 1/變異，在 log 尺度上加權平均
    double sumW = 0, sumWY = 0;
    for (const Study& s : studies) {
        const double y = std::log(s.or_);
        const double se = (std::log(s.hi) - std::log(s.lo)) / (2 * 1.959963985);
        const double w = 1.0 / (se * se);
        sumW += w;
        sumWY += w * y;
    }
    const double pooledLog = sumWY / sumW;
    const double pooledSe = std::sqrt(1.0 / sumW);
    const double pooled = std::exp(pooledLog);
    const double pooledLo = std::exp(pooledLog - 1.959963985 * pooledSe);
    const double pooledHi = std::exp(pooledLog + 1.959963985 * pooledSe);

    auto row = [](const QString& name, double est, double lo, double hi, double w,
                  bool summary) {
        ForestRow r;
        r.label = name;
        // 軸是對數軸，資料要先取 log10
        r.estimate = std::log10(est);
        r.lower = std::log10(lo);
        r.upper = std::log10(hi);
        r.weight = w;
        r.isSummary = summary;
        r.annotation = QStringLiteral("%1 [%2, %3]")
                           .arg(est, 0, 'f', 2)
                           .arg(lo, 0, 'f', 2)
                           .arg(hi, 0, 'f', 2);
        return r;
    };

    QVector<ForestRow> rows;
    for (const Study& s : studies) rows.append(row(s.name, s.or_, s.lo, s.hi, s.weight, false));
    rows.append(row(RT("合併（固定效應）"), pooled, pooledLo, pooledHi, 0, true));

    auto* f = plot.addForestPlot(rows);
    f->nullValue = 0;  // log10(1) = 0
    f->defaultColor = QColor(55, 95, 160);
    f->capSizePx = 3;

    plot.bottomAxis()->tickGenerator = std::make_shared<LogTickGenerator>();
    plot.bottomAxis()->label.text = RT("勝算比（對數軸）");
    plot.title.text = RT("森林圖：七個研究與合併估計");
    plot.grid.isVisible = false;
    // 右邊要留給數值欄
    plot.setAxisLimits(std::log10(0.12), std::log10(26), -0.7, rows.size() - 0.3);
}

// ───────────────────────── 頻譜 ─────────────────────────

QTPLOT_RECIPE("場", "頻譜", "功率譜與窗函數",
              "三個頻率疊起來的訊號，取 FFT 看頻譜。\n"
              "直接做 FFT 等於乘上一個矩形窗，而矩形窗的旁波很強，"
              "於是一個單頻訊號會在頻譜上拖出一片裙邊（頻譜洩漏），"
              "把旁邊比較小的成分蓋掉。乘上兩端漸收的窗可以把旁波壓下去，"
              "代價是主波變寬。\n"
              "強訊號 100.7 Hz 刻意不落在頻率格上（格距 2 Hz）—— 這點很要緊："
              "剛好落在格上的訊號連矩形窗都幾乎不洩漏，拿那種訊號來示範什麼都看不到。\n"
              "另外兩個成分只有 −62 dB。不開窗時裙邊落在 −40 dB 附近，"
              "兩個峰整個埋在底下；換成 Hann 就清楚露出來，"
              "Blackman–Harris 的底再低一些、也更平。") {
    const double fs = 1000;
    const int n = 500;  // 格距 = 1000/500 = 2 Hz
    QVector<double> sig(n);
    for (int i = 0; i < n; ++i) {
        const double t = i / fs;
        sig[i] = 1.0 * std::sin(2 * kPi * 100.7 * t) +
                 0.0008 * std::sin(2 * kPi * 56 * t) +
                 0.0008 * std::sin(2 * kPi * 160 * t);
    }

    struct Which { WindowFunction w; QString name; QColor color; };
    const QVector<Which> windows{
        {WindowFunction::Rectangular, RT("矩形（不開窗）"), QColor(200, 80, 50)},
        {WindowFunction::Hann, QStringLiteral("Hann"), QColor(45, 110, 185)},
        {WindowFunction::BlackmanHarris, QStringLiteral("Blackman–Harris"),
         QColor(80, 150, 80)},
    };
    for (const Which& item : windows) {
        const Periodogram pg = periodogram(sig, fs, item.w);
        auto* s = plot.addScatter(pg.frequencies, pg.decibels, item.color);
        s->marker = MarkerStyle::none();
        s->line.width = 1.2;
        s->legendText = item.name;
    }

    for (double f : {56.0, 100.7, 160.0}) {
        auto* v = plot.addVerticalLine(f);
        v->line = LineStyle(QColor(130, 130, 130, 120), 1.0, LinePattern::Dotted);
    }

    plot.bottomAxis()->label.text = RT("頻率（Hz）");
    plot.leftAxis()->label.text = RT("振幅（dB）");
    plot.title.text = RT("窗函數壓掉旁波，小的成分才看得見");
    plot.legend.isVisible = true;
    plot.legend.alignment = Qt::AlignRight | Qt::AlignTop;
    plot.setAxisLimits(0, 250, -150, 10);
}

QTPLOT_RECIPE("場", "頻譜", "頻譜圖（掃頻與諧波）",
              "短時傅立葉變換：把訊號切成一段一段，每段各做一次 FFT，"
              "一列一個頻率、一行一個時間，用顏色表示強度。\n"
              "窗長同時決定兩個軸的解析度，而且是反向的：窗長則頻率分得細、"
              "時間分得粗（短促的聲音被抹開），窗短則反過來。這是測不準關係，"
              "沒有「都要好」的設定。\n"
              "色階一定要轉 dB：聲音的能量跨好幾個數量級，"
              "線性色階會讓除了最強的那一塊之外全部變成同一個顏色。") {
    const double fs = 4000;
    const int n = 8000;  // 2 秒
    QVector<double> sig(n);
    gen::setSeed(3);
    const QVector<double> noise = gen::randomNormal(n, 0, 0.02);
    for (int i = 0; i < n; ++i) {
        const double t = i / fs;
        // 線性掃頻 200 → 900 Hz，加一個二次諧波
        const double phase = 2 * kPi * (200 * t + 175 * t * t);
        sig[i] = std::sin(phase) + 0.35 * std::sin(2 * phase);
        // 固定的 1500 Hz 哨音，只在中段出現
        if (t > 0.7 && t < 1.3) sig[i] += 0.5 * std::sin(2 * kPi * 1500 * t);
        sig[i] += noise[i];
    }

    const Spectrogram sp = spectrogram(sig, fs, 512, 64, WindowFunction::Hann, true, 70);
    auto* h = plot.addSpectrogram(sp, Colormap(Colormap::Turbo));
    plot.addColorbar(*h)->label.text = RT("強度（dB）");

    plot.title.text = RT("頻譜圖：掃頻、二次諧波與中段的哨音");
    plot.grid.isVisible = false;
    plot.setAxisLimits(0, sp.duration(), 0, sp.maxFrequency());
}

QTPLOT_RECIPE("場", "頻譜", "Welch 平均功率譜",
              "只做一次 FFT 的功率譜，每一格都是一個隨機變數：雜訊底像一片草叢，"
              "比雜訊底只高幾 dB 的成分（180 Hz 與 410 Hz）藏在草裡分不出來。\n"
              "welch() 把訊號切成彼此重疊一半的短段，每段各做一次功率譜再把功率平均起來，"
              "草叢的起伏大約縮成 1/√段數，兩個小峰就浮出來了。\n"
              "灰線是同樣 1024 點的單一段：比較要用同樣的段長 —— 雜訊的能量平均分到每一格，"
              "FFT 越長、每格分到的越少，雜訊底的高度本來就會隨長度改變。") {
    const double fs = 2000;
    const int n = 32768;
    gen::setSeed(12);
    QVector<double> sig = gen::randomNormal(n, 0, 0.5);
    for (int i = 0; i < n; ++i) {
        const double t = i / fs;
        sig[i] += 1.0 * std::sin(2 * kPi * 50 * t) + 0.1 * std::sin(2 * kPi * 180 * t) +
                  0.07 * std::sin(2 * kPi * 410 * t);
    }

    const Periodogram one = periodogram(sig.mid(0, 1024), fs);
    auto* raw = plot.addScatter(one.frequencies, one.decibels, QColor(160, 160, 160));
    raw->marker = MarkerStyle::none();
    raw->line.width = 1.0;
    raw->legendText = RT("單一段（1024 點）");

    const Periodogram avg = welch(sig, fs, 1024, 0.5);
    auto* w = plot.addScatter(avg.frequencies, avg.decibels, QColor(31, 119, 180));
    w->marker = MarkerStyle::none();
    w->line.width = 1.8;
    w->legendText = RT("Welch（%1 段平均）").arg(avg.segments);

    plot.bottomAxis()->label.text = RT("頻率（Hz）");
    plot.leftAxis()->label.text = RT("振幅（dB）");
    for (double f : {180.0, 410.0}) {
        auto* v = plot.addVerticalLine(f);
        v->line = LineStyle(QColor(130, 130, 130, 120), 1.0, LinePattern::Dotted);
    }

    plot.title.text = RT("平均之後，藏在雜訊裡的小峰才看得見");
    plot.legend.isVisible = true;
    plot.setAxisLimits(0, 500, -55, 5);
}

QTPLOT_RECIPE("場", "頻譜", "自動標出頻譜峰值",
              "馬達振動：轉速 29.5 Hz 與它的倍頻，加上一個不在倍頻上的軸承頻率。\n"
              "findSpectralPeaks() 找出比附近雜訊底高出門檻的峰，依高度排序，"
              "頻率用相鄰三格做拋物線內插 —— 格距 0.98 Hz，標出來的頻率卻準到小數點後一位。\n"
              "minSpacingHz 讓太近的峰只留一個，minFrequency 跳過直流與漂移。") {
    const double fs = 1000;
    const int n = 16384;
    gen::setSeed(4);
    QVector<double> sig = gen::randomNormal(n, 0, 0.05);
    const double rot = 29.5;
    for (int i = 0; i < n; ++i) {
        const double t = i / fs;
        sig[i] += 0.8 + 1.0 * std::sin(2 * kPi * rot * t) + 0.35 * std::sin(2 * kPi * 2 * rot * t) +
                  0.12 * std::sin(2 * kPi * 3 * rot * t) + 0.06 * std::sin(2 * kPi * 162.3 * t);
    }

    const Periodogram pg = welch(sig, fs, 1024, 0.5);
    auto* s = plot.addScatter(pg.frequencies, pg.decibels, QColor(45, 110, 185));
    s->marker = MarkerStyle::none();
    s->line.width = 1.3;

    const QVector<SpectralPeak> peaks = findSpectralPeaks(pg, 6, 15, 5, 5);
    QVector<double> px, py;
    for (const SpectralPeak& p : peaks) {
        px.append(p.frequency);
        py.append(p.decibels);
        auto* t = plot.addText(QStringLiteral("%1 Hz").arg(p.frequency, 0, 'f', 1), p.frequency, p.decibels);
        t->alignment = Qt::AlignHCenter | Qt::AlignBottom;
        t->offsetY = -10;
        t->label.font.size = 9;
        t->background = FillStyle(QColor(255, 255, 255, 210));
    }
    auto* m = plot.addMarkers(px, py, QColor(214, 39, 40));
    m->marker = MarkerStyle(MarkerShape::FilledTriangleDown, 9, QColor(214, 39, 40));

    plot.bottomAxis()->label.text = RT("頻率（Hz）");
    plot.leftAxis()->label.text = RT("振幅（dB）");
    plot.title.text = RT("轉速倍頻與軸承頻率");
    plot.setAxisLimits(0, 220, -75, 10);
}

QTPLOT_RECIPE("場", "頻譜", "3D 瀑布頻譜",
              "機台升速：轉速從 10 Hz 升到 60 Hz，三條斜線是 1、2、3 倍轉速，"
              "140 Hz 那條不動的是結構共振 —— 轉速倍頻掃過它時會被激起來。\n"
              "Waterfall3D::setSpectrogram() 把頻譜圖的每個時間格排成一條曲線，"
              "顏色由舊到新；floorZ 設成頻譜圖的最小值，曲線才會往下填到地板。"
              "可以用滑鼠拖曳旋轉。") {
    const double fs = 1000;
    const int n = 6000;  // 6 秒
    gen::setSeed(9);
    QVector<double> sig = gen::randomNormal(n, 0, 0.01);
    double phase = 0;
    for (int i = 0; i < n; ++i) {
        const double t = i / fs;
        const double rot = 10 + 50 * t / 6;
        phase += 2 * kPi * rot / fs;
        // 倍頻靠近 140 Hz 時共振被激起（簡單的共振曲線）
        double res = 0;
        for (int h = 1; h <= 3; ++h) res += 1 / (1 + std::pow((h * rot - 140) / 6, 2));
        sig[i] += std::sin(phase) + 0.45 * std::sin(2 * phase) + 0.2 * std::sin(3 * phase) +
                  0.6 * res * std::sin(2 * kPi * 140 * t);
    }

    const Spectrogram sp = spectrogram(sig, fs, 512, 128, WindowFunction::Hann, true, 45);
    auto w = std::make_unique<Waterfall3D>();
    w->setSpectrogram(sp, 30, Colormap(Colormap::Viridis), 250);
    w->edgeWidth = 0.7;
    w->fillAlpha = 230;

    auto* chart = plot.addChart3D();
    chart->add(std::move(w));
    chart->xLabel = RT("頻率（Hz）");
    chart->yLabel = RT("時間（秒）");
    chart->zLabel = RT("強度（dB）");
    chart->aspectX = 1.5;
    chart->aspectZ = 0.5;
    chart->azimuth = -25;
    chart->elevation = 30;
    plot.title.text = RT("升速過程的瀑布頻譜");
}

#include "Analysis.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QMap>
#include <QPolygonF>

#include <functional>
#include <numeric>

namespace qtplot {

// ───────────────────────── 常態分布 ─────────────────────────

double normalPdf(double x, double mean, double sd) {
    if (!(sd > 0)) return std::numeric_limits<double>::quiet_NaN();
    const double z = (x - mean) / sd;
    return std::exp(-0.5 * z * z) / (sd * std::sqrt(2 * kPi));
}

double normalCdf(double x, double mean, double sd) {
    if (!(sd > 0)) return std::numeric_limits<double>::quiet_NaN();
    // 用 erfc 而不是 1 − 0.5·erfc(z/√2)：左尾時後者是兩個相近數相減，
    // 會把有效位數吃掉（x = −8 附近直接變成 0）。
    return 0.5 * std::erfc(-(x - mean) / (sd * std::sqrt(2.0)));
}

double normalQuantile(double p, double mean, double sd) {
    if (!(sd > 0) || !std::isfinite(p)) return std::numeric_limits<double>::quiet_NaN();
    if (p <= 0) return -std::numeric_limits<double>::infinity();
    if (p >= 1) return std::numeric_limits<double>::infinity();

    // Wichura (1988) AS241 的 PPND16。三段有理逼近，分界在 |p − 0.5| = 0.425
    // 與 √(−ln r) = 5（約 p = 1.4e-12）。
    const double q = p - 0.5;
    double z = 0;
    if (std::abs(q) <= 0.425) {
        const double r = 0.180625 - q * q;
        z = q *
            (((((((2.5090809287301226727e+3 * r + 3.3430575583588128105e+4) * r +
                  6.7265770927008700853e+4) *
                     r +
                 4.5921953931549871457e+4) *
                    r +
                1.3731693765509461125e+4) *
                   r +
               1.9715909503065514427e+3) *
                  r +
              1.3314166789178437745e+2) *
                 r +
             3.3871328727963666080e+0) /
            (((((((5.2264952788528545610e+3 * r + 2.8729085735721942674e+4) * r +
                  3.9307895800092710610e+4) *
                     r +
                 2.1213794301586595867e+4) *
                    r +
                5.3941960214247511077e+3) *
                   r +
               6.8718700749205790830e+2) *
                  r +
              4.2313330701600911252e+1) *
                 r +
             1.0);
    } else {
        double r = (q < 0) ? p : 1 - p;
        r = std::sqrt(-std::log(r));
        if (r <= 5.0) {
            r -= 1.6;
            z = (((((((7.74545014278341407640e-4 * r + 2.27238449892691845833e-2) * r +
                      2.41780725177450611770e-1) *
                         r +
                     1.27045825245236838258e+0) *
                        r +
                    3.64784832476320460504e+0) *
                       r +
                   5.76949722146069140550e+0) *
                      r +
                  4.63033784615654529590e+0) *
                     r +
                 1.42343711074968357734e+0) /
                (((((((1.05075007164441684324e-9 * r + 5.47593808499534494600e-4) * r +
                      1.51986665636164571966e-2) *
                         r +
                     1.48103976427480074590e-1) *
                        r +
                    6.89767334985100004550e-1) *
                       r +
                   1.67638483018380384940e+0) *
                      r +
                  2.05319162663775882187e+0) *
                     r +
                 1.0);
        } else {
            r -= 5.0;
            z = (((((((2.01033439929228813265e-7 * r + 2.71155556874348757815e-5) * r +
                      1.24266094738807843860e-3) *
                         r +
                     2.65321895265761230930e-2) *
                        r +
                    2.96560571828504891230e-1) *
                       r +
                   1.78482653991729133580e+0) *
                      r +
                  5.46378491116411436990e+0) *
                     r +
                 6.65790464350110377720e+0) /
                (((((((2.04426310338993978564e-15 * r + 1.42151175831644588870e-7) * r +
                      1.84631831751005468180e-5) *
                         r +
                     7.86869131145613259100e-4) *
                        r +
                    1.48753612908506148525e-2) *
                       r +
                   1.36929880922735805310e-1) *
                      r +
                  5.99832206555887937690e-1) *
                     r +
                 1.0);
        }
        if (q < 0) z = -z;
    }
    return mean + sd * z;
}

// ───────────────────────── Q-Q 圖 ─────────────────────────

QQResult normalQQ(const QVector<double>& values, PlottingPosition pp) {
    QQResult out;
    QVector<double> v;
    v.reserve(values.size());
    for (double x : values)
        if (std::isfinite(x)) v.append(x);
    if (v.isEmpty()) return out;
    std::sort(v.begin(), v.end());

    const int n = static_cast<int>(v.size());
    out.sample = v;
    out.theoretical.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double rank = i + 1;
        double prob = 0;
        switch (pp) {
        case PlottingPosition::Blom:    prob = (rank - 0.375) / (n + 0.25); break;
        case PlottingPosition::Hazen:   prob = (rank - 0.5) / n; break;
        case PlottingPosition::Weibull: prob = rank / (n + 1.0); break;
        }
        out.theoretical.append(normalQuantile(prob));
    }

    // 參考線穿過兩組的 Q1 與 Q3。n 太小時四分位數會重合，那就退回不畫。
    const double sx1 = sampleQuantile(out.theoretical, 0.25);
    const double sx3 = sampleQuantile(out.theoretical, 0.75);
    const double sy1 = sampleQuantile(out.sample, 0.25);
    const double sy3 = sampleQuantile(out.sample, 0.75);
    if (std::isfinite(sx1) && std::isfinite(sx3) && sx3 > sx1) {
        out.referenceLine.slope = (sy3 - sy1) / (sx3 - sx1);
        out.referenceLine.intercept = sy1 - out.referenceLine.slope * sx1;
        out.referenceLine.count = n;
        out.referenceLine.rSquared = 1;  // 不是配適出來的，沒有 R²
    }
    return out;
}

// ───────────────────── ROC / PR ─────────────────────

namespace {

/// 依分數由大到小排好的索引。分數相同時依索引，排序才唯一。
QVector<int> orderByScoreDescending(const QVector<double>& scores) {
    QVector<int> idx(scores.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        if (scores[a] != scores[b]) return scores[a] > scores[b];
        return a < b;
    });
    return idx;
}

struct TieBlock {
    double score = 0;
    int truePositives = 0;   ///< 這一批裡的陽性數
    int falsePositives = 0;  ///< 這一批裡的陰性數
};

/// 把排好序的樣本按「分數相同」切成一批一批。
QVector<TieBlock> tieBlocks(const QVector<double>& scores, const QVector<bool>& labels,
                            int& positives, int& negatives) {
    positives = negatives = 0;
    QVector<TieBlock> blocks;
    const QVector<int> order = orderByScoreDescending(scores);
    for (int k = 0; k < order.size(); ++k) {
        const int i = order[k];
        if (!std::isfinite(scores[i])) continue;
        if (blocks.isEmpty() || blocks.last().score != scores[i])
            blocks.append({scores[i], 0, 0});
        if (labels[i]) {
            ++blocks.last().truePositives;
            ++positives;
        } else {
            ++blocks.last().falsePositives;
            ++negatives;
        }
    }
    return blocks;
}

}  // namespace

double trapezoidArea(const QVector<double>& xs, const QVector<double>& ys) {
    double a = 0;
    for (int i = 1; i < xs.size() && i < ys.size(); ++i)
        a += (xs[i] - xs[i - 1]) * (ys[i] + ys[i - 1]) / 2.0;
    return a;
}

ClassifierCurve rocCurve(const QVector<double>& scores, const QVector<bool>& labels) {
    ClassifierCurve c;
    if (scores.size() != labels.size() || scores.isEmpty()) return c;

    const QVector<TieBlock> blocks = tieBlocks(scores, labels, c.positives, c.negatives);
    if (c.positives == 0 || c.negatives == 0) return c;

    c.xs.append(0);
    c.ys.append(0);
    c.thresholds.append(std::numeric_limits<double>::infinity());

    int tp = 0, fp = 0;
    for (const TieBlock& b : blocks) {
        tp += b.truePositives;
        fp += b.falsePositives;
        c.xs.append(static_cast<double>(fp) / c.negatives);
        c.ys.append(static_cast<double>(tp) / c.positives);
        c.thresholds.append(b.score);
    }
    c.area = trapezoidArea(c.xs, c.ys);
    return c;
}

ClassifierCurve prCurve(const QVector<double>& scores, const QVector<bool>& labels) {
    ClassifierCurve c;
    if (scores.size() != labels.size() || scores.isEmpty()) return c;

    const QVector<TieBlock> blocks = tieBlocks(scores, labels, c.positives, c.negatives);
    if (c.positives == 0) return c;

    // 第一個點：召回率 0。精確率在這裡沒有定義（還沒判任何樣本為陽性），
    // 照 sklearn 的慣例填 1，純粹是為了讓曲線從左邊起頭。
    c.xs.append(0);
    c.ys.append(1);
    c.thresholds.append(std::numeric_limits<double>::infinity());

    int tp = 0, fp = 0;
    double prevRecall = 0;
    for (const TieBlock& b : blocks) {
        tp += b.truePositives;
        fp += b.falsePositives;
        const double recall = static_cast<double>(tp) / c.positives;
        const double precision = static_cast<double>(tp) / (tp + fp);
        c.xs.append(recall);
        c.ys.append(precision);
        c.thresholds.append(b.score);
        c.area += (recall - prevRecall) * precision;
        prevRecall = recall;
    }
    return c;
}

// ───────────────────────── 存活分析 ─────────────────────────

SurvivalCurve kaplanMeier(const QVector<double>& times, const QVector<bool>& events,
                          double confidence) {
    SurvivalCurve out;
    if (times.size() != events.size() || times.isEmpty()) return out;

    QVector<int> order(times.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        if (times[a] != times[b]) return times[a] < times[b];
        // 同時間時事件排在設限之前，和下面「先算事件再移除設限者」一致
        if (events[a] != events[b]) return events[a];
        return a < b;
    });

    int atRisk = 0;
    for (int i : order)
        if (std::isfinite(times[i])) ++atRisk;
    if (atRisk == 0) return out;

    const double z = normalQuantile(1 - (1 - std::clamp(confidence, 0.5, 0.9999)) / 2);
    double survival = 1.0;
    double greenwoodSum = 0;  ///< Σ d / (n (n − d))

    SurvivalPoint start;
    start.time = 0;
    start.atRisk = atRisk;
    out.points.append(start);

    int k = 0;
    while (k < order.size()) {
        const double t = times[order[k]];
        if (!std::isfinite(t)) {
            ++k;
            continue;
        }
        int d = 0, censored = 0;
        const int riskHere = atRisk;
        while (k < order.size() && times[order[k]] == t) {
            if (events[order[k]])
                ++d;
            else
                ++censored;
            ++k;
        }

        if (d > 0) {
            survival *= 1.0 - static_cast<double>(d) / riskHere;
            if (riskHere > d) greenwoodSum += static_cast<double>(d) / (riskHere * (riskHere - d));
        }

        SurvivalPoint p;
        p.time = t;
        p.survival = survival;
        p.atRisk = riskHere;
        p.events = d;
        p.censored = censored;
        p.lower = p.upper = survival;

        // log(−log S) 轉換。S 為 0 或 1 時 log(−log S) 沒有定義，
        // 那兩種情形直接把區間收成一點 —— 比硬算出 NaN 讓曲線斷掉好。
        if (survival > 0 && survival < 1 && greenwoodSum > 0) {
            const double seLogLog = std::sqrt(greenwoodSum) / std::abs(std::log(survival));
            const double half = z * seLogLog;
            p.lower = std::pow(survival, std::exp(half));
            p.upper = std::pow(survival, std::exp(-half));
        }
        out.points.append(p);

        if (censored > 0) {
            out.censorTimes.append(t);
            out.censorSurvival.append(survival);
        }
        if (std::isnan(out.medianSurvival) && survival <= 0.5) out.medianSurvival = t;

        atRisk -= d + censored;
        if (atRisk <= 0) break;
    }
    return out;
}

// ───────────────────────── 管制圖 ─────────────────────────

ControlLimits controlLimits(const QVector<double>& values, SigmaEstimate estimate) {
    ControlLimits out;
    QVector<double> v;
    v.reserve(values.size());
    for (double x : values)
        if (std::isfinite(x)) v.append(x);
    if (v.size() < 2) return out;

    out.center = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    if (estimate == SigmaEstimate::MovingRange) {
        double sum = 0;
        for (int i = 1; i < v.size(); ++i) sum += std::abs(v[i] - v[i - 1]);
        const double meanRange = sum / (v.size() - 1);
        // d2 = 1.128 是 n = 2 時「全距的期望值 / σ」。除掉它才是 σ 的無偏估計。
        out.sigma = meanRange / 1.128;
    } else {
        double ss = 0;
        for (double x : v) ss += (x - out.center) * (x - out.center);
        out.sigma = std::sqrt(ss / (v.size() - 1));
    }
    return out;
}

QString controlRuleName(ControlRule rule) {
    switch (rule) {
    case ControlRule::Beyond3Sigma: return QCoreApplication::translate("qtplot", "1 點超出 3σ");
    case ControlRule::NineSameSide: return QCoreApplication::translate("qtplot", "連續 9 點同一側");
    case ControlRule::SixTrending: return QCoreApplication::translate("qtplot", "連續 6 點單向升降");
    case ControlRule::FourteenAlternating: return QCoreApplication::translate("qtplot", "連續 14 點上下交替");
    case ControlRule::TwoOfThreeBeyond2Sigma: return QCoreApplication::translate("qtplot", "3 點中 2 點超出 2σ");
    case ControlRule::FourOfFiveBeyond1Sigma: return QCoreApplication::translate("qtplot", "5 點中 4 點超出 1σ");
    case ControlRule::FifteenWithin1Sigma: return QCoreApplication::translate("qtplot", "連續 15 點在 1σ 之內");
    case ControlRule::EightOutside1Sigma: return QCoreApplication::translate("qtplot", "連續 8 點在 1σ 之外");
    }
    return QString();
}

namespace {

/// 每個點離中心幾個 σ（含正負號）。非有限值填 NaN。
QVector<double> sigmaScores(const QVector<double>& values, const ControlLimits& limits) {
    QVector<double> z;
    z.reserve(values.size());
    for (double v : values)
        z.append(std::isfinite(v) ? (v - limits.center) / limits.sigma
                                  : std::numeric_limits<double>::quiet_NaN());
    return z;
}

/// 「連續 run 個點都滿足 pred」—— 滿足就回報那一串的最後一點。
void runRule(const QVector<double>& z, int run, ControlRule rule,
             const std::function<bool(double)>& pred, QVector<RuleViolation>& out) {
    int streak = 0;
    for (int i = 0; i < z.size(); ++i) {
        if (std::isnan(z[i])) {
            streak = 0;
            continue;
        }
        streak = pred(z[i]) ? streak + 1 : 0;
        if (streak >= run) {
            out.append({i, rule});
            // 重設而不是續算：連續 12 點同一側不該被報成 4 次（9、10、11、12）。
            streak = 0;
        }
    }
}

/// 「m 個點之中有 k 個同側超出 kSigma」。
void kOfMRule(const QVector<double>& z, int k, int m, double threshold, ControlRule rule,
              QVector<RuleViolation>& out) {
    for (int i = m - 1; i < z.size(); ++i) {
        for (int sign = -1; sign <= 1; sign += 2) {
            int hit = 0;
            bool usable = true;
            for (int j = i - m + 1; j <= i; ++j) {
                if (std::isnan(z[j])) {
                    usable = false;
                    break;
                }
                if (z[j] * sign > threshold) ++hit;
            }
            // 要求最後一點本身就超出，否則同一個形狀會連報好幾次
            if (usable && hit >= k && z[i] * sign > threshold) {
                out.append({i, rule});
                break;
            }
        }
    }
}

}  // namespace

QVector<RuleViolation> nelsonRules(const QVector<double>& values, const ControlLimits& limits,
                                   const QVector<ControlRule>& enabled) {
    QVector<RuleViolation> out;
    if (!limits.isValid() || values.isEmpty()) return out;

    QVector<ControlRule> rules = enabled;
    if (rules.isEmpty()) rules.append(ControlRule::Beyond3Sigma);

    const QVector<double> z = sigmaScores(values, limits);
    for (ControlRule r : rules) {
        switch (r) {
        case ControlRule::Beyond3Sigma:
            for (int i = 0; i < z.size(); ++i)
                if (!std::isnan(z[i]) && std::abs(z[i]) > 3.0) out.append({i, r});
            break;
        case ControlRule::NineSameSide:
            runRule(z, 9, r, [](double v) { return v > 0; }, out);
            runRule(z, 9, r, [](double v) { return v < 0; }, out);
            break;
        case ControlRule::SixTrending: {
            // 趨勢看的是相鄰差，不是離中心的距離，所以不能套 runRule
            for (int dir = -1; dir <= 1; dir += 2) {
                int streak = 1;
                for (int i = 1; i < z.size(); ++i) {
                    if (std::isnan(z[i]) || std::isnan(z[i - 1])) {
                        streak = 1;
                        continue;
                    }
                    streak = ((z[i] - z[i - 1]) * dir > 0) ? streak + 1 : 1;
                    if (streak >= 6) {
                        out.append({i, r});
                        streak = 1;
                    }
                }
            }
            break;
        }
        case ControlRule::FourteenAlternating: {
            // 規則說的是「14 個點」，也就是 13 個正負交替的差。
            // streak 數的是點數，所以看到第一個差時就已經有 2 個點了 ——
            // 從 1 開始加會變成要 15 個點才觸發。
            int streak = 1;
            for (int i = 1; i < z.size(); ++i) {
                if (std::isnan(z[i]) || std::isnan(z[i - 1])) {
                    streak = 1;
                    continue;
                }
                const bool havePrev = (i >= 2 && !std::isnan(z[i - 2]));
                const double d = z[i] - z[i - 1];
                const double prev = havePrev ? z[i - 1] - z[i - 2] : 0;
                if (havePrev && d * prev < 0)
                    ++streak;
                else
                    streak = 2;  // 這一對點重新起算一串
                if (streak >= 14) {
                    out.append({i, r});
                    streak = 1;
                }
            }
            break;
        }
        case ControlRule::TwoOfThreeBeyond2Sigma:
            kOfMRule(z, 2, 3, 2.0, r, out);
            break;
        case ControlRule::FourOfFiveBeyond1Sigma:
            kOfMRule(z, 4, 5, 1.0, r, out);
            break;
        case ControlRule::FifteenWithin1Sigma:
            runRule(z, 15, r, [](double v) { return std::abs(v) < 1.0; }, out);
            break;
        case ControlRule::EightOutside1Sigma:
            runRule(z, 8, r, [](double v) { return std::abs(v) > 1.0; }, out);
            break;
        }
    }

    std::sort(out.begin(), out.end(), [](const RuleViolation& a, const RuleViolation& b) {
        if (a.index != b.index) return a.index < b.index;
        return static_cast<int>(a.rule) < static_cast<int>(b.rule);
    });
    return out;
}

ControlLimits ControlChart::effectiveLimits() const {
    if (limits.isValid()) return limits;
    return controlLimits(values, sigmaEstimate);
}

QVector<RuleViolation> ControlChart::violations() const {
    return nelsonRules(values, effectiveLimits(), rules);
}

namespace {

double chartX(const ControlChart& c, int i) {
    return (i < c.xs.size()) ? c.xs[i] : static_cast<double>(i);
}

}  // namespace

AxisLimits ControlChart::axisLimits() const {
    AxisLimits lim;
    for (int i = 0; i < values.size(); ++i) {
        lim.x.expand(chartX(*this, i));
        lim.y.expand(values[i]);
    }
    // 界限也要含進來：3σ 超出資料範圍時（製程很穩）界限線會被裁掉，
    // 那整張圖就只剩一條在中間晃的折線，看不出有沒有靠近界限。
    const ControlLimits cl = effectiveLimits();
    if (cl.isValid()) {
        lim.y.expand(cl.upper());
        lim.y.expand(cl.lower());
    }
    return lim;
}

void ControlChart::render(RenderPack& rp) {
    if (values.isEmpty()) return;
    const ControlLimits cl = effectiveLimits();
    if (!cl.isValid()) return;

    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // ── 分區帶 ──
    const double xLeft = t.rect.left;
    const double xRight = t.rect.right;
    for (int band = 0; band < zoneColors.size(); ++band) {
        const QColor c = zoneColors[band];
        if (!c.isValid() || c.alpha() == 0) continue;
        const double inner = band;        // 0σ, 1σ, 2σ
        const double outer = band + 1.0;  // 1σ, 2σ, 3σ
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        for (int sign = -1; sign <= 1; sign += 2) {
            const double a = t.py(cl.at(sign * inner));
            const double b = t.py(cl.at(sign * outer));
            p.drawRect(QRectF(QPointF(xLeft, std::min(a, b)), QPointF(xRight, std::max(a, b))));
        }
    }

    // ── 界限線 ──
    auto hline = [&](double value, const LineStyle& style) {
        if (!style.isVisible()) return;
        QPen pen = style.pen();
        pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        const double y = t.py(value);
        p.drawLine(QPointF(xLeft, y), QPointF(xRight, y));
    };
    for (int sign = -1; sign <= 1; sign += 2)
        for (int k = 1; k <= 2; ++k) hline(cl.at(sign * k), sigmaLine);
    hline(cl.upper(), controlLine);
    hline(cl.lower(), controlLine);
    hline(cl.center, centerLine);

    // ── 折線與標記 ──
    QVector<QPointF> pts;
    pts.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        if (!std::isfinite(values[i])) continue;
        pts.append(t.p(chartX(*this, i), values[i]));
    }
    if (line.isVisible() && pts.size() >= 2) {
        QPen pen = line.pen();
        pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(pts.constData(), static_cast<int>(pts.size()));
    }
    if (marker.isVisible()) {
        MarkerStyle m = marker;
        m.size *= rp.scaleFactor;
        m.outline.width *= rp.scaleFactor;
        drawMarkers(p, pts, m);
    }

    // ── 被規則抓到的點 ──
    const QVector<RuleViolation> bad = violations();
    if (!bad.isEmpty()) {
        QVector<QPointF> badPts;
        QMap<int, QVector<int>> rulesAt;  // index → 規則編號，QMap 保證順序固定
        for (const RuleViolation& v : bad) {
            if (v.index < 0 || v.index >= values.size() || !std::isfinite(values[v.index]))
                continue;
            if (!rulesAt.contains(v.index))
                badPts.append(t.p(chartX(*this, v.index), values[v.index]));
            rulesAt[v.index].append(static_cast<int>(v.rule));
        }
        if (violationMarker.isVisible()) {
            MarkerStyle m = violationMarker;
            m.size *= rp.scaleFactor;
            m.outline.width *= rp.scaleFactor;
            drawMarkers(p, badPts, m);
        }
        if (showViolationLabels) {
            for (auto it = rulesAt.cbegin(); it != rulesAt.cend(); ++it) {
                QStringList nums;
                for (int r : it.value()) nums << QString::number(r);
                Label lb;
                lb.text = nums.join(QLatin1Char(','));
                lb.font = violationFont;
                lb.font.size *= rp.scaleFactor;
                const QPointF at = t.p(chartX(*this, it.key()), values[it.key()]);
                drawText(p, at + QPointF(0, -violationMarker.size * rp.scaleFactor * 0.8), lb,
                         Qt::AlignHCenter | Qt::AlignBottom);
            }
        }
    }

    // ── CL / UCL / LCL ──
    if (showLimitLabels) {
        struct Item { double value; QString text; };
        const Item items[] = {{cl.upper(), QStringLiteral("UCL")},
                              {cl.center, QStringLiteral("CL")},
                              {cl.lower(), QStringLiteral("LCL")}};
        for (const Item& item : items) {
            Label lb;
            lb.text = item.text;
            lb.font = limitFont;
            lb.font.size *= rp.scaleFactor;
            drawText(p, QPointF(xRight - 3 * rp.scaleFactor, t.py(item.value)), lb,
                     Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    p.restore();
}

QVector<LegendItem> ControlChart::legendItems() const {
    if (legendText.isEmpty()) return {};
    LegendItem item;
    item.text = legendText;
    item.line = line;
    item.marker = marker;
    return {item};
}

// ───────────────────────── 森林圖 ─────────────────────────

QVector<double> ForestPlot::rowPositions() const {
    QVector<double> pos;
    pos.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i)
        pos.append(topToBottom ? static_cast<double>(rows.size() - 1 - i) : static_cast<double>(i));
    return pos;
}

QStringList ForestPlot::rowLabels() const {
    // 刻度產生器要的是「位置 0, 1, 2... 各對應什麼名字」，
    // 所以要按位置排，不是按 rows 的順序。
    QStringList out;
    out.resize(rows.size());
    const QVector<double> pos = rowPositions();
    for (int i = 0; i < rows.size(); ++i) {
        const int slot = static_cast<int>(pos[i]);
        if (slot >= 0 && slot < out.size()) out[slot] = rows[i].label;
    }
    return out;
}

AxisLimits ForestPlot::axisLimits() const {
    AxisLimits lim;
    if (rows.isEmpty()) return lim;
    const QVector<double> pos = rowPositions();
    for (int i = 0; i < rows.size(); ++i) {
        const ForestRow& r = rows[i];
        lim.x.expand(r.estimate);
        lim.x.expand(r.lower);
        lim.x.expand(r.upper);
        lim.y.expand(pos[i]);
    }
    if (std::isfinite(nullValue)) lim.x.expand(nullValue);
    lim.y.min -= 0.6;
    lim.y.max += 0.6;
    return lim;
}

void ForestPlot::render(RenderPack& rp) {
    if (rows.isEmpty()) return;
    const Transform t = transform(rp);
    QPainter& p = *rp.painter;
    p.save();

    // 一列在畫面上多高。列的位置相隔 1 個資料單位，所以一列就是 scaleY 個像素 ——
    // 菱形的高度要跟著它縮放，否則縮放視野時菱形會變成細縫或胖到蓋住鄰列。
    const double rowHeightPx = std::abs(t.scaleY());

    if (std::isfinite(nullValue) && nullLine.isVisible()) {
        QPen pen = nullLine.pen();
        pen.setWidthF(pen.widthF() * rp.scaleFactor);
        p.setPen(pen);
        const double x = t.px(nullValue);
        p.drawLine(QPointF(x, t.rect.top), QPointF(x, t.rect.bottom));
    }

    double maxWeight = 0;
    for (const ForestRow& r : rows)
        if (!r.isSummary) maxWeight = std::max(maxWeight, r.weight);

    const QVector<double> pos = rowPositions();
    for (int i = 0; i < rows.size(); ++i) {
        const ForestRow& r = rows[i];
        const double y = t.py(pos[i]);
        const QColor color = r.isSummary ? summaryColor
                                         : (r.color.isValid() ? r.color : defaultColor);

        if (r.isSummary) {
            // 菱形：四個頂點分別是下界、上界與中心估計值的上下
            const double h = rowHeightPx * std::clamp(summaryHeightFraction, 0.05, 1.0) / 2;
            QPolygonF diamond;
            diamond << QPointF(t.px(r.lower), y) << QPointF(t.px(r.estimate), y - h)
                    << QPointF(t.px(r.upper), y) << QPointF(t.px(r.estimate), y + h);
            p.setPen(QPen(color, 1.0 * rp.scaleFactor));
            p.setBrush(color);
            p.drawPolygon(diamond);
        } else {
            QPen pen = intervalLine.pen();
            pen.setColor(color);
            pen.setWidthF(pen.widthF() * rp.scaleFactor);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);

            const double xl = t.px(r.lower);
            const double xr = t.px(r.upper);
            const double clipL = std::max(std::min(xl, xr), t.rect.left);
            const double clipR = std::min(std::max(xl, xr), t.rect.right);
            if (clipR > clipL) p.drawLine(QPointF(clipL, y), QPointF(clipR, y));

            const double cap = capSizePx * rp.scaleFactor;
            if (cap > 0) {
                if (std::min(xl, xr) >= t.rect.left)
                    p.drawLine(QPointF(clipL, y - cap), QPointF(clipL, y + cap));
                if (std::max(xl, xr) <= t.rect.right)
                    p.drawLine(QPointF(clipR, y - cap), QPointF(clipR, y + cap));
            }
            // 被切掉的那一端畫箭頭，否則看起來像區間就是停在圖的邊緣
            if (showClippedArrows) {
                const double a = 4 * rp.scaleFactor;
                p.setBrush(color);
                if (std::min(xl, xr) < t.rect.left) {
                    QPolygonF tip;
                    tip << QPointF(t.rect.left, y) << QPointF(t.rect.left + 2 * a, y - a)
                        << QPointF(t.rect.left + 2 * a, y + a);
                    p.drawPolygon(tip);
                }
                if (std::max(xl, xr) > t.rect.right) {
                    QPolygonF tip;
                    tip << QPointF(t.rect.right, y) << QPointF(t.rect.right - 2 * a, y - a)
                        << QPointF(t.rect.right - 2 * a, y + a);
                    p.drawPolygon(tip);
                }
            }

            // 方塊按「面積」比例縮放
            double side = maxBoxSizePx;
            if (maxWeight > 0 && r.weight > 0)
                side = minBoxSizePx +
                       (maxBoxSizePx - minBoxSizePx) * std::sqrt(r.weight / maxWeight);
            side *= rp.scaleFactor;
            const QPointF c(t.px(r.estimate), y);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRect(QRectF(c.x() - side / 2, c.y() - side / 2, side, side));
        }

        if (!r.annotation.isEmpty()) {
            Label lb;
            lb.text = r.annotation;
            lb.font = annotationFont;
            lb.font.size *= rp.scaleFactor;
            if (r.isSummary) lb.font.bold = true;
            drawText(p, QPointF(t.rect.right - annotationOffsetPx * rp.scaleFactor, y), lb,
                     Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    p.restore();
}


// ───────────────────────── Bland–Altman ─────────────────────────

BlandAltman blandAltman(const QVector<double>& methodA, const QVector<double>& methodB,
                        double agreementZ) {
    BlandAltman out;
    const int n = static_cast<int>(std::min(methodA.size(), methodB.size()));
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(methodA[i]) || !std::isfinite(methodB[i])) continue;
        out.means.append((methodA[i] + methodB[i]) / 2.0);
        out.differences.append(methodA[i] - methodB[i]);
    }
    out.count = static_cast<int>(out.differences.size());
    if (out.count == 0) return out;

    double sum = 0;
    for (double d : out.differences) sum += d;
    out.bias = sum / out.count;
    if (out.count > 1) {
        double ss = 0;
        for (double d : out.differences) ss += (d - out.bias) * (d - out.bias);
        out.sd = std::sqrt(ss / (out.count - 1));
    }
    out.lowerLimit = out.bias - agreementZ * out.sd;
    out.upperLimit = out.bias + agreementZ * out.sd;
    return out;
}

// ───────────────────── 後設分析的漏斗圖 ─────────────────────

FunnelContour funnelContour(const QVector<MetaStudy>& studies, int samples, double z) {
    FunnelContour out;
    QVector<MetaStudy> v;
    for (const MetaStudy& s : studies)
        if (std::isfinite(s.effect) && s.standardError > 0) v.append(s);
    if (v.size() < 2) return out;

    double sumW = 0, sumWY = 0, maxSe = 0;
    for (const MetaStudy& s : v) {
        const double w = 1.0 / (s.standardError * s.standardError);
        sumW += w;
        sumWY += w * s.effect;
        maxSe = std::max(maxSe, s.standardError);
    }
    out.pooledEffect = sumWY / sumW;

    // 漏斗：以合併值為中心，寬度隨 SE 線性放大
    const int n = std::max(2, samples);
    out.seLevels.reserve(n);
    out.leftBound95.reserve(n);
    out.rightBound95.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double se = maxSe * 1.1 * i / (n - 1);
        out.seLevels.append(se);
        out.leftBound95.append(out.pooledEffect - z * se);
        out.rightBound95.append(out.pooledEffect + z * se);
    }

    // Egger 迴歸：標準常態離差對精確度。
    // 直接把「效應量對 SE」做迴歸是不對的 —— 那條迴歸線的殘差變異不是常數，
    // 小研究會被過度加權。Egger 的轉換把它變成普通的最小平方問題。
    QVector<double> xs, ys;
    xs.reserve(v.size());
    ys.reserve(v.size());
    for (const MetaStudy& s : v) {
        xs.append(1.0 / s.standardError);
        ys.append(s.effect / s.standardError);
    }
    const LinearFit fit = linearFit(xs, ys);
    out.eggerIntercept = fit.intercept;
    out.eggerSlope = fit.slope;
    return out;
}

// ───────────────────────── 勞倫茲曲線 ─────────────────────────

LorenzCurve lorenzCurve(const QVector<double>& values) {
    LorenzCurve out;
    QVector<double> v;
    for (double x : values)
        if (std::isfinite(x) && x >= 0) v.append(x);
    if (v.isEmpty()) return out;
    std::sort(v.begin(), v.end());

    double total = 0;
    for (double x : v) total += x;

    const int n = static_cast<int>(v.size());
    out.populationFraction.reserve(n + 1);
    out.valueFraction.reserve(n + 1);
    out.populationFraction.append(0.0);
    out.valueFraction.append(0.0);

    double run = 0;
    for (int i = 0; i < n; ++i) {
        run += v[i];
        out.populationFraction.append(static_cast<double>(i + 1) / n);
        out.valueFraction.append(total > 0 ? run / total : 0.0);
    }

    // 吉尼 = 1 − 2·(曲線下面積)。面積用梯形法，和畫出來的折線是同一條，
    // 所以圖上看到的和算出來的數字一定一致。
    const double area = trapezoidArea(out.populationFraction, out.valueFraction);
    out.gini = std::clamp(1.0 - 2.0 * area, 0.0, 1.0);
    return out;
}

// ───────────────────────── 機率紙 ─────────────────────────

ProbabilityPlot probabilityPlot(const QVector<double>& values, ProbabilityPaper paper,
                                PlottingPosition pp) {
    ProbabilityPlot out;
    QVector<double> v;
    for (double x : values) {
        if (!std::isfinite(x)) continue;
        // Weibull 紙要取 ln(x)，所以非正值進不來
        if (paper == ProbabilityPaper::Weibull && !(x > 0)) continue;
        v.append(x);
    }
    if (v.size() < 2) return out;
    std::sort(v.begin(), v.end());

    const int n = static_cast<int>(v.size());
    out.xs.reserve(n);
    out.ys.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double rank = i + 1;
        double prob = 0;
        switch (pp) {
        case PlottingPosition::Blom:    prob = (rank - 0.375) / (n + 0.25); break;
        case PlottingPosition::Hazen:   prob = (rank - 0.5) / n; break;
        case PlottingPosition::Weibull: prob = rank / (n + 1.0); break;
        }
        prob = std::clamp(prob, 1e-9, 1 - 1e-9);

        if (paper == ProbabilityPaper::Normal) {
            out.xs.append(v[i]);
            out.ys.append(normalQuantile(prob));
        } else {
            out.xs.append(std::log(v[i]));
            out.ys.append(std::log(-std::log(1.0 - prob)));
        }
    }

    // 參考線一樣穿過四分位數，不用最小平方 —— 理由和 Q-Q 圖相同：
    // 機率紙正是用來找尾端偏離的，迴歸線會被尾端的點拉走。
    QVector<double> sx = out.xs, sy = out.ys;
    std::sort(sx.begin(), sx.end());
    std::sort(sy.begin(), sy.end());
    const double x1 = sampleQuantile(sx, 0.25), x3 = sampleQuantile(sx, 0.75);
    const double y1 = sampleQuantile(sy, 0.25), y3 = sampleQuantile(sy, 0.75);
    if (std::isfinite(x1) && std::isfinite(x3) && x3 > x1) {
        out.referenceLine.slope = (y3 - y1) / (x3 - x1);
        out.referenceLine.intercept = y1 - out.referenceLine.slope * x1;
        out.referenceLine.count = n;
        out.referenceLine.rSquared = 1;
    }

    if (paper == ProbabilityPaper::Weibull && out.referenceLine.isValid() &&
        out.referenceLine.slope > 0) {
        // Weibull 紙上 y = β·ln(x) − β·ln(η)，所以斜率就是 β，
        // 而 η = exp(−截距 / β)。
        out.weibullShape = out.referenceLine.slope;
        out.weibullScale = std::exp(-out.referenceLine.intercept / out.referenceLine.slope);
    }
    return out;
}

}  // namespace qtplot

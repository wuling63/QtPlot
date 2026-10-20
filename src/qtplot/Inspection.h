// Inspection.h — 視覺檢測與分類評估：混淆矩陣、每類的精確率／召回率、OK／NG 門檻統計。
//
// 異常熱圖疊在影像上不必另外的圖元：ImageRect 放原圖，上面疊一張 Heatmap
// 並設 hideBelow（低於門檻的格子透明），再加一條 ContourLines 描出門檻的輪廓即可。
#pragma once

#include "Plottable.h"

namespace qtplot {

// ───────────────────────── 分類評估 ─────────────────────────

/// 一個類別的評估數字。
struct ClassMetrics {
    double precision = 0;  ///< 判成這類的裡面，真的是這類的比例
    double recall = 0;     ///< 真的是這類的裡面，有被判出來的比例
    double f1 = 0;         ///< precision 與 recall 的調和平均
    int support = 0;       ///< 真的是這類的樣本數（那一列的總和）
};

/// 由混淆矩陣（counts[實際][預測]）算每一類的數字。分母為 0 時該項是 0。
QVector<ClassMetrics> classMetrics(const QVector<QVector<double>>& counts);
/// 正確率：對角線總和 ÷ 全部。
double accuracy(const QVector<QVector<double>>& counts);
/// 由逐筆的實際類別與預測類別數出混淆矩陣。超出 [0, classCount) 的筆數略過。
QVector<QVector<double>> confusionCounts(const QVector<int>& actual, const QVector<int>& predicted, int classCount);

/// OK／NG 二分類在某個門檻的統計。NG 是陽性：分數 >= 門檻就判 NG。
struct ThresholdStats {
    int tp = 0;  ///< NG 判成 NG（抓到）
    int fn = 0;  ///< NG 判成 OK（漏檢，最危險的那種錯）
    int tn = 0;  ///< OK 判成 OK
    int fp = 0;  ///< OK 判成 NG（誤判，過殺）
    double missRate = 0;         ///< 漏檢率 = fn / NG 總數
    double falseAlarmRate = 0;   ///< 誤判率 = fp / OK 總數
    double precision = 0;        ///< 判 NG 的裡面真的是 NG 的比例
    double accuracy = 0;
    /// 對應的混淆矩陣：列是實際 {OK, NG}，欄是預測 {OK, NG}。
    QVector<QVector<double>> counts() const { return {{double(tn), double(fp)}, {double(fn), double(tp)}}; }
};
ThresholdStats thresholdStats(const QVector<double>& okScores, const QVector<double>& ngScores, double threshold);

// ───────────────────────── 混淆矩陣 ─────────────────────────

/// 混淆矩陣：列是實際類別、欄是預測類別，第 0 列畫在最上面。
///
/// 格子的顏色看的是正規化之後的值，所以「依列正規化」時每一列各自從淺到深，
/// 樣本數少的類別也看得出它的錯誤集中在哪一欄 —— 不正規化的話，大類別的格子
/// 會把色階整個佔走，小類別全是淺色，看不出問題。
class ConfusionMatrix : public IPlottable {
public:
    enum class Normalize {
        None,    ///< 原始筆數
        Row,     ///< 每列加總為 1：對角線就是各類的召回率
        Column,  ///< 每欄加總為 1：對角線就是各類的精確率
        All      ///< 全部加總為 1
    };

    QStringList labels;
    QVector<QVector<double>> counts;  ///< counts[實際][預測]
    Normalize normalize = Normalize::None;
    Colormap colormap{Colormap::Blues};
    bool showCounts = true;     ///< 格子裡寫原始筆數
    bool showPercent = true;    ///< 格子裡寫百分比（Normalize::None 時是佔全部的比例）
    bool highlightDiagonal = true;
    LineStyle diagonalBorder{QColor(30, 30, 30), 1.5};
    LineStyle gridLine{QColor(255, 255, 255), 1.0};
    FontStyle cellFont;

    ConfusionMatrix();
    int size() const { return static_cast<int>(counts.size()); }
    /// 正規化之後的值（給上色與百分比用）。
    double normalized(int row, int col) const;

    AxisLimits axisLimits() const override;
    void render(RenderPack& rp) override;
    void visitFonts(const FontVisitor& visit) override { visit(cellFont); }
};

}  // namespace qtplot

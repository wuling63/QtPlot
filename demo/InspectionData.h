// InspectionData.h — 視覺檢測範例用的合成資料：帶瑕疵的金屬表面影像、異常熱圖、OK／NG 分數。
//
// 範例要能獨立跑，所以不讀真的資料集。換成自己的資料時：影像換成相機拍的圖，
// anomaly 換成模型（例如 EfficientAD）輸出的異常圖，分數換成每張圖的異常分數即可。
#pragma once

#include <QImage>
#include <QString>
#include <QVector>

namespace qtplot {
namespace demo {

/// 一張檢測影像與它的異常熱圖。
struct InspectionSample {
    QString name;               ///< 給下拉選單用（原文，顯示時用 RT 翻）
    QImage image;               ///< 320 × 240 的灰階表面
    QVector<double> anomaly;    ///< rows × cols，row-major，第 0 列對到影像最上面
    int rows = 48;
    int cols = 64;
    bool defective = false;
};

/// 共 5 張：0、1 是良品，2 是刮痕，3 是黑點，4 是兩個亮點凹坑。
InspectionSample makeInspectionSample(int index);
int inspectionSampleCount();

/// 一批產品的異常分數：良品 400 片、不良品 120 片。分布有重疊，門檻放哪都會有取捨。
struct InspectionScores {
    QVector<double> ok;
    QVector<double> ng;
};
InspectionScores makeInspectionScores();

}  // namespace demo
}  // namespace qtplot

// DataGen.h — 範例資料產生器，對應 ScottPlot 的 Generate 類別。
//
// 隨機數列預設用固定種子，所以同一支範例每次跑出來的圖都一樣
// （截圖比對、文件、回歸測試都需要這點）。
#pragma once

#include <QVector>

namespace qtplot {
namespace gen {

/// 0, 1, 2, ... count-1（乘上 step 再加 offset）。
QVector<double> consecutive(int count, double step = 1.0, double offset = 0.0);

/// 從 start 到 stop 的等距 count 個點（含兩端）。
QVector<double> linspace(double start, double stop, int count);

/// 從 start 開始、間隔 step、小於 stop 的序列。
QVector<double> range(double start, double stop, double step = 1.0);

/// 正弦波。oscillations 是整段資料裡的週期數。
QVector<double> sin(int count, double amplitude = 1.0, double oscillations = 1.0,
                    double phase = 0.0, double offset = 0.0);

/// 餘弦波。
QVector<double> cos(int count, double amplitude = 1.0, double oscillations = 1.0,
                    double phase = 0.0, double offset = 0.0);

/// 均勻分布隨機數 [min, max)。
QVector<double> randomUniform(int count, double min = 0.0, double max = 1.0);

/// 常態分布隨機數。
QVector<double> randomNormal(int count, double mean = 0.0, double stdDev = 1.0);

/// 隨機漫步（累加常態雜訊），適合當作「看起來像真資料」的訊號。
QVector<double> randomWalk(int count, double mult = 1.0, double start = 0.0);

/// 在既有資料上加常態雜訊（回傳新陣列）。
QVector<double> addNoise(const QVector<double>& values, double stdDev = 0.1);

/// 遞增的隨機整數（適合當 X 軸）。
QVector<double> randomSample(int count, double min = 0.0, double max = 100.0);

/// 2D 高斯山丘，給 Heatmap 用。回傳 row-major 的 rows * cols 陣列。
QVector<double> gaussian2D(int rows, int cols, double sigma = 0.3);

/// 兩個維度上都是 sin 的干涉圖樣，給 Heatmap / Contour 用。
QVector<double> sin2D(int rows, int cols, double oscillations = 2.0);

/// 重設隨機種子。傳 0 用時間當種子（結果就不可重現了）。
void setSeed(unsigned int seed);

/// 把亂數引擎倒回程式剛啟動時的狀態。
///
/// 不能用 setSeed(0) 代替：那個 0 的意思是「用時鐘當種子」，正好相反。
/// 要讓每張圖的資料和「前面跑過什麼」無關，用這個。
void resetSeed();

}  // namespace gen
}  // namespace qtplot

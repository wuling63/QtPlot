// SharedAxesWindow.h — 共用座標軸的示範視窗。
//
// 這個不能做成一般的 Cookbook 範例：範例只拿得到一個 Plot（或一個 PlotWidget），
// 而「共用座標軸」本質上需要兩張圖。所以獨立成一個視窗，從 Cookbook 的檢視選單開啟。
#pragma once

#include "qtplot/PlotWidget.h"

#include <QWidget>

class QCheckBox;

namespace qtplot {
namespace demo {

/// 上下兩張圖，可以用核取方塊決定是否共用 X 軸與 Y 軸。
/// 共用時在任一張圖上平移、縮放，另一張會同步。
class SharedAxesWindow : public QWidget {
    Q_OBJECT

public:
    explicit SharedAxesWindow(bool darkTheme = false, QWidget* parent = nullptr);

private:
    void relink();

    PlotWidget* m_top = nullptr;
    PlotWidget* m_bottom = nullptr;
    QCheckBox* m_shareX = nullptr;
    QCheckBox* m_shareY = nullptr;
};

}  // namespace demo
}  // namespace qtplot

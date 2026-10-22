// LegendStyleDialog.h — 讓使用者自己調圖例外觀的對話框。
//
// 外框（顏色、粗細、線型）、底色要不要塗滿、色塊符號塗滿或只畫框、字型與文字顏色。
// 改了馬上套到圖上（透過 changed 訊號重畫），按「取消」還原成打開對話框時的樣子。
// PlotWidget 的右鍵選單「圖例樣式…」用的就是它；LegendPanel 的 style 也可以直接丟進來。
#pragma once

#include "Plot.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace qtplot {

class LegendStyleDialog : public QDialog {
    Q_OBJECT

public:
    /// legend 要活得比對話框久。每改一項就直接寫進 legend，再發 changed。
    explicit LegendStyleDialog(Legend& legend, QWidget* parent = nullptr);

signals:
    /// legend 被改了，該重畫了。
    void changed();

private:
    void loadFromLegend();
    void apply();
    void restoreDefaults();
    void pickColor(QPushButton* button, QColor& color, const QString& title);
    void pickFont();
    void updateEnabled();
    static void setSwatch(QPushButton* button, const QColor& color);

    Legend& m_legend;
    const Legend m_original;  ///< 取消時還原用

    // 對話框自己記著的顏色：關掉外框或底色時，原本挑的顏色不能丟
    QColor m_borderColor;
    QColor m_backgroundColor;
    QColor m_symbolOutlineColor;
    QColor m_textColor;
    FontStyle m_font;
    bool m_loading = false;

    QCheckBox* m_borderOn = nullptr;
    QPushButton* m_borderColorButton = nullptr;
    QDoubleSpinBox* m_borderWidth = nullptr;
    QComboBox* m_borderPattern = nullptr;

    QCheckBox* m_backgroundOn = nullptr;
    QPushButton* m_backgroundColorButton = nullptr;

    QCheckBox* m_fillSymbols = nullptr;
    QCheckBox* m_symbolOutlineOn = nullptr;
    QPushButton* m_symbolOutlineColorButton = nullptr;
    QDoubleSpinBox* m_symbolOutlineWidth = nullptr;

    QPushButton* m_fontButton = nullptr;
    QPushButton* m_textColorButton = nullptr;
};

}  // namespace qtplot

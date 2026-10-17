// MainWindow.h — Cookbook 瀏覽器。
//
// 左邊是章節／分類／範例的樹，右邊上半是圖、下半是該支範例的原始碼。
// 選到任一支範例就當場重建一張 Plot，所以看到的圖和貼出去的程式一定一致。
#pragma once

#include "Recipe.h"
#include "qtplot/Multiplot.h"
#include "qtplot/PlotWidget.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTimer;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace qtplot {
namespace demo {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /// 依名稱選取範例（命令列參數用）。找不到回傳 false。
    bool selectRecipe(const QString& name);

    /// 範例程式自己的介面文字（選單、搜尋框、狀態列）跟著 QtPlot 目前的語言載入翻譯。
    /// QtPlot 的語言換了（選單列或圖上的右鍵選單都可能換）就要再叫一次。
    static void syncDemoTranslator();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onTreeSelectionChanged();
    void onFilterChanged(const QString& text);
    void onCursorMoved(qtplot::Coordinates coords);

private:
    void buildMenus();
    void buildLanguageMenu(class QMenu* menu);
    /// 換語言之後重建所有介面文字。範例清單（範例的名稱與說明）不翻譯，維持中文。
    void retranslateUi();
    void showStartupHint();
    void buildTree();
    void showRecipe(const Recipe* recipe);
    void applyTheme(bool dark);
    void exportAllRecipes();

    QTreeWidget* m_tree = nullptr;
    QLineEdit* m_filter = nullptr;
    QLabel* m_description = nullptr;
    QPlainTextEdit* m_source = nullptr;
    // 圖的區域是一個 QStackedWidget：單張圖用 PlotWidget，子圖範例用 MultiplotWidget。
    // 兩者沒有共同基底，而且 MultiplotWidget 刻意不做互動，硬塞進同一個類別
    // 只會讓「這張圖能不能拖」變成要看狀態才知道。
    QStackedWidget* m_plotStack = nullptr;
    PlotWidget* m_plotWidget = nullptr;
    MultiplotWidget* m_multiWidget = nullptr;
    QLabel* m_cursorLabel = nullptr;
    const Recipe* m_current = nullptr;
    bool m_darkTheme = false;
    QTimer* m_liveTimer = nullptr;
    int m_liveFrame = 0;
    /// 換語言時翻譯器會一個一個裝上去（函式庫、主視窗、範例），每裝一個就發一次 LanguageChange。
    /// 合併成一次重建，不然清單和範例會連續重建好幾次。
    bool m_retranslatePending = false;
};

}  // namespace demo
}  // namespace qtplot

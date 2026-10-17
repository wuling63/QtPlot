// Recipe.h — Cookbook 的範例註冊機制。
//
// 一支範例長這樣：
//
//   QTPLOT_RECIPE("入門", "快速上手", "散點圖", "任意 (x, y) 點加上折線與標記。") {
//       plot.addScatter({1, 2, 3, 4}, {1, 4, 9, 16});
//   }
//
// 巨集會把函式登記進全域清單，所以新增範例不用改任何其他檔案。
// 範例的原始碼會從 Qt 資源讀回來顯示在畫面上，程式與文件不會脫節。
#pragma once

#include "qtplot/DataGen.h"
#include "qtplot/Multiplot.h"
#include "qtplot/PlotWidget.h"

#include <QString>
#include <QVector>

namespace qtplot {
namespace demo {

using RecipeFunc = void (*)(qtplot::Plot& plot);
/// 會動的範例。frame == 0 代表「建立」，之後每次計時器觸發就 +1。
using RecipeTickFunc = void (*)(qtplot::Plot& plot, int frame);
/// 需要視窗層級功能的範例（框選、游標吸附、拖曳）。這些不是 Plot 的設定，
/// 是 PlotWidget 的 —— 所以範例拿到的是 widget，自己從 widget.plot() 取圖。
using RecipeWidgetFunc = void (*)(qtplot::PlotWidget& widget);
/// 子圖範例。拿到的是整張畫布，格子自己用 at() / span() 取。
using RecipeMultiFunc = void (*)(qtplot::Multiplot& mp);

struct Recipe {
    QString chapter;      ///< 第一層分類
    QString category;     ///< 第二層分類
    QString name;         ///< 範例名稱
    QString description;  ///< 一兩句說明，顯示在圖的上方
    QString sourceFile;   ///< __FILE__，只取檔名用來查資源
    int sourceLine = 0;   ///< 巨集所在行，用來擷取函式本體
    RecipeFunc build = nullptr;
    /// 會動的範例才有。靜態的範例是 nullptr。
    RecipeTickFunc tick = nullptr;
    /// 計時器間隔（毫秒）。0 代表不是會動的範例。
    int tickMs = 0;
    /// 需要 PlotWidget 的範例才有。
    RecipeWidgetFunc widgetFn = nullptr;
    /// 子圖範例才有。
    RecipeMultiFunc multiFn = nullptr;

    bool isLive() const { return tick != nullptr && tickMs > 0; }
    bool needsWidget() const { return widgetFn != nullptr; }
    bool needsMultiplot() const { return multiFn != nullptr; }

    /// 每支範例開跑前把亂數種子歸零。
    ///
    /// DataGen 的亂數引擎是全域的。不歸零的話，一支範例畫出什麼會取決於
    /// 「它前面跑過哪些範例」—— 在清單中間插入一支新範例，後面所有沒有自己
    /// 呼叫 setSeed() 的範例，圖都會跟著變。
    ///
    /// 那會讓 --export 的雜湊比對失去意義：明明只加了一支範例，卻有七張圖
    /// 跟著變動，分不出哪些是真的回歸。（這就是實際發生過的事。）
    /// 範例自己呼叫 setSeed() 仍然有效，只是不再影響別人。
    ///
    /// 注意是 resetSeed() 不是 setSeed(0)：後者的 0 代表「用時鐘當種子」，
    /// 用錯的話每次匯出的圖都不一樣，視覺回歸就徹底沒得比了。
    static void resetRandomSeed() { qtplot::gen::resetSeed(); }

    /// 建立這張圖。靜態與會動的範例都走這裡。
    void run(qtplot::Plot& plot) const {
        resetRandomSeed();
        if (build) build(plot);
        else if (tick) tick(plot, 0);
    }

    /// 需要 PlotWidget 的範例走這裡。和 run() 一樣會先歸零種子 ——
    /// 呼叫端直接呼叫 widgetFn 的話就漏掉了，所以統一包起來。
    void runWidget(qtplot::PlotWidget& widget) const {
        resetRandomSeed();
        if (widgetFn) widgetFn(widget);
    }

    /// 子圖範例走這裡。同樣先歸零種子。
    void runMultiplot(qtplot::Multiplot& mp) const {
        resetRandomSeed();
        if (multiFn) multiFn(mp);
    }

    /// 從 Qt 資源取出這支範例的原始碼（巨集那行起算到大括號平衡為止）。
    QString sourceCode() const;
};

class RecipeRegistry {
public:
    static RecipeRegistry& instance();

    /// 回傳值沒有意義，只是為了能在靜態初始化階段呼叫。
    int add(const Recipe& recipe);

    const QVector<Recipe>& recipes() const { return m_recipes; }
    QStringList chapters() const;
    QStringList categories(const QString& chapter) const;
    QVector<const Recipe*> inCategory(const QString& chapter, const QString& category) const;
    const Recipe* find(const QString& chapter, const QString& category, const QString& name) const;

private:
    QVector<Recipe> m_recipes;
};

}  // namespace demo
}  // namespace qtplot

// ── 巨集機械 ──
#define QTPLOT_CAT_(a, b) a##b
#define QTPLOT_CAT(a, b) QTPLOT_CAT_(a, b)

#define QTPLOT_RECIPE(chapter_, category_, name_, description_)                       \
    static void QTPLOT_CAT(qtplot_recipe_fn_, __LINE__)(qtplot::Plot& plot);          \
    static const int QTPLOT_CAT(qtplot_recipe_reg_, __LINE__) =                       \
        qtplot::demo::RecipeRegistry::instance().add(                                 \
            {QStringLiteral(chapter_), QStringLiteral(category_),                     \
             QStringLiteral(name_), QStringLiteral(description_),                     \
             QStringLiteral(__FILE__), __LINE__,                                      \
             &QTPLOT_CAT(qtplot_recipe_fn_, __LINE__)});                              \
    static void QTPLOT_CAT(qtplot_recipe_fn_, __LINE__)(qtplot::Plot& plot)

/// 會動的範例。函式簽章多一個 frame：0 是建立，之後每 intervalMs_ 毫秒加一。
/// 匯出 PNG 時會先跑一段 frame 產生有內容的靜態畫面，GUI 裡則由計時器真的跑起來，
/// 兩邊是同一份程式。
#define QTPLOT_RECIPE_LIVE(chapter_, category_, name_, description_, intervalMs_)     \
    static void QTPLOT_CAT(qtplot_live_fn_, __LINE__)(qtplot::Plot& plot, int frame); \
    static const int QTPLOT_CAT(qtplot_live_reg_, __LINE__) =                         \
        qtplot::demo::RecipeRegistry::instance().add(                                 \
            {QStringLiteral(chapter_), QStringLiteral(category_),                     \
             QStringLiteral(name_), QStringLiteral(description_),                     \
             QStringLiteral(__FILE__), __LINE__,                                      \
             nullptr, &QTPLOT_CAT(qtplot_live_fn_, __LINE__), intervalMs_});          \
    static void QTPLOT_CAT(qtplot_live_fn_, __LINE__)(qtplot::Plot& plot, int frame)

/// 需要視窗層級功能的範例（框選、游標吸附、拖曳）。拿到的是 PlotWidget，
/// 圖從 widget.plot() 取。匯出 PNG 時會建一個不顯示的 PlotWidget 跑同一份程式。
#define QTPLOT_RECIPE_WIDGET(chapter_, category_, name_, description_)                  \
    static void QTPLOT_CAT(qtplot_wfn_, __LINE__)(qtplot::PlotWidget& widget);          \
    static const int QTPLOT_CAT(qtplot_wreg_, __LINE__) =                               \
        qtplot::demo::RecipeRegistry::instance().add(                                   \
            {QStringLiteral(chapter_), QStringLiteral(category_),                       \
             QStringLiteral(name_), QStringLiteral(description_),                       \
             QStringLiteral(__FILE__), __LINE__,                                        \
             nullptr, nullptr, 0, &QTPLOT_CAT(qtplot_wfn_, __LINE__)});                 \
    static void QTPLOT_CAT(qtplot_wfn_, __LINE__)(qtplot::PlotWidget& widget)

/// 子圖範例。拿到的是整張畫布（Multiplot），用 at(row, col) 取每一格的 Plot。
/// 匯出 PNG 與 GUI 走的是同一份程式。
#define QTPLOT_RECIPE_MULTI(chapter_, category_, name_, description_)                    \
    static void QTPLOT_CAT(qtplot_mfn_, __LINE__)(qtplot::Multiplot& mp);                \
    static const int QTPLOT_CAT(qtplot_mreg_, __LINE__) =                                \
        qtplot::demo::RecipeRegistry::instance().add(                                    \
            {QStringLiteral(chapter_), QStringLiteral(category_),                        \
             QStringLiteral(name_), QStringLiteral(description_),                        \
             QStringLiteral(__FILE__), __LINE__,                                         \
             nullptr, nullptr, 0, nullptr, &QTPLOT_CAT(qtplot_mfn_, __LINE__)});         \
    static void QTPLOT_CAT(qtplot_mfn_, __LINE__)(qtplot::Multiplot& mp)

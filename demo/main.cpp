// main.cpp — Cookbook 瀏覽器的進入點。
//
// 用法：
//   QtPlotDemo                      開啟瀏覽器
//   QtPlotDemo --recipe "散點圖"     開啟並直接跳到該支範例
//   QtPlotDemo --export <資料夾>     不開視窗，把全部範例存成 PNG
//   QtPlotDemo --lang en            QtPlot 的介面文字（右鍵選單、預設軸名）換成英文
//                                   可用：en zh_CN ja ko fr de it es zh_TW；開著之後也能在右鍵選單「語言」裡換
//
// 開視窗時預設跟著系統語言；--export／--soak 預設不翻譯，產出的圖才不會因機器而異。
#include "MainWindow.h"
#include "Recipe.h"
#include "qtplot/Translation.h"

#include <QApplication>
#include <QDir>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

using namespace qtplot;

namespace {

/// 不開視窗，直接把每支範例畫成 PNG。給批次產圖或 CI 做視覺回歸用。
int exportAll(const QString& dir) {
    QTextStream out(stdout);
    if (!QDir().mkpath(dir)) {
        out << QStringLiteral("建立資料夾失敗: %1\n").arg(dir);
        return 1;
    }
    const QVector<demo::Recipe>& recipes = demo::RecipeRegistry::instance().recipes();
    int ok = 0;
    for (const demo::Recipe& r : recipes) {
        if (!r.build && !r.tick && !r.needsWidget() && !r.needsMultiplot()) continue;
        QString stem = QStringLiteral("%1_%2_%3").arg(r.chapter, r.category, r.name);
        stem.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
        const QString path = QDir(dir).filePath(stem + QStringLiteral(".png"));

        bool written = false;
        if (r.needsMultiplot()) {
            // 子圖範例畫的是整張畫布，不是單一張 Plot。
            // 各格的 autoScale 由 Multiplot::render 之前的共用設定處理，
            // 沒設共用的格子就靠自己（Plot::render 會跑 m_autoScalePending）。
            Multiplot mp;
            r.runMultiplot(mp);
            for (Plot* sub : mp.plots()) sub->queueAutoScale();
            written = mp.savePng(path, 800, 600, 2.0);
        } else {
            // 視窗層級的範例要有 PlotWidget 才設定得起來。不顯示它，只借它的 Plot。
            PlotWidget tmp;
            Plot& p = tmp.plot();
            if (r.needsWidget()) r.runWidget(tmp);
            else r.run(p);
            // 會動的範例先跑一段 frame，匯出的圖才有內容（和 GUI 是同一份程式）
            if (r.isLive())
                for (int f = 1; f <= 240; ++f) r.tick(p, f);
            // autoScale 是逐軸判斷的：範例用 setAxisLimits 或 setRange 指定過的軸會
            // 自己擋下來，沒指定的軸才會貼合資料。所以這裡無條件呼叫即可。
            p.autoScale();
            written = p.savePng(path, 800, 600, 2.0);
        }

        if (written) {
            ++ok;
        } else {
            out << QStringLiteral("寫入失敗: %1\n").arg(path);
        }
    }
    out << QStringLiteral("已輸出 %1 / %2 張圖到 %3\n").arg(ok).arg(recipes.size()).arg(dir);
    return ok == recipes.size() ? 0 : 2;
}

/// 照 GUI 的方式，把全部範例依序丟進「同一個」PlotWidget，檢查狀態有沒有殘留。
///
/// 為什麼要有這個：--export 每張圖都開一個新的 PlotWidget，永遠是乾淨的，
/// 所以它驗證不到「換圖」這條路徑。真正會重複使用同一個 Plot 的是 GUI ——
/// 殘留的狀態（最明顯的是 addAxis() 加出來的軸一張一張疊上去）只在那裡看得到。
/// 這個模式把那條路徑補上，順便確認反覆重建不會踩到已釋放的圖元。
int soakAll() {
    QTextStream out(stdout);
    const QVector<demo::Recipe>& recipes = demo::RecipeRegistry::instance().recipes();

    PlotWidget widget;
    widget.resize(800, 600);
    const int baselineAxes = widget.plot().axes().size();

    int leaks = 0;
    for (const demo::Recipe& r : recipes) {
        if (!r.build && !r.tick && !r.needsWidget() && !r.needsMultiplot()) continue;

        // 和 MainWindow::showRecipe 同一個順序
        widget.resetPlot();
        Plot& p = widget.plot();

        const int axesAfterReset = p.axes().size();
        const bool clean = axesAfterReset == baselineAxes && p.plottables().isEmpty();
        if (!clean) {
            ++leaks;
            out << QStringLiteral("殘留: %1 / %2 / %3 — 軸 %4（應為 %5）、圖元 %6\n")
                         .arg(r.chapter, r.category, r.name)
                         .arg(axesAfterReset)
                         .arg(baselineAxes)
                         .arg(p.plottables().size());
        }

        if (r.needsMultiplot()) {
            // 子圖範例每次都開一張新畫布 —— 它不共用那個 PlotWidget 的 Plot，
            // 所以殘留檢查看的是「前一支範例有沒有把 widget 弄髒」，那上面已經驗過了。
            Multiplot mp;
            r.runMultiplot(mp);
            for (Plot* sub : mp.plots()) sub->queueAutoScale();
            mp.renderToImage(800, 600, 1.0);
            continue;
        }
        if (r.needsWidget()) r.runWidget(widget);
        else r.run(p);
        if (r.isLive())
            for (int f = 1; f <= 30; ++f) r.tick(p, f);
        p.autoScale();

        // 真的畫一次。殘留的懸空指標在這裡才會發作。
        p.renderToImage(800, 600, 1.0);
    }

    out << QStringLiteral("換圖測試：%1 支範例，殘留 %2 次\n").arg(recipes.size()).arg(leaks);
    return leaks == 0 ? 0 : 3;
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QtPlot Cookbook"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    const QStringList args = QApplication::arguments();

    // 語言要在任何圖建立之前決定：預設軸名是在 add*() 的當下寫進去的
    const int langIdx = args.indexOf(QStringLiteral("--lang"));
    const bool batch = args.contains(QStringLiteral("--export")) || args.contains(QStringLiteral("--soak"));
    if (langIdx >= 0 && langIdx + 1 < args.size())
        qtplot::setLanguage(args.at(langIdx + 1));
    else if (!batch)
        qtplot::setLanguage(QString());  // 跟著系統

    // --export 要在建視窗之前處理：這個模式不需要視窗
    const int exportIdx = args.indexOf(QStringLiteral("--export"));
    if (exportIdx >= 0) {
        const QString dir = (exportIdx + 1 < args.size()) ? args.at(exportIdx + 1)
                                                          : QStringLiteral("recipes-png");
        return exportAll(dir);
    }

    // --soak 同樣不需要視窗
    if (args.contains(QStringLiteral("--soak"))) return soakAll();

    demo::MainWindow window;
    window.show();

    const int recipeIdx = args.indexOf(QStringLiteral("--recipe"));
    if (recipeIdx >= 0 && recipeIdx + 1 < args.size())
        window.selectRecipe(args.at(recipeIdx + 1));

    return app.exec();
}

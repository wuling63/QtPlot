#include "MainWindow.h"

#include "InspectionWindow.h"
#include "SharedAxesWindow.h"
#include "qtplot/Translation.h"

#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace qtplot {
namespace demo {

namespace {
constexpr int kRoleChapter = Qt::UserRole + 1;
constexpr int kRoleCategory = Qt::UserRole + 2;
constexpr int kRoleName = Qt::UserRole + 3;

// 範例程式自己的翻譯（和 QtPlot 函式庫的分開：函式庫的翻譯編在函式庫裡，使用者的程式不必帶這份）。
// 兩份：demo_* 是主視窗的介面（選單、搜尋框），recipes_* 是範例的內容（清單上的名稱、圖上的字）。
QTranslator* g_demoTranslators[2] = {nullptr, nullptr};
QString g_demoLanguage = QStringLiteral("zh_TW");  // 目前載入的是哪一種
}  // namespace

void MainWindow::syncDemoTranslator() {
    const QString want = qtplot::effectiveLanguage();
    if (want == g_demoLanguage) return;
    g_demoLanguage = want;
    for (QTranslator*& t : g_demoTranslators) {
        if (!t) continue;
        QCoreApplication::removeTranslator(t);
        delete t;
        t = nullptr;
    }
    if (want == QLatin1String("zh_TW")) return;  // 原文
    const QString files[2] = {QStringLiteral(":/demo/i18n/demo_%1.qm").arg(want),
                              QStringLiteral(":/demo/i18n/recipes_%1.qm").arg(want)};
    for (int i = 0; i < 2; ++i) {
        auto* t = new QTranslator(qApp);
        if (t->load(files[i])) {
            QCoreApplication::installTranslator(t);
            g_demoTranslators[i] = t;
        } else {
            delete t;
        }
    }
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    syncDemoTranslator();
    setWindowTitle(tr("QtPlot Cookbook — Qt/QPainter 互動式繪圖"));
    resize(1280, 860);

    // ── 左欄：搜尋 + 樹 ──
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("搜尋範例…"));
    m_filter->setClearButtonEnabled(true);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(240);

    auto* leftBox = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftBox);
    leftLayout->setContentsMargins(6, 6, 3, 6);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(m_filter);
    leftLayout->addWidget(m_tree, 1);

    // ── 右欄：說明 + 圖 + 原始碼 ──
    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setTextFormat(Qt::PlainText);
    m_description->setContentsMargins(4, 4, 4, 4);

    m_plotWidget = new PlotWidget(this);
    m_plotWidget->plot().legend.isVisible = false;
    // 游標靠近資料點時顯示數值；設了 draggable 的圖元可以直接拖
    m_plotWidget->showTooltipOnHover = true;

    m_multiWidget = new MultiplotWidget(this);
    m_plotStack = new QStackedWidget(this);
    m_plotStack->addWidget(m_plotWidget);
    m_plotStack->addWidget(m_multiWidget);

    m_source = new QPlainTextEdit(this);
    m_source->setReadOnly(true);
    m_source->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSizeF(10);
    m_source->setFont(mono);
    m_source->setTabStopDistance(4 * QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')));

    auto* rightSplit = new QSplitter(Qt::Vertical, this);
    auto* topBox = new QWidget(this);
    auto* topLayout = new QVBoxLayout(topBox);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(2);
    topLayout->addWidget(m_description);
    topLayout->addWidget(m_plotStack, 1);
    rightSplit->addWidget(topBox);
    rightSplit->addWidget(m_source);
    rightSplit->setStretchFactor(0, 3);
    rightSplit->setStretchFactor(1, 1);

    auto* mainSplit = new QSplitter(Qt::Horizontal, this);
    mainSplit->addWidget(leftBox);
    mainSplit->addWidget(rightSplit);
    mainSplit->setStretchFactor(0, 0);
    mainSplit->setStretchFactor(1, 1);
    setCentralWidget(mainSplit);

    m_cursorLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_cursorLabel);

    // 會動的範例由這個計時器推進；靜態的範例不會啟動它
    m_liveTimer = new QTimer(this);
    connect(m_liveTimer, &QTimer::timeout, this, [this] {
        if (!m_current || !m_current->isLive()) return;
        m_current->tick(m_plotWidget->plot(), ++m_liveFrame);
        m_plotWidget->refresh();
    });

    buildMenus();
    buildTree();

    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::onTreeSelectionChanged);
    connect(m_filter, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(m_plotWidget, &PlotWidget::mouseMoved, this, &MainWindow::onCursorMoved);
    connect(m_plotWidget, &PlotWidget::pointClicked, this, [this](const HitResult& hit) {
        statusBar()->showMessage(tr("點到第 %1 筆：x = %2, y = %3")
                                     .arg(hit.index)
                                     .arg(hit.point.x, 0, 'g', 6)
                                     .arg(hit.point.y, 0, 'g', 6),
                                 5000);
    });

    // 預設選第一支範例，開起來不要是空白
    if (m_tree->topLevelItemCount() > 0) {
        QTreeWidgetItem* chapter = m_tree->topLevelItem(0);
        chapter->setExpanded(true);
        if (chapter->childCount() > 0) {
            QTreeWidgetItem* category = chapter->child(0);
            category->setExpanded(true);
            if (category->childCount() > 0) m_tree->setCurrentItem(category->child(0));
        }
    }

    showStartupHint();
}

void MainWindow::showStartupHint() {
    const int n = RecipeRegistry::instance().recipes().size();
    statusBar()->showMessage(tr("共 %1 支範例。滑鼠：左鍵拖曳平移、右鍵拖曳縮放、"
                                "中鍵框選、滾輪縮放、左鍵雙擊自動範圍。").arg(n));
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && !m_retranslatePending) {
        // 語言可能是從圖上的右鍵選單換的：範例程式自己的翻譯要跟上，並記住這次的選擇。
        // 等這一輪事件處理完再重建 —— 翻譯器一個一個裝上去，每個都會再發一次 LanguageChange
        m_retranslatePending = true;
        QTimer::singleShot(0, this, [this] {
            m_retranslatePending = false;
            syncDemoTranslator();
            QSettings().setValue(QStringLiteral("language"), qtplot::language());
            retranslateUi();
        });
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("QtPlot Cookbook — Qt/QPainter 互動式繪圖"));
    m_filter->setPlaceholderText(tr("搜尋範例…"));
    menuBar()->clear();
    buildMenus();
    showStartupHint();

    // 範例清單換成新語言，選取停在同一支範例上；選取一變就會重跑範例，圖上的字跟著換
    const Recipe* current = m_current;
    m_tree->blockSignals(true);
    buildTree();
    onFilterChanged(m_filter->text());
    m_tree->blockSignals(false);
    if (current) {
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
            QTreeWidgetItem* item = *it;
            if (item->data(0, kRoleName).toString() == current->name &&
                item->data(0, kRoleChapter).toString() == current->chapter &&
                item->data(0, kRoleCategory).toString() == current->category) {
                m_tree->setCurrentItem(item);
                break;
            }
        }
        showRecipe(current);
    }
}

void MainWindow::buildLanguageMenu(QMenu* menu) {
    auto* group = new QActionGroup(menu);
    const bool followSystem = qtplot::language().isEmpty();
    auto add = [&](const QString& label, const QString& code, bool checked) {
        QAction* a = menu->addAction(label);
        a->setCheckable(true);
        a->setChecked(checked);
        group->addAction(a);
        // 換語言之後 QtPlot 會發 LanguageChange，changeEvent 再把整個介面重建；
        // 這裡不能直接重建選單 —— 這個 action 正在被觸發，當場刪掉會出事
        connect(a, &QAction::triggered, this, [code] { qtplot::setLanguage(code); });
    };
    add(tr("跟隨系統"), QString(), followSystem);
    menu->addSeparator();
    // 語言名稱一律用該語言自己寫，選錯語言時也認得出要切回哪一個
    for (const QString& code : qtplot::availableLanguages())
        add(qtplot::languageDisplayName(code), code, !followSystem && qtplot::effectiveLanguage() == code);
}

void MainWindow::buildMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("檔案(&F)"));
    fileMenu->addAction(tr("複製圖片(&C)"), QKeySequence::Copy,
                        this, [this] { m_plotWidget->copyToClipboard(); });
    fileMenu->addAction(tr("另存圖片(&S)…"), QKeySequence::Save,
                        this, [this] { m_plotWidget->saveImageDialog(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("匯出全部範例為 PNG(&E)…"), this, [this] { exportAllRecipes(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("結束(&X)"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* viewMenu = menuBar()->addMenu(tr("檢視(&V)"));
    viewMenu->addAction(tr("自動範圍(&A)"), QKeySequence(Qt::Key_F5), this, [this] {
        m_plotWidget->plot().autoScale(0.05, true);
        m_plotWidget->refresh();
    });
    QAction* dark = viewMenu->addAction(tr("深色主題(&D)"));
    dark->setCheckable(true);
    dark->setChecked(m_darkTheme);  // 換語言時選單會重建，勾選狀態要沿用
    connect(dark, &QAction::toggled, this, [this](bool on) { applyTheme(on); });

    QAction* bench = viewMenu->addAction(tr("顯示效能(&B)"));
    bench->setCheckable(true);
    bench->setChecked(m_plotWidget->showBenchmark);
    connect(bench, &QAction::toggled, this, [this](bool on) {
        m_plotWidget->showBenchmark = on;
        m_plotWidget->refresh();
    });

    viewMenu->addSeparator();
    viewMenu->addAction(tr("共用座標軸示範(&S)…"), this, [this] {
        // 共用座標軸需要兩張圖，一支範例只拿得到一個，所以獨立成一個視窗
        auto* w = new SharedAxesWindow(m_darkTheme);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    });

    // 語言：介面文字（範例程式與 QtPlot 的選單、軸名）即時切換，下次開啟會記住
    buildLanguageMenu(menuBar()->addMenu(tr("語言(&L)")));

    QMenu* helpMenu = menuBar()->addMenu(tr("說明(&H)"));
    viewMenu->addAction(tr("瑕疵檢測門檻示範(&I)…"), this, [this] {
        // 四張圖跟著同一個門檻聯動，一支範例只有一個 PlotWidget 做不到，所以是獨立視窗
        auto* w = new InspectionWindow(m_darkTheme);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    });
    helpMenu->addAction(tr("關於(&A)…"), this, [this] {
        QMessageBox::information(
            this, tr("關於 QtPlot"),
            tr("QtPlot — 用 Qt Widgets 與 QPainter 自製的互動式繪圖函式庫。\n\n"
               "繪圖核心不依賴 Qt Charts，座標換算、刻度、版面與互動都自己實作，"
               "所以圖種與互動行為完全可控。\n\n"
               "共 %1 支範例。")
                .arg(RecipeRegistry::instance().recipes().size()));
    });
}

void MainWindow::buildTree() {
    m_tree->clear();
    const RecipeRegistry& reg = RecipeRegistry::instance();
    for (const QString& chapter : reg.chapters()) {
        auto* chapterItem = new QTreeWidgetItem(m_tree);
        chapterItem->setText(0, rt(chapter));
        QFont f = chapterItem->font(0);
        f.setBold(true);
        chapterItem->setFont(0, f);
        chapterItem->setFlags(Qt::ItemIsEnabled);  // 章節不可選

        for (const QString& category : reg.categories(chapter)) {
            auto* categoryItem = new QTreeWidgetItem(chapterItem);
            categoryItem->setText(0, rt(category));
            categoryItem->setFlags(Qt::ItemIsEnabled);

            for (const Recipe* r : reg.inCategory(chapter, category)) {
                auto* recipeItem = new QTreeWidgetItem(categoryItem);
                recipeItem->setText(0, rt(r->name));
                recipeItem->setData(0, kRoleChapter, r->chapter);
                recipeItem->setData(0, kRoleCategory, r->category);
                recipeItem->setData(0, kRoleName, r->name);
            }
        }
    }
    m_tree->expandAll();
}

void MainWindow::onTreeSelectionChanged() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    const QString name = item->data(0, kRoleName).toString();
    if (name.isEmpty()) return;  // 點到章節或分類
    const Recipe* r = RecipeRegistry::instance().find(item->data(0, kRoleChapter).toString(),
                                                      item->data(0, kRoleCategory).toString(),
                                                      name);
    showRecipe(r);
}

void MainWindow::showRecipe(const Recipe* recipe) {
    // 只檢查 recipe 本身。不能檢查 build ——
    // 會動的範例（build 是 nullptr，靠 tick）、需要 widget 的範例、子圖範例
    // 三種的 build 都是空的，擋掉的話它們在 GUI 裡永遠顯示不出來，
    // 畫面上留著的是上一支範例的圖，看起來像點了沒反應。
    if (!recipe) return;
    m_current = recipe;

    m_liveTimer->stop();
    m_liveFrame = 0;
    m_description->setText(recipe->description);
    m_source->setPlainText(recipe->sourceCode());

    if (recipe->needsMultiplot()) {
        // 子圖範例：換成整張畫布那個 widget。
        // 一樣先倒回全新狀態，理由和單張圖的 resetPlot() 相同。
        m_multiWidget->resetMultiplot();
        Multiplot& mp = m_multiWidget->multiplot();
        recipe->runMultiplot(mp);
        const PlotStyle st = m_darkTheme ? PlotStyle::dark() : PlotStyle::light();
        mp.background = st.figureBackground;
        mp.title.font.color = st.textColor;
        for (Plot* sub : mp.plots()) sub->queueAutoScale();
        m_plotStack->setCurrentWidget(m_multiWidget);
        m_multiWidget->refresh();
        return;
    }

    m_plotStack->setCurrentWidget(m_plotWidget);

    // 換範例之前先倒回全新狀態。
    //
    // 這裡以前是逐項復原（清標題、換回自動刻度、把 inverted / locked 設回 false…），
    // 問題是那份清單永遠會漏：它完全沒碰 addAxis() 加出來的軸，所以「同一側兩根軸」
    // 這類範例看完之後，多出來的刻度會一路留到後面每一張圖上，愈疊愈多。
    // 復原的責任交給 Plot 自己，範例新增什麼都不必再回來補這份清單。
    m_plotWidget->resetPlot();
    Plot& plot = m_plotWidget->plot();
    plot.setStyle(m_darkTheme ? PlotStyle::dark() : PlotStyle::light());

    // autoScale 是逐軸判斷的：範例用 setAxisLimits 或 setRange 指定過的軸會自己擋下來，
    // 沒指定的軸才會貼合資料。所以這裡無條件呼叫即可。
    // 視窗層級的範例自己拿 widget 去設定（框選、吸附、拖曳）
    m_plotWidget->selectionMode = false;
    m_plotWidget->clearSelection();
    if (recipe->needsWidget()) recipe->runWidget(*m_plotWidget);
    else recipe->run(plot);
    plot.autoScale();

    // 會動的範例：建立完成後啟動計時器
    if (recipe->isLive()) m_liveTimer->start(recipe->tickMs);

    m_plotWidget->refresh();
}

void MainWindow::onFilterChanged(const QString& text) {
    const QString needle = text.trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* chapter = m_tree->topLevelItem(i);
        bool chapterVisible = false;
        for (int j = 0; j < chapter->childCount(); ++j) {
            QTreeWidgetItem* category = chapter->child(j);
            bool categoryVisible = false;
            for (int k = 0; k < category->childCount(); ++k) {
                QTreeWidgetItem* recipe = category->child(k);
                // 比對範例名、分類名與章節名，三者任一命中就留著
                const bool match =
                    needle.isEmpty() ||
                    recipe->text(0).contains(needle, Qt::CaseInsensitive) ||
                    category->text(0).contains(needle, Qt::CaseInsensitive) ||
                    chapter->text(0).contains(needle, Qt::CaseInsensitive);
                recipe->setHidden(!match);
                categoryVisible = categoryVisible || match;
            }
            category->setHidden(!categoryVisible);
            chapterVisible = chapterVisible || categoryVisible;
        }
        chapter->setHidden(!chapterVisible);
    }
    if (!needle.isEmpty()) m_tree->expandAll();
}

void MainWindow::onCursorMoved(Coordinates coords) {
    if (!std::isfinite(coords.x) || !std::isfinite(coords.y)) {
        m_cursorLabel->clear();
        return;
    }
    m_cursorLabel->setText(QStringLiteral("x = %1    y = %2")
                               .arg(coords.x, 0, 'g', 6)
                               .arg(coords.y, 0, 'g', 6));
}

void MainWindow::applyTheme(bool dark) {
    m_darkTheme = dark;
    showRecipe(m_current);
}

bool MainWindow::selectRecipe(const QString& name) {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* chapter = m_tree->topLevelItem(i);
        for (int j = 0; j < chapter->childCount(); ++j) {
            QTreeWidgetItem* category = chapter->child(j);
            for (int k = 0; k < category->childCount(); ++k) {
                QTreeWidgetItem* recipe = category->child(k);
                if (recipe->text(0).compare(name, Qt::CaseInsensitive) == 0 ||
                    recipe->data(0, kRoleName).toString().compare(name, Qt::CaseInsensitive) == 0) {
                    m_tree->setCurrentItem(recipe);
                    return true;
                }
            }
        }
    }
    return false;
}

void MainWindow::exportAllRecipes() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("選擇輸出資料夾"));
    if (dir.isEmpty()) return;

    const QVector<Recipe>& recipes = RecipeRegistry::instance().recipes();
    QProgressDialog progress(tr("匯出範例…"), tr("取消"), 0, recipes.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    int ok = 0;
    for (int i = 0; i < recipes.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) break;
        const Recipe& r = recipes.at(i);
        if (!r.build && !r.tick && !r.needsWidget() && !r.needsMultiplot()) continue;

        PlotWidget tmp;
        Plot& p = tmp.plot();
        p.setStyle(m_darkTheme ? PlotStyle::dark() : PlotStyle::light());
        if (r.needsWidget()) r.runWidget(tmp);
        else r.run(p);
        // 會動的範例先跑一段，匯出的圖才有內容
        if (r.isLive())
            for (int f = 1; f <= 240; ++f) r.tick(p, f);
        p.autoScale();

        // 檔名用「章節_分類_名稱」，路徑不合法的字元換掉
        QString stem = QStringLiteral("%1_%2_%3").arg(r.chapter, r.category, r.name);
        stem.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
        if (p.savePng(QDir(dir).filePath(stem + QStringLiteral(".png")), 800, 600, 2.0)) ++ok;
    }
    progress.setValue(recipes.size());
    statusBar()->showMessage(tr("已匯出 %1 / %2 張圖到 %3").arg(ok).arg(recipes.size()).arg(dir),
                             8000);
}

}  // namespace demo
}  // namespace qtplot

// widget_test.cpp — PlotWidget 的互動檢查：翻譯、列印、視野動畫、手勢。
//
// selftest 只有 QGuiApplication，碰不到 QWidget；這幾項要真的建一個 widget 才測得到。
// 視窗設成 WA_DontShowOnScreen：isVisible() 為真（動畫才會跑），但不會閃出畫面。
//
// 用法: qtplot_widget_test [暫存資料夾]   全部通過回傳 0，否則回傳失敗數
#include "qtplot/PlotWidget.h"
#include "qtplot/Translation.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <QPrinter>
#include <QTest>
#include <QTextStream>
#include <QThread>
#include <QWheelEvent>

#include <atomic>
#include <cmath>
#include <functional>

using namespace qtplot;

namespace {

int g_failures = 0;

QTextStream& out() {
    static QTextStream s(stdout);
    return s;
}

void check(bool ok, const QString& what, const QString& detail = QString()) {
    out() << (ok ? "  ok   " : "  FAIL ") << what;
    if (!ok) {
        ++g_failures;
        if (!detail.isEmpty()) out() << "   (" << detail << ")";
    }
    out() << "\n";
}

bool close(double a, double b, double tol = 1e-9) { return std::abs(a - b) <= tol * (1 + std::abs(a) + std::abs(b)); }

/// 一直處理事件直到條件成立或逾時。
bool waitFor(const std::function<bool()>& cond, int timeoutMs = 3000) {
    QElapsedTimer t;
    t.start();
    while (!cond()) {
        if (t.elapsed() > timeoutMs) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QTest::qWait(5);
    }
    return true;
}

/// 一張有資料、已經畫過一次（有 lastDataRect）的圖。
void prepare(PlotWidget& w) {
    w.resetPlot();
    QVector<double> xs, ys;
    for (int i = 0; i <= 100; ++i) {
        xs.append(i);
        ys.append(std::sin(i * 0.1) * 10);
    }
    w.plot().addScatter(xs, ys);
    w.plot().autoScale();
    w.repaint();
    QCoreApplication::processEvents();
}

QString limitsText(const AxisLimits& l) {
    return QStringLiteral("x[%1, %2] y[%3, %4]").arg(l.x.min).arg(l.x.max).arg(l.y.min).arg(l.y.max);
}

// ───────────────────────────── 翻譯 ─────────────────────────────

void testTranslation() {
    out() << "翻譯\n";
    const QStringList langs = availableLanguages();
    for (const char* code : {"zh_TW", "en", "zh_CN", "ja", "ko", "fr", "de", "it", "es"})
        check(langs.contains(QLatin1String(code)), QStringLiteral("有 %1（%2）").arg(QLatin1String(code), languageDisplayName(QLatin1String(code))));

    // 每一種翻譯都要真的不一樣，且整份都翻了（隨便抽兩條：一條非 QObject、一條選單）
    for (const QString& code : langs) {
        if (code == QLatin1String("zh_TW")) continue;
        check(setLanguage(code), QStringLiteral("載入 %1").arg(code));
        check(effectiveLanguage() == code, QStringLiteral("%1：effectiveLanguage 正確").arg(code), effectiveLanguage());
        const QString axis = QCoreApplication::translate("qtplot", "累積百分比");
        const QString menu = QCoreApplication::translate("qtplot::PlotWidget", "語言(&N)");
        check(axis != QStringLiteral("累積百分比") && menu != QStringLiteral("語言(&N)"),
              QStringLiteral("%1：字串有翻（%2／%3）").arg(code, axis, menu));
    }
    check(setLanguage(QStringLiteral("en_US")) && effectiveLanguage() == QLatin1String("en"), "en_US 退回 en");
    check(QCoreApplication::translate("qtplot", "人", "fishbone 6M: Man") == QStringLiteral("Man"),
          "同一個字靠註解分辨（魚骨圖 6M）");
    check(!setLanguage(QStringLiteral("zh_TW")), "繁體中文是原始語言，不必載入");
    check(!setLanguage(QStringLiteral("zh_HK")) && effectiveLanguage() == QLatin1String("zh_TW"), "zh_HK 也用原文");
    check(!setLanguage(QStringLiteral("ru")) && effectiveLanguage() == QLatin1String("zh_TW"), "沒有的語言維持繁體中文");
    check(setLanguage(QStringLiteral("zh_CN")) &&
              QCoreApplication::translate("qtplot", "累積百分比") == QStringLiteral("累积百分比"),
          "简体中文");

    // ── 畫好的圖跟著換 ──
    out() << "切換語言時畫好的圖跟著換\n";
    setLanguage(QStringLiteral("zh_TW"));
    Plot p;
    Pareto* pr = p.addPareto({{QStringLiteral("A"), 5, QColor()}, {QStringLiteral("B"), 3, QColor()}});
    Fishbone* fb = nullptr;
    {
        Plot q;  // 用不到，只是確認另一張圖不會互相干擾
        q.addNormalQQ({1, 2, 3, 4, 5});
        setLanguage(QStringLiteral("de"));
        q.renderToImage(200, 150);
        check(q.leftAxis()->label.text == QStringLiteral("Stichprobenquantile"), "另一張圖也換了");
        setLanguage(QStringLiteral("zh_TW"));
    }
    Plot f;
    fb = f.addFishbone(QStringLiteral("良率低"), Fishbone::sixM());
    p.renderToImage(400, 300);
    f.renderToImage(400, 300);
    check(pr->cumulativeAxis->label.text == QStringLiteral("累積百分比"), "一開始是繁體中文");

    setLanguage(QStringLiteral("fr"));
    p.renderToImage(400, 300);
    f.renderToImage(400, 300);
    check(pr->cumulativeAxis->label.text == QStringLiteral("Pourcentage cumulé"), "換成法文後，下一次畫就是法文",
          pr->cumulativeAxis->label.text);
    check(fb->categories.size() == 6 && fb->categories[0].name == QStringLiteral("Main-d'œuvre") &&
              fb->categories[5].name == QStringLiteral("Mesure"),
          "魚骨圖的 6M 也換了", fb->categories.isEmpty() ? QString() : fb->categories[0].name);

    // 使用者自己改過的不能被蓋掉
    pr->cumulativeAxis->label.text = QStringLiteral("我的軸名");
    fb->categories[1].name = QStringLiteral("設備");
    setLanguage(QStringLiteral("ja"));
    p.renderToImage(400, 300);
    f.renderToImage(400, 300);
    check(pr->cumulativeAxis->label.text == QStringLiteral("我的軸名"), "使用者改過的軸名不會被翻掉");
    check(fb->categories[1].name == QStringLiteral("設備") && fb->categories[2].name == QStringLiteral("材料"),
          "使用者改過的類別不動，其他照翻");

    // 圖元被移除、類別被刪掉之後再換語言，不能碰到已經不存在的東西
    fb->categories.resize(2);
    f.renderToImage(400, 300);
    setLanguage(QStringLiteral("es"));
    f.renderToImage(400, 300);
    check(fb->categories[0].name == QStringLiteral("Mano de obra"), "類別被刪掉一部分之後，留下的照翻");
    f.remove(fb);
    setLanguage(QStringLiteral("it"));
    f.renderToImage(400, 300);
    check(true, "圖元移除後換語言不會當掉");

    setLanguage(QStringLiteral("zh_TW"));
    p.renderToImage(400, 300);
    check(pr->cumulativeAxis->label.text == QStringLiteral("我的軸名"), "換回繁體中文也不動使用者的字");

    // 月份／星期跟著 QtPlot 的語言，不動整個程式的 QLocale
    const QLocale before = QLocale();
    setLanguage(QStringLiteral("de"));
    check(displayLocale().language() == QLocale::German, "displayLocale 跟著換");
    check(QLocale() == before, "沒有動到程式全域的 QLocale");

    // PlotWidget 收到 LanguageChange 會重畫
    {
        PlotWidget w;
        w.setAttribute(Qt::WA_DontShowOnScreen);
        w.resize(300, 200);
        w.plot().addNormalQQ({1, 2, 3, 4, 5});
        w.show();
        w.repaint();
        setLanguage(QStringLiteral("ko"));
        QCoreApplication::processEvents();  // LanguageChange 是 post 的
        waitFor([&] { return w.plot().leftAxis()->label.text == QStringLiteral("표본 분위수"); }, 1000);
        check(w.plot().leftAxis()->label.text == QStringLiteral("표본 분위수"), "PlotWidget 換語言後自動重畫成新語言",
              w.plot().leftAxis()->label.text);
    }

    // 跟著系統
    setLanguage(QString());
    check(language().isEmpty(), "空字串 = 跟著系統");
    setLanguage(QStringLiteral("zh_TW"));
}

// ───────────────────────────── 列印 ─────────────────────────────

void testPrint(PlotWidget& w, const QString& dir) {
    out() << "列印\n";
    prepare(w);
    w.plot().title.text = QStringLiteral("列印測試");
    w.repaint();
    const PixelRect before = w.plot().lastDataRect();

    const QString path = dir + QStringLiteral("/widget_print_test.pdf");
    QFile::remove(path);
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageOrientation(QPageLayout::Landscape);
    w.print(printer);

    QFile f(path);
    const bool opened = f.open(QIODevice::ReadOnly);
    const QByteArray head = opened ? f.read(5) : QByteArray();
    const qint64 size = f.size();
    f.close();
    check(opened && head == "%PDF-", "印成 PDF");
    check(size > 3000, "PDF 有內容", QString::number(size));

    const PixelRect after = w.plot().lastDataRect();
    check(close(before.left, after.left) && close(before.width(), after.width()) &&
              close(before.height(), after.height()),
          "列印後 lastDataRect 和螢幕一致（滑鼠座標換算不會亂掉）");
    QFile::remove(path);
}

// ───────────────────────────── 動畫 ─────────────────────────────

void testAnimation(PlotWidget& w) {
    out() << "視野動畫\n";
    prepare(w);
    w.animationDurationMs = 200;

    w.plot().setAxisLimits(40, 50, -1, 1);  // 先縮到一小塊
    w.clearViewHistory();
    w.repaint();
    const AxisLimits zoomed = w.plot().axisLimits();

    // 自動範圍的終點
    w.plot().autoScale(0.05, true);
    const AxisLimits target = w.plot().axisLimits();
    w.plot().setAxisLimits(zoomed);

    int changed = 0;
    auto conn = QObject::connect(&w, &PlotWidget::axisLimitsChanged, [&] { ++changed; });

    w.autoScaleAnimated();
    check(w.isAnimating(), "自動範圍會動畫");
    check(close(w.plot().axisLimits().x.min, zoomed.x.min), "動畫剛開始時視野還在原處");

    // 中途取樣：應該在起點與終點之間
    QTest::qWait(60);
    const AxisLimits mid = w.plot().axisLimits();
    check(mid.x.min < zoomed.x.min && mid.x.min > target.x.min, "動畫中途的視野介於起點與終點之間",
          limitsText(mid));
    check(changed == 0, "動畫中途不發 axisLimitsChanged");

    check(waitFor([&] { return !w.isAnimating(); }), "動畫會結束");
    const AxisLimits end = w.plot().axisLimits();
    check(close(end.x.min, target.x.min) && close(end.x.max, target.x.max) && close(end.y.min, target.y.min) &&
              close(end.y.max, target.y.max),
          "結束時剛好停在終點", limitsText(end) + " vs " + limitsText(target));
    check(changed == 1, "結束時發一次 axisLimitsChanged", QString::number(changed));
    QObject::disconnect(conn);

    // 復原：動畫走回去，結束時在原本的視野
    check(w.canUndoView(), "自動範圍記進了歷程");
    w.undoView();
    check(w.isAnimating(), "復原也會動畫");
    waitFor([&] { return !w.isAnimating(); });
    check(close(w.plot().axisLimits().x.min, zoomed.x.min) && close(w.plot().axisLimits().x.max, zoomed.x.max),
          "復原回到自動範圍之前的視野");
    w.redoView();
    waitFor([&] { return !w.isAnimating(); });
    check(close(w.plot().axisLimits().x.min, target.x.min), "重做回到自動範圍之後");

    // 中途按下滑鼠：停在當下，不跳到終點
    w.plot().setAxisLimits(zoomed);
    w.autoScaleAnimated();
    QTest::qWait(50);
    const QPoint c = w.rect().center();
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, c);
    const AxisLimits grabbed = w.plot().axisLimits();
    check(!w.isAnimating(), "動畫中途按下滑鼠會停掉動畫");
    check(grabbed.x.min < zoomed.x.min && grabbed.x.min > target.x.min, "停在當下的位置，不跳到終點",
          limitsText(grabbed));
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, c);

    // 關掉動畫：直接跳
    w.animationDurationMs = 0;
    w.plot().setAxisLimits(zoomed);
    w.autoScaleAnimated();
    check(!w.isAnimating() && close(w.plot().axisLimits().x.min, target.x.min), "animationDurationMs = 0 直接跳到終點");

    // animateTo
    w.animationDurationMs = 150;
    w.animateTo(AxisLimits(10, 20, -2, 2));
    waitFor([&] { return !w.isAnimating(); });
    const AxisLimits at = w.plot().axisLimits();
    check(close(at.x.min, 10) && close(at.x.max, 20) && close(at.y.min, -2) && close(at.y.max, 2), "animateTo 停在指定的視野");
    check(close(w.plot().topAxis()->range.min, 10), "上軸跟著底軸");
}

// ───────────────────────────── 手勢 ─────────────────────────────

void testGestures(PlotWidget& w) {
    out() << "手勢\n";
    w.animationDurationMs = 0;

    // ── 觸控板的原生捏合（macOS／Linux）──
    prepare(w);
    w.plot().setAxisLimits(0, 100, -10, 10);
    w.repaint();
    const PixelRect d = w.plot().lastDataRect();
    const QPointF center((d.left + d.right) / 2, (d.top + d.bottom) / 2);
    const double cx = w.plot().bottomAxis()->getCoordinate(center.x(), d);
    {
        QNativeGestureEvent ev(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(), 2, center, center,
                               w.mapToGlobal(center), 1.0 /* +100% = 兩倍 */, QPointF());
        QCoreApplication::sendEvent(&w, &ev);
    }
    const AxisLimits z = w.plot().axisLimits();
    check(close(z.x.span(), 50, 1e-6) && close(z.y.span(), 10, 1e-6), "原生捏合 value = 1 放大兩倍（兩軸一起）",
          limitsText(z));
    check(close(w.plot().bottomAxis()->getCoordinate(center.x(), d), cx, 1e-6), "以手勢中心為定點");

    // ── Windows 精確式觸控板：捏合被送成 Ctrl + 捲動 ──
    QPointingDevice touchpad(QStringLiteral("test touchpad"), 4242, QInputDevice::DeviceType::TouchPad,
                             QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 2, 0);
    auto ctrlWheel = [&](const QPointingDevice* dev) {
        QWheelEvent ev(center, w.mapToGlobal(center), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
                       Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized, dev);
        QCoreApplication::sendEvent(&w, &ev);
    };
    w.plot().setAxisLimits(0, 100, -10, 10);
    ctrlWheel(&touchpad);
    const AxisLimits tp = w.plot().axisLimits();
    check(tp.x.span() < 100 && tp.y.span() < 20, "觸控板 Ctrl + 捲動：兩軸一起縮放", limitsText(tp));

    w.plot().setAxisLimits(0, 100, -10, 10);
    ctrlWheel(QPointingDevice::primaryPointingDevice());
    const AxisLimits ms = w.plot().axisLimits();
    check(close(ms.x.span(), 100) && ms.y.span() < 20, "滑鼠 Ctrl + 滾輪：照舊只縮 Y", limitsText(ms));

    // ── 觸控螢幕的兩指捏合 ──
    w.plot().setAxisLimits(0, 100, -10, 10);
    w.repaint();
    QPointingDevice* touch = QTest::createTouchDevice();
    const QPoint c = center.toPoint();
    QTest::touchEvent(&w, touch).press(0, c - QPoint(40, 0)).press(1, c + QPoint(40, 0));
    QCoreApplication::processEvents();
    for (int step = 1; step <= 6; ++step) {
        QTest::touchEvent(&w, touch).move(0, c - QPoint(40 + step * 15, 0)).move(1, c + QPoint(40 + step * 15, 0));
        QCoreApplication::processEvents();
    }
    QTest::touchEvent(&w, touch).release(0, c - QPoint(130, 0)).release(1, c + QPoint(130, 0));
    QCoreApplication::processEvents();
    const AxisLimits pz = w.plot().axisLimits();
    check(pz.x.span() < 60, "兩指張開：放大", limitsText(pz));
    check(std::abs(pz.x.center() - 50) < 1, "放大的中心在兩指中間（對稱張開不會平移）", limitsText(pz));

    // 單指拖曳：平移
    w.plot().setAxisLimits(0, 100, -10, 10);
    QTest::touchEvent(&w, touch).press(0, c);
    QTest::touchEvent(&w, touch).move(0, c + QPoint(100, 0));
    QTest::touchEvent(&w, touch).release(0, c + QPoint(100, 0));
    QCoreApplication::processEvents();
    const AxisLimits pan = w.plot().axisLimits();
    check(pan.x.min < 0 && close(pan.x.span(), 100), "單指往右拖：視野左移、寬度不變", limitsText(pan));

    // 點兩下：自動範圍
    w.plot().setAxisLimits(40, 45, -1, 1);
    for (int i = 0; i < 2; ++i) {
        QTest::touchEvent(&w, touch).press(0, c);
        QTest::touchEvent(&w, touch).release(0, c);
    }
    QCoreApplication::processEvents();
    check(w.plot().axisLimits().x.span() > 90, "點兩下：自動範圍", limitsText(w.plot().axisLimits()));
    check(w.canUndoView(), "捏合結束記進歷程，可以 Ctrl + Z 回去");

    // 關掉手勢
    w.gesturesEnabled = false;
    w.plot().setAxisLimits(0, 100, -10, 10);
    {
        QNativeGestureEvent ev(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(), 2, center, center,
                               w.mapToGlobal(center), 1.0, QPointF());
        QCoreApplication::sendEvent(&w, &ev);
    }
    check(close(w.plot().axisLimits().x.span(), 100), "gesturesEnabled = false 時不理手勢");
    w.gesturesEnabled = true;
}


// ───────────────────────────── 分層快取與自動重畫 ─────────────────────────────

/// 數自己被畫了幾次的圖元。
class CountingPlottable : public IPlottable {
public:
    int renders = 0;
    AxisLimits axisLimits() const override { return AxisLimits(0, 1, 0, 1); }
    void render(RenderPack&) override { ++renders; }
};

void testLayerCache(PlotWidget& w) {
    out() << "分層快取\n";
    prepare(w);
    w.animationDurationMs = 0;
    w.showTooltipOnHover = true;
    auto* counter = w.plot().add(std::make_unique<CountingPlottable>());
    w.refresh();
    w.repaint();
    const int base = counter->renders;
    check(base >= 1, "整張畫過一次");

    // 滑鼠在資料點之間移動：提示框一直換，但圖本身不該重畫
    const PixelRect d = w.plot().lastDataRect();
    for (int i = 0; i < 30; ++i) {
        const QPointF pos(d.left + 20 + i * 15, (d.top + d.bottom) / 2);
        QMouseEvent ev(QEvent::MouseMove, pos, w.mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &ev);
        // 讓 widget 自己要求的重畫跑完。不能直接呼叫 repaint()：那是「外面」要求的重畫，
        // 分不出只有疊加層在變，照設計會整張重畫
        QCoreApplication::processEvents();
    }
    check(counter->renders == base, "滑鼠移動只換提示框，沒有整張重畫",
          QStringLiteral("多畫了 %1 次").arg(counter->renders - base));

    // 外面直接要求重畫（QWidget::update／repaint）：一律整張畫，寧可多畫也不畫錯
    w.repaint();
    check(counter->renders == base + 1, "外部的 repaint() 整張重畫");

    // refresh() 一定整張重畫
    w.refresh();
    QCoreApplication::processEvents();
    check(counter->renders == base + 2, "refresh() 整張重畫");

    // 程式改了視野卻沒呼叫 refresh()，接著使用者拖框選框（只請求疊加層）：
    // 指紋發現軸範圍變了，還是要整張重畫，不能貼舊的快取
    w.plot().setAxisLimits(0, 50, -5, 5);
    w.selectionMode = true;
    {
        const QPoint c0 = w.rect().center();
        QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, c0);
        QTest::mouseMove(&w, c0 + QPoint(20, 20));  // 框選框在動：requestOverlay
        QCoreApplication::processEvents();
        check(counter->renders == base + 3, "軸範圍改了，即使只請求疊加層也整張重畫（指紋擋住）",
              QString::number(counter->renders - base));
        const int during = counter->renders;
        QTest::mouseMove(&w, c0 + QPoint(40, 30));  // 再拖：這次圖沒變，該沿用快取
        QCoreApplication::processEvents();
        check(counter->renders == during, "拖框選框時沿用快取");
        QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, c0 + QPoint(40, 30));
        QCoreApplication::processEvents();
    }
    w.selectionMode = false;

    // 平移一定重畫
    const int beforePan = counter->renders;
    const QPoint c = w.rect().center();
    QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, c);
    QTest::mouseMove(&w, c + QPoint(30, 0));
    w.repaint();
    QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, c + QPoint(30, 0));
    check(counter->renders > beforePan, "平移時整張重畫");

    // 關掉快取：每次都畫
    w.layerCacheEnabled = false;
    const int beforeOff = counter->renders;
    w.repaint();
    w.repaint();
    check(counter->renders == beforeOff + 2, "layerCacheEnabled = false 時每次都整張畫");
    w.layerCacheEnabled = true;
    w.showTooltipOnHover = false;

    out() << "自動重畫（背景執行緒餵資料）\n";
    prepare(w);
    auto* counter2 = w.plot().add(std::make_unique<CountingPlottable>());
    DataStreamer* s = w.plot().addDataStreamer(1000);
    w.repaint();
    w.setAutoRefresh(20);
    QTest::qWait(120);
    const int idle = counter2->renders;
    QTest::qWait(150);
    check(counter2->renders == idle, "沒有新資料時不重畫", QStringLiteral("多畫了 %1 次").arg(counter2->renders - idle));

    std::atomic<bool> stop{false};
    QThread* producer = QThread::create([&] {
        double ph = 0;
        while (!stop) {
            s->add(std::sin(ph += 0.05));
            QThread::msleep(1);
        }
    });
    producer->start();
    QTest::qWait(300);
    stop = true;
    producer->wait();
    delete producer;
    const int fed = counter2->renders - idle;
    check(fed >= 5, "背景執行緒餵資料時，GUI 執行緒自動重畫", QStringLiteral("%1 次").arg(fed));
    check(fed <= 30, "重畫次數受間隔限制（不是每筆資料都畫）", QStringLiteral("%1 次").arg(fed));
    w.setAutoRefresh(0);
    check(w.autoRefreshInterval() == 0, "setAutoRefresh(0) 關掉");
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QString dir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::tempPath();

    PlotWidget w;
    w.setAttribute(Qt::WA_DontShowOnScreen);
    w.resize(800, 500);
    w.show();
    QCoreApplication::processEvents();

    testTranslation();
    testPrint(w, dir);
    testAnimation(w);
    testGestures(w);
    testLayerCache(w);

    out() << "\n" << (g_failures == 0 ? QStringLiteral("全部通過") : QStringLiteral("%1 項失敗").arg(g_failures))
          << "\n";
    out().flush();
    return g_failures;
}

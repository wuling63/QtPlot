// bench.cpp — 渲染效能量測。不是測試（不判對錯），是改效能前後拿來比的數字。
//
// 用法: qtplot_bench [--quick]
//   每一項先畫一張暖機（第一張另外列出來），再取後面幾張的最小值。
//   --quick 只跑小資料量，給 ctest 當冒煙測試用。
#include "qtplot/PlotWidget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QTextStream>
#include <QThread>

#include <atomic>
#include <cmath>
#include <functional>
#include <random>

using namespace qtplot;

namespace {

QTextStream& out() {
    static QTextStream s(stdout);
    return s;
}

QVector<double> randomWalk(qsizetype n, unsigned seed = 1) {
    QVector<double> v(n);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-0.5, 0.5);
    double y = 0;
    for (qsizetype i = 0; i < n; ++i) v[i] = (y += d(rng));
    return v;
}

struct Timing {
    double first = 0;
    double best = 1e300;
};

/// 畫 reps 張，回傳第一張與後面最好的一張（毫秒）。
Timing measure(const std::function<void()>& frame, int reps = 6) {
    Timing t;
    for (int i = 0; i < reps; ++i) {
        QElapsedTimer timer;
        timer.start();
        frame();
        const double ms = timer.nsecsElapsed() / 1e6;
        if (i == 0) t.first = ms;
        else t.best = std::min(t.best, ms);
    }
    return t;
}

void report(const QString& name, const Timing& t) {
    out() << QStringLiteral("  %1  %2 ms   （第一張 %3 ms）\n")
                 .arg(name, -44)
                 .arg(t.best, 8, 'f', 2)
                 .arg(t.first, 0, 'f', 1);
    out().flush();
}

void benchPlot(const QString& name, Plot& plot, int w = 1600, int h = 900) {
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    report(name, measure([&] {
        QPainter p(&img);
        plot.render(p, QRectF(0, 0, w, h));
    }));
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const bool quick = app.arguments().contains(QStringLiteral("--quick"));
    const qsizetype big = quick ? 200'000 : 10'000'000;
    const qsizetype huge = quick ? 400'000 : 50'000'000;

    // 行程裡第一次畫圖：和資料無關的一次性成本（載入字型等），先單獨量出來
    {
        Plot empty;
        QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
        QElapsedTimer t;
        t.start();
        {
            QPainter p(&img);
            empty.render(p, QRectF(0, 0, 400, 300));
        }
        out() << QStringLiteral("行程裡第一次畫圖（空白的圖）  %1 ms\n").arg(t.nsecsElapsed() / 1e6, 0, 'f', 1);
    }

    out() << "Signal（全圖）\n";
    for (qsizetype n : {qsizetype(1'000'000), big, huge}) {
        Plot plot;
        plot.addSignal(randomWalk(n));
        plot.autoScale();
        benchPlot(QStringLiteral("%1 點").arg(n), plot);
    }

    out() << "Signal（放大到 1%）\n";
    {
        Plot plot;
        plot.addSignal(randomWalk(big));
        plot.setAxisLimits(big * 0.5, big * 0.51, -2000, 2000);
        benchPlot(QStringLiteral("%1 點，看其中 1%").arg(big), plot);
    }

    out() << "SignalXY\n";
    {
        Plot plot;
        QVector<double> xs(big);
        for (qsizetype i = 0; i < big; ++i) xs[i] = i * 0.001 + (i % 7) * 1e-5;
        plot.addSignalXY(xs, randomWalk(big));
        plot.autoScale();
        benchPlot(QStringLiteral("%1 點").arg(big), plot);
    }

    out() << "Scatter\n";
    for (qsizetype n : {qsizetype(10'000), qsizetype(100'000)}) {
        Plot plot;
        QVector<double> xs(n), ys(n);
        std::mt19937 rng(3);
        std::normal_distribution<double> d;
        for (qsizetype i = 0; i < n; ++i) {
            xs[i] = d(rng);
            ys[i] = d(rng);
        }
        Scatter* s = plot.addScatter(xs, ys);
        s->line.width = 0;  // 只有點
        plot.autoScale();
        benchPlot(QStringLiteral("%1 點（實心圓標記，無線）").arg(n), plot);
    }

    out() << "即時資料\n";
    {
        Plot plot;
        DataStreamer* s = plot.addDataStreamer(100'000);
        s->addRange(randomWalk(100'000));
        benchPlot(QStringLiteral("DataStreamer 100000 點"), plot);
    }

    out() << "PlotWidget 重畫（1M 點 Signal）\n";
    {
        PlotWidget w;
        w.setAttribute(Qt::WA_DontShowOnScreen);
        w.resize(1600, 900);
        w.plot().addSignal(randomWalk(1'000'000));
        w.plot().autoScale();
        w.showTooltipOnHover = true;
        w.show();
        w.repaint();
        report(QStringLiteral("整張重畫（refresh）"), measure([&] {
                   w.refresh();
                   w.repaint();
               }));
        // 只有提示框在動：滑鼠移過去時的情況
        QPointF pos(800, 450);
        report(QStringLiteral("滑鼠移動（只有提示框變）"), measure([&] {
                   pos += QPointF(1, 0);
                   QMouseEvent ev(QEvent::MouseMove, pos, w.mapToGlobal(pos), Qt::NoButton, Qt::NoButton,
                                  Qt::NoModifier);
                   QCoreApplication::sendEvent(&w, &ev);
                   w.repaint();
               }, 20));
    }

    out() << "背景執行緒餵資料（DataStreamer）\n";
    {
        Plot plot;
        DataStreamer* s = plot.addDataStreamer(10'000);
        std::atomic<bool> stop{false};
        QThread* producer = QThread::create([&] {
            double phase = 0;
            while (!stop) {
                QVector<double> chunk(100);
                for (double& v : chunk) v = std::sin(phase += 0.01);
                s->addRange(chunk);
            }
        });
        producer->start();
        QImage img(800, 400, QImage::Format_ARGB32_Premultiplied);
        int frames = 0;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < (quick ? 300 : 1500)) {
            QPainter p(&img);
            plot.render(p, QRectF(0, 0, 800, 400));
            ++frames;
        }
        stop = true;
        producer->wait();
        delete producer;
        out() << QStringLiteral("  %1 張，沒有當掉（沒有 mutex 時這裡可能會當）\n").arg(frames);
    }
    return 0;
}

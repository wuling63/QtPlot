#include "SharedAxesWindow.h"

#include "qtplot/DataGen.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace qtplot {
namespace demo {

SharedAxesWindow::SharedAxesWindow(bool darkTheme, QWidget* parent) : QWidget(parent) {
    setWindowTitle(tr("共用座標軸"));
    resize(900, 700);

    m_shareX = new QCheckBox(tr("共用 X 軸"), this);
    m_shareY = new QCheckBox(tr("共用 Y 軸"), this);
    m_shareX->setChecked(true);

    auto* hint = new QLabel(
        tr("在任一張圖上平移或縮放，另一張會同步。"
           "常見用途是上圖看價格、下圖看成交量，兩者的時間軸必須對齊。"),
        this);
    hint->setWordWrap(true);

    m_top = new PlotWidget(this);
    m_bottom = new PlotWidget(this);

    const PlotStyle style = darkTheme ? PlotStyle::dark() : PlotStyle::light();
    gen::setSeed(19);

    m_top->plot().setStyle(style);
    m_top->plot().addSignal(gen::sin(200, 1.0, 2.0))->legendText = QStringLiteral("sin");
    m_top->plot().legend.isVisible = true;
    m_top->plot().title.text = tr("上圖");
    m_top->plot().autoScale();

    m_bottom->plot().setStyle(style);
    m_bottom->plot().addSignal(gen::cos(200, 1.0, 2.0), 1.0, QColor(214, 39, 40))->legendText =
        QStringLiteral("cos");
    m_bottom->plot().legend.isVisible = true;
    m_bottom->plot().title.text = tr("下圖");
    m_bottom->plot().autoScale();

    auto* controls = new QHBoxLayout;
    controls->addWidget(m_shareX);
    controls->addWidget(m_shareY);
    controls->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(hint);
    layout->addWidget(m_top, 1);
    layout->addWidget(m_bottom, 1);

    connect(m_shareX, &QCheckBox::toggled, this, [this] { relink(); });
    connect(m_shareY, &QCheckBox::toggled, this, [this] { relink(); });
    relink();
}

void SharedAxesWindow::relink() {
    // 先解綁再重綁。不解就重綁的話，舊的設定還留著，
    // 取消勾選「共用 X」之後 X 仍然會同步。
    m_top->unlinkAxes(m_bottom);
    if (m_shareX->isChecked() || m_shareY->isChecked())
        m_top->linkAxes(m_bottom, m_shareX->isChecked(), m_shareY->isChecked());
    m_top->refresh();
    m_bottom->refresh();
}

}  // namespace demo
}  // namespace qtplot

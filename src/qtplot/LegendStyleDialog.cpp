#include "LegendStyleDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace qtplot {

namespace {

// 外框線型選單的順序
const LinePattern kPatterns[] = {LinePattern::Solid, LinePattern::Dashed, LinePattern::Dotted,
                                 LinePattern::DashDot};

int patternIndex(LinePattern p) {
    for (int i = 0; i < 4; ++i)
        if (kPatterns[i] == p) return i;
    return 0;
}

QDoubleSpinBox* widthSpin() {
    auto* s = new QDoubleSpinBox;
    s->setRange(0.5, 10);
    s->setSingleStep(0.5);
    s->setDecimals(1);
    s->setSuffix(QStringLiteral(" px"));
    return s;
}

}  // namespace

LegendStyleDialog::LegendStyleDialog(Legend& legend, QWidget* parent)
    : QDialog(parent), m_legend(legend), m_original(legend) {
    setWindowTitle(tr("圖例樣式"));

    // ── 外框 ──
    m_borderOn = new QCheckBox(tr("顯示外框"));
    m_borderColorButton = new QPushButton;
    m_borderWidth = widthSpin();
    m_borderPattern = new QComboBox;
    m_borderPattern->addItems({tr("實線"), tr("虛線"), tr("點線"), tr("點虛線")});
    auto* borderBox = new QGroupBox(tr("外框"));
    auto* borderForm = new QFormLayout(borderBox);
    borderForm->addRow(m_borderOn);
    borderForm->addRow(tr("顏色"), m_borderColorButton);
    borderForm->addRow(tr("粗細"), m_borderWidth);
    borderForm->addRow(tr("線型"), m_borderPattern);

    // ── 底色 ──
    m_backgroundOn = new QCheckBox(tr("塗滿底色"));
    m_backgroundColorButton = new QPushButton;
    auto* backBox = new QGroupBox(tr("底色"));
    auto* backForm = new QFormLayout(backBox);
    backForm->addRow(m_backgroundOn);
    backForm->addRow(tr("顏色"), m_backgroundColorButton);

    // ── 符號 ──
    m_fillSymbols = new QCheckBox(tr("色塊塗滿"));
    m_symbolOutlineOn = new QCheckBox(tr("色塊加外框"));
    m_symbolOutlineColorButton = new QPushButton;
    m_symbolOutlineWidth = widthSpin();
    auto* symBox = new QGroupBox(tr("符號"));
    auto* symForm = new QFormLayout(symBox);
    symForm->addRow(m_fillSymbols);
    symForm->addRow(m_symbolOutlineOn);
    symForm->addRow(tr("顏色"), m_symbolOutlineColorButton);
    symForm->addRow(tr("粗細"), m_symbolOutlineWidth);

    // ── 文字 ──
    m_fontButton = new QPushButton;
    m_textColorButton = new QPushButton;
    auto* textBox = new QGroupBox(tr("文字"));
    auto* textForm = new QFormLayout(textBox);
    textForm->addRow(tr("字型"), m_fontButton);
    textForm->addRow(tr("顏色"), m_textColorButton);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::RestoreDefaults);
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(tr("還原預設"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            &LegendStyleDialog::restoreDefaults);

    auto* columns = new QHBoxLayout;
    auto* left = new QVBoxLayout;
    left->addWidget(borderBox);
    left->addWidget(backBox);
    left->addStretch();
    auto* right = new QVBoxLayout;
    right->addWidget(symBox);
    right->addWidget(textBox);
    right->addStretch();
    columns->addLayout(left);
    columns->addLayout(right);
    // 給測試與自動化工具找控制項用（文字會隨語言變）
    m_borderOn->setObjectName(QStringLiteral("borderOn"));
    m_borderPattern->setObjectName(QStringLiteral("borderPattern"));
    m_backgroundOn->setObjectName(QStringLiteral("backgroundOn"));
    m_fillSymbols->setObjectName(QStringLiteral("fillSymbols"));
    m_symbolOutlineOn->setObjectName(QStringLiteral("symbolOutlineOn"));
    buttons->button(QDialogButtonBox::RestoreDefaults)->setObjectName(QStringLiteral("restoreDefaults"));

    auto* root = new QVBoxLayout(this);
    root->addLayout(columns);
    root->addWidget(buttons);

    loadFromLegend();

    for (QCheckBox* c : {m_borderOn, m_backgroundOn, m_fillSymbols, m_symbolOutlineOn})
        connect(c, &QCheckBox::toggled, this, &LegendStyleDialog::apply);
    for (QDoubleSpinBox* s : {m_borderWidth, m_symbolOutlineWidth})
        connect(s, &QDoubleSpinBox::valueChanged, this, &LegendStyleDialog::apply);
    connect(m_borderPattern, &QComboBox::currentIndexChanged, this, &LegendStyleDialog::apply);
    connect(m_borderColorButton, &QPushButton::clicked, this,
            [this] { pickColor(m_borderColorButton, m_borderColor, tr("外框顏色")); });
    connect(m_backgroundColorButton, &QPushButton::clicked, this,
            [this] { pickColor(m_backgroundColorButton, m_backgroundColor, tr("底色")); });
    connect(m_symbolOutlineColorButton, &QPushButton::clicked, this,
            [this] { pickColor(m_symbolOutlineColorButton, m_symbolOutlineColor, tr("色塊外框顏色")); });
    connect(m_textColorButton, &QPushButton::clicked, this,
            [this] { pickColor(m_textColorButton, m_textColor, tr("文字顏色")); });
    connect(m_fontButton, &QPushButton::clicked, this, &LegendStyleDialog::pickFont);

    // 取消：把打開時的樣子寫回去
    connect(this, &QDialog::rejected, this, [this] {
        m_legend = m_original;
        emit changed();
    });
}

void LegendStyleDialog::loadFromLegend() {
    m_loading = true;
    const Legend& lg = m_legend;

    m_borderOn->setChecked(lg.border.isVisible());
    // 外框關著的時候顏色可能是全透明的，給一個看得見的顏色當起點
    m_borderColor = lg.border.color.alpha() > 0 ? lg.border.color : QColor(0, 0, 0, 70);
    m_borderWidth->setValue(lg.border.width > 0 ? lg.border.width : 1.0);
    m_borderPattern->setCurrentIndex(patternIndex(lg.border.pattern));

    m_backgroundOn->setChecked(lg.background.isVisible());
    m_backgroundColor = lg.background.color.alpha() > 0 ? lg.background.color : QColor(255, 255, 255, 235);

    m_fillSymbols->setChecked(lg.fillSymbols);
    m_symbolOutlineOn->setChecked(lg.symbolOutline.isVisible());
    m_symbolOutlineColor = lg.symbolOutline.color.alpha() > 0 ? lg.symbolOutline.color : QColor(0, 0, 0);
    m_symbolOutlineWidth->setValue(lg.symbolOutline.width > 0 ? lg.symbolOutline.width : 1.0);

    m_font = lg.font;
    m_textColor = lg.font.color;

    setSwatch(m_borderColorButton, m_borderColor);
    setSwatch(m_backgroundColorButton, m_backgroundColor);
    setSwatch(m_symbolOutlineColorButton, m_symbolOutlineColor);
    setSwatch(m_textColorButton, m_textColor);
    const QFont f = m_font.font();
    QStringList parts{f.family(), QString::number(m_font.size, 'g', 3) + QStringLiteral(" pt")};
    if (m_font.effectiveWeight() >= FontStyle::DemiBold) parts << tr("粗體");
    if (m_font.italic) parts << tr("斜體");
    m_fontButton->setText(parts.join(QStringLiteral(", ")));
    m_fontButton->setFont(f);

    updateEnabled();
    m_loading = false;
}

void LegendStyleDialog::apply() {
    if (m_loading) return;
    Legend& lg = m_legend;

    lg.border.color = m_borderColor;
    lg.border.width = m_borderWidth->value();
    lg.border.pattern = m_borderOn->isChecked() ? kPatterns[m_borderPattern->currentIndex()] : LinePattern::None;

    lg.background.color = m_backgroundOn->isChecked() ? m_backgroundColor : QColor(0, 0, 0, 0);
    lg.background.hatch = HatchPattern::None;
    lg.background.gradient = GradientDirection::None;

    lg.fillSymbols = m_fillSymbols->isChecked();
    lg.symbolOutline = m_symbolOutlineOn->isChecked()
                           ? LineStyle(m_symbolOutlineColor, m_symbolOutlineWidth->value())
                           : LineStyle::none();

    lg.font = m_font;
    lg.font.color = m_textColor;
    // 標題跟著換字型家族，大小與粗細維持它自己的
    lg.title.font.family = m_font.family;
    lg.title.font.color = m_textColor;

    updateEnabled();
    emit changed();
}

void LegendStyleDialog::restoreDefaults() {
    const Legend d;
    m_legend.border = d.border;
    m_legend.background = d.background;
    m_legend.fillSymbols = d.fillSymbols;
    m_legend.symbolOutline = d.symbolOutline;
    m_legend.font = d.font;
    m_legend.title.font = d.title.font;
    loadFromLegend();
    emit changed();
}

void LegendStyleDialog::pickColor(QPushButton* button, QColor& color, const QString& title) {
    const QColor c = QColorDialog::getColor(color, this, title, QColorDialog::ShowAlphaChannel);
    if (!c.isValid()) return;  // 按了取消
    color = c;
    setSwatch(button, c);
    apply();
}

void LegendStyleDialog::pickFont() {
    bool ok = false;
    const QFont f = QFontDialog::getFont(&ok, m_font.font(), this, tr("圖例字型"));
    if (!ok) return;
    m_font.family = f.family();
    if (f.pointSizeF() > 0) m_font.size = f.pointSizeF();
    // 一般與粗體回到 Auto + bold，其他字重才寫進 weight
    const int w = f.weight();
    if (w == QFont::Normal || w == QFont::Bold) {
        m_font.weight = FontStyle::Auto;
        m_font.bold = (w == QFont::Bold);
    } else {
        m_font.weight = w;
    }
    m_font.italic = f.italic();
    m_font.underline = f.underline();
    m_font.strikeOut = f.strikeOut();
    apply();
    loadFromLegend();  // 更新按鈕上的字型說明
}

void LegendStyleDialog::updateEnabled() {
    const bool border = m_borderOn->isChecked();
    m_borderColorButton->setEnabled(border);
    m_borderWidth->setEnabled(border);
    m_borderPattern->setEnabled(border);
    m_backgroundColorButton->setEnabled(m_backgroundOn->isChecked());
    const bool outline = m_symbolOutlineOn->isChecked();
    m_symbolOutlineColorButton->setEnabled(outline);
    m_symbolOutlineWidth->setEnabled(outline);
}

void LegendStyleDialog::setSwatch(QPushButton* button, const QColor& color) {
    // 棋盤格墊底，半透明的顏色才看得出來
    QPixmap pm(36, 16);
    QPainter p(&pm);
    for (int y = 0; y < 16; y += 4)
        for (int x = 0; x < 36; x += 4)
            p.fillRect(x, y, 4, 4, ((x + y) / 4) % 2 ? QColor(200, 200, 200) : QColor(255, 255, 255));
    p.fillRect(pm.rect(), color);
    p.setPen(QColor(90, 90, 90));
    p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
    p.end();
    button->setIcon(QIcon(pm));
    button->setIconSize(pm.size());
    button->setText(color.alpha() == 255 ? color.name() : color.name(QColor::HexArgb));
}

}  // namespace qtplot

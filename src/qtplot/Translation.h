// Translation.h — QtPlot 自己的介面文字（右鍵選單、對話框、預設軸名）的翻譯。
//
// 原始字串是繁體中文，另外附英文、簡體中文、日文、韓文（編進函式庫裡，不必另外帶 .qm 檔）。
//
//     QApplication app(argc, argv);
//     qtplot::setLanguage(QString());          // 跟著系統語言
//     qtplot::setLanguage(QStringLiteral("en")); // 指定英文
//
// 隨時都可以換，已經畫好的圖會跟著換：
//   - 右鍵選單每次打開時才建，自然是新的語言。
//   - add*() 自動填的文字（柏拉圖的「累積百分比」、Q-Q 圖的軸名、魚骨圖的 6M）由 Plot 記著，
//     下次 render 時重翻。使用者自己改過的不會被蓋掉。
//   - PlotWidget／MultiplotWidget 收到 LanguageChange 會自己重畫。
// PlotWidget 的右鍵選單裡也有「語言」子選單，使用者可以直接切換。
#pragma once

#include <QLocale>
#include <QStringList>

namespace qtplot {

/// 換語言。code 是 "en"、"zh_CN" 這種代碼（見 availableLanguages()）；空字串代表跟著系統。
/// 有載入翻譯回傳 true；原始語言（繁體中文）或沒有那個語言的翻譯時回傳 false，介面維持繁體中文。
bool setLanguage(const QString& code);
/// 目前的設定。空字串代表跟著系統。
QString language();
/// 實際在用的語言代碼（跟著系統時，是系統語言對應到的那一個；找不到翻譯就是 "zh_TW"）。
QString effectiveLanguage();

/// 可以選的語言，含原始語言 zh_TW，依代碼排序。
QStringList availableLanguages();
/// 語言代碼的顯示名稱，用那個語言自己寫（「English」「日本語」），選單用。
QString languageDisplayName(const QString& code);

/// 顯示用的 locale：月份、星期名稱、數值格式照它。跟著目前的語言設定。
///
/// 刻意不去改 QLocale::setDefault()：那是整個程式共用的設定，
/// 一個繪圖函式庫換語言不該順便改掉宿主程式其他地方的數字格式。
QLocale displayLocale();

/// 每換一次語言就加一。Plot 在 render 時比對它，決定要不要重翻自動產生的文字。
int languageGeneration();

// ── 舊的介面，保留相容 ──
/// 依 locale 載入翻譯，等同 setLanguage(locale 對應的代碼)。
bool installTranslator(const QLocale& locale = QLocale());
/// 拿掉翻譯，回到繁體中文。
void removeTranslator();
/// 有附翻譯的語言，不含原始語言 zh_TW。
QStringList availableTranslations();

}  // namespace qtplot

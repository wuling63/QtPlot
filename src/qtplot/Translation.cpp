#include "Translation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QTranslator>

#include <memory>

// 翻譯檔由 CMake 的 qt_add_translations 編成 .qm 放進資源，路徑是 :/qtplot/i18n/。
// qtplot 是靜態函式庫，資源要先初始化才找得到。
static void initQtPlotTranslationResources() {
    static bool done = false;
    if (done) return;
    done = true;
    Q_INIT_RESOURCE(qtplot_translations);
}

namespace qtplot {

namespace {

const QString kDir = QStringLiteral(":/qtplot/i18n");
const QString kSource = QStringLiteral("zh_TW");

std::unique_ptr<QTranslator> g_translator;
QString g_setting;                 // setLanguage 收到的代碼；空 = 跟著系統
QString g_effective = kSource;     // 實際載入的語言
int g_generation = 0;

/// 繁體中文（台灣、香港、澳門，或明確標了繁體字）是原始語言，不必翻。
bool isSourceLanguage(const QLocale& l) {
    if (l.language() != QLocale::Chinese) return false;
    if (l.script() == QLocale::TraditionalChineseScript) return true;
    const auto t = l.territory();
    return t == QLocale::Taiwan || t == QLocale::HongKong || t == QLocale::Macau;
}

void unload() {
    if (!g_translator) return;
    QCoreApplication::removeTranslator(g_translator.get());
    g_translator.reset();
}

/// 換成 locale 對應的翻譯。不動 g_setting。
bool apply(const QLocale& locale) {
    initQtPlotTranslationResources();
    unload();
    g_effective = kSource;
    bool loaded = false;
    if (!isSourceLanguage(locale)) {
        auto t = std::make_unique<QTranslator>();
        // load(locale, ...) 會依序試 qtplot_en_US、qtplot_en……，找不到就失敗
        if (t->load(locale, QStringLiteral("qtplot"), QStringLiteral("_"), kDir) &&
            QCoreApplication::installTranslator(t.get())) {
            g_effective = QFileInfo(t->filePath()).completeBaseName().mid(7);  // 去掉 "qtplot_"
            g_translator = std::move(t);
            loaded = true;
        }
    }
    // 沒有翻譯也要加一：從英文切回繁體中文時，畫好的圖一樣要重翻回來
    ++g_generation;
    return loaded;
}

QLocale localeForCode(const QString& code) {
    return code.isEmpty() ? QLocale::system() : QLocale(code);
}

}  // namespace

bool setLanguage(const QString& code) {
    g_setting = code;
    return apply(localeForCode(code));
}

QString language() { return g_setting; }

QString effectiveLanguage() { return g_effective; }

QStringList availableLanguages() {
    QStringList out = availableTranslations();
    out.append(kSource);
    out.sort();
    return out;
}

QString languageDisplayName(const QString& code) {
    // 自己寫，不用 nativeLanguageName：中文兩種寫法它都回「中文」分不出來，
    // 英文、西班牙文又會帶出地區（「American English」「Español de España」）
    static const QHash<QString, QString> names{
        {QStringLiteral("zh_TW"), QStringLiteral("繁體中文")}, {QStringLiteral("zh_CN"), QStringLiteral("简体中文")},
        {QStringLiteral("en"), QStringLiteral("English")},     {QStringLiteral("ja"), QStringLiteral("日本語")},
        {QStringLiteral("ko"), QStringLiteral("한국어")},       {QStringLiteral("fr"), QStringLiteral("Français")},
        {QStringLiteral("de"), QStringLiteral("Deutsch")},     {QStringLiteral("it"), QStringLiteral("Italiano")},
        {QStringLiteral("es"), QStringLiteral("Español")},
    };
    if (const auto it = names.constFind(code); it != names.constEnd()) return *it;
    // 之後新增、表裡還沒有的語言
    const QLocale l(code);
    QString name = l.nativeLanguageName();
    if (name.isEmpty()) return code;
    name[0] = name[0].toUpper();  // 「english」→「English」
    return name;
}

QLocale displayLocale() {
    if (g_effective == kSource) {
        // 原始語言：跟著系統且系統本來就是繁體中文的變體（zh_HK）就照系統，其他一律 zh_TW
        const QLocale sys = QLocale::system();
        return (g_setting.isEmpty() && isSourceLanguage(sys)) ? sys : QLocale(kSource);
    }
    return QLocale(g_effective);
}

int languageGeneration() { return g_generation; }

bool installTranslator(const QLocale& locale) {
    g_setting = isSourceLanguage(locale) ? kSource : locale.name();
    return apply(locale);
}

void removeTranslator() { setLanguage(kSource); }

QStringList availableTranslations() {
    initQtPlotTranslationResources();
    QStringList out;
    for (const QString& f : QDir(kDir).entryList({QStringLiteral("qtplot_*.qm")}, QDir::Files)) {
        QString code = f.mid(7);  // 去掉 "qtplot_"
        code.chop(3);             // 去掉 ".qm"
        out.append(code);
    }
    return out;
}

}  // namespace qtplot

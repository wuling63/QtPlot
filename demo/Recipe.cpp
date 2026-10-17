#include "Recipe.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace qtplot {
namespace demo {

RecipeRegistry& RecipeRegistry::instance() {
    static RecipeRegistry reg;
    return reg;
}

int RecipeRegistry::add(const Recipe& recipe) {
    m_recipes.append(recipe);
    return m_recipes.size();
}

QStringList RecipeRegistry::chapters() const {
    QStringList out;
    for (const Recipe& r : m_recipes)
        if (!out.contains(r.chapter)) out.append(r.chapter);
    return out;  // 保持註冊順序，不排序 —— 章節的先後是刻意安排的
}

QStringList RecipeRegistry::categories(const QString& chapter) const {
    QStringList out;
    for (const Recipe& r : m_recipes)
        if (r.chapter == chapter && !out.contains(r.category)) out.append(r.category);
    return out;
}

QVector<const Recipe*> RecipeRegistry::inCategory(const QString& chapter,
                                                  const QString& category) const {
    QVector<const Recipe*> out;
    for (const Recipe& r : m_recipes)
        if (r.chapter == chapter && r.category == category) out.append(&r);
    return out;
}

const Recipe* RecipeRegistry::find(const QString& chapter, const QString& category,
                                   const QString& name) const {
    for (const Recipe& r : m_recipes)
        if (r.chapter == chapter && r.category == category && r.name == name) return &r;
    return nullptr;
}

QString Recipe::sourceCode() const {
    // __FILE__ 是編譯時的絕對路徑，執行時不一定存在（換機器、換目錄都會失效），
    // 所以原始碼是編進資源的；這裡只用檔名去對。
    const QString base = QFileInfo(sourceFile).fileName();
    QFile f(QStringLiteral(":/recipes/") + base);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QObject::tr("（找不到原始碼資源 :/recipes/%1）").arg(base);

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    const QStringList lines = in.readAll().split(QLatin1Char('\n'));

    // sourceLine 是 QTPLOT_RECIPE 那一行（1-based）。從那裡開始輸出，
    // 直到大括號收支平衡為止，就剛好是整支範例。
    const int start = sourceLine - 1;
    if (start < 0 || start >= lines.size()) return QString();

    QStringList out;
    int depth = 0;
    bool seenBrace = false;
    for (int i = start; i < lines.size(); ++i) {
        const QString& line = lines.at(i);
        out.append(line);
        for (QChar ch : line) {
            if (ch == QLatin1Char('{')) {
                ++depth;
                seenBrace = true;
            } else if (ch == QLatin1Char('}')) {
                --depth;
            }
        }
        if (seenBrace && depth <= 0) break;
    }
    return out.join(QLatin1Char('\n'));
}

}  // namespace demo
}  // namespace qtplot

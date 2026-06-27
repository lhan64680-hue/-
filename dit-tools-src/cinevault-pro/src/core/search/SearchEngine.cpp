#include "core/search/SearchEngine.h"

#include <QRegularExpression>

QString SearchEngine::buildLikePattern(const QString &keyword) const
{
    auto normalized = keyword.trimmed();
    normalized.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    normalized.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return QStringLiteral("%") + normalized + QStringLiteral("%");
}

QString SearchEngine::buildFtsQuery(const QString &keyword) const
{
    const auto tokens = keyword.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    QStringList escapedTokens;
    for (auto token : tokens) {
        token.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        escapedTokens.append(QStringLiteral("\"%1\"").arg(token));
    }
    if (escapedTokens.isEmpty()) {
        return {};
    }
    return escapedTokens.join(QStringLiteral(" OR "));
}

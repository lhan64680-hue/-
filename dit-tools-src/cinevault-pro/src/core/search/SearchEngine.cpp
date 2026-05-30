#include "core/search/SearchEngine.h"

QString SearchEngine::buildLikePattern(const QString &keyword) const
{
    auto normalized = keyword.trimmed();
    normalized.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    normalized.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return QStringLiteral("%") + normalized + QStringLiteral("%");
}

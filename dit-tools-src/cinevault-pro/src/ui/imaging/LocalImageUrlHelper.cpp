#include "ui/imaging/LocalImageUrlHelper.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace {
constexpr auto kProviderPrefix = "image://cinevault-local/";

bool isWebpFile(const QString &localPath)
{
    return QFileInfo(localPath).suffix().compare(QStringLiteral("webp"), Qt::CaseInsensitive) == 0;
}

QString localPathFromInput(const QString &input)
{
    const auto trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed);
    if (url.isLocalFile()) {
        return QDir::cleanPath(url.toLocalFile());
    }

    if (trimmed.startsWith(QStringLiteral("file:/"), Qt::CaseInsensitive)) {
        return QDir::cleanPath(QUrl(trimmed).toLocalFile());
    }

    return QDir::cleanPath(trimmed);
}
}

LocalImageUrlHelper::LocalImageUrlHelper(QObject *parent)
    : QObject(parent)
{
}

QString LocalImageUrlHelper::sourceForInput(const QString &input) const
{
    return sourceForInputString(input);
}

QString LocalImageUrlHelper::sourceForInputString(const QString &input)
{
    const auto trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (trimmed.startsWith(QLatin1String(kProviderPrefix), Qt::CaseInsensitive)) {
        return trimmed;
    }

    const auto localPath = localPathFromInput(trimmed);
    if (localPath.isEmpty()) {
        return trimmed;
    }

    const auto fileUrl = QUrl::fromLocalFile(localPath).toString();
    if (isWebpFile(localPath)) {
        return QStringLiteral("%1%2")
            .arg(QLatin1String(kProviderPrefix),
                 QString::fromLatin1(QUrl::toPercentEncoding(fileUrl)));
    }

    return fileUrl;
}

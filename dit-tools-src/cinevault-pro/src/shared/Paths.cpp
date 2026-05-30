#include "shared/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace {
QString ensureDir(const QString &path, QString *errorMessage)
{
    QDir dir;
    if (!dir.mkpath(path)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建目录：%1").arg(path);
        }
        return {};
    }
    return path;
}
}

QString Paths::appDataRoot()
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base.isEmpty() ? QDir::homePath() + QStringLiteral("/影资管家") : base;
}

QString Paths::projectsRoot()
{
    return appDataRoot() + QStringLiteral("/projects");
}

QString Paths::cacheRoot()
{
    return appDataRoot() + QStringLiteral("/cache");
}

QString Paths::configRoot()
{
    return appDataRoot() + QStringLiteral("/config");
}

QString Paths::logsRoot()
{
    return appDataRoot() + QStringLiteral("/logs");
}

bool Paths::ensureBaseDirectories(QString *errorMessage)
{
    return !ensureDir(projectsRoot(), errorMessage).isEmpty()
        && !ensureDir(cacheRoot(), errorMessage).isEmpty()
        && !ensureDir(configRoot(), errorMessage).isEmpty()
        && !ensureDir(logsRoot(), errorMessage).isEmpty();
}

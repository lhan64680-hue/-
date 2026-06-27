#include "core/backup/VolumeEjectService.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>

VolumeEjectService::VolumeEjectService(QObject *parent)
    : QObject(parent)
{
}

bool VolumeEjectService::ejectVolumeForPath(const QString &path, QString *message) const
{
    const QStorageInfo storage(path);
    const auto rootPath = storage.rootPath();
    if (rootPath.isEmpty()) {
        if (message) {
            *message = QStringLiteral("无法识别源卷：%1").arg(path);
        }
        return false;
    }

#if defined(Q_OS_WIN)
    const auto normalizedRoot = rootPath.endsWith(QLatin1Char('\\')) || rootPath.endsWith(QLatin1Char('/'))
        ? rootPath
        : rootPath + QStringLiteral("\\");
    if (!QRegularExpression(QStringLiteral("^[A-Za-z]:[\\\\/]$")).match(normalizedRoot).hasMatch()) {
        if (message) {
            *message = QStringLiteral("当前源路径不是可弹出的盘符卷：%1").arg(rootPath);
        }
        return false;
    }

    const auto drive = normalizedRoot.left(3).replace(QLatin1Char('/'), QLatin1Char('\\'));
    const auto script = QStringLiteral(
        "$drive = '%1'; "
        "$shell = New-Object -ComObject Shell.Application; "
        "$item = $shell.Namespace(17).ParseName($drive); "
        "if ($null -eq $item) { exit 2 }; "
        "$item.InvokeVerb('Eject'); "
        "Start-Sleep -Milliseconds 800; "
        "exit 0").arg(drive);

    QProcess process;
    process.start(QStringLiteral("powershell"),
                  {QStringLiteral("-NoProfile"),
                   QStringLiteral("-ExecutionPolicy"),
                   QStringLiteral("Bypass"),
                   QStringLiteral("-Command"),
                   script});
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        if (message) {
            const auto errorText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *message = errorText.isEmpty()
                ? QStringLiteral("已完成主目标拷贝，但系统未确认安全弹出：%1").arg(rootPath)
                : QStringLiteral("安全弹出失败：%1").arg(errorText);
        }
        return false;
    }

    if (message) {
        *message = QStringLiteral("已请求安全弹出源卷：%1").arg(rootPath);
    }
    return true;
#else
    if (message) {
        *message = QStringLiteral("当前平台暂不支持自动安全弹出：%1").arg(rootPath);
    }
    return false;
#endif
}

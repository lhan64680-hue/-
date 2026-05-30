#include "infrastructure/logging/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
QString g_logFilePath;
}

bool Logger::initialize(const QString &logFilePath, QString *errorMessage)
{
    QFileInfo info(logFilePath);
    QDir dir;
    if (!dir.mkpath(info.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建日志目录：%1").arg(info.absolutePath());
        }
        return false;
    }

    QFile file(logFilePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开日志文件：%1").arg(logFilePath);
        }
        return false;
    }
    g_logFilePath = logFilePath;
    QTextStream stream(&file);
    stream << "\n[" << QDateTime::currentDateTime().toString(Qt::ISODate) << "] [INFO] 日志系统已初始化\n";
    return true;
}

void Logger::info(const QString &message)
{
    write(QStringLiteral("INFO"), message);
}

void Logger::warn(const QString &message)
{
    write(QStringLiteral("WARN"), message);
}

void Logger::error(const QString &message)
{
    write(QStringLiteral("ERROR"), message);
}

QString Logger::currentLogFile()
{
    return g_logFilePath;
}

void Logger::write(const QString &level, const QString &message)
{
    if (g_logFilePath.isEmpty()) {
        return;
    }

    QFile file(g_logFilePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    stream << "[" << QDateTime::currentDateTime().toString(Qt::ISODate) << "] [" << level << "] " << message << '\n';
}

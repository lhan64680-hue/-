#include "core/backup/BackupPlanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>

namespace {
Qt::CaseSensitivity pathCaseSensitivity()
{
#if defined(Q_OS_WIN)
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

QString cleanAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
}

QString comparablePath(const QString &path)
{
    const auto cleaned = cleanAbsolutePath(path);
    return pathCaseSensitivity() == Qt::CaseInsensitive ? cleaned.toLower() : cleaned;
}

bool pathContains(const QString &root, const QString &candidate)
{
    auto normalizedRoot = cleanAbsolutePath(root);
    const auto normalizedCandidate = cleanAbsolutePath(candidate);
    if (normalizedRoot.compare(normalizedCandidate, pathCaseSensitivity()) == 0) {
        return true;
    }
    if (!normalizedRoot.endsWith(QLatin1Char('/'))) {
        normalizedRoot.append(QLatin1Char('/'));
    }
    return normalizedCandidate.startsWith(normalizedRoot, pathCaseSensitivity());
}

QString fallbackNameForPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.fileName().isEmpty()) {
        return info.fileName();
    }
    auto cleaned = cleanAbsolutePath(path);
    cleaned.replace(QLatin1Char(':'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('/'), QLatin1Char('_'));
    return cleaned.trimmed().isEmpty() ? QStringLiteral("source") : cleaned;
}

QString modifiedAt(const QFileInfo &info)
{
    return info.lastModified().toString(Qt::ISODate);
}

bool appendFile(BackupPlan *plan,
                BackupSource *source,
                const QString &sourceId,
                const QString &sourcePath,
                const QString &relativePath,
                const QFileInfo &fileInfo)
{
    if (!plan || !source) {
        return false;
    }

    if (!fileInfo.isReadable()) {
        plan->errors.append(QStringLiteral("文件不可读：%1").arg(sourcePath));
        return false;
    }

    BackupFileItem item;
    item.sourceId = sourceId;
    item.sourcePath = cleanAbsolutePath(sourcePath);
    item.relativePath = QDir::cleanPath(relativePath).replace(QLatin1Char('\\'), QLatin1Char('/'));
    item.sizeBytes = fileInfo.size();
    item.modifiedAt = modifiedAt(fileInfo);
    plan->files.append(item);

    ++source->totalFiles;
    source->totalBytes += item.sizeBytes;
    ++plan->totalFiles;
    plan->totalBytes += item.sizeBytes;
    return true;
}

void collectSourceFiles(BackupPlan *plan, BackupSource *source)
{
    const QFileInfo info(source->path);
    if (!info.exists()) {
        plan->errors.append(QStringLiteral("源不存在：%1").arg(source->path));
        return;
    }

    source->id = source->id.trimmed().isEmpty()
        ? QStringLiteral("source_%1").arg(plan->sources.size() + 1)
        : source->id;
    source->rootPath = cleanAbsolutePath(info.absoluteFilePath());
    source->name = source->name.trimmed().isEmpty() ? fallbackNameForPath(source->rootPath) : source->name.trimmed();
    source->readable = info.isReadable();

    if (!source->readable) {
        plan->errors.append(QStringLiteral("源不可读：%1").arg(source->rootPath));
        return;
    }

    const auto topSegment = BackupPlanner::safePathSegment(source->name);
    if (source->kind == BackupSourceKind::File) {
        if (!info.isFile()) {
            plan->errors.append(QStringLiteral("源不是文件：%1").arg(source->rootPath));
            return;
        }
        appendFile(plan, source, source->id, source->rootPath, QFileInfo(source->rootPath).fileName(), info);
        return;
    }

    if (!info.isDir()) {
        plan->errors.append(QStringLiteral("源不是目录或磁盘卷：%1").arg(source->rootPath));
        return;
    }

    QDirIterator iterator(source->rootPath,
                          QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const auto filePath = iterator.next();
        const QFileInfo fileInfo(filePath);
        const auto relativeInsideSource = QDir(source->rootPath).relativeFilePath(fileInfo.absoluteFilePath());
        const auto relativePath = QDir(topSegment).filePath(relativeInsideSource);
        appendFile(plan, source, source->id, fileInfo.absoluteFilePath(), relativePath, fileInfo);
    }
}
}

BackupPlan BackupPlanner::buildPlan(const BackupRequest &request) const
{
    BackupPlan plan;
    plan.batchName = request.batchName.trimmed().isEmpty()
        ? defaultBatchName(request.projectName)
        : safePathSegment(request.batchName);
    plan.verificationMode = request.verificationMode;
    plan.cascadeEnabled = request.cascadeEnabled;
    plan.primaryDestinationIndex = qBound(0, request.primaryDestinationIndex, qMax(0, request.destinations.size() - 1));

    if (request.sources.isEmpty()) {
        plan.errors.append(QStringLiteral("至少需要添加一个待备份源。"));
    }
    if (request.destinations.isEmpty()) {
        plan.errors.append(QStringLiteral("至少需要添加一个备份目的地。"));
    }

    for (auto source : request.sources) {
        collectSourceFiles(&plan, &source);
        source.statusText = source.totalFiles > 0
            ? QStringLiteral("%1 个文件，%2 字节").arg(source.totalFiles).arg(source.totalBytes)
            : QStringLiteral("未发现可备份文件");
        plan.sources.append(source);
    }

    QSet<QString> relativePaths;
    for (const auto &file : plan.files) {
        const auto key = pathCaseSensitivity() == Qt::CaseInsensitive
            ? file.relativePath.toLower()
            : file.relativePath;
        if (relativePaths.contains(key)) {
            plan.errors.append(QStringLiteral("目标相对路径重复：%1").arg(file.relativePath));
        }
        relativePaths.insert(key);
    }

    QSet<QString> destinationRoots;
    for (int index = 0; index < request.destinations.size(); ++index) {
        auto destination = request.destinations.at(index);
        const QFileInfo info(destination.rootPath);
        destination.id = destination.id.trimmed().isEmpty()
            ? QStringLiteral("destination_%1").arg(index + 1)
            : destination.id;
        destination.rootPath = cleanAbsolutePath(destination.rootPath);
        destination.name = destination.name.trimmed().isEmpty() ? fallbackNameForPath(destination.rootPath) : destination.name.trimmed();
        destination.primary = index == plan.primaryDestinationIndex;
        destination.plannedRootPath = cleanAbsolutePath(QDir(destination.rootPath).filePath(plan.batchName));
        destination.writable = info.exists() && info.isDir() && info.isWritable();

        const auto destinationKey = comparablePath(destination.rootPath);
        if (destinationRoots.contains(destinationKey)) {
            plan.errors.append(QStringLiteral("备份目的地重复：%1").arg(destination.rootPath));
        }
        destinationRoots.insert(destinationKey);

        if (!info.exists() || !info.isDir()) {
            plan.errors.append(QStringLiteral("备份目的地不是可用目录：%1").arg(destination.rootPath));
        } else if (!destination.writable) {
            plan.errors.append(QStringLiteral("备份目的地不可写：%1").arg(destination.rootPath));
        }
        if (QFileInfo::exists(destination.plannedRootPath)) {
            plan.errors.append(QStringLiteral("备份批次目录已存在：%1").arg(destination.plannedRootPath));
        }

        for (const auto &source : plan.sources) {
            if (source.rootPath.isEmpty()) {
                continue;
            }
            if (pathContains(source.rootPath, destination.rootPath)) {
                plan.errors.append(QStringLiteral("备份目的地位于源内部：%1").arg(destination.rootPath));
            }
            if (pathContains(destination.rootPath, source.rootPath)) {
                plan.errors.append(QStringLiteral("待备份源位于目的地内部：%1").arg(source.rootPath));
            }
        }

        if (destination.availableBytes < 0) {
            const QStorageInfo storage(destination.rootPath);
            destination.availableBytes = storage.isValid() ? storage.bytesAvailable() : -1;
        }
        if (destination.availableBytes >= 0 && plan.totalBytes > destination.availableBytes) {
            plan.errors.append(QStringLiteral("备份目的地空间不足：%1").arg(destination.rootPath));
        }

        destination.statusText = destination.primary ? QStringLiteral("主目标") : QStringLiteral("副本目标");
        plan.destinations.append(destination);

        BackupDestinationTask task;
        task.destinationId = destination.id;
        task.name = destination.name;
        task.rootPath = destination.rootPath;
        task.plannedRootPath = destination.plannedRootPath;
        task.primary = destination.primary;
        task.totalFiles = plan.totalFiles;
        task.totalBytes = plan.totalBytes;
        task.statusText = QStringLiteral("等待开始");
        plan.tasks.append(task);
    }

    if (plan.files.isEmpty() && !request.sources.isEmpty()) {
        plan.errors.append(QStringLiteral("没有发现可备份文件。"));
    }

    plan.valid = plan.errors.isEmpty();
    return plan;
}

QString BackupPlanner::defaultBatchName(const QString &projectName, const QDateTime &timestamp)
{
    const auto safeProjectName = safePathSegment(projectName.trimmed().isEmpty()
        ? QStringLiteral("未命名项目")
        : projectName.trimmed());
    return QStringLiteral("%1_%2").arg(safeProjectName, timestamp.toString(QStringLiteral("yyyyMMdd_HHmmss")));
}

QString BackupPlanner::safePathSegment(const QString &name)
{
    auto value = name.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]")), QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    value = value.trimmed();
    while (value.startsWith(QLatin1Char('.'))) {
        value.remove(0, 1);
    }
    while (value.endsWith(QLatin1Char('.')) || value.endsWith(QLatin1Char('_'))) {
        value.chop(1);
    }
    return value.isEmpty() ? QStringLiteral("backup") : value;
}

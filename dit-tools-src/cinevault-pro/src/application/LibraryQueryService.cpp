#include "application/LibraryQueryService.h"

#include "core/search/SearchEngine.h"
#include "infrastructure/db/DatabaseManager.h"
#include "shared/Formatters.h"

#include <QSqlQuery>

namespace {
QVariantList makeDetails(const QList<QPair<QString, QString>> &items)
{
    QVariantList details;
    for (const auto &item : items) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), item.first);
        row.insert(QStringLiteral("value"), item.second);
        details.append(row);
    }
    return details;
}
}

LibraryQueryService::LibraryQueryService(DatabaseManager *databaseManager, SearchEngine *searchEngine, QObject *parent)
    : QObject(parent)
    , m_databaseManager(databaseManager)
    , m_searchEngine(searchEngine)
{
}

QVector<SourceRoot> LibraryQueryService::fetchSourceRoots() const
{
    QVector<SourceRoot> rows;
    if (!m_databaseManager->hasOpenProject()) {
        return rows;
    }

    QSqlQuery query(m_databaseManager->database());
    query.exec(QStringLiteral(
        "SELECT id, name, path, status, total_files, total_folders, total_size_bytes, video_count, audio_count, image_count, other_count, warning_count "
        "FROM source_root ORDER BY created_at DESC"));
    while (query.next()) {
        SourceRoot row;
        row.id = query.value(0).toLongLong();
        row.name = query.value(1).toString();
        row.path = query.value(2).toString();
        row.status = query.value(3).toString();
        row.totalFiles = query.value(4).toLongLong();
        row.totalFolders = query.value(5).toLongLong();
        row.totalSizeBytes = query.value(6).toLongLong();
        row.videoCount = query.value(7).toLongLong();
        row.audioCount = query.value(8).toLongLong();
        row.imageCount = query.value(9).toLongLong();
        row.otherCount = query.value(10).toLongLong();
        row.warningCount = query.value(11).toLongLong();
        rows.append(row);
    }
    return rows;
}

QVector<AssetFile> LibraryQueryService::fetchAssets(const QString &keyword, std::optional<qint64> sourceRootId, std::optional<AssetType> assetType) const
{
    QVector<AssetFile> rows;
    if (!m_databaseManager->hasOpenProject()) {
        return rows;
    }

    QString sql = QStringLiteral(
        "SELECT id, source_root_id, name, extension, absolute_path, relative_path, parent_path, asset_type, size_bytes, modified_at, is_readable "
        "FROM asset_file WHERE 1 = 1");

    QVariantList binds;
    if (sourceRootId.has_value()) {
        sql += QStringLiteral(" AND source_root_id = ?");
        binds.append(sourceRootId.value());
    }
    if (assetType.has_value()) {
        sql += QStringLiteral(" AND asset_type = ?");
        binds.append(static_cast<int>(assetType.value()));
    }
    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (name LIKE ? ESCAPE '\\' OR relative_path LIKE ? ESCAPE '\\')");
        const auto pattern = m_searchEngine->buildLikePattern(keyword);
        binds.append(pattern);
        binds.append(pattern);
    }
    sql += QStringLiteral(" ORDER BY modified_at DESC, id DESC LIMIT 5000");

    QSqlQuery query(m_databaseManager->database());
    query.prepare(sql);
    for (const auto &bind : binds) {
        query.addBindValue(bind);
    }
    query.exec();

    while (query.next()) {
        AssetFile row;
        row.id = query.value(0).toLongLong();
        row.sourceRootId = query.value(1).toLongLong();
        row.name = query.value(2).toString();
        row.extension = query.value(3).toString();
        row.absolutePath = query.value(4).toString();
        row.relativePath = query.value(5).toString();
        row.parentPath = query.value(6).toString();
        row.assetType = static_cast<AssetType>(query.value(7).toInt());
        row.sizeBytes = query.value(8).toLongLong();
        row.modifiedAt = query.value(9).toString();
        row.readable = query.value(10).toInt() == 1;
        rows.append(row);
    }
    return rows;
}

InspectorState LibraryQueryService::buildSourceInspector(qint64 sourceRootId) const
{
    InspectorState state;
    state.title = QStringLiteral("检查器");
    state.subtitle = QStringLiteral("选择素材源查看统计");

    if (!m_databaseManager->hasOpenProject() || sourceRootId <= 0) {
        state.details = makeDetails({{QStringLiteral("状态"), QStringLiteral("未选择素材源")}});
        return state;
    }

    QSqlQuery query(m_databaseManager->database());
    query.prepare(QStringLiteral(
        "SELECT name, path, status, total_files, total_folders, total_size_bytes, video_count, audio_count, image_count, warning_count "
        "FROM source_root WHERE id = ?"));
    query.addBindValue(sourceRootId);
    query.exec();
    if (!query.next()) {
        state.details = makeDetails({{QStringLiteral("状态"), QStringLiteral("素材源不存在")}});
        return state;
    }

    state.title = query.value(0).toString();
    state.subtitle = Formatters::statusLabel(query.value(2).toString());
    state.details = makeDetails({
        {QStringLiteral("源路径"), query.value(1).toString()},
        {QStringLiteral("文件数"), QString::number(query.value(3).toLongLong())},
        {QStringLiteral("文件夹数"), QString::number(query.value(4).toLongLong())},
        {QStringLiteral("总容量"), Formatters::formatBytes(query.value(5).toLongLong())},
        {QStringLiteral("视频数"), QString::number(query.value(6).toLongLong())},
        {QStringLiteral("音频数"), QString::number(query.value(7).toLongLong())},
        {QStringLiteral("图片数"), QString::number(query.value(8).toLongLong())},
        {QStringLiteral("警告数"), QString::number(query.value(9).toLongLong())}
    });
    return state;
}

InspectorState LibraryQueryService::buildAssetInspector(qint64 assetId) const
{
    InspectorState state;
    state.title = QStringLiteral("检查器");
    state.subtitle = QStringLiteral("选择素材查看属性");

    if (!m_databaseManager->hasOpenProject() || assetId <= 0) {
        state.details = makeDetails({{QStringLiteral("状态"), QStringLiteral("未选择素材")}});
        return state;
    }

    QSqlQuery query(m_databaseManager->database());
    query.prepare(QStringLiteral(
        "SELECT name, absolute_path, relative_path, asset_type, size_bytes, modified_at, is_readable "
        "FROM asset_file WHERE id = ?"));
    query.addBindValue(assetId);
    query.exec();
    if (!query.next()) {
        state.details = makeDetails({{QStringLiteral("状态"), QStringLiteral("素材不存在")}});
        return state;
    }

    state.title = query.value(0).toString();
    state.subtitle = Formatters::assetTypeLabel(static_cast<AssetType>(query.value(3).toInt()));
    state.details = makeDetails({
        {QStringLiteral("绝对路径"), query.value(1).toString()},
        {QStringLiteral("相对路径"), query.value(2).toString()},
        {QStringLiteral("类型"), Formatters::assetTypeLabel(static_cast<AssetType>(query.value(3).toInt()))},
        {QStringLiteral("文件大小"), Formatters::formatBytes(query.value(4).toLongLong())},
        {QStringLiteral("修改时间"), query.value(5).toString()},
        {QStringLiteral("可读状态"), query.value(6).toInt() == 1 ? QStringLiteral("正常") : QStringLiteral("不可读")}
    });
    return state;
}

qint64 LibraryQueryService::assetCount(const QString &keyword, std::optional<qint64> sourceRootId, std::optional<AssetType> assetType) const
{
    if (!m_databaseManager->hasOpenProject()) {
        return 0;
    }

    QString sql = QStringLiteral("SELECT COUNT(*) FROM asset_file WHERE 1 = 1");
    QVariantList binds;
    if (sourceRootId.has_value()) {
        sql += QStringLiteral(" AND source_root_id = ?");
        binds.append(sourceRootId.value());
    }
    if (assetType.has_value()) {
        sql += QStringLiteral(" AND asset_type = ?");
        binds.append(static_cast<int>(assetType.value()));
    }
    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (name LIKE ? ESCAPE '\\' OR relative_path LIKE ? ESCAPE '\\')");
        const auto pattern = m_searchEngine->buildLikePattern(keyword);
        binds.append(pattern);
        binds.append(pattern);
    }

    QSqlQuery query(m_databaseManager->database());
    query.prepare(sql);
    for (const auto &bind : binds) {
        query.addBindValue(bind);
    }
    query.exec();
    return query.next() ? query.value(0).toLongLong() : 0;
}

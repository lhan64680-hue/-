#include "application/SearchDocumentSyncService.h"

#include "core/search/SemanticSearchIndexService.h"
#include "infrastructure/db/GlobalDatabaseManager.h"
#include "shared/Formatters.h"

#include <QtConcurrent>

#include <QMap>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTimer>

#include <limits>
#include <utility>

namespace {
void appendText(QStringList *parts, const QVariant &value)
{
    const auto text = value.toString().simplified();
    if (!text.isEmpty()) {
        parts->append(text);
    }
}

void appendTexts(QStringList *parts, const QList<QVariant> &values)
{
    for (const auto &value : values) {
        appendText(parts, value);
    }
}

void updateSourceTimestamp(SearchDocumentInput *input, const QString &timestamp)
{
    const auto normalized = timestamp.trimmed();
    if (normalized > input->sourceUpdatedAt) {
        input->sourceUpdatedAt = normalized;
    }
}

bool executeQuery(QSqlQuery *query, const QString &context, QString *errorMessage)
{
    if (query->exec()) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("%1：%2").arg(context, query->lastError().text());
    }
    return false;
}

bool collectSearchDocuments(QSqlDatabase db,
                            QVector<SearchDocumentInput> *documents,
                            QStringList *removedDocumentKeys,
                            QString *errorMessage)
{
    QMap<QString, SearchDocumentInput> byDocumentKey;
    QHash<QString, QStringList> assetParts;

    QSqlQuery folderQuery(db);
    folderQuery.prepare(QStringLiteral(
        "SELECT folder_key, project_name, source_root_name, name, absolute_path, "
        "relative_path, parent_relative_path, normalized_date, date_anchor, updated_at "
        "FROM global_folder_node ORDER BY folder_key"));
    if (!executeQuery(&folderQuery,
                      QStringLiteral("读取文件夹搜索文档"),
                      errorMessage)) {
        return false;
    }
    while (folderQuery.next()) {
        SearchDocumentInput input;
        input.entityKey = folderQuery.value(0).toString();
        if (input.entityKey.trimmed().isEmpty()) {
            continue;
        }
        input.documentKey = QStringLiteral("folder:%1").arg(input.entityKey);
        input.documentType = SearchDocumentType::Folder;
        QStringList parts;
        appendTexts(&parts,
                    {folderQuery.value(1), folderQuery.value(2), folderQuery.value(3),
                     folderQuery.value(4), folderQuery.value(5), folderQuery.value(6),
                     folderQuery.value(7), folderQuery.value(8), QStringLiteral("文件夹 目录")});
        parts.removeDuplicates();
        input.contentText = parts.join(QLatin1Char(' '));
        input.sourceUpdatedAt = folderQuery.value(9).toString();
        byDocumentKey.insert(input.documentKey, std::move(input));
    }

    QSqlQuery assetQuery(db);
    assetQuery.prepare(QStringLiteral(
        "SELECT g.video_key, g.project_name, g.source_root_name, g.file_name, "
        "g.extension, g.absolute_path, g.relative_path, g.asset_type, "
        "g.technical_summary, g.source_text, g.modified_at, g.capture_time, g.capture_date, "
        "g.capture_time_source, g.capture_time_confidence, g.updated_at, "
        "COALESCE(r.summary, ''), COALESCE(r.keywords_json, ''), "
        "COALESCE(r.scenes_json, ''), COALESCE(r.search_text, ''), "
        "COALESCE(r.analyzed_at, ''), COALESCE(r.confirmed_at, '') "
        "FROM global_video_asset g "
        "LEFT JOIN video_analysis_result r ON r.video_key = g.video_key "
        "ORDER BY g.video_key"));
    if (!executeQuery(&assetQuery,
                      QStringLiteral("读取素材搜索文档"),
                      errorMessage)) {
        return false;
    }
    while (assetQuery.next()) {
        SearchDocumentInput input;
        input.entityKey = assetQuery.value(0).toString();
        if (input.entityKey.trimmed().isEmpty()) {
            continue;
        }
        input.documentKey = QStringLiteral("asset:%1").arg(input.entityKey);
        input.documentType = SearchDocumentType::Asset;
        QStringList parts;
        appendTexts(&parts,
                    {assetQuery.value(1), assetQuery.value(2), assetQuery.value(3),
                     assetQuery.value(4), assetQuery.value(5), assetQuery.value(6),
                     Formatters::assetTypeLabel(static_cast<AssetType>(assetQuery.value(7).toInt())),
                     assetQuery.value(8), assetQuery.value(9), assetQuery.value(10),
                     assetQuery.value(11), assetQuery.value(12), assetQuery.value(13),
                     assetQuery.value(16), assetQuery.value(17), assetQuery.value(18),
                     assetQuery.value(19)});
        assetParts.insert(input.entityKey, std::move(parts));
        input.sourceUpdatedAt = assetQuery.value(15).toString();
        updateSourceTimestamp(&input, assetQuery.value(20).toString());
        updateSourceTimestamp(&input, assetQuery.value(21).toString());
        byDocumentKey.insert(input.documentKey, std::move(input));
    }

    QSqlQuery frameQuery(db);
    frameQuery.prepare(QStringLiteral(
        "SELECT video_key, frame_number, timestamp_ms, caption, tags_json, objects_json, actions, setting_text, "
        "entities_json, ocr_text, ocr_blocks_json, analyzed_at "
        "FROM video_frame_analysis ORDER BY video_key, frame_number"));
    if (!executeQuery(&frameQuery,
                      QStringLiteral("读取逐帧搜索文档"),
                      errorMessage)) {
        return false;
    }
    while (frameQuery.next()) {
        const auto videoKey = frameQuery.value(0).toString();
        auto parts = assetParts.find(videoKey);
        const auto documentKey = QStringLiteral("asset:%1").arg(videoKey);
        auto document = byDocumentKey.find(documentKey);
        if (parts == assetParts.end() || document == byDocumentKey.end()) {
            continue;
        }
        appendTexts(&parts.value(),
                    {frameQuery.value(3), frameQuery.value(4), frameQuery.value(5),
                     frameQuery.value(6), frameQuery.value(7), frameQuery.value(8),
                     frameQuery.value(9), frameQuery.value(10)});
        updateSourceTimestamp(&document.value(), frameQuery.value(11).toString());

        SearchDocumentInput frameDocument;
        frameDocument.documentKey = QStringLiteral("frame:%1:%2")
                                        .arg(videoKey)
                                        .arg(frameQuery.value(1).toInt());
        frameDocument.documentType = SearchDocumentType::VisualEntity;
        frameDocument.entityKey = videoKey;
        QStringList frameParts;
        appendTexts(&frameParts,
                    {QStringLiteral("画面 帧"), frameQuery.value(3), frameQuery.value(4),
                     frameQuery.value(5), frameQuery.value(6), frameQuery.value(7),
                     frameQuery.value(8), frameQuery.value(9), frameQuery.value(10)});
        const auto timestampMs = frameQuery.value(2).toLongLong();
        if (timestampMs >= 0) {
            frameParts.append(QStringLiteral("时间点 %1 毫秒").arg(timestampMs));
        }
        frameParts.removeDuplicates();
        frameDocument.contentText = frameParts.join(QLatin1Char(' '));
        frameDocument.sourceUpdatedAt = frameQuery.value(11).toString();
        if (!frameDocument.contentText.trimmed().isEmpty()) {
            byDocumentKey.insert(frameDocument.documentKey, std::move(frameDocument));
        }
    }

    QSqlQuery dimensionQuery(db);
    dimensionQuery.prepare(QStringLiteral(
        "SELECT video_key, dimension_name, detail, analyzed_at "
        "FROM material_dimension_analysis ORDER BY video_key, id"));
    if (!executeQuery(&dimensionQuery,
                      QStringLiteral("读取维度搜索文档"),
                      errorMessage)) {
        return false;
    }
    while (dimensionQuery.next()) {
        const auto videoKey = dimensionQuery.value(0).toString();
        auto parts = assetParts.find(videoKey);
        auto document = byDocumentKey.find(QStringLiteral("asset:%1").arg(videoKey));
        if (parts == assetParts.end() || document == byDocumentKey.end()) {
            continue;
        }
        appendTexts(&parts.value(), {dimensionQuery.value(1), dimensionQuery.value(2)});
        updateSourceTimestamp(&document.value(), dimensionQuery.value(3).toString());
    }

    QSqlQuery dimensionFrameQuery(db);
    dimensionFrameQuery.prepare(QStringLiteral(
        "SELECT video_key, dimension_name, detail, analyzed_at "
        "FROM material_dimension_frame_analysis WHERE analysis_state = 1 "
        "ORDER BY video_key, id"));
    if (!executeQuery(&dimensionFrameQuery,
                      QStringLiteral("读取逐帧维度搜索文档"),
                      errorMessage)) {
        return false;
    }
    while (dimensionFrameQuery.next()) {
        const auto videoKey = dimensionFrameQuery.value(0).toString();
        auto parts = assetParts.find(videoKey);
        auto document = byDocumentKey.find(QStringLiteral("asset:%1").arg(videoKey));
        if (parts == assetParts.end() || document == byDocumentKey.end()) {
            continue;
        }
        appendTexts(&parts.value(), {dimensionFrameQuery.value(1), dimensionFrameQuery.value(2)});
        updateSourceTimestamp(&document.value(), dimensionFrameQuery.value(3).toString());
    }

    for (auto document = byDocumentKey.begin(); document != byDocumentKey.end(); ++document) {
        if (document->documentType != SearchDocumentType::Asset) {
            continue;
        }
        auto parts = assetParts.value(document->entityKey);
        parts.removeDuplicates();
        document->contentText = parts.join(QLatin1Char(' '));
    }

    QSet<QString> existingOwnedKeys;
    QSqlQuery existingQuery(db);
    existingQuery.prepare(QStringLiteral(
        "SELECT document_key FROM search_document WHERE document_type IN (?, ?, ?)"));
    existingQuery.addBindValue(static_cast<int>(SearchDocumentType::Folder));
    existingQuery.addBindValue(static_cast<int>(SearchDocumentType::Asset));
    existingQuery.addBindValue(static_cast<int>(SearchDocumentType::VisualEntity));
    if (!executeQuery(&existingQuery, QStringLiteral("读取现有搜索文档"), errorMessage)) {
        return false;
    }
    while (existingQuery.next()) {
        existingOwnedKeys.insert(existingQuery.value(0).toString());
    }

    documents->clear();
    documents->reserve(byDocumentKey.size());
    QSet<QString> currentKeys;
    for (auto document = byDocumentKey.begin(); document != byDocumentKey.end(); ++document) {
        currentKeys.insert(document.key());
        documents->append(std::move(document.value()));
    }
    removedDocumentKeys->clear();
    for (const auto &key : std::as_const(existingOwnedKeys)) {
        if (!currentKeys.contains(key)) {
            removedDocumentKeys->append(key);
        }
    }
    removedDocumentKeys->sort(Qt::CaseInsensitive);
    return true;
}

bool synchronizeDatabase(GlobalDatabaseManager *databaseManager,
                         SemanticSearchIndexService *semanticSearchIndexService,
                         SemanticIndexUpdateResult *result,
                         QString *errorMessage,
                         const SemanticIndexProgressCallback &progressCallback = {})
{
    if (!databaseManager || !databaseManager->isOpen() || !semanticSearchIndexService) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("搜索文档同步依赖尚未就绪");
        }
        return false;
    }
    QVector<SearchDocumentInput> documents;
    QStringList removals;
    if (!collectSearchDocuments(databaseManager->database(),
                                &documents,
                                &removals,
                                errorMessage)) {
        return false;
    }
    if (progressCallback) {
        progressCallback(0,
                         documents.size(),
                         QStringLiteral("已收集 %1 份搜索文档，正在检查增量变更")
                             .arg(documents.size()));
    }
    return semanticSearchIndexService->applyChanges(documents,
                                                    removals,
                                                    result,
                                                    errorMessage,
                                                    progressCallback);
}
}

SearchDocumentSyncService::SearchDocumentSyncService(
    GlobalDatabaseManager *globalDatabaseManager,
    SemanticSearchIndexService *semanticSearchIndexService,
    QObject *parent)
    : QObject(parent)
    , m_globalDatabaseManager(globalDatabaseManager)
    , m_semanticSearchIndexService(semanticSearchIndexService)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(250);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &SearchDocumentSyncService::startScheduledSync);
}

bool SearchDocumentSyncService::synchronizeNow(SemanticIndexUpdateResult *result,
                                               QString *errorMessage)
{
    return synchronizeDatabase(m_globalDatabaseManager,
                               m_semanticSearchIndexService,
                               result,
                               errorMessage);
}

void SearchDocumentSyncService::scheduleFullSync()
{
    if (!m_globalDatabaseManager || !m_globalDatabaseManager->isOpen()
        || !m_semanticSearchIndexService) {
        return;
    }
    if (m_running) {
        m_pending = true;
        return;
    }
    m_debounceTimer->start();
}

void SearchDocumentSyncService::startScheduledSync()
{
    if (m_running || !m_globalDatabaseManager || !m_globalDatabaseManager->isOpen()
        || !m_semanticSearchIndexService) {
        m_pending = m_running;
        return;
    }
    m_running = true;
    m_pending = false;
    emit synchronizationProgress(0, 0, QStringLiteral("正在收集语义索引文档"));
    const auto indexPath = m_semanticSearchIndexService->indexFilePath();
    QPointer<SearchDocumentSyncService> guard(this);

    auto future = QtConcurrent::run([guard, indexPath]() {
        SemanticIndexUpdateResult updateResult;
        QString errorMessage;
        bool success = false;
        GlobalDatabaseManager workerDatabaseManager;
        if (workerDatabaseManager.openDatabase(&errorMessage)) {
            {
                SemanticSearchIndexService workerIndexService(&workerDatabaseManager, indexPath);
                const SemanticIndexProgressCallback progressCallback =
                    [guard](qsizetype processed, qsizetype total, const QString &detail) {
                        if (!guard) {
                            return;
                        }
                        QMetaObject::invokeMethod(guard, [guard, processed, total, detail]() {
                            if (!guard) {
                                return;
                            }
                            emit guard->synchronizationProgress(
                                static_cast<int>(qMin<qsizetype>(processed, std::numeric_limits<int>::max())),
                                static_cast<int>(qMin<qsizetype>(total, std::numeric_limits<int>::max())),
                                detail);
                        }, Qt::QueuedConnection);
                    };
                success = synchronizeDatabase(&workerDatabaseManager,
                                              &workerIndexService,
                                              &updateResult,
                                              &errorMessage,
                                              progressCallback);
            }
            workerDatabaseManager.closeDatabase();
        }
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, success, updateResult, errorMessage]() {
            if (!guard) {
                return;
            }
            if (success && guard->m_semanticSearchIndexService) {
                guard->m_semanticSearchIndexService->discardLoadedIndex();
            }
            guard->m_running = false;
            const auto message = success
                ? QStringLiteral("搜索文档同步完成")
                : errorMessage;
            emit guard->synchronizationFinished(success,
                                                updateResult.inserted,
                                                updateResult.updated,
                                                updateResult.unchanged,
                                                updateResult.removed,
                                                message);
            if (guard->m_pending) {
                guard->m_pending = false;
                guard->m_debounceTimer->start(0);
            }
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

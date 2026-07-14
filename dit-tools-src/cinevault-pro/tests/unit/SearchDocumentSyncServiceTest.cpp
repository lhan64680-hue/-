#include "application/SearchDocumentSyncService.h"
#include "core/search/SemanticSearchIndexService.h"
#include "infrastructure/db/GlobalDatabaseManager.h"

#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace {
QString globalDatabasePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/material-center.sqlite"));
}

bool execute(QSqlDatabase db, const QString &statement, QString *errorMessage)
{
    QSqlQuery query(db);
    if (query.exec(statement)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

class Fixture {
public:
    Fixture()
    {
        QFile::remove(globalDatabasePath());
        if (!manager.openDatabase(&errorMessage)) {
            return;
        }
        valid = seed();
    }

    ~Fixture()
    {
        manager.closeDatabase();
        QFile::remove(globalDatabasePath());
    }

    GlobalDatabaseManager manager;
    bool valid = false;
    QString errorMessage;

private:
    bool seed()
    {
        const QStringList statements{
            QStringLiteral(
                "INSERT INTO project_registry(project_uuid, project_name, project_database_path, sync_status) "
                "VALUES ('project-search', '雪山广告项目', 'G:/projects/search/project.sqlite', 'success')"),
            QStringLiteral(
                "INSERT INTO global_folder_node("
                "folder_key, project_uuid, project_name, project_database_path, source_root_id, source_root_name, "
                "name, absolute_path, path_key, relative_path, normalized_date, updated_at) VALUES "
                "('folder-summit', 'project-search', '雪山广告项目', 'G:/projects/search/project.sqlite', "
                "1, '主摄影机', '山顶日出', 'G:/projects/search/2026-07-14/山顶日出', "
                "'g:/projects/search/2026-07-14/山顶日出', '2026-07-14/山顶日出', "
                "'2026-07-14', '2026-07-14T08:00:00')"),
            QStringLiteral(
                "INSERT INTO global_video_asset("
                "video_key, project_uuid, project_name, project_database_path, source_root_id, source_root_name, "
                "asset_id, file_name, extension, absolute_path, relative_path, asset_type, modified_at, "
                "technical_summary, source_text, updated_at) VALUES "
                "('asset-summit', 'project-search', '雪山广告项目', 'G:/projects/search/project.sqlite', "
                "1, '主摄影机', 1, 'clip001.mov', 'mov', 'G:/projects/search/clip001.mov', "
                "'山顶日出/clip001.mov', 1, '2026-07-14T07:00:00', 'ProRes 4K', '', "
                "'2026-07-14T08:00:00'), "
                "('asset-contract', 'project-search', '雪山广告项目', 'G:/projects/search/project.sqlite', "
                "1, '制作文档', 2, 'contract.md', 'md', 'G:/projects/search/contract.md', "
                "'docs/contract.md', 6, '2026-07-14T07:00:00', 'Markdown 文档', "
                "'品牌独家授权合同', '2026-07-14T08:00:00')"),
            QStringLiteral(
                "INSERT INTO video_analysis_result("
                "video_key, summary, keywords_json, scenes_json, search_text, analyzed_at) VALUES "
                "('asset-summit', '模特在雪山山顶观看日出', '[\"雪山\",\"日出\"]', "
                "'[\"山顶\"]', '雪山山顶日出 户外广告', '2026-07-14T09:00:00')"),
            QStringLiteral(
                "INSERT INTO video_frame_analysis("
                "video_key, frame_number, caption, tags_json, objects_json, actions, setting_text, "
                "entities_json, ocr_text, structured_profile_version, facts_complete, analysis_state, analyzed_at) VALUES "
                "('asset-summit', 1, '红色牛仔短裤模特站在雪山山顶', '[\"日出\"]', '[\"人物\"]', "
                "'观看日出', '雪山户外', "
                "'[{\"label\":\"短裤\",\"colors\":[\"红色\"],\"materials\":[\"牛仔\"],\"attributes\":[]}]', "
                "'SUMMIT', 2, 1, 1, '2026-07-14T09:05:00')"),
            QStringLiteral(
                "INSERT INTO material_dimension_analysis("
                "video_key, dimension_key, dimension_name, detail, analyzed_at) VALUES "
                "('asset-summit', 'brand', '品牌适配', '适合户外运动广告', '2026-07-14T09:10:00')")
        };
        for (const auto &statement : statements) {
            if (!execute(manager.database(), statement, &errorMessage)) {
                return false;
            }
        }
        return true;
    }
};

qint64 scalarCount(QSqlDatabase db, const QString &statement)
{
    QSqlQuery query(db);
    return query.exec(statement) && query.next() ? query.value(0).toLongLong() : -1;
}
}

class SearchDocumentSyncServiceTest : public QObject {
    Q_OBJECT

private slots:
    void aggregatesContentAndMaintainsPersistentIndexIncrementally()
    {
        Fixture fixture;
        QVERIFY2(fixture.valid, qPrintable(fixture.errorMessage));
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto indexPath = QDir(temp.path()).filePath(QStringLiteral("documents.usearch"));
        SemanticSearchIndexService semanticIndex(&fixture.manager, indexPath);
        SearchDocumentSyncService syncService(&fixture.manager, &semanticIndex);

        QString errorMessage;
        SemanticIndexUpdateResult first;
        QVERIFY2(syncService.synchronizeNow(&first, &errorMessage), qPrintable(errorMessage));
        QCOMPARE(first.inserted, 4);
        QCOMPARE(scalarCount(fixture.manager.database(),
                             QStringLiteral("SELECT COUNT(*) FROM search_document")),
                 qint64{4});

        QSqlQuery contentQuery(fixture.manager.database());
        contentQuery.prepare(QStringLiteral(
            "SELECT content_text, source_updated_at FROM search_document WHERE document_key = ?"));
        contentQuery.addBindValue(QStringLiteral("asset:asset-summit"));
        QVERIFY(contentQuery.exec());
        QVERIFY(contentQuery.next());
        const auto content = contentQuery.value(0).toString();
        QVERIFY(content.contains(QStringLiteral("红色牛仔短裤")));
        QVERIFY(content.contains(QStringLiteral("适合户外运动广告")));
        QVERIFY(content.contains(QStringLiteral("SUMMIT")));
        QCOMPARE(contentQuery.value(1).toString(), QStringLiteral("2026-07-14T09:10:00"));

        QSqlQuery frameDocumentQuery(fixture.manager.database());
        frameDocumentQuery.prepare(QStringLiteral(
            "SELECT document_type, entity_key, content_text FROM search_document "
            "WHERE document_key = 'frame:asset-summit:1'"));
        QVERIFY(frameDocumentQuery.exec());
        QVERIFY(frameDocumentQuery.next());
        QCOMPARE(frameDocumentQuery.value(0).toInt(),
                 static_cast<int>(SearchDocumentType::VisualEntity));
        QCOMPARE(frameDocumentQuery.value(1).toString(), QStringLiteral("asset-summit"));
        QVERIFY(frameDocumentQuery.value(2).toString().contains(QStringLiteral("红色牛仔短裤")));

        const auto semanticHits = semanticIndex.search(QStringLiteral("雪山山顶日出户外广告"),
                                                       3,
                                                       &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
        QVERIFY(std::any_of(semanticHits.cbegin(), semanticHits.cend(), [](const auto &hit) {
            return hit.documentKey == QStringLiteral("asset:asset-summit");
        }));

        SemanticIndexUpdateResult unchanged;
        QVERIFY2(syncService.synchronizeNow(&unchanged, &errorMessage), qPrintable(errorMessage));
        QCOMPARE(unchanged.unchanged, 4);
        QCOMPARE(unchanged.inserted, 0);
        QCOMPARE(unchanged.updated, 0);

        QVERIFY(execute(fixture.manager.database(),
                        QStringLiteral(
                            "UPDATE video_analysis_result SET summary = '模特在金色晨光中的雪山山顶观看日出', "
                            "analyzed_at = '2026-07-14T10:00:00' WHERE video_key = 'asset-summit'"),
                        &errorMessage));
        SemanticIndexUpdateResult updated;
        QVERIFY2(syncService.synchronizeNow(&updated, &errorMessage), qPrintable(errorMessage));
        QCOMPARE(updated.updated, 1);
        QCOMPARE(updated.unchanged, 3);

        QVERIFY(execute(fixture.manager.database(),
                        QStringLiteral("DELETE FROM global_video_asset WHERE video_key = 'asset-contract'"),
                        &errorMessage));
        SemanticIndexUpdateResult removed;
        QVERIFY2(syncService.synchronizeNow(&removed, &errorMessage), qPrintable(errorMessage));
        QCOMPARE(removed.removed, 1);
        QCOMPARE(scalarCount(fixture.manager.database(),
                             QStringLiteral("SELECT COUNT(*) FROM search_document")),
                 qint64{3});
        QCOMPARE(scalarCount(fixture.manager.database(),
                             QStringLiteral("SELECT COUNT(*) FROM search_document "
                                            "WHERE document_key = 'asset:asset-contract'")),
                 qint64{0});
        QVERIFY(QFile::exists(indexPath));
    }

    void scheduledSyncUsesBackgroundConnectionAndPublishesCompletion()
    {
        Fixture fixture;
        QVERIFY2(fixture.valid, qPrintable(fixture.errorMessage));
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SemanticSearchIndexService semanticIndex(
            &fixture.manager,
            QDir(temp.path()).filePath(QStringLiteral("background.usearch")));
        SearchDocumentSyncService syncService(&fixture.manager, &semanticIndex);
        QSignalSpy completion(&syncService,
                              &SearchDocumentSyncService::synchronizationFinished);

        syncService.scheduleFullSync();

        QVERIFY2(completion.wait(10000), "后台搜索文档同步未在超时前完成");
        const auto arguments = completion.takeFirst();
        QVERIFY(arguments.at(0).toBool());
        QCOMPARE(arguments.at(1).toInt(), 4);
        QCOMPARE(scalarCount(fixture.manager.database(),
                             QStringLiteral("SELECT COUNT(*) FROM search_document")),
                 qint64{4});
    }
};

QTEST_GUILESS_MAIN(SearchDocumentSyncServiceTest)

#include "SearchDocumentSyncServiceTest.moc"

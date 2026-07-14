#include "application/ImportService.h"
#include "application/JobService.h"
#include "core/jobs/JobEngine.h"
#include "core/scan/FileTypeService.h"
#include "core/scan/ScanEngine.h"
#include "infrastructure/db/DatabaseManager.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace {
void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(content), content.size());
}

qint64 countAssets(QSqlDatabase db, QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM asset_file"));
    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }
    return query.value(0).toLongLong();
}

int scanVersion(QSqlDatabase db, QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT scan_version FROM source_root LIMIT 1"));
    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }
    return query.value(0).toInt();
}

QVariantList folderRow(QSqlDatabase db, const QString &relativePath, QString *errorMessage)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT parent_relative_path, depth, direct_file_count, recursive_file_count, normalized_date, date_anchor, path_key "
        "FROM folder_node WHERE relative_path = ?"));
    query.addBindValue(relativePath.isNull() ? QStringLiteral("") : relativePath);
    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }
    QVariantList row;
    for (int index = 0; index < 7; ++index) {
        row.append(query.value(index));
    }
    return row;
}
}

class ImportServiceLegacyRescanTest : public QObject {
    Q_OBJECT

private slots:
    void rescanLegacySourceRoots_rebuildsOldVideoOnlyIndex()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto projectDb = QDir(temp.path()).filePath(QStringLiteral("project.cvdb"));
        const auto sourcePath = QDir(temp.path()).filePath(QStringLiteral("CardA"));
        writeFile(QDir(sourcePath).filePath(QStringLiteral("A001.mov")), "video");
        writeFile(QDir(sourcePath).filePath(QStringLiteral("Audio/A001.wav")), "audio");
        writeFile(QDir(sourcePath).filePath(QStringLiteral("Notes/shot.md")), "notes");
        writeFile(QDir(sourcePath).filePath(QStringLiteral("2026-07-14/CameraA/B001.mov")), "video2");

        DatabaseManager databaseManager;
        QString errorMessage;
        QVERIFY2(databaseManager.openProjectDatabase(projectDb, &errorMessage), qPrintable(errorMessage));

        QSqlQuery sourceInsert(databaseManager.database());
        sourceInsert.prepare(QStringLiteral(
            "INSERT INTO source_root "
            "(name, path, status, total_files, total_folders, total_size_bytes, video_count, audio_count, image_count, other_count, warning_count, scan_version, created_at, updated_at) "
            "VALUES ('CardA', ?, 'ok', 1, 0, 5, 1, 0, 0, 0, 0, 0, '2026-07-04T10:00:00', '2026-07-04T10:00:00')"));
        sourceInsert.addBindValue(QFileInfo(sourcePath).absoluteFilePath());
        QVERIFY2(sourceInsert.exec(), qPrintable(sourceInsert.lastError().text()));
        const auto sourceRootId = sourceInsert.lastInsertId().toLongLong();

        QSqlQuery oldAsset(databaseManager.database());
        oldAsset.prepare(QStringLiteral(
            "INSERT INTO asset_file "
            "(source_root_id, name, extension, absolute_path, relative_path, parent_path, asset_type, size_bytes, modified_at, is_readable, created_at) "
            "VALUES (?, 'A001.mov', 'mov', ?, 'A001.mov', ?, ?, 5, '2026-07-04T10:00:00', 1, '2026-07-04T10:00:00')"));
        oldAsset.addBindValue(sourceRootId);
        oldAsset.addBindValue(QDir(sourcePath).filePath(QStringLiteral("A001.mov")));
        oldAsset.addBindValue(QFileInfo(sourcePath).absoluteFilePath());
        oldAsset.addBindValue(static_cast<int>(AssetType::Video));
        QVERIFY2(oldAsset.exec(), qPrintable(oldAsset.lastError().text()));
        QCOMPARE(countAssets(databaseManager.database(), &errorMessage), qint64{1});

        JobEngine jobEngine(&databaseManager);
        JobService jobService(&jobEngine);
        ScanEngine scanEngine(&databaseManager, &jobEngine, nullptr, nullptr);
        ImportService importService(&databaseManager, &jobService, &scanEngine);
        QSignalSpy scanFinished(&scanEngine, &ScanEngine::scanFinished);

        importService.rescanLegacySourceRoots();

        QVERIFY(scanFinished.wait(10000));
        scanEngine.waitForIdle();
        QCoreApplication::processEvents();
        QCOMPARE(countAssets(databaseManager.database(), &errorMessage), qint64{4});
        QCOMPARE(scanVersion(databaseManager.database(), &errorMessage), ScanEngine::CurrentScanVersion);

        const auto root = folderRow(databaseManager.database(), QString(), &errorMessage);
        QCOMPARE(root.size(), 7);
        QCOMPARE(root.at(1).toInt(), 0);
        QCOMPARE(root.at(2).toLongLong(), qint64{1});
        QCOMPARE(root.at(3).toLongLong(), qint64{4});
        QVERIFY(!root.at(6).toString().isEmpty());

        const auto dateFolder = folderRow(databaseManager.database(), QStringLiteral("2026-07-14"), &errorMessage);
        QCOMPARE(dateFolder.size(), 7);
        QCOMPARE(dateFolder.at(0).toString(), QString());
        QCOMPARE(dateFolder.at(1).toInt(), 1);
        QCOMPARE(dateFolder.at(2).toLongLong(), qint64{0});
        QCOMPARE(dateFolder.at(3).toLongLong(), qint64{1});
        QCOMPARE(dateFolder.at(4).toString(), QStringLiteral("2026-07-14"));
        QCOMPARE(dateFolder.at(5).toString(), QStringLiteral("2026-07-14"));

        const auto cameraFolder = folderRow(databaseManager.database(), QStringLiteral("2026-07-14/CameraA"), &errorMessage);
        QCOMPARE(cameraFolder.size(), 7);
        QCOMPARE(cameraFolder.at(0).toString(), QStringLiteral("2026-07-14"));
        QCOMPARE(cameraFolder.at(1).toInt(), 2);
        QCOMPARE(cameraFolder.at(2).toLongLong(), qint64{1});
        QCOMPARE(cameraFolder.at(3).toLongLong(), qint64{1});
        QCOMPARE(cameraFolder.at(4).toString(), QStringLiteral("2026-07-14"));
        QCOMPARE(cameraFolder.at(5).toString(), QStringLiteral("2026-07-14"));
    }
};

QTEST_GUILESS_MAIN(ImportServiceLegacyRescanTest)

#include "ImportServiceLegacyRescanTest.moc"

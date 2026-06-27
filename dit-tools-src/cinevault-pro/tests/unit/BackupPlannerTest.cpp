#include "core/backup/BackupPlanner.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace {
void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(content), content.size());
}

BackupSource directorySource(const QString &path)
{
    BackupSource source;
    source.kind = BackupSourceKind::Directory;
    source.path = path;
    return source;
}

BackupSource fileSource(const QString &path)
{
    BackupSource source;
    source.kind = BackupSourceKind::File;
    source.path = path;
    return source;
}

BackupDestination destination(const QString &path)
{
    BackupDestination target;
    target.rootPath = path;
    return target;
}
}

class BackupPlannerTest : public QObject {
    Q_OBJECT

private slots:
    void defaultBatchName_sanitizesProjectName()
    {
        const auto name = BackupPlanner::defaultBatchName(QStringLiteral("Demo:Day/01"),
                                                          QDateTime(QDate(2026, 6, 27), QTime(9, 8, 7)));
        QCOMPARE(name, QStringLiteral("Demo_Day_01_20260627_090807"));
    }

    void buildPlan_collectsFilesAndDestinations()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto sourceRoot = QDir(temp.path()).filePath(QStringLiteral("CardA"));
        const auto looseFile = QDir(temp.path()).filePath(QStringLiteral("look.cdl"));
        const auto targetA = QDir(temp.path()).filePath(QStringLiteral("TargetA"));
        const auto targetB = QDir(temp.path()).filePath(QStringLiteral("TargetB"));
        QDir().mkpath(targetA);
        QDir().mkpath(targetB);
        writeFile(QDir(sourceRoot).filePath(QStringLiteral("A001.mov")), "video");
        writeFile(QDir(sourceRoot).filePath(QStringLiteral("Audio/A001.wav")), "audio");
        writeFile(looseFile, "lut");

        BackupRequest request;
        request.projectName = QStringLiteral("Project");
        request.batchName = QStringLiteral("Batch01");
        request.sources = {directorySource(sourceRoot), fileSource(looseFile)};
        request.destinations = {destination(targetA), destination(targetB)};

        const auto plan = BackupPlanner().buildPlan(request);

        QVERIFY2(plan.valid, qPrintable(plan.errors.join(QStringLiteral("; "))));
        QCOMPARE(plan.totalFiles, 3);
        QCOMPARE(plan.destinations.size(), 2);
        QCOMPARE(plan.tasks.size(), 2);
        QCOMPARE(plan.destinations.at(0).plannedRootPath, QDir(targetA).filePath(QStringLiteral("Batch01")).replace(QLatin1Char('\\'), QLatin1Char('/')));
        QStringList relativePaths;
        for (const auto &file : plan.files) {
            relativePaths.append(file.relativePath);
        }
        QVERIFY(relativePaths.contains(QStringLiteral("CardA/A001.mov")));
        QVERIFY(relativePaths.contains(QStringLiteral("CardA/Audio/A001.wav")));
        QVERIFY(relativePaths.contains(QStringLiteral("look.cdl")));
    }

    void buildPlan_rejectsNestedDestination()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto sourceRoot = QDir(temp.path()).filePath(QStringLiteral("CardA"));
        const auto nestedTarget = QDir(sourceRoot).filePath(QStringLiteral("Backup"));
        QDir().mkpath(nestedTarget);
        writeFile(QDir(sourceRoot).filePath(QStringLiteral("A001.mov")), "video");

        BackupRequest request;
        request.projectName = QStringLiteral("Project");
        request.sources = {directorySource(sourceRoot)};
        request.destinations = {destination(nestedTarget)};

        const auto plan = BackupPlanner().buildPlan(request);

        QVERIFY(!plan.valid);
        QVERIFY(plan.errors.join(QStringLiteral("\n")).contains(QStringLiteral("备份目的地位于源内部")));
    }

    void buildPlan_rejectsSourceInsideDestination()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto targetRoot = QDir(temp.path()).filePath(QStringLiteral("Archive"));
        const auto sourceRoot = QDir(targetRoot).filePath(QStringLiteral("CardA"));
        QDir().mkpath(targetRoot);
        writeFile(QDir(sourceRoot).filePath(QStringLiteral("A001.mov")), "video");

        BackupRequest request;
        request.projectName = QStringLiteral("Project");
        request.sources = {directorySource(sourceRoot)};
        request.destinations = {destination(targetRoot)};

        const auto plan = BackupPlanner().buildPlan(request);

        QVERIFY(!plan.valid);
        QVERIFY(plan.errors.join(QStringLiteral("\n")).contains(QStringLiteral("待备份源位于目的地内部")));
    }

    void buildPlan_rejectsDuplicateDestinations()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto sourceRoot = QDir(temp.path()).filePath(QStringLiteral("CardA"));
        const auto targetRoot = QDir(temp.path()).filePath(QStringLiteral("Archive"));
        QDir().mkpath(targetRoot);
        writeFile(QDir(sourceRoot).filePath(QStringLiteral("A001.mov")), "video");

        BackupRequest request;
        request.projectName = QStringLiteral("Project");
        request.sources = {directorySource(sourceRoot)};
        request.destinations = {destination(targetRoot), destination(targetRoot)};

        const auto plan = BackupPlanner().buildPlan(request);

        QVERIFY(!plan.valid);
        QVERIFY(plan.errors.join(QStringLiteral("\n")).contains(QStringLiteral("备份目的地重复")));
    }

    void buildPlan_rejectsInsufficientSpace()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto sourceRoot = QDir(temp.path()).filePath(QStringLiteral("CardA"));
        const auto targetRoot = QDir(temp.path()).filePath(QStringLiteral("Archive"));
        QDir().mkpath(targetRoot);
        writeFile(QDir(sourceRoot).filePath(QStringLiteral("A001.mov")), "video-data");
        auto target = destination(targetRoot);
        target.availableBytes = 1;

        BackupRequest request;
        request.projectName = QStringLiteral("Project");
        request.sources = {directorySource(sourceRoot)};
        request.destinations = {target};

        const auto plan = BackupPlanner().buildPlan(request);

        QVERIFY(!plan.valid);
        QVERIFY(plan.errors.join(QStringLiteral("\n")).contains(QStringLiteral("备份目的地空间不足")));
    }
};

QTEST_MAIN(BackupPlannerTest)

#include "BackupPlannerTest.moc"

#include "application/DocumentPreviewService.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {
void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(content), static_cast<qint64>(content.size()));
}
}

class DocumentPreviewServiceTest : public QObject {
    Q_OBJECT

private slots:
    void extractTextForSummary_readsPlainTextLikeFiles_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QByteArray>("content");
        QTest::addColumn<QString>("expected");

        QTest::newRow("txt") << QStringLiteral("notes.txt")
                              << QByteArrayLiteral("scene notes include sunset keyword")
                              << QStringLiteral("sunset keyword");
        QTest::newRow("markdown") << QStringLiteral("brief.md")
                                   << QByteArrayLiteral("# Brief\nexclusive license notes")
                                   << QStringLiteral("exclusive license");
        QTest::newRow("json") << QStringLiteral("payload.json")
                               << QByteArrayLiteral("{\"shot\":\"kitchen\",\"tag\":\"macro\"}")
                               << QStringLiteral("macro");
        QTest::newRow("csv") << QStringLiteral("sheet.csv")
                              << QByteArrayLiteral("name,value\nasset,searchable-csv-token\n")
                              << QStringLiteral("searchable-csv-token");
    }

    void extractTextForSummary_readsPlainTextLikeFiles()
    {
        QFETCH(QString, fileName);
        QFETCH(QByteArray, content);
        QFETCH(QString, expected);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const auto path = QDir(tempDir.path()).filePath(fileName);
        writeFile(path, content);

        bool truncated = true;
        QString errorMessage;
        const auto text = DocumentPreviewService::extractTextForSummary(path, &truncated, &errorMessage);

        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
        QVERIFY2(text.contains(expected), qPrintable(text));
        QVERIFY(!truncated);
    }

    void extractTextForSummary_truncatesLargeText()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const auto path = QDir(tempDir.path()).filePath(QStringLiteral("large.txt"));
        writeFile(path, QByteArray(70000, 'a'));

        bool truncated = false;
        QString errorMessage;
        const auto text = DocumentPreviewService::extractTextForSummary(path, &truncated, &errorMessage);

        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
        QVERIFY(truncated);
        QCOMPARE(text.size(), 64000);
    }

    void extractTextForSummary_skipsMetadataOnlyFormats_data()
    {
        QTest::addColumn<QString>("fileName");

        QTest::newRow("pdf") << QStringLiteral("scan.pdf");
        QTest::newRow("doc") << QStringLiteral("legacy.doc");
        QTest::newRow("xls") << QStringLiteral("legacy.xls");
        QTest::newRow("ppt") << QStringLiteral("legacy.ppt");
    }

    void extractTextForSummary_skipsMetadataOnlyFormats()
    {
        QFETCH(QString, fileName);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const auto path = QDir(tempDir.path()).filePath(fileName);
        writeFile(path, QByteArrayLiteral("placeholder"));

        bool truncated = true;
        QString errorMessage;
        const auto text = DocumentPreviewService::extractTextForSummary(path, &truncated, &errorMessage);

        QVERIFY(text.isEmpty());
        QVERIFY(!truncated);
        QVERIFY(errorMessage.contains(QStringLiteral("只进入元数据搜索")));
    }
};

QTEST_MAIN(DocumentPreviewServiceTest)

#include "DocumentPreviewServiceTest.moc"

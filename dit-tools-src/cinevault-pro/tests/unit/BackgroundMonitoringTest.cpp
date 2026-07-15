#include "application/SourceChangeMonitor.h"
#include "application/SystemIdleMonitor.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

class BackgroundMonitoringTest : public QObject {
    Q_OBJECT

private slots:
    void systemIdleMonitorUsesSystemThresholdAndResumes()
    {
        qint64 idleDuration = 0;
        SystemIdleMonitor monitor;
        monitor.setIdleDurationProviderForTesting([&idleDuration]() { return idleDuration; });
        QSignalSpy becameIdle(&monitor, &SystemIdleMonitor::becameIdle);
        QSignalSpy activityResumed(&monitor, &SystemIdleMonitor::activityResumed);

        monitor.start(1000, 60000);
        QVERIFY(monitor.isActive());
        QVERIFY(!monitor.isIdle());

        idleDuration = 999;
        monitor.pollNow();
        QCOMPARE(becameIdle.count(), 0);

        idleDuration = 1000;
        monitor.pollNow();
        QVERIFY(monitor.isIdle());
        QCOMPARE(becameIdle.count(), 1);

        idleDuration = 5000;
        monitor.pollNow();
        QCOMPARE(becameIdle.count(), 1);

        idleDuration = 20;
        monitor.pollNow();
        QVERIFY(!monitor.isIdle());
        QCOMPARE(activityResumed.count(), 1);
        monitor.stop();
        QVERIFY(!monitor.isActive());
    }

    void sourceMonitorDetectsRecursiveFileChanges()
    {
#ifndef Q_OS_WIN
        QSKIP("递归目录通知契约当前针对 Windows 桌面版。");
#else
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto nestedPath = QDir(temp.path()).filePath(QStringLiteral("nested/deep"));
        QVERIFY(QDir().mkpath(nestedPath));

        SourceRoot sourceRoot;
        sourceRoot.id = 42;
        sourceRoot.name = QStringLiteral("Monitor Test");
        sourceRoot.path = temp.path();

        SourceChangeMonitor monitor;
        QSignalSpy changed(&monitor, &SourceChangeMonitor::sourceChanged);
        QSignalSpy unavailable(&monitor, &SourceChangeMonitor::sourceUnavailable);
        monitor.setSourceRoots({sourceRoot});
        QCOMPARE(monitor.watchedSourceCount(), 1);
        QTest::qWait(250);
        QCOMPARE(unavailable.count(), 0);

        QFile file(QDir(nestedPath).filePath(QStringLiteral("new-file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("recursive change"), qint64{16});
        file.close();

        QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 5000);
        QCOMPARE(changed.first().at(0).toLongLong(), qint64{42});
        QCOMPARE(QDir::cleanPath(changed.first().at(1).toString()),
                 QDir::cleanPath(temp.path()));
        monitor.stop();
        QCOMPARE(monitor.watchedSourceCount(), 0);
#endif
    }
};

QTEST_GUILESS_MAIN(BackgroundMonitoringTest)

#include "BackgroundMonitoringTest.moc"

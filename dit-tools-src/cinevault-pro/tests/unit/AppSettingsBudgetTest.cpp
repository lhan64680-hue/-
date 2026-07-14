#include "infrastructure/config/AppSettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class AppSettingsBudgetTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_settingsRoot.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("CineVaultTests"));
        QCoreApplication::setApplicationName(QStringLiteral("AppSettingsBudgetTest"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,
                           QSettings::UserScope,
                           m_settingsRoot.path());
    }

    void cleanup()
    {
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    void defaultsEnableAssistanceWithFiniteBudget()
    {
        AppSettings settings;

        QVERIFY(settings.searchAssistantEnabled());
        QVERIFY(settings.frameRerankEnabled());
        QVERIFY(!settings.localOnlySearch());
        QVERIFY(settings.allowSearchFrameUpload());
        QCOMPARE(settings.dailySearchModelCallLimit(), 100);
        QCOMPARE(settings.searchModelCallsForDate(QDate(2026, 7, 14)), 0);
        QVERIFY(settings.canUseSearchModel(QDate(2026, 7, 14)));
    }

    void zeroLimitAllowsUnlimitedCalls()
    {
        AppSettings settings;
        const QDate date(2026, 7, 14);
        settings.setDailySearchModelCallLimit(0);

        for (int index = 0; index < 256; ++index) {
            QVERIFY(settings.tryConsumeSearchModelCall(date));
        }

        QCOMPARE(settings.searchModelCallsForDate(date), 256);
        QVERIFY(settings.canUseSearchModel(date));
    }

    void finiteBudgetStopsAtLimit()
    {
        AppSettings settings;
        const QDate date(2026, 7, 14);
        settings.setDailySearchModelCallLimit(2);

        QVERIFY(settings.tryConsumeSearchModelCall(date));
        QVERIFY(settings.tryConsumeSearchModelCall(date));
        QCOMPARE(settings.searchModelCallsForDate(date), 2);
        QVERIFY(!settings.canUseSearchModel(date));
        QVERIFY(!settings.tryConsumeSearchModelCall(date));
        QCOMPARE(settings.searchModelCallsForDate(date), 2);
    }

    void nextDateStartsWithFreshBudget()
    {
        AppSettings settings;
        const QDate firstDate(2026, 7, 14);
        const QDate nextDate(2026, 7, 15);
        settings.setDailySearchModelCallLimit(1);

        QVERIFY(settings.tryConsumeSearchModelCall(firstDate));
        QVERIFY(!settings.canUseSearchModel(firstDate));
        QCOMPARE(settings.searchModelCallsForDate(nextDate), 0);
        QVERIFY(settings.canUseSearchModel(nextDate));
        QVERIFY(settings.tryConsumeSearchModelCall(nextDate));
        QCOMPARE(settings.searchModelCallsForDate(nextDate), 1);
    }

    void searchSettingsAndBudgetPersist()
    {
        const QDate date(2026, 7, 14);
        {
            AppSettings settings;
            settings.setSearchAssistantEnabled(false);
            settings.setFrameRerankEnabled(false);
            settings.setLocalOnlySearch(true);
            settings.setAllowSearchFrameUpload(false);
            settings.setDailySearchModelCallLimit(7);
            QVERIFY(settings.tryConsumeSearchModelCall(date));
            settings.sync();
        }

        AppSettings restored;
        QVERIFY(!restored.searchAssistantEnabled());
        QVERIFY(!restored.frameRerankEnabled());
        QVERIFY(restored.localOnlySearch());
        QVERIFY(!restored.allowSearchFrameUpload());
        QCOMPARE(restored.dailySearchModelCallLimit(), 7);
        QCOMPARE(restored.searchModelCallsForDate(date), 1);
    }

private:
    QTemporaryDir m_settingsRoot;
};

QTEST_APPLESS_MAIN(AppSettingsBudgetTest)

#include "AppSettingsBudgetTest.moc"

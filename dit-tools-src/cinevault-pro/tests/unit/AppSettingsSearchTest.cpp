#include "infrastructure/config/AppSettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class AppSettingsSearchTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_settingsRoot.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("CineVaultTests"));
        QCoreApplication::setApplicationName(QStringLiteral("AppSettingsSearchTest"));
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

    void defaultsEnableUnlimitedAssistanceAndQuickSearch()
    {
        AppSettings settings;

        QVERIFY(settings.searchAssistantEnabled());
        QVERIFY(settings.frameRerankEnabled());
        QVERIFY(!settings.localOnlySearch());
        QVERIFY(settings.allowSearchFrameUpload());
        QVERIFY(settings.quickSearchEnabled());
        QCOMPARE(settings.quickSearchShortcut(), QStringLiteral("Alt+Space"));
        QVERIFY(!settings.startAtLogin());
    }

    void searchAndQuickSearchSettingsPersist()
    {
        {
            AppSettings settings;
            settings.setSearchAssistantEnabled(false);
            settings.setFrameRerankEnabled(false);
            settings.setLocalOnlySearch(true);
            settings.setAllowSearchFrameUpload(false);
            settings.setQuickSearchEnabled(false);
            settings.setQuickSearchShortcut(QStringLiteral("Ctrl+Shift+K"));
            settings.setStartAtLogin(true);
            settings.sync();
        }

        AppSettings restored;
        QVERIFY(!restored.searchAssistantEnabled());
        QVERIFY(!restored.frameRerankEnabled());
        QVERIFY(restored.localOnlySearch());
        QVERIFY(!restored.allowSearchFrameUpload());
        QVERIFY(!restored.quickSearchEnabled());
        QCOMPARE(restored.quickSearchShortcut(), QStringLiteral("Ctrl+Shift+K"));
        QVERIFY(restored.startAtLogin());
    }

    void emptyShortcutFallsBackToDefault()
    {
        AppSettings settings;
        settings.setQuickSearchShortcut(QStringLiteral("   "));
        QCOMPARE(settings.quickSearchShortcut(), QStringLiteral("Alt+Space"));
    }

private:
    QTemporaryDir m_settingsRoot;
};

QTEST_APPLESS_MAIN(AppSettingsSearchTest)

#include "AppSettingsSearchTest.moc"

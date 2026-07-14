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
        QVERIFY(!settings.hasQuickSearchWindowPosition());
        QVERIFY(!settings.startAtLogin());
        QCOMPARE(settings.closeButtonBehavior(), 0);
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
            settings.setQuickSearchWindowPosition(QPoint(-820, 135));
            settings.setStartAtLogin(true);
            settings.setCloseButtonBehavior(1);
            settings.sync();
        }

        AppSettings restored;
        QVERIFY(!restored.searchAssistantEnabled());
        QVERIFY(!restored.frameRerankEnabled());
        QVERIFY(restored.localOnlySearch());
        QVERIFY(!restored.allowSearchFrameUpload());
        QVERIFY(!restored.quickSearchEnabled());
        QCOMPARE(restored.quickSearchShortcut(), QStringLiteral("Ctrl+Shift+K"));
        QVERIFY(restored.hasQuickSearchWindowPosition());
        QCOMPARE(restored.quickSearchWindowPosition(), QPoint(-820, 135));
        QVERIFY(restored.startAtLogin());
        QCOMPARE(restored.closeButtonBehavior(), 1);
    }

    void emptyShortcutFallsBackToDefault()
    {
        AppSettings settings;
        settings.setQuickSearchShortcut(QStringLiteral("   "));
        QCOMPARE(settings.quickSearchShortcut(), QStringLiteral("Alt+Space"));
    }

    void invalidCloseButtonBehaviorFallsBackToAsk()
    {
        QSettings rawSettings;
        rawSettings.setValue(QStringLiteral("ui/closeButtonBehavior"), 99);
        rawSettings.sync();

        AppSettings settings;
        QCOMPARE(settings.closeButtonBehavior(), 0);
        settings.setCloseButtonBehavior(2);
        QCOMPARE(settings.closeButtonBehavior(), 2);
    }

private:
    QTemporaryDir m_settingsRoot;
};

QTEST_APPLESS_MAIN(AppSettingsSearchTest)

#include "AppSettingsSearchTest.moc"

#include "infrastructure/config/AppSettings.h"
#include "ui/window/QuickSearchController.h"

#include <QApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class QuickSearchControllerTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_settingsRoot.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("CineVaultTests"));
        QCoreApplication::setApplicationName(QStringLiteral("QuickSearchControllerTest"));
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

    void normalizesDefaultAndRecordedShortcuts()
    {
        QCOMPARE(QuickSearchController::normalizedShortcut(QStringLiteral("Alt+Space")),
                 QStringLiteral("Alt+Space"));
        QCOMPARE(QuickSearchController::shortcutFromKeyEvent(Qt::Key_K,
                                                             Qt::ControlModifier | Qt::ShiftModifier),
                 QStringLiteral("Ctrl+Shift+K"));
        QVERIFY(QuickSearchController::normalizedShortcut(QStringLiteral("Space")).isEmpty());
    }

    void registersDefaultHotkeyAndDispatchesActivation()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows global hotkey integration test");
#else
        AppSettings settings;
        settings.setQuickSearchEnabled(false);
        QuickSearchController controller(&settings);

        QString errorMessage;
        QVERIFY2(controller.applyShortcutConfiguration(true,
                                                       QStringLiteral("Alt+Space"),
                                                       &errorMessage),
                 qPrintable(errorMessage));
        QCOMPARE(controller.shortcut(), QStringLiteral("Alt+Space"));
        QVERIFY(controller.shortcutStatusText().contains(QStringLiteral("已启用")));

        QSignalSpy activationSpy(&controller, &QuickSearchController::quickSearchRequested);
        QVERIFY(PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY, 0x4356, 0));
        QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 2000);
#endif
    }

    void clampsRememberedPositionAcrossMultipleScreens()
    {
        const QList<QRect> screens{
            QRect(0, 0, 1920, 1080),
            QRect(1920, 0, 1280, 1024)
        };
        QCOMPARE(QuickSearchController::clampWindowPosition(QPoint(4000, 2000),
                                                            QSize(820, 650),
                                                            screens,
                                                            QPoint(2000, 100)),
                 QPoint(2380, 374));
        QCOMPARE(QuickSearchController::clampWindowPosition(QPoint(2100, 100),
                                                            QSize(820, 650),
                                                            screens,
                                                            QPoint(100, 100)),
                 QPoint(2100, 100));
    }

private:
    QTemporaryDir m_settingsRoot;
};

QTEST_MAIN(QuickSearchControllerTest)

#include "QuickSearchControllerTest.moc"

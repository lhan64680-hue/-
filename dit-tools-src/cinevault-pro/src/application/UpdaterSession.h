#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;
class QTimer;

struct UpdaterInstallSession {
    QString sessionId;
    QString versionTag;
    QString installerPath;
    QString installRoot;
    QString executableName;
    qint64 oldProcessId = 0;
};

struct UpdaterProgressEvent {
    int stepIndex = 0;
    QString stepLabel;
    QString message;
    QString substep;
    bool isError = false;
    bool isSuccess = false;
};

class UpdaterSessionRunner : public QObject {
    Q_OBJECT

public:
    explicit UpdaterSessionRunner(QObject *parent = nullptr);

    static QStringList buildArguments(const UpdaterInstallSession &session);
    static bool parseArguments(const QStringList &arguments,
                               UpdaterInstallSession *session,
                               QString *errorMessage = nullptr);
    static bool launchDetached(const QString &versionTag,
                               const QString &installerPath,
                               const QString &installRoot,
                               const QString &sourceExecutablePath,
                               qint64 oldProcessId,
                               QString *errorMessage = nullptr);

    void start(const UpdaterInstallSession &session);

signals:
    void progressChanged(const UpdaterProgressEvent &event);
    void finished(bool success, const QString &message);

private:
    static QString argumentValue(const QStringList &arguments, const QString &prefix);
    static QString safeSessionId(const QString &value);
    static bool stageRuntime(const QString &sessionId,
                             const QString &sourceRoot,
                             const QString &sourceExecutablePath,
                             QString *stagedExecutablePath,
                             QString *errorMessage);
    static bool copyRuntimeDirectory(const QString &sourceRoot,
                                     const QString &targetRoot,
                                     QString *errorMessage);
    static bool processExists(qint64 processId);
    static QString powerShellLiteral(const QString &value);

    void emitProgress(int stepIndex,
                      const QString &stepLabel,
                      const QString &message,
                      const QString &substep = QString(),
                      bool isError = false,
                      bool isSuccess = false);
    void waitForOldProcess();
    void startSilentInstaller();
    void handleInstallerFinished(int exitCode);
    void completeFailure(const QString &message, const QString &substep = QString());
    void completeSuccess();

    UpdaterInstallSession m_session;
    QTimer *m_waitTimer = nullptr;
    QProcess *m_installerProcess = nullptr;
    qint64 m_waitStartedAtMs = 0;
    bool m_started = false;
    bool m_finished = false;
};

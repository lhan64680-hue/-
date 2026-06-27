#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class AppSettings;

struct UpdateReleaseInfo {
    QString versionTag;
    QString installerName;
    QString installerUrl;
    qint64 installerSize = 0;
};

class UpdateService : public QObject {
    Q_OBJECT

public:
    explicit UpdateService(AppSettings *settings, QObject *parent = nullptr);
    ~UpdateService() override;

    static QString normalizeVersionTag(const QString &versionTag);
    static int compareVersionTags(const QString &left, const QString &right);
    static QString expectedInstallerName(const QString &versionTag);
    static bool parseLatestRelease(const QByteArray &payload, UpdateReleaseInfo *info, QString *errorMessage);
    static QString latestReleaseStatusMessage(int statusCode, const QString &networkErrorString);

    QString currentVersionTag() const;
    bool isBusy() const;
    bool hasPendingUpdate() const;

    void beginStartupFlow();
    void checkForUpdates(bool manual);
    bool installPendingUpdateNow(QString *errorMessage);

signals:
    void busyChanged();
    void statusMessageChanged(const QString &message);
    void updateReady(const QString &versionTag, const QString &installerPath, bool manual);

private:
    void setBusy(bool busy);
    void setStatusMessage(const QString &message);
    void clearPendingUpdateIfCurrentOrMissing();
    bool readPendingUpdate(QString *versionTag, QString *installerPath) const;
    bool useExistingInstaller(const UpdateReleaseInfo &release, bool manual);
    void startInstallerDownload(const UpdateReleaseInfo &release, bool manual);
    void finishCheckProcess(int exitCode, QProcess::ExitStatus exitStatus);
    void finishDownloadProcess(int exitCode, QProcess::ExitStatus exitStatus);

    AppSettings *m_settings = nullptr;
    QProcess *m_checkProcess = nullptr;
    QProcess *m_downloadProcess = nullptr;
    QString m_downloadVersionTag;
    QString m_downloadTargetPath;
    QString m_downloadPartPath;
    QString m_statusMessage;
    bool m_busy = false;
    bool m_manualCheck = false;
    qint64 m_downloadExpectedSize = 0;
};

#pragma once

#include "domain/Enums.h"

#include <QStringList>

class QSettings;

class AppSettings {
public:
    AppSettings();
    ~AppSettings();

    QStringList recentProjects() const;
    void addRecentProject(const QString &projectPath);
    QStringList knownProjects() const;
    void addKnownProject(const QString &projectPath);
    void removeKnownProject(const QString &projectPath);
    void replaceProjectPath(const QString &oldProjectPath, const QString &newProjectPath);

    QString visionBaseUrl() const;
    void setVisionBaseUrl(const QString &value);

    QString visionApiKey() const;
    void setVisionApiKey(const QString &value);

    QString visionModel() const;
    void setVisionModel(const QString &value);

    AnalysisMode analysisMode() const;
    void setAnalysisMode(AnalysisMode mode);

    int frameInterval() const;
    void setFrameInterval(int value);

    int thumbnailFrameIndex() const;
    void setThumbnailFrameIndex(int value);

    int contactSheetFrameCount() const;
    void setContactSheetFrameCount(int value);

    int analysisTimeoutSec() const;
    void setAnalysisTimeoutSec(int value);

    int themeMode() const;
    void setThemeMode(int value);

    QString pendingUpdateVersion() const;
    void setPendingUpdateVersion(const QString &value);

    QString pendingUpdateInstallerPath() const;
    void setPendingUpdateInstallerPath(const QString &value);

    QString downloadedUpdateVersion() const;
    void setDownloadedUpdateVersion(const QString &value);
    void clearPendingUpdate();

    int updateDownloadMode() const;
    void setUpdateDownloadMode(int value);

    QString updateManualProxyUrl() const;
    void setUpdateManualProxyUrl(const QString &value);

    QString materialBackupQueueJson(const QString &projectDatabasePath) const;
    void setMaterialBackupQueueJson(const QString &projectDatabasePath, const QString &json);

    QString feedbackSessionJson() const;
    void setFeedbackSessionJson(const QString &json);

    void sync();

private:
    QSettings *m_settings = nullptr;
};

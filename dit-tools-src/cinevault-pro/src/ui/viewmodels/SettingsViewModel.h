#pragma once

#include <QObject>

class AppSettings;
class UpdateService;
class VideoAnalysisService;
class VisionApiClient;

class SettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString visionBaseUrl READ visionBaseUrl WRITE setVisionBaseUrl NOTIFY settingsChanged)
    Q_PROPERTY(QString visionApiKey READ visionApiKey WRITE setVisionApiKey NOTIFY settingsChanged)
    Q_PROPERTY(QString visionModel READ visionModel WRITE setVisionModel NOTIFY settingsChanged)
    Q_PROPERTY(int analysisMode READ analysisMode WRITE setAnalysisMode NOTIFY settingsChanged)
    Q_PROPERTY(int frameInterval READ frameInterval WRITE setFrameInterval NOTIFY settingsChanged)
    Q_PROPERTY(int thumbnailFrameIndex READ thumbnailFrameIndex WRITE setThumbnailFrameIndex NOTIFY settingsChanged)
    Q_PROPERTY(int contactSheetFrameCount READ contactSheetFrameCount WRITE setContactSheetFrameCount NOTIFY settingsChanged)
    Q_PROPERTY(int analysisTimeoutSec READ analysisTimeoutSec WRITE setAnalysisTimeoutSec NOTIFY settingsChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY settingsChanged)
    Q_PROPERTY(bool updateBusy READ updateBusy NOTIFY settingsChanged)
    Q_PROPERTY(bool autoInstallUpdates READ autoInstallUpdates WRITE setAutoInstallUpdates NOTIFY settingsChanged)
    Q_PROPERTY(QString currentVersionLabel READ currentVersionLabel NOTIFY settingsChanged)
    Q_PROPERTY(int updateDownloadMode READ updateDownloadMode WRITE setUpdateDownloadMode NOTIFY settingsChanged)
    Q_PROPERTY(QString updateManualProxyUrl READ updateManualProxyUrl WRITE setUpdateManualProxyUrl NOTIFY settingsChanged)
    Q_PROPERTY(QString dataRootPath READ dataRootPath NOTIFY settingsChanged)
    Q_PROPERTY(QString frameCacheSizeLabel READ frameCacheSizeLabel NOTIFY settingsChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY settingsChanged)

public:
    explicit SettingsViewModel(AppSettings *settings,
                               VisionApiClient *visionApiClient,
                               VideoAnalysisService *videoAnalysisService,
                               UpdateService *updateService,
                               QObject *parent = nullptr);

    QString visionBaseUrl() const;
    void setVisionBaseUrl(const QString &value);
    QString visionApiKey() const;
    void setVisionApiKey(const QString &value);
    QString visionModel() const;
    void setVisionModel(const QString &value);
    int analysisMode() const;
    void setAnalysisMode(int value);
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
    bool updateBusy() const;
    bool autoInstallUpdates() const;
    void setAutoInstallUpdates(bool enabled);
    QString currentVersionLabel() const;
    int updateDownloadMode() const;
    void setUpdateDownloadMode(int value);
    QString updateManualProxyUrl() const;
    void setUpdateManualProxyUrl(const QString &value);
    QString dataRootPath() const;
    QString frameCacheSizeLabel() const;
    QString lastMessage() const;

    Q_INVOKABLE void beginStartupUpdateFlow();
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void saveUpdateDownloadSettings(int updateDownloadMode, const QString &updateManualProxyUrl);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshCacheInfo();
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void testConnectionWith(const QString &visionBaseUrl,
                                        const QString &visionApiKey,
                                        const QString &visionModel,
                                        int analysisTimeoutSec);
    Q_INVOKABLE void saveAndApply(const QString &visionBaseUrl,
                                  const QString &visionApiKey,
                                  const QString &visionModel,
                                  int analysisMode,
                                  int frameInterval,
                                  int thumbnailFrameIndex,
                                  int contactSheetFrameCount,
                                  int analysisTimeoutSec,
                                  int updateDownloadMode,
                                  const QString &updateManualProxyUrl);

signals:
    void settingsChanged();

private:
    void setLastMessage(const QString &message);
    void promptInstallUpdate(const QString &versionTag);

    AppSettings *m_settings = nullptr;
    VisionApiClient *m_visionApiClient = nullptr;
    VideoAnalysisService *m_videoAnalysisService = nullptr;
    UpdateService *m_updateService = nullptr;
    QString m_frameCacheSizeLabel;
    QString m_lastMessage;
};

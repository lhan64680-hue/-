#include "ui/viewmodels/SettingsViewModel.h"

#include "application/UpdateService.h"
#include "domain/Enums.h"
#include "infrastructure/config/AppSettings.h"
#include "infrastructure/network/VisionApiClient.h"
#include "shared/Formatters.h"
#include "shared/Paths.h"
#include "ui/window/QuickSearchController.h"

#include <QtConcurrent>

#include <QApplication>
#include <QDirIterator>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>

namespace {
qint64 directorySize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QWidget *dialogParent()
{
    return QApplication::activeWindow();
}
}

SettingsViewModel::SettingsViewModel(AppSettings *settings,
                                     VisionApiClient *visionApiClient,
                                     VideoAnalysisService *videoAnalysisService,
                                     UpdateService *updateService,
                                     QuickSearchController *quickSearchController,
                                     QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_visionApiClient(visionApiClient)
    , m_videoAnalysisService(videoAnalysisService)
    , m_updateService(updateService)
    , m_quickSearchController(quickSearchController)
{
    if (m_quickSearchController) {
        connect(m_quickSearchController,
                &QuickSearchController::shortcutStatusChanged,
                this,
                &SettingsViewModel::settingsChanged);
    }
    if (m_updateService) {
        connect(m_updateService, &UpdateService::statusMessageChanged, this, &SettingsViewModel::setLastMessage);
        connect(m_updateService, &UpdateService::busyChanged, this, &SettingsViewModel::settingsChanged);
        connect(m_updateService, &UpdateService::updateReady, this, [this](const QString &versionTag, const QString &, bool) {
            if (m_settings && m_settings->autoInstallUpdates()) {
                QString errorMessage;
                if (!m_updateService->installPendingUpdateNow(&errorMessage)) {
                    setLastMessage(errorMessage);
                    QMessageBox::warning(dialogParent(), QStringLiteral("安装更新失败"), errorMessage);
                }
                return;
            }
            promptInstallUpdate(versionTag);
        });
    }

    refreshCacheInfo();
}

QString SettingsViewModel::visionBaseUrl() const
{
    return m_settings ? m_settings->visionBaseUrl() : QString();
}

void SettingsViewModel::setVisionBaseUrl(const QString &value)
{
    if (!m_settings || m_settings->visionBaseUrl() == value.trimmed()) {
        return;
    }
    m_settings->setVisionBaseUrl(value);
    emit settingsChanged();
}

QString SettingsViewModel::visionApiKey() const
{
    return m_settings ? m_settings->visionApiKey() : QString();
}

void SettingsViewModel::setVisionApiKey(const QString &value)
{
    if (!m_settings || m_settings->visionApiKey() == value.trimmed()) {
        return;
    }
    m_settings->setVisionApiKey(value);
    emit settingsChanged();
}

QString SettingsViewModel::visionModel() const
{
    return m_settings ? m_settings->visionModel() : QString();
}

void SettingsViewModel::setVisionModel(const QString &value)
{
    if (!m_settings || m_settings->visionModel() == value.trimmed()) {
        return;
    }
    m_settings->setVisionModel(value);
    emit settingsChanged();
}

bool SettingsViewModel::searchAssistantEnabled() const
{
    return m_settings && m_settings->searchAssistantEnabled();
}

void SettingsViewModel::setSearchAssistantEnabled(bool enabled)
{
    if (!m_settings || m_settings->searchAssistantEnabled() == enabled) return;
    m_settings->setSearchAssistantEnabled(enabled);
    emit settingsChanged();
}

bool SettingsViewModel::frameRerankEnabled() const
{
    return m_settings && m_settings->frameRerankEnabled();
}

void SettingsViewModel::setFrameRerankEnabled(bool enabled)
{
    if (!m_settings || m_settings->frameRerankEnabled() == enabled) return;
    m_settings->setFrameRerankEnabled(enabled);
    emit settingsChanged();
}

bool SettingsViewModel::localOnlySearch() const
{
    return m_settings && m_settings->localOnlySearch();
}

void SettingsViewModel::setLocalOnlySearch(bool enabled)
{
    if (!m_settings || m_settings->localOnlySearch() == enabled) return;
    m_settings->setLocalOnlySearch(enabled);
    emit settingsChanged();
}

bool SettingsViewModel::allowSearchFrameUpload() const
{
    return m_settings && m_settings->allowSearchFrameUpload();
}

void SettingsViewModel::setAllowSearchFrameUpload(bool enabled)
{
    if (!m_settings || m_settings->allowSearchFrameUpload() == enabled) return;
    m_settings->setAllowSearchFrameUpload(enabled);
    emit settingsChanged();
}

bool SettingsViewModel::quickSearchEnabled() const
{
    return m_settings && m_settings->quickSearchEnabled();
}

QString SettingsViewModel::quickSearchShortcut() const
{
    return m_settings ? m_settings->quickSearchShortcut() : QStringLiteral("Alt+Space");
}

bool SettingsViewModel::startAtLogin() const
{
    return m_settings && m_settings->startAtLogin();
}

QString SettingsViewModel::quickSearchStatusText() const
{
    return m_quickSearchController
        ? m_quickSearchController->shortcutStatusText()
        : QStringLiteral("快捷搜索控制器未初始化");
}

int SettingsViewModel::analysisMode() const
{
    return m_settings ? static_cast<int>(m_settings->analysisMode()) : 0;
}

void SettingsViewModel::setAnalysisMode(int value)
{
    if (!m_settings || analysisMode() == value) {
        return;
    }
    if (value == static_cast<int>(AnalysisMode::EveryFrame)) {
        m_settings->setAnalysisMode(AnalysisMode::EveryFrame);
    } else if (value == static_cast<int>(AnalysisMode::CustomInterval)) {
        m_settings->setAnalysisMode(AnalysisMode::CustomInterval);
    } else {
        m_settings->setAnalysisMode(AnalysisMode::Every10Frames);
    }
    emit settingsChanged();
}

int SettingsViewModel::frameInterval() const
{
    return m_settings ? m_settings->frameInterval() : 10;
}

void SettingsViewModel::setFrameInterval(int value)
{
    if (!m_settings || frameInterval() == qMax(1, value)) {
        return;
    }
    m_settings->setFrameInterval(value);
    emit settingsChanged();
}

int SettingsViewModel::thumbnailFrameIndex() const
{
    return m_settings ? m_settings->thumbnailFrameIndex() : 3;
}

void SettingsViewModel::setThumbnailFrameIndex(int value)
{
    if (!m_settings || thumbnailFrameIndex() == qMax(1, value)) {
        return;
    }
    m_settings->setThumbnailFrameIndex(value);
    emit settingsChanged();
}

int SettingsViewModel::contactSheetFrameCount() const
{
    return m_settings ? m_settings->contactSheetFrameCount() : 24;
}

void SettingsViewModel::setContactSheetFrameCount(int value)
{
    if (!m_settings || contactSheetFrameCount() == qBound(1, value, 64)) {
        return;
    }
    m_settings->setContactSheetFrameCount(value);
    emit settingsChanged();
}

int SettingsViewModel::analysisTimeoutSec() const
{
    return m_settings ? m_settings->analysisTimeoutSec() : 60;
}

void SettingsViewModel::setAnalysisTimeoutSec(int value)
{
    if (!m_settings || analysisTimeoutSec() == qMax(5, value)) {
        return;
    }
    m_settings->setAnalysisTimeoutSec(value);
    emit settingsChanged();
}

int SettingsViewModel::themeMode() const
{
    return m_settings ? m_settings->themeMode() : 0;
}

void SettingsViewModel::setThemeMode(int value)
{
    if (!m_settings || themeMode() == value) {
        return;
    }
    m_settings->setThemeMode(value);
    m_settings->sync();
    emit settingsChanged();
}

bool SettingsViewModel::updateBusy() const
{
    return m_updateService && m_updateService->isBusy();
}

bool SettingsViewModel::autoInstallUpdates() const
{
    return m_settings && m_settings->autoInstallUpdates();
}

void SettingsViewModel::setAutoInstallUpdates(bool enabled)
{
    if (!m_settings || m_settings->autoInstallUpdates() == enabled) {
        return;
    }
    m_settings->setAutoInstallUpdates(enabled);
    m_settings->sync();
    emit settingsChanged();
}

QString SettingsViewModel::currentVersionLabel() const
{
    return QStringLiteral("当前版本：%1").arg(
        m_updateService ? m_updateService->currentVersionTag() : QStringLiteral("v0.0.0"));
}

int SettingsViewModel::updateDownloadMode() const
{
    return m_settings ? m_settings->updateDownloadMode() : 0;
}

void SettingsViewModel::setUpdateDownloadMode(int value)
{
    if (!m_settings || updateDownloadMode() == qBound(0, value, 2)) {
        return;
    }
    m_settings->setUpdateDownloadMode(value);
    m_settings->sync();
    emit settingsChanged();
}

QString SettingsViewModel::updateManualProxyUrl() const
{
    return m_settings ? m_settings->updateManualProxyUrl() : QString();
}

void SettingsViewModel::setUpdateManualProxyUrl(const QString &value)
{
    if (!m_settings || updateManualProxyUrl() == value.trimmed()) {
        return;
    }
    m_settings->setUpdateManualProxyUrl(value);
    m_settings->sync();
    emit settingsChanged();
}

QString SettingsViewModel::dataRootPath() const
{
    return Paths::resolvedDataRoot();
}

QString SettingsViewModel::frameCacheSizeLabel() const
{
    return m_frameCacheSizeLabel;
}

QString SettingsViewModel::lastMessage() const
{
    return m_lastMessage;
}

void SettingsViewModel::beginStartupUpdateFlow()
{
    if (!m_updateService) {
        return;
    }

    m_updateService->beginStartupFlow();
}

void SettingsViewModel::checkForUpdates()
{
    if (!m_updateService) {
        return;
    }

    m_updateService->checkForUpdates(true);
}

void SettingsViewModel::saveUpdateDownloadSettings(int updateDownloadMode, const QString &updateManualProxyUrl)
{
    if (!m_settings) {
        return;
    }

    m_settings->setUpdateDownloadMode(updateDownloadMode);
    m_settings->setUpdateManualProxyUrl(updateManualProxyUrl);
    m_settings->sync();
    emit settingsChanged();
}

void SettingsViewModel::refresh()
{
    refreshCacheInfo();
    emit settingsChanged();
}

void SettingsViewModel::refreshCacheInfo()
{
    m_frameCacheSizeLabel = Formatters::formatBytes(directorySize(Paths::frameCacheRoot()));
    emit settingsChanged();
}

void SettingsViewModel::testConnection()
{
    if (!m_visionApiClient || !m_settings) {
        return;
    }

    testConnectionWith(m_settings->visionBaseUrl(),
                       m_settings->visionApiKey(),
                       m_settings->visionModel(),
                       m_settings->analysisTimeoutSec());
}

void SettingsViewModel::testConnectionWith(const QString &visionBaseUrl,
                                           const QString &visionApiKey,
                                           const QString &visionModel,
                                           int analysisTimeoutSec)
{
    if (!m_visionApiClient) {
        return;
    }

    const auto baseUrl = visionBaseUrl.trimmed();
    const auto apiKey = visionApiKey.trimmed();
    const auto model = visionModel.trimmed();
    const auto timeoutSec = qMax(5, analysisTimeoutSec);
    setLastMessage(QStringLiteral("正在测试视觉接口连通状态..."));

    auto future = QtConcurrent::run([this, baseUrl, apiKey, model, timeoutSec]() {
        QString errorMessage;
        const auto ok = m_visionApiClient->testConnection(baseUrl, apiKey, model, timeoutSec, &errorMessage);
        QMetaObject::invokeMethod(this, [this, ok, errorMessage]() {
            setLastMessage(ok ? QStringLiteral("视觉接口连通测试成功。") : errorMessage);
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

QString SettingsViewModel::shortcutFromKeyEvent(int key, int modifiers) const
{
    return QuickSearchController::shortcutFromKeyEvent(key, modifiers);
}

void SettingsViewModel::saveAndApply(const QString &visionBaseUrl,
                                     const QString &visionApiKey,
                                     const QString &visionModel,
                                     bool searchAssistantEnabled,
                                     bool frameRerankEnabled,
                                     bool localOnlySearch,
                                     bool allowSearchFrameUpload,
                                     bool quickSearchEnabled,
                                     const QString &quickSearchShortcut,
                                     bool startAtLogin,
                                     int analysisMode,
                                     int frameInterval,
                                     int thumbnailFrameIndex,
                                     int contactSheetFrameCount,
                                     int analysisTimeoutSec,
                                     int updateDownloadMode,
                                     const QString &updateManualProxyUrl)
{
    if (!m_settings) {
        return;
    }

    const auto normalizedShortcut = QuickSearchController::normalizedShortcut(quickSearchShortcut);
    QString shortcutError;
    if (m_quickSearchController
        && !m_quickSearchController->applyShortcutConfiguration(quickSearchEnabled,
                                                                 normalizedShortcut,
                                                                 &shortcutError)) {
        setLastMessage(shortcutError);
        emit settingsChanged();
        return;
    }

    QString startAtLoginError;
    const auto startAtLoginChanged = m_settings->startAtLogin() != startAtLogin;
    if (startAtLoginChanged
        && m_quickSearchController
        && !m_quickSearchController->setStartAtLogin(startAtLogin, &startAtLoginError)) {
        setLastMessage(startAtLoginError);
        emit settingsChanged();
        return;
    }

    m_settings->setVisionBaseUrl(visionBaseUrl);
    m_settings->setVisionApiKey(visionApiKey);
    m_settings->setVisionModel(visionModel);
    m_settings->setSearchAssistantEnabled(searchAssistantEnabled);
    m_settings->setFrameRerankEnabled(frameRerankEnabled);
    m_settings->setLocalOnlySearch(localOnlySearch);
    m_settings->setAllowSearchFrameUpload(allowSearchFrameUpload);
    m_settings->setQuickSearchEnabled(quickSearchEnabled);
    m_settings->setQuickSearchShortcut(normalizedShortcut);
    m_settings->setStartAtLogin(startAtLogin);
    AnalysisMode resolvedMode = AnalysisMode::Every10Frames;
    if (analysisMode == static_cast<int>(AnalysisMode::EveryFrame)) {
        resolvedMode = AnalysisMode::EveryFrame;
    } else if (analysisMode == static_cast<int>(AnalysisMode::CustomInterval)) {
        resolvedMode = AnalysisMode::CustomInterval;
    }
    m_settings->setAnalysisMode(resolvedMode);
    m_settings->setFrameInterval(resolvedMode == AnalysisMode::Every10Frames ? 10 : frameInterval);
    m_settings->setThumbnailFrameIndex(thumbnailFrameIndex);
    m_settings->setContactSheetFrameCount(contactSheetFrameCount);
    m_settings->setAnalysisTimeoutSec(analysisTimeoutSec);
    m_settings->setUpdateDownloadMode(updateDownloadMode);
    m_settings->setUpdateManualProxyUrl(updateManualProxyUrl);
    m_settings->sync();

    setLastMessage(QStringLiteral("设置已保存并应用，快捷搜索、素材解析与搜索将使用新参数。"));
    emit settingsChanged();
    emit searchSettingsChanged();
}

void SettingsViewModel::setLastMessage(const QString &message)
{
    if (m_lastMessage == message) {
        return;
    }
    m_lastMessage = message;
    emit settingsChanged();
}

void SettingsViewModel::promptInstallUpdate(const QString &versionTag)
{
    if (!m_updateService) {
        return;
    }

    QMessageBox box(dialogParent());
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("发现新版本"));
    box.setText(QStringLiteral("更新包 %1 已下载完成。").arg(versionTag));
    box.setInformativeText(QStringLiteral("立即更新会打开独立进度窗口、关闭当前程序、静默安装并自动启动新版本。也可以安排在下次启动时自动更新。"));
    QAbstractButton *installNowButton = box.addButton(QStringLiteral("立即更新"), QMessageBox::AcceptRole);
    QAbstractButton *updateLaterButton = box.addButton(QStringLiteral("下次启动更新"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == installNowButton) {
        if (m_settings) {
            m_settings->setScheduledUpdateVersion(QString());
            m_settings->sync();
        }
        QString errorMessage;
        if (!m_updateService->installPendingUpdateNow(&errorMessage)) {
            setLastMessage(errorMessage);
            QMessageBox::warning(dialogParent(), QStringLiteral("安装更新失败"), errorMessage);
        }
        return;
    }

    if (box.clickedButton() == updateLaterButton || box.clickedButton() == nullptr) {
        if (m_settings) {
            m_settings->setScheduledUpdateVersion(versionTag);
            m_settings->sync();
        }
        setLastMessage(QStringLiteral("已安排下次启动更新：%1").arg(versionTag));
    }
}

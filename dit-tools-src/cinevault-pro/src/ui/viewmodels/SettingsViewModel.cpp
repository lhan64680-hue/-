#include "ui/viewmodels/SettingsViewModel.h"

#include "application/UpdateService.h"
#include "domain/Enums.h"
#include "infrastructure/config/AppSettings.h"
#include "infrastructure/network/VisionApiClient.h"
#include "shared/Formatters.h"
#include "shared/Paths.h"

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
                                     QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_visionApiClient(visionApiClient)
    , m_videoAnalysisService(videoAnalysisService)
    , m_updateService(updateService)
{
    if (m_updateService) {
        connect(m_updateService, &UpdateService::statusMessageChanged, this, &SettingsViewModel::setLastMessage);
        connect(m_updateService, &UpdateService::busyChanged, this, &SettingsViewModel::settingsChanged);
        connect(m_updateService, &UpdateService::updateReady, this, [this](const QString &versionTag, const QString &, bool) {
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

void SettingsViewModel::saveAndApply(const QString &visionBaseUrl,
                                     const QString &visionApiKey,
                                     const QString &visionModel,
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

    m_settings->setVisionBaseUrl(visionBaseUrl);
    m_settings->setVisionApiKey(visionApiKey);
    m_settings->setVisionModel(visionModel);
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

    setLastMessage(QStringLiteral("设置已保存并应用，下一次解析将使用新参数。"));
    emit settingsChanged();
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
    box.setInformativeText(QStringLiteral("点击立即更新后会关闭当前程序，并打开已下载的当前平台更新包。也可以保留到下次启动时更新。"));
    QAbstractButton *installNowButton = box.addButton(QStringLiteral("立即更新"), QMessageBox::AcceptRole);
    QAbstractButton *updateLaterButton = box.addButton(QStringLiteral("下次启动更新"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == installNowButton) {
        QString errorMessage;
        if (!m_updateService->installPendingUpdateNow(&errorMessage)) {
            setLastMessage(errorMessage);
            QMessageBox::warning(dialogParent(), QStringLiteral("安装更新失败"), errorMessage);
        }
        return;
    }

    if (box.clickedButton() == updateLaterButton) {
        setLastMessage(QStringLiteral("更新包已保留，下次启动时将再次提示安装：%1").arg(versionTag));
    }
}

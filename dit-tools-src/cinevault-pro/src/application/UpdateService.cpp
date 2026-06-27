#include "application/UpdateService.h"

#include "infrastructure/config/AppSettings.h"
#include "shared/Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>
#include <QVersionNumber>

namespace {
constexpr auto kLatestReleaseUrl = "https://api.github.com/repos/luojiang419/dit-tools/releases/latest";

QString toPowerShellLiteral(const QString &value)
{
    auto escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'") + escaped + QStringLiteral("'");
}

QString installerScriptPath()
{
    return QDir(Paths::updatesRoot()).filePath(QStringLiteral("apply-update.ps1"));
}
}

UpdateService::UpdateService(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

UpdateService::~UpdateService()
{
    if (m_checkReply) {
        m_checkReply->abort();
    }
    if (m_downloadReply) {
        m_downloadReply->abort();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
    }
}

QString UpdateService::normalizeVersionTag(const QString &versionTag)
{
    auto normalized = versionTag.trimmed();
    if (normalized.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        normalized.remove(0, 1);
    }

    const auto parsed = QVersionNumber::fromString(normalized);
    if (parsed.isNull()) {
        return {};
    }

    return QStringLiteral("v") + parsed.toString();
}

int UpdateService::compareVersionTags(const QString &left, const QString &right)
{
    const auto normalizedLeft = normalizeVersionTag(left);
    const auto normalizedRight = normalizeVersionTag(right);

    if (normalizedLeft.isEmpty() && normalizedRight.isEmpty()) {
        return 0;
    }
    if (normalizedLeft.isEmpty()) {
        return -1;
    }
    if (normalizedRight.isEmpty()) {
        return 1;
    }

    return QVersionNumber::compare(QVersionNumber::fromString(normalizedLeft.mid(1)),
                                   QVersionNumber::fromString(normalizedRight.mid(1)));
}

QString UpdateService::expectedInstallerName(const QString &versionTag)
{
    const auto normalized = normalizeVersionTag(versionTag);
    return normalized.isEmpty() ? QString() : QStringLiteral("CineVault-Setup-%1.exe").arg(normalized);
}

bool UpdateService::parseLatestRelease(const QByteArray &payload, UpdateReleaseInfo *info, QString *errorMessage)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GitHub 发布信息解析失败。");
        }
        return false;
    }

    const auto root = document.object();
    const auto versionTag = normalizeVersionTag(root.value(QStringLiteral("tag_name")).toString());
    if (versionTag.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GitHub 发布信息缺少有效版本号。");
        }
        return false;
    }

    const auto expectedName = expectedInstallerName(versionTag);
    const auto assets = root.value(QStringLiteral("assets")).toArray();
    for (const auto &assetValue : assets) {
        const auto assetObject = assetValue.toObject();
        if (assetObject.value(QStringLiteral("name")).toString() != expectedName) {
            continue;
        }

        const auto downloadUrl = assetObject.value(QStringLiteral("browser_download_url")).toString().trimmed();
        if (downloadUrl.isEmpty()) {
            continue;
        }

        if (info) {
            info->versionTag = versionTag;
            info->installerName = expectedName;
            info->installerUrl = downloadUrl;
            info->installerSize = assetObject.value(QStringLiteral("size")).toVariant().toLongLong();
        }
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("最新发布版本缺少安装包：%1").arg(expectedName);
    }
    return false;
}

QString UpdateService::latestReleaseStatusMessage(int statusCode, const QString &networkErrorString)
{
    if (statusCode == 404) {
        return QStringLiteral("当前仓库还没有可用的发布版本。");
    }

    return QStringLiteral("检查更新失败：%1").arg(networkErrorString);
}

QString UpdateService::currentVersionTag() const
{
    const auto normalized = normalizeVersionTag(QCoreApplication::applicationVersion());
    return normalized.isEmpty() ? QStringLiteral("v0.0.0") : normalized;
}

bool UpdateService::isBusy() const
{
    return m_busy;
}

bool UpdateService::hasPendingUpdate() const
{
    QString versionTag;
    QString installerPath;
    if (!readPendingUpdate(&versionTag, &installerPath)) {
        return false;
    }

    return compareVersionTags(versionTag, currentVersionTag()) > 0;
}

void UpdateService::beginStartupFlow()
{
    clearPendingUpdateIfCurrentOrMissing();

    QString versionTag;
    QString installerPath;
    if (readPendingUpdate(&versionTag, &installerPath)
        && compareVersionTags(versionTag, currentVersionTag()) > 0) {
        setStatusMessage(QStringLiteral("已检测到待安装更新：%1").arg(versionTag));
        emit updateReady(versionTag, installerPath, false);
        return;
    }

    checkForUpdates(false);
}

void UpdateService::checkForUpdates(bool manual)
{
    if (m_busy) {
        setStatusMessage(QStringLiteral("正在检查或下载更新，请稍候。"));
        return;
    }

    clearPendingUpdateIfCurrentOrMissing();

    QString versionTag;
    QString installerPath;
    if (readPendingUpdate(&versionTag, &installerPath)
        && compareVersionTags(versionTag, currentVersionTag()) > 0) {
        setStatusMessage(QStringLiteral("已找到待安装更新：%1").arg(versionTag));
        emit updateReady(versionTag, installerPath, manual);
        return;
    }

    m_manualCheck = manual;
    setBusy(true);
    setStatusMessage(manual
        ? QStringLiteral("正在检查最新发布版本...")
        : QStringLiteral("启动后正在检查最新发布版本..."));

    QNetworkRequest request(QUrl(QString::fromLatin1(kLatestReleaseUrl)));
    request.setRawHeader("User-Agent", "CineVault");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_checkReply = m_networkManager->get(request);
    connect(m_checkReply, &QNetworkReply::finished, this, &UpdateService::finishCheckReply);
}

bool UpdateService::installPendingUpdateNow(QString *errorMessage)
{
    clearPendingUpdateIfCurrentOrMissing();

    QString versionTag;
    QString installerPath;
    if (!readPendingUpdate(&versionTag, &installerPath)
        || compareVersionTags(versionTag, currentVersionTag()) <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前没有可安装的更新包。");
        }
        return false;
    }

    if (!QDir().mkpath(Paths::updatesRoot())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建更新缓存目录：%1").arg(Paths::updatesRoot());
        }
        return false;
    }

    QFile scriptFile(installerScriptPath());
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建更新脚本：%1").arg(scriptFile.fileName());
        }
        return false;
    }

    const auto appDir = QCoreApplication::applicationDirPath();
    const auto appExe = QCoreApplication::applicationFilePath();
    const auto appPid = QCoreApplication::applicationPid();

    QStringList scriptLines;
    scriptLines << QStringLiteral("$ErrorActionPreference = 'SilentlyContinue'")
                << QStringLiteral("$pidToWait = %1").arg(appPid)
                << QStringLiteral("$installerPath = %1").arg(toPowerShellLiteral(installerPath))
                << QStringLiteral("$appDir = %1").arg(toPowerShellLiteral(appDir))
                << QStringLiteral("$appExe = %1").arg(toPowerShellLiteral(appExe))
                << QStringLiteral("while (Get-Process -Id $pidToWait -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 500 }")
                << QStringLiteral("$installerArgs = @('/VERYSILENT', '/NORESTART', '/SUPPRESSMSGBOXES', ('/DIR=' + $appDir))")
                << QStringLiteral("$process = Start-Process -FilePath $installerPath -ArgumentList $installerArgs -Wait -PassThru")
                << QStringLiteral("if (Test-Path $appExe) { Start-Process -FilePath $appExe | Out-Null }")
                << QStringLiteral("Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue");
    scriptFile.write(scriptLines.join(QStringLiteral("\r\n")).toUtf8());
    scriptFile.close();

    const QStringList arguments{
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"),
        QStringLiteral("Hidden"),
        QStringLiteral("-File"),
        QDir::toNativeSeparators(scriptFile.fileName())
    };

    if (!QProcess::startDetached(QStringLiteral("powershell.exe"), arguments, Paths::updatesRoot())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法启动更新安装脚本。");
        }
        return false;
    }

    setStatusMessage(QStringLiteral("正在退出程序并安装更新：%1").arg(versionTag));
    QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
    return true;
}

void UpdateService::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }

    m_busy = busy;
    emit busyChanged();
}

void UpdateService::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }

    m_statusMessage = message;
    emit statusMessageChanged(message);
}

void UpdateService::clearPendingUpdateIfCurrentOrMissing()
{
    if (!m_settings) {
        return;
    }

    const auto versionTag = normalizeVersionTag(m_settings->pendingUpdateVersion());
    const auto installerPath = m_settings->pendingUpdateInstallerPath().trimmed();
    if (versionTag.isEmpty()) {
        return;
    }

    if (!QFileInfo::exists(installerPath)
        || compareVersionTags(versionTag, currentVersionTag()) <= 0) {
        m_settings->clearPendingUpdate();
    }
}

bool UpdateService::readPendingUpdate(QString *versionTag, QString *installerPath) const
{
    if (!m_settings) {
        return false;
    }

    const auto normalizedVersionTag = normalizeVersionTag(m_settings->pendingUpdateVersion());
    const auto normalizedInstallerPath = m_settings->pendingUpdateInstallerPath().trimmed();
    if (normalizedVersionTag.isEmpty() || normalizedInstallerPath.isEmpty()) {
        return false;
    }

    if (!QFileInfo::exists(normalizedInstallerPath)) {
        return false;
    }

    if (versionTag) {
        *versionTag = normalizedVersionTag;
    }
    if (installerPath) {
        *installerPath = normalizedInstallerPath;
    }
    return true;
}

bool UpdateService::useExistingInstaller(const UpdateReleaseInfo &release, bool manual)
{
    QString versionTag;
    QString installerPath;
    if (readPendingUpdate(&versionTag, &installerPath)
        && compareVersionTags(versionTag, release.versionTag) == 0) {
        setStatusMessage(QStringLiteral("已找到已下载更新包：%1").arg(versionTag));
        emit updateReady(versionTag, installerPath, manual);
        return true;
    }

    const auto existingInstallerPath = QDir(Paths::updatesRoot()).filePath(release.installerName);
    if (!QFileInfo::exists(existingInstallerPath)) {
        return false;
    }

    if (m_settings) {
        m_settings->setDownloadedUpdateVersion(release.versionTag);
        m_settings->setPendingUpdateVersion(release.versionTag);
        m_settings->setPendingUpdateInstallerPath(existingInstallerPath);
        m_settings->sync();
    }

    setStatusMessage(QStringLiteral("已复用已下载更新包：%1").arg(release.versionTag));
    emit updateReady(release.versionTag, existingInstallerPath, manual);
    return true;
}

void UpdateService::startInstallerDownload(const UpdateReleaseInfo &release, bool manual)
{
    if (!QDir().mkpath(Paths::updatesRoot())) {
        setBusy(false);
        setStatusMessage(QStringLiteral("无法创建更新缓存目录：%1").arg(Paths::updatesRoot()));
        return;
    }

    m_manualCheck = manual;
    m_lastDownloadPercent = -1;
    m_downloadVersionTag = release.versionTag;
    m_downloadTargetPath = QDir(Paths::updatesRoot()).filePath(release.installerName);
    const auto partPath = m_downloadTargetPath + QStringLiteral(".part");
    QFile::remove(partPath);

    m_downloadFile = new QFile(partPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
        setBusy(false);
        setStatusMessage(QStringLiteral("无法创建更新包文件：%1").arg(partPath));
        return;
    }

    QNetworkRequest request(QUrl(release.installerUrl));
    request.setRawHeader("User-Agent", "CineVault");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    setStatusMessage(QStringLiteral("发现新版本 %1，正在下载更新包...").arg(release.versionTag));
    m_downloadReply = m_networkManager->get(request);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile && m_downloadReply) {
            m_downloadFile->write(m_downloadReply->readAll());
        }
    });
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total <= 0) {
            return;
        }

        const auto progress = static_cast<int>((received * 100) / total);
        if (progress == m_lastDownloadPercent) {
            return;
        }

        m_lastDownloadPercent = progress;
        setStatusMessage(QStringLiteral("正在下载更新包 %1：%2%").arg(m_downloadVersionTag).arg(progress));
    });
    connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateService::finishDownloadReply);
}

void UpdateService::finishCheckReply()
{
    auto *reply = m_checkReply;
    m_checkReply = nullptr;
    if (!reply) {
        setBusy(false);
        return;
    }

    const auto payload = reply->readAll();
    const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto replyError = reply->error();
    const auto errorString = reply->errorString();
    reply->deleteLater();

    if (replyError != QNetworkReply::NoError) {
        setBusy(false);
        setStatusMessage(latestReleaseStatusMessage(statusCode, errorString));
        return;
    }

    UpdateReleaseInfo release;
    QString errorMessage;
    if (!parseLatestRelease(payload, &release, &errorMessage)) {
        setBusy(false);
        setStatusMessage(errorMessage);
        return;
    }

    if (compareVersionTags(release.versionTag, currentVersionTag()) <= 0) {
        setBusy(false);
        setStatusMessage(QStringLiteral("当前已是最新版本：%1").arg(currentVersionTag()));
        return;
    }

    if (useExistingInstaller(release, m_manualCheck)) {
        setBusy(false);
        return;
    }

    startInstallerDownload(release, m_manualCheck);
}

void UpdateService::finishDownloadReply()
{
    auto *reply = m_downloadReply;
    auto *downloadFile = m_downloadFile;
    const auto versionTag = m_downloadVersionTag;
    const auto targetPath = m_downloadTargetPath;

    m_downloadReply = nullptr;
    m_downloadFile = nullptr;
    m_downloadVersionTag.clear();
    m_downloadTargetPath.clear();
    m_lastDownloadPercent = -1;

    if (!reply || !downloadFile) {
        setBusy(false);
        return;
    }

    downloadFile->write(reply->readAll());
    downloadFile->flush();
    downloadFile->close();

    const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto replyError = reply->error();
    const auto errorString = reply->errorString();
    reply->deleteLater();

    if (replyError != QNetworkReply::NoError || statusCode >= 400) {
        downloadFile->remove();
        downloadFile->deleteLater();
        setBusy(false);
        setStatusMessage(QStringLiteral("下载更新包失败：%1").arg(errorString));
        return;
    }

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        downloadFile->remove();
        downloadFile->deleteLater();
        setBusy(false);
        setStatusMessage(QStringLiteral("无法覆盖旧更新包：%1").arg(targetPath));
        return;
    }

    if (!downloadFile->rename(targetPath)) {
        downloadFile->remove();
        downloadFile->deleteLater();
        setBusy(false);
        setStatusMessage(QStringLiteral("无法保存更新包：%1").arg(targetPath));
        return;
    }

    downloadFile->deleteLater();

    if (m_settings) {
        m_settings->setDownloadedUpdateVersion(versionTag);
        m_settings->setPendingUpdateVersion(versionTag);
        m_settings->setPendingUpdateInstallerPath(targetPath);
        m_settings->sync();
    }

    setBusy(false);
    setStatusMessage(QStringLiteral("更新包已下载完成：%1").arg(versionTag));
    emit updateReady(versionTag, targetPath, m_manualCheck);
}

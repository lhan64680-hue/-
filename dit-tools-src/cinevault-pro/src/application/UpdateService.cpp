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
#include <QProcess>
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
{
}

UpdateService::~UpdateService()
{
    if (m_checkProcess) {
        m_checkProcess->kill();
        m_checkProcess->waitForFinished(2000);
    }
    if (m_downloadProcess) {
        m_downloadProcess->kill();
        m_downloadProcess->waitForFinished(2000);
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

    return QStringLiteral("检查更新失败：%1").arg(
        networkErrorString.trimmed().isEmpty() ? QStringLiteral("网络请求没有返回结果。") : networkErrorString);
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

    if (m_checkProcess) {
        m_checkProcess->kill();
        m_checkProcess->waitForFinished(2000);
        m_checkProcess->deleteLater();
        m_checkProcess = nullptr;
    }

    m_checkProcess = new QProcess(this);
    m_checkProcess->setProgram(QStringLiteral("curl.exe"));
    m_checkProcess->setArguments({
        QStringLiteral("--noproxy"),
        QStringLiteral("*"),
        QStringLiteral("-L"),
        QStringLiteral("--silent"),
        QStringLiteral("--show-error"),
        QStringLiteral("--connect-timeout"),
        QStringLiteral("20"),
        QStringLiteral("--max-time"),
        QStringLiteral("60"),
        QStringLiteral("-H"),
        QStringLiteral("User-Agent: CineVault"),
        QStringLiteral("-H"),
        QStringLiteral("Accept: application/vnd.github+json"),
        QStringLiteral("--output"),
        QStringLiteral("-"),
        QStringLiteral("--write-out"),
        QStringLiteral("\n%{http_code}"),
        QString::fromLatin1(kLatestReleaseUrl)
    });
    connect(m_checkProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &UpdateService::finishCheckProcess);
    m_checkProcess->start();
    if (!m_checkProcess->waitForStarted(3000)) {
        m_checkProcess->deleteLater();
        m_checkProcess = nullptr;
        setBusy(false);
        setStatusMessage(QStringLiteral("无法启动版本检查进程。"));
    }
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
    const auto nativeInstallerPath = QDir::toNativeSeparators(installerPath);
    const auto nativeAppDir = QDir::toNativeSeparators(appDir);
    const auto nativeAppExe = QDir::toNativeSeparators(appExe);
    const auto appPid = QCoreApplication::applicationPid();

    QStringList scriptLines;
    scriptLines << QStringLiteral("$ErrorActionPreference = 'SilentlyContinue'")
                << QStringLiteral("$pidToWait = %1").arg(appPid)
                << QStringLiteral("$installerPath = %1").arg(toPowerShellLiteral(nativeInstallerPath))
                << QStringLiteral("$appDir = %1").arg(toPowerShellLiteral(nativeAppDir))
                << QStringLiteral("$appExe = %1").arg(toPowerShellLiteral(nativeAppExe))
                << QStringLiteral("while (Get-Process -Id $pidToWait -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 500 }")
                << QStringLiteral("$installerDirArg = '/DIR=\"' + $appDir + '\"'")
                << QStringLiteral("$installerArgs = @('/VERYSILENT', '/NORESTART', '/SUPPRESSMSGBOXES', $installerDirArg)")
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

    if (m_downloadProcess) {
        m_downloadProcess->kill();
        m_downloadProcess->waitForFinished(2000);
        m_downloadProcess->deleteLater();
        m_downloadProcess = nullptr;
    }

    m_manualCheck = manual;
    m_downloadVersionTag = release.versionTag;
    m_downloadTargetPath = QDir(Paths::updatesRoot()).filePath(release.installerName);
    m_downloadPartPath = m_downloadTargetPath + QStringLiteral(".part");
    m_downloadExpectedSize = release.installerSize;
    QFile::remove(m_downloadPartPath);

    setStatusMessage(QStringLiteral("发现新版本 %1，正在下载更新包...").arg(release.versionTag));
    m_downloadProcess = new QProcess(this);
    m_downloadProcess->setProgram(QStringLiteral("curl.exe"));
    m_downloadProcess->setArguments({
        QStringLiteral("--noproxy"),
        QStringLiteral("*"),
        QStringLiteral("-L"),
        QStringLiteral("--silent"),
        QStringLiteral("--show-error"),
        QStringLiteral("--connect-timeout"),
        QStringLiteral("20"),
        QStringLiteral("--max-time"),
        QStringLiteral("600"),
        QStringLiteral("-H"),
        QStringLiteral("User-Agent: CineVault"),
        QStringLiteral("--output"),
        QDir::toNativeSeparators(m_downloadPartPath),
        release.installerUrl
    });
    m_downloadProcess->setWorkingDirectory(Paths::updatesRoot());
    connect(m_downloadProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &UpdateService::finishDownloadProcess);
    m_downloadProcess->start();
    if (!m_downloadProcess->waitForStarted(3000)) {
        m_downloadProcess->deleteLater();
        m_downloadProcess = nullptr;
        setBusy(false);
        setStatusMessage(QStringLiteral("无法启动更新包下载进程。"));
    }
}

void UpdateService::finishCheckProcess(int exitCode, QProcess::ExitStatus exitStatus)
{
    auto *checkProcess = m_checkProcess;
    m_checkProcess = nullptr;

    if (!checkProcess) {
        setBusy(false);
        return;
    }

    const auto standardOutput = QString::fromLocal8Bit(checkProcess->readAllStandardOutput()).trimmed();
    const auto standardError = QString::fromLocal8Bit(checkProcess->readAllStandardError()).trimmed();
    checkProcess->deleteLater();

    if (exitStatus != QProcess::NormalExit) {
        setBusy(false);
        setStatusMessage(latestReleaseStatusMessage(0, standardError.isEmpty() ? standardOutput : standardError));
        return;
    }

    if (exitCode != 0) {
        setBusy(false);
        setStatusMessage(latestReleaseStatusMessage(0, standardError.isEmpty() ? standardOutput : standardError));
        return;
    }

    auto normalizedOutput = standardOutput;
    normalizedOutput.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const auto separatorIndex = normalizedOutput.lastIndexOf(QLatin1Char('\n'));
    if (separatorIndex <= 0) {
        setBusy(false);
        setStatusMessage(QStringLiteral("检查更新失败：版本检查结果无法解析。"));
        return;
    }

    const auto payload = normalizedOutput.left(separatorIndex).toUtf8();
    const auto statusCode = normalizedOutput.mid(separatorIndex + 1).trimmed().toInt();

    if (statusCode != 200) {
        setBusy(false);
        setStatusMessage(latestReleaseStatusMessage(statusCode, standardError));
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

void UpdateService::finishDownloadProcess(int exitCode, QProcess::ExitStatus exitStatus)
{
    auto *downloadProcess = m_downloadProcess;
    const auto versionTag = m_downloadVersionTag;
    const auto targetPath = m_downloadTargetPath;
    const auto partPath = m_downloadPartPath;
    const auto expectedSize = m_downloadExpectedSize;

    m_downloadProcess = nullptr;
    m_downloadVersionTag.clear();
    m_downloadTargetPath.clear();
    m_downloadPartPath.clear();
    m_downloadExpectedSize = 0;

    if (!downloadProcess) {
        setBusy(false);
        return;
    }

    const auto standardError = QString::fromLocal8Bit(downloadProcess->readAllStandardError()).trimmed();
    const auto standardOutput = QString::fromLocal8Bit(downloadProcess->readAllStandardOutput()).trimmed();
    downloadProcess->deleteLater();

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QFile::remove(partPath);
        setBusy(false);
        setStatusMessage(QStringLiteral("下载更新包失败：%1").arg(
            standardError.isEmpty() ? (standardOutput.isEmpty() ? QStringLiteral("未知错误") : standardOutput)
                                    : standardError));
        return;
    }

    QFileInfo partInfo(partPath);
    if (!partInfo.exists() || partInfo.size() <= 0) {
        QFile::remove(partPath);
        setBusy(false);
        setStatusMessage(QStringLiteral("下载更新包失败：未生成完整安装包。"));
        return;
    }

    if (expectedSize > 0 && partInfo.size() != expectedSize) {
        QFile::remove(partPath);
        setBusy(false);
        setStatusMessage(QStringLiteral("下载更新包失败：安装包大小与发布资产不一致。"));
        return;
    }

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        QFile::remove(partPath);
        setBusy(false);
        setStatusMessage(QStringLiteral("无法覆盖旧更新包：%1").arg(targetPath));
        return;
    }

    if (!QFile::rename(partPath, targetPath)) {
        QFile::remove(partPath);
        setBusy(false);
        setStatusMessage(QStringLiteral("无法保存更新包：%1").arg(targetPath));
        return;
    }

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

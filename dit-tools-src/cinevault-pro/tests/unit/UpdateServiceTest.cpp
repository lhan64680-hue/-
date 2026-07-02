#include "application/UpdateService.h"

#include <QNetworkProxy>
#include <QProcessEnvironment>
#include <QtTest>

class UpdateServiceTest : public QObject {
    Q_OBJECT

private slots:
    void compareVersionTags_ordersSemanticVersions();
    void compareVersionTags_handlesInvalidValues();
    void expectedInstallerName_returnsPlatformSpecificPrimaryAsset();
    void parseLatestRelease_returnsInstallerAsset();
    void parseLatestRelease_windowsIgnoresMacAsset();
    void parseLatestRelease_macosReturnsDmgAsset();
    void parseLatestRelease_macosFallsBackToPkgAsset();
    void parseLatestRelease_rejectsMissingInstaller();
    void parseLatestRelease_rejectsInvalidPayload();
    void latestReleaseStatusMessage_handlesNoRelease();
    void normalizedProxyUrl_addsHttpSchemeForHostPort();
    void normalizedProxyUrl_rejectsInvalidValues();
    void proxyUrlForNetworkProxy_handlesHttpProxy();
    void proxyUrlForNetworkProxy_handlesSocksProxy();
    void preferredProxyUrl_skipsUnsupportedEntries();
    void proxyUrlsForEnvironment_readsCommonVariables();
    void localProxyCandidates_containsCommonPorts();
};

void UpdateServiceTest::compareVersionTags_ordersSemanticVersions()
{
    QCOMPARE(UpdateService::compareVersionTags(QStringLiteral("v0.1.75"), QStringLiteral("v0.1.74")), 1);
    QCOMPARE(UpdateService::compareVersionTags(QStringLiteral("0.1.74"), QStringLiteral("v0.1.74")), 0);
    QCOMPARE(UpdateService::compareVersionTags(QStringLiteral("v0.1.74"), QStringLiteral("v0.1.75")), -1);
}

void UpdateServiceTest::compareVersionTags_handlesInvalidValues()
{
    QCOMPARE(UpdateService::compareVersionTags(QStringLiteral("invalid"), QStringLiteral("v0.1.75")), -1);
    QCOMPARE(UpdateService::compareVersionTags(QStringLiteral("v0.1.75"), QStringLiteral("invalid")), 1);
    QCOMPARE(UpdateService::compareVersionTags(QStringLiteral("invalid"), QStringLiteral("broken")), 0);
}

void UpdateServiceTest::expectedInstallerName_returnsPlatformSpecificPrimaryAsset()
{
    QCOMPARE(UpdateService::expectedInstallerName(QStringLiteral("v0.1.75"), QStringLiteral("windows")),
             QStringLiteral("CineVault-Setup-v0.1.75.exe"));
    QCOMPARE(UpdateService::expectedInstallerName(QStringLiteral("v0.1.75"), QStringLiteral("macos")),
             QStringLiteral("CineVault-macOS-v0.1.75.dmg"));
}

void UpdateServiceTest::parseLatestRelease_returnsInstallerAsset()
{
    const QByteArray payload = R"({
        "tag_name": "v0.1.75",
        "assets": [
            {
                "name": "README.txt",
                "browser_download_url": "https://example.com/README.txt",
                "size": 1
            },
            {
                "name": "CineVault-Setup-v0.1.75.exe",
                "browser_download_url": "https://example.com/CineVault-Setup-v0.1.75.exe",
                "size": 123456
            }
        ]
    })";

    UpdateReleaseInfo info;
    QString errorMessage;
    QVERIFY(UpdateService::parseLatestRelease(payload, &info, &errorMessage));
    QCOMPARE(info.versionTag, QStringLiteral("v0.1.75"));
    QCOMPARE(info.installerName, QStringLiteral("CineVault-Setup-v0.1.75.exe"));
    QCOMPARE(info.installerUrl, QStringLiteral("https://example.com/CineVault-Setup-v0.1.75.exe"));
    QCOMPARE(info.installerSize, 123456);
}

void UpdateServiceTest::parseLatestRelease_windowsIgnoresMacAsset()
{
    const QByteArray payload = R"({
        "tag_name": "v0.1.75",
        "assets": [
            {
                "name": "CineVault-macOS-v0.1.75.dmg",
                "browser_download_url": "https://example.com/CineVault-macOS-v0.1.75.dmg",
                "size": 456
            },
            {
                "name": "CineVault-Setup-v0.1.75.exe",
                "browser_download_url": "https://example.com/CineVault-Setup-v0.1.75.exe",
                "size": 123
            }
        ]
    })";

    UpdateReleaseInfo info;
    QString errorMessage;
    QVERIFY(UpdateService::parseLatestRelease(payload, &info, &errorMessage, QStringLiteral("windows")));
    QCOMPARE(info.installerName, QStringLiteral("CineVault-Setup-v0.1.75.exe"));
    QCOMPARE(info.installerUrl, QStringLiteral("https://example.com/CineVault-Setup-v0.1.75.exe"));
}

void UpdateServiceTest::parseLatestRelease_macosReturnsDmgAsset()
{
    const QByteArray payload = R"({
        "tag_name": "v0.1.75",
        "assets": [
            {
                "name": "CineVault-Setup-v0.1.75.exe",
                "browser_download_url": "https://example.com/CineVault-Setup-v0.1.75.exe",
                "size": 123
            },
            {
                "name": "CineVault-macOS-v0.1.75.dmg",
                "browser_download_url": "https://example.com/CineVault-macOS-v0.1.75.dmg",
                "size": 456
            }
        ]
    })";

    UpdateReleaseInfo info;
    QString errorMessage;
    QVERIFY(UpdateService::parseLatestRelease(payload, &info, &errorMessage, QStringLiteral("macos")));
    QCOMPARE(info.installerName, QStringLiteral("CineVault-macOS-v0.1.75.dmg"));
    QCOMPARE(info.installerUrl, QStringLiteral("https://example.com/CineVault-macOS-v0.1.75.dmg"));
    QCOMPARE(info.installerSize, 456);
}

void UpdateServiceTest::parseLatestRelease_macosFallsBackToPkgAsset()
{
    const QByteArray payload = R"({
        "tag_name": "v0.1.75",
        "assets": [
            {
                "name": "CineVault-macOS-v0.1.75.pkg",
                "browser_download_url": "https://example.com/CineVault-macOS-v0.1.75.pkg",
                "size": 789
            }
        ]
    })";

    UpdateReleaseInfo info;
    QString errorMessage;
    QVERIFY(UpdateService::parseLatestRelease(payload, &info, &errorMessage, QStringLiteral("macos")));
    QCOMPARE(info.installerName, QStringLiteral("CineVault-macOS-v0.1.75.pkg"));
    QCOMPARE(info.installerUrl, QStringLiteral("https://example.com/CineVault-macOS-v0.1.75.pkg"));
    QCOMPARE(info.installerSize, 789);
}

void UpdateServiceTest::parseLatestRelease_rejectsMissingInstaller()
{
    const QByteArray payload = R"({
        "tag_name": "v0.1.75",
        "assets": [
            {
                "name": "CineVault-Portable-v0.1.75.zip",
                "browser_download_url": "https://example.com/CineVault-Portable-v0.1.75.zip",
                "size": 1
            }
        ]
    })";

    UpdateReleaseInfo info;
    QString errorMessage;
    QVERIFY(!UpdateService::parseLatestRelease(payload, &info, &errorMessage, QStringLiteral("windows")));
    QVERIFY(errorMessage.contains(QStringLiteral("CineVault-Setup-v0.1.75.exe")));
}

void UpdateServiceTest::parseLatestRelease_rejectsInvalidPayload()
{
    UpdateReleaseInfo info;
    QString errorMessage;
    QVERIFY(!UpdateService::parseLatestRelease("not-json", &info, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void UpdateServiceTest::latestReleaseStatusMessage_handlesNoRelease()
{
    QCOMPARE(UpdateService::latestReleaseStatusMessage(404, QStringLiteral("Not Found")),
             QStringLiteral("当前仓库还没有可用的发布版本。"));
    QCOMPARE(UpdateService::latestReleaseStatusMessage(500, QStringLiteral("Server Error")),
             QStringLiteral("检查更新失败：Server Error"));
}

void UpdateServiceTest::normalizedProxyUrl_addsHttpSchemeForHostPort()
{
    QCOMPARE(UpdateService::normalizedProxyUrl(QStringLiteral("127.0.0.1:7890")),
             QStringLiteral("http://127.0.0.1:7890"));
    QCOMPARE(UpdateService::normalizedProxyUrl(QStringLiteral("socks5://localhost:1080")),
             QStringLiteral("socks5://localhost:1080"));
}

void UpdateServiceTest::normalizedProxyUrl_rejectsInvalidValues()
{
    QVERIFY(UpdateService::normalizedProxyUrl(QString()).isEmpty());
    QVERIFY(UpdateService::normalizedProxyUrl(QStringLiteral("127.0.0.1")).isEmpty());
    QVERIFY(UpdateService::normalizedProxyUrl(QStringLiteral("ftp://127.0.0.1:21")).isEmpty());
}

void UpdateServiceTest::proxyUrlForNetworkProxy_handlesHttpProxy()
{
    const QNetworkProxy proxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), 7890);
    QCOMPARE(UpdateService::proxyUrlForNetworkProxy(proxy), QStringLiteral("http://127.0.0.1:7890"));
}

void UpdateServiceTest::proxyUrlForNetworkProxy_handlesSocksProxy()
{
    QNetworkProxy proxy(QNetworkProxy::Socks5Proxy, QStringLiteral("127.0.0.1"), 1080);
    proxy.setUser(QStringLiteral("tester"));
    proxy.setPassword(QStringLiteral("secret"));
    QCOMPARE(UpdateService::proxyUrlForNetworkProxy(proxy),
             QStringLiteral("socks5://tester:secret@127.0.0.1:1080"));
}

void UpdateServiceTest::preferredProxyUrl_skipsUnsupportedEntries()
{
    const QList<QNetworkProxy> proxies{
        QNetworkProxy(QNetworkProxy::NoProxy),
        QNetworkProxy(QNetworkProxy::DefaultProxy),
        QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), 7890)
    };
    QCOMPARE(UpdateService::preferredProxyUrl(proxies), QStringLiteral("http://127.0.0.1:7890"));
}

void UpdateServiceTest::proxyUrlsForEnvironment_readsCommonVariables()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("HTTPS_PROXY"), QStringLiteral("127.0.0.1:7890"));
    environment.insert(QStringLiteral("ALL_PROXY"), QStringLiteral("socks5://localhost:1080"));

    const auto proxyUrls = UpdateService::proxyUrlsForEnvironment(environment);
    QVERIFY(proxyUrls.contains(QStringLiteral("http://127.0.0.1:7890")));
    QVERIFY(proxyUrls.contains(QStringLiteral("socks5://localhost:1080")));
}

void UpdateServiceTest::localProxyCandidates_containsCommonPorts()
{
    const auto proxyUrls = UpdateService::localProxyCandidates({QStringLiteral("127.0.0.1")});
    QVERIFY(proxyUrls.contains(QStringLiteral("http://127.0.0.1:7890")));
    QVERIFY(proxyUrls.contains(QStringLiteral("http://127.0.0.1:10809")));
    QVERIFY(proxyUrls.contains(QStringLiteral("socks5://127.0.0.1:1080")));
}

QTEST_APPLESS_MAIN(UpdateServiceTest)

#include "UpdateServiceTest.moc"

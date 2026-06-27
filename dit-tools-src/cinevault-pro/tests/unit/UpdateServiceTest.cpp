#include "application/UpdateService.h"

#include <QNetworkProxy>
#include <QtTest>

class UpdateServiceTest : public QObject {
    Q_OBJECT

private slots:
    void compareVersionTags_ordersSemanticVersions();
    void compareVersionTags_handlesInvalidValues();
    void parseLatestRelease_returnsInstallerAsset();
    void parseLatestRelease_rejectsMissingInstaller();
    void parseLatestRelease_rejectsInvalidPayload();
    void latestReleaseStatusMessage_handlesNoRelease();
    void proxyUrlForNetworkProxy_handlesHttpProxy();
    void proxyUrlForNetworkProxy_handlesSocksProxy();
    void preferredProxyUrl_skipsUnsupportedEntries();
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
    QVERIFY(!UpdateService::parseLatestRelease(payload, &info, &errorMessage));
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

QTEST_APPLESS_MAIN(UpdateServiceTest)

#include "UpdateServiceTest.moc"

#include "ui/models/MaterialCenterFolderListModel.h"
#include "ui/models/MaterialCenterFrameListModel.h"
#include "ui/models/MaterialCenterListModel.h"

#include <QFile>
#include <QtTest>

namespace {
QString sourceFile(const QString &relativePath)
{
    QFile file(QString::fromUtf8(CINEVAULT_SOURCE_DIR) + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString sourceSection(const QString &source,
                      const QString &startMarker,
                      const QString &endMarker)
{
    const auto start = source.indexOf(startMarker);
    if (start < 0) {
        return {};
    }
    const auto end = source.indexOf(endMarker, start + startMarker.size());
    return end < 0 ? source.mid(start) : source.mid(start, end - start);
}

void verifyOrdered(const QString &source, const QStringList &contracts)
{
    qsizetype previous = -1;
    for (const auto &contract : contracts) {
        const auto current = source.indexOf(contract, previous + 1);
        QVERIFY2(current >= 0,
                 qPrintable(QStringLiteral("源码缺少或顺序错误：%1").arg(contract)));
        previous = current;
    }
}

GlobalVideoAsset sampleAsset()
{
    GlobalVideoAsset asset;
    asset.videoKey = QStringLiteral("asset-1");
    asset.assetKey = QStringLiteral("asset-key-1");
    asset.fileName = QStringLiteral("A001_C001.mov");
    asset.projectName = QStringLiteral("上海项目");
    asset.sourceRootName = QStringLiteral("Camera A");
    asset.relativePath = QStringLiteral("2026-07-14/A001_C001.mov");
    asset.assetType = AssetType::Video;
    asset.captureDate = QStringLiteral("2026-07-14");
    asset.captureTimeSource = QStringLiteral("quicktime_creation_date");
    asset.searchScore = 0.91;
    asset.searchConfidence = 0.88;
    asset.searchReasons = {QStringLiteral("拍摄日期命中"), QStringLiteral("视觉帧命中：00:01:24")};
    asset.matchedTimestampMs = 84000;
    asset.matchedFrameCaption = QStringLiteral("雨夜里人物撑着红伞");
    return asset;
}

FolderSearchHit sampleFolder()
{
    FolderSearchHit folder;
    folder.folderKey = QStringLiteral("folder-1");
    folder.projectUuid = QStringLiteral("project-1");
    folder.projectName = QStringLiteral("上海项目");
    folder.projectDatabasePath = QStringLiteral("G:/projects/shanghai/project.sqlite");
    folder.sourceRootId = 17;
    folder.sourceRootName = QStringLiteral("Camera A");
    folder.name = QStringLiteral("夜景航拍");
    folder.absolutePath = QStringLiteral("G:/media/2026-07-14/夜景航拍");
    folder.relativePath = QStringLiteral("2026-07-14/夜景航拍");
    folder.parentRelativePath = QStringLiteral("2026-07-14");
    folder.depth = 2;
    folder.directFileCount = 8;
    folder.recursiveFileCount = 13;
    folder.normalizedDate = QStringLiteral("2026-07-14");
    folder.available = true;
    folder.score = 0.82;
    folder.confidence = 0.76;
    folder.reasons = {QStringLiteral("目录日期命中"), QStringLiteral("文件夹名称或路径命中")};
    return folder;
}

FrameSearchHit sampleFrame()
{
    FrameSearchHit frame;
    frame.frameKey = QStringLiteral("frame:asset-1:61");
    frame.videoKey = QStringLiteral("asset-1");
    frame.assetKey = QStringLiteral("asset-key-1");
    frame.fileName = QStringLiteral("A001_C001.mov");
    frame.projectName = QStringLiteral("上海项目");
    frame.sourceRootName = QStringLiteral("Camera A");
    frame.relativePath = QStringLiteral("2026-07-14/A001_C001.mov");
    frame.assetType = AssetType::Video;
    frame.frameNumber = 61;
    frame.timestampMs = 2000;
    frame.imagePath = QStringLiteral("G:/cache/frame-61.jpg");
    frame.caption = QStringLiteral("模特穿着深蓝色牛仔裤坐在窗边");
    frame.tags = {QStringLiteral("蓝色"), QStringLiteral("牛仔")};
    frame.objects = {QStringLiteral("牛仔裤"), QStringLiteral("藤编椅")};
    frame.actions = QStringLiteral("坐在藤编椅上");
    frame.setting = QStringLiteral("室内窗边");
    VisionEntityFact jeans;
    jeans.label = QStringLiteral("长裤");
    jeans.colors = {QStringLiteral("蓝色")};
    jeans.materials = {QStringLiteral("牛仔")};
    frame.entities = {jeans};
    frame.factsComplete = true;
    frame.score = 0.93;
    frame.confidence = 0.89;
    frame.reasons = {QStringLiteral("帧视觉事实或文字命中"),
                     QStringLiteral("同一帧、同一视觉对象属性已验证")};
    return frame;
}
}

class MaterialCenterUiContractTest : public QObject {
    Q_OBJECT

private slots:
    void folderModelExposesNavigationAndRankingRoles()
    {
        MaterialCenterFolderListModel model;
        model.setItems({sampleFolder()});

        QCOMPARE(model.rowCount(), 1);
        const auto index = model.index(0, 0);
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::FolderKeyRole).toString(),
                 QStringLiteral("folder-1"));
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::ProjectDatabasePathRole).toString(),
                 QStringLiteral("G:/projects/shanghai/project.sqlite"));
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::AbsolutePathRole).toString(),
                 QStringLiteral("G:/media/2026-07-14/夜景航拍"));
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::RecursiveFileCountRole).toLongLong(),
                 qint64{13});
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::ScoreRole).toDouble(), 0.82);
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::ConfidenceRole).toDouble(), 0.76);
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::ReasonsRole).toString(),
                 QStringLiteral("目录日期命中 · 文件夹名称或路径命中"));
        QCOMPARE(model.data(index, MaterialCenterFolderListModel::ResultRankRole).toInt(), 1);

        const auto roles = model.roleNames();
        QCOMPARE(roles.value(MaterialCenterFolderListModel::FolderKeyRole), QByteArray("folderKey"));
        QCOMPARE(roles.value(MaterialCenterFolderListModel::ProjectDatabasePathRole),
                 QByteArray("projectDatabasePath"));
        QCOMPARE(roles.value(MaterialCenterFolderListModel::ScoreRole), QByteArray("score"));
        QCOMPARE(roles.value(MaterialCenterFolderListModel::ConfidenceRole), QByteArray("confidence"));
        QCOMPARE(roles.value(MaterialCenterFolderListModel::ReasonsRole), QByteArray("reasons"));
        QCOMPARE(roles.value(MaterialCenterFolderListModel::ResultRankRole), QByteArray("resultRank"));
    }

    void assetModelPreservesHybridResultOrder()
    {
        auto first = sampleAsset();
        auto second = sampleAsset();
        second.videoKey = QStringLiteral("asset-2");
        second.assetKey = QStringLiteral("asset-key-2");
        second.fileName = QStringLiteral("A001_C002.mov");

        MaterialCenterListModel model;
        model.setItems({first, second});

        QCOMPARE(model.rowCount({}), 2);
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::VideoKeyRole).toString(),
                 QStringLiteral("asset-1"));
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::CaptureDateRole).toString(),
                 QStringLiteral("2026-07-14"));
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::CaptureTimeSourceLabelRole).toString(),
                 QStringLiteral("QuickTime 拍摄时间"));
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::SearchConfidenceRole).toDouble(),
                 0.88);
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::MatchedTimestampLabelRole).toString(),
                 QStringLiteral("00:01:24"));
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::MatchedFrameCaptionRole).toString(),
                 QStringLiteral("雨夜里人物撑着红伞"));
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::ResultRankRole).toInt(), 1);
        QCOMPARE(model.data(model.index(1, 0), MaterialCenterListModel::VideoKeyRole).toString(),
                 QStringLiteral("asset-2"));
        QCOMPARE(model.data(model.index(1, 0), MaterialCenterListModel::ResultRankRole).toInt(), 2);
    }

    void frameModelExposesThumbnailDetailsAndRankingRoles()
    {
        MaterialCenterFrameListModel model;
        model.setItems({sampleFrame()});

        QCOMPARE(model.rowCount(), 1);
        const auto index = model.index(0, 0);
        QCOMPARE(model.data(index, MaterialCenterFrameListModel::FrameKeyRole).toString(),
                 QStringLiteral("frame:asset-1:61"));
        QCOMPARE(model.data(index, MaterialCenterFrameListModel::FrameNumberRole).toInt(), 61);
        QCOMPARE(model.data(index, MaterialCenterFrameListModel::TimestampLabelRole).toString(),
                 QStringLiteral("00:00:02"));
        QCOMPARE(model.data(index, MaterialCenterFrameListModel::ImagePathRole).toString(),
                 QStringLiteral("G:/cache/frame-61.jpg"));
        QVERIFY(model.data(index, MaterialCenterFrameListModel::CaptionRole).toString()
                    .contains(QStringLiteral("深蓝色牛仔裤")));
        QVERIFY(model.data(index, MaterialCenterFrameListModel::EntitySummaryRole).toString()
                    .contains(QStringLiteral("长裤")));
        QCOMPARE(model.data(index, MaterialCenterFrameListModel::ConfidenceRole).toDouble(), 0.89);
        QCOMPARE(model.data(index, MaterialCenterFrameListModel::ResultRankRole).toInt(), 1);

        const auto roles = model.roleNames();
        QCOMPARE(roles.value(MaterialCenterFrameListModel::FrameKeyRole), QByteArray("frameKey"));
        QCOMPARE(roles.value(MaterialCenterFrameListModel::ImagePathRole), QByteArray("imagePath"));
        QCOMPARE(roles.value(MaterialCenterFrameListModel::EntitySummaryRole), QByteArray("entitySummary"));
        QCOMPARE(roles.value(MaterialCenterFrameListModel::ReasonsRole), QByteArray("reasons"));
    }

    void viewModelUsesHybridQueryAndKeepsEveryFilter()
    {
        const auto source = sourceFile(QStringLiteral("src/ui/viewmodels/MaterialCenterViewModel.cpp"));
        QVERIFY2(!source.isEmpty(), "无法读取 MaterialCenterViewModel.cpp");
        QVERIFY(source.contains(QStringLiteral("m_queryService->searchMaterials(m_searchText,")));
        QVERIFY(!source.contains(QStringLiteral("m_queryService->fetchAssets(")));
        QVERIFY(source.contains(QStringLiteral("scope.projectUuid = m_projectFilter")));
        QVERIFY(source.contains(QStringLiteral("scope.sourceRootName = m_sourceFilter")));
        QVERIFY(source.contains(QStringLiteral("scope.analysisStatusFilter = m_analysisStatusFilter")));
        QVERIFY(source.contains(QStringLiteral("scope.confirmationStatusFilter = m_confirmationStatusFilter")));
        QVERIFY(source.contains(QStringLiteral("scope.assetTypeFilter = m_assetTypeFilter")));
        QVERIFY(source.contains(QStringLiteral("m_folderModel->setItems(m_folders)")));
        QVERIFY(source.contains(QStringLiteral("m_frameModel->setItems(m_frames)")));
        QVERIFY(source.contains(QStringLiteral("m_semanticSearchAvailable = result.semanticSearchAvailable")));
        QVERIFY(source.contains(QStringLiteral("m_searchWarningMessage = result.warningMessage")));
        QVERIFY(source.contains(QStringLiteral("m_excludedPartialCount = result.excludedPartialCount")));
        QVERIFY(source.contains(QStringLiteral("m_searchInterpretationText = result.parsedQuery.interpretationLabels")));
        QVERIFY(source.contains(QStringLiteral("m_lastParsedQuery = result.parsedQuery")));
        QVERIFY(source.contains(QStringLiteral("m_searchEmptyReason")));
        QVERIFY(source.contains(QStringLiteral("SearchDocumentSyncService::synchronizationProgress")));
        QVERIFY(source.contains(QStringLiteral("SearchDocumentSyncService::synchronizationFinished")));
        QVERIFY(source.contains(QStringLiteral("m_searchRefreshTimer->start(0)")));
        QVERIFY(source.contains(QStringLiteral("understandSearchQuery")));
        QVERIFY(source.contains(QStringLiteral("rerankFrameCandidates")));
        QVERIFY(source.contains(QStringLiteral("task.generation != m_searchGeneration")));
    }

    void qmlConsumesSearchStateAndFolderActions()
    {
        const auto source = sourceFile(
            QStringLiteral("src/ui/qml/workspaces/MaterialCenterWorkspace.qml"));
        QVERIFY2(!source.isEmpty(), "无法读取 MaterialCenterWorkspace.qml");
        const QStringList contracts = {
            QStringLiteral("model: viewModel ? viewModel.folderModel : null"),
            QStringLiteral("model: viewModel ? viewModel.assetModel : null"),
            QStringLiteral("model: viewModel ? viewModel.frameModel : null"),
            QStringLiteral("viewModel.frameSearchMode"),
            QStringLiteral("id: frameResultGrid"),
            QStringLiteral("text: \"视觉帧命中\""),
            QStringLiteral("root.openImageViewer(root.imageFileSource(imagePath))"),
            QStringLiteral("visible: viewModel && !viewModel.frameSearchMode"),
            QStringLiteral("viewModel.semanticSearchAvailable"),
            QStringLiteral("viewModel.semanticSearchStatusText"),
            QStringLiteral("viewModel.semanticIndexing"),
            QStringLiteral("viewModel.semanticIndexProgress"),
            QStringLiteral("viewModel.semanticIndexStatusText"),
            QStringLiteral("viewModel.searchWarningMessage"),
            QStringLiteral("viewModel.searchInterpretationText"),
            QStringLiteral("viewModel.searchAssistantStatusText"),
            QStringLiteral("viewModel.searchEmptyReason"),
            QStringLiteral("viewModel.excludedPartialCount"),
            QStringLiteral("viewModel.openFolderProject(folderKey)"),
            QStringLiteral("viewModel.locateFolder(folderKey)"),
            QStringLiteral("viewModel.projectFilter"),
            QStringLiteral("viewModel.sourceFilter"),
            QStringLiteral("viewModel.analysisStatusFilter"),
            QStringLiteral("viewModel.confirmationStatusFilter"),
            QStringLiteral("searchConfidence"),
            QStringLiteral("searchReasons"),
            QStringLiteral("confidence"),
            QStringLiteral("reasons")
        };
        for (const auto &contract : contracts) {
            QVERIFY2(source.contains(contract), qPrintable(QStringLiteral("QML 缺少接口：%1").arg(contract)));
        }
    }

    void searchSettingsExposePrivacyAndBudgetContracts()
    {
        const auto header = sourceFile(QStringLiteral("src/ui/viewmodels/SettingsViewModel.h"));
        const auto implementation = sourceFile(QStringLiteral("src/ui/viewmodels/SettingsViewModel.cpp"));
        const auto qml = sourceFile(QStringLiteral("src/ui/qml/components/SettingsDialog.qml"));
        const auto appContext = sourceFile(QStringLiteral("src/app/AppContext.cpp"));
        QVERIFY2(!header.isEmpty() && !implementation.isEmpty() && !qml.isEmpty()
                     && !appContext.isEmpty(),
                 "无法读取搜索设置契约源码");

        const QStringList propertyContracts = {
            QStringLiteral("Q_PROPERTY(bool searchAssistantEnabled"),
            QStringLiteral("Q_PROPERTY(bool frameRerankEnabled"),
            QStringLiteral("Q_PROPERTY(bool localOnlySearch"),
            QStringLiteral("Q_PROPERTY(bool allowSearchFrameUpload"),
            QStringLiteral("Q_PROPERTY(int dailySearchModelCallLimit"),
            QStringLiteral("Q_PROPERTY(int searchModelCallsToday"),
            QStringLiteral("Q_PROPERTY(QString searchModelBudgetLabel"),
            QStringLiteral("void searchSettingsChanged()")
        };
        for (const auto &contract : propertyContracts) {
            QVERIFY2(header.contains(contract),
                     qPrintable(QStringLiteral("SettingsViewModel 缺少接口：%1").arg(contract)));
        }

        const QStringList qmlContracts = {
            QStringLiteral("模型辅助搜索与隐私"),
            QStringLiteral("仅本地搜索（不发起任何搜索模型网络请求）"),
            QStringLiteral("使用视觉语言模型辅助理解自然语言查询"),
            QStringLiteral("使用视觉语言模型复核前 8 个候选帧"),
            QStringLiteral("允许将候选帧缩略图发送到已配置的模型接口"),
            QStringLiteral("enabled: !root.draftLocalOnlySearch && root.draftFrameRerankEnabled"),
            QStringLiteral("viewModel.searchModelBudgetLabel"),
            QStringLiteral("查询理解只发送搜索文字"),
            QStringLiteral("候选帧复核会发送最多 8 张缩略图和对应候选 ID")
        };
        for (const auto &contract : qmlContracts) {
            QVERIFY2(qml.contains(contract),
                     qPrintable(QStringLiteral("设置 QML 缺少契约：%1").arg(contract)));
        }

        verifyOrdered(qml, {
            QStringLiteral("root.draftVisionModel,"),
            QStringLiteral("root.draftSearchAssistantEnabled,"),
            QStringLiteral("root.draftFrameRerankEnabled,"),
            QStringLiteral("root.draftLocalOnlySearch,"),
            QStringLiteral("root.draftAllowSearchFrameUpload,"),
            QStringLiteral("root.draftDailySearchModelCallLimit,"),
            QStringLiteral("root.draftAnalysisMode,")
        });
        QVERIFY(implementation.contains(QStringLiteral("emit searchSettingsChanged()")));
        QVERIFY(appContext.contains(QStringLiteral("&SettingsViewModel::searchSettingsChanged")));
    }

    void modelSearchGatesKeepCacheLocalAndConsumeOnlyBeforeRequests()
    {
        const auto source = sourceFile(QStringLiteral("src/ui/viewmodels/MaterialCenterViewModel.cpp"));
        QVERIFY2(!source.isEmpty(), "无法读取 MaterialCenterViewModel.cpp");

        const auto understanding = sourceSection(
            source,
            QStringLiteral("void MaterialCenterViewModel::startSearchUnderstanding"),
            QStringLiteral("void MaterialCenterViewModel::startFrameRerank"));
        QVERIFY2(!understanding.isEmpty(), "无法定位查询理解门禁");
        verifyOrdered(understanding, {
            QStringLiteral("m_settings && m_settings->localOnlySearch()"),
            QStringLiteral("!m_settings->searchAssistantEnabled()"),
            QStringLiteral("m_searchUnderstandingCache.constFind(cacheKey)"),
            QStringLiteral("!m_settings->canUseSearchModel(referenceDate)"),
            QStringLiteral("!m_visionApiClient"),
            QStringLiteral("!m_settings->tryConsumeSearchModelCall(referenceDate)"),
            QStringLiteral("client->understandSearchQuery")
        });
        QVERIFY(understanding.contains(QStringLiteral("已保留本地搜索")));

        const auto rerank = sourceSection(
            source,
            QStringLiteral("void MaterialCenterViewModel::startFrameRerank"),
            QStringLiteral("void MaterialCenterViewModel::applyFrameRerank"));
        QVERIFY2(!rerank.isEmpty(), "无法定位候选帧复核门禁");
        verifyOrdered(rerank, {
            QStringLiteral("m_settings && m_settings->localOnlySearch()"),
            QStringLiteral("!m_settings->frameRerankEnabled()"),
            QStringLiteral("!m_settings->allowSearchFrameUpload()"),
            QStringLiteral("m_frameRerankCache.constFind(cacheKey)"),
            QStringLiteral("!m_settings->canUseSearchModel(referenceDate)"),
            QStringLiteral("!m_visionApiClient"),
            QStringLiteral("!m_settings->tryConsumeSearchModelCall(referenceDate)"),
            QStringLiteral("client->rerankFrameCandidates")
        });
        QVERIFY(rerank.contains(QStringLiteral("候选帧缩略图发送未授权，保留本地排序")));
        QVERIFY(rerank.contains(QStringLiteral("候选帧保持本地排序")));
    }
};

QTEST_APPLESS_MAIN(MaterialCenterUiContractTest)

#include "MaterialCenterUiContractTest.moc"

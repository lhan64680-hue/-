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
    asset.thumbnailPath = QStringLiteral("G:/cache/A001_C001.webp");
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
        QCOMPARE(model.data(model.index(0, 0), MaterialCenterListModel::QuickPreviewPathRole).toString(),
                 QStringLiteral("G:/cache/A001_C001.webp"));
        QVERIFY(model.data(model.index(0, 0), MaterialCenterListModel::QuickDetailRole).toString()
                    .contains(QStringLiteral("雨夜里人物撑着红伞")));
        QVERIFY(model.data(model.index(0, 0), MaterialCenterListModel::QuickMetaRole).toString()
                    .contains(QStringLiteral("视频")));
        QVERIFY(model.data(model.index(0, 0), MaterialCenterListModel::QuickReasonsRole).toString()
                    .contains(QStringLiteral("拍摄日期命中")));
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
        QCOMPARE(model.data(index, MaterialCenterFrameListModel::QuickPreviewPathRole).toString(),
                 QStringLiteral("G:/cache/frame-61.jpg"));
        QVERIFY(model.data(index, MaterialCenterFrameListModel::QuickDetailRole).toString()
                    .contains(QStringLiteral("深蓝色牛仔裤")));
        QVERIFY(model.data(index, MaterialCenterFrameListModel::QuickMetaRole).toString()
                    .contains(QStringLiteral("00:00:02")));
        QVERIFY(model.data(index, MaterialCenterFrameListModel::QuickReasonsRole).toString()
                    .contains(QStringLiteral("同一帧")));

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
            QStringLiteral("id: searchInput"),
            QStringLiteral("text: \"清空输入\""),
            QStringLiteral("enabled: searchInput.text.length > 0"),
            QStringLiteral("shellVm.globalSearchText = \"\""),
            QStringLiteral("searchInput.forceActiveFocus()"),
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

    void batchAnalysisRequiresExplicitSupplementOrRebuildPolicy()
    {
        const auto qml = sourceFile(QStringLiteral("src/ui/qml/workspaces/MaterialCenterWorkspace.qml"));
        const auto viewModelHeader = sourceFile(QStringLiteral("src/ui/viewmodels/MaterialCenterViewModel.h"));
        const auto viewModel = sourceFile(QStringLiteral("src/ui/viewmodels/MaterialCenterViewModel.cpp"));
        const auto serviceHeader = sourceFile(QStringLiteral("src/application/VideoAnalysisService.h"));
        const auto service = sourceFile(QStringLiteral("src/application/VideoAnalysisService.cpp"));
        QVERIFY2(!qml.isEmpty() && !viewModelHeader.isEmpty() && !viewModel.isEmpty()
                     && !serviceHeader.isEmpty() && !service.isEmpty(),
                 "无法读取批量解析策略契约源码");

        QVERIFY(qml.contains(QStringLiteral("选择批量解析方式")));
        QVERIFY(qml.contains(QStringLiteral("补充解析（推荐）")));
        QVERIFY(qml.contains(QStringLiteral("完整帧直接跳过")));
        QVERIFY(qml.contains(QStringLiteral("viewModel.analyzeVisibleSupplement()")));
        QVERIFY(qml.contains(QStringLiteral("viewModel.analyzeVisibleAll()")));
        QVERIFY(qml.contains(QStringLiteral("Math.max(1, Math.min(620, root.width - 24))")));
        QVERIFY(qml.contains(QStringLiteral("Math.max(1, Math.min(430, root.height - 24))")));
        QVERIFY(qml.contains(QStringLiteral("id: batchAnalyzeScroll")));
        QVERIFY(qml.contains(QStringLiteral("implicitHeight: supplementOptionContent.implicitHeight + 28")));
        QVERIFY(!qml.contains(QStringLiteral("解析未完成素材")));

        QVERIFY(viewModelHeader.contains(QStringLiteral("Q_INVOKABLE void analyzeVisibleSupplement()")));
        QVERIFY(viewModel.contains(QStringLiteral("asset.analysisStatus == VideoAnalysisStatus::Failed")));
        QVERIFY(viewModel.contains(QStringLiteral("asset.analysisStatus == VideoAnalysisStatus::Running")));
        QVERIFY(viewModel.contains(QStringLiteral("enqueueVideosForSupplement(videoKeys, &message)")));
        QVERIFY(viewModel.contains(QStringLiteral("enqueueVideosForRebuild(videoKeys, &message)")));
        QVERIFY(serviceHeader.contains(QStringLiteral("enqueueVideosForSupplement")));
        QVERIFY(serviceHeader.contains(QStringLiteral("enqueueVideosForRebuild")));

        const auto supplement = sourceSection(
            service,
            QStringLiteral("int VideoAnalysisService::enqueueVideosForSupplement"),
            QStringLiteral("int VideoAnalysisService::enqueueVideosForRebuild"));
        QVERIFY(supplement.contains(QStringLiteral("hasIncompleteVisualFrames")));
        QVERIFY(supplement.contains(QStringLiteral("AnalysisRunMode::Resume")));
        QVERIFY(supplement.contains(QStringLiteral("if (!hasVisualGap)")));
        QVERIFY(!supplement.contains(QStringLiteral("AnalysisRunMode::Rebuild")));
        QVERIFY(!supplement.contains(QStringLiteral("deleteAnalysisArtifacts")));

        const auto rebuild = sourceSection(
            service,
            QStringLiteral("int VideoAnalysisService::enqueueVideosForRebuild"),
            QStringLiteral("bool VideoAnalysisService::retryFrame"));
        QVERIFY(rebuild.contains(QStringLiteral("AnalysisRunMode::Rebuild")));
    }

    void searchSettingsExposePrivacyUnlimitedCallsAndQuickSearchContracts()
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
            QStringLiteral("Q_PROPERTY(bool quickSearchEnabled"),
            QStringLiteral("Q_PROPERTY(QString quickSearchShortcut"),
            QStringLiteral("Q_PROPERTY(bool startAtLogin"),
            QStringLiteral("Q_PROPERTY(QString quickSearchStatusText"),
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
            QStringLiteral("像 Flow Launcher 一样"),
            QStringLiteral("root.draftQuickSearchShortcut"),
            QStringLiteral("viewModel.shortcutFromKeyEvent"),
            QStringLiteral("viewModel.quickSearchStatusText"),
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
            QStringLiteral("root.draftQuickSearchEnabled,"),
            QStringLiteral("root.draftQuickSearchShortcut,"),
            QStringLiteral("root.draftStartAtLogin,"),
            QStringLiteral("root.draftCloseButtonBehavior,"),
            QStringLiteral("root.draftAnalysisMode,")
        });
        QVERIFY(!header.contains(QStringLiteral("dailySearchModelCallLimit")));
        QVERIFY(!header.contains(QStringLiteral("searchModelCallsToday")));
        QVERIFY(!qml.contains(QStringLiteral("每日搜索模型调用上限")));
        QVERIFY(!qml.contains(QStringLiteral("searchModelBudgetLabel")));
        QVERIFY(implementation.contains(QStringLiteral("emit searchSettingsChanged()")));
        QVERIFY(appContext.contains(QStringLiteral("&SettingsViewModel::searchSettingsChanged")));
    }

    void settingsSwitchLabelsFollowActiveTheme()
    {
        const auto settingsQml = sourceFile(QStringLiteral("src/ui/qml/components/SettingsDialog.qml"));
        const auto switchQml = sourceFile(QStringLiteral("src/ui/qml/components/ThemedSwitch.qml"));
        QVERIFY2(!settingsQml.isEmpty() && !switchQml.isEmpty(),
                 "无法读取设置页主题开关源码");

        QCOMPARE(settingsQml.count(QStringLiteral("ThemedSwitch {")), 7);
        QVERIFY(switchQml.contains(QStringLiteral("color: control.enabled ? Theme.text : Theme.weak")));
        QVERIFY(switchQml.contains(QStringLiteral("palette.windowText: control.enabled ? Theme.text : Theme.weak")));
        QVERIFY(switchQml.contains(QStringLiteral("color: control.checked ? Theme.primaryBg : Theme.inputPressed")));
    }

    void sourceImportAcceptsTypedNetworkPaths()
    {
        const auto mainQml = sourceFile(QStringLiteral("src/ui/qml/Main.qml"));
        const auto shellHeader = sourceFile(QStringLiteral("src/ui/viewmodels/ShellViewModel.h"));
        const auto importSource = sourceFile(QStringLiteral("src/application/ImportService.cpp"));
        QVERIFY2(!mainQml.isEmpty() && !shellHeader.isEmpty() && !importSource.isEmpty(),
                 "无法读取素材源网络路径契约源码");

        QVERIFY(mainQml.contains(QStringLiteral("UNC 网络共享路径")));
        QVERIFY(mainQml.contains(QStringLiteral("root.shellViewModel.importSourcePath(sourcePathField.text)")));
        QVERIFY(shellHeader.contains(QStringLiteral("Q_INVOKABLE bool importSourcePath(const QString &directoryPath)")));
        QVERIFY(importSource.contains(QStringLiteral("FolderPathMetadata::normalizeSourcePath(directoryPath)")));
        QVERIFY(importSource.contains(QStringLiteral("目录不存在或网络路径不可访问")));
    }

    void closeButtonOffersPromptTrayAndExitBehaviors()
    {
        const auto mainQml = sourceFile(QStringLiteral("src/ui/qml/Main.qml"));
        const auto settingsQml = sourceFile(QStringLiteral("src/ui/qml/components/SettingsDialog.qml"));
        const auto settingsHeader = sourceFile(QStringLiteral("src/ui/viewmodels/SettingsViewModel.h"));
        const auto trayHeader = sourceFile(QStringLiteral("src/ui/window/QuickSearchController.h"));
        QVERIFY2(!mainQml.isEmpty() && !settingsQml.isEmpty()
                     && !settingsHeader.isEmpty() && !trayHeader.isEmpty(),
                 "无法读取关闭行为契约源码");

        QVERIFY(settingsHeader.contains(QStringLiteral("Q_PROPERTY(int closeButtonBehavior")));
        QVERIFY(trayHeader.contains(QStringLiteral("Q_PROPERTY(bool trayAvailable")));
        QVERIFY(settingsQml.contains(QStringLiteral("每次询问")));
        QVERIFY(settingsQml.contains(QStringLiteral("最小化到托盘")));
        QVERIFY(settingsQml.contains(QStringLiteral("直接退出软件")));
        QVERIFY(mainQml.contains(QStringLiteral("if (behavior === 2)")));
        QVERIFY(mainQml.contains(QStringLiteral("if (behavior === 1)")));
        QVERIFY(mainQml.contains(QStringLiteral("closeConfirmDialog.open()")));
        QVERIFY(mainQml.contains(QStringLiteral("Qt.quit()")));
    }

    void modelSearchGatesKeepLocalControlsWithoutCallLimits()
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
            QStringLiteral("!m_visionApiClient"),
            QStringLiteral("client->understandSearchQuery")
        });
        QVERIFY(understanding.contains(QStringLiteral("已保留本地搜索")));
        QVERIFY(!understanding.contains(QStringLiteral("canUseSearchModel")));
        QVERIFY(!understanding.contains(QStringLiteral("tryConsumeSearchModelCall")));
        QVERIFY(!understanding.contains(QStringLiteral("预算")));

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
            QStringLiteral("!m_visionApiClient"),
            QStringLiteral("client->rerankFrameCandidates")
        });
        QVERIFY(rerank.contains(QStringLiteral("候选帧缩略图发送未授权，保留本地排序")));
        QVERIFY(!rerank.contains(QStringLiteral("canUseSearchModel")));
        QVERIFY(!rerank.contains(QStringLiteral("tryConsumeSearchModelCall")));
        QVERIFY(!rerank.contains(QStringLiteral("预算")));
    }

    void flowStyleQuickSearchHasGlobalHotkeyAndKeyboardContracts()
    {
        const auto controllerHeader = sourceFile(QStringLiteral("src/ui/window/QuickSearchController.h"));
        const auto controllerSource = sourceFile(QStringLiteral("src/ui/window/QuickSearchController.cpp"));
        const auto quickSearchQml = sourceFile(QStringLiteral("src/ui/qml/components/QuickSearchWindow.qml"));
        const auto mainQml = sourceFile(QStringLiteral("src/ui/qml/Main.qml"));
        QVERIFY2(!controllerHeader.isEmpty() && !controllerSource.isEmpty()
                     && !quickSearchQml.isEmpty() && !mainQml.isEmpty(),
                 "无法读取快捷搜索契约源码");

        const QStringList nativeContracts = {
            QStringLiteral("QAbstractNativeEventFilter"),
            QStringLiteral("RegisterHotKey"),
            QStringLiteral("UnregisterHotKey"),
            QStringLiteral("Alt+Space"),
            QStringLiteral("quickSearchRequested"),
            QStringLiteral("QSystemTrayIcon"),
            QStringLiteral("--background")
        };
        for (const auto &contract : nativeContracts) {
            QVERIFY2(controllerHeader.contains(contract) || controllerSource.contains(contract),
                     qPrintable(QStringLiteral("快捷搜索控制器缺少契约：%1").arg(contract)));
        }

        const QStringList qmlContracts = {
            QStringLiteral("Qt.FramelessWindowHint"),
            QStringLiteral("Qt.WindowStaysOnTopHint"),
            QStringLiteral("materialCenterViewModel.setSearchText"),
            QStringLiteral("materialCenterViewModel.folderModel"),
            QStringLiteral("materialCenterViewModel.frameModel"),
            QStringLiteral("materialCenterViewModel.assetModel"),
            QStringLiteral("Keys.onDownPressed"),
            QStringLiteral("Keys.onUpPressed"),
            QStringLiteral("Ctrl+Enter 定位"),
            QStringLiteral("sequence: \"Escape\""),
            QStringLiteral("openSelectedProject"),
            QStringLiteral("openFolderProject"),
            QStringLiteral("required property string quickPreviewPath"),
            QStringLiteral("required property string quickDetail"),
            QStringLiteral("required property string quickMeta"),
            QStringLiteral("required property string quickReasons"),
            QStringLiteral("DragHandler"),
            QStringLiteral("restoredWindowPosition"),
            QStringLiteral("rememberWindowPosition"),
            QStringLiteral("打开详情  →")
        };
        for (const auto &contract : qmlContracts) {
            QVERIFY2(quickSearchQml.contains(contract),
                     qPrintable(QStringLiteral("快捷搜索 QML 缺少契约：%1").arg(contract)));
        }
        const QStringList neutralDarkPaletteContracts = {
            QStringLiteral("readonly property color quickBg: \"#0E1014\""),
            QStringLiteral("readonly property color quickHeader: \"#171A20\""),
            QStringLiteral("readonly property color quickSelected: \"#2A3039\""),
            QStringLiteral("readonly property color quickAccent: \"#C6CDD7\"")
        };
        for (const auto &contract : neutralDarkPaletteContracts) {
            QVERIFY2(quickSearchQml.contains(contract),
                     qPrintable(QStringLiteral("快捷搜索缺少中性深灰主题契约：%1").arg(contract)));
        }
        QVERIFY(!quickSearchQml.contains(QStringLiteral("Theme.selectedBg")));
        QVERIFY(!quickSearchQml.contains(QStringLiteral("Theme.selectedLine")));
        QVERIFY(!quickSearchQml.contains(QStringLiteral("Theme.blue")));
        QVERIFY(!quickSearchQml.contains(QStringLiteral("Theme.orange")));
        QVERIFY(!quickSearchQml.contains(QStringLiteral("model.")));
        QVERIFY(controllerHeader.contains(QStringLiteral("clampWindowPosition")));
        QVERIFY(controllerSource.contains(QStringLiteral("availableGeometry")));
        QVERIFY(mainQml.contains(QStringLiteral("sequence: \"Ctrl+K\"")));
        QVERIFY(mainQml.contains(QStringLiteral("QuickSearchWindow")));
    }
};

QTEST_APPLESS_MAIN(MaterialCenterUiContractTest)

#include "MaterialCenterUiContractTest.moc"

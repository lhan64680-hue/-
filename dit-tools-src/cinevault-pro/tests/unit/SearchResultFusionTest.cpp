#include "core/search/SearchResultFusion.h"

#include <QtTest>

namespace {
GlobalVideoAsset asset(const QString &key)
{
    GlobalVideoAsset item;
    item.videoKey = key;
    item.fileName = key + QStringLiteral(".mov");
    return item;
}

FrameSearchHit frame(const QString &key, const QString &videoKey)
{
    FrameSearchHit item;
    item.frameKey = key;
    item.videoKey = videoKey;
    return item;
}
}

class SearchResultFusionTest : public QObject {
    Q_OBJECT

private slots:
    void keepsNonEmptyBaselineWhenEnhancedResultIsEmpty()
    {
        MaterialSearchResult baseline;
        baseline.parsedQuery.originalText = QStringLiteral("查找黄昏的画面");
        baseline.parsedQuery.resultTarget = SearchResultTarget::Frames;
        baseline.frames = {frame(QStringLiteral("frame:dusk:1"), QStringLiteral("dusk"))};

        MaterialSearchResult enhanced;
        enhanced.parsedQuery = baseline.parsedQuery;

        const auto outcome = SearchResultFusion::preserveBaselineRecall(baseline, enhanced);

        QCOMPARE(outcome.protection, SearchRecallProtection::EnhancedResultEmpty);
        QCOMPARE(outcome.preservedHitCount, qsizetype{1});
        QCOMPARE(outcome.result.frames.size(), 1);
        QCOMPARE(outcome.result.frames.first().frameKey, QStringLiteral("frame:dusk:1"));
    }

    void unionsEnhancedAndBaselineHitsForTheSameTarget()
    {
        MaterialSearchResult baseline;
        baseline.parsedQuery.originalText = QStringLiteral("红色牛仔裤女人");
        baseline.assets = {asset(QStringLiteral("baseline"))};
        MaterialSearchResult enhanced = baseline;
        enhanced.assets = {asset(QStringLiteral("enhanced"))};

        const auto outcome = SearchResultFusion::preserveBaselineRecall(baseline, enhanced);

        QCOMPARE(outcome.protection, SearchRecallProtection::BaselineHitsAdded);
        QCOMPARE(outcome.result.assets.size(), 2);
        QCOMPARE(outcome.result.assets.at(0).videoKey, QStringLiteral("enhanced"));
        QCOMPARE(outcome.result.assets.at(1).videoKey, QStringLiteral("baseline"));
    }

    void refusesToReplaceExistingRecallWithAnotherResultTarget()
    {
        MaterialSearchResult baseline;
        baseline.parsedQuery.originalText = QStringLiteral("找穿裤子的人");
        baseline.assets = {asset(QStringLiteral("asset-1"))};
        MaterialSearchResult enhanced;
        enhanced.parsedQuery.originalText = baseline.parsedQuery.originalText;
        enhanced.parsedQuery.resultTarget = SearchResultTarget::Frames;
        enhanced.frames = {frame(QStringLiteral("frame:asset-2:1"), QStringLiteral("asset-2"))};

        const auto outcome = SearchResultFusion::preserveBaselineRecall(baseline, enhanced);

        QCOMPARE(outcome.protection, SearchRecallProtection::ResultTargetChanged);
        QCOMPARE(outcome.result.parsedQuery.resultTarget, SearchResultTarget::Assets);
        QCOMPARE(outcome.result.assets.size(), 1);
        QVERIFY(outcome.result.frames.isEmpty());
    }

    void usesEnhancedResultWhenBaselineHasNoHits()
    {
        MaterialSearchResult baseline;
        baseline.parsedQuery.originalText = QStringLiteral("模糊关系查询");
        MaterialSearchResult enhanced = baseline;
        enhanced.assets = {asset(QStringLiteral("model-hit"))};

        const auto outcome = SearchResultFusion::preserveBaselineRecall(baseline, enhanced);

        QCOMPARE(outcome.protection, SearchRecallProtection::None);
        QCOMPARE(outcome.result.assets.size(), 1);
    }
};

QTEST_GUILESS_MAIN(SearchResultFusionTest)

#include "SearchResultFusionTest.moc"

#include "core/search/SearchResultFusion.h"

#include "core/search/SearchReliabilityEvaluator.h"

#include <QSet>

#include <algorithm>

namespace {
qsizetype hitCount(const MaterialSearchResult &result)
{
    return result.folders.size() + result.assets.size() + result.frames.size();
}

void appendWarning(QString *target, const QString &warning)
{
    const auto normalized = warning.trimmed();
    if (normalized.isEmpty() || target->contains(normalized)) {
        return;
    }
    if (!target->isEmpty()) {
        target->append(QStringLiteral("；"));
    }
    target->append(normalized);
}

template<typename Item, typename KeySelector>
qsizetype appendMissing(QVector<Item> *target,
                        const QVector<Item> &baseline,
                        KeySelector keySelector)
{
    QSet<QString> existing;
    existing.reserve(target->size());
    for (const auto &item : std::as_const(*target)) {
        existing.insert(keySelector(item));
    }

    qsizetype added = 0;
    for (const auto &item : baseline) {
        const auto key = keySelector(item);
        if (key.isEmpty() || existing.contains(key)) {
            continue;
        }
        target->append(item);
        existing.insert(key);
        ++added;
    }
    return added;
}

void mergeSearchMetadata(MaterialSearchResult *target,
                         const MaterialSearchResult &other)
{
    target->semanticSearchAvailable = target->semanticSearchAvailable
        || other.semanticSearchAvailable;
    target->excludedPartialCount = std::max(target->excludedPartialCount,
                                            other.excludedPartialCount);
    appendWarning(&target->warningMessage, other.warningMessage);
}
}

SearchResultFusionOutcome SearchResultFusion::preserveBaselineRecall(
    const MaterialSearchResult &baseline,
    const MaterialSearchResult &enhanced)
{
    SearchResultFusionOutcome outcome;
    const auto baselineCount = hitCount(baseline);
    const auto enhancedCount = hitCount(enhanced);

    if (baselineCount <= 0) {
        outcome.result = enhanced;
        outcome.result.reliability = SearchReliabilityEvaluator::evaluate(outcome.result);
        return outcome;
    }

    if (enhancedCount <= 0) {
        outcome.result = baseline;
        mergeSearchMetadata(&outcome.result, enhanced);
        outcome.protection = SearchRecallProtection::EnhancedResultEmpty;
        outcome.preservedHitCount = baselineCount;
        outcome.result.reliability = SearchReliabilityEvaluator::evaluate(outcome.result);
        return outcome;
    }

    if (baseline.parsedQuery.resultTarget != enhanced.parsedQuery.resultTarget) {
        outcome.result = baseline;
        mergeSearchMetadata(&outcome.result, enhanced);
        outcome.protection = SearchRecallProtection::ResultTargetChanged;
        outcome.preservedHitCount = baselineCount;
        outcome.result.reliability = SearchReliabilityEvaluator::evaluate(outcome.result);
        return outcome;
    }

    outcome.result = enhanced;
    switch (enhanced.parsedQuery.resultTarget) {
    case SearchResultTarget::Folders:
        outcome.preservedHitCount = appendMissing(
            &outcome.result.folders,
            baseline.folders,
            [](const FolderSearchHit &item) { return item.folderKey; });
        break;
    case SearchResultTarget::Frames:
        outcome.preservedHitCount = appendMissing(
            &outcome.result.frames,
            baseline.frames,
            [](const FrameSearchHit &item) { return item.frameKey; });
        break;
    case SearchResultTarget::Assets:
        outcome.preservedHitCount = appendMissing(
            &outcome.result.assets,
            baseline.assets,
            [](const GlobalVideoAsset &item) { return item.videoKey; });
        break;
    }
    mergeSearchMetadata(&outcome.result, baseline);
    if (outcome.preservedHitCount > 0) {
        outcome.protection = SearchRecallProtection::BaselineHitsAdded;
    }
    outcome.result.reliability = SearchReliabilityEvaluator::evaluate(outcome.result);
    return outcome;
}

#include "core/search/NaturalLanguageQueryParser.h"
#include "core/search/SearchQueryUnderstanding.h"

#include <QJsonArray>
#include <QtTest>

#include <algorithm>

namespace {
QJsonObject validPayload()
{
    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("result_target"), QStringLiteral("frames")},
        {QStringLiteral("semantic_text"), QStringLiteral("穿蓝色牛仔长裤的人物")},
        {QStringLiteral("lexical_terms"), QJsonArray{QStringLiteral("人物"), QStringLiteral("长裤")}},
        {QStringLiteral("asset_types"), QJsonArray{QStringLiteral("video")}},
        {QStringLiteral("date"), QJsonObject{
            {QStringLiteral("start"), QString()}, {QStringLiteral("end"), QString()},
            {QStringLiteral("matched_text"), QString()}, {QStringLiteral("preferred_field"), QStringLiteral("any")}
        }},
        {QStringLiteral("folder_by_asset_criteria"), false},
        {QStringLiteral("ocr_text"), QString()},
        {QStringLiteral("entities"), QJsonArray{QJsonObject{
            {QStringLiteral("label"), QStringLiteral("裤子")},
            {QStringLiteral("colors"), QJsonArray{QStringLiteral("蓝色")}},
            {QStringLiteral("materials"), QJsonArray{QStringLiteral("牛仔")}},
            {QStringLiteral("attributes"), QJsonArray{QStringLiteral("长裤")}}
        }}},
        {QStringLiteral("confidence"), 0.91},
        {QStringLiteral("explanation"), QStringLiteral("用户在描述画面内容")}
    };
}
}

class SearchQueryUnderstandingTest : public QObject {
    Q_OBJECT

private slots:
    void parsesStrictWhitelistedPayload()
    {
        QString error;
        const auto parsed = SearchQueryUnderstanding::parseModelPayload(validPayload(), &error);
        QVERIFY2(parsed.has_value(), qPrintable(error));
        QCOMPARE(parsed->resultTarget, SearchResultTarget::Frames);
        QCOMPARE(parsed->assetTypeFilters, QVector<int>{static_cast<int>(AssetType::Video)});
        QCOMPARE(parsed->strictEntities.size(), 1);
        QCOMPARE(parsed->strictEntities.first().colors, QStringList{QStringLiteral("蓝色")});
    }

    void localExplicitDateTypeAndTargetAlwaysWin()
    {
        NaturalLanguageQueryParser parser;
        const auto local = parser.parse(QStringLiteral("昨天拍摄的视频"), QDate(2026, 7, 14));
        auto payload = validPayload();
        payload.insert(QStringLiteral("date"), QJsonObject{
            {QStringLiteral("start"), QStringLiteral("2020-01-01")},
            {QStringLiteral("end"), QStringLiteral("2020-01-02")},
            {QStringLiteral("matched_text"), QStringLiteral("错误日期")},
            {QStringLiteral("preferred_field"), QStringLiteral("modified")}
        });
        QString error;
        const auto model = SearchQueryUnderstanding::parseModelPayload(payload, &error);
        QVERIFY2(model.has_value(), qPrintable(error));
        bool applied = false;
        const auto merged = SearchQueryUnderstanding::merge(local, *model, &applied);
        QVERIFY(applied);
        QCOMPARE(merged.resultTarget, SearchResultTarget::Assets);
        QCOMPARE(merged.assetTypeFilters, QVector<int>{static_cast<int>(AssetType::Video)});
        QCOMPARE(merged.dateConstraint.startDate, QStringLiteral("2026-07-13"));
        QCOMPARE(merged.dateConstraint.preferredField, SearchDateField::CapturedTime);
    }

    void modelCanEnrichVagueVisualQuery()
    {
        NaturalLanguageQueryParser parser;
        const auto local = parser.parse(QStringLiteral("找那个穿裤子的人"));
        QString error;
        const auto model = SearchQueryUnderstanding::parseModelPayload(validPayload(), &error);
        QVERIFY2(model.has_value(), qPrintable(error));
        bool applied = false;
        const auto merged = SearchQueryUnderstanding::merge(local, *model, &applied);
        QVERIFY(applied);
        QCOMPARE(merged.resultTarget, SearchResultTarget::Frames);
        QVERIFY(merged.lexicalTerms.contains(QStringLiteral("蓝色")));
        QVERIFY(merged.lexicalTerms.contains(QStringLiteral("牛仔")));
        QVERIFY(merged.interpretationLabels.contains(QStringLiteral("视觉语言模型辅助理解")));
    }

    void rejectsInvalidDatesAndUnknownTypes()
    {
        auto invalidDate = validPayload();
        invalidDate.insert(QStringLiteral("date"), QJsonObject{
            {QStringLiteral("start"), QStringLiteral("2026-99-99")},
            {QStringLiteral("end"), QStringLiteral("2026-01-01")},
            {QStringLiteral("matched_text"), QStringLiteral("某天")},
            {QStringLiteral("preferred_field"), QStringLiteral("captured")}
        });
        QVERIFY(!SearchQueryUnderstanding::parseModelPayload(invalidDate).has_value());

        auto unknownType = validPayload();
        unknownType.insert(QStringLiteral("asset_types"), QJsonArray{QStringLiteral("executable")});
        QVERIFY(!SearchQueryUnderstanding::parseModelPayload(unknownType).has_value());
    }

    void lowConfidenceNeverChangesLocalQuery()
    {
        auto payload = validPayload();
        payload.insert(QStringLiteral("confidence"), 0.2);
        const auto model = SearchQueryUnderstanding::parseModelPayload(payload);
        QVERIFY(model.has_value());
        NaturalLanguageQueryParser parser;
        const auto local = parser.parse(QStringLiteral("昨天的图片"), QDate(2026, 7, 14));
        bool applied = true;
        const auto merged = SearchQueryUnderstanding::merge(local, *model, &applied);
        QVERIFY(!applied);
        QCOMPARE(merged.resultTarget, local.resultTarget);
        QCOMPARE(merged.dateConstraint.startDate, local.dateConstraint.startDate);
        QCOMPARE(merged.semanticText, local.semanticText);
    }

    void frameRerankDropsHallucinatedAndDuplicateIds()
    {
        const QJsonObject payload{
            {QStringLiteral("version"), 1},
            {QStringLiteral("matches"), QJsonArray{
                QJsonObject{{QStringLiteral("candidate_id"), QStringLiteral("frame:a:1")}, {QStringLiteral("relevant"), true}, {QStringLiteral("score"), 0.92}, {QStringLiteral("reason"), QStringLiteral("蓝色牛仔裤清晰可见")}},
                QJsonObject{{QStringLiteral("candidate_id"), QStringLiteral("frame:invented:99")}, {QStringLiteral("relevant"), true}, {QStringLiteral("score"), 1.0}, {QStringLiteral("reason"), QStringLiteral("虚构")}},
                QJsonObject{{QStringLiteral("candidate_id"), QStringLiteral("frame:a:1")}, {QStringLiteral("relevant"), false}, {QStringLiteral("score"), 0.1}, {QStringLiteral("reason"), QStringLiteral("重复")}},
                QJsonObject{{QStringLiteral("candidate_id"), QStringLiteral("frame:b:2")}, {QStringLiteral("relevant"), false}, {QStringLiteral("score"), 0.25}, {QStringLiteral("reason"), QStringLiteral("只有蓝色上衣")}}
            }}
        };
        QString error;
        const auto scores = SearchQueryUnderstanding::parseFrameRerankPayload(
            payload,
            {QStringLiteral("frame:a:1"), QStringLiteral("frame:b:2")},
            &error);
        QVERIFY2(scores.has_value(), qPrintable(error));
        QCOMPARE(scores->size(), 2);
        QCOMPARE(scores->first().frameKey, QStringLiteral("frame:a:1"));
        QVERIFY(scores->first().relevant);
        QVERIFY(std::none_of(scores->cbegin(), scores->cend(), [](const auto &item) {
            return item.frameKey == QStringLiteral("frame:invented:99");
        }));
    }
};

QTEST_GUILESS_MAIN(SearchQueryUnderstandingTest)

#include "SearchQueryUnderstandingTest.moc"

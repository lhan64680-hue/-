#include "shared/VisualAnalysisMetadata.h"

#include <QtTest>

class VisualAnalysisMetadataTest : public QObject {
    Q_OBJECT

private slots:
    void entityFacts_roundTripPreservesSameEntityBindings()
    {
        VisionEntityFact shorts;
        shorts.category = QStringLiteral("clothing");
        shorts.label = QStringLiteral("短裤");
        shorts.colors = {QStringLiteral("红色")};
        shorts.materials = {QStringLiteral("牛仔")};
        shorts.attributes = {QStringLiteral("破洞")};

        VisionEntityFact shirt;
        shirt.category = QStringLiteral("clothing");
        shirt.label = QStringLiteral("衬衫");
        shirt.colors = {QStringLiteral("蓝色")};
        shirt.materials = {QStringLiteral("棉")};

        const auto restored = VisualAnalysisMetadata::entityFactsFromJson(
            VisualAnalysisMetadata::entityFactsToJson({shorts, shirt}));

        QCOMPARE(restored.size(), 2);
        QCOMPARE(restored.at(0).label, QStringLiteral("短裤"));
        QCOMPARE(restored.at(0).colors, QStringList{QStringLiteral("红色")});
        QCOMPARE(restored.at(0).materials, QStringList{QStringLiteral("牛仔")});
        QCOMPARE(restored.at(1).label, QStringLiteral("衬衫"));
        QCOMPARE(restored.at(1).colors, QStringList{QStringLiteral("蓝色")});
    }

    void plannedFrameNumbers_usesOnlyFixedInterval()
    {
        QCOMPARE(VisualAnalysisMetadata::fixedFrameInterval(AnalysisMode::EveryFrame, 99), 1);
        QCOMPARE(VisualAnalysisMetadata::fixedFrameInterval(AnalysisMode::Every10Frames, 3), 10);
        QCOMPARE(VisualAnalysisMetadata::fixedFrameInterval(AnalysisMode::CustomInterval, 7), 7);
        QCOMPARE(VisualAnalysisMetadata::plannedFrameNumbers(25, 10), QVector<int>({1, 11, 21}));
    }

    void incompletePlan_detectsMissingFailedAndLegacyFramesOnly()
    {
        FrameAnalysisRecord complete;
        complete.frameNumber = 1;
        complete.analysisState = FrameAnalysisState::Success;
        complete.factsComplete = true;
        complete.structuredProfileVersion = 2;

        FrameAnalysisRecord legacy = complete;
        legacy.frameNumber = 11;
        legacy.factsComplete = false;
        legacy.structuredProfileVersion = 1;

        FrameAnalysisRecord failed = complete;
        failed.frameNumber = 21;
        failed.analysisState = FrameAnalysisState::Failed;

        QCOMPARE(VisualAnalysisMetadata::incompletePlannedFrameNumbers(
                     35, 10, {complete, legacy, failed}, 2),
                 QVector<int>({11, 21, 31}));
    }
};

QTEST_GUILESS_MAIN(VisualAnalysisMetadataTest)

#include "VisualAnalysisMetadataTest.moc"

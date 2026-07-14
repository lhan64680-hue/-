#pragma once

#include "domain/Entities.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace VisualAnalysisMetadata {

QString entityFactsToJson(const QVector<VisionEntityFact> &facts);
QVector<VisionEntityFact> entityFactsFromJson(const QString &json);
QStringList entityFactSearchTerms(const QVector<VisionEntityFact> &facts);

int fixedFrameInterval(AnalysisMode mode, int configuredInterval);
QVector<int> plannedFrameNumbers(int sourceFrameCount, int frameInterval);
bool isFrameAnalysisComplete(const FrameAnalysisRecord &frame, int requiredProfileVersion);
QVector<int> incompletePlannedFrameNumbers(int sourceFrameCount,
                                           int frameInterval,
                                           const QVector<FrameAnalysisRecord> &frames,
                                           int requiredProfileVersion);

} // namespace VisualAnalysisMetadata

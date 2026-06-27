#pragma once

#include "domain/Entities.h"

#include <optional>

class VisionApiClient {
public:
    bool testConnection(const QString &baseUrl,
                        const QString &apiKey,
                        const QString &model,
                        int timeoutSec,
                        QString *errorMessage) const;

    std::optional<VisionFrameAnalysis> analyzeFrame(const QString &imagePath,
                                                    const QString &baseUrl,
                                                    const QString &apiKey,
                                                    const QString &model,
                                                    int timeoutSec,
                                                    QString *errorMessage) const;

    std::optional<VisionVideoSummary> summarizeVideo(const QString &fileName,
                                                     const QVector<FrameAnalysisRecord> &frames,
                                                     const QString &baseUrl,
                                                     const QString &apiKey,
                                                     const QString &model,
                                                     int timeoutSec,
                                                     QString *errorMessage) const;
};

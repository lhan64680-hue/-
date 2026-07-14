#pragma once

#include "domain/Entities.h"
#include "domain/SearchTypes.h"

#include <QDate>
#include <optional>

class VisionApiClient {
public:
    std::optional<ModelSearchUnderstanding> understandSearchQuery(
        const QString &queryText,
        const QDate &referenceDate,
        const QString &baseUrl,
        const QString &apiKey,
        const QString &model,
        int timeoutSec,
        QString *errorMessage,
        int *httpStatusCode = nullptr) const;

    std::optional<QVector<ModelFrameRerankScore>> rerankFrameCandidates(
        const QString &queryText,
        const QVector<FrameSearchHit> &candidates,
        const QString &baseUrl,
        const QString &apiKey,
        const QString &model,
        int timeoutSec,
        QString *errorMessage,
        int *httpStatusCode = nullptr) const;

    bool testConnection(const QString &baseUrl,
                        const QString &apiKey,
                        const QString &model,
                        int timeoutSec,
                        QString *errorMessage) const;

    std::optional<VisionFrameAnalysis> analyzeFrame(const QString &imagePath,
                                                    const QString &sourceFileName,
                                                    const QString &baseUrl,
                                                    const QString &apiKey,
                                                    const QString &model,
                                                    int timeoutSec,
                                                    QString *errorMessage,
                                                    int *httpStatusCode = nullptr) const;

    std::optional<QVector<MaterialDimensionAnalysis>> analyzeFrameDimensions(const QString &imagePath,
                                                                             const QString &sourceFileName,
                                                                             const QString &frameContext,
                                                                             const QStringList &dimensions,
                                                                             const QString &baseUrl,
                                                                             const QString &apiKey,
                                                                             const QString &model,
                                                                             int timeoutSec,
                                                                             QString *errorMessage,
                                                                             int *httpStatusCode = nullptr) const;

    std::optional<VisionVideoSummary> analyzeImage(const QString &imagePath,
                                                   const QString &fileName,
                                                   const QString &baseUrl,
                                                   const QString &apiKey,
                                                   const QString &model,
                                                   int timeoutSec,
                                                   QString *errorMessage,
                                                   int *httpStatusCode = nullptr) const;

    std::optional<VisionVideoSummary> summarizeText(const QString &fileName,
                                                    const QString &text,
                                                    const QString &baseUrl,
                                                    const QString &apiKey,
                                                    const QString &model,
                                                    int timeoutSec,
                                                    QString *errorMessage,
                                                    int *httpStatusCode = nullptr) const;

    std::optional<VisionVideoSummary> summarizeVideo(const QString &fileName,
                                                     const QVector<FrameAnalysisRecord> &frames,
                                                     const QString &baseUrl,
                                                     const QString &apiKey,
                                                     const QString &model,
                                                     int timeoutSec,
                                                     QString *errorMessage,
                                                     int *attemptCount = nullptr,
                                                     int *httpStatusCode = nullptr) const;

    std::optional<QVector<MaterialDimensionAnalysis>> analyzeDimensions(const QString &fileName,
                                                                        const QString &baseContext,
                                                                        const QStringList &dimensions,
                                                                        const QString &baseUrl,
                                                                        const QString &apiKey,
                                                                        const QString &model,
                                                                        int timeoutSec,
                                                                        QString *errorMessage,
                                                                        int *httpStatusCode = nullptr) const;
};

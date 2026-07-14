#pragma once

#include "domain/SearchTypes.h"

#include <QJsonObject>

#include <optional>

class SearchQueryUnderstanding {
public:
    static QJsonObject responseSchema();
    static QJsonObject frameRerankResponseSchema();
    static std::optional<ModelSearchUnderstanding> parseModelPayload(
        const QJsonObject &payload,
        QString *errorMessage = nullptr);
    static ParsedMaterialQuery merge(const ParsedMaterialQuery &localQuery,
                                     const ModelSearchUnderstanding &modelUnderstanding,
                                     bool *modelApplied = nullptr);
    static std::optional<QVector<ModelFrameRerankScore>> parseFrameRerankPayload(
        const QJsonObject &payload,
        const QStringList &allowedFrameKeys,
        QString *errorMessage = nullptr);
};

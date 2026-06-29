#pragma once

#include "domain/Entities.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace VisionResponseParser {

std::optional<QString> extractAssistantContent(const QByteArray &responseBody, QString *errorMessage);
std::optional<QJsonObject> parseAssistantJson(const QByteArray &responseBody, QString *errorMessage);
std::optional<VisionFrameAnalysis> normalizeFrameAnalysis(const QJsonObject &payload, QString *errorMessage);
std::optional<VisionVideoSummary> normalizeVideoSummary(const QJsonObject &payload, QString *errorMessage);
std::optional<VisionFrameAnalysis> fallbackFrameAnalysisFromContent(const QString &content, QString *errorMessage);
std::optional<VisionVideoSummary> fallbackVideoSummaryFromContent(const QString &content, QString *errorMessage);

}

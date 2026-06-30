#include "infrastructure/network/VisionApiClient.h"

#include "infrastructure/logging/Logger.h"
#include "infrastructure/network/VisionResponseParser.h"

#include <algorithm>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QtMath>

namespace {
struct HttpResult {
    bool success = false;
    int statusCode = 0;
    QByteArray body;
    QString errorMessage;
};

QString truncateForLog(QString text, int maxLength = 600)
{
    text = text.trimmed();
    if (text.size() <= maxLength) {
        return text;
    }
    return text.left(maxLength) + QStringLiteral("...");
}

QString extractServiceErrorMessage(const QByteArray &body)
{
    const auto plainText = QString::fromUtf8(body).trimmed();
    if (plainText.isEmpty()) {
        return {};
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return truncateForLog(plainText, 180);
    }

    const auto object = document.object();
    const auto errorValue = object.value(QStringLiteral("error"));
    if (errorValue.isObject()) {
        const auto errorObject = errorValue.toObject();
        const auto message = errorObject.value(QStringLiteral("message")).toString().trimmed();
        if (!message.isEmpty()) {
            return message;
        }
        const auto detail = errorObject.value(QStringLiteral("detail")).toString().trimmed();
        if (!detail.isEmpty()) {
            return detail;
        }
    }

    for (const auto &key : {QStringLiteral("message"), QStringLiteral("detail"), QStringLiteral("error_message")}) {
        const auto text = object.value(key).toString().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
    }

    return truncateForLog(plainText, 180);
}

QString httpStatusMessage(int statusCode, const QByteArray &body)
{
    QString base;
    if (statusCode == 401) {
        base = QStringLiteral("视觉接口鉴权失败（401）");
    } else if (statusCode == 413) {
        base = QStringLiteral("视觉接口请求内容过大（413）");
    } else if (statusCode == 429) {
        base = QStringLiteral("视觉接口触发限流（429）");
    } else {
        base = QStringLiteral("视觉接口返回异常状态码：%1").arg(statusCode);
    }

    const auto detail = extractServiceErrorMessage(body);
    if (!detail.isEmpty()) {
        base += QStringLiteral("，%1").arg(detail);
    }
    return base;
}

bool shouldRetrySummaryFailure(int statusCode, const QString &errorMessage)
{
    if (statusCode == 400 || statusCode == 413) {
        return true;
    }

    const auto normalizedError = errorMessage.toLower();
    return normalizedError.contains(QStringLiteral("context"))
        || normalizedError.contains(QStringLiteral("token"))
        || normalizedError.contains(QStringLiteral("too large"))
        || normalizedError.contains(QStringLiteral("length"));
}

QVector<FrameAnalysisRecord> sampleFramesForSummary(const QVector<FrameAnalysisRecord> &frames, int maxFrames)
{
    if (maxFrames <= 0 || frames.size() <= maxFrames) {
        return frames;
    }
    if (maxFrames == 1) {
        return QVector<FrameAnalysisRecord>{frames.first()};
    }

    QVector<int> indexes;
    indexes.reserve(maxFrames);
    for (int index = 0; index < maxFrames; ++index) {
        const auto mappedIndex = qRound((static_cast<double>(index) * (frames.size() - 1)) / (maxFrames - 1));
        if (!indexes.contains(mappedIndex)) {
            indexes.append(mappedIndex);
        }
    }
    std::sort(indexes.begin(), indexes.end());

    QVector<FrameAnalysisRecord> sampled;
    sampled.reserve(indexes.size());
    for (const auto index : indexes) {
        sampled.append(frames.at(index));
    }
    return sampled;
}

QString normalizeEndpoint(QString baseUrl)
{
    auto url = baseUrl.trimmed();
    if (url.isEmpty()) {
        return {};
    }
    if (!url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        && !url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        url = QStringLiteral("http://") + url;
    }
    if (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    if (url.endsWith(QStringLiteral("/chat/completions"))) {
        return url;
    }
    if (url.endsWith(QStringLiteral("/v1"))) {
        return url + QStringLiteral("/chat/completions");
    }

    const QUrl parsedUrl(url);
    if (parsedUrl.path().isEmpty() || parsedUrl.path() == QStringLiteral("/")) {
        return url + QStringLiteral("/v1/chat/completions");
    }
    return url + QStringLiteral("/chat/completions");
}

HttpResult postJson(const QString &endpoint,
                    const QString &apiKey,
                    const QJsonObject &payload,
                    int timeoutSec)
{
    HttpResult result;

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!apiKey.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey.trimmed()).toUtf8());
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    auto *reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(qMax(5, timeoutSec) * 1000);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
        result.errorMessage = QStringLiteral("请求超时");
        reply->deleteLater();
        return result;
    }

    result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError && result.statusCode == 0) {
        result.errorMessage = reply->errorString();
        reply->deleteLater();
        return result;
    }

    if (result.statusCode != 200) {
        result.errorMessage = httpStatusMessage(result.statusCode, result.body);
        Logger::warn(QStringLiteral("视觉接口请求失败：status=%1 endpoint=%2 body=%3")
                         .arg(result.statusCode)
                         .arg(endpoint, truncateForLog(QString::fromUtf8(result.body))));
        reply->deleteLater();
        return result;
    }

    result.success = true;
    reply->deleteLater();
    return result;
}

QJsonObject makeChatPayload(const QString &model, const QJsonArray &content, int maxTokens)
{
    return QJsonObject{
        {QStringLiteral("model"), model},
        {QStringLiteral("max_tokens"), maxTokens},
        {QStringLiteral("response_format"), QJsonObject{{QStringLiteral("type"), QStringLiteral("json_object")}}},
        {QStringLiteral("messages"), QJsonArray{
            QJsonObject{
                {QStringLiteral("role"), QStringLiteral("user")},
                {QStringLiteral("content"), content}
            }
        }}
    };
}

bool isResponseFormatRejected(const HttpResult &result)
{
    if (result.statusCode != 400) {
        return false;
    }
    const auto bodyText = QString::fromUtf8(result.body);
    return bodyText.contains(QStringLiteral("response_format"), Qt::CaseInsensitive);
}

HttpResult postChatPayload(const QString &endpoint,
                           const QString &apiKey,
                           QJsonObject payload,
                           int timeoutSec)
{
    auto result = postJson(endpoint, apiKey, payload, timeoutSec);
    if (!result.success && isResponseFormatRejected(result)) {
        payload.insert(QStringLiteral("response_format"),
                       QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}});
        result = postJson(endpoint, apiKey, payload, timeoutSec);
    }
    return result;
}

std::optional<QJsonObject> repairAssistantPayload(const QByteArray &responseBody,
                                                  const QString &endpoint,
                                                  const QString &apiKey,
                                                  const QString &model,
                                                  int timeoutSec,
                                                  const QString &schema,
                                                  const QString &failureReason,
                                                  int maxTokens,
                                                  QString *errorMessage)
{
    QString contentError;
    const auto originalContent = VisionResponseParser::extractAssistantContent(responseBody, &contentError);
    if (!originalContent.has_value()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1；自动修复失败：无法读取原始返回内容（%2）")
                                .arg(failureReason, contentError);
        }
        return std::nullopt;
    }

    auto boundedContent = originalContent->trimmed();
    if (boundedContent.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1；自动修复失败：原始返回内容为空").arg(failureReason);
        }
        return std::nullopt;
    }
    if (boundedContent.size() > 12000) {
        boundedContent = boundedContent.left(12000);
    }

    const QJsonArray content = {
        QJsonObject{
            {QStringLiteral("type"), QStringLiteral("text")},
            {QStringLiteral("text"),
             QStringLiteral("你是视觉解析结果格式修复器。请把原始返回内容转换为严格 JSON。"
                            "只能使用原始内容中已经出现的信息，不要新增事实，不要解释，不要 Markdown。"
                            "目标 JSON 结构为：%1\n\n"
                            "上一次失败原因：%2\n\n"
                            "原始返回内容：\n%3")
                 .arg(schema, failureReason, boundedContent)}
        }
    };

    const auto result = postChatPayload(endpoint, apiKey, makeChatPayload(model, content, maxTokens), timeoutSec);
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1；自动修复失败：%2").arg(failureReason, result.errorMessage);
        }
        return std::nullopt;
    }

    QString repairParseError;
    auto repairedPayload = VisionResponseParser::parseAssistantJson(result.body, &repairParseError);
    if (!repairedPayload.has_value()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1；自动修复失败：%2").arg(failureReason, repairParseError);
        }
        return std::nullopt;
    }
    return repairedPayload;
}

QString combineFallbackError(const QString &existingError, const QString &fallbackError)
{
    if (existingError.trimmed().isEmpty()) {
        return QStringLiteral("纯文本兜底失败：%1").arg(fallbackError);
    }
    return QStringLiteral("%1；纯文本兜底失败：%2").arg(existingError, fallbackError);
}

std::optional<VisionFrameAnalysis> fallbackFrameAnalysisFromResponse(const QByteArray &responseBody,
                                                                    QString *errorMessage)
{
    const auto existingError = errorMessage ? *errorMessage : QString();

    QString contentError;
    const auto content = VisionResponseParser::extractAssistantContent(responseBody, &contentError);
    if (!content.has_value()) {
        if (errorMessage) {
            *errorMessage = combineFallbackError(existingError, contentError);
        }
        return std::nullopt;
    }

    QString fallbackError;
    auto analysis = VisionResponseParser::fallbackFrameAnalysisFromContent(*content, &fallbackError);
    if (!analysis.has_value()) {
        if (errorMessage) {
            *errorMessage = combineFallbackError(existingError, fallbackError);
        }
        return std::nullopt;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return analysis;
}

std::optional<VisionVideoSummary> fallbackVideoSummaryFromResponse(const QByteArray &responseBody,
                                                                  QString *errorMessage)
{
    const auto existingError = errorMessage ? *errorMessage : QString();

    QString contentError;
    const auto content = VisionResponseParser::extractAssistantContent(responseBody, &contentError);
    if (!content.has_value()) {
        if (errorMessage) {
            *errorMessage = combineFallbackError(existingError, contentError);
        }
        return std::nullopt;
    }

    QString fallbackError;
    auto summary = VisionResponseParser::fallbackVideoSummaryFromContent(*content, &fallbackError);
    if (!summary.has_value()) {
        if (errorMessage) {
            *errorMessage = combineFallbackError(existingError, fallbackError);
        }
        return std::nullopt;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return summary;
}
}

bool VisionApiClient::testConnection(const QString &baseUrl,
                                     const QString &apiKey,
                                     const QString &model,
                                     int timeoutSec,
                                     QString *errorMessage) const
{
    const auto endpoint = normalizeEndpoint(baseUrl);
    if (endpoint.trimmed().isEmpty() || apiKey.trimmed().isEmpty() || model.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先完整填写 Base URL、API Key 和模型名");
        }
        return false;
    }

    const QJsonArray content = {
        QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                    {QStringLiteral("text"), QStringLiteral("请只返回 JSON：{\"status\":\"ok\"}")}}
    };
    const auto result = postChatPayload(endpoint, apiKey, makeChatPayload(model, content, 32), timeoutSec);
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return false;
    }

    auto payload = VisionResponseParser::parseAssistantJson(result.body, errorMessage);
    return payload.has_value() && payload->value(QStringLiteral("status")).toString().trimmed() == QStringLiteral("ok");
}

std::optional<VisionFrameAnalysis> VisionApiClient::analyzeFrame(const QString &imagePath,
                                                                 const QString &baseUrl,
                                                                 const QString &apiKey,
                                                                 const QString &model,
                                                                 int timeoutSec,
                                                                 QString *errorMessage,
                                                                 int *httpStatusCode) const
{
    QFile imageFile(imagePath);
    if (!imageFile.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取解析帧：%1").arg(imagePath);
        }
        return std::nullopt;
    }
    const auto base64 = QString::fromUtf8(imageFile.readAll().toBase64());
    imageFile.close();

    const auto endpoint = normalizeEndpoint(baseUrl);
    const QJsonArray content = {
        QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                    {QStringLiteral("text"),
                     QStringLiteral("请分析这张视频帧图片，只返回 JSON，格式为 "
                                    "{\"caption\":\"\",\"tags\":[],\"objects\":[],\"actions\":\"\",\"setting\":\"\"}。"
                                    "caption 用一句中文概括画面，tags/objects 为中文关键词数组。")}},
        QJsonObject{{QStringLiteral("type"), QStringLiteral("image_url")},
                    {QStringLiteral("image_url"),
                     QJsonObject{{QStringLiteral("url"), QStringLiteral("data:image/jpeg;base64,%1").arg(base64)}}}}
    };

    const auto result = postChatPayload(endpoint, apiKey, makeChatPayload(model, content, 300), timeoutSec);
    if (httpStatusCode) {
        *httpStatusCode = result.statusCode;
    }
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return std::nullopt;
    }

    const auto schema = QStringLiteral("{\"caption\":\"\",\"tags\":[],\"objects\":[],\"actions\":\"\",\"setting\":\"\"}");
    QString parseError;
    auto payload = VisionResponseParser::parseAssistantJson(result.body, &parseError);
    auto usedRepair = false;
    if (!payload.has_value()) {
        payload = repairAssistantPayload(result.body,
                                         endpoint,
                                         apiKey,
                                         model,
                                         timeoutSec,
                                         schema,
                                         parseError,
                                         350,
                                         errorMessage);
        usedRepair = true;
        if (!payload.has_value()) {
            return fallbackFrameAnalysisFromResponse(result.body, errorMessage);
        }
    }

    QString normalizeError;
    auto analysis = VisionResponseParser::normalizeFrameAnalysis(*payload, &normalizeError);
    if (!analysis.has_value() && !usedRepair) {
        payload = repairAssistantPayload(result.body,
                                         endpoint,
                                         apiKey,
                                         model,
                                         timeoutSec,
                                         schema,
                                         normalizeError,
                                         350,
                                         errorMessage);
        usedRepair = true;
        if (payload.has_value()) {
            analysis = VisionResponseParser::normalizeFrameAnalysis(*payload, &normalizeError);
        } else {
            return fallbackFrameAnalysisFromResponse(result.body, errorMessage);
        }
    }
    if (!analysis.has_value()) {
        auto fallback = fallbackFrameAnalysisFromResponse(result.body, errorMessage);
        if (fallback.has_value()) {
            return fallback;
        }
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = normalizeError;
        }
    }
    return analysis;
}

std::optional<VisionVideoSummary> VisionApiClient::summarizeVideo(const QString &fileName,
                                                                  const QVector<FrameAnalysisRecord> &frames,
                                                                  const QString &baseUrl,
                                                                  const QString &apiKey,
                                                                  const QString &model,
                                                                  int timeoutSec,
                                                                  QString *errorMessage,
                                                                  int *attemptCount,
                                                                  int *httpStatusCode) const
{
    const auto endpoint = normalizeEndpoint(baseUrl);
    if (attemptCount) {
        *attemptCount = 0;
    }
    if (httpStatusCode) {
        *httpStatusCode = 0;
    }

    auto attemptSummary = [&](const QVector<FrameAnalysisRecord> &sourceFrames,
                              int maxTokens,
                              QString *attemptError,
                              int *attemptStatusCode) -> std::optional<VisionVideoSummary> {
        QStringList frameLines;
        for (const auto &frame : sourceFrames) {
            QStringList parts;
            if (!frame.caption.trimmed().isEmpty()) {
                parts.append(QStringLiteral("描述：%1").arg(frame.caption.trimmed()));
            }
            if (!frame.tags.isEmpty()) {
                parts.append(QStringLiteral("标签：%1").arg(frame.tags.join(QStringLiteral("、"))));
            }
            if (!frame.objects.isEmpty()) {
                parts.append(QStringLiteral("对象：%1").arg(frame.objects.join(QStringLiteral("、"))));
            }
            if (!frame.actions.trimmed().isEmpty()) {
                parts.append(QStringLiteral("动作：%1").arg(frame.actions.trimmed()));
            }
            if (!frame.setting.trimmed().isEmpty()) {
                parts.append(QStringLiteral("场景：%1").arg(frame.setting.trimmed()));
            }
            if (!parts.isEmpty()) {
                frameLines.append(QStringLiteral("第 %1 帧：%2").arg(frame.frameNumber).arg(parts.join(QStringLiteral("；"))));
            }
        }

        if (frameLines.isEmpty()) {
            if (attemptError) {
                *attemptError = QStringLiteral("没有可用于汇总的视频帧描述");
            }
            if (attemptStatusCode) {
                *attemptStatusCode = 0;
            }
            return std::nullopt;
        }

        const QJsonArray content = {
            QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                        {QStringLiteral("text"),
                         QStringLiteral("下面是视频《%1》的抽帧解析结果，请汇总成中文 JSON，格式为 "
                                        "{\"summary\":\"\",\"keywords\":[],\"scenes\":[]}。"
                                        "summary 需要比普通标题摘要更详细，用 4-6 句说明视频的主要主体、场景环境、动作变化、"
                                        "可见物体、情绪/氛围和镜头内容变化；如果抽帧信息有限，也要基于已有帧明确说明可确认内容。"
                                        "keywords 返回 8-16 个适合搜索的中文关键词，scenes 返回场景/地点/环境类中文关键词数组。"
                                        "只返回 JSON，不要加入 Markdown。\n\n%2")
                             .arg(fileName, frameLines.join(QStringLiteral("\n"))) }}
        };

        const auto result = postChatPayload(endpoint, apiKey, makeChatPayload(model, content, maxTokens), timeoutSec);
        if (attemptStatusCode) {
            *attemptStatusCode = result.statusCode;
        }
        if (!result.success) {
            if (attemptError) {
                *attemptError = result.errorMessage;
            }
            return std::nullopt;
        }

        const auto schema = QStringLiteral("{\"summary\":\"\",\"keywords\":[],\"scenes\":[]}");
        QString parseError;
        auto payload = VisionResponseParser::parseAssistantJson(result.body, &parseError);
        auto usedRepair = false;
        if (!payload.has_value()) {
            payload = repairAssistantPayload(result.body,
                                             endpoint,
                                             apiKey,
                                             model,
                                             timeoutSec,
                                             schema,
                                             parseError,
                                             maxTokens,
                                             attemptError);
            usedRepair = true;
            if (!payload.has_value()) {
                return fallbackVideoSummaryFromResponse(result.body, attemptError);
            }
        }

        QString normalizeError;
        auto summary = VisionResponseParser::normalizeVideoSummary(*payload, &normalizeError);
        if (!summary.has_value() && !usedRepair) {
            payload = repairAssistantPayload(result.body,
                                             endpoint,
                                             apiKey,
                                             model,
                                             timeoutSec,
                                             schema,
                                             normalizeError,
                                             maxTokens,
                                             attemptError);
            usedRepair = true;
            if (payload.has_value()) {
                summary = VisionResponseParser::normalizeVideoSummary(*payload, &normalizeError);
            } else {
                return fallbackVideoSummaryFromResponse(result.body, attemptError);
            }
        }
        if (!summary.has_value()) {
            auto fallback = fallbackVideoSummaryFromResponse(result.body, attemptError);
            if (fallback.has_value()) {
                return fallback;
            }
            if (attemptError && attemptError->isEmpty()) {
                *attemptError = normalizeError;
            }
        }
        return summary;
    };

    QVector<QPair<int, int>> plans;
    auto appendPlan = [&](int maxFrames, int maxTokens) {
        for (const auto &plan : plans) {
            if (plan.first == maxFrames && plan.second == maxTokens) {
                return;
            }
        }
        plans.append({maxFrames, maxTokens});
    };

    appendPlan(0, 900);
    appendPlan(qMin(18, frames.size()), 700);
    appendPlan(qMin(12, frames.size()), 550);

    QString lastError = QStringLiteral("视频内容汇总失败");
    int lastStatusCode = 0;
    int usedAttempts = 0;
    for (int index = 0; index < plans.size(); ++index) {
        const auto &plan = plans.at(index);
        ++usedAttempts;
        const auto sampledFrames = sampleFramesForSummary(frames, plan.first);
        const auto summary = attemptSummary(sampledFrames, plan.second, &lastError, &lastStatusCode);
        if (summary.has_value()) {
            if (attemptCount) {
                *attemptCount = usedAttempts;
            }
            if (httpStatusCode) {
                *httpStatusCode = lastStatusCode;
            }
            return summary;
        }

        if (index >= plans.size() - 1 || !shouldRetrySummaryFailure(lastStatusCode, lastError)) {
            break;
        }
    }

    if (attemptCount) {
        *attemptCount = usedAttempts;
    }
    if (httpStatusCode) {
        *httpStatusCode = lastStatusCode;
    }
    if (errorMessage) {
        *errorMessage = lastError;
    }
    return std::nullopt;
}

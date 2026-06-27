#include "infrastructure/network/VisionApiClient.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace {
struct HttpResult {
    bool success = false;
    int statusCode = 0;
    QByteArray body;
    QString errorMessage;
};

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

QString extractMessageContent(const QJsonValue &contentValue)
{
    if (contentValue.isString()) {
        return contentValue.toString().trimmed();
    }
    if (!contentValue.isArray()) {
        return {};
    }

    QStringList parts;
    const auto items = contentValue.toArray();
    for (const auto &itemValue : items) {
        const auto item = itemValue.toObject();
        if (item.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
            parts.append(item.value(QStringLiteral("text")).toString());
        }
    }
    return parts.join(QStringLiteral("\n")).trimmed();
}

bool parsesJsonObject(const QString &text)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject();
}

QStringList extractJsonObjectCandidates(const QString &text)
{
    QStringList candidates;
    auto start = -1;
    auto depth = 0;
    auto inString = false;
    auto escaped = false;

    for (auto index = 0; index < text.size(); ++index) {
        const auto ch = text.at(index);
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == QLatin1Char('\\')) {
                escaped = true;
            } else if (ch == QLatin1Char('"')) {
                inString = false;
            }
            continue;
        }

        if (ch == QLatin1Char('"')) {
            inString = true;
            continue;
        }
        if (ch == QLatin1Char('{')) {
            if (depth == 0) {
                start = index;
            }
            ++depth;
            continue;
        }
        if (ch == QLatin1Char('}') && depth > 0) {
            --depth;
            if (depth == 0 && start >= 0) {
                candidates.append(text.mid(start, index - start + 1));
                start = -1;
            }
        }
    }
    return candidates;
}

QString stripReasoningBlocks(QString text)
{
    static const QRegularExpression thinkBlock(
        QStringLiteral("<think\\b[^>]*>[\\s\\S]*?</think>"),
        QRegularExpression::CaseInsensitiveOption);
    return text.remove(thinkBlock).trimmed();
}

QString extractJsonBlock(QString text)
{
    text = stripReasoningBlocks(text);

    QStringList sources;
    static const QRegularExpression fencedBlock(
        QStringLiteral("```(?:json)?\\s*([\\s\\S]*?)```"),
        QRegularExpression::CaseInsensitiveOption);
    auto matchIterator = fencedBlock.globalMatch(text);
    while (matchIterator.hasNext()) {
        sources.append(matchIterator.next().captured(1).trimmed());
    }
    sources.append(text);

    for (const auto &source : sources) {
        const auto trimmed = source.trimmed();
        if (parsesJsonObject(trimmed)) {
            return trimmed;
        }

        const auto candidates = extractJsonObjectCandidates(trimmed);
        for (auto index = candidates.size() - 1; index >= 0; --index) {
            const auto candidate = candidates.at(index).trimmed();
            if (parsesJsonObject(candidate)) {
                return candidate;
            }
        }
    }
    return text;
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
        if (result.statusCode == 401) {
            result.errorMessage = QStringLiteral("视觉接口鉴权失败（401）");
        } else if (result.statusCode == 429) {
            result.errorMessage = QStringLiteral("视觉接口触发限流（429）");
        } else {
            result.errorMessage = QStringLiteral("视觉接口返回异常状态码：%1").arg(result.statusCode);
        }
        reply->deleteLater();
        return result;
    }

    result.success = true;
    reply->deleteLater();
    return result;
}

std::optional<QJsonObject> parseAssistantJson(const QByteArray &responseBody, QString *errorMessage)
{
    QJsonParseError responseParseError;
    const auto responseDocument = QJsonDocument::fromJson(responseBody, &responseParseError);
    if (responseParseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉接口返回非 JSON 响应");
        }
        return std::nullopt;
    }

    const auto root = responseDocument.object();
    const auto choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉接口没有返回可解析结果");
        }
        return std::nullopt;
    }

    const auto message = choices.first().toObject().value(QStringLiteral("message")).toObject();
    const auto contentText = extractMessageContent(message.value(QStringLiteral("content")));
    const auto jsonText = extractJsonBlock(contentText);

    QJsonParseError contentParseError;
    const auto contentDocument = QJsonDocument::fromJson(jsonText.toUtf8(), &contentParseError);
    if (contentParseError.error != QJsonParseError::NoError || !contentDocument.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉接口返回内容不是有效 JSON");
        }
        return std::nullopt;
    }
    return contentDocument.object();
}

QStringList jsonStringList(const QJsonValue &value)
{
    QStringList items;
    if (value.isArray()) {
        const auto array = value.toArray();
        for (const auto &entry : array) {
            const auto text = entry.toString().trimmed();
            if (!text.isEmpty()) {
                items.append(text);
            }
        }
        return items;
    }

    const auto single = value.toString().trimmed();
    if (!single.isEmpty()) {
        items.append(single);
    }
    return items;
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

    auto payload = parseAssistantJson(result.body, errorMessage);
    return payload.has_value() && payload->value(QStringLiteral("status")).toString().trimmed() == QStringLiteral("ok");
}

std::optional<VisionFrameAnalysis> VisionApiClient::analyzeFrame(const QString &imagePath,
                                                                 const QString &baseUrl,
                                                                 const QString &apiKey,
                                                                 const QString &model,
                                                                 int timeoutSec,
                                                                 QString *errorMessage) const
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
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return std::nullopt;
    }

    auto payload = parseAssistantJson(result.body, errorMessage);
    if (!payload.has_value()) {
        return std::nullopt;
    }

    VisionFrameAnalysis analysis;
    analysis.caption = payload->value(QStringLiteral("caption")).toString().trimmed();
    analysis.tags = jsonStringList(payload->value(QStringLiteral("tags")));
    analysis.objects = jsonStringList(payload->value(QStringLiteral("objects")));
    analysis.actions = payload->value(QStringLiteral("actions")).toString().trimmed();
    analysis.setting = payload->value(QStringLiteral("setting")).toString().trimmed();
    return analysis;
}

std::optional<VisionVideoSummary> VisionApiClient::summarizeVideo(const QString &fileName,
                                                                  const QVector<FrameAnalysisRecord> &frames,
                                                                  const QString &baseUrl,
                                                                  const QString &apiKey,
                                                                  const QString &model,
                                                                  int timeoutSec,
                                                                  QString *errorMessage) const
{
    QStringList frameLines;
    for (const auto &frame : frames) {
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
        if (errorMessage) {
            *errorMessage = QStringLiteral("没有可用于汇总的视频帧描述");
        }
        return std::nullopt;
    }

    const auto endpoint = normalizeEndpoint(baseUrl);
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

    const auto result = postChatPayload(endpoint, apiKey, makeChatPayload(model, content, 900), timeoutSec);
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return std::nullopt;
    }

    auto payload = parseAssistantJson(result.body, errorMessage);
    if (!payload.has_value()) {
        return std::nullopt;
    }

    VisionVideoSummary summary;
    summary.summary = payload->value(QStringLiteral("summary")).toString().trimmed();
    summary.keywords = jsonStringList(payload->value(QStringLiteral("keywords")));
    summary.scenes = jsonStringList(payload->value(QStringLiteral("scenes")));
    return summary;
}

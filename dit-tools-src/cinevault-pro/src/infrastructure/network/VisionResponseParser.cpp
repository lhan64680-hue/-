#include "infrastructure/network/VisionResponseParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace {
void appendUnique(QStringList *items, const QString &text)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty() || items->contains(trimmed)) {
        return;
    }
    items->append(trimmed);
}

QJsonValue firstValue(const QJsonObject &object, const QStringList &keys)
{
    for (const auto &key : keys) {
        if (object.contains(key)) {
            return object.value(key);
        }
    }

    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        for (const auto &key : keys) {
            if (it.key().compare(key, Qt::CaseInsensitive) == 0) {
                return it.value();
            }
        }
    }
    return {};
}

QStringList textListFromValue(const QJsonValue &value);

QString textFromScalarOrObject(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (value.isDouble() || value.isBool()) {
        return value.toVariant().toString().trimmed();
    }
    if (!value.isObject()) {
        return {};
    }

    const auto object = value.toObject();
    return textFromScalarOrObject(firstValue(object, {
        QStringLiteral("text"),
        QStringLiteral("value"),
        QStringLiteral("label"),
        QStringLiteral("name"),
        QStringLiteral("description"),
        QStringLiteral("caption"),
        QStringLiteral("summary")
    }));
}

QStringList splitListText(const QString &text)
{
    QStringList items;
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return items;
    }

    if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isArray()) {
            return textListFromValue(document.array());
        }
    }

    static const QRegularExpression separator(QStringLiteral("[,，;；、|/\\n\\r]+"));
    const auto parts = trimmed.split(separator, Qt::SkipEmptyParts);
    if (parts.size() <= 1) {
        appendUnique(&items, trimmed);
        return items;
    }

    for (const auto &part : parts) {
        appendUnique(&items, part);
    }
    return items;
}

QStringList textListFromValue(const QJsonValue &value)
{
    QStringList items;
    if (value.isArray()) {
        const auto array = value.toArray();
        for (const auto &entry : array) {
            const auto nestedItems = textListFromValue(entry);
            for (const auto &item : nestedItems) {
                appendUnique(&items, item);
            }
        }
        return items;
    }

    const auto text = textFromScalarOrObject(value);
    if (text.isEmpty()) {
        return items;
    }

    const auto splitItems = splitListText(text);
    for (const auto &item : splitItems) {
        appendUnique(&items, item);
    }
    return items;
}

QString textFromValue(const QJsonValue &value)
{
    if (value.isArray()) {
        return textListFromValue(value).join(QStringLiteral("；"));
    }
    return textFromScalarOrObject(value);
}

QString firstText(const QJsonObject &payload, const QStringList &keys)
{
    return textFromValue(firstValue(payload, keys));
}

QStringList firstTextList(const QJsonObject &payload, const QStringList &keys)
{
    return textListFromValue(firstValue(payload, keys));
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

QString fallbackText(QString text)
{
    text = stripReasoningBlocks(text).trimmed();
    if (text.size() > 4000) {
        text = text.left(4000).trimmed();
    }
    return text;
}
}

std::optional<QString> VisionResponseParser::extractAssistantContent(const QByteArray &responseBody,
                                                                     QString *errorMessage)
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
    return extractMessageContent(message.value(QStringLiteral("content")));
}

std::optional<QJsonObject> VisionResponseParser::parseAssistantJson(const QByteArray &responseBody,
                                                                    QString *errorMessage)
{
    const auto contentText = extractAssistantContent(responseBody, errorMessage);
    if (!contentText.has_value()) {
        return std::nullopt;
    }

    const auto jsonText = extractJsonBlock(*contentText);

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

std::optional<VisionFrameAnalysis> VisionResponseParser::normalizeFrameAnalysis(const QJsonObject &payload,
                                                                                QString *errorMessage)
{
    VisionFrameAnalysis analysis;
    analysis.caption = firstText(payload, {
        QStringLiteral("caption"),
        QStringLiteral("description"),
        QStringLiteral("desc"),
        QStringLiteral("summary"),
        QStringLiteral("title"),
        QStringLiteral("画面描述"),
        QStringLiteral("描述")
    });
    analysis.tags = firstTextList(payload, {
        QStringLiteral("tags"),
        QStringLiteral("tag"),
        QStringLiteral("keywords"),
        QStringLiteral("keyword"),
        QStringLiteral("labels"),
        QStringLiteral("label"),
        QStringLiteral("标签"),
        QStringLiteral("关键词")
    });
    analysis.objects = firstTextList(payload, {
        QStringLiteral("objects"),
        QStringLiteral("object"),
        QStringLiteral("visible_objects"),
        QStringLiteral("subjects"),
        QStringLiteral("subject"),
        QStringLiteral("items"),
        QStringLiteral("物体"),
        QStringLiteral("对象"),
        QStringLiteral("主体")
    });
    analysis.actions = firstText(payload, {
        QStringLiteral("actions"),
        QStringLiteral("action"),
        QStringLiteral("motion"),
        QStringLiteral("activity"),
        QStringLiteral("动作"),
        QStringLiteral("行为")
    });
    analysis.setting = firstText(payload, {
        QStringLiteral("setting"),
        QStringLiteral("scene"),
        QStringLiteral("scenes"),
        QStringLiteral("environment"),
        QStringLiteral("location"),
        QStringLiteral("place"),
        QStringLiteral("场景"),
        QStringLiteral("环境"),
        QStringLiteral("地点")
    });

    if (analysis.caption.isEmpty()
        && analysis.tags.isEmpty()
        && analysis.objects.isEmpty()
        && analysis.actions.isEmpty()
        && analysis.setting.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉接口返回帧解析字段为空");
        }
        return std::nullopt;
    }
    return analysis;
}

std::optional<VisionVideoSummary> VisionResponseParser::normalizeVideoSummary(const QJsonObject &payload,
                                                                              QString *errorMessage)
{
    VisionVideoSummary summary;
    summary.summary = firstText(payload, {
        QStringLiteral("summary"),
        QStringLiteral("overview"),
        QStringLiteral("description"),
        QStringLiteral("caption"),
        QStringLiteral("title"),
        QStringLiteral("摘要"),
        QStringLiteral("概述"),
        QStringLiteral("描述")
    });
    summary.keywords = firstTextList(payload, {
        QStringLiteral("keywords"),
        QStringLiteral("keyword"),
        QStringLiteral("tags"),
        QStringLiteral("tag"),
        QStringLiteral("labels"),
        QStringLiteral("label"),
        QStringLiteral("关键词"),
        QStringLiteral("标签")
    });
    summary.scenes = firstTextList(payload, {
        QStringLiteral("scenes"),
        QStringLiteral("scene"),
        QStringLiteral("settings"),
        QStringLiteral("setting"),
        QStringLiteral("locations"),
        QStringLiteral("location"),
        QStringLiteral("places"),
        QStringLiteral("place"),
        QStringLiteral("场景"),
        QStringLiteral("环境"),
        QStringLiteral("地点")
    });

    if (summary.summary.isEmpty() && summary.keywords.isEmpty() && summary.scenes.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉接口返回视频汇总字段为空");
        }
        return std::nullopt;
    }
    return summary;
}

std::optional<VisionFrameAnalysis> VisionResponseParser::fallbackFrameAnalysisFromContent(const QString &content,
                                                                                          QString *errorMessage)
{
    const auto text = fallbackText(content);
    if (text.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉接口原始返回内容为空，无法生成帧解析兜底文本");
        }
        return std::nullopt;
    }

    VisionFrameAnalysis analysis;
    analysis.caption = text;
    return analysis;
}

std::optional<VisionVideoSummary> VisionResponseParser::fallbackVideoSummaryFromContent(const QString &content,
                                                                                        QString *errorMessage)
{
    const auto text = fallbackText(content);
    if (text.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视觉接口原始返回内容为空，无法生成视频汇总兜底文本");
        }
        return std::nullopt;
    }

    VisionVideoSummary summary;
    summary.summary = text;
    return summary;
}

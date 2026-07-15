#include "core/search/SearchQueryUnderstanding.h"

#include <QDate>
#include <QJsonArray>
#include <QSet>

#include <algorithm>

namespace {
constexpr int kMaxTextLength = 240;
constexpr int kMaxTermCount = 24;
constexpr int kMaxEntityCount = 8;
constexpr double kMinimumConfidence = 0.55;

QString boundedText(const QJsonValue &value, int maxLength = kMaxTextLength)
{
    if (!value.isString()) {
        return {};
    }
    auto text = value.toString().simplified();
    if (text.size() > maxLength) {
        text = text.left(maxLength).trimmed();
    }
    return text;
}

QStringList uniqueTerms(const QStringList &values, int limit = kMaxTermCount)
{
    QStringList result;
    QSet<QString> seen;
    for (auto value : values) {
        value = value.simplified();
        if (value.isEmpty()) {
            continue;
        }
        if (value.size() > 48) {
            value = value.left(48).trimmed();
        }
        const auto key = value.toCaseFolded();
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        result.append(value);
        if (result.size() >= limit) {
            break;
        }
    }
    return result;
}

QStringList stringArray(const QJsonValue &value, int limit = kMaxTermCount)
{
    if (!value.isArray()) {
        return {};
    }
    QStringList values;
    const auto array = value.toArray();
    for (const auto &item : array) {
        if (item.isString()) {
            values.append(item.toString());
        }
        if (values.size() >= limit) {
            break;
        }
    }
    return uniqueTerms(values, limit);
}

int assetTypeFromName(const QString &name)
{
    const auto key = name.trimmed().toLower();
    if (key == QStringLiteral("video")) return static_cast<int>(AssetType::Video);
    if (key == QStringLiteral("audio")) return static_cast<int>(AssetType::Audio);
    if (key == QStringLiteral("image")) return static_cast<int>(AssetType::Image);
    if (key == QStringLiteral("document")) return static_cast<int>(AssetType::Document);
    if (key == QStringLiteral("subtitle")) return static_cast<int>(AssetType::Subtitle);
    if (key == QStringLiteral("archive")) return static_cast<int>(AssetType::Archive);
    if (key == QStringLiteral("project_file")) return static_cast<int>(AssetType::ProjectFile);
    return -1;
}

SearchDateField dateFieldFromName(const QString &name)
{
    const auto key = name.trimmed().toLower();
    if (key == QStringLiteral("captured")) return SearchDateField::CapturedTime;
    if (key == QStringLiteral("folder")) return SearchDateField::FolderDate;
    if (key == QStringLiteral("modified")) return SearchDateField::FileModifiedTime;
    return SearchDateField::Any;
}

bool hasExplicitAssetTarget(const ParsedMaterialQuery &query)
{
    if (!query.assetTypeFilters.isEmpty()) {
        return true;
    }
    const auto text = query.originalText;
    return text.contains(QStringLiteral("素材"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("文件"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("视频"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("图片"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("照片"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("音频"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("文档"), Qt::CaseInsensitive);
}

QString targetLabel(SearchResultTarget target, bool folderByAssetCriteria)
{
    if (target == SearchResultTarget::Folders) {
        return folderByAssetCriteria
            ? QStringLiteral("目标：匹配素材所在的文件夹")
            : QStringLiteral("目标：文件夹");
    }
    if (target == SearchResultTarget::Frames) {
        return QStringLiteral("目标：视觉帧");
    }
    return QStringLiteral("目标：素材");
}

QString assetTypeLabel(int type)
{
    switch (static_cast<AssetType>(type)) {
    case AssetType::Video: return QStringLiteral("视频");
    case AssetType::Audio: return QStringLiteral("音频");
    case AssetType::Image: return QStringLiteral("图片");
    case AssetType::Document: return QStringLiteral("文档");
    case AssetType::Subtitle: return QStringLiteral("字幕");
    case AssetType::Archive: return QStringLiteral("压缩包");
    case AssetType::ProjectFile: return QStringLiteral("工程文件");
    default: return {};
    }
}

void replaceInterpretationLabel(QStringList *labels,
                                const QStringList &prefixes,
                                const QString &replacement)
{
    labels->erase(std::remove_if(labels->begin(), labels->end(), [&prefixes](const QString &label) {
        for (const auto &prefix : prefixes) {
            if (label.startsWith(prefix)) {
                return true;
            }
        }
        return false;
    }), labels->end());
    if (!replacement.trimmed().isEmpty()) {
        labels->append(replacement);
    }
}

void mergeEntity(QVector<StrictEntityConstraint> *entities,
                  const StrictEntityConstraint &incoming)
{
    for (auto &entity : *entities) {
        if (entity.label.compare(incoming.label, Qt::CaseInsensitive) != 0) {
            continue;
        }
        entity.colors = uniqueTerms(entity.colors + incoming.colors);
        entity.materials = uniqueTerms(entity.materials + incoming.materials);
        entity.attributes = uniqueTerms(entity.attributes + incoming.attributes);
        return;
    }
    if (entities->size() < kMaxEntityCount) {
        entities->append(incoming);
    }
}

void removeTermsImpliedByLabel(QStringList *terms, const QString &label)
{
    const auto normalizedLabel = label.simplified().toCaseFolded();
    terms->erase(std::remove_if(terms->begin(), terms->end(), [&](const QString &term) {
        const auto normalizedTerm = term.simplified().toCaseFolded();
        return !normalizedTerm.isEmpty()
            && normalizedLabel != normalizedTerm
            && normalizedLabel.contains(normalizedTerm);
    }), terms->end());
}

void normalizeEntityConstraints(QVector<StrictEntityConstraint> *entities)
{
    for (auto &entity : *entities) {
        removeTermsImpliedByLabel(&entity.materials, entity.label);
        entity.colors = uniqueTerms(entity.colors);
        entity.materials = uniqueTerms(entity.materials);
        entity.attributes = uniqueTerms(entity.attributes);
    }
}
}

QJsonObject SearchQueryUnderstanding::responseSchema()
{
    const auto stringArraySchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("array")},
        {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
        {QStringLiteral("maxItems"), kMaxTermCount}
    };
    const auto entitySchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("label"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
            {QStringLiteral("colors"), stringArraySchema},
            {QStringLiteral("materials"), stringArraySchema},
            {QStringLiteral("attributes"), stringArraySchema}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("label"), QStringLiteral("colors"), QStringLiteral("materials"), QStringLiteral("attributes")}}
    };
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("version"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("const"), 1}}},
            {QStringLiteral("result_target"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("enum"), QJsonArray{QStringLiteral("unspecified"), QStringLiteral("assets"), QStringLiteral("folders"), QStringLiteral("frames")}}}},
            {QStringLiteral("semantic_text"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
            {QStringLiteral("lexical_terms"), stringArraySchema},
            {QStringLiteral("asset_types"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")}, {QStringLiteral("items"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("enum"), QJsonArray{QStringLiteral("video"), QStringLiteral("audio"), QStringLiteral("image"), QStringLiteral("document"), QStringLiteral("subtitle"), QStringLiteral("archive"), QStringLiteral("project_file")}}}}}},
            {QStringLiteral("date"), QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}, {QStringLiteral("additionalProperties"), false}, {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("start"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                {QStringLiteral("end"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                {QStringLiteral("matched_text"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                {QStringLiteral("preferred_field"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("enum"), QJsonArray{QStringLiteral("any"), QStringLiteral("captured"), QStringLiteral("folder"), QStringLiteral("modified")}}}}
            }}, {QStringLiteral("required"), QJsonArray{QStringLiteral("start"), QStringLiteral("end"), QStringLiteral("matched_text"), QStringLiteral("preferred_field")}}}},
            {QStringLiteral("folder_by_asset_criteria"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
            {QStringLiteral("ocr_text"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
            {QStringLiteral("entities"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")}, {QStringLiteral("items"), entitySchema}, {QStringLiteral("maxItems"), kMaxEntityCount}}},
            {QStringLiteral("confidence"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}, {QStringLiteral("minimum"), 0.0}, {QStringLiteral("maximum"), 1.0}}},
            {QStringLiteral("explanation"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("version"), QStringLiteral("result_target"), QStringLiteral("semantic_text"),
            QStringLiteral("lexical_terms"), QStringLiteral("asset_types"), QStringLiteral("date"),
            QStringLiteral("folder_by_asset_criteria"), QStringLiteral("ocr_text"), QStringLiteral("entities"),
            QStringLiteral("confidence"), QStringLiteral("explanation")
        }}
    };
}

QJsonObject SearchQueryUnderstanding::frameRerankResponseSchema()
{
    const auto matchSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("candidate_id"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
            {QStringLiteral("relevant"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
            {QStringLiteral("score"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}, {QStringLiteral("minimum"), 0.0}, {QStringLiteral("maximum"), 1.0}}},
            {QStringLiteral("reason"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("candidate_id"), QStringLiteral("relevant"), QStringLiteral("score"), QStringLiteral("reason")}}
    };
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("version"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("const"), 1}}},
            {QStringLiteral("matches"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")}, {QStringLiteral("items"), matchSchema}, {QStringLiteral("maxItems"), 8}}}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("version"), QStringLiteral("matches")}}
    };
}

std::optional<ModelSearchUnderstanding> SearchQueryUnderstanding::parseModelPayload(
    const QJsonObject &payload,
    QString *errorMessage)
{
    if (payload.value(QStringLiteral("version")).toInt(-1) != 1) {
        if (errorMessage) *errorMessage = QStringLiteral("模型查询理解协议版本不受支持");
        return std::nullopt;
    }
    const auto confidenceValue = payload.value(QStringLiteral("confidence"));
    if (!confidenceValue.isDouble()) {
        if (errorMessage) *errorMessage = QStringLiteral("模型查询理解缺少有效置信度");
        return std::nullopt;
    }

    ModelSearchUnderstanding result;
    result.confidence = confidenceValue.toDouble();
    if (result.confidence < 0.0 || result.confidence > 1.0) {
        if (errorMessage) *errorMessage = QStringLiteral("模型查询理解置信度越界");
        return std::nullopt;
    }
    result.semanticText = boundedText(payload.value(QStringLiteral("semantic_text")));
    result.lexicalTerms = stringArray(payload.value(QStringLiteral("lexical_terms")));
    result.ocrText = boundedText(payload.value(QStringLiteral("ocr_text")), 160);
    result.explanation = boundedText(payload.value(QStringLiteral("explanation")), 200);
    result.folderByAssetCriteria = payload.value(QStringLiteral("folder_by_asset_criteria")).toBool(false);

    const auto target = payload.value(QStringLiteral("result_target")).toString().trimmed().toLower();
    if (target == QStringLiteral("assets")) {
        result.resultTarget = SearchResultTarget::Assets;
        result.resultTargetSpecified = true;
    } else if (target == QStringLiteral("folders")) {
        result.resultTarget = SearchResultTarget::Folders;
        result.resultTargetSpecified = true;
    } else if (target == QStringLiteral("frames")) {
        result.resultTarget = SearchResultTarget::Frames;
        result.resultTargetSpecified = true;
    } else if (target != QStringLiteral("unspecified")) {
        if (errorMessage) *errorMessage = QStringLiteral("模型返回了未知结果类型");
        return std::nullopt;
    }

    const auto typeNames = stringArray(payload.value(QStringLiteral("asset_types")));
    for (const auto &name : typeNames) {
        const auto type = assetTypeFromName(name);
        if (type < 0) {
            if (errorMessage) *errorMessage = QStringLiteral("模型返回了未知素材类型：%1").arg(name);
            return std::nullopt;
        }
        if (!result.assetTypeFilters.contains(type)) {
            result.assetTypeFilters.append(type);
        }
    }

    const auto dateObject = payload.value(QStringLiteral("date")).toObject();
    const auto startText = boundedText(dateObject.value(QStringLiteral("start")), 10);
    const auto endText = boundedText(dateObject.value(QStringLiteral("end")), 10);
    if (!startText.isEmpty() || !endText.isEmpty()) {
        const auto start = QDate::fromString(startText, Qt::ISODate);
        const auto end = QDate::fromString(endText, Qt::ISODate);
        if (!start.isValid() || !end.isValid() || start > end || start.daysTo(end) > 3660) {
            if (errorMessage) *errorMessage = QStringLiteral("模型返回了无效或过宽的日期范围");
            return std::nullopt;
        }
        result.dateConstraint.startDate = start.toString(Qt::ISODate);
        result.dateConstraint.endDate = end.toString(Qt::ISODate);
        result.dateConstraint.matchedText = boundedText(dateObject.value(QStringLiteral("matched_text")), 80);
        result.dateConstraint.preferredField = dateFieldFromName(dateObject.value(QStringLiteral("preferred_field")).toString());
    }

    const auto entityArray = payload.value(QStringLiteral("entities")).toArray();
    for (const auto &value : entityArray) {
        if (!value.isObject()) {
            continue;
        }
        const auto object = value.toObject();
        StrictEntityConstraint entity;
        entity.label = boundedText(object.value(QStringLiteral("label")), 48);
        entity.colors = stringArray(object.value(QStringLiteral("colors")), 8);
        entity.materials = stringArray(object.value(QStringLiteral("materials")), 8);
        entity.attributes = stringArray(object.value(QStringLiteral("attributes")), 8);
        if (!entity.isEmpty()) {
            result.strictEntities.append(entity);
        }
        if (result.strictEntities.size() >= kMaxEntityCount) {
            break;
        }
    }
    return result;
}

ParsedMaterialQuery SearchQueryUnderstanding::merge(
    const ParsedMaterialQuery &localQuery,
    const ModelSearchUnderstanding &modelUnderstanding,
    bool *modelApplied)
{
    if (modelApplied) {
        *modelApplied = false;
    }
    auto merged = localQuery;
    if (modelUnderstanding.confidence < kMinimumConfidence) {
        return merged;
    }

    bool changed = false;
    if (!modelUnderstanding.semanticText.isEmpty()) {
        const auto localSemantic = merged.semanticText.simplified();
        const auto modelSemantic = modelUnderstanding.semanticText.simplified();
        if (localSemantic.isEmpty()) {
            merged.semanticText = modelSemantic;
        } else if (localSemantic.compare(modelSemantic, Qt::CaseInsensitive) != 0) {
            merged.semanticText = QStringLiteral("%1 %2").arg(localSemantic, modelSemantic).simplified();
        }
        changed = true;
    }

    if (merged.dateConstraint.isEmpty() && !modelUnderstanding.dateConstraint.isEmpty()) {
        merged.dateConstraint = modelUnderstanding.dateConstraint;
        merged.normalizedDate = merged.dateConstraint.isExactDate()
            ? merged.dateConstraint.startDate
            : QString();
        changed = true;
        const auto dateValue = merged.dateConstraint.isExactDate()
            ? merged.dateConstraint.startDate
            : QStringLiteral("%1 至 %2").arg(merged.dateConstraint.startDate, merged.dateConstraint.endDate);
        replaceInterpretationLabel(&merged.interpretationLabels,
                                   {QStringLiteral("日期："), QStringLiteral("拍摄日期："), QStringLiteral("目录日期："), QStringLiteral("文件修改日期：")},
                                   QStringLiteral("模型日期：%1").arg(dateValue));
    }

    if (merged.assetTypeFilters.isEmpty() && !modelUnderstanding.assetTypeFilters.isEmpty()) {
        merged.assetTypeFilters = modelUnderstanding.assetTypeFilters;
        merged.assetTypeFilter = merged.assetTypeFilters.first();
        QStringList labels;
        for (const auto type : merged.assetTypeFilters) {
            labels.append(assetTypeLabel(type));
        }
        replaceInterpretationLabel(&merged.interpretationLabels,
                                   {QStringLiteral("类型：")},
                                   QStringLiteral("类型：%1").arg(labels.join(QStringLiteral(" / "))));
        changed = true;
    }

    const bool localTargetLocked = merged.folderIntent || merged.frameIntent || hasExplicitAssetTarget(localQuery);
    if (!localTargetLocked && modelUnderstanding.resultTargetSpecified) {
        merged.resultTarget = modelUnderstanding.resultTarget;
        merged.folderIntent = merged.resultTarget == SearchResultTarget::Folders;
        merged.frameIntent = merged.resultTarget == SearchResultTarget::Frames;
        merged.folderByAssetCriteria = merged.folderIntent && modelUnderstanding.folderByAssetCriteria;
        replaceInterpretationLabel(&merged.interpretationLabels,
                                   {QStringLiteral("目标：")},
                                   targetLabel(merged.resultTarget, merged.folderByAssetCriteria));
        changed = true;
    }

    if (merged.ocrText.isEmpty() && !modelUnderstanding.ocrText.isEmpty()) {
        merged.ocrText = modelUnderstanding.ocrText;
        replaceInterpretationLabel(&merged.interpretationLabels,
                                   {QStringLiteral("画面文字：")},
                                   QStringLiteral("画面文字：%1").arg(merged.ocrText));
        changed = true;
    }

    for (const auto &entity : modelUnderstanding.strictEntities) {
        mergeEntity(&merged.strictEntities, entity);
        changed = true;
    }
    normalizeEntityConstraints(&merged.strictEntities);
    QStringList lexical = merged.lexicalTerms;
    lexical.append(modelUnderstanding.lexicalTerms);
    for (const auto &entity : merged.strictEntities) {
        lexical.append(entity.allTerms());
    }
    if (!merged.ocrText.isEmpty()) {
        lexical.append(merged.ocrText);
    }
    merged.lexicalTerms = uniqueTerms(lexical);

    if (!modelUnderstanding.strictEntities.isEmpty()) {
        QStringList entityLabels;
        for (const auto &entity : merged.strictEntities) {
            entityLabels.append(entity.allTerms().join(QLatin1Char(' ')));
        }
        replaceInterpretationLabel(&merged.interpretationLabels,
                                   {QStringLiteral("同一对象：")},
                                   QStringLiteral("同一对象：%1").arg(entityLabels.join(QStringLiteral("；"))));
    }
    if (!merged.semanticText.isEmpty()) {
        replaceInterpretationLabel(&merged.interpretationLabels,
                                   {QStringLiteral("内容：")},
                                   QStringLiteral("内容：%1").arg(merged.semanticText));
    }
    if (changed) {
        merged.interpretationLabels.append(QStringLiteral("视觉语言模型辅助理解"));
    }
    merged.interpretationLabels = uniqueTerms(merged.interpretationLabels, 32);
    if (modelApplied) {
        *modelApplied = changed;
    }
    return merged;
}

std::optional<QVector<ModelFrameRerankScore>> SearchQueryUnderstanding::parseFrameRerankPayload(
    const QJsonObject &payload,
    const QStringList &allowedFrameKeys,
    QString *errorMessage)
{
    if (payload.value(QStringLiteral("version")).toInt(-1) != 1) {
        if (errorMessage) *errorMessage = QStringLiteral("候选帧复核协议版本不受支持");
        return std::nullopt;
    }
    QSet<QString> allowed;
    for (const auto &key : allowedFrameKeys) {
        if (!key.trimmed().isEmpty()) {
            allowed.insert(key.trimmed());
        }
    }
    if (allowed.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("候选帧白名单为空");
        return std::nullopt;
    }

    QVector<ModelFrameRerankScore> scores;
    QSet<QString> seen;
    const auto matches = payload.value(QStringLiteral("matches")).toArray();
    for (const auto &value : matches) {
        if (!value.isObject()) {
            continue;
        }
        const auto object = value.toObject();
        const auto key = boundedText(object.value(QStringLiteral("candidate_id")), 180);
        if (!allowed.contains(key) || seen.contains(key)) {
            continue;
        }
        const auto scoreValue = object.value(QStringLiteral("score"));
        if (!scoreValue.isDouble()) {
            continue;
        }
        const auto score = scoreValue.toDouble();
        if (score < 0.0 || score > 1.0) {
            continue;
        }
        ModelFrameRerankScore item;
        item.frameKey = key;
        item.relevant = object.value(QStringLiteral("relevant")).toBool(false);
        item.score = score;
        item.reason = boundedText(object.value(QStringLiteral("reason")), 120);
        scores.append(item);
        seen.insert(key);
    }
    if (scores.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("模型没有返回任何白名单内的候选帧");
        return std::nullopt;
    }
    std::stable_sort(scores.begin(), scores.end(), [](const auto &left, const auto &right) {
        if (left.relevant != right.relevant) {
            return left.relevant;
        }
        return left.score > right.score;
    });
    return scores;
}

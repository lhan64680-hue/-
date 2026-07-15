#pragma once

#include "domain/Entities.h"

#include <QString>
#include <QStringList>
#include <QVector>

enum class SearchDocumentType : int {
    Unknown = 0,
    Folder = 1,
    Asset = 2,
    VisualEntity = 3
};

struct SearchDocumentInput {
    QString documentKey;
    SearchDocumentType documentType = SearchDocumentType::Unknown;
    QString entityKey;
    QString contentText;
    QString sourceUpdatedAt;
};

struct SemanticIndexUpdateResult {
    int inserted = 0;
    int updated = 0;
    int unchanged = 0;
    int removed = 0;
    bool rebuilt = false;
};

struct StrictEntityConstraint {
    QString label;
    QStringList colors;
    QStringList materials;
    QStringList attributes;

    [[nodiscard]] bool isEmpty() const { return label.trimmed().isEmpty(); }
    [[nodiscard]] QStringList allTerms() const
    {
        QStringList terms{label};
        terms.append(colors);
        terms.append(materials);
        terms.append(attributes);
        terms.removeAll(QString());
        terms.removeDuplicates();
        return terms;
    }
};

enum class SearchDateField : int {
    Any = 0,
    CapturedTime = 1,
    FolderDate = 2,
    FileModifiedTime = 3
};

enum class SearchResultTarget : int {
    Assets = 0,
    Folders = 1,
    Frames = 2
};

enum class SearchResultQuickFilter : int {
    Smart = 0,
    Video = 1,
    Frames = 2,
    Image = 3,
    Document = 4
};

struct SearchDateConstraint {
    QString startDate;
    QString endDate;
    QString matchedText;
    SearchDateField preferredField = SearchDateField::Any;
    bool allowFallback = true;

    [[nodiscard]] bool isEmpty() const
    {
        return startDate.trimmed().isEmpty() || endDate.trimmed().isEmpty();
    }

    [[nodiscard]] bool isExactDate() const
    {
        return !isEmpty() && startDate == endDate;
    }
};

struct ParsedMaterialQuery {
    QString originalText;
    QString semanticText;
    QStringList lexicalTerms;
    SearchDateConstraint dateConstraint;
    // Kept for callers that only understand an exact date. Range-aware search uses dateConstraint.
    QString normalizedDate;
    QVector<int> assetTypeFilters;
    // Kept for callers that only understand one type. New search uses assetTypeFilters as OR conditions.
    int assetTypeFilter = -1;
    SearchResultTarget resultTarget = SearchResultTarget::Assets;
    bool folderIntent = false;
    bool frameIntent = false;
    bool folderByAssetCriteria = false;
    QString ocrText;
    // Canonical entity labels explicitly present in the user's text. They are
    // kept separate from strictEntities because modifier ownership may require
    // the text model to resolve a multi-entity relation.
    QStringList explicitEntityLabels;
    QVector<StrictEntityConstraint> strictEntities;
    QStringList interpretationLabels;
    QStringList ignoredIntentTerms;

    [[nodiscard]] bool hasStrictEntityConstraints() const { return !strictEntities.isEmpty(); }
};

struct ModelSearchUnderstanding {
    QString semanticText;
    QStringList lexicalTerms;
    SearchDateConstraint dateConstraint;
    QVector<int> assetTypeFilters;
    SearchResultTarget resultTarget = SearchResultTarget::Assets;
    bool resultTargetSpecified = false;
    bool folderByAssetCriteria = false;
    QString ocrText;
    QVector<StrictEntityConstraint> strictEntities;
    double confidence = 0.0;
    QString explanation;
};

struct SearchReliabilityAssessment {
    bool shouldUseAssistant = false;
    double score = 1.0;
    qsizetype resultCount = 0;
    double bestResultScore = 0.0;
    double bestResultConfidence = 0.0;
    QStringList reasons;
};

struct FolderSearchHit {
    QString folderKey;
    QString projectUuid;
    QString projectName;
    QString projectDatabasePath;
    qint64 sourceRootId = 0;
    QString sourceRootName;
    QString name;
    QString absolutePath;
    QString relativePath;
    QString parentRelativePath;
    int depth = 0;
    qint64 directFileCount = 0;
    qint64 recursiveFileCount = 0;
    QString normalizedDate;
    bool available = true;
    double score = 0.0;
    double confidence = 0.0;
    QStringList reasons;
};

struct FrameSearchHit {
    QString frameKey;
    QString videoKey;
    QString assetKey;
    QString fileName;
    QString projectName;
    QString sourceRootName;
    QString relativePath;
    AssetType assetType = AssetType::Video;
    int frameNumber = 0;
    qint64 timestampMs = 0;
    QString imagePath;
    QString caption;
    QStringList tags;
    QStringList objects;
    QString actions;
    QString setting;
    QVector<VisionEntityFact> entities;
    QString ocrText;
    bool factsComplete = false;
    double score = 0.0;
    double confidence = 0.0;
    QStringList reasons;
};

struct MaterialSearchResult {
    ParsedMaterialQuery parsedQuery;
    SearchReliabilityAssessment reliability;
    QVector<FolderSearchHit> folders;
    QVector<GlobalVideoAsset> assets;
    QVector<FrameSearchHit> frames;
    int excludedPartialCount = 0;
    bool semanticSearchAvailable = false;
    QString warningMessage;
};

struct SemanticSearchHit {
    QString documentKey;
    double similarity = 0.0;
};

struct MaterialSearchScope {
    QString projectUuid;
    QString sourceRootName;
    int analysisStatusFilter = -1;
    int confirmationStatusFilter = -1;
    int assetTypeFilter = -1;
    SearchResultQuickFilter resultQuickFilter = SearchResultQuickFilter::Smart;
    qsizetype limit = 2000;
};

struct HybridSearchHit {
    QString documentKey;
    SearchDocumentType documentType = SearchDocumentType::Unknown;
    QString entityKey;
    QString assetEntityKey;
    double lexicalScore = 0.0;
    double pathScore = 0.0;
    double dateScore = 0.0;
    double typeScore = 0.0;
    double semanticScore = 0.0;
    double score = 0.0;
    double confidence = 0.0;
    QString dateValue;
    QString dateSource;
    double dateConfidence = 0.0;
    QStringList reasons;
    int matchedFrameNumber = -1;
    qint64 matchedTimestampMs = -1;
    QString matchedFrameCaption;
};

struct HybridSearchResult {
    ParsedMaterialQuery parsedQuery;
    QVector<HybridSearchHit> hits;
    bool semanticSearchAvailable = false;
    QString warningMessage;
};

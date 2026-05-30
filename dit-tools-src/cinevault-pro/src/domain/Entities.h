#pragma once

#include "domain/Enums.h"

#include <QDateTime>
#include <QString>
#include <QVariantList>

struct Project {
    QString id;
    QString name;
    QString rootPath;
    QString databasePath;
    QString createdAt;
};

struct SourceRoot {
    qint64 id = 0;
    QString name;
    QString path;
    QString status;
    qint64 totalFiles = 0;
    qint64 totalFolders = 0;
    qint64 totalSizeBytes = 0;
    qint64 videoCount = 0;
    qint64 audioCount = 0;
    qint64 imageCount = 0;
    qint64 otherCount = 0;
    qint64 warningCount = 0;
};

struct FolderNode {
    qint64 id = 0;
    qint64 sourceRootId = 0;
    QString absolutePath;
    QString relativePath;
    qint64 fileCount = 0;
};

struct AssetFile {
    qint64 id = 0;
    qint64 sourceRootId = 0;
    QString name;
    QString extension;
    QString absolutePath;
    QString relativePath;
    QString parentPath;
    AssetType assetType = AssetType::Unknown;
    qint64 sizeBytes = 0;
    QString modifiedAt;
    bool readable = false;
};

struct Job {
    qint64 id = 0;
    JobType type = JobType::Scan;
    JobState state = JobState::Pending;
    QString title;
    QString detail;
    QString errorMessage;
    qint64 progress = 0;
    qint64 sourceRootId = 0;
    QDateTime startedAt;
    QDateTime updatedAt;
};

struct SelectionState {
    SelectionKind kind = SelectionKind::None;
    qint64 sourceRootId = 0;
    qint64 folderId = 0;
    qint64 assetId = 0;
};

struct ScanBatch {
    qint64 sourceRootId = 0;
    qint64 processedEntries = 0;
    qint64 totalFiles = 0;
    qint64 totalFolders = 0;
    qint64 totalSizeBytes = 0;
    qint64 warningCount = 0;
    qint64 progressPercent = 0;
};

struct InspectorState {
    QString title;
    QString subtitle;
    QVariantList details;
};

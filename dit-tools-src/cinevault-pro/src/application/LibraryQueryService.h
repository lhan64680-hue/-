#pragma once

#include "domain/Entities.h"

#include <QObject>
#include <optional>

class DatabaseManager;
class SearchEngine;

class LibraryQueryService : public QObject {
    Q_OBJECT

public:
    explicit LibraryQueryService(DatabaseManager *databaseManager, SearchEngine *searchEngine, QObject *parent = nullptr);

    QVector<SourceRoot> fetchSourceRoots() const;
    QVector<AssetFile> fetchAssets(const QString &keyword, std::optional<qint64> sourceRootId, std::optional<AssetType> assetType) const;
    InspectorState buildSourceInspector(qint64 sourceRootId) const;
    InspectorState buildAssetInspector(qint64 assetId) const;
    qint64 assetCount(const QString &keyword, std::optional<qint64> sourceRootId, std::optional<AssetType> assetType) const;

signals:
    void dataChanged();

private:
    DatabaseManager *m_databaseManager = nullptr;
    SearchEngine *m_searchEngine = nullptr;
};

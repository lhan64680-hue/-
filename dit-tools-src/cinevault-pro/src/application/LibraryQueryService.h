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
    QVector<AssetFile> fetchAssets(const QString &keyword,
                                   std::optional<qint64> sourceRootId,
                                   std::optional<AssetType> assetType,
                                   bool favoritesOnly,
                                   bool modifiedTimeAscending) const;
    InspectorState buildSourceInspector(qint64 sourceRootId) const;
    InspectorState buildAssetInspector(qint64 assetId) const;
    qint64 assetCount(const QString &keyword, std::optional<qint64> sourceRootId, std::optional<AssetType> assetType, bool favoritesOnly) const;
    bool setAssetFavorite(qint64 assetId, bool favorite);
    bool removeAsset(qint64 assetId);
    bool removeSourceRoot(qint64 sourceRootId);

signals:
    void dataChanged();

private:
    DatabaseManager *m_databaseManager = nullptr;
    SearchEngine *m_searchEngine = nullptr;
};

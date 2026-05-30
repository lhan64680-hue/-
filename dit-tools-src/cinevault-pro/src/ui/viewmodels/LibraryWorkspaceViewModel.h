#pragma once

#include "domain/Enums.h"

#include <QObject>
#include <optional>

class AssetListModel;
class LibraryQueryService;

class LibraryWorkspaceViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(AssetListModel* model READ model CONSTANT)
    Q_PROPERTY(int viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(qint64 selectedAssetId READ selectedAssetId NOTIFY selectionChanged)

public:
    explicit LibraryWorkspaceViewModel(LibraryQueryService *libraryQueryService, QObject *parent = nullptr);

    AssetListModel *model() const;
    int viewMode() const;
    QString statusText() const;
    qint64 selectedAssetId() const;

    void setViewMode(int viewMode);

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setSearchText(const QString &searchText);
    Q_INVOKABLE void setSourceFilter(qint64 sourceRootId);
    Q_INVOKABLE void setAssetTypeFilter(int assetType);
    Q_INVOKABLE void selectAsset(qint64 assetId);

signals:
    void viewModeChanged();
    void statusChanged();
    void selectionChanged();
    void assetSelected(qint64 assetId);

private:
    void refresh();

    LibraryQueryService *m_libraryQueryService = nullptr;
    AssetListModel *m_model = nullptr;
    QString m_searchText;
    int m_viewMode = 0;
    qint64 m_selectedAssetId = 0;
    std::optional<qint64> m_sourceFilter;
    std::optional<AssetType> m_assetTypeFilter;
    qint64 m_totalCount = 0;
};

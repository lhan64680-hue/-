#include "ui/viewmodels/LibraryWorkspaceViewModel.h"

#include "application/LibraryQueryService.h"
#include "ui/models/AssetListModel.h"

LibraryWorkspaceViewModel::LibraryWorkspaceViewModel(LibraryQueryService *libraryQueryService, QObject *parent)
    : QObject(parent)
    , m_libraryQueryService(libraryQueryService)
    , m_model(new AssetListModel(this))
{
}

AssetListModel *LibraryWorkspaceViewModel::model() const
{
    return m_model;
}

int LibraryWorkspaceViewModel::viewMode() const
{
    return m_viewMode;
}

QString LibraryWorkspaceViewModel::statusText() const
{
    return QStringLiteral("当前结果 %1 条").arg(m_totalCount);
}

qint64 LibraryWorkspaceViewModel::selectedAssetId() const
{
    return m_selectedAssetId;
}

void LibraryWorkspaceViewModel::setViewMode(int viewMode)
{
    if (m_viewMode == viewMode) {
        return;
    }
    m_viewMode = viewMode;
    emit viewModeChanged();
}

void LibraryWorkspaceViewModel::reload()
{
    refresh();
}

void LibraryWorkspaceViewModel::setSearchText(const QString &searchText)
{
    if (m_searchText == searchText) {
        return;
    }
    m_searchText = searchText;
    refresh();
}

void LibraryWorkspaceViewModel::setSourceFilter(qint64 sourceRootId)
{
    m_sourceFilter = sourceRootId > 0 ? std::optional<qint64>(sourceRootId) : std::nullopt;
    if (m_selectedAssetId != 0) {
        m_selectedAssetId = 0;
        emit selectionChanged();
    }
    refresh();
}

void LibraryWorkspaceViewModel::setAssetTypeFilter(int assetType)
{
    if (assetType < 0) {
        m_assetTypeFilter.reset();
    } else {
        m_assetTypeFilter = static_cast<AssetType>(assetType);
    }
    refresh();
}

void LibraryWorkspaceViewModel::selectAsset(qint64 assetId)
{
    if (m_selectedAssetId == assetId) {
        return;
    }
    m_selectedAssetId = assetId;
    emit selectionChanged();
    emit assetSelected(assetId);
}

void LibraryWorkspaceViewModel::refresh()
{
    const auto items = m_libraryQueryService->fetchAssets(m_searchText, m_sourceFilter, m_assetTypeFilter);
    m_model->setItems(items);
    m_totalCount = m_libraryQueryService->assetCount(m_searchText, m_sourceFilter, m_assetTypeFilter);
    bool foundSelection = false;
    for (const auto &item : items) {
        if (item.id == m_selectedAssetId) {
            foundSelection = true;
            break;
        }
    }
    if (!foundSelection && m_selectedAssetId != 0) {
        m_selectedAssetId = 0;
        emit selectionChanged();
    }
    emit statusChanged();
}

#include "ui/viewmodels/MinimalLibraryWorkspaceViewModel.h"

#include "ui/models/AssetListModel.h"

MinimalLibraryWorkspaceViewModel::MinimalLibraryWorkspaceViewModel(QObject *parent)
    : QObject(parent)
    , m_model(new AssetListModel(this))
{
    seedAssets();
    refresh();
}

AssetListModel *MinimalLibraryWorkspaceViewModel::model() const
{
    return m_model;
}

int MinimalLibraryWorkspaceViewModel::viewMode() const
{
    return m_viewMode;
}

QString MinimalLibraryWorkspaceViewModel::statusText() const
{
    return QStringLiteral("当前结果 %1 条").arg(m_totalCount);
}

qint64 MinimalLibraryWorkspaceViewModel::selectedAssetId() const
{
    return m_selectedAssetId;
}

AssetFile MinimalLibraryWorkspaceViewModel::assetById(qint64 assetId) const
{
    for (const auto &asset : m_allAssets) {
        if (asset.id == assetId) {
            return asset;
        }
    }
    return {};
}

void MinimalLibraryWorkspaceViewModel::setViewMode(int viewMode)
{
    if (m_viewMode == viewMode) {
        return;
    }
    m_viewMode = viewMode;
    emit viewModeChanged();
}

void MinimalLibraryWorkspaceViewModel::reload()
{
    refresh();
}

void MinimalLibraryWorkspaceViewModel::setSearchText(const QString &searchText)
{
    if (m_searchText == searchText) {
        return;
    }
    m_searchText = searchText;
    refresh();
}

void MinimalLibraryWorkspaceViewModel::setSourceFilter(qint64 sourceRootId)
{
    m_sourceFilter = sourceRootId > 0 ? std::optional<qint64>(sourceRootId) : std::nullopt;
    refresh();
}

void MinimalLibraryWorkspaceViewModel::setAssetTypeFilter(int assetType)
{
    m_assetTypeFilter = assetType >= 0 ? std::optional<AssetType>(static_cast<AssetType>(assetType)) : std::nullopt;
    refresh();
}

void MinimalLibraryWorkspaceViewModel::selectAsset(qint64 assetId)
{
    if (m_selectedAssetId == assetId) {
        return;
    }
    m_selectedAssetId = assetId;
    emit selectionChanged();
    emit assetSelected(assetId);
}

void MinimalLibraryWorkspaceViewModel::seedAssets()
{
    m_allAssets = {
        AssetFile{101, 1, QStringLiteral("A001_C001.mov"), QStringLiteral("mov"), QStringLiteral("G:/demo/A001_CARD/A001_C001.mov"), QStringLiteral("A001_C001.mov"), QStringLiteral("G:/demo/A001_CARD"), AssetType::Video, 1850LL * 1024 * 1024, QStringLiteral("2026-05-30T09:10:00"), true},
        AssetFile{102, 1, QStringLiteral("A001_C002.mov"), QStringLiteral("mov"), QStringLiteral("G:/demo/A001_CARD/A001_C002.mov"), QStringLiteral("A001_C002.mov"), QStringLiteral("G:/demo/A001_CARD"), AssetType::Video, 1934LL * 1024 * 1024, QStringLiteral("2026-05-30T09:12:00"), true},
        AssetFile{201, 2, QStringLiteral("B001_C014.mov"), QStringLiteral("mov"), QStringLiteral("G:/demo/B_CAM_REEL/B001_C014.mov"), QStringLiteral("DAY01/B001_C014.mov"), QStringLiteral("G:/demo/B_CAM_REEL/DAY01"), AssetType::Video, 2230LL * 1024 * 1024, QStringLiteral("2026-05-30T08:48:00"), true},
        AssetFile{301, 3, QStringLiteral("SND_001.wav"), QStringLiteral("wav"), QStringLiteral("G:/demo/SOUND_DAY01/SND_001.wav"), QStringLiteral("SND_001.wav"), QStringLiteral("G:/demo/SOUND_DAY01"), AssetType::Audio, 184LL * 1024 * 1024, QStringLiteral("2026-05-30T07:35:00"), true},
        AssetFile{302, 3, QStringLiteral("SND_002.wav"), QStringLiteral("wav"), QStringLiteral("G:/demo/SOUND_DAY01/SND_002.wav"), QStringLiteral("SND_002.wav"), QStringLiteral("G:/demo/SOUND_DAY01"), AssetType::Audio, 176LL * 1024 * 1024, QStringLiteral("2026-05-30T07:40:00"), true}
    };
}

void MinimalLibraryWorkspaceViewModel::refresh()
{
    QVector<AssetFile> filtered;
    for (const auto &asset : m_allAssets) {
        if (m_sourceFilter.has_value() && asset.sourceRootId != m_sourceFilter.value()) {
            continue;
        }
        if (m_assetTypeFilter.has_value() && asset.assetType != m_assetTypeFilter.value()) {
            continue;
        }
        if (!m_searchText.trimmed().isEmpty()) {
            const auto keyword = m_searchText.trimmed();
            if (!asset.name.contains(keyword, Qt::CaseInsensitive)
                && !asset.relativePath.contains(keyword, Qt::CaseInsensitive)) {
                continue;
            }
        }
        filtered.append(asset);
    }

    m_model->setItems(filtered);
    m_totalCount = filtered.size();

    bool selectionExists = false;
    for (const auto &asset : filtered) {
        if (asset.id == m_selectedAssetId) {
            selectionExists = true;
            break;
        }
    }
    if (!selectionExists && m_selectedAssetId != 0) {
        m_selectedAssetId = 0;
        emit selectionChanged();
    }

    emit statusChanged();
}

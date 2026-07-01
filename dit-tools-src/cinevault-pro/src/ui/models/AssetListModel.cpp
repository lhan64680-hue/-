#include "ui/models/AssetListModel.h"

#include "shared/Formatters.h"

AssetListModel::AssetListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AssetListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant AssetListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    switch (role) {
    case IdRole: return item.id;
    case SourceRootIdRole: return item.sourceRootId;
    case NameRole: return item.name;
    case RelativePathRole: return item.relativePath;
    case ParentPathRole: return item.parentPath;
    case TypeRole: return static_cast<int>(item.assetType);
    case TypeLabelRole: return Formatters::assetTypeLabel(item.assetType);
    case SizeBytesRole: return item.sizeBytes;
    case SizeLabelRole: return Formatters::formatBytes(item.sizeBytes);
    case ModifiedAtRole: return item.modifiedAt;
    case ReadableRole: return item.readable;
    case ThumbnailPathRole: return item.thumbnailPath;
    case ThumbnailStatusRole: return static_cast<int>(item.thumbnailStatus);
    case ThumbnailLoadingRole: return item.thumbnailStatus == ThumbnailStatus::Running && item.thumbnailPath.isEmpty();
    case DurationLabelRole: return Formatters::formatDuration(item.durationMs);
    case BitRateLabelRole: return Formatters::formatBitRate(item.bitRate);
    case TechnicalSummaryRole: return item.technicalSummary;
    case ProbeStatusLabelRole: return Formatters::probeStatusLabel(item.probeStatus);
    case FavoriteRole: return item.favorite;
    default: return {};
    }
}

QHash<int, QByteArray> AssetListModel::roleNames() const
{
    return {
        {IdRole, "assetId"},
        {SourceRootIdRole, "sourceRootId"},
        {NameRole, "name"},
        {RelativePathRole, "relativePath"},
        {ParentPathRole, "parentPath"},
        {TypeRole, "type"},
        {TypeLabelRole, "typeLabel"},
        {SizeBytesRole, "sizeBytes"},
        {SizeLabelRole, "sizeLabel"},
        {ModifiedAtRole, "modifiedAt"},
        {ReadableRole, "readable"},
        {ThumbnailPathRole, "thumbnailPath"},
        {ThumbnailStatusRole, "thumbnailStatus"},
        {ThumbnailLoadingRole, "thumbnailLoading"},
        {DurationLabelRole, "durationLabel"},
        {BitRateLabelRole, "bitRateLabel"},
        {TechnicalSummaryRole, "technicalSummary"},
        {ProbeStatusLabelRole, "probeStatusLabel"},
        {FavoriteRole, "favorite"}
    };
}

void AssetListModel::setItems(const QVector<AssetFile> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

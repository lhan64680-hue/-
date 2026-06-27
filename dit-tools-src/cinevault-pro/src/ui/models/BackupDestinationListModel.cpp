#include "ui/models/BackupDestinationListModel.h"

#include "shared/Formatters.h"

BackupDestinationListModel::BackupDestinationListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BackupDestinationListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant BackupDestinationListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    switch (role) {
    case IdRole: return item.id;
    case NameRole: return item.name;
    case RootPathRole: return item.rootPath;
    case PlannedRootPathRole: return item.plannedRootPath;
    case PrimaryRole: return item.primary;
    case AvailableTextRole: return item.availableBytes >= 0 ? Formatters::formatBytes(item.availableBytes) : QStringLiteral("未知容量");
    case StatusTextRole: return item.statusText;
    case WritableRole: return item.writable;
    default: return {};
    }
}

QHash<int, QByteArray> BackupDestinationListModel::roleNames() const
{
    return {
        {IdRole, "destinationId"},
        {NameRole, "name"},
        {RootPathRole, "rootPath"},
        {PlannedRootPathRole, "plannedRootPath"},
        {PrimaryRole, "primary"},
        {AvailableTextRole, "availableText"},
        {StatusTextRole, "statusText"},
        {WritableRole, "writable"}
    };
}

void BackupDestinationListModel::setItems(const QVector<BackupDestination> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

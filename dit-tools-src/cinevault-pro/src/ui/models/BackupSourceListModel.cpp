#include "ui/models/BackupSourceListModel.h"

#include "shared/Formatters.h"

namespace {
QString sourceKindLabel(BackupSourceKind kind)
{
    switch (kind) {
    case BackupSourceKind::File: return QStringLiteral("文件");
    case BackupSourceKind::Volume: return QStringLiteral("磁盘卷");
    case BackupSourceKind::Directory:
    default: return QStringLiteral("文件夹");
    }
}
}

BackupSourceListModel::BackupSourceListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BackupSourceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant BackupSourceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    switch (role) {
    case IdRole: return item.id;
    case NameRole: return item.name;
    case PathRole: return item.path;
    case KindLabelRole: return sourceKindLabel(item.kind);
    case FileCountRole: return item.totalFiles;
    case SizeTextRole: return Formatters::formatBytes(item.totalBytes);
    case StatusTextRole: return item.statusText;
    case ReadableRole: return item.readable;
    default: return {};
    }
}

QHash<int, QByteArray> BackupSourceListModel::roleNames() const
{
    return {
        {IdRole, "sourceId"},
        {NameRole, "name"},
        {PathRole, "path"},
        {KindLabelRole, "kindLabel"},
        {FileCountRole, "fileCount"},
        {SizeTextRole, "sizeText"},
        {StatusTextRole, "statusText"},
        {ReadableRole, "readable"}
    };
}

void BackupSourceListModel::setItems(const QVector<BackupSource> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

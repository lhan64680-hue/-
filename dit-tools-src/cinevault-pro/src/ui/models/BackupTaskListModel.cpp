#include "ui/models/BackupTaskListModel.h"

#include "shared/Formatters.h"

namespace {
QString taskStateLabel(BackupTaskState state)
{
    switch (state) {
    case BackupTaskState::Running: return QStringLiteral("复制中");
    case BackupTaskState::Verifying: return QStringLiteral("校验中");
    case BackupTaskState::Completed: return QStringLiteral("已完成");
    case BackupTaskState::Warning: return QStringLiteral("有警告");
    case BackupTaskState::Failed: return QStringLiteral("失败");
    case BackupTaskState::Cancelled: return QStringLiteral("已取消");
    case BackupTaskState::Pending:
    default: return QStringLiteral("等待中");
    }
}
}

BackupTaskListModel::BackupTaskListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BackupTaskListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant BackupTaskListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    const auto progress = item.totalBytes > 0
        ? static_cast<int>((item.copiedBytes * qint64{100}) / item.totalBytes)
        : (item.state == BackupTaskState::Completed ? 100 : 0);
    switch (role) {
    case DestinationIdRole: return item.destinationId;
    case NameRole: return item.name;
    case PlannedRootPathRole: return item.plannedRootPath;
    case PrimaryRole: return item.primary;
    case StateRole: return static_cast<int>(item.state);
    case StateLabelRole: return taskStateLabel(item.state);
    case ProgressRole: return qBound(0, progress, 100);
    case CopiedTextRole: return QStringLiteral("%1 / %2").arg(Formatters::formatBytes(item.copiedBytes), Formatters::formatBytes(item.totalBytes));
    case SpeedTextRole: return item.bytesPerSecond > 0.0 ? QStringLiteral("%1/s").arg(Formatters::formatBytes(static_cast<qint64>(item.bytesPerSecond))) : QStringLiteral("-");
    case StatusTextRole: return item.statusText;
    case ErrorMessageRole: return item.errorMessage;
    default: return {};
    }
}

QHash<int, QByteArray> BackupTaskListModel::roleNames() const
{
    return {
        {DestinationIdRole, "destinationId"},
        {NameRole, "name"},
        {PlannedRootPathRole, "plannedRootPath"},
        {PrimaryRole, "primary"},
        {StateRole, "state"},
        {StateLabelRole, "stateLabel"},
        {ProgressRole, "progress"},
        {CopiedTextRole, "copiedText"},
        {SpeedTextRole, "speedText"},
        {StatusTextRole, "statusText"},
        {ErrorMessageRole, "errorMessage"}
    };
}

void BackupTaskListModel::setItems(const QVector<BackupDestinationTask> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

void BackupTaskListModel::updateTask(const BackupDestinationTask &task)
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).destinationId != task.destinationId) {
            continue;
        }
        m_items[row] = task;
        const auto modelIndex = index(row, 0);
        emit dataChanged(modelIndex, modelIndex);
        return;
    }

    beginInsertRows({}, m_items.size(), m_items.size());
    m_items.append(task);
    endInsertRows();
}

QVector<BackupDestinationTask> BackupTaskListModel::items() const
{
    return m_items;
}

#include "ui/models/JobListModel.h"

#include "shared/Formatters.h"

JobListModel::JobListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int JobListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant JobListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    switch (role) {
    case IdRole: return item.id;
    case TitleRole: return item.title;
    case DetailRole: return item.detail;
    case ProgressRole: return item.progress;
    case StateRole: return static_cast<int>(item.state);
    case StateLabelRole: return Formatters::jobStateLabel(item.state);
    case SourceRootIdRole: return item.sourceRootId;
    case ErrorRole: return item.errorMessage;
    default: return {};
    }
}

QHash<int, QByteArray> JobListModel::roleNames() const
{
    return {
        {IdRole, "jobId"},
        {TitleRole, "title"},
        {DetailRole, "detail"},
        {ProgressRole, "progress"},
        {StateRole, "state"},
        {StateLabelRole, "stateLabel"},
        {SourceRootIdRole, "sourceRootId"},
        {ErrorRole, "errorMessage"}
    };
}

void JobListModel::setItems(const QVector<Job> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

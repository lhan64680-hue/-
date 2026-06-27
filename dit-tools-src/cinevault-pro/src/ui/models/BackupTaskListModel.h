#pragma once

#include "domain/Entities.h"

#include <QAbstractListModel>

class BackupTaskListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        DestinationIdRole = Qt::UserRole + 1,
        NameRole,
        PlannedRootPathRole,
        PrimaryRole,
        StateRole,
        StateLabelRole,
        ProgressRole,
        CopiedTextRole,
        SpeedTextRole,
        StatusTextRole,
        ErrorMessageRole
    };

    explicit BackupTaskListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QVector<BackupDestinationTask> &items);
    void updateTask(const BackupDestinationTask &task);
    QVector<BackupDestinationTask> items() const;

private:
    QVector<BackupDestinationTask> m_items;
};

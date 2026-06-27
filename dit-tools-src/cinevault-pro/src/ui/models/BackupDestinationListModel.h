#pragma once

#include "domain/Entities.h"

#include <QAbstractListModel>

class BackupDestinationListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        RootPathRole,
        PlannedRootPathRole,
        PrimaryRole,
        AvailableTextRole,
        StatusTextRole,
        WritableRole
    };

    explicit BackupDestinationListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QVector<BackupDestination> &items);

private:
    QVector<BackupDestination> m_items;
};

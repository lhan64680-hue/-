#pragma once

#include "domain/Entities.h"

#include <QAbstractListModel>

class BackupSourceListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        KindLabelRole,
        FileCountRole,
        SizeTextRole,
        StatusTextRole,
        ReadableRole
    };

    explicit BackupSourceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QVector<BackupSource> &items);

private:
    QVector<BackupSource> m_items;
};

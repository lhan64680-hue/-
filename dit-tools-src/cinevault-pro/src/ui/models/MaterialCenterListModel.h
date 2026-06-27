#pragma once

#include "domain/Entities.h"

#include <QAbstractListModel>
#include <QVector>

class MaterialCenterListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        VideoKeyRole = Qt::UserRole + 1,
        FileNameRole,
        ProjectNameRole,
        SourceNameRole,
        RelativePathRole,
        SummaryRole,
        KeywordsRole,
        AnalysisStatusLabelRole,
        ConfirmationStatusRole,
        ConfirmationStatusLabelRole,
        IsConfirmedRole,
        ThumbnailPathRole,
        UpdatedAtRole,
        DurationMsRole,
        ErrorMessageRole
    };

    explicit MaterialCenterListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QVector<GlobalVideoAsset> &items);

private:
    QVector<GlobalVideoAsset> m_items;
};

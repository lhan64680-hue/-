#pragma once

#include <QObject>

class MinimalImportWorkspaceViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString summaryText READ summaryText CONSTANT)

public:
    explicit MinimalImportWorkspaceViewModel(QObject *parent = nullptr);

    QString summaryText() const;
};

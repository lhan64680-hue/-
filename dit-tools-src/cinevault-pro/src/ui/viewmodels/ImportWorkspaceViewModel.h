#pragma once

#include <QObject>

class ImportService;

class ImportWorkspaceViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString summaryText READ summaryText NOTIFY summaryChanged)

public:
    explicit ImportWorkspaceViewModel(ImportService *importService, QObject *parent = nullptr);

    QString summaryText() const;

signals:
    void summaryChanged();

private:
    ImportService *m_importService = nullptr;
};

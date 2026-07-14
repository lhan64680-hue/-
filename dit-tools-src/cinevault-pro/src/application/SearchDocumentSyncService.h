#pragma once

#include "domain/SearchTypes.h"

#include <QObject>

class GlobalDatabaseManager;
class QTimer;
class SemanticSearchIndexService;

class SearchDocumentSyncService : public QObject {
    Q_OBJECT

public:
    explicit SearchDocumentSyncService(GlobalDatabaseManager *globalDatabaseManager,
                                       SemanticSearchIndexService *semanticSearchIndexService,
                                       QObject *parent = nullptr);

    bool synchronizeNow(SemanticIndexUpdateResult *result,
                        QString *errorMessage);

public slots:
    void scheduleFullSync();

signals:
    void synchronizationFinished(bool success,
                                 int inserted,
                                 int updated,
                                 int unchanged,
                                 int removed,
                                 const QString &message);

private:
    void startScheduledSync();

    GlobalDatabaseManager *m_globalDatabaseManager = nullptr;
    SemanticSearchIndexService *m_semanticSearchIndexService = nullptr;
    QTimer *m_debounceTimer = nullptr;
    bool m_running = false;
    bool m_pending = false;
};

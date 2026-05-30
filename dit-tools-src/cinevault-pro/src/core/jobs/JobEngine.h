#pragma once

#include "domain/Entities.h"

#include <QObject>
#include <QVector>

class DatabaseManager;

class JobEngine : public QObject {
    Q_OBJECT

public:
    explicit JobEngine(DatabaseManager *databaseManager, QObject *parent = nullptr);

    qint64 createJob(JobType type, const QString &title, const QString &detail, qint64 sourceRootId = 0);
    qint64 queueJob(JobType type, const QString &title, const QString &detail, qint64 sourceRootId = 0);
    void updateJob(qint64 jobId, qint64 progress, const QString &detail);
    void completeJob(qint64 jobId, const QString &detail);
    void failJob(qint64 jobId, const QString &errorMessage);

    QVector<Job> jobs() const;

signals:
    void jobsChanged();

private:
    void persistJob(const Job &job);
    Job *findJob(qint64 jobId);
    qint64 appendJob(JobType type, JobState state, const QString &title, const QString &detail, qint64 sourceRootId);

    DatabaseManager *m_databaseManager = nullptr;
    QVector<Job> m_jobs;
    qint64 m_nextId = 1;
};

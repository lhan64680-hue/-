#include "core/jobs/JobEngine.h"

#include "infrastructure/db/DatabaseManager.h"

#include <QDateTime>
#include <QSqlQuery>

JobEngine::JobEngine(DatabaseManager *databaseManager, QObject *parent)
    : QObject(parent)
    , m_databaseManager(databaseManager)
{
}

qint64 JobEngine::createJob(JobType type, const QString &title, const QString &detail, qint64 sourceRootId)
{
    Job job;
    job.id = m_nextId++;
    job.type = type;
    job.state = JobState::Running;
    job.title = title;
    job.detail = detail;
    job.progress = 0;
    job.sourceRootId = sourceRootId;
    job.startedAt = QDateTime::currentDateTime();
    job.updatedAt = job.startedAt;
    m_jobs.prepend(job);
    persistJob(job);
    emit jobsChanged();
    return job.id;
}

void JobEngine::updateJob(qint64 jobId, qint64 progress, const QString &detail)
{
    if (auto *job = findJob(jobId)) {
        job->progress = progress;
        job->detail = detail;
        job->updatedAt = QDateTime::currentDateTime();
        persistJob(*job);
        emit jobsChanged();
    }
}

void JobEngine::completeJob(qint64 jobId, const QString &detail)
{
    if (auto *job = findJob(jobId)) {
        job->state = JobState::Completed;
        job->progress = 100;
        job->detail = detail;
        job->updatedAt = QDateTime::currentDateTime();
        persistJob(*job);
        emit jobsChanged();
    }
}

void JobEngine::failJob(qint64 jobId, const QString &errorMessage)
{
    if (auto *job = findJob(jobId)) {
        job->state = JobState::Failed;
        job->errorMessage = errorMessage;
        job->detail = errorMessage;
        job->updatedAt = QDateTime::currentDateTime();
        persistJob(*job);
        emit jobsChanged();
    }
}

QVector<Job> JobEngine::jobs() const
{
    return m_jobs;
}

void JobEngine::persistJob(const Job &job)
{
    if (!m_databaseManager || !m_databaseManager->hasOpenProject()) {
        return;
    }

    QSqlQuery query(m_databaseManager->database());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO job (id, type, state, title, detail, error_message, progress, source_root_id, started_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(job.id);
    query.addBindValue(static_cast<int>(job.type));
    query.addBindValue(static_cast<int>(job.state));
    query.addBindValue(job.title);
    query.addBindValue(job.detail);
    query.addBindValue(job.errorMessage);
    query.addBindValue(job.progress);
    query.addBindValue(job.sourceRootId);
    query.addBindValue(job.startedAt.toString(Qt::ISODate));
    query.addBindValue(job.updatedAt.toString(Qt::ISODate));
    query.exec();
}

Job *JobEngine::findJob(qint64 jobId)
{
    for (auto &job : m_jobs) {
        if (job.id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

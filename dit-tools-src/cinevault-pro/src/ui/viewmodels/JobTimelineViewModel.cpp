#include "ui/viewmodels/JobTimelineViewModel.h"

#include "application/JobService.h"
#include "application/VideoAnalysisService.h"
#include "core/jobs/JobEngine.h"
#include "shared/Formatters.h"
#include "ui/models/JobListModel.h"

JobTimelineViewModel::JobTimelineViewModel(JobService *jobService, VideoAnalysisService *videoAnalysisService, QObject *parent)
    : QObject(parent)
    , m_jobService(jobService)
    , m_videoAnalysisService(videoAnalysisService)
    , m_model(new JobListModel(this))
{
    connect(m_jobService->engine(), &JobEngine::jobsChanged, this, &JobTimelineViewModel::reload);
    if (m_videoAnalysisService) {
        connect(m_videoAnalysisService, &VideoAnalysisService::analysisBatchChanged, this, &JobTimelineViewModel::stateChanged);
    }
}

JobListModel *JobTimelineViewModel::model() const
{
    return m_model;
}

QVariantList JobTimelineViewModel::timelineItems() const
{
    return m_timelineItems;
}

bool JobTimelineViewModel::hasBatchSummary() const
{
    return m_videoAnalysisService && m_videoAnalysisService->hasBatchSummary();
}

QString JobTimelineViewModel::batchTitle() const
{
    return QStringLiteral("视频解析总量进度");
}

QString JobTimelineViewModel::batchProgressText() const
{
    if (!hasBatchSummary()) {
        return QStringLiteral("暂无批次");
    }
    return QStringLiteral("%1/%2 · %3%")
        .arg(batchFinishedCount())
        .arg(batchTotalCount())
        .arg(batchProgress());
}

int JobTimelineViewModel::batchProgress() const
{
    return m_videoAnalysisService ? static_cast<int>(m_videoAnalysisService->batchProgressPercent()) : 0;
}

QString JobTimelineViewModel::batchStatusText() const
{
    return m_videoAnalysisService ? m_videoAnalysisService->batchStatusText() : QStringLiteral("暂无视频解析批次。");
}

QString JobTimelineViewModel::batchCurrentLabel() const
{
    return m_videoAnalysisService ? m_videoAnalysisService->batchCurrentLabel() : QString();
}

QString JobTimelineViewModel::batchCurrentDetail() const
{
    return m_videoAnalysisService
        ? m_videoAnalysisService->batchCurrentDetail()
        : QStringLiteral("当前没有正在处理的素材。");
}

int JobTimelineViewModel::batchCurrentProgress() const
{
    return m_videoAnalysisService ? static_cast<int>(m_videoAnalysisService->batchCurrentProgressPercent()) : 0;
}

bool JobTimelineViewModel::hasActiveBatchItem() const
{
    return m_videoAnalysisService && !m_videoAnalysisService->batchCurrentLabel().trimmed().isEmpty();
}

int JobTimelineViewModel::batchFinishedCount() const
{
    return m_videoAnalysisService ? m_videoAnalysisService->batchFinishedCount() : 0;
}

int JobTimelineViewModel::batchSuccessfulCount() const
{
    return m_videoAnalysisService ? m_videoAnalysisService->batchSuccessfulCount() : 0;
}

int JobTimelineViewModel::batchFailedCount() const
{
    return m_videoAnalysisService ? m_videoAnalysisService->batchFailedCount() : 0;
}

int JobTimelineViewModel::batchTotalCount() const
{
    return m_videoAnalysisService ? m_videoAnalysisService->batchTotalCount() : 0;
}

int JobTimelineViewModel::batchQueuedCount() const
{
    return m_videoAnalysisService ? m_videoAnalysisService->batchQueuedCount() : 0;
}

bool JobTimelineViewModel::hasSelection() const
{
    return selectedJob() != nullptr;
}

qint64 JobTimelineViewModel::selectedJobId() const
{
    return m_selectedJobId;
}

QString JobTimelineViewModel::selectedJobTitle() const
{
    const auto *job = selectedJob();
    return job ? job->title : QString();
}

QString JobTimelineViewModel::selectedJobDetail() const
{
    const auto *job = selectedJob();
    return job ? job->detail : QStringLiteral("选择左侧任务查看详情。");
}

QString JobTimelineViewModel::selectedJobStateLabel() const
{
    const auto *job = selectedJob();
    return job ? Formatters::jobStateLabel(job->state) : QString();
}

QString JobTimelineViewModel::selectedJobError() const
{
    const auto *job = selectedJob();
    return job ? job->errorMessage : QString();
}

int JobTimelineViewModel::selectedJobProgress() const
{
    const auto *job = selectedJob();
    return job ? static_cast<int>(job->progress) : 0;
}

QString JobTimelineViewModel::selectedJobUpdatedAt() const
{
    const auto *job = selectedJob();
    return job ? job->updatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
}

void JobTimelineViewModel::reload()
{
    const auto jobs = m_jobService->engine()->jobs();
    m_jobs = jobs;
    m_model->setItems(jobs);

    QVariantList items;
    for (int i = 0; i < jobs.size() && i < 5; ++i) {
        QVariantMap row;
        row.insert(QStringLiteral("title"), jobs.at(i).title);
        row.insert(QStringLiteral("detail"), jobs.at(i).detail);
        row.insert(QStringLiteral("progress"), jobs.at(i).progress);
        row.insert(QStringLiteral("stateLabel"), Formatters::jobStateLabel(jobs.at(i).state));
        items.append(row);
    }
    m_timelineItems = items;
    if (!m_jobs.isEmpty()) {
        bool found = false;
        for (const auto &job : m_jobs) {
            if (job.id == m_selectedJobId) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_selectedJobId = m_jobs.first().id;
        }
    } else {
        m_selectedJobId = 0;
    }
    emit timelineChanged();
    emit stateChanged();
}

void JobTimelineViewModel::selectJob(qint64 jobId)
{
    if (m_selectedJobId == jobId) {
        return;
    }
    m_selectedJobId = jobId;
    emit stateChanged();
}

const Job *JobTimelineViewModel::selectedJob() const
{
    for (const auto &job : m_jobs) {
        if (job.id == m_selectedJobId) {
            return &job;
        }
    }
    return nullptr;
}

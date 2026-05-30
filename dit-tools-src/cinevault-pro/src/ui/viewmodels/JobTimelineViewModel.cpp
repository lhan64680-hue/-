#include "ui/viewmodels/JobTimelineViewModel.h"

#include "application/JobService.h"
#include "core/jobs/JobEngine.h"
#include "shared/Formatters.h"
#include "ui/models/JobListModel.h"

JobTimelineViewModel::JobTimelineViewModel(JobService *jobService, QObject *parent)
    : QObject(parent)
    , m_jobService(jobService)
    , m_model(new JobListModel(this))
{
    connect(m_jobService->engine(), &JobEngine::jobsChanged, this, &JobTimelineViewModel::reload);
}

JobListModel *JobTimelineViewModel::model() const
{
    return m_model;
}

QVariantList JobTimelineViewModel::timelineItems() const
{
    return m_timelineItems;
}

void JobTimelineViewModel::reload()
{
    const auto jobs = m_jobService->engine()->jobs();
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
    emit timelineChanged();
}

#pragma once

#include <QObject>
#include <QVariantList>
#include <QVector>

#include "domain/Entities.h"

class JobListModel;

class MinimalJobTimelineViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(JobListModel* model READ model CONSTANT)
    Q_PROPERTY(QVariantList timelineItems READ timelineItems NOTIFY timelineChanged)

public:
    explicit MinimalJobTimelineViewModel(QObject *parent = nullptr);

    JobListModel *model() const;
    QVariantList timelineItems() const;

public slots:
    void reload();

signals:
    void timelineChanged();

private:
    void seedJobs();

    JobListModel *m_model = nullptr;
    QVector<Job> m_jobs;
    QVariantList m_timelineItems;
};

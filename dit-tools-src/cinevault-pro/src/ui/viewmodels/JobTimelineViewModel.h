#pragma once

#include <QObject>
#include <QVariantList>

class JobListModel;
class JobService;

class JobTimelineViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(JobListModel* model READ model CONSTANT)
    Q_PROPERTY(QVariantList timelineItems READ timelineItems NOTIFY timelineChanged)

public:
    explicit JobTimelineViewModel(JobService *jobService, QObject *parent = nullptr);

    JobListModel *model() const;
    QVariantList timelineItems() const;

public slots:
    void reload();

signals:
    void timelineChanged();

private:
    JobService *m_jobService = nullptr;
    JobListModel *m_model = nullptr;
    QVariantList m_timelineItems;
};

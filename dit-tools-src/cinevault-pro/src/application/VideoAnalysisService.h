#pragma once

#include "domain/Entities.h"

#include <QObject>
#include <QQueue>
#include <QSet>
#include <QStringList>

class AppSettings;
class FFmpegAdapter;
class GlobalDatabaseManager;
class JobEngine;
class VisionApiClient;

class VideoAnalysisService : public QObject {
    Q_OBJECT

public:
    explicit VideoAnalysisService(GlobalDatabaseManager *globalDatabaseManager,
                                  JobEngine *jobEngine,
                                  AppSettings *settings,
                                  FFmpegAdapter *ffmpegAdapter,
                                  VisionApiClient *visionApiClient,
                                  QObject *parent = nullptr);

    bool enqueueVideo(const QString &videoKey, QString *errorMessage = nullptr);
    int enqueueVideos(const QStringList &videoKeys, QString *errorMessage = nullptr);

public slots:
    void analyzeVideo(const QString &videoKey);
    bool confirmVideo(const QString &videoKey);
    int confirmVideos(const QStringList &videoKeys);

signals:
    void catalogChanged();
    void analysisProgressChanged(const QString &videoKey,
                                 qint64 progress,
                                 const QString &detail,
                                 int state,
                                 const QString &errorMessage);
    void analysisQueueChanged(const QString &currentVideoKey, int queuedCount);

private:
    bool validateReadyForEnqueue(const QString &videoKey, QString *errorMessage) const;
    void startNextAnalysis();
    void finishCurrentAnalysis(const QString &videoKey);
    void reportAnalysisProgress(const QString &videoKey,
                                qint64 progress,
                                const QString &detail,
                                JobState state,
                                const QString &errorMessage = QString());
    void updateJob(qint64 jobId, qint64 progress, const QString &detail);
    void completeJob(qint64 jobId, const QString &detail);
    void failJob(qint64 jobId, const QString &errorMessage);
    void notifyCatalogChanged();

    GlobalDatabaseManager *m_globalDatabaseManager = nullptr;
    JobEngine *m_jobEngine = nullptr;
    AppSettings *m_settings = nullptr;
    FFmpegAdapter *m_ffmpegAdapter = nullptr;
    VisionApiClient *m_visionApiClient = nullptr;
    QQueue<QString> m_analysisQueue;
    QSet<QString> m_queuedVideoKeys;
    QString m_currentVideoKey;
    bool m_analysisRunning = false;
};

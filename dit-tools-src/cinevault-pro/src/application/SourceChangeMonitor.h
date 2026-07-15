#pragma once

#include "domain/Entities.h"

#include <QHash>
#include <QObject>

#include <memory>
#include <vector>

class SourceChangeMonitor final : public QObject {
    Q_OBJECT

public:
    explicit SourceChangeMonitor(QObject *parent = nullptr);
    ~SourceChangeMonitor() override;

    void setSourceRoots(const QVector<SourceRoot> &sourceRoots);
    void stop();
    [[nodiscard]] int watchedSourceCount() const;

signals:
    void sourceChanged(qint64 sourceRootId, const QString &sourcePath);
    void sourceUnavailable(qint64 sourceRootId,
                           const QString &sourcePath,
                           const QString &message);

private:
    struct WatchRegistration;

    void postChange(qint64 sourceRootId, const QString &sourcePath);
    void postUnavailable(qint64 sourceRootId,
                         const QString &sourcePath,
                         const QString &message);

    std::vector<std::unique_ptr<WatchRegistration>> m_watches;
    QHash<qint64, qint64> m_lastNotificationMs;
};

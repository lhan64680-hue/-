#pragma once

#include "domain/Entities.h"

#include <QDateTime>

class BackupPlanner {
public:
    BackupPlan buildPlan(const BackupRequest &request) const;

    static QString defaultBatchName(const QString &projectName, const QDateTime &timestamp = QDateTime::currentDateTime());
    static QString safePathSegment(const QString &name);
};

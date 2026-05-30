#pragma once

#include "domain/Entities.h"

#include <QString>

class Formatters {
public:
    static QString formatBytes(qint64 bytes);
    static QString assetTypeLabel(AssetType type);
    static QString statusLabel(const QString &status);
    static QString statusColor(const QString &status);
    static QString jobStateLabel(JobState state);
    static QString workspaceLabel(WorkspaceId workspaceId);
};

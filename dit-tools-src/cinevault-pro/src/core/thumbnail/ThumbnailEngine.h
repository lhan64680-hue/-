#pragma once

#include "domain/Entities.h"

#include <QObject>

class FFmpegAdapter;

class ThumbnailEngine : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailEngine(FFmpegAdapter *adapter, QObject *parent = nullptr);

    bool isAvailable() const;
    QString statusMessage() const;
    ThumbnailResult createPlaceholder(const ThumbnailRequest &request) const;

private:
    FFmpegAdapter *m_adapter = nullptr;
};

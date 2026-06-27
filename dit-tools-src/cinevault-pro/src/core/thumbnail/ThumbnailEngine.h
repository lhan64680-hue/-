#pragma once

#include "domain/Entities.h"

#include <QObject>

class FFmpegAdapter;
class AppSettings;

class ThumbnailEngine : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailEngine(FFmpegAdapter *adapter, AppSettings *settings, QObject *parent = nullptr);

    bool isAvailable() const;
    QString statusMessage() const;
    ThumbnailResult createPlaceholder(const ThumbnailRequest &request) const;

private:
    FFmpegAdapter *m_adapter = nullptr;
    AppSettings *m_settings = nullptr;
};

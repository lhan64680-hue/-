#pragma once

#include "domain/Entities.h"

#include <QString>

class FFmpegAdapter {
public:
    FFmpegAdapter();

    bool isAvailable() const;
    QString unavailableReason() const;
    MediaProbeResult probe(const AssetFile &asset) const;
    ThumbnailResult generateThumbnail(const ThumbnailRequest &request) const;

private:
    bool m_available = false;
    QString m_unavailableReason;
};

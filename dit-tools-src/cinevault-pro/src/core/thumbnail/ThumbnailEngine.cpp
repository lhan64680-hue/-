#include "core/thumbnail/ThumbnailEngine.h"

#include "infrastructure/ffmpeg/FFmpegAdapter.h"

ThumbnailEngine::ThumbnailEngine(FFmpegAdapter *adapter, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
{
}

bool ThumbnailEngine::isAvailable() const
{
    return m_adapter && m_adapter->isAvailable();
}

QString ThumbnailEngine::statusMessage() const
{
    return isAvailable()
        ? QStringLiteral("缩略图模块可用")
        : (m_adapter ? m_adapter->unavailableReason() : QStringLiteral("缩略图模块未初始化"));
}

ThumbnailResult ThumbnailEngine::createPlaceholder(const ThumbnailRequest &request) const
{
    if (!m_adapter) {
        ThumbnailResult result;
        result.assetId = request.assetId;
        result.success = false;
        result.errorMessage = QStringLiteral("缩略图模块未初始化");
        return result;
    }
    return m_adapter->generateThumbnail(request);
}

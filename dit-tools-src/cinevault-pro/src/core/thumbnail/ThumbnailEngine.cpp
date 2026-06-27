#include "core/thumbnail/ThumbnailEngine.h"

#include "infrastructure/config/AppSettings.h"
#include "infrastructure/ffmpeg/FFmpegAdapter.h"

ThumbnailEngine::ThumbnailEngine(FFmpegAdapter *adapter, AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
    , m_settings(settings)
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
    auto normalizedRequest = request;
    if (normalizedRequest.assetType == AssetType::Image) {
        normalizedRequest.frameIndex = 1;
    } else if (m_settings) {
        normalizedRequest.frameIndex = m_settings->thumbnailFrameIndex();
    }
    return m_adapter->generateThumbnail(normalizedRequest);
}

#include "infrastructure/ffmpeg/FFmpegAdapter.h"

FFmpegAdapter::FFmpegAdapter()
{
#if CINEVAULT_HAS_FFMPEG
    m_available = true;
#else
    m_available = false;
    m_unavailableReason = QStringLiteral("未启用 FFmpeg 开发包，媒体模块仅保留占位链路。");
#endif
}

bool FFmpegAdapter::isAvailable() const
{
    return m_available;
}

QString FFmpegAdapter::unavailableReason() const
{
    return m_unavailableReason;
}

MediaProbeResult FFmpegAdapter::probe(const AssetFile &asset) const
{
    MediaProbeResult result;
    result.assetId = asset.id;
    result.mediaType = asset.assetType == AssetType::Video
        ? MediaType::Video
        : (asset.assetType == AssetType::Audio ? MediaType::Audio : MediaType::Image);

#if CINEVAULT_HAS_FFMPEG
    result.status = ProbeStatus::Unsupported;
    result.errorMessage = QStringLiteral("FFmpeg 已接入编译链路，但本阶段尚未实现真实探测逻辑。");
#else
    result.status = ProbeStatus::Unavailable;
    result.errorMessage = m_unavailableReason;
#endif

    return result;
}

ThumbnailResult FFmpegAdapter::generateThumbnail(const ThumbnailRequest &request) const
{
    ThumbnailResult result;
    result.assetId = request.assetId;

#if CINEVAULT_HAS_FFMPEG
    result.success = false;
    result.errorMessage = QStringLiteral("FFmpeg 已接入编译链路，但本阶段尚未实现真实缩略图逻辑。");
#else
    result.success = false;
    result.errorMessage = m_unavailableReason;
#endif

    return result;
}

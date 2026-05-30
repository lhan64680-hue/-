#include "shared/Formatters.h"

QString Formatters::formatBytes(qint64 bytes)
{
    static const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB"), QStringLiteral("TB")};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < units.size() - 1) {
        value /= 1024.0;
        ++unitIndex;
    }
    return QString::number(value, unitIndex == 0 ? 'f' : 'f', unitIndex == 0 ? 0 : 1) + units.at(unitIndex);
}

QString Formatters::assetTypeLabel(AssetType type)
{
    switch (type) {
    case AssetType::Video: return QStringLiteral("视频");
    case AssetType::Audio: return QStringLiteral("音频");
    case AssetType::Image: return QStringLiteral("图片");
    case AssetType::Subtitle: return QStringLiteral("字幕");
    case AssetType::ProjectFile: return QStringLiteral("工程");
    case AssetType::Document: return QStringLiteral("文档");
    case AssetType::Archive: return QStringLiteral("压缩包");
    case AssetType::Other: return QStringLiteral("其他");
    case AssetType::Unknown:
    default: return QStringLiteral("未知");
    }
}

QString Formatters::statusLabel(const QString &status)
{
    if (status == QStringLiteral("scanning")) return QStringLiteral("正在扫描");
    if (status == QStringLiteral("ok")) return QStringLiteral("正常");
    if (status == QStringLiteral("warning")) return QStringLiteral("有警告");
    if (status == QStringLiteral("failed")) return QStringLiteral("失败");
    if (status == QStringLiteral("offline")) return QStringLiteral("离线");
    return QStringLiteral("待处理");
}

QString Formatters::statusColor(const QString &status)
{
    if (status == QStringLiteral("scanning")) return QStringLiteral("#4F8CFF");
    if (status == QStringLiteral("ok")) return QStringLiteral("#22C55E");
    if (status == QStringLiteral("warning")) return QStringLiteral("#F59E0B");
    if (status == QStringLiteral("failed")) return QStringLiteral("#EF4444");
    if (status == QStringLiteral("offline")) return QStringLiteral("#626D7D");
    return QStringLiteral("#9AA4B2");
}

QString Formatters::jobStateLabel(JobState state)
{
    switch (state) {
    case JobState::Pending: return QStringLiteral("等待中");
    case JobState::Running: return QStringLiteral("进行中");
    case JobState::Completed: return QStringLiteral("已完成");
    case JobState::Failed: return QStringLiteral("失败");
    case JobState::Cancelled: return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}

QString Formatters::workspaceLabel(WorkspaceId workspaceId)
{
    switch (workspaceId) {
    case WorkspaceId::Import: return QStringLiteral("导入");
    case WorkspaceId::Library: return QStringLiteral("素材库");
    case WorkspaceId::Qc: return QStringLiteral("检查/质检");
    case WorkspaceId::Report: return QStringLiteral("报表");
    case WorkspaceId::Jobs: return QStringLiteral("任务");
    }
    return QStringLiteral("导入");
}

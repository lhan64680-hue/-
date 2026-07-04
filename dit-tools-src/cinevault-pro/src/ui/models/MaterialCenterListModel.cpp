#include "ui/models/MaterialCenterListModel.h"

#include "shared/Formatters.h"

#include <QSet>

namespace {
bool isSupportedTextAsset(AssetType assetType, const QString &extension)
{
    static const QSet<QString> textExtensions = {
        QStringLiteral("txt"), QStringLiteral("log"), QStringLiteral("md"),
        QStringLiteral("json"), QStringLiteral("csv"), QStringLiteral("tsv"),
        QStringLiteral("xml"), QStringLiteral("yaml"), QStringLiteral("yml"),
        QStringLiteral("docx"), QStringLiteral("xlsx"), QStringLiteral("pptx"),
        QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("vtt")
    };
    const auto normalizedExtension = extension.trimmed().toLower();
    return assetType == AssetType::Subtitle
        || (assetType == AssetType::Document && textExtensions.contains(normalizedExtension));
}

bool canAnalyzeAsset(const GlobalVideoAsset &asset)
{
    return asset.assetType == AssetType::Video
        || asset.assetType == AssetType::Image
        || isSupportedTextAsset(asset.assetType, asset.extension);
}
}

MaterialCenterListModel::MaterialCenterListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MaterialCenterListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant MaterialCenterListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    switch (role) {
    case VideoKeyRole: return item.videoKey;
    case AssetKeyRole: return item.assetKey.trimmed().isEmpty() ? item.videoKey : item.assetKey;
    case FileNameRole: return item.fileName;
    case AssetTypeRole: return static_cast<int>(item.assetType);
    case AssetTypeLabelRole: return Formatters::assetTypeLabel(item.assetType);
    case ExtensionRole: return item.extension;
    case ProjectNameRole: return item.projectName;
    case SourceNameRole: return item.sourceRootName;
    case RelativePathRole: return item.relativePath;
    case SummaryRole: return item.summary;
    case KeywordsRole: return item.keywords.join(QStringLiteral("、"));
    case AnalysisStatusLabelRole: return Formatters::videoAnalysisStatusLabel(item.analysisStatus, item.confirmationStatus);
    case ConfirmationStatusRole: return static_cast<int>(item.confirmationStatus);
    case ConfirmationStatusLabelRole: return Formatters::confirmationStatusLabel(item.confirmationStatus);
    case IsConfirmedRole: return item.confirmationStatus == ConfirmationStatus::Confirmed;
    case ThumbnailPathRole: return item.thumbnailPath;
    case ThumbnailStatusRole: return static_cast<int>(item.thumbnailStatus);
    case ThumbnailLoadingRole: return item.thumbnailStatus == ThumbnailStatus::Running && item.thumbnailPath.isEmpty();
    case UpdatedAtRole: return item.updatedAt;
    case DurationMsRole: return item.durationMs;
    case ErrorMessageRole: return item.errorMessage;
    case CanAnalyzeRole: return canAnalyzeAsset(item);
    case CanConfirmRole: return item.analysisStatus == VideoAnalysisStatus::Ready
            && item.confirmationStatus != ConfirmationStatus::Confirmed;
    default: return {};
    }
}

QHash<int, QByteArray> MaterialCenterListModel::roleNames() const
{
    return {
        {VideoKeyRole, "videoKey"},
        {AssetKeyRole, "assetKey"},
        {FileNameRole, "fileName"},
        {AssetTypeRole, "assetType"},
        {AssetTypeLabelRole, "assetTypeLabel"},
        {ExtensionRole, "extension"},
        {ProjectNameRole, "projectName"},
        {SourceNameRole, "sourceName"},
        {RelativePathRole, "relativePath"},
        {SummaryRole, "summary"},
        {KeywordsRole, "keywords"},
        {AnalysisStatusLabelRole, "analysisStatusLabel"},
        {ConfirmationStatusRole, "confirmationStatus"},
        {ConfirmationStatusLabelRole, "confirmationStatusLabel"},
        {IsConfirmedRole, "isConfirmed"},
        {ThumbnailPathRole, "thumbnailPath"},
        {ThumbnailStatusRole, "thumbnailStatus"},
        {ThumbnailLoadingRole, "thumbnailLoading"},
        {UpdatedAtRole, "updatedAt"},
        {DurationMsRole, "durationMs"},
        {ErrorMessageRole, "errorMessage"},
        {CanAnalyzeRole, "canAnalyze"},
        {CanConfirmRole, "canConfirm"}
    };
}

void MaterialCenterListModel::setItems(const QVector<GlobalVideoAsset> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

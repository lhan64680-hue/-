#include "ui/models/MaterialCenterListModel.h"

#include "shared/Formatters.h"

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
    case FileNameRole: return item.fileName;
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
    default: return {};
    }
}

QHash<int, QByteArray> MaterialCenterListModel::roleNames() const
{
    return {
        {VideoKeyRole, "videoKey"},
        {FileNameRole, "fileName"},
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
        {ErrorMessageRole, "errorMessage"}
    };
}

void MaterialCenterListModel::setItems(const QVector<GlobalVideoAsset> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

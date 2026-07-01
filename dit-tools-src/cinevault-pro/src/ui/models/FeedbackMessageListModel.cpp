#include "ui/models/FeedbackMessageListModel.h"

#include "shared/Formatters.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSet>
#include <QUrl>
#include <QVariantList>

namespace {
QString senderLabel(const QString &role)
{
    return role == QStringLiteral("admin")
        ? QStringLiteral("开发者")
        : QStringLiteral("我");
}

QString timestampLabel(const QString &value)
{
    auto dateTime = QDateTime::fromString(value, Qt::ISODate);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    }
    if (!dateTime.isValid()) {
        return value;
    }
    return dateTime.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"));
}

QString attachmentMimeType(const FeedbackAttachment &attachment)
{
    const auto normalized = attachment.mimeType.trimmed().toLower();
    if (!normalized.isEmpty()) {
        return normalized;
    }

    QMimeDatabase database;
    const auto guessed = database.mimeTypeForFile(attachment.name, QMimeDatabase::MatchExtension);
    return guessed.name().trimmed().toLower();
}

QString attachmentExtension(const FeedbackAttachment &attachment)
{
    const auto fromName = QFileInfo(attachment.name.trimmed()).suffix().trimmed().toLower();
    if (!fromName.isEmpty()) {
        return fromName;
    }
    return QFileInfo(QUrl(attachment.url).path()).suffix().trimmed().toLower();
}

bool isImageAttachment(const FeedbackAttachment &attachment)
{
    static const QSet<QString> kImageExtensions = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("bmp"),
        QStringLiteral("gif"),
        QStringLiteral("webp"),
        QStringLiteral("tif"),
        QStringLiteral("tiff")
    };

    const auto mimeType = attachmentMimeType(attachment);
    return mimeType.startsWith(QStringLiteral("image/")) || kImageExtensions.contains(attachmentExtension(attachment));
}

bool isVideoAttachment(const FeedbackAttachment &attachment)
{
    static const QSet<QString> kVideoExtensions = {
        QStringLiteral("mp4"),
        QStringLiteral("mov"),
        QStringLiteral("m4v"),
        QStringLiteral("avi"),
        QStringLiteral("mkv"),
        QStringLiteral("webm"),
        QStringLiteral("wmv"),
        QStringLiteral("mpeg"),
        QStringLiteral("mpg")
    };

    const auto mimeType = attachmentMimeType(attachment);
    return mimeType.startsWith(QStringLiteral("video/")) || kVideoExtensions.contains(attachmentExtension(attachment));
}

bool isDocumentAttachment(const FeedbackAttachment &attachment)
{
    static const QSet<QString> kDocumentExtensions = {
        QStringLiteral("pdf"),
        QStringLiteral("md"),
        QStringLiteral("txt"),
        QStringLiteral("log"),
        QStringLiteral("xml"),
        QStringLiteral("yaml"),
        QStringLiteral("yml"),
        QStringLiteral("json"),
        QStringLiteral("csv"),
        QStringLiteral("tsv"),
        QStringLiteral("docx"),
        QStringLiteral("xlsx"),
        QStringLiteral("pptx"),
        QStringLiteral("doc"),
        QStringLiteral("xls"),
        QStringLiteral("ppt")
    };

    const auto mimeType = attachmentMimeType(attachment);
    return mimeType.startsWith(QStringLiteral("text/")) || kDocumentExtensions.contains(attachmentExtension(attachment));
}

QString previewKindForAttachment(const FeedbackAttachment &attachment)
{
    if (isImageAttachment(attachment)) {
        return QStringLiteral("image");
    }
    if (isVideoAttachment(attachment)) {
        return QStringLiteral("video");
    }
    if (isDocumentAttachment(attachment)) {
        return QStringLiteral("document");
    }
    return {};
}

QVariantList attachmentList(const QVector<FeedbackAttachment> &attachments)
{
    QVariantList items;
    items.reserve(attachments.size());
    for (const auto &attachment : attachments) {
        const auto previewKind = previewKindForAttachment(attachment);
        QVariantMap row;
        row.insert(QStringLiteral("id"), attachment.id);
        row.insert(QStringLiteral("name"), attachment.name);
        row.insert(QStringLiteral("mimeType"), attachment.mimeType);
        row.insert(QStringLiteral("url"), attachment.url);
        row.insert(QStringLiteral("sizeBytes"), attachment.sizeBytes);
        row.insert(QStringLiteral("sizeLabel"), Formatters::formatBytes(attachment.sizeBytes));
        row.insert(QStringLiteral("previewKind"), previewKind);
        row.insert(QStringLiteral("isImage"), previewKind == QStringLiteral("image"));
        row.insert(QStringLiteral("isVideo"), previewKind == QStringLiteral("video"));
        row.insert(QStringLiteral("isPreviewable"), !previewKind.isEmpty());
        row.insert(QStringLiteral("canDownload"), !attachment.url.trimmed().isEmpty());
        items.append(row);
    }
    return items;
}
}

FeedbackMessageListModel::FeedbackMessageListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FeedbackMessageListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant FeedbackMessageListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    switch (role) {
    case IdRole: return item.id;
    case SenderRole: return item.senderRole;
    case SenderLabelRole: return senderLabel(item.senderRole);
    case OutgoingRole: return item.senderRole == QStringLiteral("client");
    case TextRole: return item.text;
    case CreatedAtRole: return item.createdAt;
    case CreatedAtLabelRole: return timestampLabel(item.createdAt);
    case AttachmentsRole: return attachmentList(item.attachments);
    case HasTextRole: return !item.text.trimmed().isEmpty();
    case HasAttachmentsRole: return !item.attachments.isEmpty();
    default: return {};
    }
}

QHash<int, QByteArray> FeedbackMessageListModel::roleNames() const
{
    return {
        {IdRole, "messageId"},
        {SenderRole, "senderRole"},
        {SenderLabelRole, "senderLabel"},
        {OutgoingRole, "outgoing"},
        {TextRole, "text"},
        {CreatedAtRole, "createdAt"},
        {CreatedAtLabelRole, "createdAtLabel"},
        {AttachmentsRole, "attachments"},
        {HasTextRole, "hasText"},
        {HasAttachmentsRole, "hasAttachments"}
    };
}

void FeedbackMessageListModel::setItems(const QVector<FeedbackMessage> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

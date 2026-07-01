#include "ui/models/FeedbackMessageListModel.h"

#include "shared/Formatters.h"

#include <QDateTime>
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

QVariantList attachmentList(const QVector<FeedbackAttachment> &attachments)
{
    QVariantList items;
    items.reserve(attachments.size());
    for (const auto &attachment : attachments) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), attachment.id);
        row.insert(QStringLiteral("name"), attachment.name);
        row.insert(QStringLiteral("mimeType"), attachment.mimeType);
        row.insert(QStringLiteral("url"), attachment.url);
        row.insert(QStringLiteral("sizeBytes"), attachment.sizeBytes);
        row.insert(QStringLiteral("sizeLabel"), Formatters::formatBytes(attachment.sizeBytes));
        row.insert(QStringLiteral("isImage"), attachment.mimeType.startsWith(QStringLiteral("image/")));
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

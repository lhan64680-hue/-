#include "ui/viewmodels/FeedbackViewModel.h"

#include "application/FeedbackService.h"
#include "shared/Formatters.h"
#include "ui/models/FeedbackMessageListModel.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>

namespace {
QString displayTime(const QString &value)
{
    auto dateTime = QDateTime::fromString(value, Qt::ISODate);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    }
    if (!dateTime.isValid()) {
        return QStringLiteral("暂无");
    }
    return dateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}
}

FeedbackViewModel::FeedbackViewModel(FeedbackService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_messageModel(new FeedbackMessageListModel(this))
{
    if (!m_service) {
        return;
    }

    connect(m_service, &FeedbackService::messagesChanged, this, [this]() {
        rebuildMessages();
        emit stateChanged();
    });
    connect(m_service, &FeedbackService::stateChanged, this, &FeedbackViewModel::stateChanged);
    connect(m_service, &FeedbackService::messageSubmitted, this, [this](bool success) {
        if (success) {
            m_pendingAttachments.clear();
            emit stateChanged();
        }
        emit messageSubmitted(success);
    });

    rebuildMessages();
}

QObject *FeedbackViewModel::messageModel() const
{
    return m_messageModel;
}

bool FeedbackViewModel::needsProfile() const
{
    return !m_service || !m_service->hasProfile();
}

bool FeedbackViewModel::ready() const
{
    return m_service && m_service->ready();
}

bool FeedbackViewModel::busy() const
{
    return m_service && m_service->busy();
}

bool FeedbackViewModel::sending() const
{
    return m_service && m_service->sending();
}

QString FeedbackViewModel::title() const
{
    return QStringLiteral("使用反馈");
}

QString FeedbackViewModel::subtitle() const
{
    if (!m_service) {
        return QStringLiteral("当前构建未启用反馈模块。");
    }
    if (needsProfile()) {
        return QStringLiteral("首次进入请填写昵称和联系方式，之后可长期保留同一会话。");
    }
    return QStringLiteral("支持发送文本、图片、文件，开发者回复会即时同步到本页。");
}

QString FeedbackViewModel::statusMessage() const
{
    return m_service ? m_service->statusMessage() : QStringLiteral("反馈模块不可用。");
}

QString FeedbackViewModel::connectionStatus() const
{
    if (!m_service) {
        return QStringLiteral("未启用");
    }
    if (m_service->socketConnected()) {
        return QStringLiteral("实时通道已连接");
    }
    if (m_service->busy()) {
        return QStringLiteral("正在同步反馈数据");
    }
    if (!m_service->hasProfile()) {
        return QStringLiteral("等待创建反馈会话");
    }
    return QStringLiteral("实时通道重连中");
}

QString FeedbackViewModel::profileSummary() const
{
    if (!m_service) {
        return QStringLiteral("未启用");
    }
    const auto conversation = m_service->conversation();
    if (conversation.nickname.isEmpty() && conversation.contact.isEmpty()) {
        return QStringLiteral("尚未建立反馈会话");
    }
    return QStringLiteral("%1 · %2")
        .arg(conversation.nickname.isEmpty() ? QStringLiteral("未命名用户") : conversation.nickname,
             conversation.contact.isEmpty() ? QStringLiteral("未填写联系方式") : conversation.contact);
}

QString FeedbackViewModel::projectSummary() const
{
    if (!m_service) {
        return QStringLiteral("未启用");
    }
    const auto conversation = m_service->conversation();
    if (!conversation.projectName.isEmpty() && !conversation.projectPath.isEmpty()) {
        return QStringLiteral("%1\n%2").arg(conversation.projectName, conversation.projectPath);
    }
    if (!conversation.projectName.isEmpty()) {
        return conversation.projectName;
    }
    if (!conversation.projectPath.isEmpty()) {
        return conversation.projectPath;
    }
    return QStringLiteral("当前未附带项目上下文");
}

QString FeedbackViewModel::conversationStatusLabel() const
{
    if (!m_service) {
        return QStringLiteral("未启用");
    }
    return feedbackStatusLabel(m_service->conversation().status);
}

QString FeedbackViewModel::appVersionLabel() const
{
    if (!m_service) {
        return QStringLiteral("未启用");
    }
    const auto conversation = m_service->conversation();
    return conversation.appVersion.isEmpty()
        ? QStringLiteral("未知版本")
        : conversation.appVersion;
}

QString FeedbackViewModel::latestUpdatedAt() const
{
    if (!m_service) {
        return QStringLiteral("暂无");
    }
    const auto conversation = m_service->conversation();
    return displayTime(!conversation.updatedAt.isEmpty() ? conversation.updatedAt : conversation.latestMessageAt);
}

QVariantList FeedbackViewModel::pendingAttachments() const
{
    QVariantList items;
    items.reserve(m_pendingAttachments.size());
    for (const auto &attachment : m_pendingAttachments) {
        QVariantMap row;
        row.insert(QStringLiteral("path"), attachment.path);
        row.insert(QStringLiteral("name"), attachment.name);
        row.insert(QStringLiteral("sizeBytes"), attachment.sizeBytes);
        row.insert(QStringLiteral("sizeLabel"), Formatters::formatBytes(attachment.sizeBytes));
        items.append(row);
    }
    return items;
}

int FeedbackViewModel::unreadCount() const
{
    return m_service ? m_service->unreadCount() : 0;
}

void FeedbackViewModel::activate()
{
    if (m_service) {
        m_service->activate();
    }
}

void FeedbackViewModel::setWorkspaceActive(bool active)
{
    if (m_service) {
        m_service->setWorkspaceActive(active);
    }
}

void FeedbackViewModel::submitProfile(const QString &nickname, const QString &contact)
{
    if (m_service) {
        m_service->createOrRestoreSession(nickname, contact);
    }
}

void FeedbackViewModel::refresh()
{
    if (m_service) {
        m_service->refreshMessages();
    }
}

void FeedbackViewModel::sendMessage(const QString &text)
{
    if (!m_service) {
        emit messageSubmitted(false);
        return;
    }

    QStringList paths;
    paths.reserve(m_pendingAttachments.size());
    for (const auto &attachment : m_pendingAttachments) {
        paths.append(attachment.path);
    }
    m_service->sendMessage(text, paths);
}

void FeedbackViewModel::addAttachmentUrls(const QVariantList &urls)
{
    for (const auto &value : urls) {
        const auto url = value.toUrl();
        const auto localPath = url.isLocalFile() ? url.toLocalFile() : value.toString();
        const QFileInfo info(localPath);
        if (!info.exists() || !info.isFile()) {
            continue;
        }

        bool exists = false;
        for (const auto &attachment : m_pendingAttachments) {
            if (attachment.path == info.absoluteFilePath()) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        m_pendingAttachments.append(PendingAttachment{
            info.absoluteFilePath(),
            info.fileName(),
            info.size()
        });
    }
    emit stateChanged();
}

void FeedbackViewModel::removePendingAttachment(int index)
{
    if (index < 0 || index >= m_pendingAttachments.size()) {
        return;
    }
    m_pendingAttachments.removeAt(index);
    emit stateChanged();
}

void FeedbackViewModel::copyMessageText(qint64 messageId)
{
    if (!m_service || messageId <= 0) {
        return;
    }

    for (const auto &message : m_service->messages()) {
        if (message.id == messageId && !message.text.isEmpty()) {
            if (auto *clipboard = QGuiApplication::clipboard()) {
                clipboard->setText(message.text);
            }
            break;
        }
    }
}

void FeedbackViewModel::deleteOwnMessage(qint64 messageId)
{
    if (m_service && messageId > 0) {
        m_service->deleteMessage(messageId);
    }
}

void FeedbackViewModel::clearOwnMessages()
{
    if (m_service) {
        m_service->clearClientMessages();
    }
}

void FeedbackViewModel::openAttachment(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void FeedbackViewModel::rebuildMessages()
{
    m_messageModel->setItems(m_service ? m_service->messages() : QVector<FeedbackMessage>{});
}

QString FeedbackViewModel::feedbackStatusLabel(const QString &status)
{
    if (status == QStringLiteral("resolved")) {
        return QStringLiteral("已解决");
    }
    if (status == QStringLiteral("in_progress")) {
        return QStringLiteral("跟进中");
    }
    return QStringLiteral("待处理");
}

#pragma once

#include <QObject>
#include <QVariantList>

class FeedbackMessageListModel;
class FeedbackService;

class FeedbackViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* messageModel READ messageModel CONSTANT)
    Q_PROPERTY(bool needsProfile READ needsProfile NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY stateChanged)
    Q_PROPERTY(QString profileSummary READ profileSummary NOTIFY stateChanged)
    Q_PROPERTY(QString projectSummary READ projectSummary NOTIFY stateChanged)
    Q_PROPERTY(QString conversationStatusLabel READ conversationStatusLabel NOTIFY stateChanged)
    Q_PROPERTY(QString appVersionLabel READ appVersionLabel NOTIFY stateChanged)
    Q_PROPERTY(QString latestUpdatedAt READ latestUpdatedAt NOTIFY stateChanged)
    Q_PROPERTY(QVariantList pendingAttachments READ pendingAttachments NOTIFY stateChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY stateChanged)

public:
    explicit FeedbackViewModel(FeedbackService *service, QObject *parent = nullptr);

    QObject *messageModel() const;
    bool needsProfile() const;
    bool ready() const;
    bool busy() const;
    bool sending() const;
    QString title() const;
    QString subtitle() const;
    QString statusMessage() const;
    QString connectionStatus() const;
    QString profileSummary() const;
    QString projectSummary() const;
    QString conversationStatusLabel() const;
    QString appVersionLabel() const;
    QString latestUpdatedAt() const;
    QVariantList pendingAttachments() const;
    int unreadCount() const;

    Q_INVOKABLE void activate();
    Q_INVOKABLE void setWorkspaceActive(bool active);
    Q_INVOKABLE void submitProfile(const QString &nickname, const QString &contact);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void addAttachmentUrls(const QVariantList &urls);
    Q_INVOKABLE void removePendingAttachment(int index);
    Q_INVOKABLE void openAttachment(const QString &url);

signals:
    void stateChanged();
    void messageSubmitted(bool success);

private:
    struct PendingAttachment {
        QString path;
        QString name;
        qint64 sizeBytes = 0;
    };

    void rebuildMessages();
    static QString feedbackStatusLabel(const QString &status);

    FeedbackService *m_service = nullptr;
    FeedbackMessageListModel *m_messageModel = nullptr;
    QVector<PendingAttachment> m_pendingAttachments;
};

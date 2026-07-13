#pragma once

#include "domain/Enums.h"

#include <QObject>
#include <QVariantList>
#include <QUrl>

class ImportService;
class FeedbackService;
class ProjectService;

class ShellViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString projectName READ projectName NOTIFY stateChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY stateChanged)
    Q_PROPERTY(bool projectEntered READ projectEntered NOTIFY stateChanged)
    Q_PROPERTY(QString globalSearchText READ globalSearchText WRITE setGlobalSearchText NOTIFY globalSearchTextChanged)
    Q_PROPERTY(int currentWorkspace READ currentWorkspace WRITE setCurrentWorkspace NOTIFY currentWorkspaceChanged)
    Q_PROPERTY(QString statusSummary READ statusSummary NOTIFY stateChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY stateChanged)
    Q_PROPERTY(QVariantList workspaceTabs READ workspaceTabs NOTIFY stateChanged)
    Q_PROPERTY(int projectLibraryWorkspaceId READ projectLibraryWorkspaceId CONSTANT)
    Q_PROPERTY(int libraryWorkspaceId READ libraryWorkspaceId CONSTANT)
    Q_PROPERTY(int materialCenterWorkspaceId READ materialCenterWorkspaceId CONSTANT)
    Q_PROPERTY(int reportWorkspaceId READ reportWorkspaceId CONSTANT)
    Q_PROPERTY(int jobsWorkspaceId READ jobsWorkspaceId CONSTANT)
    Q_PROPERTY(int feedbackWorkspaceId READ feedbackWorkspaceId CONSTANT)

public:
    explicit ShellViewModel(ProjectService *projectService, ImportService *importService, FeedbackService *feedbackService, QObject *parent = nullptr);

    QString projectName() const;
    QString projectPath() const;
    bool projectEntered() const;
    QString globalSearchText() const;
    int currentWorkspace() const;
    QString statusSummary() const;
    QString lastMessage() const;
    QVariantList workspaceTabs() const;
    int projectLibraryWorkspaceId() const;
    int libraryWorkspaceId() const;
    int materialCenterWorkspaceId() const;
    int reportWorkspaceId() const;
    int jobsWorkspaceId() const;
    int feedbackWorkspaceId() const;

    void resetProjectUiState();
    void enterProjectFromLibrary();
    void setGlobalSearchText(const QString &text);
    void setCurrentWorkspace(int workspace);

    Q_INVOKABLE void createProject();
    Q_INVOKABLE void openProject();
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE void addSourceDirectory();
    Q_INVOKABLE void importSourceDirectory(const QUrl &directoryUrl);
    Q_INVOKABLE void cancelAddSourceDirectory();
    Q_INVOKABLE void openSettings();

signals:
    void stateChanged();
    void currentWorkspaceChanged();
    void globalSearchTextChanged();
    void searchRequested(const QString &text);
    void sourceImported();
    void addSourceDirectoryRequested();
    void openSettingsRequested();

private:
    ProjectService *m_projectService = nullptr;
    ImportService *m_importService = nullptr;
    FeedbackService *m_feedbackService = nullptr;
    QString m_globalSearchText;
    WorkspaceId m_currentWorkspace = WorkspaceId::ProjectLibrary;
    QString m_lastMessage;
    bool m_projectEntered = false;
};

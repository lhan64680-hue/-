#pragma once

#include "domain/Enums.h"

#include <QObject>

class ImportService;
class ProjectService;

class ShellViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString projectName READ projectName NOTIFY stateChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY stateChanged)
    Q_PROPERTY(QString globalSearchText READ globalSearchText WRITE setGlobalSearchText NOTIFY globalSearchTextChanged)
    Q_PROPERTY(int currentWorkspace READ currentWorkspace WRITE setCurrentWorkspace NOTIFY currentWorkspaceChanged)
    Q_PROPERTY(QString statusSummary READ statusSummary NOTIFY stateChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY stateChanged)

public:
    explicit ShellViewModel(ProjectService *projectService, ImportService *importService, QObject *parent = nullptr);

    QString projectName() const;
    QString projectPath() const;
    QString globalSearchText() const;
    int currentWorkspace() const;
    QString statusSummary() const;
    QString lastMessage() const;

    void setGlobalSearchText(const QString &text);
    void setCurrentWorkspace(int workspace);

    Q_INVOKABLE void createProject();
    Q_INVOKABLE void openProject();
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE void addSourceDirectory();

signals:
    void stateChanged();
    void currentWorkspaceChanged();
    void globalSearchTextChanged();
    void searchRequested(const QString &text);
    void sourceImported();

private:
    ProjectService *m_projectService = nullptr;
    ImportService *m_importService = nullptr;
    QString m_globalSearchText;
    WorkspaceId m_currentWorkspace = WorkspaceId::Import;
    QString m_lastMessage;
};

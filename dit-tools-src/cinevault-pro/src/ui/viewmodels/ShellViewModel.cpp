#include "ui/viewmodels/ShellViewModel.h"

#include "application/ImportService.h"
#include "application/ProjectService.h"

#include <QApplication>
#include <QMessageBox>
#include <QVariantMap>

namespace {
QWidget *dialogParent()
{
    return QApplication::activeWindow();
}

void showWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(dialogParent(), title, message);
}

QVariantMap workspaceTab(const QString &label, WorkspaceId workspace, int buttonWidth, bool enabled)
{
    return QVariantMap{
        {QStringLiteral("label"), label},
        {QStringLiteral("value"), static_cast<int>(workspace)},
        {QStringLiteral("buttonWidth"), buttonWidth},
        {QStringLiteral("enabled"), enabled}
    };
}
}

ShellViewModel::ShellViewModel(ProjectService *projectService, ImportService *importService, QObject *parent)
    : QObject(parent)
    , m_projectService(projectService)
    , m_importService(importService)
{
    connect(m_projectService, &ProjectService::projectChanged, this, &ShellViewModel::stateChanged);
    connect(m_importService, &ImportService::importStateChanged, this, [this]() {
        m_lastMessage = m_importService->lastMessage();
        emit stateChanged();
    });
}

QString ShellViewModel::projectName() const
{
    return m_projectService->hasOpenProject() ? m_projectService->currentProject().name : QStringLiteral("未打开项目");
}

QString ShellViewModel::projectPath() const
{
    return m_projectService->currentProject().rootPath;
}

bool ShellViewModel::projectEntered() const
{
    return m_projectEntered && m_projectService->hasOpenProject();
}

QString ShellViewModel::globalSearchText() const
{
    return m_globalSearchText;
}

int ShellViewModel::currentWorkspace() const
{
    return static_cast<int>(m_currentWorkspace);
}

QString ShellViewModel::statusSummary() const
{
    if (!m_projectService->hasOpenProject()) {
        return QStringLiteral("请先在项目库新建或打开项目");
    }
    if (!projectEntered()) {
        return QStringLiteral("请在项目库点击项目卡片进入项目");
    }
    return QStringLiteral("项目已就绪");
}

QString ShellViewModel::lastMessage() const
{
    return m_lastMessage;
}

QVariantList ShellViewModel::workspaceTabs() const
{
    const auto projectReady = projectEntered();
    return {
        workspaceTab(QStringLiteral("项目库"), WorkspaceId::ProjectLibrary, 70, true),
        workspaceTab(QStringLiteral("素材备份"), WorkspaceId::Import, 86, projectReady),
        workspaceTab(QStringLiteral("素材库"), WorkspaceId::Library, 70, projectReady),
        workspaceTab(QStringLiteral("素材管理中心"), WorkspaceId::MaterialCenter, 108, projectReady),
        workspaceTab(QStringLiteral("报表"), WorkspaceId::Report, 56, projectReady),
        workspaceTab(QStringLiteral("任务"), WorkspaceId::Jobs, 56, projectReady)
    };
}

int ShellViewModel::projectLibraryWorkspaceId() const
{
    return static_cast<int>(WorkspaceId::ProjectLibrary);
}

int ShellViewModel::materialBackupWorkspaceId() const
{
    return static_cast<int>(WorkspaceId::Import);
}

int ShellViewModel::libraryWorkspaceId() const
{
    return static_cast<int>(WorkspaceId::Library);
}

int ShellViewModel::materialCenterWorkspaceId() const
{
    return static_cast<int>(WorkspaceId::MaterialCenter);
}

int ShellViewModel::reportWorkspaceId() const
{
    return static_cast<int>(WorkspaceId::Report);
}

int ShellViewModel::jobsWorkspaceId() const
{
    return static_cast<int>(WorkspaceId::Jobs);
}

void ShellViewModel::resetProjectUiState()
{
    if (!m_projectService->hasOpenProject()) {
        m_projectEntered = false;
        if (m_currentWorkspace != WorkspaceId::ProjectLibrary) {
            m_currentWorkspace = WorkspaceId::ProjectLibrary;
            emit currentWorkspaceChanged();
        }
    }
    if (m_globalSearchText.isEmpty()) {
        return;
    }
    m_globalSearchText.clear();
    emit globalSearchTextChanged();
    emit searchRequested(m_globalSearchText);
}

void ShellViewModel::enterProjectFromLibrary()
{
    if (!m_projectService->hasOpenProject()) {
        m_projectEntered = false;
        m_lastMessage = QStringLiteral("请先在项目库打开一个项目。");
        emit stateChanged();
        return;
    }

    m_projectEntered = true;
    if (m_currentWorkspace != WorkspaceId::Library) {
        m_currentWorkspace = WorkspaceId::Library;
        emit currentWorkspaceChanged();
    }
    emit stateChanged();
    emit searchRequested(m_globalSearchText);
}

void ShellViewModel::setGlobalSearchText(const QString &text)
{
    if (m_globalSearchText == text) {
        return;
    }
    m_globalSearchText = text;
    emit globalSearchTextChanged();
    emit searchRequested(text);
}

void ShellViewModel::setCurrentWorkspace(int workspace)
{
    const auto value = static_cast<WorkspaceId>(workspace);
    const auto normalizedValue = value == WorkspaceId::Qc
        ? WorkspaceId::Library
        : value;
    if (normalizedValue != WorkspaceId::ProjectLibrary && !projectEntered()) {
        m_lastMessage = QStringLiteral("请先在项目库点击项目卡片进入项目。");
        if (m_currentWorkspace != WorkspaceId::ProjectLibrary) {
            m_currentWorkspace = WorkspaceId::ProjectLibrary;
            emit currentWorkspaceChanged();
        }
        emit stateChanged();
        return;
    }
    if (m_currentWorkspace == normalizedValue) {
        return;
    }
    m_currentWorkspace = normalizedValue;
    emit currentWorkspaceChanged();
    emit searchRequested(m_globalSearchText);
}

void ShellViewModel::createProject()
{
    m_currentWorkspace = WorkspaceId::ProjectLibrary;
    m_lastMessage = QStringLiteral("请在项目库页面新建项目。");
    emit currentWorkspaceChanged();
    emit stateChanged();
}

void ShellViewModel::openProject()
{
    m_currentWorkspace = WorkspaceId::ProjectLibrary;
    m_lastMessage = QStringLiteral("请在项目库页面打开项目。");
    emit currentWorkspaceChanged();
    emit stateChanged();
}

void ShellViewModel::closeProject()
{
    m_projectEntered = false;
    m_projectService->closeProject();
    if (m_currentWorkspace != WorkspaceId::ProjectLibrary) {
        m_currentWorkspace = WorkspaceId::ProjectLibrary;
        emit currentWorkspaceChanged();
    }
    m_lastMessage = QStringLiteral("项目已关闭");
    emit stateChanged();
}

void ShellViewModel::addSourceDirectory()
{
    if (!projectEntered()) {
        m_lastMessage = QStringLiteral("请先在项目库点击项目卡片进入项目。");
        if (m_currentWorkspace != WorkspaceId::ProjectLibrary) {
            m_currentWorkspace = WorkspaceId::ProjectLibrary;
            emit currentWorkspaceChanged();
        }
        emit stateChanged();
        return;
    }

    emit addSourceDirectoryRequested();
}

void ShellViewModel::importSourceDirectory(const QUrl &directoryUrl)
{
    QString errorMessage;
    const auto directory = directoryUrl.toLocalFile();
    if (directory.isEmpty()) {
        m_lastMessage = QStringLiteral("已取消添加素材源。");
        emit stateChanged();
        return;
    }

    if (!m_importService->importDirectory(directory, &errorMessage)) {
        m_lastMessage = errorMessage;
        showWarning(QStringLiteral("添加素材源失败"), m_lastMessage);
    } else {
        m_lastMessage = m_importService->lastMessage();
        emit sourceImported();
    }
    emit stateChanged();
}

void ShellViewModel::cancelAddSourceDirectory()
{
    m_lastMessage = QStringLiteral("已取消添加素材源。");
    emit stateChanged();
}

void ShellViewModel::openSettings()
{
    emit openSettingsRequested();
}

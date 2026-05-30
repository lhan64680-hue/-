#include "ui/viewmodels/ShellViewModel.h"

#include "application/ImportService.h"
#include "application/ProjectService.h"
#include "shared/Paths.h"

#include <QFileDialog>
#include <QInputDialog>

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
        return QStringLiteral("请先创建或打开项目");
    }
    return QStringLiteral("项目已就绪");
}

QString ShellViewModel::lastMessage() const
{
    return m_lastMessage;
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
    if (m_currentWorkspace == value) {
        return;
    }
    m_currentWorkspace = value;
    emit currentWorkspaceChanged();
}

void ShellViewModel::createProject()
{
    QString errorMessage;
    const auto projectName = QInputDialog::getText(nullptr, QStringLiteral("新建项目"), QStringLiteral("项目名称"));
    if (projectName.trimmed().isEmpty()) {
        return;
    }

    const auto parentDirectory = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("选择项目保存目录"), Paths::projectsRoot());
    if (parentDirectory.isEmpty()) {
        return;
    }

    if (!m_projectService->createProject(projectName, parentDirectory, &errorMessage)) {
        m_lastMessage = errorMessage;
    } else {
        m_lastMessage = QStringLiteral("项目已创建：%1").arg(projectName);
    }
    emit stateChanged();
}

void ShellViewModel::openProject()
{
    QString errorMessage;
    const auto databasePath = QFileDialog::getOpenFileName(nullptr, QStringLiteral("打开项目"), Paths::projectsRoot(), QStringLiteral("影资管家项目 (*.cvdb)"));
    if (databasePath.isEmpty()) {
        return;
    }

    if (!m_projectService->openProject(databasePath, &errorMessage)) {
        m_lastMessage = errorMessage;
    } else {
        m_lastMessage = QStringLiteral("项目已打开：%1").arg(m_projectService->currentProject().name);
    }
    emit stateChanged();
}

void ShellViewModel::closeProject()
{
    m_projectService->closeProject();
    m_lastMessage = QStringLiteral("项目已关闭");
    emit stateChanged();
}

void ShellViewModel::addSourceDirectory()
{
    if (!m_projectService->hasOpenProject()) {
        m_lastMessage = QStringLiteral("请先创建或打开项目。");
        emit stateChanged();
        return;
    }

    QString errorMessage;
    const auto directory = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("选择素材源目录"), QString());
    if (directory.isEmpty()) {
        return;
    }

    if (!m_importService->importDirectory(directory, &errorMessage)) {
        m_lastMessage = errorMessage;
    } else {
        m_lastMessage = m_importService->lastMessage();
        emit sourceImported();
    }
    emit stateChanged();
}

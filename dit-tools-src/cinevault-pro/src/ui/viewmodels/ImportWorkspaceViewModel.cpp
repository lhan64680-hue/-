#include "ui/viewmodels/ImportWorkspaceViewModel.h"

#include "application/ImportService.h"

ImportWorkspaceViewModel::ImportWorkspaceViewModel(ImportService *importService, QObject *parent)
    : QObject(parent)
    , m_importService(importService)
{
    connect(m_importService, &ImportService::importStateChanged, this, &ImportWorkspaceViewModel::summaryChanged);
}

QString ImportWorkspaceViewModel::summaryText() const
{
    const auto message = m_importService->lastMessage();
    if (!message.isEmpty()) {
        return message;
    }
    return QStringLiteral("拖入素材卡、硬盘或项目目录。导入后将自动识别目录结构并写入项目数据库。");
}

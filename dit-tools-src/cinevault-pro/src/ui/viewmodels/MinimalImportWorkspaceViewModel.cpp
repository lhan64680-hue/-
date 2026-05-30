#include "ui/viewmodels/MinimalImportWorkspaceViewModel.h"

MinimalImportWorkspaceViewModel::MinimalImportWorkspaceViewModel(QObject *parent)
    : QObject(parent)
{
}

QString MinimalImportWorkspaceViewModel::summaryText() const
{
    return QStringLiteral("当前版本只用于验证窗口壳体、布局和切页，真实导入逻辑将在后续里程碑接回。");
}

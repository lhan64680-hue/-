#include "ui/viewmodels/SourceRailViewModel.h"

#include "application/LibraryQueryService.h"
#include "ui/models/SourceRootListModel.h"

SourceRailViewModel::SourceRailViewModel(LibraryQueryService *libraryQueryService, QObject *parent)
    : QObject(parent)
    , m_libraryQueryService(libraryQueryService)
    , m_model(new SourceRootListModel(this))
{
}

SourceRootListModel *SourceRailViewModel::model() const
{
    return m_model;
}

qint64 SourceRailViewModel::selectedSourceId() const
{
    return m_selectedSourceId;
}

void SourceRailViewModel::reload()
{
    const auto items = m_libraryQueryService->fetchSourceRoots();
    m_model->setItems(items);
    bool foundSelection = false;
    for (const auto &item : items) {
        if (item.id == m_selectedSourceId) {
            foundSelection = true;
            break;
        }
    }
    if (!foundSelection && m_selectedSourceId != 0) {
        m_selectedSourceId = 0;
        emit selectionChanged();
    }
}

void SourceRailViewModel::selectSource(qint64 sourceRootId)
{
    if (m_selectedSourceId == sourceRootId) {
        return;
    }
    m_selectedSourceId = sourceRootId;
    emit selectionChanged();
    emit sourceSelected(sourceRootId);
}

void SourceRailViewModel::clearSelection()
{
    selectSource(0);
}

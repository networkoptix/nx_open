#include "archivedropinstrument.h"
#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/control/display_state.h>
#include <ui/model/layout_grid_mapper.h>
#include <ui/model/layout_model.h>
#include <ui/model/resource_item_model.h>
#include <utils/common/warnings.h>
#include <file_processor.h>

ArchiveDropInstrument::ArchiveDropInstrument(QnDisplayState *state, QObject *parent):
    Instrument(VIEWPORT, makeSet(QEvent::DragEnter, QEvent::DragMove, QEvent::DragLeave, QEvent::Drop), parent),
    m_state(state)
{
    if(state == NULL) {
        qnNullWarning(state);
        return;
    }
}

void ArchiveDropInstrument::drop(QGraphicsView *view, const QUrl &url, QPoint &pos) {
    QStringList files;
    QnFileProcessor::findAcceptedFiles(url.toLocalFile(), &files);
    if(m_files.empty())
        return;

    dropInternal(view, files, pos);
}

bool ArchiveDropInstrument::dragEnterEvent(QWidget * /*viewport*/, QDragEnterEvent *event) {
    if(m_state == NULL)
        return false;

    m_files.clear();
    foreach (const QUrl &url, event->mimeData()->urls())
        QnFileProcessor::findAcceptedFiles(url.toLocalFile(), &m_files);
    if (m_files.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool ArchiveDropInstrument::dragMoveEvent(QWidget * /*viewport*/, QDragMoveEvent *event) {
    if(m_state == NULL)
        return false;

    if(m_files.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool ArchiveDropInstrument::dragLeaveEvent(QWidget * /*viewport*/, QDragLeaveEvent * /*event*/) {
    if(m_state == NULL)
        return false;

    if(m_files.empty())
        return false;

    return true;
}

bool ArchiveDropInstrument::dropEvent(QWidget *viewport, QDropEvent *event) {
    return dropInternal(view(viewport), m_files, event->pos());
}

bool ArchiveDropInstrument::dropInternal(QGraphicsView *view, const QStringList &files, const QPoint &pos) {
    if(m_state == NULL)
        return false;

    QnResourceList resources = QnResourceDirectoryBrowser::instance().checkFiles(files);
    qnResPool->addResources(resources);

    QRect geometry(m_state->gridMapper()->mapToGrid(view->mapToScene(pos)), QSize(1, 1));
    foreach(QnResourcePtr resource, resources) {
        QnLayoutItemModel *item = new QnLayoutItemModel(resource->getUniqueId());
        item->setGeometry(geometry);

        m_state->model()->addItem(item);
        if(!item->isPinned()) {
            /* Place already taken, pick closest one. */
            QRect newGeometry = m_state->model()->closestFreeSlot(geometry.topLeft(), geometry.size());
            m_state->model()->pinItem(item, newGeometry);
        }
    }

    return true;
}
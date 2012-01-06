#include "drop_instrument.h"

#include <QtGui/QtEvents>

#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/view_drag_and_drop.h>

#include "file_processor.h"

DropInstrument::DropInstrument(QnWorkbenchController *controller, QObject *parent):
    Instrument(VIEWPORT, makeSet(QEvent::DragEnter, QEvent::DragMove, QEvent::DragLeave, QEvent::Drop), parent),
    m_controller(controller)
{
    Q_ASSERT(controller);
}

bool DropInstrument::dragEnterEvent(QWidget * /*viewport*/, QDragEnterEvent *event) {
    m_files = QnFileProcessor::findAcceptedFiles(event->mimeData()->urls());
    m_resources = deserializeResources(event->mimeData()->data(resourcesMime()));
    for(int i = m_resources.size() - 1; i >= 0; i--)
        if(m_controller->display()->widget(m_resources[i]) != NULL)
            m_resources.removeAt(i); /* Don't allow duplicates. */

    if (m_files.empty() && m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragMoveEvent(QWidget * /*viewport*/, QDragMoveEvent *event) {
    if(m_files.empty() && m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragLeaveEvent(QWidget * /*viewport*/, QDragLeaveEvent * /*event*/) {
    if(m_files.empty() && m_resources.empty())
        return false;

    return true;
}

bool DropInstrument::dropEvent(QWidget *viewport, QDropEvent *event) {
    QPointF gridPos = m_controller->workbench()->mapper()->mapToGridF(view(viewport)->mapToScene(event->pos()));

    if(!m_resources.empty())
        m_controller->drop(m_resources, gridPos);
    else if(!m_files.empty())
        m_controller->drop(m_files, gridPos, false);

    return true;
}

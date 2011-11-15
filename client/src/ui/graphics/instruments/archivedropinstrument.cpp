#include "archivedropinstrument.h"
#include <ui/control/display_controller.h>
#include <ui/control/display_state.h>
#include <ui/model/layout_grid_mapper.h>
#include <utils/common/warnings.h>
#include <file_processor.h>

ArchiveDropInstrument::ArchiveDropInstrument(QnDisplayController *controller, QObject *parent):
    Instrument(VIEWPORT, makeSet(QEvent::DragEnter, QEvent::DragMove, QEvent::DragLeave, QEvent::Drop), parent),
    m_controller(controller)
{
    if(controller == NULL) {
        qnNullWarning(controller);
        return;
    }
}

bool ArchiveDropInstrument::dragEnterEvent(QWidget * /*viewport*/, QDragEnterEvent *event) {
    if(m_controller == NULL)
        return false;

    m_files = QnFileProcessor::findAcceptedFiles(event->mimeData()->urls());
    if (m_files.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool ArchiveDropInstrument::dragMoveEvent(QWidget * /*viewport*/, QDragMoveEvent *event) {
    if(m_controller == NULL)
        return false;

    if(m_files.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool ArchiveDropInstrument::dragLeaveEvent(QWidget * /*viewport*/, QDragLeaveEvent * /*event*/) {
    if(m_controller == NULL)
        return false;

    if(m_files.empty())
        return false;

    return true;
}

bool ArchiveDropInstrument::dropEvent(QWidget *viewport, QDropEvent *event) {
    if(m_controller == NULL)
        return false;

    m_controller->drop(m_files, m_controller->state()->mapper()->mapToGrid(view(viewport)->mapToScene(event->pos())), false);
}

#include "drop_instrument.h"
#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/view_drag_and_drop.h>
#include "file_processor.h"

DropInstrument::DropInstrument(QnWorkbenchController *controller, QObject *parent):
    Instrument(SCENE, makeSet(QEvent::GraphicsSceneDragEnter, QEvent::GraphicsSceneDragMove, QEvent::GraphicsSceneDragLeave, QEvent::GraphicsSceneDrop), parent),
    m_controller(controller)
{
    Q_ASSERT(controller);
}

bool DropInstrument::dragEnterEvent(QGraphicsScene * /*scene*/, QGraphicsSceneDragDropEvent *event) {
    m_files = QnFileProcessor::findAcceptedFiles(event->mimeData()->urls());
    m_resources = deserializeResources(event->mimeData()->data(resourcesMime()));

    if (m_files.empty() && m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragMoveEvent(QGraphicsScene * /*scene*/, QGraphicsSceneDragDropEvent *event) {
    if(m_files.empty() && m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragLeaveEvent(QGraphicsScene * /*scene*/, QGraphicsSceneDragDropEvent * /*event*/) {
    if(m_files.empty() && m_resources.empty())
        return false;

    return true;
}

bool DropInstrument::dropEvent(QGraphicsScene *scene, QGraphicsSceneDragDropEvent *event) {
    QPointF gridPos = m_controller->workbench()->mapper()->mapToGridF(event->scenePos());

    if(!m_resources.empty())
        m_controller->drop(m_resources, gridPos);
    else if(!m_files.empty())
        m_controller->drop(m_files, gridPos, false);

    return true;
}

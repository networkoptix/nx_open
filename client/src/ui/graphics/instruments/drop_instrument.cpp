#include "drop_instrument.h"
#include <limits>
#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>
#include <QGraphicsItem>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/view_drag_and_drop.h>
#include "file_processor.h"

class DropSurfaceItem: public QGraphicsItem {
public:
    DropSurfaceItem(QGraphicsItem *parent = NULL): 
        QGraphicsItem(parent)
    {
        qreal d = std::numeric_limits<qreal>::max() / 4;
        m_boundingRect = QRectF(QPointF(-d, -d), QPoint(d, d));

        setAcceptedMouseButtons(0);
        /* Don't disable this item here or it will swallow mouse wheel events. */
    }

    DropInstrument *dropInstrument() const {
        return m_dropInstrument.data();
    }

    void setDropInstrument(DropInstrument *dropInstrument) {
        m_dropInstrument = dropInstrument;
    }

protected:
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->dragEnterEvent(this, event);
    }

    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->dragLeaveEvent(this, event);
    }

    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->dragMoveEvent(this, event);
    }

    virtual void dropEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->dropEvent(this, event);
    }
    
private:
    QWeakPointer<DropInstrument> m_dropInstrument;
    QRectF m_boundingRect;
};

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

#include "drop_instrument.h"
#include <limits>
#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>
#include <QGraphicsItem>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/view_drag_and_drop.h>
#include "file_processor.h"
#include "destruction_guard_item.h"

class DropSurfaceItem: public QGraphicsObject {
public:
    DropSurfaceItem(QGraphicsItem *parent = NULL): 
        QGraphicsObject(parent)
    {
        qreal d = std::numeric_limits<qreal>::max() / 4;
        m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));

        setAcceptedMouseButtons(0);
        setAcceptDrops(true);
        /* Don't disable this item here or it will swallow mouse wheel events. */
    }

    DropInstrument *dropInstrument() const {
        return m_dropInstrument.data();
    }

    void setDropInstrument(DropInstrument *dropInstrument) {
        m_dropInstrument = dropInstrument;
    }

    virtual QRectF boundingRect() const override {
        return m_boundingRect;
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

protected:
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->sceneEvent(this, event);
    }

    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->sceneEvent(this, event);
    }

    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->sceneEvent(this, event);
    }

    virtual void dropEvent(QGraphicsSceneDragDropEvent *event) override {
        if(m_dropInstrument)
            m_dropInstrument.data()->sceneEvent(this, event);
    }
    
private:
    QWeakPointer<DropInstrument> m_dropInstrument;
    QRectF m_boundingRect;
};


DropInstrument::DropInstrument(QnWorkbench *workbench, QObject *parent):
    Instrument(ITEM, makeSet(/* No events here, we'll receive them from the surface item. */), parent),
    m_workbench(workbench)
{
    if(workbench == NULL)
        qnNullWarning(workbench)

    DropSurfaceItem *surface = new DropSurfaceItem();
    surface->setDropInstrument(this);
    surface->setParent(this);
    m_surface = surface;
}

QGraphicsObject *DropInstrument::surface() const {
    return m_surface.data();
}

void DropInstrument::installedNotify() {
    DestructionGuardItem *guard = new DestructionGuardItem();
    guard->setGuarded(surface());
    guard->setPos(0.0, 0.0);
    scene()->addItem(guard);

    m_guard = guard;
}

void DropInstrument::aboutToBeUninstalledNotify() {
    if(guard() != NULL)
        delete guard();
}

bool DropInstrument::dragEnterEvent(QGraphicsItem * /*item*/, QGraphicsSceneDragDropEvent *event) {
    m_files = QnFileProcessor::findAcceptedFiles(event->mimeData()->urls());
    m_resources = deserializeResources(event->mimeData()->data(resourcesMime()));

    if (m_files.empty() && m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragMoveEvent(QGraphicsItem * /*item*/, QGraphicsSceneDragDropEvent *event) {
    if(m_files.empty() && m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragLeaveEvent(QGraphicsItem * /*item*/, QGraphicsSceneDragDropEvent * /*event*/) {
    if(m_files.empty() && m_resources.empty())
        return false;

    return true;
}

bool DropInstrument::dropEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) {
    QnWorkbench *workbench = m_workbench.data();
    if(workbench == NULL)
        return true;

    QnResourceList resources = QnFileProcessor::createResourcesForFiles(m_files);
    resources.append(m_resources);

    QPointF gridPos = workbench->mapper()->mapToGridF(event->scenePos());

    foreach(const QnResourcePtr &resource, m_resources) {
        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid());
        workbench->currentLayout()->addItem(item);
        item->adjustGeometry(gridPos);
    }

    return true;
}

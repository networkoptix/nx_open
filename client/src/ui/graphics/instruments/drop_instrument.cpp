#include "drop_instrument.h"
#include <limits>
#include <QGraphicsSceneDragDropEvent>
#include <QMimeData>
#include <QGraphicsItem>
#include <utils/common/warnings.h>
#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>
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

    virtual QRectF boundingRect() const override {
        return m_boundingRect;
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

private:
    QRectF m_boundingRect;
};


DropInstrument::DropInstrument(bool intoNewLayout, QnWorkbenchContext *context, QObject *parent):
    Instrument(Item, makeSet(/* No events here, we'll receive them from the surface item. */), parent),
    m_context(context),
    m_intoNewLayout(intoNewLayout)
{
    if(context == NULL)
        qnNullWarning(context);

    m_filterItem = new SceneEventFilterItem();
    m_filterItem->setEventFilter(this);
}

QGraphicsObject *DropInstrument::surface() const {
    return m_surface.data();
}

void DropInstrument::setSurface(QGraphicsObject *surface) {
    if(this->surface()) {
        this->surface()->removeSceneEventFilter(m_filterItem);

        if(this->surface()->parent() == this)
            delete this->surface();
    }

    m_surface = surface;

    if(this->surface()) {
        this->surface()->setAcceptDrops(true);
        this->surface()->installSceneEventFilter(m_filterItem);
    }
}

void DropInstrument::installedNotify() {
    DestructionGuardItem *guard = new DestructionGuardItem();
    guard->setGuarded(m_filterItem);
    guard->setPos(0.0, 0.0);
    scene()->addItem(guard);

    if(surface() == NULL) {
        DropSurfaceItem *surface = new DropSurfaceItem();
        surface->setParent(this);
        scene()->addItem(surface);
        setSurface(surface);
    }

    m_guard = guard;
}

void DropInstrument::aboutToBeUninstalledNotify() {
    if(guard() != NULL)
        delete guard();

    setSurface(NULL);
}

bool DropInstrument::sceneEventFilter(QGraphicsItem *watched, QEvent *event) {
    return this->sceneEvent(watched, event);
}

bool DropInstrument::dragEnterEvent(QGraphicsItem * /*item*/, QGraphicsSceneDragDropEvent *event) {
    m_resources = QnWorkbenchResource::deserializeResources(event->mimeData());
    if (m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragMoveEvent(QGraphicsItem * /*item*/, QGraphicsSceneDragDropEvent *event) {
    if(m_resources.empty())
        return false;

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragLeaveEvent(QGraphicsItem * /*item*/, QGraphicsSceneDragDropEvent * /*event*/) {
    if(m_resources.empty())
        return false;

    return true;
}

bool DropInstrument::dropEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) {
    QnWorkbenchContext *context = m_context.data();
    if(context == NULL)
        return true;

    if(!m_intoNewLayout) {
        QVariantMap params;
        params[Qn::GridPositionParameter] = context->workbench()->mapper()->mapToGridF(event->scenePos());

        context->menu()->trigger(Qn::DropResourcesAction, m_resources, params);
    } else {
        context->menu()->trigger(Qn::DropResourcesIntoNewLayoutAction, m_resources);
    }

    event->acceptProposedAction();
    return true;
}

#include "resize_hover_instrument.h"

#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QGraphicsSceneHoverEvent>

#include <ui/common/geometry.h>
#include <ui/common/frame_section_queryable.h>

namespace {
    // TODO: #Elric shared with resize_hover_instrument
    class GraphicsWidget: public QGraphicsWidget {
    public:
        friend class ResizingInstrument;

        Qt::WindowFrameSection getWindowFrameSectionAt(const QPointF &pos) const {
            return this->windowFrameSectionAt(pos);
        }
    };

    GraphicsWidget *open(QGraphicsWidget *widget) {
        return static_cast<GraphicsWidget *>(widget);
    }

    bool hasDecoration(QGraphicsWidget *widget) {
        return (widget->windowFlags() & Qt::Window) && (widget->windowFlags() & Qt::WindowTitleHint);
    }

} // anonymous namespace 

ResizeHoverInstrument::ResizeHoverInstrument(QObject *parent):
    Instrument(Item, makeSet(QEvent::GraphicsSceneHoverMove, QEvent::GraphicsSceneHoverLeave), parent),
    m_effectRadius(0.0)
{}

bool ResizeHoverInstrument::registeredNotify(QGraphicsItem *item) {
    if(!item->isWidget())
        return false;

    FrameSectionQueryable *queryable = dynamic_cast<FrameSectionQueryable *>(item);
    if(queryable != NULL)
        m_queryableByItem.insert(item, queryable);

    return true;
}

void ResizeHoverInstrument::unregisteredNotify(QGraphicsItem *item) {
    m_queryableByItem.remove(item);
    m_affectedItems.remove(item);
}

bool ResizeHoverInstrument::hoverMoveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) {
    QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item);
    if(!satisfiesItemConditions(widget))
        return false;
    
    FrameSectionQueryable *queryable = m_queryableByItem.value(item);
    if(!queryable && !hasDecoration(widget))
        return false; /* Has no decorations and not queryable for frame sections. */

    Qt::WindowFrameSection section;
    QCursor cursor;
    if(queryable == NULL) {
        section = open(widget)->getWindowFrameSectionAt(event->pos());
        cursor = Qn::calculateHoverCursorShape(section);
    } else {
        QRectF effectiveRect = item->mapRectFromScene(0, 0, m_effectRadius, m_effectRadius);
        qreal effectiveDistance = qMax(effectiveRect.width(), effectiveRect.height());
        section = queryable->windowFrameSectionAt(QRectF(event->pos() - QPointF(effectiveDistance, effectiveDistance), QSizeF(2 * effectiveDistance, 2 * effectiveDistance)));
        cursor = queryable->windowCursorAt(section);
    }

    if(cursor.shape() != widget->cursor().shape() || cursor.shape() == Qt::BitmapCursor)
        widget->setCursor(cursor);
    m_affectedItems.insert(widget);

    return false;
}

bool ResizeHoverInstrument::hoverLeaveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *) {
    if(!m_affectedItems.contains(item))
        return false;

    item->unsetCursor();

    return false;
}


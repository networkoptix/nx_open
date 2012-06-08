#include "resize_hover_instrument.h"
#include <QGraphicsItem>
#include <QGraphicsWidget>
#include <QGraphicsSceneHoverEvent>
#include <ui/common/geometry.h>
#include <ui/common/frame_section_queryable.h>

namespace {
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
    m_effectiveDistance(0.0),
    m_effective(true)
{}

bool ResizeHoverInstrument::registeredNotify(QGraphicsItem *item) {
    if(!item->isWidget())
        return false;

    FrameSectionQuearyable *queryable = dynamic_cast<FrameSectionQuearyable *>(item);
    if(queryable != NULL)
        m_queryableByItem.insert(item, queryable);

    return true;
}

void ResizeHoverInstrument::unregisteredNotify(QGraphicsItem *item) {
    m_queryableByItem.remove(item);
}

bool ResizeHoverInstrument::hoverMoveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) {
    QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item);
    FrameSectionQuearyable *queryable = m_queryableByItem.value(item);
    if(!queryable && !((widget->windowFlags() & Qt::Window) && (widget->windowFlags() & Qt::WindowTitleHint)))
        return false; /* Has no decorations and not queryable for frame sections. */

    Qt::WindowFrameSection section;
    if(!m_effective) {
        section = Qt::NoSection;
    } else {
        if(queryable == NULL) {
            section = open(widget)->getWindowFrameSectionAt(event->pos());
        } else {
            QRectF effectiveRect = item->mapRectFromScene(0, 0, m_effectiveDistance, m_effectiveDistance);
            qreal effectiveDistance = qMax(effectiveRect.width(), effectiveRect.height());
            section = queryable->windowFrameSectionAt(QRectF(event->pos() - QPointF(effectiveDistance, effectiveDistance), QSizeF(2 * effectiveDistance, 2 * effectiveDistance)));
        }
    }

    Qt::CursorShape cursorShape;
    switch (section) {
    case Qt::TopLeftSection:
    case Qt::BottomRightSection:
        cursorShape = Qt::SizeFDiagCursor;
        break;
    case Qt::TopRightSection:
    case Qt::BottomLeftSection:
        cursorShape = Qt::SizeBDiagCursor;
        break;
    case Qt::LeftSection:
    case Qt::RightSection:
        cursorShape = Qt::SizeHorCursor;
        break;
    case Qt::TopSection:
    case Qt::BottomSection:
        cursorShape = Qt::SizeVerCursor;
        break;
    default:
        cursorShape = Qt::ArrowCursor;
        break;
    }

    if(widget->cursor().shape() != cursorShape)
        widget->setCursor(cursorShape);

    return false;
}

bool ResizeHoverInstrument::hoverLeaveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *) {
    QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item);
    if(!hasDecoration(widget))
        return false;

    widget->unsetCursor();
    return false;
}


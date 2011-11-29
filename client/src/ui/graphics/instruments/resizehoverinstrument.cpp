#include "resizehoverinstrument.h"
#include <QGraphicsItem>
#include <QGraphicsWidget>
#include <QGraphicsSceneHoverEvent>
#include <ui/common/scene_utility.h>
#include <ui/common/frame_section_queryable.h>

namespace {
    bool hasDecoration(QGraphicsWidget *widget) {
        return (widget->windowFlags() & Qt::Window) && (widget->windowFlags() & Qt::WindowTitleHint);
    }

} // anonymous namespace 

ResizeHoverInstrument::ResizeHoverInstrument(QObject *parent):
    Instrument(ITEM, makeSet(QEvent::GraphicsSceneHoverMove, QEvent::GraphicsSceneHoverLeave), parent),
    m_effectiveDistance(0.0)
{}

bool ResizeHoverInstrument::registeredNotify(QGraphicsItem *item) {
    if(!item->isWidget())
        return false;

    FrameSectionQuearyable *queryable = dynamic_cast<FrameSectionQuearyable *>(item);
    if(queryable == NULL)
        return false;

    m_queryableByItem.insert(item, queryable);
    return true;
}

void ResizeHoverInstrument::unregisteredNotify(QGraphicsItem *item) {
    m_queryableByItem.remove(item);
}

bool ResizeHoverInstrument::hoverMoveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) {
    QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item);
    if (!hasDecoration(widget))
        return false;

    FrameSectionQuearyable *queryable = m_queryableByItem.value(item);

    QRectF effectiveRect = item->mapRectFromScene(0, 0, m_effectiveDistance, m_effectiveDistance);
    qreal effectiveDistance = qMax(effectiveRect.width(), effectiveRect.height());
    Qt::WindowFrameSection section = queryable->windowFrameSectionAt(QRectF(event->pos() - QPointF(effectiveDistance, effectiveDistance), QSizeF(2 * effectiveDistance, 2 * effectiveDistance)));

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

bool ResizeHoverInstrument::hoverLeaveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) {
    QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item);
    if(!hasDecoration(widget))
        return false;

    widget->unsetCursor();
    return false;
}


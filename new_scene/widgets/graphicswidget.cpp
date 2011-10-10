#include "graphicswidget.h"
#include "graphicswidget_p.h"
#include <cassert>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>

bool GraphicsWidgetPrivate::isResizeGrip(Qt::WindowFrameSection section) const
{
    return section != Qt::NoSection && section != Qt::TitleBarArea;
}

bool GraphicsWidgetPrivate::isDragGrip(Qt::WindowFrameSection section) const 
{
    return section == Qt::TitleBarArea;
}

void GraphicsWidgetPrivate::draggingResizingFinished()
{
    Q_Q(GraphicsWidget);

    if(resizing) 
    {
        resizing = false;

        if(resizingStartedEmitted)
        {
            resizingStartedEmitted = false;
            Q_EMIT q->resizingFinished();
        }
    }

    if(dragging)
    {
        dragging = false;

        if(draggingStartedEmitted)
        {
            draggingStartedEmitted = false;
            Q_EMIT q->draggingFinished();
        }
    }
}

GraphicsWidget::GraphicsWidget(QGraphicsItem *parent):
    base_type(parent),
    d_ptr(new GraphicsWidgetPrivate(this))
{}

GraphicsWidget::GraphicsWidget(QGraphicsItem *parent, GraphicsWidgetPrivate *dd):
    base_type(parent),
    d_ptr(dd)
{
    assert(dd != NULL);
}

GraphicsWidget::~GraphicsWidget()
{
    return;
}

GraphicsWidget::GraphicsExtraFlags GraphicsWidget::extraFlags() const
{
    return d_func()->extraFlags;
}

void GraphicsWidget::setExtraFlag(GraphicsExtraFlag flag, bool enabled)
{
    Q_D(GraphicsWidget);

    if (enabled)
        setExtraFlags(d->extraFlags | flag);
    else
        setExtraFlags(d->extraFlags & ~flag);
}

void GraphicsWidget::setExtraFlags(GraphicsExtraFlags flags)
{
    Q_D(GraphicsWidget);

    if (d->extraFlags == flags)
        return;

    flags = GraphicsExtraFlags(itemChange(ItemExtraFlagsChange, quint32(flags)).toUInt());
    if (d->extraFlags == flags)
        return;

    d->extraFlags = flags;
    itemChange(ItemExtraFlagsHaveChanged, quint32(flags));
}

void GraphicsWidget::mousePressEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(GraphicsWidget);

    base_type::mousePressEvent(event);

    if((flags() & ItemIsMovable) && event->button() == Qt::LeftButton) 
        d->dragging = true;

    /* This event must be accepted in order to receive the corresponding mouse release event. */
    event->accept();
}

void GraphicsWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(GraphicsWidget);

    /* This will call mousePressEvent. */
    base_type::mouseDoubleClickEvent(event);

    if(event->button() == Qt::LeftButton)
        d->doubleClicked = true;
}

void GraphicsWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(GraphicsWidget);

    if(d->dragging && !d->draggingStartedEmitted)
    {
        Q_EMIT draggingStarted();
        d->draggingStartedEmitted = true;
    }

    base_type::mouseMoveEvent(event);
}

void GraphicsWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(GraphicsWidget);

    base_type::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton)
    {
        d->draggingResizingFinished();

        if ((event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength() < QApplication::startDragDistance())
        {
            if(d->doubleClicked)
            {
                Q_EMIT doubleClicked();
            }
            else
            {
                Q_EMIT clicked();
            }
        }

        d->doubleClicked = false;
    }
}

bool GraphicsWidget::windowFrameEvent(QEvent *event) 
{
    Q_D(GraphicsWidget);

    /* Note that default implementation of windowFrameEvent returns false for
     * mouse release events if the mouse is located outside the item's boundaries.
     * 
     * Therefore we cannot leave early based on the return value of windowFrameEvent. */
    bool result = false; 

    switch(event->type()) {
    case QEvent::GraphicsSceneMousePress: 
    {
        result = base_type::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        if(e->button() == Qt::LeftButton)
        {
            Qt::WindowFrameSection section = windowFrameSectionAt(e->pos());

            if(d->isResizeGrip(section))
                d->resizing = true;
            
            if(d->isDragGrip(section))
                d->dragging = true;
        }

        break;
    }
    case QEvent::GraphicsSceneMouseMove:
    {
        if(d->resizing && !d->resizingStartedEmitted) 
        {
            Q_EMIT resizingStarted();
            d->resizingStartedEmitted = true;
        }

        if(d->dragging && !d->draggingStartedEmitted)
        {
            Q_EMIT draggingStarted();
            d->draggingStartedEmitted = true;
        }

        result = base_type::windowFrameEvent(event);
        break;
    }
    case QEvent::GraphicsSceneMouseRelease: 
    {
        result = base_type::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        if(e->button() == Qt::LeftButton) {
            if(!d->draggingStartedEmitted && !d->resizingStartedEmitted)
                Q_EMIT clicked();

            d->draggingResizingFinished();
        }

        break;
    }
    case QEvent::GraphicsSceneHoverLeave:
    {
        result = base_type::windowFrameEvent(event);

        /* In some cases we won't receive release event for left button, 
         * but we still need to emit the signals. */
        d->draggingResizingFinished();

        break;
    }
    default:
        result = base_type::windowFrameEvent(event);
        break;
    }

    return result;
}

Qt::WindowFrameSection GraphicsWidget::windowFrameSectionAt(const QPointF &pos) const 
{
    Q_D(const GraphicsWidget);

    Qt::WindowFrameSection result = base_type::windowFrameSectionAt(pos);
    
    if(!(d->extraFlags & ItemIsResizable) && d->isResizeGrip(result))
        return Qt::NoSection;

    return result;
}
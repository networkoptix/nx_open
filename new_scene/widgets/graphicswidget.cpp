#include "graphicswidget.h"
#include "graphicswidget_p.h"

#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QStyleOption>

void GraphicsWidgetPrivate::draggingResizingFinished()
{
    Q_Q(GraphicsWidget);

    if (resizing)
    {
        resizing = false;

        if (resizingStartedEmitted)
        {
            resizingStartedEmitted = false;
            Q_EMIT q->resizingFinished();
        }
    }

    if (dragging)
    {
        dragging = false;

        if (draggingStartedEmitted)
        {
            draggingStartedEmitted = false;
            Q_EMIT q->draggingFinished();
        }
    }
}


GraphicsWidget::GraphicsWidget(QGraphicsItem *parent):
    QGraphicsWidget(parent),
    d_ptr(new GraphicsWidgetPrivate)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::GraphicsWidget(QGraphicsItem *parent, GraphicsWidgetPrivate &dd):
    QGraphicsWidget(parent), d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::~GraphicsWidget()
{
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

    flags = GraphicsExtraFlags(itemChange(ItemExtraFlagsChange, quint32(flags)).toUInt());
    if (d->extraFlags == flags)
        return;

    d->extraFlags = flags;
    itemChange(ItemExtraFlagsHasChanged, quint32(flags));
}

/*!
    \reimp
*/
void GraphicsWidget::initStyleOption(QStyleOption *option) const
{
    Q_D(const GraphicsWidget);

    QGraphicsWidget::initStyleOption(option);
    // QTBUG-18797: When setting the flag ItemIgnoresTransformations for an item,
    // it will receive mouse events as if it was transformed by the view.
    if (d->isUnderMouse)
        option->state |= QStyle::State_MouseOver;
}

/*!
    \reimp
*/
bool GraphicsWidget::event(QEvent *event)
{
    Q_D(GraphicsWidget);

    switch (event->type()) {
    case QEvent::GraphicsSceneHoverEnter:
        d->isUnderMouse = true;
        break;
    case QEvent::GraphicsSceneHoverLeave:
        d->isUnderMouse = false;
        break;

    default:
        break;
    }

    return QGraphicsWidget::event(event);
}

void GraphicsWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsWidget);

    QGraphicsWidget::mousePressEvent(event);

    if ((flags() & ItemIsMovable) && event->button() == Qt::LeftButton)
        d->dragging = true;

    /* This event must be accepted in order to receive the corresponding mouse release event. */
    event->accept();
}

void GraphicsWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsWidget);

    /* This will call mousePressEvent. */
    QGraphicsWidget::mouseDoubleClickEvent(event);

    if (event->button() == Qt::LeftButton)
        d->doubleClicked = true;
}

void GraphicsWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsWidget);

    if (d->dragging && !d->draggingStartedEmitted)
    {
        Q_EMIT draggingStarted();
        d->draggingStartedEmitted = true;
    }

    QGraphicsWidget::mouseMoveEvent(event);
}

void GraphicsWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsWidget);

    QGraphicsWidget::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton)
    {
        d->draggingResizingFinished();

        if ((event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength() < QApplication::startDragDistance())
        {
            if (d->doubleClicked)
                Q_EMIT doubleClicked();
            else
                Q_EMIT clicked();
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
        result = QGraphicsWidget::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);
        if(e->button() == Qt::LeftButton)
        {
            Qt::WindowFrameSection section = windowFrameSectionAt(e->pos());
            if (d->isResizeGrip(section))
                d->resizing = true;
            if (d->isDragGrip(section))
                d->dragging = true;
        }

        break;
    }
    case QEvent::GraphicsSceneMouseMove:
    {
        if (d->resizing && !d->resizingStartedEmitted)
        {
            Q_EMIT resizingStarted();
            d->resizingStartedEmitted = true;
        }

        if (d->dragging && !d->draggingStartedEmitted)
        {
            Q_EMIT draggingStarted();
            d->draggingStartedEmitted = true;
        }

        result = QGraphicsWidget::windowFrameEvent(event);
        break;
    }
    case QEvent::GraphicsSceneMouseRelease:
    {
        result = QGraphicsWidget::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);
        if (e->button() == Qt::LeftButton) {
            if(!d->draggingStartedEmitted && !d->resizingStartedEmitted)
                Q_EMIT clicked();

            d->draggingResizingFinished();
        }

        break;
    }
    case QEvent::GraphicsSceneHoverLeave:
    {
        result = QGraphicsWidget::windowFrameEvent(event);

        /* In some cases we won't receive release event for left button,
         * but we still need to emit the signals. */
        d->draggingResizingFinished();

        break;
    }
    default:
        result = QGraphicsWidget::windowFrameEvent(event);
        break;
    }

    return result;
}

Qt::WindowFrameSection GraphicsWidget::windowFrameSectionAt(const QPointF &pos) const
{
    Q_D(const GraphicsWidget);

    Qt::WindowFrameSection result = QGraphicsWidget::windowFrameSectionAt(pos);
    if (!(d->extraFlags & ItemIsResizable) && d->isResizeGrip(result))
        return Qt::NoSection;

    return result;
}

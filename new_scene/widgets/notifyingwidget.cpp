#include "notifyingwidget.h"
#include "notifyingwidget_p.h"
#include <QApplication>
#include <QGraphicsSceneMouseEvent>

bool NotifyingWidgetPrivate::isResizeGrip(Qt::WindowFrameSection section) const
{
    return section != Qt::NoSection && section != Qt::TitleBarArea;
}

bool NotifyingWidgetPrivate::isDragGrip(Qt::WindowFrameSection section) const 
{
    return section == Qt::TitleBarArea;
}

NotifyingWidget::NotifyingWidget(QGraphicsItem *parent):
    base_type(parent),
    PimplBase(new NotifyingWidgetPrivate())
{}

NotifyingWidget::NotifyingWidget(QGraphicsItem *parent, NotifyingWidgetPrivate *dd):
    base_type(parent),
    PimplBase(dd)
{}

NotifyingWidget::~NotifyingWidget()
{
    return;
}

void NotifyingWidget::mousePressEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(NotifyingWidget);

    base_type::mousePressEvent(event);

    if((flags() & ItemIsMovable) && event->button() == Qt::LeftButton) 
        d->dragging = true;

    /* This event must be accepted in order to receive the corresponding mouse release event. */
    event->accept();
}

void NotifyingWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(NotifyingWidget);

    /* This will call mousePressEvent. */
    base_type::mouseDoubleClickEvent(event);

    if(event->button() == Qt::LeftButton)
        d->doubleClicked = true;
}

void NotifyingWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(NotifyingWidget);

    if(d->dragging && !d->draggingStartedEmitted)
    {
        Q_EMIT draggingStarted();
        d->draggingStartedEmitted = true;
    }

    base_type::mouseMoveEvent(event);
}

void NotifyingWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(NotifyingWidget);

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

bool NotifyingWidget::windowFrameEvent(QEvent *event) 
{
    Q_D(NotifyingWidget);

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

void NotifyingWidgetPrivate::draggingResizingFinished()
{
    Q_Q(NotifyingWidget);

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
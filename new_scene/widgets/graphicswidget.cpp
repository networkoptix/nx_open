#include "graphicswidget.h"
#include "graphicswidget_p.h"
#include <cassert>
#include <QGraphicsScene>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QStyleOption>

namespace {
    class DragStore: public QObject {
    public:
        DragStore(QObject *parent = NULL): 
            QObject(parent),
            m_draggedWidget(NULL)
        {}
        
        static DragStore *dragStoreOf(QObject *object)
        {
            if(object == NULL)
                return NULL;

            static const char *const name = "_qn_dragStore";

            DragStore *dragStore = static_cast<DragStore *>(object->findChild<QObject *>(QLatin1String(name)));
            if(dragStore != NULL)
                return dragStore;

            dragStore = new DragStore(object);
            dragStore->setObjectName(QLatin1String(name));
            return dragStore;
        }

        void setCurrentDraggedWidget(GraphicsWidget *widget)
        {
            m_draggedWidget = widget;
        }

        GraphicsWidget *currentDraggedWidget() const 
        {
            return m_draggedWidget;
        }

    private:
        GraphicsWidget *m_draggedWidget;
    };


    class DragFilter: public QObject {
    public:
        DragFilter(QObject *parent = NULL):
            QObject(parent)
        {}

        static const char *tokenPropertyName() 
        {
            return "_qn_dragToken";
        }

        static QLatin1String tokenMimeType()
        {
            return QLatin1String("application/x-qn-graphics-widget-token");
        }

        static void ensureInstalledAt(QObject *object)
        {
            if(object == NULL)
                return;

            static const char *const name = "_qn_dragFilter";

            DragFilter *dragFilter = static_cast<DragFilter *>(object->findChild<QObject *>(QLatin1String(name)));
            if(dragFilter != NULL)
                return;

            dragFilter = new DragFilter(object);
            dragFilter->setObjectName(QLatin1String(name));
            object->installEventFilter(dragFilter);
            return;
        }

        virtual bool eventFilter(QObject *watched, QEvent *event)
        {
            switch(event->type()) 
            {
            case QEvent::GraphicsSceneDragEnter:
                return dragEnterEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
            case QEvent::GraphicsSceneDragMove:
                return dragMoveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
            case QEvent::GraphicsSceneDragLeave:
                return dragLeaveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
            case QEvent::GraphicsSceneDrop:
                return dropEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
            default:
                return false;
            }
        }

    private:
        bool dragEnterEvent(QObject *watched, QGraphicsSceneDragDropEvent *event)
        {
            DragStore *dragStore = DragStore::dragStoreOf(watched);
            if(dragStore == NULL)
                return false;

            GraphicsWidget *widget = dragStore->currentDraggedWidget();
            if(widget == NULL)
                return false;

            if(!event->mimeData()->hasFormat(tokenMimeType()))
                return false;

            QByteArray token = event->mimeData()->data(tokenMimeType());
            if(token != widget->property(tokenPropertyName()).toByteArray())
                return false;
            
            event->accept();
            return true;
        }

        bool dragMoveEvent(QObject *watched, QGraphicsSceneDragDropEvent *event)
        {
            return false;
        }

        bool dragLeaveEvent(QObject *watched, QGraphicsSceneDragDropEvent *event)
        {
            return false;
        }

        bool dropEvent(QObject *watched, QGraphicsSceneDragDropEvent *event)
        {
            return false;
        }
    };


} // anonymous namespace

void GraphicsWidgetPrivate::movingResizingFinished()
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

    if(moving)
    {
        moving = false;

        if(movingStartedEmitted)
        {
            movingStartedEmitted = false;
            Q_EMIT q->movingFinished();
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

    if ((flags() & ItemIsMovable) && !(extraFlags() & ItemIsDraggable) && event->button() == Qt::LeftButton) 
        d->moving = true; 

    if ((extraFlags() & ItemIsDraggable) && event->button() == Qt::LeftButton)
        d->preDragging = true;

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

    if (d->preDragging)
    {
        if ((event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength() > QApplication::startDragDistance())
        {
            d->preDragging = false;
            drag(event);
        }

        return;
    }

    if(d->moving && !d->movingStartedEmitted)
    {
        Q_EMIT movingStarted();
        d->movingStartedEmitted = true;
    }

    QGraphicsWidget::mouseMoveEvent(event);
}

void GraphicsWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsWidget);

    QGraphicsWidget::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton)
    {
        d->movingResizingFinished();

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
        result = QGraphicsWidget::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        if(e->button() == Qt::LeftButton)
        {
            Qt::WindowFrameSection section = windowFrameSectionAt(e->pos());
            if (d->isResizeGrip(section))
                d->resizing = true;
            if(d->isMoveGrip(section))
            {
                if(extraFlags() & ItemIsDraggable)
                {
                    d->preDragging = true;
                }
                else 
                {
                    d->moving = true;
                }
            }
        }

        break;
    }
    case QEvent::GraphicsSceneMouseMove:
    {
        if(d->preDragging)
            return false;

        if (d->resizing && !d->resizingStartedEmitted)
        {
            Q_EMIT resizingStarted();
            d->resizingStartedEmitted = true;
        }

        if(d->moving && !d->movingStartedEmitted)
        {
            Q_EMIT movingStarted();
            d->movingStartedEmitted = true;
        }

        result = QGraphicsWidget::windowFrameEvent(event);
        break;
    }
    case QEvent::GraphicsSceneMouseRelease:
    {
        result = QGraphicsWidget::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);
        if (e->button() == Qt::LeftButton) {
            if(!d->movingStartedEmitted && !d->resizingStartedEmitted)
                Q_EMIT clicked();

            d->movingResizingFinished();
        }

        break;
    }
    case QEvent::GraphicsSceneHoverLeave:
    {
        result = QGraphicsWidget::windowFrameEvent(event);

        /* In some cases we won't receive release event for left button,
         * but we still need to emit the signals. */
        d->movingResizingFinished();

        break;
    }
    default:
        result = QGraphicsWidget::windowFrameEvent(event);
        break;
    }

    return result;
}

QVariant GraphicsWidget::itemChange(GraphicsItemChange change, const QVariant &value) 
{
    if(change == ItemSceneHasChanged)
        DragFilter::ensureInstalledAt(scene());

    return QGraphicsWidget::itemChange(change, value);
}

Qt::WindowFrameSection GraphicsWidget::windowFrameSectionAt(const QPointF &pos) const
{
    return filterWindowFrameSection(QGraphicsWidget::windowFrameSectionAt(pos));
}

Qt::WindowFrameSection GraphicsWidget::filterWindowFrameSection(Qt::WindowFrameSection section) const
{
    Q_D(const GraphicsWidget);

    if(!(extraFlags() & ItemIsResizable) && d->isResizeGrip(section))
        return Qt::NoSection;

    if(!(flags() & ItemIsMovable) && d->isMoveGrip(section))
        return Qt::NoSection;

    return section;
}

QDrag *GraphicsWidget::createDrag(QGraphicsSceneMouseEvent *event)
{
    QDrag *drag = new QDrag(event->widget());
    
    QMimeData *mimeData = new QMimeData();
    drag->setMimeData(mimeData);

    return drag;
}

void GraphicsWidget::startDrag(QDrag *drag)
{
    drag->exec();
}

void GraphicsWidget::drag(QGraphicsSceneMouseEvent *event)
{
    QByteArray token = QString::number(qrand()).toLatin1();
    setProperty(DragFilter::tokenPropertyName(), token);

    DragStore *dragStore = DragStore::dragStoreOf(scene());
    dragStore->setCurrentDraggedWidget(this);

    QDrag *drag = createDrag(event);
    if(drag->mimeData() != NULL)
        drag->mimeData()->setData(DragFilter::tokenMimeType(), token);
    startDrag(drag);

    dragStore->setCurrentDraggedWidget(NULL);
    setProperty(DragFilter::tokenPropertyName(), QVariant());
}


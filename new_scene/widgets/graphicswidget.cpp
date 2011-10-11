#include "graphicswidget.h"
#include "graphicswidget_p.h"
#include <cassert>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>

namespace {
    class DragStore: public QObject {
    public:
        DragStore(QObject *parent = NULL): 
            QObject(parent),
            m_draggedWidget(NULL)
        {}
        
        static DragStore *dragStoreOf(QObject *object)
        {
            assert(object != NULL);

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

bool GraphicsWidgetPrivate::isResizeGrip(Qt::WindowFrameSection section) const
{
    return section != Qt::NoSection && section != Qt::TitleBarArea;
}

bool GraphicsWidgetPrivate::isMoveGrip(Qt::WindowFrameSection section) const 
{
    return section == Qt::TitleBarArea;
}

void GraphicsWidgetPrivate::movingResizingFinished()
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
    base_type::mouseDoubleClickEvent(event);

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

    base_type::mouseMoveEvent(event);
}

void GraphicsWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) 
{
    Q_D(GraphicsWidget);

    base_type::mouseReleaseEvent(event);

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
        result = base_type::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        if(e->button() == Qt::LeftButton)
        {
            Qt::WindowFrameSection section = windowFrameSectionAt(e->pos());

            if(d->isResizeGrip(section))
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

        if(d->resizing && !d->resizingStartedEmitted) 
        {
            Q_EMIT resizingStarted();
            d->resizingStartedEmitted = true;
        }

        if(d->moving && !d->movingStartedEmitted)
        {
            Q_EMIT movingStarted();
            d->movingStartedEmitted = true;
        }

        result = base_type::windowFrameEvent(event);
        break;
    }
    case QEvent::GraphicsSceneMouseRelease: 
    {
        result = base_type::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        if(e->button() == Qt::LeftButton) {
            if(!d->movingStartedEmitted && !d->resizingStartedEmitted)
                Q_EMIT clicked();

            d->movingResizingFinished();
        }

        break;
    }
    case QEvent::GraphicsSceneHoverLeave:
    {
        result = base_type::windowFrameEvent(event);

        /* In some cases we won't receive release event for left button, 
         * but we still need to emit the signals. */
        d->movingResizingFinished();

        break;
    }
    default:
        result = base_type::windowFrameEvent(event);
        break;
    }

    return result;
}

QVariant GraphicsWidget::itemChange(GraphicsItemChange change, const QVariant &value) 
{
    if(change == ItemSceneHasChanged)
        DragFilter::ensureInstalledAt(scene());

    return base_type::itemChange(change, value);
}

Qt::WindowFrameSection GraphicsWidget::windowFrameSectionAt(const QPointF &pos) const 
{
    return filterWindowFrameSection(base_type::windowFrameSectionAt(pos));
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


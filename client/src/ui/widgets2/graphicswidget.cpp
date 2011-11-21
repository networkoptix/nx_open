#include "graphicswidget.h"
#include "graphicswidget_p.h"
#include <cassert>
#include <QGraphicsScene>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QStyleOption>

#include <QDebug>

namespace {

    class DragFilter: public QObject {
    public:
        DragFilter(QGraphicsScene *scene):
            QObject(scene)
        {
            if (scene == NULL)
                return;

            setObjectName(staticObjectName());
            scene->installEventFilter(this);
        }

        static void ensureInstalledAt(QGraphicsScene *scene)
        {
            if (scene == NULL)
                return;

            if (scene->findChild<QObject *>(staticObjectName()) != NULL)
                return;

            new DragFilter(scene);
            return;
        }

        static DragFilter *dragFilterOf(QGraphicsScene *scene)
        {
            if (scene == NULL)
                return NULL;

            return static_cast<DragFilter *>(scene->findChild<QObject *>(staticObjectName()));
        }

        virtual bool eventFilter(QObject *watched, QEvent *event) override
        {
            switch (event->type())
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

        void dragStarted(GraphicsWidget *draggerWidget, const QList<QGraphicsItem *> &draggedItems, const QPointF &buttonDownScenePos)
        {
            m_draggerWidget = draggerWidget;
            m_draggedItems = draggedItems;
            m_initialScenePos = m_lastScenePos = buttonDownScenePos;
            m_droppedOnSelf = false;
        }

        void dragEnded()
        {
            m_draggerWidget = NULL;
            m_draggedItems.clear();
        }

        bool droppedOnSelf() const
        {
            return m_droppedOnSelf;
        }

        QPointF totalDelta() const
        {
            return m_lastScenePos - m_initialScenePos;
        }

    private:
        static QLatin1String staticObjectName()
        {
            return QLatin1String("_qn_dragFilter");
        }

        bool dragEnterEvent(QObject *, QGraphicsSceneDragDropEvent *event)
        {
            if (!inLocalDrag())
                return false;

            event->setProposedAction(Qt::MoveAction);
            event->acceptProposedAction();

            moveItems(event);

            return true;
        }

        bool dragMoveEvent(QObject *, QGraphicsSceneDragDropEvent *event)
        {
            if (!inLocalDrag())
                return false;

            event->setProposedAction(Qt::MoveAction);
            event->acceptProposedAction();

            moveItems(event);

            return true;
        }

        bool dragLeaveEvent(QObject *, QGraphicsSceneDragDropEvent *event)
        {
            if (!inLocalDrag())
                return false;

            event->setProposedAction(Qt::MoveAction);
            event->acceptProposedAction();

            moveItems(event);

            return true;
        }

        bool dropEvent(QObject *, QGraphicsSceneDragDropEvent *event)
        {
            if (!inLocalDrag())
                return false;

            event->setProposedAction(Qt::MoveAction);
            event->acceptProposedAction();

            moveItems(event);
            m_droppedOnSelf = true;

            return true;
        }

        void moveItems(QGraphicsSceneDragDropEvent *event)
        {
            foreach (QGraphicsItem *item, m_draggedItems)
                item->setPos(item->pos() + item->mapFromScene(event->scenePos()) - item->mapFromScene(m_lastScenePos));

            m_lastScenePos = event->scenePos();
        }

        bool inLocalDrag() const
        {
            return m_draggerWidget != NULL;
        }

    private:
        GraphicsWidget *m_draggerWidget;
        QList<QGraphicsItem *> m_draggedItems;
        QPointF m_lastScenePos;
        QPointF m_initialScenePos;
        bool m_droppedOnSelf;
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

    if (moving)
    {
        moving = false;

        if (movingStartedEmitted)
        {
            movingStartedEmitted = false;
            Q_EMIT q->movingFinished();
        }
    }
}


GraphicsWidget::GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags f)
    : base_type(parent, f), d_ptr(new GraphicsWidgetPrivate)
{
    d_ptr->q_ptr = this;

    setAcceptHoverEvents(true);

    DragFilter::ensureInstalledAt(scene());
}

GraphicsWidget::GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags f)
    : base_type(parent, f), d_ptr(&dd)
{
    d_ptr->q_ptr = this;

    setAcceptHoverEvents(true);

    DragFilter::ensureInstalledAt(scene());
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

    base_type::initStyleOption(option);
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

    return base_type::event(event);
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

    if (d->moving && !d->movingStartedEmitted)
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
            if (d->doubleClicked)
            {
                //Q_EMIT doubleClicked();
            }
            else
            {
                //Q_EMIT clicked();
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

    switch (event->type()) {
    case QEvent::GraphicsSceneMousePress:
    {
        result = base_type::windowFrameEvent(event);
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        if (e->button() == Qt::LeftButton)
        {
            Qt::WindowFrameSection section = windowFrameSectionAt(e->pos());
            if (d->isResizeGrip(section)) {
                d->resizing = true;
                d->preDragging = false;
            }
            if (d->isMoveGrip(section))
            {
                if (extraFlags() & ItemIsDraggable)
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
        if (d->preDragging)
            return false; /* Handle it at mouseMoveEvent. */

        if (d->resizing && !d->resizingStartedEmitted)
        {
            Q_EMIT resizingStarted();
            d->resizingStartedEmitted = true;
        }

        if (d->moving && !d->movingStartedEmitted)
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
        if (e->button() == Qt::LeftButton) {
            if (!d->movingStartedEmitted && !d->resizingStartedEmitted)
                //Q_EMIT clicked();

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
    if (change == ItemSceneHasChanged)
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

    if (!(extraFlags() & ItemIsResizable) && d->isResizeGrip(section))
        return Qt::NoSection;

    if (!(flags() & ItemIsMovable) && d->isMoveGrip(section))
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

Qt::DropAction GraphicsWidget::startDrag(QDrag *drag)
{
    return drag->exec();
}

void GraphicsWidget::endDrag(Qt::DropAction dropAction)
{
    if (dropAction == Qt::MoveAction)
    {
        foreach (QGraphicsItem *item, scene()->selectedItems())
        {
            if (item == this)
                continue;

            delete item;
        }

        this->deleteLater();
    }
}

void GraphicsWidget::drag(QGraphicsSceneMouseEvent *event)
{
    DragFilter *dragFilter = DragFilter::dragFilterOf(scene());

    QList<QGraphicsItem *> draggedItems = scene()->selectedItems();
    if (!draggedItems.contains(this))
        draggedItems.push_back(this);
    dragFilter->dragStarted(this, draggedItems, event->buttonDownScenePos(Qt::LeftButton));

    if (flags() & ItemIsMovable)
        Q_EMIT movingStarted();

    QDrag *drag = createDrag(event);
    Qt::DropAction dropAction = startDrag(drag);
    if (dragFilter->droppedOnSelf())
    {
        dropAction = Qt::IgnoreAction;
    }
    else if (dropAction != Qt::MoveAction)
    {
        QPointF delta = -dragFilter->totalDelta();
        foreach (QGraphicsItem *item, draggedItems)
            item->setPos(item->pos() + item->mapFromScene(delta) - item->mapFromScene(QPointF(0, 0)));
    }
    endDrag(dropAction);

    if (flags() & ItemIsMovable)
        Q_EMIT movingFinished();

    dragFilter->dragEnded();
}


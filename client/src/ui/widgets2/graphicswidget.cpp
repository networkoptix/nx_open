#include "graphicswidget.h"
#include "graphicswidget_p.h"
#include <cassert>
#include <QGraphicsScene>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QStyleOption>

#include <QDebug>

#if 0
// Do not remove. 
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
#endif

GraphicsWidget::GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags f)
    : base_type(parent, f), d_ptr(new GraphicsWidgetPrivate)
{
    d_ptr->q_ptr = this;

    setAcceptHoverEvents(true);
}

GraphicsWidget::GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags f)
    : base_type(parent, f), d_ptr(&dd)
{
    d_ptr->q_ptr = this;

    setAcceptHoverEvents(true);
}

GraphicsWidget::~GraphicsWidget()
{
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

#if 0
// Do not remove. 
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
#endif

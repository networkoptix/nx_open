#include "move_instrument.h"

#include <QtGui/QMouseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsItem>

namespace {

const auto isNonProxyLeftClickableItem =
    [](QGraphicsItem *item)
    {
        return ((item->acceptedMouseButtons() & Qt::LeftButton) &&
            !dynamic_cast<QGraphicsProxyWidget*>(item));
    };

} // anonymous namespace

MoveInstrument::MoveInstrument(QObject *parent):
    DragProcessingInstrument(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_moveStartedEmitted(true)
{}

MoveInstrument::~MoveInstrument() {
    ensureUninstalled();
}

void MoveInstrument::moveItem(QGraphicsItem *item, const QPointF &sceneDeltaPos) const {
    item->setPos(
        item->pos() +
        item->mapToParent(item->mapFromScene(sceneDeltaPos)) -
        item->mapToParent(item->mapFromScene(QPointF()))
    );
}

bool MoveInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(!dragProcessor()->isWaiting())
        return false;

    QGraphicsView *view = this->view(viewport);
    if (!view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Find the item to drag. */
    QGraphicsItem *draggedItem = item(view, event->pos(), isNonProxyLeftClickableItem);
    if (draggedItem == NULL || !(draggedItem->flags() & QGraphicsItem::ItemIsMovable) || !satisfiesItemConditions(draggedItem))
        return false;
    m_draggedItem = draggedItem;

    /* Don't construct a list of all items to drag here, it's expensive. */

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

void MoveInstrument::startDragProcess(DragInfo *info) {
    emit moveProcessStarted(info->view());
}

void MoveInstrument::startDrag(DragInfo *info) {
    m_moveStartedEmitted = false;

    if(!draggedItem()) {
        reset();
        return;
    }

    /* If a user tries to ctrl-drag an item, we should select it when dragging starts. */
    if ((info->modifiers() & Qt::ControlModifier))
        draggedItem()->setSelected(true);

    m_draggedItems.clear();
    if(!(draggedItem()->flags() & QGraphicsItem::ItemIsSelectable)) {
        /* When dragging an item that is not selectable, currently selected items are not dragged. */
        m_draggedItems.push_back(draggedItem());
    } else {
        m_draggedItems = scene()->selectedItems();
    }

    emit moveStarted(info->view(), m_draggedItems.materialized());
    m_moveStartedEmitted = true;
}

void MoveInstrument::dragMove(DragInfo *info) {
    QPointF sceneDelta = info->mouseScenePos() - info->lastMouseScenePos();

    /* Drag selected items. */
    foreach (const WeakGraphicsItemPointer &item, m_draggedItems)
        if(item)
            moveItem(item.data(), sceneDelta);

    emit move(info->view(), info->mouseScenePos() - info->mousePressScenePos());
}

void MoveInstrument::finishDrag(DragInfo *info) {
    if(m_moveStartedEmitted)
        emit moveFinished(info->view(), m_draggedItems.materialized());
}

void MoveInstrument::finishDragProcess(DragInfo *info) {
    emit moveProcessFinished(info->view());
}

#include "move_instrument.h"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QMouseEvent>
#include <QApplication>

namespace {
    struct ItemAcceptsLeftMouseButton: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->acceptedMouseButtons() & Qt::LeftButton;
        }
    };

} // anonymous namespace

MoveInstrument::MoveInstrument(QObject *parent): 
    DragProcessingInstrument(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_effective(true)
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
    QGraphicsItem *draggedItem = item(view, event->pos(), ItemAcceptsLeftMouseButton());
    if (draggedItem == NULL || !(draggedItem->flags() & QGraphicsItem::ItemIsMovable))
        return false;
    m_draggedItem = draggedItem;

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

void MoveInstrument::startDragProcess(DragInfo *info) {
    emit moveProcessStarted(info->view());
}

void MoveInstrument::startDrag(DragInfo *info) {
    if(!m_effective)
        return;

    /* If a user tries to ctrl-drag an item, we should select it when dragging starts. */
    if ((info->modifiers() & Qt::ControlModifier) && draggedItem())
        draggedItem()->setSelected(true);

    emit moveStarted(info->view(), scene()->selectedItems());
}

void MoveInstrument::dragMove(DragInfo *info) {
    if(!m_effective)
        return;

    QPointF sceneDelta = info->mouseScenePos() - info->lastMouseScenePos();

    /* Drag selected items. */
    QList<QGraphicsItem *> selectedItems = scene()->selectedItems();
    foreach (QGraphicsItem *item, selectedItems)
        moveItem(item, sceneDelta);
    if(draggedItem() && !draggedItem()->isSelected())
        moveItem(draggedItem(), sceneDelta);

    emit move(info->view(), scene()->selectedItems(), info->mouseScenePos() - info->mousePressScenePos());
}

void MoveInstrument::finishDrag(DragInfo *info) {
    if(!m_effective)
        return;

    emit moveFinished(info->view(), scene() == NULL ? QList<QGraphicsItem *>() : scene()->selectedItems());
}

void MoveInstrument::finishDragProcess(DragInfo *info) {
    emit moveProcessFinished(info->view());
}

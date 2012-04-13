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

    /* If a user tries to ctrl-drag an item, we should select it when dragging starts. */
    if (event->modifiers() & Qt::ControlModifier) {
        m_itemToSelect = draggedItem;
    } else {
        m_itemToSelect.clear();
    }
    
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

    if(!m_itemToSelect.isNull()) {
        m_itemToSelect.data()->setSelected(true);
        m_itemToSelect.clear();
    }

    emit moveStarted(info->view(), scene()->selectedItems());
}

void MoveInstrument::dragMove(DragInfo *info) {
    if(!m_effective)
        return;

    QPointF delta = info->mouseScenePos() - info->lastMouseScenePos();

    /* Drag selected items. */
    foreach (QGraphicsItem *item, scene()->selectedItems())
        item->setPos(item->pos() + delta);

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

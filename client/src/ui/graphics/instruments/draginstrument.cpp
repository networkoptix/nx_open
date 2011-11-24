#include "draginstrument.h"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QMouseEvent>
#include <QApplication>

namespace {
    struct ItemIsMovable: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return (item->flags() & QGraphicsItem::ItemIsMovable) && (item->acceptedMouseButtons() & Qt::LeftButton);
        }
    };

} // anonymous namespace

DragInstrument::DragInstrument(QObject *parent): 
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent)
{}

DragInstrument::~DragInstrument() {
    ensureUninstalled();
}

bool DragInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(!processor()->isWaiting())
        return false;

    QGraphicsView *view = this->view(viewport);
    if (!view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Find the item to drag. */
    QGraphicsItem *draggedItem = item(view, event->pos(), ItemIsMovable());
    if (draggedItem == NULL)
        return false;

    /* If a user tries to ctrl-drag an item, we should select it when dragging starts. */
    if (event->modifiers() & Qt::ControlModifier) {
        m_itemToSelect = draggedItem;
    } else {
        m_itemToSelect = NULL;
    }

    processor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

bool DragInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    processor()->mouseMoveEvent(viewport, event);

    event->accept();
    return false;
}

bool DragInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    processor()->mouseReleaseEvent(viewport, event);

    event->accept();
    return false;
}

bool DragInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    processor()->paintEvent(viewport, event);

    return false;
}

void DragInstrument::startDrag() {
    if(m_itemToSelect != NULL) {
        m_itemToSelect->setSelected(true);
        m_itemToSelect = NULL;
    }

    emit draggingStarted(processor()->view(), scene()->selectedItems());
}

void DragInstrument::drag() {
    QPointF delta = processor()->mouseScenePos() - processor()->lastMouseScenePos();

    /* Drag selected items. */
    foreach (QGraphicsItem *item, scene()->selectedItems())
        item->setPos(item->pos() + delta);
}

void DragInstrument::finishDrag() {
    emit draggingFinished(processor()->view(), scene() == NULL ? QList<QGraphicsItem *>() : scene()->selectedItems());
}


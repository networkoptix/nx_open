#include "draginstrument.h"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QMouseEvent>
#include <QApplication>

namespace {
    struct ItemIsMovable: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->flags() & QGraphicsItem::ItemIsMovable;
        }
    };

} // anonymous namespace

DragInstrument::DragInstrument(QObject *parent): 
    Instrument(makeSet(), makeSet(), makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease), false, parent) 
{}

void DragInstrument::installedNotify() {
    m_state = INITIAL;

    Instrument::installedNotify();
}

void DragInstrument::stopCurrentOperation() {
    m_state = INITIAL;
}

bool DragInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state != INITIAL || !view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Find the item to drag. */
    QGraphicsItem *draggedItem = item(view, event->pos(), ItemIsMovable());
    if (draggedItem == NULL)
        return false;

    m_state = PREPAIRING;
    m_mousePressPos = event->pos();
    m_lastMouseScenePos = view->mapToScene(event->pos());

    /* If a user tries to ctrl-drag an item, we should select it when dragging starts. */
    if (event->modifiers() & Qt::ControlModifier) {
        m_itemToSelect = draggedItem;
    } else {
        m_itemToSelect = NULL;
    }

    event->accept();
    return false;
}

bool DragInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state == INITIAL || !view->isInteractive())
        return false;

    /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release events). */
    if (!(event->buttons() & Qt::LeftButton)) {
        stopDragging(view);
        return false;
    }

    /* Check for drag distance. */
    if (m_state == PREPAIRING) {
        if ((m_mousePressPos - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            return false;
        } else {
            startDragging(view);
        }
    }

    /* Update mouse positions & calculate delta. */
    QPointF currentMouseScenePos = view->mapToScene(event->pos());
    QPointF delta = currentMouseScenePos - m_lastMouseScenePos;
    m_lastMouseScenePos = currentMouseScenePos;

    /* Drag selected items. */
    foreach (QGraphicsItem *item, scene()->selectedItems())
        item->setPos(item->pos() + delta);

    event->accept();
    return false; /* Let other instruments receive mouse move events too! */
}

bool DragInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state == INITIAL || !view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    stopDragging(view);

    event->accept();
    return false;
}

void DragInstrument::startDragging(QGraphicsView *view) {
    m_state = DRAGGING;

    if(m_itemToSelect != NULL) {
        m_itemToSelect->setSelected(true);
        m_itemToSelect = NULL;
    }

    emit draggingStarted(view, scene()->selectedItems());
}

void DragInstrument::stopDragging(QGraphicsView *view) {
    if(m_state == DRAGGING)
        emit draggingFinished(view, scene()->selectedItems());
    m_state = INITIAL;
}



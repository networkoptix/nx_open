#include "draginstrument.h"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QMouseEvent>
#include <QApplication>

namespace {
    QEvent::Type viewportEventTypes[] = {
        QEvent::MouseButtonPress,
        QEvent::MouseMove,
        QEvent::MouseButtonRelease
    };
}

DragInstrument::DragInstrument(QObject *parent): 
    Instrument(makeSet(), makeSet(), makeSet(viewportEventTypes), false, parent) 
{}

void DragInstrument::installedNotify() {
    m_state = INITIAL;

    Instrument::installedNotify();
}

bool DragInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state != INITIAL || !view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Find the item to select. */
    struct ItemIsSelectable: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->flags() & QGraphicsItem::ItemIsSelectable;
        }
    };
    QGraphicsItem *itemToSelect = item(view, event->pos(), ItemIsSelectable());
    if (itemToSelect == NULL)
        return false;

    m_state = PREPAIRING;
    m_mousePressPos = event->pos();
    m_lastMouseScenePos = view->mapToScene(event->pos());

    if (event->modifiers() & Qt::ControlModifier) {
        /* Toggle selection of the current item if Ctrl is pressed. This is the
         * behavior that is to be expected. */
        itemToSelect->setSelected(!itemToSelect->isSelected());
    } else {
        /* Don't clear selection if the item to select is already selected so
         * that user can drag all selected items by dragging only one of them. */
        if (!itemToSelect->isSelected()) {
            scene()->clearSelection();
            itemToSelect->setSelected(true);
        }
    }

    event->accept();
    return true;
}

bool DragInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state == INITIAL || !view->isInteractive())
        return false;

    /* Stop dragging if the user has let go of the trigger button (even if we didn't get the release events). */
    if (!(event->buttons() & Qt::LeftButton)) {
        stopDragging();
        return false;
    }

    /* Check for drag distance. */
    if (m_state == PREPAIRING) {
        if ((m_mousePressPos - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            return false;
        } else {
            startDragging();
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

    stopDragging();

    event->accept();
    return true;
}

void DragInstrument::startDragging() {
    m_state = DRAGGING;
    emit draggingStarted(scene()->selectedItems());
}

void DragInstrument::stopDragging() {
    if(m_state == DRAGGING)
        emit draggingFinished(scene()->selectedItems());
    m_state = INITIAL;
}



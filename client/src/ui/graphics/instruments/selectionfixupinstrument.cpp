#include "selectionfixupinstrument.h"
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>

namespace {
    struct ItemIsSelectable: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->flags() & QGraphicsItem::ItemIsSelectable;
        }
    };

} // anonymous namespace

SelectionFixupInstrument::SelectionFixupInstrument(QObject *parent):
    DragProcessingInstrument(ITEM, makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), parent)
{}

bool SelectionFixupInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(!(item->flags() & QGraphicsItem::ItemIsSelectable))
        return false;

    /* If we don't accept it here, scene will clear selection. */
    event->accept();

    if(event->button() != Qt::LeftButton)
        return false;

    m_isClick = true;
    dragProcessor()->mousePressEvent(item, event);

    if(event->modifiers() & Qt::ControlModifier)
        return false; /* Ctrl-selection is processed when mouse button is released. */
   
    if(!item->isSelected()) {
        item->scene()->clearSelection();
        item->setSelected(true);
    }

    return false;
}

bool SelectionFixupInstrument::mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if (!m_isClick)
        return false;

    dragProcessor()->mouseMoveEvent(item, event);
    return false;

}

bool SelectionFixupInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(!m_isClick)
        return false; 

    if(!(item->flags() & QGraphicsItem::ItemIsSelectable))
        return false;
    
    event->accept();

    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::ControlModifier)) {
        item->scene()->clearSelection(); /* User may let go of the Ctrl button while mouse button is still pressed. */
        item->setSelected(true);
    }

    dragProcessor()->mouseReleaseEvent(item, event);
    return false;
}

void SelectionFixupInstrument::startDrag(DragInfo *) {
    dragProcessor()->reset();
}

void SelectionFixupInstrument::finishDragProcess(DragInfo *info) {
    m_isClick = false;
}

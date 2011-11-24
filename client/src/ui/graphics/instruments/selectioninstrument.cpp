#include "selectioninstrument.h"

namespace {
    struct ItemIsSelectable: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->flags() & QGraphicsItem::ItemIsSelectable;
        }
    };

} // anonymous namespace

SelectionInstrument::SelectionInstrument(QObject *parent):
    Instrument(ITEM, makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease), parent)
{}

bool SelectionInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(!(item->flags() & QGraphicsItem::ItemIsSelectable))
        return false;

    /* If we don't accept it here, scene will clear selection. */
    event->accept();

    if(event->button() != Qt::LeftButton)
        return false;

    if(event->modifiers() & Qt::ControlModifier)
        return false; /* Ctrl-selection is processed when mouse button is released. */
   
    if(!item->isSelected()) {
        item->scene()->clearSelection();
        item->setSelected(true);
    }

    return false;
}

bool SelectionInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(!(item->flags() & QGraphicsItem::ItemIsSelectable))
        return false;
    
    event->accept();

    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::ControlModifier)) {
        item->scene()->clearSelection(); /* User may let go of the Ctrl button while mouse button is still pressed. */
        item->setSelected(true);
    }

    return false;
}

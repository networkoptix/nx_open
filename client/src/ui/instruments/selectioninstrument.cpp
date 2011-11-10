#include "selectioninstrument.h"

namespace {
    struct ItemIsSelectable: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->flags() & QGraphicsItem::ItemIsSelectable;
        }
    };

} // anonymous namespace

SelectionInstrument::SelectionInstrument(QObject *parent):
    Instrument(makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease), makeSet(), makeSet(), makeSet(), parent)
{}

bool SelectionInstrument::mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    if(event->modifiers() & Qt::ControlModifier)
        return false; /* Ctrl-selection is processed when mouse button is released. */
   
    QGraphicsItem *item = this->item(event, ItemIsSelectable());
    if(item == NULL)
        return false;

    if(!item->isSelected()) {
        scene->clearSelection();
        item->setSelected(true);
    }

    return false;
}

bool SelectionInstrument::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    QGraphicsItem *item = this->item(event, ItemIsSelectable());
    if(item == NULL)
        return false;

    if(!item->isSelected()) {
        if(!(event->modifiers() & Qt::ControlModifier))
            scene->clearSelection(); /* User may let go of the Ctrl button while mouse button is still pressed. */
        item->setSelected(true);
    }

    return false;
}

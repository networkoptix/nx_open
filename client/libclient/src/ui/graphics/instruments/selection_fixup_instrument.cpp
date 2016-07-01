#include "selection_fixup_instrument.h"
#include <limits>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsSceneMouseEvent>

class SelectionPreFixupInstrument: public Instrument {
public:
    SelectionPreFixupInstrument(QObject *parent):
        Instrument(Item, makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), parent)
    {
        m_nan = std::numeric_limits<qreal>::quiet_NaN();
        m_intMin = std::numeric_limits<int>::min();
    }

protected:
    virtual bool mousePressEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) override {
        fixButtonDownPositions(event);
            
        return false;
    }

    virtual bool mouseMoveEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) override {
        fixButtonDownPositions(event);

        return false;
    }

    virtual bool mouseReleaseEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *event) override {
        fixButtonDownPositions(event);
            
        return false;
    }

private:
    void fixButtonDownPositions(QGraphicsSceneMouseEvent *event) {
        for (uint i = 0x1; i <= 0x10; i <<= 1) {
            if((event->buttons() & i) || event->button() == i)
                continue;

            /* Qt is totally inconsistent in setting the button down positions for
             * mouse events. So we invalidate positions for buttons that are not pressed. 
             * This fixes some really surprising quirks in how selection is handled in
             * graphics items. */
            Qt::MouseButton button = static_cast<Qt::MouseButton>(i);
            event->setButtonDownPos(button, QPointF(m_nan, m_nan));
            event->setButtonDownScenePos(button, QPointF(m_nan, m_nan));
            event->setButtonDownScreenPos(button, QPoint(m_intMin, m_intMin));
        }
    }

private:
    qreal m_nan;
    int m_intMin;
};



SelectionFixupInstrument::SelectionFixupInstrument(QObject *parent):
    DragProcessingInstrument(Item, makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), parent)
{
    m_preForwardingInstrument = new SelectionPreFixupInstrument(this);
}

Instrument *SelectionFixupInstrument::preForwardingInstrument() const {
    return m_preForwardingInstrument;
}

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
        /* User may let go of the Ctrl button while mouse button is still pressed. */
        bool signalsBlocked = item->scene()->blockSignals(true); /* Don't emit multiple notifications. */
        item->scene()->clearSelection(); 
        item->scene()->blockSignals(signalsBlocked);
        item->setSelected(true);
    } else {
        /* QGraphicsItem processes Ctrl+click as multi-selection only of the mouse didn't move. We work this around. */
        if(event->scenePos() != event->buttonDownScenePos(Qt::LeftButton))
            item->setSelected(!item->isSelected());
    }

    dragProcessor()->mouseReleaseEvent(item, event);
    return false;
}

void SelectionFixupInstrument::startDrag(DragInfo *) {
    dragProcessor()->reset();
}

void SelectionFixupInstrument::finishDragProcess(DragInfo *) {
    m_isClick = false;
}

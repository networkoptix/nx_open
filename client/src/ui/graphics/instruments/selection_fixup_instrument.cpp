#include "selection_fixup_instrument.h"
#include <limits>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>

namespace {
    class SelectionPreFixupInstrument: public Instrument {
    public:
        SelectionPreFixupInstrument(QObject *parent):
            Instrument(ITEM, makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), parent)
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
            for (int i = 0x1; i <= 0x10; i <<= 1) {
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

} // anonymous namespace


SelectionFixupInstrument::SelectionFixupInstrument(QObject *parent):
    DragProcessingInstrument(ITEM, makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), parent)
{
    m_preForwardingInstrument = new SelectionPreFixupInstrument(this);
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
        item->scene()->clearSelection(); /* User may let go of the Ctrl button while mouse button is still pressed. */
        item->setSelected(true);
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

#ifndef QN_SELECTION_FIXUP_INSTRUMENT_H
#define QN_SELECTION_FIXUP_INSTRUMENT_H

#include "drag_processing_instrument.h"

class SelectionPreFixupInstrument;

/**
 * Clicks on graphics widget's frame is treated differently compared to a click
 * on the surface, which results in surprises with selection handling.
 * 
 * Also in graphics items right click is treated the same as left click, which
 * is not the desired behavior.
 * 
 * This instrument fixes these problems.
 * 
 * It is to be installed at item level after a forwarding instrument, 
 * but before an instrument that stops event processing.
 */
class SelectionFixupInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    SelectionFixupInstrument(QObject *parent = NULL);

    Instrument *preForwardingInstrument() const;

protected:
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    bool m_isClick;
    SelectionPreFixupInstrument *m_preForwardingInstrument;
};


#endif // QN_SELECTION_FIXUP_INSTRUMENT_H

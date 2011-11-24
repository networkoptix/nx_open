#ifndef QN_SELECTION_FIXUP_INSTRUMENT_H
#define QN_SELECTION_FIXUP_INSTRUMENT_H

#include "instrument.h"

/**
 * Selection of graphics widgets works differently than selection of normal items.
 * 
 * This instrument fixes this problem.
 * 
 * It is to be installed at item level after a forwarding instrument, 
 * but before an instrument that stops event processing.
 */
class SelectionFixupInstrument: public Instrument {
    Q_OBJECT;
public:
    SelectionFixupInstrument(QObject *parent = NULL);

protected:
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
};


#endif QN_SELECTION_FIXUP_INSTRUMENT_H

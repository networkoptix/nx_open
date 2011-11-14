#ifndef QN_SELECTION_INSTRUMENT_H
#define QN_SELECTION_INSTRUMENT_H

#include "instrument.h"

/**
 * When clicking on item's frame, the item does not get selected.
 * 
 * This instrument fixes this problem.
 * 
 * It is to be installed after a forwarding instrument, but before an instrument
 * that stops accepted event processing.
 */
class SelectionInstrument: public Instrument {
    Q_OBJECT;
public:
    SelectionInstrument(QObject *parent = NULL);

protected:
    virtual bool mousePressEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) override;
    virtual bool mouseReleaseEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) override;
};


#endif QN_SELECTION_INSTRUMENT_H

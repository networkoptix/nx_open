#ifndef QN_PTZ_INSTRUMENT_H
#define QN_PTZ_INSTRUMENT_H

#include "drag_processing_instrument.h"

class PtzInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;
public:
    PtzInstrument(QObject *parent = NULL);
    virtual ~PtzInstrument();

protected:
    virtual bool registeredNotify(QGraphicsItem *item) override;

private:

};


#endif // QN_PTZ_INSTRUMENT_H

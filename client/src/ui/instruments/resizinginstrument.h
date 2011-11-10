#ifndef QN_RESIZING_INSTRUMENT_H
#define QN_RESIZING_INSTRUMENT_H

#include "instrument.h"

class ResizingInstrument: public Instrument {
    Q_OBJECT;
public:
    ResizingInstrument(QObject *parent = NULL);

private:

};

#endif // QN_RESIZING_INSTRUMENT_H

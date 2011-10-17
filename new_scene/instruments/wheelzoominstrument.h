#ifndef QN_WHEEL_ZOOM_INSTRUMENT_H
#define QN_WHEEL_ZOOM_INSTRUMENT_H

#include "instrument.h"

class WheelZoomInstrument: public Instrument {
public:
    WheelZoomInstrument(QObject *parent);

protected:
    virtual bool wheelEvent(QWidget *viewport, QWheelEvent *event) override;
};

#endif // QN_WHEEL_ZOOM_INSTRUMENT_H

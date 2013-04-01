#ifndef QN_ZOOM_WINDOW_INSTRUMENT_H
#define QN_ZOOM_WINDOW_INSTRUMENT_H

#include "instrument.h"

class ZoomWindowInstrument: public Instrument {
    Q_OBJECT
    typedef Instrument base_type;

public:
    ZoomWindowInstrument(QObject *parent = NULL);
    virtual ~ZoomWindowInstrument();

protected:
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

private:

};

#endif // QN_ZOOM_WINDOW_INSTRUMENT_H

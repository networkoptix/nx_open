#ifndef QN_STOP_INSTRUMENT_H
#define QN_STOP_INSTRUMENT_H

#include "instrument.h"

class StopInstrument: public Instrument {
    Q_OBJECT;
public:
    StopInstrument(const EventTypeSet &sceneEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &viewportEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(sceneEventTypes, viewEventTypes, viewportEventTypes, itemEventTypes, parent)
    {}

private:
    virtual bool event(QGraphicsScene *, QEvent *) override { return true; }
    virtual bool event(QGraphicsView *, QEvent *) override { return true; }
    virtual bool event(QWidget *, QEvent *) override { return true; }
    virtual bool sceneEvent(QGraphicsItem *, QEvent *) override { return true; }
};

#endif // QN_STOP_INSTRUMENT_H

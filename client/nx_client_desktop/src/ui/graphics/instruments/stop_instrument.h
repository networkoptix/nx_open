#ifndef QN_STOP_INSTRUMENT_H
#define QN_STOP_INSTRUMENT_H

#include "instrument.h"

/**
 * This instrument stops event processing. 
 */
class StopInstrument: public Instrument {
    Q_OBJECT;
public:
    StopInstrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(viewportEventTypes, viewEventTypes, sceneEventTypes, itemEventTypes, parent)
    {}

    StopInstrument(WatchedType type, const EventTypeSet &eventTypes, QObject *parent = NULL):
        Instrument(type, eventTypes, parent)
    {}

private:
    virtual bool event(QGraphicsScene *, QEvent *) override { return true; }
    virtual bool event(QGraphicsView *, QEvent *) override { return true; }
    virtual bool event(QWidget *, QEvent *) override { return true; }
    virtual bool sceneEvent(QGraphicsItem *, QEvent *) override { return true; }
};

#endif // QN_STOP_INSTRUMENT_H

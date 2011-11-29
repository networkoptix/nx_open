#ifndef QN_STOP_ACCEPTED_INSTRUMENT_H
#define QN_STOP_ACCEPTED_INSTRUMENT_H

#include "instrument.h"

/**
 * This instrument stops processing of accepted events. 
 */
class StopAcceptedInstrument: public Instrument {
    Q_OBJECT;
public:
    StopAcceptedInstrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(viewportEventTypes, viewEventTypes, sceneEventTypes, itemEventTypes, parent)
    {}

    StopAcceptedInstrument(WatchedType type, const EventTypeSet &eventTypes, QObject *parent = NULL):
        Instrument(type, eventTypes, parent)
    {}

private:
    virtual bool event(QGraphicsScene *, QEvent *event) override { return event->isAccepted(); }
    virtual bool event(QGraphicsView *, QEvent *event) override { return event->isAccepted(); }
    virtual bool event(QWidget *, QEvent *event) override { return event->isAccepted(); }
    virtual bool sceneEvent(QGraphicsItem *, QEvent *event) override { return event->isAccepted(); }
};

#endif // QN_STOP_ACCEPTED_INSTRUMENT_H

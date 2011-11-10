#ifndef QN_STOP_ACCEPTED_INSTRUMENT_H
#define QN_STOP_ACCEPTED_INSTRUMENT_H

#include "instrument.h"

class StopAcceptedInstrument: public Instrument {
    Q_OBJECT;
public:
    StopAcceptedInstrument(const EventTypeSet &sceneEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &viewportEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(sceneEventTypes, viewEventTypes, viewportEventTypes, itemEventTypes, parent)
    {}

private:
    virtual bool event(QGraphicsScene *, QEvent *event) override { return event->isAccepted(); }
    virtual bool event(QGraphicsView *, QEvent *event) override { return event->isAccepted(); }
    virtual bool event(QWidget *, QEvent *event) override { return event->isAccepted(); }
    virtual bool sceneEvent(QGraphicsItem *, QEvent *event) override { return event->isAccepted(); }
};

#endif // QN_STOP_ACCEPTED_INSTRUMENT_H

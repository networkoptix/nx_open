#ifndef QN_STOP_ACCEPTED_INSTRUMENT_H
#define QN_STOP_ACCEPTED_INSTRUMENT_H

#include "instrument.h"

class StopAcceptedInstrument: public Instrument {
    Q_OBJECT;
public:
    StopAcceptedInstrument(const QSet<QEvent::Type> &sceneEventTypes, const QSet<QEvent::Type> &viewEventTypes, const QSet<QEvent::Type> &viewportEventTypes, bool watchesItemEvents, QObject *parent):
        Instrument(sceneEventTypes, viewEventTypes, viewportEventTypes, watchesItemEvents, parent)
    {}

private:
    virtual bool event(QGraphicsScene *, QEvent *event) override { return event->isAccepted(); }
    virtual bool event(QGraphicsView *, QEvent *event) override { return event->isAccepted(); }
    virtual bool event(QWidget *, QEvent *event) override { return event->isAccepted(); }
    virtual bool sceneEvent(QGraphicsItem *, QEvent *event) override { return event->isAccepted(); }
};

#endif // QN_STOP_ACCEPTED_INSTRUMENT_H

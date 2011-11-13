#ifndef QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H
#define QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

#include "instrument.h"

class ForwardingInstrument: public Instrument {
    Q_OBJECT;
public:
    ForwardingInstrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(viewportEventTypes, viewEventTypes, sceneEventTypes, itemEventTypes, parent)
    {}

    ForwardingInstrument(WatchedType watchedType, const EventTypeSet &eventTypes, QObject *parent):
        Instrument(watchedType, eventTypes, parent)
    {}


protected:
    virtual bool event(QGraphicsScene *, QEvent *) override;
    virtual bool event(QGraphicsView *, QEvent *) override;
    virtual bool event(QWidget *, QEvent *) override;
    virtual bool sceneEvent(QGraphicsItem *, QEvent *) override;
};

#endif // QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

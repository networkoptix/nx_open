// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H
#define QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

#include "instrument.h"

/**
 * This instrument forwards the events to the target object.
 */
class ForwardingInstrument: public Instrument {
    Q_OBJECT;
public:
    ForwardingInstrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(viewportEventTypes, viewEventTypes, sceneEventTypes, itemEventTypes, parent)
    {}

    ForwardingInstrument(WatchedType type, const EventTypeSet &eventTypes, QObject *parent):
        Instrument(type, eventTypes, parent)
    {}

protected:
    virtual bool anyEvent(QGraphicsScene *, QEvent *) override;
    virtual bool anyEvent(QGraphicsView *, QEvent *) override;
    virtual bool anyEvent(QWidget *, QEvent *) override;
    virtual bool sceneEvent(QGraphicsItem *, QEvent *) override;
};

#endif // QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

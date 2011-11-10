#ifndef QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H
#define QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

#include "instrument.h"

class ForwardingInstrument: public Instrument {
    Q_OBJECT;
public:
    ForwardingInstrument(const EventTypeSet &sceneEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &viewportEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
        Instrument(sceneEventTypes, viewEventTypes, viewportEventTypes, itemEventTypes, parent)
    {}

protected:
    virtual bool event(QGraphicsScene *, QEvent *) override;
    virtual bool event(QGraphicsView *, QEvent *) override;
    virtual bool event(QWidget *, QEvent *) override;
    virtual bool sceneEvent(QGraphicsItem *, QEvent *) override;
};

#endif // QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_REDIRECTING_INSTRUMENT_H
#define QN_REDIRECTING_INSTRUMENT_H

#include "instrument.h"

/**
 * This instrument redirects event filtering to another instrument.
 */
class RedirectingInstrument: public Instrument {
    Q_OBJECT;
public:
    RedirectingInstrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, Instrument *target, QObject *parent):
        Instrument(viewportEventTypes, viewEventTypes, sceneEventTypes, itemEventTypes, parent)
    {
        init(target);
    }

    RedirectingInstrument(WatchedType type, const EventTypeSet &eventTypes, Instrument *target, QObject *parent):
        Instrument(type, eventTypes, parent)
    {
        init(target);
    }

protected:
    virtual bool anyEvent(QGraphicsScene *, QEvent *) override;
    virtual bool anyEvent(QGraphicsView *, QEvent *) override;
    virtual bool anyEvent(QWidget *, QEvent *) override;
    virtual bool sceneEvent(QGraphicsItem *, QEvent *) override;

private:
    void init(Instrument *target);

private:
    Instrument *m_target;
};

#endif // QN_REDIRECTING_INSTRUMENT_H

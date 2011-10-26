#ifndef QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H
#define QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

#include "instrument.h"

class SceneInputForwardingInstrument: public Instrument {
    Q_OBJECT;
public:
    SceneInputForwardingInstrument(QObject *parent = NULL);

protected:
    virtual bool event(QGraphicsScene *, QEvent *) override;
};

#endif // QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H
